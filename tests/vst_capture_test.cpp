#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/vst/capture_api.hpp"
#include "mpccv/scene.hpp"
#include <cassert>
#include <chrono>
#include <dlfcn.h>
#include <thread>
using namespace mpccv;
using namespace mpccv::vst;
thread_local Time clock_info{};
intptr_t host(Effect*,int32_t op,int32_t,intptr_t,void*,float) {
    if(op==1)return 2400;
    if(op==7)return reinterpret_cast<intptr_t>(&clock_info);
    return 0;
}
void clock_at(double ppq) {
    clock_info={};clock_info.ppqPos=ppq;clock_info.tempo=120;clock_info.flags=2|(1<<9)|(1<<10);
}
void setup(Effect* effect,uint8_t output) {
    Scene scene;scene.count=1;scene.enabled=uint8_t(1u<<output);
    scene.nodes[0].kind=Kind::gate;scene.nodes[0].rate=4;scene.nodes[0].scale=5;
    scene.routes[output].source=0;
    SceneBytes bytes;assert(save_scene(scene,bytes));
    effect->dispatcher(effect,12,0,1,nullptr,0);
    assert(effect->dispatcher(effect,24,0,intptr_t(bytes.size),bytes.bytes.data(),0));
    clock_at(0);effect->processReplacing(effect,nullptr,nullptr,1);
    effect->setParameter(effect,0,1);
}
int main(int argc,char** argv) {
    assert(argc==2);void* library=dlopen(argv[1],RTLD_NOW);assert(library);
    auto entry=reinterpret_cast<Effect*(*)(Callback)>(dlsym(library,"VSTPluginMain"));
    auto start=reinterpret_cast<decltype(&mpc_cv_capture_start)>(dlsym(library,"mpc_cv_capture_start"));
    auto stop=reinterpret_cast<decltype(&mpc_cv_capture_stop)>(dlsym(library,"mpc_cv_capture_stop"));
    auto status=reinterpret_cast<decltype(&mpc_cv_capture_status)>(dlsym(library,"mpc_cv_capture_status"));
    auto copy=reinterpret_cast<decltype(&mpc_cv_capture_copy)>(dlsym(library,"mpc_cv_capture_copy"));
    assert(entry && start && stop && status && copy);
    auto* a=entry(host);auto* b=entry(host);assert(a && b);
    CaptureStatus snapshot;assert(!start(nullptr,1));assert(!start(a,256));
    assert(status(a,&snapshot) && snapshot.state==0 && snapshot.mask==0);
    setup(a,0);setup(b,7);
    assert(start(a,1)==1);assert(start(b,1)==-1);assert(start(b,128)==1);
    assert(status(a,&snapshot) && snapshot.active_sessions==2);
    clock_at(0);a->processReplacing(a,nullptr,nullptr,1);
    clock_at(.2);a->processReplacing(a,nullptr,nullptr,1);
    clock_at(0);b->processReplacing(b,nullptr,nullptr,1);
    clock_at(.2);b->processReplacing(b,nullptr,nullptr,1);
    for(unsigned i=0;i<200;++i) {
        status(a,&snapshot);if(snapshot.delivered>=3)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(snapshot.delivered>=3);
    std::array<CaptureRecord,capture_history_size> records{};
    auto count=copy(a,records.data(),uint32_t(records.size()));assert(count==3);
    assert(records[1].role==1 && records[2].role==1);
    assert(records[2].sample==records[1].sample+1); // one-sample synthetic host gate retained
    assert(records[1].bytes[2]==127 && records[1].bytes[5]==127);
    assert(records[2].bytes[2]==0 && records[2].bytes[5]==0);
    assert(copy(a,records.data(),uint32_t(records.size()+1))==0);
    assert(stop(a,&snapshot)==1 && snapshot.safe_acknowledged && snapshot.active_sessions==1);
    count=copy(a,records.data(),uint32_t(records.size()));assert(count==4 && records[3].role==2);
    assert(records[3].bytes[2]==0 && records[3].bytes[5]==0);
    assert(stop(b,&snapshot)==1 && snapshot.safe_acknowledged && snapshot.active_sessions==0);
    // Default path stays inert. Capture starts only by explicit control API and is not preset state.
    assert(start(a,1)==1);a->setParameter(a,0,1);
    std::thread audio([&] {
        clock_at(0);
        for(unsigned i=0;i<500;++i) {
            a->processReplacing(a,nullptr,nullptr,128);
            clock_info.ppqPos+=128*2.0/44100;
        }
    });
    int result=stop(a,&snapshot);assert(result==1 || result==-2);
    audio.join();assert(stop(a,&snapshot)==1 && snapshot.safe_acknowledged);
    // Two render threads have separate producers/graphs but share one capture worker.
    assert(start(a,1)==1);assert(start(b,128)==1);
    auto render=[&](Effect* e) {
        e->setParameter(e,0,1);clock_at(0);
        for(unsigned i=0;i<64;++i){e->processReplacing(e,nullptr,nullptr,128);clock_info.ppqPos+=128*2.0/44100;}
    };
    std::thread a_audio(render,a),b_audio(render,b);a_audio.join();b_audio.join();
    // effClose must stop/join even if the client omitted explicit capture_stop.
    a->dispatcher(a,1,0,0,nullptr,0);b->dispatcher(b,1,0,0,nullptr,0);
    assert(dlclose(library)==0);
    // Repeated load, capture, implicit stop and unload leaves no worker using unloaded code.
    for(unsigned i=0;i<8;++i) {
        library=dlopen(argv[1],RTLD_NOW);assert(library);
        entry=reinterpret_cast<Effect*(*)(Callback)>(dlsym(library,"VSTPluginMain"));
        start=reinterpret_cast<decltype(start)>(dlsym(library,"mpc_cv_capture_start"));
        auto* effect=entry(host);assert(effect);assert(start(effect,255)==1);
        effect->dispatcher(effect,1,0,0,nullptr,0);assert(dlclose(library)==0);
    }
}
