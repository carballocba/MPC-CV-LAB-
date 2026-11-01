# Architecture — consolidated laboratory

## Implemented boundaries

The independent C++17 core has no physical-device dependency. `core.hpp` retains the original eight-output generator API, candidate protocol codec and bounded SPSC queue. `graph.hpp` adds a compiled DAG of at most 16 virtual nodes, separately routed to eight logical outputs. `backend.hpp` supplies the abstract byte transport, bounded capture backend, partial-write state machine, offline sender lifecycle and quarantined ownership model. Nothing depends on a named Eurorack module.

The VST path is `host time/atomic parameters -> Graph -> simulated levels/atomic telemetry`. The offline lab path adds `Frame -> OutputService -> ReliablePacket -> CaptureBackend -> file written by offline main`. The VST does not instantiate a device sender or export MIDI events to MPC. A captured CC packet is not physical output.

## Engine contract and modulation matrix

A virtual `Node` declares kind, two dependency IDs, rate, scale, offset, modulation amount, slew rate, quantization interval, eight steps and random seed. The fixed evaluator supports off, sine, triangle, gate, sequence, random hold, mix, slew and quantizer. To add a family, extend the typed kind/validation/evaluator/serialization migration; public VST parameter IDs must remain stable. Multi-signal engines can use multiple node IDs. There is no heap-polymorphic engine allocation on the audio thread.

`compile` validates all values, resolves a deterministic topological order and rejects cycles, unknown kinds and references. `modulator` can alter source rate; mix combines an input with a scaled second dependency. Dedicated slew/quantizer nodes process virtual signals. Routes select a source with scale/offset and a 0–5 V interval; negative scale implements inversion/attenuation. One source can feed multiple outputs. Feedback cycles currently fail rather than inserting implicit delay. An explicit delay node and safe feedback policy remain future work.

Each sample derives source phase from PPQ. Stateful slew advances by sample period; stopped/invalid/disarmed processing clears its memory. Internal signals are bounded at ±100, physical candidate conversion is clamped at 16383 after rounding. The near-5 V overflow discovered by end-to-end simulation has a regression test. Graph evaluation preserves narrow logical gate transitions, and the separately tested EventPipeline retains timestamped edges without coalescing. Wiring that queue into the VST/physical coordinator is still pending.

## Host, control and UI

`src/vst/plugin.cpp` owns all mutable graph state on its processing thread. Parameter setters publish independent lock-free 32-bit atomic slots. They support concurrent callers, not transactional multi-parameter edits. Host chunk operations serialize producers under a control-only mutex; a single SPSC consumer applies at most one validated scene at block start. A pending-scene barrier prevents arming while older queued scenes remain; only the processing thread publishes the complete scene parameter set. A rejected full-queue load does not replace the desired scene. The host must serialize processing callbacks for an instance and stop them before close/unload.

Configuration validation/copy is bounded at 16 nodes and currently runs at block start when parameters change; it does not allocate, but still needs ARM timing measurement. Parameter changes reset slew state. Chunk parsing/checksum happens outside processing. Sample rate is validated; absent/invalid host time, stop, suspend and fault produce zero simulated levels. The adapter handles PPQ loop straddling per sample when valid cycle positions are available. It is not an MPC timing calibration or a cure for unknown host jitter.

Native MPC skin files bind 52 parameter indices. Indices 44–51 append eight independent output enables; the original indices remain unchanged. A single atomic integer mask backs both legacy mask automation and individual switches, avoiding lost updates between callers. Read-only voltages/transport/fault use atomic telemetry; diagnostic snapshots can contain values from adjacent blocks. Q-Links are native skin assignments, not ALSA MIDI. There is no desktop editor window. Device/scene browsing remains a foundation, with actual scene persistence through host chunks. See `UI.md`.

## Scene and preset model

`Scene` v1 stores active node definitions, eight routes, enabled mask, host-sync flag and four future control assignment IDs. `scene.hpp` encodes a canonical little-endian IEEE64 format with magic, explicit payload size and corruption checksum. It rejects truncation, oversized/unknown-version data, invalid parameters and cycles before publishing. It never stores physical arming. Host-sync=false is rejected until an independent clock is designed. Empty and smaller scenes are supported; route references beyond loaded node count resolve to Off through the parameter adapter.

`EnginePreset` applies one node transactionally to a chosen graph slot and validates the resulting DAG. The offline scene_tool.py now loads JSON presets, validates the resulting graph and compiles/decodes canonical MCVS scenes; Python/C++ roundtrip is tested natively and under ARM/QEMU. No JSON parsing or filesystem operation was added to audio or the VST UI. A Device Profile is a separate versioned JSON contract describing external input ranges/polarity/types/threshold/scaling and suggested assignments. Only the generic template exists and is validated by tooling; VST JSON profile loading and specific module profiles remain pending.

Scenes load disarmed, with one final graph change at the next processing boundary. This is recovery, not seamless switching: slew state resets. Morphing, tempo-independent clocks, dynamic scene Q-Link remapping and a touch browser are not promised.

## Output service and hardware boundary

`ReliablePacket` retains cursor after short/zero writes, distinguishes retryable interruption/backpressure from fatal/disconnect, validates byte counts, refuses packet replacement in flight and latches fatal errors. Explicit new-connection reset must start disarmed. `OutputService` finishes an in-flight packet before sending safe zeros on shutdown, requires a subsequent completion transition and permits an external timeout to fault. Each tick performs one bounded backend call. These are worker/coordinator models, not a thread implementation or an endpoint discovery strategy.

`Leases` rejects conflicts and keeps draining outputs unavailable until matching-owner acknowledgement. All calls require one coordinator thread and unique nonzero owner tokens. It cannot arbitrate native CV tracks, unrelated plugin libraries or host traffic. The legacy Broker/Packet remain for Phase 0 API history and are not production-safe alternatives.

A future physical backend needs exact endpoint identity, ambiguous discovery rejection, descriptor close/reuse handling, host stream serialization, nonblocking/bounded writes without changing borrowed fd flags, ordered gate edges, overflow panic, watchdog and shutdown deadlines. A kernel write acknowledgement does not prove voltage. No reset after process death is guaranteed. Hardware readiness ends at the separately documented test/rollback plan.

## ARM and licensing

Zig 0.15.2 cross-builds a Linux ARMv7 EABI5 hard-float ELF with exported `VSTPluginMain` and a dependency report. Static ABI sizes and lock-free uint32 atomics compile on that target. Seven ARM tests including a dynamic VST host now execute under QEMU for GNU and Zig toolchains. Hakai loader/skin compatibility and physical timing remain unproven. Native `.so` output on Mac is Mach-O, distinct from the ARM ELF.

The minimal ABI declarations carry upstream MIT attribution. No SDK, upstream DSP, rawmidi code or UI artwork is linked into the plugin. Binary runtime redistribution notices remain a release-packaging gate; see `LICENSE_AUDIT.md`.

## Timestamped event pipeline — November update

`EventPipeline<N>` is a separate fixed-capacity SPSC handoff: one producer, one worker consumer. Start first sends zeros to an explicitly reserved output mask. Only after the byte packet completes can the producer publish. Events retain monotonic sample timestamps and selected-channel values; future edges are held until due. Continuous levels are not silently coalesced either, so callers must budget event rate. Overflow latches fault instead of dropping a gate edge unnoticed.

Stop/fault discards events not yet sent, completes an in-flight CC packet to preserve byte framing, then attempts safe zeros. One bounded backend call occurs per worker tick. EAGAIN/zero writes preserve cursor; fatal/disconnect, backward clock and deadline failure quarantine the single-use session. A heartbeat deadline detects a stalled producer. Safe acknowledgement means backend byte acceptance only; failure does not falsely acknowledge zero. Caller must keep ticking and provide a nonblocking backend; this is not a production thread or a guarantee after process death. Reconnect needs a new quiesced session and renewed reservations.

The pipeline, ownership model and VST remain separate at this checkpoint. No discovery/syscall backend or shared coordinator is hidden in the queue. See the November report and RT audit for tested limits.
