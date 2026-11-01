#pragma once
#include "backend.hpp"

namespace mpccv {
struct OutputEvent { Frame frame{}; uint8_t mask=0; };
enum class PipelineState:uint32_t { idle, starting, active, zeroing, stopped, fault };
enum class PipelineFault:uint32_t { none, overflow, invalid_event, timestamp, transport, write_timeout, heartbeat_timeout, clock };
// Capture/worker abstraction only: no device access and no thread creation.
// Exactly one producer owns request_start/publish/heartbeat/request_stop.
// Exactly one consumer owns tick. A session is single-use; destroy only after both stop.
template<size_t N> class EventPipeline {
    Queue<N,OutputEvent> events;
    ReliablePacket packet;
    const uint8_t reserved;
    const uint64_t timeout_ms, heartbeat_ms;
    std::atomic<uint32_t> state_{uint32_t(PipelineState::idle)}, fault_{0};
    std::atomic<uint32_t> start_{0}, stop_{0}, heartbeat_{0}, delivered_{0}, safe_{0};
    // Producer-owned fields.
    uint64_t last_sample=0;
    bool have_sample=false;
    // Consumer-owned fields.
    OutputEvent held{};
    bool have_held=false, clock_started=false;
    uint64_t last_ms=0, packet_ms=0, heartbeat_seen_ms=0;
    uint32_t heartbeat_seen=0;
    enum class PacketKind { prime, event, safe } kind=PacketKind::prime;
    void set_state(PipelineState s) noexcept {state_.store(uint32_t(s),std::memory_order_release);}
    void fail(PipelineFault f) noexcept {
        uint32_t expected=0;
        fault_.compare_exchange_strong(expected,uint32_t(f),std::memory_order_acq_rel);
    }
    void discard() noexcept {
        have_held=false;
        OutputEvent ignored;
        // Bounded even if a producer is racing the fault notification.
        for(size_t i=0;i<N-1 && events.pop(ignored);++i) {}
    }
    bool load(const Frame& f,uint8_t mask,PacketKind packet_kind,uint64_t ms) noexcept {
        if(!packet.load(f,mask)) {fail(PipelineFault::invalid_event);set_state(PipelineState::fault);return false;}
        kind=packet_kind;packet_ms=ms;return true;
    }
public:
    explicit EventPipeline(uint8_t reserved_mask=255,uint64_t write_timeout_ms=100,uint64_t watchdog_ms=250) noexcept
      :reserved(reserved_mask),timeout_ms(write_timeout_ms?write_timeout_ms:1),heartbeat_ms(watchdog_ms?watchdog_ms:1) {}
    PipelineState state() const noexcept {return PipelineState(state_.load(std::memory_order_acquire));}
    PipelineFault fault() const noexcept {return PipelineFault(fault_.load(std::memory_order_acquire));}
    uint32_t delivered() const noexcept {return delivered_.load(std::memory_order_acquire);}
    bool safe_acknowledged() const noexcept {return safe_.load(std::memory_order_acquire)!=0;}
    bool request_start() noexcept {
        if(state()!=PipelineState::idle || start_.load() || stop_.load())return false;
        if(!reserved) {fail(PipelineFault::invalid_event);return false;}
        start_.store(1,std::memory_order_release);return true;
    }
    void request_stop() noexcept {stop_.store(1,std::memory_order_release);}
    void heartbeat() noexcept {heartbeat_.fetch_add(1,std::memory_order_relaxed);}
    bool publish(const Frame& frame,uint8_t mask) noexcept {
        if(state()!=PipelineState::active || fault()!=PipelineFault::none || stop_.load(std::memory_order_acquire))return false;
        if(!mask || (mask&reserved)!=mask) {fail(PipelineFault::invalid_event);return false;}
        for(unsigned i=0;i<outputs;++i)if((mask&(1u<<i)) && frame.values[i]>16383) {
            fail(PipelineFault::invalid_event);return false;
        }
        if(have_sample && frame.sample<last_sample) {fail(PipelineFault::timestamp);return false;}
        if(!events.push(OutputEvent{frame,mask})) {fail(PipelineFault::overflow);return false;}
        last_sample=frame.sample;have_sample=true;heartbeat();return true;
    }
    // One bounded backend call per tick. Caller supplies host-audio sample position
    // and monotonic worker time; neither is obtained on the producer/audio path.
    void tick(Backend& backend,uint64_t due_sample,uint64_t ms) noexcept {
        auto s=state();
        if(s==PipelineState::stopped || s==PipelineState::fault)return;
        if(clock_started && ms<last_ms) {fail(PipelineFault::clock);set_state(PipelineState::fault);return;}
        clock_started=true;last_ms=ms;
        if(s==PipelineState::idle) {
            if(fault()!=PipelineFault::none) {set_state(PipelineState::fault);return;}
            if(!start_.load(std::memory_order_acquire))return;
            set_state(PipelineState::starting);
            if(!load(Frame{},reserved,PacketKind::prime,ms))return;
        }
        if(state()==PipelineState::active) {
            auto epoch=heartbeat_.load(std::memory_order_relaxed);
            if(epoch!=heartbeat_seen) {heartbeat_seen=epoch;heartbeat_seen_ms=ms;}
            if(ms-heartbeat_seen_ms>=heartbeat_ms)fail(PipelineFault::heartbeat_timeout);
        }
        bool closing=stop_.load(std::memory_order_acquire) || fault()!=PipelineFault::none;
        if(closing) {discard();set_state(PipelineState::zeroing);}
        if(!packet.pending()) {
            if(closing) {
                if(!load(Frame{},reserved,PacketKind::safe,ms))return;
            } else if(state()==PipelineState::active) {
                if(!have_held)have_held=events.pop(held);
                if(!have_held || held.frame.sample>due_sample)return;
                if(!load(held.frame,held.mask,PacketKind::event,ms))return;
                have_held=false;
            } else return;
        }
        if(ms-packet_ms>=timeout_ms) {fail(PipelineFault::write_timeout);set_state(PipelineState::fault);return;}
        auto result=packet.pump(backend);
        if(result==Transfer::fault) {fail(PipelineFault::transport);set_state(PipelineState::fault);return;}
        if(result!=Transfer::complete)return;
        delivered_.fetch_add(1,std::memory_order_release);
        closing=stop_.load(std::memory_order_acquire) || fault()!=PipelineFault::none;
        if(kind==PacketKind::safe || (kind==PacketKind::prime && closing)) {
            safe_.store(1,std::memory_order_release);
            set_state(fault()==PipelineFault::none?PipelineState::stopped:PipelineState::fault);
        } else if(kind==PacketKind::prime) {
            heartbeat_seen=heartbeat_.load(std::memory_order_relaxed);heartbeat_seen_ms=ms;
            set_state(PipelineState::active);
        }
    }
};
}
