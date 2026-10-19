#include "mpccv/vst/abi.hpp"
#include "mpccv/scene.hpp"
#include <cassert>
#include <dlfcn.h>
#include <vector>
#include <thread>
using namespace mpccv::vst;
Time clock_info{};
bool missing=false;
intptr_t host(Effect*,int32_t op,int32_t,intptr_t,void*,float) {
    if(op==1)return 2400;
    if(op==7)return missing?0:reinterpret_cast<intptr_t>(&clock_info);
    return 0;
}
int main(int argc,char** argv) {
    assert(argc==2);void* lib=dlopen(argv[1],RTLD_NOW);assert(lib);
    auto entry=reinterpret_cast<Effect*(*)(Callback)>(dlsym(lib,"VSTPluginMain"));assert(entry);
    auto telemetry=reinterpret_cast<int(*)(Effect*,Telemetry*)>(dlsym(lib,"mpc_cv_read_telemetry"));assert(telemetry);
    assert(!entry(nullptr));auto* e=entry(host);auto* other=entry(host);assert(e&&other);
    assert(e->magic==magic&&e->uniqueID==identity&&e->numParams==26&&e->numOutputs==2);
    assert(e->dispatcher(e,0,0,0,nullptr,0));
    e->dispatcher(e,10,0,0,nullptr,44100);e->dispatcher(e,11,0,128,nullptr,0);e->dispatcher(e,12,0,1,nullptr,0);
    clock_info.ppqPos=0.25;clock_info.tempo=120;clock_info.flags=2|(1<<9)|(1<<10);
    float left[128],right[128];float* out[]{left,right};
    for(auto& v:left)v=1;for(auto& v:right)v=1;
    e->processReplacing(e,nullptr,out,128);for(auto v:left)assert(v==0);for(auto v:right)assert(v==0);
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
    // Host automation may arrive concurrently; atomic parameters avoid an audio lock.
    std::thread automation([&]{for(int i=0;i<10000;++i)e->setParameter(e,2,float(i%100)/100);});
    for(int i=0;i<500;++i)e->processReplacing(e,nullptr,out,128);automation.join();
    e->dispatcher(e,12,0,0,nullptr,0);telemetry(e,&t);assert(!t.armed);
    for(auto& v:left)v=0.5f;e->process(e,nullptr,out,128);for(auto v:left)assert(v==0.5f);
    telemetry(other,&t);assert(!t.armed);
    e->dispatcher(e,1,0,0,nullptr,0);other->dispatcher(other,1,0,0,nullptr,0);assert(!dlclose(lib));
}
