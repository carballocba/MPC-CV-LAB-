#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/scene.hpp"
#include <cassert>
#include <limits>
using namespace mpccv;
int main() {
    Scene s=demo_scene();Plan plan;assert(compile(s,plan)==GraphError::none);
    Graph g;assert(g.configure(s)==GraphError::none);
    Transport t{true,true,0.125,120,44100};assert(g.sample(t,0).values==Frame{}.values);
    g.armed=true;auto f=g.sample(t,0);assert(f.values[0]==f.values[6]);assert(f.values[1]==f.values[7]);
    for(unsigned i=0;i<10000;++i) {
        t.ppq=double(i)/22050; auto frame=g.sample(t,i);
        for(auto v:frame.values)assert(v<=16383);
    }
    t.playing=false;assert(g.sample(t,0).values==Frame{}.values);t.playing=true;
    s.nodes[0].modulator=1;s.nodes[1].modulator=0;
    assert(compile(s,plan)==GraphError::cycle);
    s=demo_scene();s.nodes[0].input=16;assert(compile(s,plan)==GraphError::reference);
    s=demo_scene();s.routes[0].high=6;assert(compile(s,plan)==GraphError::parameter);
    s=demo_scene();s.nodes[0].offset=std::numeric_limits<double>::quiet_NaN();assert(compile(s,plan)==GraphError::parameter);
    s=demo_scene();SceneBytes bytes;assert(save_scene(s,bytes));Scene restored;
    assert(load_scene(bytes.bytes.data(),bytes.size,restored));
    SceneBytes again;assert(save_scene(restored,again));assert(bytes.size==again.size && bytes.bytes==again.bytes);
    for(size_t n=0;n<bytes.size;++n)assert(!load_scene(bytes.bytes.data(),n,restored));
    bytes.bytes[30]^=1;assert(!load_scene(bytes.bytes.data(),bytes.size,restored));
    s=demo_scene();s.nodes[0].kind=Kind::gate;s.nodes[0].rate=64;assert(g.configure(s)==GraphError::none);g.armed=true;
    unsigned rises=0,falls=0;bool previous=false;
    for(unsigned i=0;i<44100;++i){t.ppq=double(i)*2/44100;bool high=g.sample(t,i).values[0]!=0;
        rises+=high&&!previous;falls+=!high&&previous;previous=high;}
    assert(rises==128 && falls==128);
    s=demo_scene();EnginePreset preset;preset.node.kind=Kind::triangle;
    assert(apply_preset(s,0,preset));assert(s.nodes[0].kind==Kind::triangle);
    preset.node.input=0;assert(!apply_preset(s,0,preset));assert(s.nodes[0].kind==Kind::triangle);
    assert(!apply_preset(s,16,preset));
    s=demo_scene();s.routes[0].source=4;assert(g.configure(s)==GraphError::none);assert(!g.armed);
    g.armed=true;t.ppq=2;assert(g.sample(t,0).values[0]==code(2));
    s.routes[0].source=2;assert(g.configure(s)==GraphError::none);g.armed=true;
    t.ppq=0.125;assert(g.sample(t,0).values[0]==0);t.ppq=0;assert(g.sample(t,0).values[0]==16383);
}
