#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace mpccv {
constexpr unsigned outputs = 8;
// Candidate MPC X transfer function; NOT a calibrated MPC One profile.
inline uint16_t code(double volts) noexcept {
    if (!std::isfinite(volts) || volts <= 0) return 0;
    if (volts >= 5) return 16383;
    return static_cast<uint16_t>(std::floor(volts * 3276.8 + 0.5));
}
inline std::array<uint8_t, 6> encode(unsigned output, uint16_t value) noexcept {
    if (output >= outputs || value > 16383) return {};
    return {{0xbf, static_cast<uint8_t>(output * 2), static_cast<uint8_t>(value >> 7),
             0xbf, static_cast<uint8_t>(output * 2 + 1), static_cast<uint8_t>(value & 127)}};
}
struct Frame { uint64_t sample = 0; std::array<uint16_t, outputs> values{}; };
// One producer, one consumer only. Capacity is N-1. No OS calls in either operation.
template<size_t N, class T=Frame> class Queue {
    static_assert(N > 1, "queue too small");
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "requires lock-free 32-bit atomics");
    std::array<T, N> data{};
    alignas(64) std::atomic<uint32_t> head{0};
    alignas(64) std::atomic<uint32_t> tail{0};
public:
    bool push(const T& f) noexcept {
        auto h = head.load(std::memory_order_relaxed);
        auto next = (h + 1) % N;
        if (next == tail.load(std::memory_order_acquire)) return false;
        data[h] = f; head.store(next, std::memory_order_release); return true;
    }
    bool pop(T& f) noexcept {
        auto t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false;
        f = data[t]; tail.store((t + 1) % N, std::memory_order_release); return true;
    }
};
// Control-plane broker model. Serialize all access on one coordinator thread.
// Reject conflicts instead of stealing outputs. Tokens must be unique and nonzero.
class Broker {
    std::array<uint32_t, outputs> owners{};
public:
    bool claim(uint32_t token, uint8_t mask) noexcept {
        if (!token || !mask) return false;
        for (unsigned j=0;j<outputs;++j)
            if ((mask & (1u<<j)) && owners[j] && owners[j]!=token) return false;
        for (unsigned j=0;j<outputs;++j) if(mask & (1u<<j)) owners[j]=token;
        return true;
    }
    uint8_t release(uint32_t token) noexcept {
        uint8_t mask=0;
        for(unsigned j=0;j<outputs;++j) if(token && owners[j]==token) {
            owners[j]=0; mask |= 1u<<j;
        }
        return mask; // coordinator must send and acknowledge safe values before reuse in production
    }
};
enum class Source { off, sine, triangle, gate, sequence, random_hold };
struct Lane {
    Source source=Source::off;
    double cycles_per_beat=1, offset=0, depth=5;
    std::array<double, 8> steps{{0,1,2,3,4,3,2,1}};
    uint32_t seed=1;
};
struct Transport { bool valid=false, playing=false; double ppq=0, bpm=120, sample_rate=44100; };
inline bool valid(const Transport& t) noexcept {
    return t.valid && std::isfinite(t.ppq) && t.ppq>=0 && t.ppq<1e9 &&
        std::isfinite(t.bpm) && t.bpm>0 && t.bpm<=1000 &&
        std::isfinite(t.sample_rate) && t.sample_rate>=8000 && t.sample_rate<=384000;
}
class Engine {
public:
    std::array<Lane, outputs> lanes{};
    uint8_t enabled=0x0f; // logical outputs 1..4, no assumptions about cable wiring
    bool armed=false;
    Frame render(const Transport& t, uint64_t sample) const noexcept {
        Frame f; f.sample=sample;
        if (!armed || !t.playing || !valid(t)) return f;
        for(unsigned j=0;j<outputs;++j) {
            const auto& l=lanes[j];
            if (!(enabled & (1u<<j)) || !std::isfinite(l.cycles_per_beat) ||
                l.cycles_per_beat<=0 || l.cycles_per_beat>64) continue;
            double pos=t.ppq*l.cycles_per_beat, phase=pos-std::floor(pos), v=0;
            switch(l.source) {
            case Source::off: continue;
            case Source::sine: v=0.5-0.5*std::cos(6.283185307179586*phase); break;
            case Source::triangle: v=1-std::abs(2*phase-1); break;
            case Source::gate: v=phase<0.5?1:0; break;
            case Source::sequence: v=l.steps[static_cast<uint64_t>(pos)%8]/5; break;
            case Source::random_hold: {
                uint32_t x=static_cast<uint32_t>(static_cast<uint64_t>(pos)) ^ l.seed;
                x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16;
                v=double(x)/4294967295.0; break;
            }}
            f.values[j]=code(l.offset+l.depth*v);
        }
        return f;
    }
};
// Offline writer contract: short writes retain the exact remaining byte stream.
// Return 0 for temporary backpressure, -1 for a fatal transport error.
// Legacy Phase 0 helper, retained for API history; use ReliablePacket for new code.
class Packet {
    std::array<uint8_t, 48> bytes{};
    size_t cursor=48;
public:
    void load(const Frame& f) noexcept {
        cursor=0;
        for(unsigned j=0;j<outputs;++j) {
            auto p=encode(j,f.values[j]);
            for(unsigned k=0;k<6;++k) bytes[j*6+k]=p[k];
        }
    }
    template<class Writer> bool pump(Writer& w) {
        if(cursor==bytes.size()) return true;
        auto n=w.write(bytes.data()+cursor,bytes.size()-cursor);
        if(n<0) return false;
        if(static_cast<size_t>(n)>bytes.size()-cursor) return false;
        cursor+=static_cast<size_t>(n); return cursor==bytes.size();
    }
};
}
