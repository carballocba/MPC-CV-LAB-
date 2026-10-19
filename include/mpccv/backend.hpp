#pragma once
#include "core.hpp"

namespace mpccv {
enum class IoStatus { progress, again, interrupted, disconnected, fatal };
struct IoResult { IoStatus status; size_t count=0; };
class Backend {
public:
    virtual ~Backend()=default;
    // Worker/control context only. Production implementations must be bounded.
    virtual IoResult write(const uint8_t*,size_t) noexcept=0;
};
class CaptureBackend final: public Backend {
public:
    std::array<uint8_t, 48*256> bytes{};
    size_t used=0;
    IoResult write(const uint8_t* p,size_t n) noexcept override {
        if(n>bytes.size()-used) return {IoStatus::again,0};
        for(size_t i=0;i<n;++i) bytes[used++]=p[i];
        return {IoStatus::progress,n};
    }
};
enum class Transfer { idle, pending, complete, fault };
class ReliablePacket {
    std::array<uint8_t,48> bytes{};
    size_t cursor=48;
    bool failed=false;
public:
    bool load(const Frame& f) noexcept {
        if(failed || cursor!=48) return false;
        for(unsigned j=0;j<outputs;++j) {
            if(f.values[j]>16383) return false;
        }
        for(unsigned j=0;j<outputs;++j) {
            auto p=encode(j,f.values[j]);
            for(unsigned k=0;k<6;++k) bytes[j*6+k]=p[k];
        }
        cursor=0; return true;
    }
    Transfer pump(Backend& b) noexcept {
        if(failed) return Transfer::fault;
        if(cursor==48) return Transfer::idle;
        auto r=b.write(bytes.data()+cursor,48-cursor);
        if(r.status==IoStatus::again || r.status==IoStatus::interrupted) {
            if(r.count) { failed=true; return Transfer::fault; }
            return Transfer::pending;
        }
        if(r.status!=IoStatus::progress || r.count>48-cursor) {
            failed=true; return Transfer::fault;
        }
        cursor+=r.count;
        return cursor==48?Transfer::complete:Transfer::pending;
    }
    bool pending() const noexcept { return cursor!=48; }
    // Only after replacing the connection, while disarmed; never append a broken stream.
    void reset_connection() noexcept { cursor=48; failed=false; }
};
enum class ServiceState { disarmed, active, draining, stopped, fault };
// Offline sender state model. All calls belong to exactly one coordinator thread.
// No thread, descriptor discovery or physical backend is supplied.
class OutputService {
    ReliablePacket packet;
    Frame desired{};
    bool queued=false, safe_loaded=false;
public:
    ServiceState state=ServiceState::disarmed;
    uint32_t delivered=0;
    bool arm() noexcept {
        if(state!=ServiceState::disarmed) return false;
        state=ServiceState::active; return true;
    }
    bool submit(const Frame& f) noexcept {
        if(state!=ServiceState::active || queued || packet.pending()) return false;
        desired=f; queued=true; return true;
    }
    void shutdown() noexcept {
        if(state==ServiceState::fault || state==ServiceState::stopped) return;
        state=ServiceState::draining; queued=false; safe_loaded=false;
    }
    void tick(Backend& b) noexcept {
        if(state==ServiceState::stopped || state==ServiceState::fault || state==ServiceState::disarmed) return;
        if(!packet.pending()) {
            if(state==ServiceState::draining) {
                if(safe_loaded) { state=ServiceState::stopped; return; }
                if(!packet.load(Frame{})) {state=ServiceState::fault; return;}
                safe_loaded=true;
            } else if(queued) {
                if(!packet.load(desired)) {state=ServiceState::fault; return;}
                queued=false;
            } else return;
        }
        auto r=packet.pump(b);
        if(r==Transfer::fault) state=ServiceState::fault;
        if(r==Transfer::complete) ++delivered;
    }
    void timeout() noexcept { state=ServiceState::fault; queued=false; }
    void new_connection() noexcept {
        packet.reset_connection(); desired={}; queued=false; safe_loaded=false;
        state=ServiceState::disarmed;
    }
};
// Lease quarantine prevents reuse until safe-delivery acknowledgement.
class Leases {
    struct Slot { uint32_t owner=0; bool draining=false; };
    std::array<Slot,outputs> slots{};
public:
    bool claim(uint32_t owner,uint8_t mask) noexcept {
        if(!owner || !mask) return false;
        for(unsigned j=0;j<outputs;++j) if(mask&(1u<<j))
            if(slots[j].draining || (slots[j].owner && slots[j].owner!=owner)) return false;
        for(unsigned j=0;j<outputs;++j) if(mask&(1u<<j)) slots[j].owner=owner;
        return true;
    }
    uint8_t drain(uint32_t owner) noexcept {
        uint8_t mask=0;
        for(unsigned j=0;j<outputs;++j) if(owner && slots[j].owner==owner) {
            slots[j].draining=true; mask|=1u<<j;
        }
        return mask;
    }
    bool acknowledge(uint32_t owner,uint8_t mask) noexcept {
        if(!owner || !mask) return false;
        for(unsigned j=0;j<outputs;++j) if(mask&(1u<<j))
            if(slots[j].owner!=owner || !slots[j].draining) return false;
        for(unsigned j=0;j<outputs;++j) if(mask&(1u<<j)) slots[j]={};
        return true;
    }
};
}
