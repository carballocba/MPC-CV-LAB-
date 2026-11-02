#include "mpccv/graph.hpp"
#include "mpccv/backend.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"usage: lab_simulate output.csv capture.bin\n";return 1;}
    std::ofstream csv(argv[1]),midi(argv[2],std::ios::binary);if(!csv||!midi)return 2;
    mpccv::Graph graph;graph.configure(mpccv::demo_scene());graph.armed=true;
    mpccv::CaptureBackend capture;mpccv::OutputService service;service.arm();
    csv<<"sample,ppq,cv1,cv2,cv3,cv4,cv5,cv6,cv7,cv8\n";
    mpccv::Frame frame;
    for(uint64_t base=0;base<44100*4;base+=128) {
        for(unsigned offset=0;offset<128 && base+offset<44100*4;++offset) {
            auto index=base+offset;mpccv::Transport t{true,true,double(index)*2/44100,120,44100};
            frame=graph.sample(t,index);
        }
        if(!service.submit(frame))return 3;
        service.tick(capture);if(service.state==mpccv::ServiceState::fault)return 4;
        csv<<frame.sample<<','<<double(frame.sample)*2/44100;
        for(auto value:frame.values)csv<<','<<value;
        csv<<'\n';
        midi.write(reinterpret_cast<const char*>(capture.bytes.data()),std::streamsize(capture.used));capture.used=0;
    }
    service.shutdown();service.tick(capture);service.tick(capture);
    midi.write(reinterpret_cast<const char*>(capture.bytes.data()),std::streamsize(capture.used));
    if(service.state!=mpccv::ServiceState::stopped||!csv||!midi)return 5;
}
