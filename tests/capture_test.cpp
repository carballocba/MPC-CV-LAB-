#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/capture.hpp"
#include <cassert>
#include <chrono>
#include <thread>
using namespace mpccv;
int main() {
    // First registry construction and last-worker join can come from different
    // control threads. Each port has its own producer and private owner token.
    std::array<CapturePort,8> ports;
    std::array<CaptureResult,8> results{};
    std::array<std::thread,8> controls;
    for(unsigned i=0;i<8;++i)controls[i]=std::thread([&,i]{results[i]=ports[i].start(1u<<i);});
    for(auto& thread:controls)thread.join();
    for(auto result:results)assert(result==CaptureResult::ok);
    assert(ports[0].status().active_sessions==8);
    for(unsigned i=0;i<8;++i)controls[i]=std::thread([&,i]{results[i]=ports[i].stop();});
    for(auto& thread:controls)thread.join();
    for(auto result:results)assert(result==CaptureResult::ok);
    assert(ports[0].status().active_sessions==0);
    CapturePort a,b;
    assert(a.start(0)==CaptureResult::invalid);
    assert(a.start(3)==CaptureResult::ok);
    assert(b.start(2)==CaptureResult::conflict);
    assert(b.start(128)==CaptureResult::ok);
    assert(a.status().active_sessions==2);
    {
        CapturePort::Reader reader(a);assert(reader.get());
        Frame high;high.sample=10;high.values[0]=16383;
        reader.get()->begin_block(10,true,3);reader.get()->gates(high,1);
        Frame low;low.sample=11;reader.get()->gates(low,1);
        assert(a.stop()==CaptureResult::busy); // detached from future callbacks, reader still owns session
        assert(a.start(3)==CaptureResult::busy);
        for(unsigned i=0;i<200 && a.status().delivered<3;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(a.status().delivered>=3);
    }
    CaptureStatus stopped;assert(a.stop(&stopped)==CaptureResult::ok);
    assert(stopped.safe_acknowledged && stopped.active_sessions==1);
    std::array<CaptureRecord,8> records{};auto count=a.copy(records.data(),records.size());assert(count==4);
    assert(records[0].role==0 && records[1].role==1 && records[2].role==1 && records[3].role==2);
    assert(records[1].sample==10 && records[2].sample==11);
    assert(records[1].bytes[2]==127 && records[1].bytes[5]==127);
    assert(records[2].bytes[2]==0 && records[2].bytes[5]==0);
    for(size_t i=0;i<count;++i)for(unsigned j=0;j<records[i].length;j+=6)assert(records[i].bytes[j+1]<=2);
    assert(a.start(128)==CaptureResult::conflict);
    assert(a.start(3)==CaptureResult::ok);assert(a.stop()==CaptureResult::ok);
    assert(b.stop()==CaptureResult::ok);assert(b.status().active_sessions==0);
    // Last session teardown joins the worker. A fresh attach restarts it safely.
    for(unsigned i=0;i<20;++i){CapturePort c;assert(c.start(255)==CaptureResult::ok);}
    // Rolling history reports overwrite separately from delivery; bounded capture never blocks audio.
    CapturePort rolling;assert(rolling.start(1)==CaptureResult::ok);
    {
        CapturePort::Reader reader(rolling);assert(reader.get());
        reader.get()->begin_block(0,true,1);
        for(unsigned i=0;i<1200;++i) {Frame frame;frame.sample=i;frame.values[0]=i%2?0:16383;reader.get()->gates(frame,1);}
    }
    for(unsigned i=0;i<200 && rolling.status().delivered<1201;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(rolling.status().delivered==1201);
    assert(rolling.stop(&stopped)==CaptureResult::ok && stopped.safe_acknowledged);
    assert(stopped.history_count==capture_history_size && stopped.history_overwritten==690);
    std::array<CaptureRecord,capture_history_size> recent{};
    assert(rolling.copy(recent.data(),recent.size())==recent.size());
    assert(recent.back().sequence==1202 && recent.back().role==2);
    // Watchdog zeroes an abandoned producer; safe fault then releases its lease on stop.
    CapturePort watchdog;assert(watchdog.start(1)==CaptureResult::ok);
    for(unsigned i=0;i<1000 && watchdog.status().fault==0;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(watchdog.status().fault==uint32_t(PipelineFault::heartbeat_timeout));
    assert(watchdog.status().safe_acknowledged);
    assert(watchdog.stop()==CaptureResult::ok);
    CapturePort reuse;assert(reuse.start(1)==CaptureResult::ok);assert(reuse.stop()==CaptureResult::ok);
}
