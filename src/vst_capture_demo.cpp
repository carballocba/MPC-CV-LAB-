#include "mpccv/vst/capture_api.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace mpccv;
using namespace mpccv::vst;
Time time_info{};
intptr_t host(Effect*,int32_t op,int32_t,intptr_t,void*,float) {
    if(op==1)return 2400;
    if(op==7)return reinterpret_cast<intptr_t>(&time_info);
    return 0;
}
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"usage: vst_capture_demo plugin.so trace.csv\n";return 1;}
    auto* library=dlopen(argv[1],RTLD_NOW);if(!library)return 2;
    auto entry=reinterpret_cast<Effect*(*)(Callback)>(dlsym(library,"VSTPluginMain"));
    auto start=reinterpret_cast<decltype(&mpc_cv_capture_start)>(dlsym(library,"mpc_cv_capture_start"));
    auto stop=reinterpret_cast<decltype(&mpc_cv_capture_stop)>(dlsym(library,"mpc_cv_capture_stop"));
    auto status=reinterpret_cast<decltype(&mpc_cv_capture_status)>(dlsym(library,"mpc_cv_capture_status"));
    auto copy=reinterpret_cast<decltype(&mpc_cv_capture_copy)>(dlsym(library,"mpc_cv_capture_copy"));
    if(!entry || !start || !stop || !status || !copy)return 3;
    auto* effect=entry(host);if(!effect)return 4;
    std::ofstream csv(argv[2]);if(!csv)return 5;
    csv<<"sequence,sample,role,output,code\n";
    uint32_t seen=0, gaps=0;bool complete=true;
    std::array<CaptureRecord,capture_history_size> records{};
    auto collect=[&] {
        auto count=copy(effect,records.data(),uint32_t(records.size()));
        for(uint32_t i=0;i<count;++i) {
            const auto& record=records[i];if(record.sequence<=seen)continue;
            if(record.sequence!=seen+1) {complete=false;++gaps;}
            seen=record.sequence;
            for(unsigned byte=0;byte<record.length;byte+=6) {
                if(record.bytes[byte]!=0xbf || record.bytes[byte+3]!=0xbf || record.bytes[byte+4]!=record.bytes[byte+1]+1)complete=false;
                unsigned output=record.bytes[byte+1]/2+1;
                unsigned value=unsigned(record.bytes[byte+2])*128+record.bytes[byte+5];
                csv<<record.sequence<<','<<record.sample<<','<<record.role<<','<<output<<','<<value<<'\n';
            }
        }
    };
    CaptureStatus info;
    auto drain=[&] {
        // A quiesced producer has an exact target; scheduler silence is not a drain acknowledgement.
        if(!status(effect,&info))return false;
        auto target=info.accepted+1; // one priming packet plus accepted events
        for(unsigned i=0;i<1000;++i) {
            collect();
            if(!status(effect,&info) || info.fault)return false;
            if(info.delivered>=target)return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    };
    effect->dispatcher(effect,12,0,1,nullptr,0);
    if(start(effect,255)!=1)return 6;
    effect->setParameter(effect,1,1);effect->setParameter(effect,0,1);
    time_info.tempo=120;time_info.flags=2|(1<<9)|(1<<10);
    for(uint64_t sample=0;sample<176400;sample+=128) {
        time_info.ppqPos=double(sample)*2/44100;
        int count=int(176400-sample<128?176400-sample:128);
        effect->processReplacing(effect,nullptr,nullptr,count);
        collect(); // Diagnostics/file I/O on this offline host, outside render.
        // Bound diagnostic backlog for a non-real-time, faster-than-audio host.
        if((sample/128)%16==15 && !drain())complete=false;
    }
    if(!drain())complete=false;
    time_info.flags=(1<<9)|(1<<10);
    effect->processReplacing(effect,nullptr,nullptr,128);
    if(!drain())complete=false;
    // Capture stop itself appends a complete reserved-output zero packet.
    if(stop(effect,&info)!=1 || !info.safe_acknowledged)complete=false;
    collect();if(seen!=info.delivered)complete=false;
    csv.flush();if(!csv)complete=false;
    effect->dispatcher(effect,1,0,0,nullptr,0);if(dlclose(library)!=0)complete=false;
    if(!complete){std::cerr<<"capture incomplete: fault="<<info.fault<<" seen="<<seen<<" delivered="<<info.delivered<<" accepted="<<info.accepted<<" gaps="<<gaps<<"\n";return 7;}
    std::cout<<"PASS integrated VST capture: "<<seen<<" packets, "<<info.history_overwritten<<" history overwrites, complete CSV\n";
}
