#pragma once
#include "core.hpp"

namespace mpccv {
constexpr unsigned max_nodes=16;
constexpr uint8_t no_node=255;
enum class Kind:uint8_t { off, sine, triangle, gate, sequence, random_hold, mix, slew, quantizer };
struct Node {
    Kind kind=Kind::off;
    uint8_t input=no_node, modulator=no_node;
    double rate=1, scale=1, offset=0, amount=0, slew_per_second=10, quantum=1.0/12;
    std::array<double,8> steps{{0,1,2,3,4,3,2,1}};
    uint32_t seed=1;
};
struct Route {
    uint8_t source=no_node;
    double scale=1, offset=0, low=0, high=5;
};
struct Scene {
    uint32_t version=1;
    uint8_t count=0, enabled=15;
    bool sync=true;
    std::array<Node,max_nodes> nodes{};
    std::array<Route,outputs> routes{};
    // Four future Q-Link assignments refer to virtual nodes, not physical connectors.
    std::array<uint8_t,4> controls{{no_node,no_node,no_node,no_node}};
};
enum class GraphError { none, version, count, parameter, reference, cycle };
struct Plan { Scene scene{}; std::array<uint8_t,max_nodes> order{}; };
inline GraphError compile(const Scene& s,Plan& result) noexcept {
    if(s.version!=1) return GraphError::version;
    if(s.count>max_nodes) return GraphError::count;
    if(!s.sync) return GraphError::parameter; // free-running clock not implemented
    auto bounded=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    for(unsigned i=0;i<s.count;++i) {
        const auto& n=s.nodes[i];
        if(static_cast<unsigned>(n.kind)>static_cast<unsigned>(Kind::quantizer) ||
           !bounded(n.rate,0.001,64)||!bounded(n.scale,-16,16)||!bounded(n.offset,-20,20)||
           !bounded(n.amount,-16,16)||!bounded(n.slew_per_second,0.001,10000)||!bounded(n.quantum,0.001,10)) return GraphError::parameter;
        for(double v:n.steps) if(!bounded(v,-20,20)) return GraphError::parameter;
        if((n.input!=no_node && n.input>=s.count)||(n.modulator!=no_node && n.modulator>=s.count)) return GraphError::reference;
    }
    for(auto c:s.controls) if(c!=no_node && c>=s.count) return GraphError::reference;
    for(const auto& r:s.routes) {
        if(r.source!=no_node && r.source>=s.count) return GraphError::reference;
        if(!bounded(r.scale,-16,16)||!bounded(r.offset,-20,20)||!bounded(r.low,0,5)||!bounded(r.high,0,5)||r.low>r.high) return GraphError::parameter;
    }
    Plan candidate; candidate.scene=s;
    uint32_t done=0;
    for(unsigned slot=0;slot<s.count;++slot) {
        bool found=false;
        for(unsigned i=0;i<s.count;++i) {
            if(done&(1u<<i)) continue;
            const auto& n=s.nodes[i];
            if((n.input!=no_node && !(done&(1u<<n.input))) ||
               (n.modulator!=no_node && !(done&(1u<<n.modulator)))) continue;
            candidate.order[slot]=static_cast<uint8_t>(i); done|=1u<<i; found=true; break;
        }
        if(!found) return GraphError::cycle;
    }
    result=candidate; return GraphError::none;
}
struct EnginePreset { uint32_t version=1; Node node{}; };
inline bool apply_preset(Scene& scene,unsigned index,const EnginePreset& preset) noexcept {
    if(preset.version!=1 || index>=scene.count)return false;
    auto candidate=scene; candidate.nodes[index]=preset.node;
    Plan p;if(compile(candidate,p)!=GraphError::none)return false;
    scene=candidate;return true;
}
inline Scene demo_scene() noexcept {
    Scene s; s.count=6; s.enabled=255; s.controls={{0,1,2,3}};
    s.nodes[0].kind=Kind::sine; s.nodes[0].scale=5;
    s.nodes[1].kind=Kind::triangle; s.nodes[1].scale=5;
    s.nodes[2].kind=Kind::gate; s.nodes[2].scale=5; s.nodes[2].rate=4;
    s.nodes[3].kind=Kind::random_hold; s.nodes[3].scale=5;
    s.nodes[4].kind=Kind::sequence;
    s.nodes[5].kind=Kind::slew; s.nodes[5].input=4;
    for(unsigned j=0;j<outputs;++j) s.routes[j].source=static_cast<uint8_t>(j%6);
    return s;
}
class Graph {
    Plan plan{};
    std::array<double,max_nodes> signal{}, memory{};
public:
    bool armed=false;
    GraphError configure(const Scene& s) noexcept {
        Plan p; auto error=compile(s,p); if(error!=GraphError::none) return error;
        plan=p; memory.fill(0); signal.fill(0); armed=false; return GraphError::none;
    }
    const Scene& scene() const noexcept {return plan.scene;}
    Frame sample(const Transport& t,uint64_t sample_index) noexcept {
        Frame f; f.sample=sample_index;
        if(!armed || !valid(t) || !t.playing) {memory.fill(0); signal.fill(0); return f;}
        for(unsigned slot=0;slot<plan.scene.count;++slot) {
            auto i=plan.order[slot]; const auto& n=plan.scene.nodes[i];
            double in=n.input==no_node?0:signal[n.input];
            double mod=n.modulator==no_node?0:signal[n.modulator];
            double rate=std::fmax(0.001,std::fmin(64,n.rate+mod*n.amount));
            double pos=t.ppq*rate, phase=pos-std::floor(pos), v=0;
            switch(n.kind) {
            case Kind::off: signal[i]=0; continue;
            case Kind::sine: v=0.5-0.5*std::cos(6.283185307179586*phase); break;
            case Kind::triangle: v=1-std::abs(phase*2-1); break;
            case Kind::gate: v=phase<0.5?1:0; break;
            case Kind::sequence: v=n.steps[static_cast<uint64_t>(pos)%8]; break;
            case Kind::random_hold: {
                uint32_t x=static_cast<uint32_t>(static_cast<uint64_t>(pos))^n.seed;
                x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16;
                v=double(x)/4294967295.0; break;
            }
            case Kind::mix: v=in+mod*n.amount; break;
            case Kind::slew: {
                double step=n.slew_per_second/t.sample_rate;
                memory[i]+=std::fmax(-step,std::fmin(step,in-memory[i])); v=memory[i]; break;
            }
            case Kind::quantizer: v=std::round(in/n.quantum)*n.quantum; break;
            }
            signal[i]=std::fmax(-100,std::fmin(100,v*n.scale+n.offset));
        }
        for(unsigned j=0;j<outputs;++j) {
            const auto& r=plan.scene.routes[j];
            if(!(plan.scene.enabled&(1u<<j)) || r.source==no_node) continue;
            f.values[j]=code(std::fmax(r.low,std::fmin(r.high,signal[r.source]*r.scale+r.offset)));
        }
        return f;
    }
};
}
