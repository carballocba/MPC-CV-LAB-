#include "mpccv/capture.hpp"
#include <time.h>
#include <limits>

namespace mpccv {
namespace {
// Some no-SDK shared builds omit CRT fini glue for __cxa_atexit.
// Use module ownership instead of a process-exit callback into unloaded code.
// These C-initialized globals have no automatic C++ destructor registration.
pthread_mutex_t creation_mutex=PTHREAD_MUTEX_INITIALIZER;
CaptureCoordinator* module_coordinator=nullptr;
__attribute__((destructor)) void release_capture_module() noexcept {
    delete module_coordinator;module_coordinator=nullptr;
}
uint64_t now_ms() noexcept {
    timespec time{};
    if(clock_gettime(CLOCK_MONOTONIC,&time)!=0)return std::numeric_limits<uint64_t>::max();
    return uint64_t(time.tv_sec)*1000+uint64_t(time.tv_nsec)/1000000;
}
}
IoResult CaptureSession::Sink::write(const uint8_t* bytes,size_t size) noexcept {
    if(size>48 || size%6)return {IoStatus::fatal,0};
    const auto meta=session.pipeline.packet_metadata();
    auto& record=session.history[session.next_record];
    record={};record.sequence=++session.sequence;record.sample=meta.sample;record.role=meta.role;record.length=uint32_t(size);
    for(size_t i=0;i<size;++i)record.bytes[i]=bytes[i];
    session.next_record=(session.next_record+1)%capture_history_size;
    if(session.record_count<capture_history_size)++session.record_count;
    else if(session.overwritten<std::numeric_limits<uint32_t>::max())++session.overwritten;
    return {IoStatus::progress,size};
}
void CaptureSession::publish(const Frame& frame,uint8_t mask) noexcept {
    if(!mask || !producer_ok)return;
    if(!pipeline.publish(frame,mask)) {producer_ok=false;return;}
    for(unsigned j=0;j<outputs;++j)if(mask&(1u<<j))last_sent[j]=frame.values[j];
}
void CaptureSession::begin_block(uint64_t sample,bool armed,uint8_t enabled) noexcept {
    if(pipeline.state()!=PipelineState::active || pipeline.fault()!=PipelineFault::none) {producer_ok=false;return;}
    pipeline.heartbeat();
    Frame zero;zero.sample=sample;uint8_t mask=0;
    for(unsigned j=0;j<outputs;++j)
        if((reserved&(1u<<j)) && last_sent[j] && (!armed || !(enabled&(1u<<j))))mask|=1u<<j;
    publish(zero,mask);
}
void CaptureSession::gates(const Frame& frame,uint8_t gate_mask) noexcept {
    uint8_t changed=0;
    for(unsigned j=0;j<outputs;++j)
        if((reserved&gate_mask&(1u<<j)) && frame.values[j]!=last_sent[j])changed|=1u<<j;
    publish(frame,changed);
}
void CaptureSession::levels(const Frame& frame,uint8_t gate_mask) noexcept {
    uint8_t changed=0;
    for(unsigned j=0;j<outputs;++j)
        if((reserved&(1u<<j)) && !(gate_mask&(1u<<j)) && frame.values[j]!=last_sent[j])changed|=1u<<j;
    publish(frame,changed);
}
CaptureCoordinator& capture_coordinator() {
    pthread_mutex_lock(&creation_mutex);
    try {
        if(!module_coordinator)module_coordinator=new CaptureCoordinator;
    } catch(...) {pthread_mutex_unlock(&creation_mutex);throw;}
    auto* result=module_coordinator;
    pthread_mutex_unlock(&creation_mutex);
    return *result;
}
CaptureCoordinator::~CaptureCoordinator() {
    quit.store(1);
    if(worker_running)pthread_join(worker,nullptr);
}
CaptureResult CaptureCoordinator::attach(CaptureSession& session) {
    std::lock_guard<std::mutex> life(lifecycle);
    std::lock_guard<std::mutex> lock(registry);
    if(worker_running && quit.load())return CaptureResult::unavailable;
    if(session.owner || session.pipeline.state()!=PipelineState::idle || session.pipeline.fault()!=PipelineFault::none)return CaptureResult::invalid;
    if(!session.reserved || !next_owner || count==slots.size())return CaptureResult::unavailable;
    auto token=next_owner++; // Never reuse a token after uint32 wrap.
    if(!leases.claim(token,session.reserved))return CaptureResult::conflict;
    session.owner=token;
    if(!session.pipeline.request_start())return CaptureResult::unavailable;
    // Capture sink completes immediately. No worker/producer has this session yet.
    session.pipeline.tick(session.sink,std::numeric_limits<uint64_t>::max(),now_ms());
    if(session.pipeline.state()!=PipelineState::active)return CaptureResult::unavailable;
    if(!worker_running) {
        quit.store(0);
        // Keep thread ownership explicit and avoid an unloadable static C++
        // thread runtime registering process-exit callbacks in the host.
        int error=pthread_create(&worker,nullptr,[](void* context)->void* {
            static_cast<CaptureCoordinator*>(context)->loop();return nullptr;
        },this);
        if(error) {
            leases.drain(token);session.pipeline.request_stop();
            session.pipeline.tick(session.sink,std::numeric_limits<uint64_t>::max(),now_ms());
            if(session.pipeline.safe_acknowledged())leases.acknowledge(token,session.reserved);
            return CaptureResult::unavailable;
        }
        worker_running=true;
    }
    for(auto& slot:slots)if(!slot) {slot=&session;++count;break;}
    return CaptureResult::ok;
}
CaptureResult CaptureCoordinator::detach(CaptureSession& session) {
    std::lock_guard<std::mutex> life(lifecycle);
    bool join=false;
    {
        std::lock_guard<std::mutex> lock(registry);
        auto found=slots.end();
        for(auto it=slots.begin();it!=slots.end();++it)if(*it==&session){found=it;break;}
        if(found==slots.end())return CaptureResult::invalid;
        leases.drain(session.owner);
        // Producer is quiesced by CapturePort's reader barrier. Consumer transfers
        // to this control thread under registry, while worker cannot touch session.
        session.pipeline.request_stop();
        for(unsigned i=0;i<3 && session.pipeline.state()!=PipelineState::stopped && session.pipeline.state()!=PipelineState::fault;++i)
            session.pipeline.tick(session.sink,std::numeric_limits<uint64_t>::max(),now_ms());
        if(session.pipeline.safe_acknowledged())leases.acknowledge(session.owner,session.reserved);
        *found=nullptr;--count;
        if(!count) {quit.store(1);join=true;}
    }
    if(join && worker_running) {
        if(pthread_join(worker,nullptr)!=0)return CaptureResult::unavailable;
        worker_running=false;
    }
    return CaptureResult::ok;
}
void CaptureCoordinator::loop() noexcept {
    while(!quit.load()) {
        {
            std::lock_guard<std::mutex> lock(registry);
            for(auto* session:slots)if(session) {
                for(unsigned budget=0;budget<64;++budget) {
                    auto before=session->pipeline.delivered();
                    session->pipeline.tick(session->sink,std::numeric_limits<uint64_t>::max(),now_ms());
                    if(before==session->pipeline.delivered())break;
                }
            }
        }
        // No wake syscall from audio. This delay only paces the capture worker;
        // logical timestamps are preserved, not scheduled as electrical pulses.
        timespec delay{0,1000000};nanosleep(&delay,nullptr);
    }
}
CaptureStatus CaptureCoordinator::status_locked(const CaptureSession& session) const noexcept {
    CaptureStatus status;status.state=uint32_t(session.pipeline.state());status.fault=uint32_t(session.pipeline.fault());
    status.mask=session.reserved;status.owner=session.owner;status.delivered=session.pipeline.delivered();
    status.safe_acknowledged=session.pipeline.safe_acknowledged()?1u:0u;
    status.history_count=uint32_t(session.record_count);status.history_overwritten=session.overwritten;
    status.active_sessions=uint32_t(count);status.accepted=session.pipeline.accepted();return status;
}
CaptureStatus CaptureCoordinator::status(const CaptureSession& session) {
    std::lock_guard<std::mutex> lock(registry);return status_locked(session);
}
size_t CaptureCoordinator::copy(const CaptureSession& session,CaptureRecord* dest,size_t capacity) {
    if(!dest || !capacity)return 0;
    std::lock_guard<std::mutex> lock(registry);
    size_t count_copy=capacity<session.record_count?capacity:session.record_count;
    size_t first=(session.next_record+capture_history_size-count_copy)%capture_history_size;
    for(size_t i=0;i<count_copy;++i)dest[i]=session.history[(first+i)%capture_history_size];
    return count_copy;
}
CapturePort::~CapturePort() {
    // VST lifecycle requires callbacks to have ended before effClose.
    current.store(nullptr);
    if(registered && owned)capture_coordinator().detach(*owned);
}
CaptureResult CapturePort::start(uint32_t mask) {
    if(!mask || mask>255)return CaptureResult::invalid;
    std::lock_guard<std::mutex> lock(control);
    if(registered || readers.load())return CaptureResult::busy;
    std::unique_ptr<CaptureSession> session(new(std::nothrow) CaptureSession(uint8_t(mask)));
    if(!session)return CaptureResult::unavailable;
    auto result=capture_coordinator().attach(*session);
    if(result!=CaptureResult::ok)return result;
    owned=std::move(session);registered=true;
    current.store(owned.get());return CaptureResult::ok;
}
CaptureResult CapturePort::stop(CaptureStatus* last) {
    std::lock_guard<std::mutex> lock(control);
    current.store(nullptr);
    if(readers.load()) {if(last && owned)*last=capture_coordinator().status(*owned);return CaptureResult::busy;}
    if(!registered || !owned) {
        if(last)*last=owned?capture_coordinator().status(*owned):CaptureStatus{};
        return CaptureResult::ok;
    }
    auto result=capture_coordinator().detach(*owned);registered=false;
    if(last)*last=capture_coordinator().status(*owned);
    return result;
}
CaptureStatus CapturePort::status() {
    std::lock_guard<std::mutex> lock(control);
    return owned?capture_coordinator().status(*owned):CaptureStatus{};
}
size_t CapturePort::copy(CaptureRecord* dest,size_t capacity) {
    std::lock_guard<std::mutex> lock(control);
    return owned?capture_coordinator().copy(*owned,dest,capacity):0;
}
}
