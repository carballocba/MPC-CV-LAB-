#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/vst/abi.hpp"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <new>
// Instrument allocation on the thread driving the processing callback.
thread_local bool in_render=false;
void* operator new(std::size_t n) {assert(!in_render);if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {assert(!in_render);std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace mpccv::vst;
Time ti{};
intptr_t host(Effect*,int32_t op,int32_t,intptr_t,void*,float) {
    if(op==1)return 2400;if(op==7)return reinterpret_cast<intptr_t>(&ti);return 0;
}
int main() {
    auto* e=VSTPluginMain(host);assert(e);
    e->dispatcher(e,12,0,1,nullptr,0);e->setParameter(e,0,1);e->setParameter(e,1,1);
    ti.flags=2|(1<<9)|(1<<10);ti.tempo=120;
    float a[2048],b[2048];float* output[]{a,b};
    for(unsigned i=0;i<1000;++i) {
        int frames=i%3==0?1:i%3==1?128:2048;
        e->setParameter(e,2,float(i%100)/100);
        in_render=true;e->processReplacing(e,nullptr,output,frames);in_render=false;
        ti.ppqPos+=frames*120.0/(60*44100);
    }
    e->dispatcher(e,1,0,0,nullptr,0);
}
