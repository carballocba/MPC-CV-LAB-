#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/vst/abi.hpp"
#include "mpccv/scene.hpp"
#include <cassert>
#include <dlfcn.h>
#include <vector>
#include <thread>
using namespace mpccv::vst;
Time clock_info{};
bool missing=false, arm_in_callback=false;
intptr_t host(Effect* e,int32_t op,int32_t,intptr_t,void*,float) {
    if(op==1)return 2400;
    if(op==7) {
        if(arm_in_callback)e->setParameter(e,0,1);
        return missing?0:reinterpret_cast<intptr_t>(&clock_info);
    }
    return 0;
}
int main(int argc,char** argv) {
    assert(argc==2);void* lib=dlopen(argv[1],RTLD_NOW);assert(lib);
    auto entry=reinterpret_cast<Effect*(*)(Callback)>(dlsym(lib,"VSTPluginMain"));assert(entry);
    auto telemetry=reinterpret_cast<int(*)(Effect*,Telemetry*)>(dlsym(lib,"mpc_cv_read_telemetry"));assert(telemetry);
    assert(!entry(nullptr));auto* e=entry(host);auto* other=entry(host);assert(e&&other);
    assert(e->magic==magic&&e->uniqueID==identity&&e->numParams==52&&e->numOutputs==2);
    assert(e->dispatcher(e,0,0,0,nullptr,0));
    char display[8]{};assert(e->dispatcher(e,7,4,0,display,0));assert(std::strcmp(display,"Sine")==0);
    assert(e->dispatcher(e,7,36,0,display,0));assert(std::strcmp(display,"Node 1")==0);
    e->dispatcher(e,10,0,0,nullptr,44100);e->dispatcher(e,11,0,128,nullptr,0);e->dispatcher(e,12,0,1,nullptr,0);
    clock_info.ppqPos=0.25;clock_info.tempo=120;clock_info.flags=2|(1<<9)|(1<<10);
    float left[128],right[128];float* out[]{left,right};
    for(auto& v:left) {v=1;}
    for(auto& v:right) {v=1;}
    e->processReplacing(e,nullptr,out,128);for(auto v:left)assert(v==0);for(auto v:right)assert(v==0);
    for(unsigned i=0;i<8;++i)assert(e->getParameter(e,44+int(i))==(i<4?1.0f:0.0f));
    e->setParameter(e,44,0);assert(e->getParameter(e,44)==0);
    e->setParameter(e,51,1);assert(e->getParameter(e,51)==1);
    assert(std::lround(e->getParameter(e,1)*255)==142);
    e->setParameter(e,1,0);
    std::thread enable_a([&]{for(unsigned i=0;i<1000;++i)e->setParameter(e,44,1);});
    std::thread enable_b([&]{for(unsigned i=0;i<1000;++i)e->setParameter(e,51,1);});
    enable_a.join();enable_b.join();assert(std::lround(e->getParameter(e,1)*255)==129);
    e->setParameter(e,1,15.0f/255);
    Telemetry t{};assert(telemetry(e,&t));assert(!t.armed);
    e->setParameter(e,0,1);e->setParameter(e,1,1);e->processReplacing(e,nullptr,out,128);
    telemetry(e,&t);assert(t.armed&&!t.fault&&t.codes[6]>0&&t.codes[7]>0);
    missing=true;e->processReplacing(e,nullptr,out,128);telemetry(e,&t);assert(!t.armed);for(auto v:t.codes)assert(v==0);missing=false;
    clock_info.flags=0;e->processReplacing(e,nullptr,out,128);telemetry(e,&t);assert(!t.playing);
    clock_info.flags=2|(1<<9)|(1<<10);
    void* data=nullptr;auto n=e->dispatcher(e,23,0,0,&data,0);assert(n>0);
    std::vector<uint8_t> saved(static_cast<uint8_t*>(data),static_cast<uint8_t*>(data)+n);
    e->setParameter(e,2,0.7f);assert(e->getParameter(e,2)>0.69f);
    assert(e->dispatcher(e,24,0,n,saved.data(),0));assert(e->getParameter(e,0)==0);
    e->processReplacing(e,nullptr,out,128);assert(e->getParameter(e,2)<0.1f);
    saved[15]^=1;assert(!e->dispatcher(e,24,0,n,saved.data(),0));
    // Loop boundary straddles this block; evaluate the wrapped PPQ inside it.
    clock_info.flags=2|4|(1<<9)|(1<<10)|(1<<12);
    clock_info.ppqPos=0.999;clock_info.cycleStartPos=0;clock_info.cycleEndPos=1;
    e->setParameter(e,0,1);e->processReplacing(e,nullptr,out,128);
    telemetry(e,&t);assert(t.codes[2]==16383 && !t.fault);
    e->setParameter(e,36,3.0f/16);e->processReplacing(e,nullptr,out,128);
    telemetry(e,&t);assert(t.codes[0]==t.codes[2]);
    assert(e->getParameter(e,26)==float(t.codes[0])/16383);
    e->setParameter(e,26,0);assert(e->getParameter(e,26)>0); // read-only output meter
    clock_info.flags=2|(1<<9)|(1<<10);clock_info.ppqPos=0.25;
    // Host automation may arrive concurrently; atomic parameters avoid an audio lock.
    std::thread automation([&]{for(int i=0;i<10000;++i)e->setParameter(e,2,float(i%100)/100);});
    for(int i=0;i<500;++i) {e->processReplacing(e,nullptr,out,128);}
    automation.join();
    // Scene writes must stay disarmed until all accepted whole scenes are applied.
    saved[15]^=1;
    mpccv::Scene scene;assert(mpccv::load_scene(saved.data(),saved.size(),scene));
    mpccv::SceneBytes bytes;
    for(unsigned i=0;i<3;++i) {
        scene.nodes[0].offset=double(i)+0.5;assert(mpccv::save_scene(scene,bytes));
        assert(e->dispatcher(e,24,0,intptr_t(bytes.size),bytes.bytes.data(),0));
    }
    scene.nodes[0].offset=9;assert(mpccv::save_scene(scene,bytes));
    assert(!e->dispatcher(e,24,0,intptr_t(bytes.size),bytes.bytes.data(),0)); // full queue rejects without changing desired scene
    data=nullptr;n=e->dispatcher(e,23,0,0,&data,0);
    assert(mpccv::load_scene(static_cast<uint8_t*>(data),size_t(n),scene));assert(scene.nodes[0].offset==2.5);
    arm_in_callback=true;
    for(unsigned i=0;i<3;++i) {
        e->processReplacing(e,nullptr,out,128);telemetry(e,&t);
        assert(t.armed==(i==2?1u:0u));
    }
    arm_in_callback=false;
    // Multiple control callers serialize chunk publication; only audio consumes.
    std::atomic<unsigned> accepted{0};
    auto load_chunks=[&] {for(unsigned i=0;i<200;++i)
        if(e->dispatcher(e,24,0,intptr_t(bytes.size),bytes.bytes.data(),0))accepted.fetch_add(1);};
    std::thread chunks_a(load_chunks),chunks_b(load_chunks);
    for(unsigned i=0;i<500;++i)e->processReplacing(e,nullptr,out,128);
    chunks_a.join();chunks_b.join();assert(accepted.load()>0);
    for(unsigned i=0;i<4;++i)e->processReplacing(e,nullptr,out,128);
    telemetry(e,&t);assert(!t.armed && !t.fault);
    e->dispatcher(e,12,0,0,nullptr,0);telemetry(e,&t);assert(!t.armed);
    for(auto& v:left) {v=0.5f;}
    e->process(e,nullptr,out,128);
    for(auto v:left) {assert(v==0.5f);}
    telemetry(other,&t);assert(!t.armed);
    e->dispatcher(e,1,0,0,nullptr,0);other->dispatcher(other,1,0,0,nullptr,0);assert(!dlclose(lib));
}
