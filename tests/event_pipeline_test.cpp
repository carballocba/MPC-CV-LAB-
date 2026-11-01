#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/event_pipeline.hpp"
#include <cassert>
#include <thread>
using namespace mpccv;
struct Short:Backend {
    CaptureBackend capture; size_t max_write=5;
    IoStatus status=IoStatus::progress;
    IoResult write(const uint8_t* p,size_t n) noexcept override {
        if(status!=IoStatus::progress)return {status,0};
        return capture.write(p,n>max_write?max_write:n);
    }
};
template<size_t N> void prime(EventPipeline<N>& p,Backend& b) {
    assert(p.request_start());
    for(unsigned i=0;i<64 && p.state()!=PipelineState::active;++i)p.tick(b,0,i);
    assert(p.state()==PipelineState::active);
}
int main() {
    EventPipeline<8> p;Short b;prime(p,b);assert(b.capture.used==48);
    Frame high;high.sample=100;high.values[7]=16383;
    Frame low;low.sample=101;
    assert(p.publish(high,128));assert(p.publish(low,128));
    p.tick(b,99,20);assert(b.capture.used==48); // Do not send future gate edges early.
    for(unsigned i=0;i<4;++i)p.tick(b,100,21+i);
    assert(b.capture.used==54);
    auto bytes=encode(7,16383);for(unsigned i=0;i<6;++i)assert(b.capture.bytes[48+i]==bytes[i]);
    for(unsigned i=0;i<4;++i)p.tick(b,101,26+i);
    bytes=encode(7,0);for(unsigned i=0;i<6;++i)assert(b.capture.bytes[54+i]==bytes[i]);
    p.request_stop();assert(!p.publish(high,128));
    for(unsigned i=0;i<64 && p.state()!=PipelineState::stopped;++i)p.tick(b,102,31+i);
    assert(p.safe_acknowledged() && p.state()==PipelineState::stopped && b.capture.used==108);
    assert(!p.request_start());
    EventPipeline<3> overflow;Short ob;ob.max_write=48;prime(overflow,ob);
    assert(overflow.publish(high,128));assert(overflow.publish(low,128));assert(!overflow.publish(low,128));
    overflow.tick(ob,200,20);assert(overflow.state()==PipelineState::fault);
    assert(overflow.fault()==PipelineFault::overflow && overflow.safe_acknowledged() && ob.capture.used==96);
    EventPipeline<4> partial;Short pb;prime(partial,pb);
    assert(partial.publish(high,128));partial.tick(pb,100,20);partial.request_stop();
    for(unsigned i=0;i<64 && partial.state()!=PipelineState::stopped;++i)partial.tick(pb,100,21+i);
    assert(partial.safe_acknowledged() && pb.capture.used==102); // Complete partial CC stream before safe zeros.
    EventPipeline<4> order;Short order_backend;order_backend.max_write=48;prime(order,order_backend);
    assert(order.publish(low,128));assert(!order.publish(high,128));order.tick(order_backend,200,20);
    assert(order.fault()==PipelineFault::timestamp && order.safe_acknowledged());
    EventPipeline<4> disconnected;Short db;prime(disconnected,db);db.status=IoStatus::disconnected;
    disconnected.request_stop();disconnected.tick(db,0,20);
    assert(disconnected.state()==PipelineState::fault && !disconnected.safe_acknowledged());
    EventPipeline<4> timeout(255,10);Short tb;assert(timeout.request_start());tb.status=IoStatus::again;
    timeout.tick(tb,0,0);timeout.tick(tb,0,10);
    assert(timeout.fault()==PipelineFault::write_timeout && !timeout.safe_acknowledged());
    EventPipeline<4> watchdog(1,100,20);Short wb;wb.max_write=48;prime(watchdog,wb);
    watchdog.tick(wb,0,21);assert(watchdog.fault()==PipelineFault::heartbeat_timeout && watchdog.safe_acknowledged());
    EventPipeline<4> clock;Short cb;prime(clock,cb);clock.tick(cb,0,20);clock.tick(cb,0,19);
    assert(clock.fault()==PipelineFault::clock && !clock.safe_acknowledged());
    EventPipeline<4> reserved(1);Short rb;prime(reserved,rb);assert(!reserved.publish(high,128));reserved.tick(rb,200,20);reserved.tick(rb,200,21);
    assert(reserved.fault()==PipelineFault::invalid_event && rb.capture.used==12);
    // Two real threads, one producer/one worker. Waiting occurs in this test only.
    struct Counter:Backend {
        uint64_t count=0;IoResult write(const uint8_t*,size_t n) noexcept override {count+=n;return {IoStatus::progress,n};}
    } counter;
    EventPipeline<32> concurrent(1,100000,100000);prime(concurrent,counter);
    std::thread producer([&] {
        for(unsigned i=0;i<10000;++i) {
            Frame f;f.sample=i;f.values[0]=i%2?0:16383;
            assert(concurrent.publish(f,1));
            while(concurrent.delivered()<i+2)std::this_thread::yield();
        }
        concurrent.request_stop();
    });
    while(concurrent.state()!=PipelineState::stopped && concurrent.state()!=PipelineState::fault)concurrent.tick(counter,10000,0);
    producer.join();assert(concurrent.state()==PipelineState::stopped && concurrent.safe_acknowledged());
    assert(counter.count==60012);
}
