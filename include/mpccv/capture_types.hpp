#pragma once
#include <cstddef>
#include <cstdint>
namespace mpccv {
constexpr size_t capture_history_size=512;
enum class CaptureResult:int32_t { ok=1, invalid=0, conflict=-1, busy=-2, unavailable=-3 };
// Native diagnostic ABI only, never a hardware acknowledgement.
struct CaptureStatus {
    uint32_t version=2, state=0, fault=0, mask=0, owner=0, delivered=0;
    uint32_t safe_acknowledged=0, history_count=0, history_overwritten=0, active_sessions=0, accepted=0;
};
static_assert(sizeof(CaptureStatus)==44,"capture status v2 diagnostic ABI");
struct CaptureRecord {
    uint64_t sample=0;
    uint32_t role=0, length=0, sequence=0, reserved=0; // role: 0 priming zero, 1 musical event, 2 closing zero
    uint8_t bytes[48]{};
};
static_assert(sizeof(CaptureRecord)==72,"capture diagnostic ABI");
}
