#include "mpccv/vst/abi.hpp"
#include "mpccv/scene.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>

namespace {
using namespace mpccv;
using namespace mpccv::vst;
static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit atomic required");
uint32_t bits(float value) noexcept {uint32_t v;std::memcpy(&v,&value,4);return v;}
float number(uint32_t value) noexcept {float v;std::memcpy(&v,&value,4);return v;}
Scene initial_scene() {
    auto s=demo_scene();s.count=8;s.enabled=15;
    s.nodes[6]=s.nodes[0];s.nodes[7]=s.nodes[1];
    for(unsigned i=0;i<8;++i)s.routes[i].source=uint8_t(i);
    return s;
}
struct Plugin {
    Effect effect{};
    Callback host;
    std::array<std::atomic<uint32_t>,parameter_count> params{};
    std::atomic<uint32_t> sample_rate{bits(44100)},active{0};
    std::array<std::atomic<uint32_t>,8> codes{},rises{};
    std::atomic<uint32_t> playing{0},armed{0},fault{0},pending_scenes{0};
    Graph graph;
    Scene shadow=initial_scene();
    Queue<4,Scene> incoming;
    std::mutex control; // chunks only, never used by render/setParameter
    SceneBytes chunk;
    uint64_t sample_index=0;
    std::array<uint32_t,parameter_count> last_parameters{};
    std::array<bool,8> gate_high{};
    explicit Plugin(Callback h):host(h) {
        graph.configure(shadow);set_scene_parameters(shadow);
    }
    void set_scene_parameters(const Scene& s) noexcept {
        params[0].store(bits(0));params[1].store(bits(float(s.enabled)/255));
        for(unsigned j=0;j<8;++j)params[36+j].store(bits(s.routes[j].source==no_node?0:float(s.routes[j].source+1)/16));
        for(unsigned i=0;i<8;++i) {
            params[2+i*3].store(bits(float((s.nodes[i].rate-0.001)/63.999)));
            params[3+i*3].store(bits(float((s.nodes[i].scale+16)/32)));
            params[4+i*3].store(bits(float(unsigned(s.nodes[i].kind))/8));
        }
    }
    float param(unsigned i) const noexcept {return number(params[i].load(std::memory_order_relaxed));}
    Scene apply_parameters(Scene s) const noexcept {
        s.enabled=uint8_t(std::lround(param(1)*255));
        for(unsigned i=0;i<8 && i<s.count;++i) {
            s.nodes[i].rate=0.001+63.999*param(2+i*3);
            s.nodes[i].scale=-16+32*param(3+i*3);
            s.nodes[i].kind=static_cast<Kind>(std::lround(param(4+i*3)*8));
        }
        for(unsigned j=0;j<8;++j) {
            unsigned selected=unsigned(std::lround(param(36+j)*16));
            s.routes[j].source=selected && selected<=s.count?uint8_t(selected-1):no_node;
        }
        return s;
    }
};
Plugin& get(Effect* e) {return *static_cast<Plugin*>(e->object);}
void text(void* p,const char* value,size_t capacity) {
    if(!p || !capacity)return;
    auto* dest=static_cast<char*>(p);
    size_t i=0;
    while(i+1<capacity && value[i]) {dest[i]=value[i];++i;}
    dest[i]=0;
}
void set(Effect* e,int32_t index,float value) {
    if(index<0 || unsigned(index)>=parameter_count || (index>=26 && index<36) || !std::isfinite(value))return;
    get(e).params[unsigned(index)].store(bits(std::fmax(0,std::fmin(1,value))),std::memory_order_relaxed);
}
float read(Effect* e,int32_t index) {
    auto& p=get(e);
    if(index>=26 && index<34)return float(p.codes[unsigned(index)-26].load())/16383;
    if(index==34)return float(p.playing.load());
    if(index==35)return float(p.fault.load()!=0);
    return index>=0 && unsigned(index)<parameter_count?p.param(unsigned(index)):0;
}
void render(Effect* e,float**,float** out,int32_t count) noexcept {
    auto& p=get(e);
    // Replacing callback must clear audio even when transport is absent.
    if(out && count>0)for(unsigned ch=0;ch<2;++ch)if(out[ch])std::fill(out[ch],out[ch]+count,0.0f);
    if(count<=0 || count>8192) {p.fault.store(1);p.armed.store(0);for(auto& c:p.codes)c.store(0);return;}
    Scene next;
    bool popped=p.incoming.pop(next);
    bool changed=popped;
    if(changed) {
        p.set_scene_parameters(next);
        if(p.graph.configure(next)!=GraphError::none)p.fault.store(2);
    }
    for(unsigned i=1;i<parameter_count;++i) {
        if(i>=26 && i<36)continue;
        auto value=p.params[i].load(std::memory_order_relaxed);
        if(value!=p.last_parameters[i])changed=true;
        p.last_parameters[i]=value;
    }
    if(changed && p.graph.configure(p.apply_parameters(p.graph.scene()))!=GraphError::none)
        p.fault.store(2);
    if(popped)p.pending_scenes.fetch_sub(1,std::memory_order_acq_rel);
    const auto* time=reinterpret_cast<const Time*>(p.host(e,7,0,(1<<9)|(1<<10)|(1<<12),nullptr,0));
    Transport t;
    if(time) {
        t={bool((time->flags&(1<<9))&&(time->flags&(1<<10))),bool(time->flags&2),time->ppqPos,time->tempo,
           number(p.sample_rate.load(std::memory_order_relaxed))};
    }
    bool can_run=valid(t)&&t.playing&&p.active.load(std::memory_order_relaxed)&&!p.fault.load()&&!p.pending_scenes.load(std::memory_order_acquire);
    p.graph.armed=p.param(0)>=0.5f&&can_run;
    p.playing.store(can_run?1u:0u);p.armed.store(p.graph.armed?1u:0u);
    double start=t.ppq;
    bool loop=time && (time->flags&4) && (time->flags&(1<<12)) &&
        std::isfinite(time->cycleStartPos)&&std::isfinite(time->cycleEndPos)&&
        time->cycleStartPos>=0&&time->cycleEndPos>time->cycleStartPos && start>=time->cycleStartPos && start<time->cycleEndPos;
    Frame f;
    for(int32_t i=0;i<count;++i) {
        if(can_run) {
            t.ppq=start+double(i)*t.bpm/(60*t.sample_rate);
            if(loop && t.ppq>=time->cycleEndPos)
                t.ppq=time->cycleStartPos+std::fmod(t.ppq-time->cycleEndPos,time->cycleEndPos-time->cycleStartPos);
        }
        f=p.graph.sample(t,p.sample_index++);
        for(unsigned j=0;j<8;++j) {
            bool high=f.values[j]!=0;
            auto source=p.graph.scene().routes[j].source;
            if(source!=no_node && p.graph.scene().nodes[source].kind==Kind::gate && high&&!p.gate_high[j])
                p.rises[j].fetch_add(1,std::memory_order_relaxed);
            p.gate_high[j]=high;
        }
    }
    for(unsigned j=0;j<8;++j)p.codes[j].store(f.values[j],std::memory_order_relaxed);
}
void legacy(Effect* e,float**,float**,int32_t count) noexcept {
    // Plugin contributes silence, so accumulating legacy output is left intact.
    render(e,nullptr,nullptr,count);
}
intptr_t dispatch(Effect* e,int32_t op,int32_t index,intptr_t value,void* ptr,float opt) {
    auto& p=get(e);
    switch(op) {
    case 0:return 1;
    case 1:delete &p;return 1;
    case 2:return value==0;
    case 3:return 0;
    case 5:text(ptr,"Default",24);return 1;
    case 6:text(ptr,index>=26 && index<34?"V":index>=2 && index<26 && (index-2)%3==0?"cyc/bt":"",8);return 1;
    case 7: {
        double value=read(e,index);
        if(index>=2 && index<26 && (index-2)%3==2) {
            constexpr const char* names[]{"Off","Sine","Tri","Gate","Seq","Random","Mix","Slew","Quant"};
            text(ptr,names[unsigned(std::lround(value*8))],8);return 1;
        }
        if(index>=36 && index<44) {
            if(ptr) {auto node=unsigned(std::lround(value*16));
                constexpr const char* labels[]{"Off","Node 1","Node 2","Node 3","Node 4","Node 5","Node 6","Node 7","Node 8","Node 9","Node 10","Node 11","Node 12","Node 13","Node 14","Node 15","Node 16"};
                text(ptr,labels[node<=16?node:0],8);}
            return 1;
        }
        if(index==0 || index==34 || index==35) {text(ptr,value>=0.5?(index==35?"Fault":"On"):(index==35?"OK":"Off"),8);return 1;}
        if(index==1) {if(ptr)std::snprintf(static_cast<char*>(ptr),8,"%u",unsigned(std::lround(value*255))&255u);return 1;}
        if(index>=26 && index<34)value=double(p.codes[unsigned(index)-26].load())/3276.8;
        else if(index>=2 && index<26 && (index-2)%3==0)value=0.001+63.999*value;
        else if(index>=2 && index<26 && (index-2)%3==1)value=-16+32*value;
        if(ptr)std::snprintf(static_cast<char*>(ptr),8,"%.3f",value);
        return 1;
    }
    case 8: {
        char name[16]{};
        if(index==0)text(ptr,"Sim Arm",8);
        else if(index==1)text(ptr,"OutMask",8);
        else if(index>=26 && index<34){std::snprintf(name,sizeof(name),"CV%d",index-25);text(ptr,name,8);}
        else if(index==34)text(ptr,"Playing",8);
        else if(index==35)text(ptr,"Fault",8);
        else if(index>=36 && index<44){std::snprintf(name,sizeof(name),"Route%d",index-35);text(ptr,name,8);}
        else if(index>=2 && index<26) {
            std::snprintf(name,sizeof(name),"N%d %s",(index-2)/3+1,(index-2)%3==0?"Rate":(index-2)%3==1?"Scale":"Type");text(ptr,name,8);
        }
        return 1;
    }
    case 10:if(std::isfinite(opt)&&opt>=8000&&opt<=384000) {p.sample_rate.store(bits(opt));return 1;}return 0;
    case 11:return value>0&&value<=8192;
    case 12:p.active.store(value?1u:0u);if(!value){set(e,0,0);p.armed.store(0);for(auto& c:p.codes)c.store(0);}return 1;
    case 23: {
        if(!ptr)return 0;
        std::lock_guard<std::mutex> lock(p.control);
        auto s=p.pending_scenes.load(std::memory_order_acquire)?p.shadow:p.apply_parameters(p.shadow);
        if(!save_scene(s,p.chunk))return 0;
        *static_cast<void**>(ptr)=p.chunk.bytes.data();return intptr_t(p.chunk.size);
    }
    case 24: {
        if(value<=0 || value>4096)return 0;
        Scene s;if(!load_scene(static_cast<const uint8_t*>(ptr),size_t(value),s))return 0;
        std::lock_guard<std::mutex> lock(p.control);
        // Publish a pending barrier before the SPSC item. The audio callback alone
        // updates all scene parameters; no half-loaded control scene can run.
        p.pending_scenes.fetch_add(1,std::memory_order_acq_rel);
        if(!p.incoming.push(s)) {p.pending_scenes.fetch_sub(1,std::memory_order_acq_rel);return 0;}
        p.params[0].store(bits(0),std::memory_order_release);
        p.shadow=s;return 1;
    }
    case 26:return index>=0 && unsigned(index)<parameter_count && !(index>=26 && index<36);
    case 35:return 2; // synth
    case 45:text(ptr,"MPC-CV LAB",32);return 1;
    case 47:text(ptr,"carballocba",64);return 1;
    case 48:text(ptr,"MPC-CV LAB",64);return 1;
    case 49:return 100;
    case 51:return ptr && !std::strcmp(static_cast<const char*>(ptr),"receiveVstTimeInfo")?1:0;
    case 58:return 2400;
    default:return 0;
    }
}
}
extern "C" mpccv::vst::Effect* VSTPluginMain(mpccv::vst::Callback host) {
    if(!host || !host(nullptr,1,0,0,nullptr,0))return nullptr;
    auto* p=new(std::nothrow) Plugin(host);if(!p)return nullptr;
    auto& e=p->effect;e.magic=magic;e.dispatcher=dispatch;e.process=legacy;
    e.setParameter=set;e.getParameter=read;e.numPrograms=1;e.numParams=parameter_count;
    e.numInputs=0;e.numOutputs=2;e.flags=(1<<4)|(1<<5)|(1<<8);
    e.object=p;e.uniqueID=identity;e.version=100;e.processReplacing=render;
    return &e;
}
extern "C" int mpc_cv_read_telemetry(mpccv::vst::Effect* e,mpccv::vst::Telemetry* out) {
    if(!e || e->magic!=magic || !out)return 0;
    auto& p=get(e);
    for(unsigned j=0;j<8;++j){out->codes[j]=p.codes[j].load();out->gate_rises[j]=p.rises[j].load();}
    out->playing=p.playing.load();out->armed=p.armed.load();out->fault=p.fault.load();return 1;
}
