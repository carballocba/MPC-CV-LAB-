#include "mpccv/backend.hpp"
#include <cassert>
using namespace mpccv;
struct Script: Backend {
    CaptureBackend capture; unsigned call=0; bool fail=false;
    IoResult write(const uint8_t* p,size_t n) noexcept override {
        ++call;
        if(fail)return {IoStatus::disconnected,0};
        if(call==1)return {IoStatus::interrupted,0};
        if(call==2)return {IoStatus::again,0};
        if(call==3)return {IoStatus::progress,0};
        return capture.write(p,n>5?5:n);
    }
};
int main() {
    Frame f; f.values.fill(6554); Script w; ReliablePacket p;
    assert(p.load(f)); assert(!p.load(f));
    Transfer t=Transfer::pending;
    for(int i=0;i<32 && t!=Transfer::complete;++i) t=p.pump(w);
    assert(t==Transfer::complete && w.capture.used==48);
    for(unsigned j=0;j<8;++j) {auto bytes=encode(j,6554); for(unsigned k=0;k<6;++k) assert(w.capture.bytes[j*6+k]==bytes[k]);}
    assert(p.load(f)); w.fail=true; assert(p.pump(w)==Transfer::fault);
    assert(!p.load(f)); assert(p.pump(w)==Transfer::fault);
    p.reset_connection(); assert(p.load(Frame{}));
    Leases l; assert(l.claim(1,3)); assert(!l.claim(2,6)); assert(l.claim(2,4));
    assert(l.drain(1)==3); assert(!l.claim(1,1)); assert(!l.claim(3,3));
    assert(!l.acknowledge(2,3)); assert(l.acknowledge(1,3)); assert(l.claim(3,3));
    OutputService s; Script backend;
    assert(!s.submit(f)); assert(s.arm()); assert(s.submit(f)); assert(!s.submit(f));
    s.tick(backend); s.shutdown(); assert(!s.submit(f));
    for(int i=0;i<64 && s.state!=ServiceState::stopped;++i) s.tick(backend);
    assert(s.state==ServiceState::stopped && s.delivered==2 && backend.capture.used==96);
    for(size_t i=48;i<96;i+=6) assert(backend.capture.bytes[i+2]==0 && backend.capture.bytes[i+5]==0);
    s.new_connection(); assert(s.state==ServiceState::disarmed); assert(s.arm());
    assert(s.submit(f)); backend.fail=true; s.tick(backend); assert(s.state==ServiceState::fault);
    s.shutdown(); assert(s.state==ServiceState::fault); s.new_connection(); s.shutdown(); s.timeout();
    assert(s.state==ServiceState::fault);
}
