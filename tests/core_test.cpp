#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/core.hpp"
#include <cassert>
#include <limits>
#include <thread>
#include <vector>
int main() {
    using namespace mpccv;
    assert(code(4.99999)==16383);
    for(unsigned i=0;i<=100000;++i)assert(code(double(i)/20000)<=16383);
    assert(code(-1)==0 && code(5)==16383 && code(2)==6554);
    assert(code(std::numeric_limits<double>::quiet_NaN())==0);
    assert(code(std::numeric_limits<double>::infinity())==0);
    for(unsigned j=0;j<8;++j) for(unsigned v=0;v<16384;++v) {
        auto p=encode(j,v);
        assert(p[0]==0xbf && p[3]==0xbf && p[1]==j*2 && p[4]==j*2+1);
        assert(((unsigned(p[2])<<7)|p[5])==v);
    }
    Broker b; assert(b.claim(1,3)); assert(!b.claim(2,6));
    assert(b.claim(2,4)); assert(b.release(1)==3); assert(b.claim(3,3));
    assert(b.release(1)==0); assert(!b.claim(0,8));
    Queue<4> q; Frame f; assert(!q.pop(f));
    assert(q.push(f)&&q.push(f)&&q.push(f)&&!q.push(f));
    assert(q.pop(f)&&q.pop(f)&&q.pop(f)&&!q.pop(f));
    Queue<64> concurrent;
    std::thread producer([&]{for(uint64_t i=0;i<100000;++i){Frame x; x.sample=i; while(!concurrent.push(x)) {}}});
    for(uint64_t i=0;i<100000;++i){while(!concurrent.pop(f)) {} assert(f.sample==i);}
    producer.join();
    Engine e; Transport t{true,true,0.25,120,44100};
    e.lanes[0].source=Source::gate;
    assert(e.render(t,0).values[0]==0); e.armed=true;
    assert(e.render(t,0).values[0]==16383);
    t.playing=false; assert(e.render(t,0).values[0]==0); t.playing=true;
    t.valid=false; assert(e.render(t,0).values[0]==0); t.valid=true;
    e.lanes[4].source=Source::gate; assert(e.render(t,0).values[4]==0);
    e.enabled=255; assert(e.render(t,0).values[4]==16383);
    e.lanes[0].source=Source::random_hold;
    assert(e.render(t,0).values==e.render(t,0).values);
    t.ppq=-1; assert(e.render(t,0).values[0]==0);
    struct Writer { std::vector<uint8_t> data; int calls=0;
        int write(const uint8_t* p,size_t n) { ++calls; if(calls==1)return 0; if(calls==2)return -1;
            size_t k=n>5?5:n; data.insert(data.end(),p,p+k); return int(k); }} w;
    f.values.fill(6554); Packet p; p.load(f);
    assert(!p.pump(w)); assert(!p.pump(w)); while(!p.pump(w)) {}
    assert(w.data.size()==48);
    for(unsigned j=0;j<8;++j) {auto expected=encode(j,6554);
        for(unsigned k=0;k<6;++k) assert(w.data[j*6+k]==expected[k]);}
}
