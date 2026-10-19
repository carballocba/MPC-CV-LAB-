# Real-time audit — 19 October 2026

Scope: `src/vst/plugin.cpp::render`, `Graph::sample/configure`, `Queue::pop` and typed atomic parameter handoff. This is an offline source audit and allocation test, not a deadline guarantee on MPC.

| Property | Implementation/evidence | Remaining limit |
|---|---|---|
| Audio memory | Fixed arrays, 16 nodes, eight routes, one pending scene per block, no allocation in render | `tests/rt_test.cpp` overrides C++ new/delete and drives 1/128/2048-frame blocks with automation. It does not intercept every libc allocation or prove libm timing. |
| Locks | No mutex in render or setParameter; uint32 atomics have compile-time lock-free requirement, including successful ARM build | Host time callback and libm implementations remain outside this proof. |
| Control handoff | Arbitrarily many parameter callers use independent atomic slots. Chunk producers serialize using a control-only mutex before one SPSC producer pushes; audio is sole consumer | Parameter groups do not form a transaction; latest individual automation wins. Host must not process/close the same instance concurrently. |
| Scene load | Bounded decode/checksum/validation outside render; fixed-size copy and graph validation at block boundary | Graph reconfiguration currently resets slew memory. No seamless transition or morphing is claimed. |
| Work bounds | At most 8192 samples, 16 nodes and eight routes; topology validation bounded at 16² | Worst-case wall time has not been measured on ARM hardware. Unsupported block sizes latch a fault. |
| I/O | No filesystem, logging, rawmidi or socket operation in render. Plugin contains no physical backend | Native skin polling/chunk operations are host/control context; chunk getters use mutex and must not be called by a host from its time-critical processing loop. |
| Telemetry | Eight atomic code snapshots, gate counters, transport/arm/fault | Values can span two blocks when read concurrently; they are diagnostic, not a coherent timestamped event stream. |
| Gate timing | Graph evaluates levels each sample; host harness tests loop straddling; unit test counts 128 narrow gate rises/falls over a second | Plugin exposes final levels/counters only. A physical timestamped edge queue is not implemented. |
| Suspension/failure | Missing time, invalid time, stop or disarm returns zero simulated levels. Suspend clears arm/meter state | No sender watchdog or DAC acknowledgement exists in VST because hardware is absent. |

The legacy Phase 0 `Broker` and `Packet` are retained for source/API history. New work uses `Leases` and `ReliablePacket`. These are single-coordinator models. `OutputService` has bounded single-step pumping and explicit fault/new-connection states, not a production worker thread. Its caller owns deadlines and quarantine. A reconnect explicitly disarms. Safe delivery means captured/kernel-accepted bytes, never measured zero volts.

TSan checks the SPSC thread stress and host automation concurrent with processing. It cannot audit native host writers, native CV tracks, other plugin libraries or controller firmware. No architecture can guarantee DAC reset after process death without controller support or external safety hardware.
