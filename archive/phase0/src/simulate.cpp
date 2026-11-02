#include "mpccv/core.hpp"
#include <iostream>
int main() {
    mpccv::Engine e; e.armed=true;
    e.lanes[0].source=mpccv::Source::sine;
    e.lanes[1].source=mpccv::Source::gate;
    e.lanes[2].source=mpccv::Source::sequence;
    e.lanes[3].source=mpccv::Source::random_hold;
    std::cout<<"sample,ppq,cv1,cv2,cv3,cv4\n";
    for(uint64_t s=0;s<44100*4;s+=128) {
        mpccv::Transport t{true,true,double(s)*2/44100,120,44100};
        auto f=e.render(t,s);
        std::cout<<s<<','<<t.ppq;
        for(unsigned j=0;j<4;++j) std::cout<<','<<f.values[j];
        std::cout<<'\n';
    }
}
