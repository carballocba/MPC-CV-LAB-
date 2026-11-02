#pragma once
#include "event_pipeline.hpp"
#include "capture_types.hpp"
#include <memory>
#include <mutex>
#include <thread>

namespace mpccv {
class CaptureSession {
    class Sink final: public Backend {
        CaptureSession& session;
    public:
        explicit Sink(CaptureSession& s):session(s) {}
        IoResult write(const uint8_t*,size_t) noexcept override;
    } sink{*this};
    std::array<CaptureRecord,capture_history_size> history{};
    size_t next_record=0, record_count=0;
    uint32_t overwritten=0, sequence=0;
    std::array<uint16_t,outputs> last_sent{}; // sole audio producer
    bool producer_ok=true;
    friend class CaptureCoordinator;
public:
    EventPipeline<4096> pipeline;
    const uint8_t reserved;
    uint32_t owner=0; // coordinator only
    explicit CaptureSession(uint8_t mask):pipeline(mask),reserved(mask) {}
    // Called only by the host's serialized audio processing callback.
    void begin_block(uint64_t sample,bool armed,uint8_t enabled) noexcept;
    void gates(const Frame&,uint8_t gate_mask) noexcept;
    void levels(const Frame&,uint8_t gate_mask) noexcept;
private:
    void publish(const Frame&,uint8_t mask) noexcept;
};
class CaptureCoordinator {
    std::mutex lifecycle,registry;
    std::array<CaptureSession*,outputs> slots{};
    Leases leases;
    std::thread worker;
    std::atomic<uint32_t> quit{0};
    uint32_t next_owner=1;
    size_t count=0;
    void loop() noexcept;
    CaptureStatus status_locked(const CaptureSession&) const noexcept;
public:
    ~CaptureCoordinator();
    CaptureResult attach(CaptureSession&);
    CaptureResult detach(CaptureSession&);
    CaptureStatus status(const CaptureSession&);
    size_t copy(const CaptureSession&,CaptureRecord*,size_t);
};
// One registry per linked plugin image, not a promise of ownership across other libraries.
__attribute__((visibility("hidden"))) CaptureCoordinator& capture_coordinator();
class CapturePort {
    std::atomic<CaptureSession*> current{nullptr};
    std::atomic<uint32_t> readers{0};
    std::unique_ptr<CaptureSession> owned; // active, retiring or immutable archive
    bool registered=false;
    std::mutex control; // never taken by Reader/audio
public:
    static_assert(std::atomic<CaptureSession*>::is_always_lock_free,"lock-free capture pointer required");
    class Reader {
        CapturePort& port;
        bool counted=false;
        CaptureSession* session=nullptr;
    public:
        explicit Reader(CapturePort& p) noexcept:port(p) {
            if(!port.current.load())return;
            port.readers.fetch_add(1);counted=true;
            session=port.current.load();
        }
        ~Reader() {if(counted)port.readers.fetch_sub(1);}
        CaptureSession* get() const noexcept {return session;}
        Reader(const Reader&)=delete;
        Reader& operator=(const Reader&)=delete;
    };
    ~CapturePort(); // host must quiesce callbacks before destroying plugin
    CaptureResult start(uint32_t mask);
    CaptureResult stop(CaptureStatus* last=nullptr);
    CaptureStatus status();
    size_t copy(CaptureRecord*,size_t);
};
}
