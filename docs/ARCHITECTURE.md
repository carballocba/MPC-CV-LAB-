# MPC-CV architecture

## Boundaries

Target: an ARMv7 Linux VST2 instrument rendering silence inside MPC standalone. Phase 0 delivers a native offline core, not an installable plugin. Modules are generic signal sources, not named Eurorack destinations. Four logical lanes are enabled initially, eight exist without an ABI change. A device profile, connector map and optional module presets are data, never generator dependencies.

Production flow:

`MPC host -> typed transport/MIDI adapter -> signal graph -> calibrated routes -> per-instance SPSC queues -> shared coordinator -> physical backend`

UI/control updates travel through bounded command queues to the engine. Engine publishes telemetry snapshots to UI. Host processing alone owns engine state; control code must never mutate `lanes` concurrently. One producer and one consumer per queue. No allocations, syscalls, logging, locks, waits, formatted strings or thread creation on the render path. Verify lock-free atomics on the actual ARM toolchain; host results are insufficient.

## Signal and performance model

Sources: LFO sine/triangle, clock/gate, step sequence and seeded random hold are implemented. Envelopes, slew, quantizer, Euclidean gates, probability, ratchets, sample/hold, logic and macros are extension points. Use explicit volts for physical routes and normalized values inside modulation graph; preserve 1 V/oct only in pitch routes. Route stages: scale, offset, limits, slew, device calibration, encoding. Bipolar internal modulation requires a deliberate unipolar transformation or external hardware; software cannot create negative voltage through this candidate protocol.

Future graph compilation occurs off audio thread, with fixed memory and bounded node count. Reject algebraic cycles or require an explicit one-tick delay. Presets store schema version, stable parameter IDs, seed, graph, routes, limits and calibration reference. Never restore an armed physical-output state from a preset. Parameter indices are append-only once released. None of graph editing, persistence, MIDI input, envelopes or macros is implemented yet.

Performance controls: arm, mute, panic, per-lane depth/offset/rate, scene recall and bounded morph. Muting should keep musical phase while driving configured safe values. Panic overrides all generators. Initial safe value is zero, including pitch; configurable holds require explicit future design. Current simulator only implements arm and stop-to-zero.

## Output coordination and safety

Production coordinator must be unique across all MPC-CV instances and versions, with verified lifetime/unload behavior. Other plugins and native CV tracks cannot be arbitrated by this broker. Their outputs must be reserved externally. Never steal a route automatically. Claim transitions: free -> reserved -> safe-write acknowledged -> active -> draining -> free; quarantine on failure. The prototype Broker tests conflict rejection only and releases immediately; it is not this production state machine.

Continuous CV: bounded latest-value coalescing. Gates/triggers: separate ordered timestamped edge queue, reserve capacity for falling edges. Queue overflow must latch fault, disarm and request safe values out of band; do not silently overwrite an edge. An independent coordinator timeout handles suspended audio callbacks; it cannot make a latched DAC safe after process death. The implemented Queue reports full; caller policy is not wired into a worker yet.

Physical backend remains absent. A future backend needs an allowlisted firmware/device profile, exact identity validation, ambiguous-port rejection, a strategy for host-shared stream serialization, monotonic scheduling, bounded I/O and explicit backpressure. Never set O_NONBLOCK on a borrowed fd: file-status flags may be shared with the host. Duplicating an fd does not create an independent stream or independent flags. Whole-message writes alone do not prove noninterleaving with host SysEx or other writers.

Maintain packet byte offset after short writes; EINTR/EAGAIN retry only under bounded worker scheduling. Fatal failures invalidate the endpoint and disarm; reconnect starts with safe values and requires re-arm. `Packet` models cursor preservation but exposes only a bool completion; a real transport must distinguish retryable versus fatal outcomes and never replay partial packets blindly after reconnect. A successful write acknowledges kernel acceptance, not measured voltage. Shutdown requests safe values, waits for bounded acknowledgement, records failure, stops worker and joins off audio thread. Never promise zero after crash, disconnect or controller failure.

## Timing

Use validated host PPQ at block start, tempo, sample rate, frame count and loop boundaries. Missing/nonfinite time means safe output. Keep transport phase independent of sender wall time. A production event scheduler splits blocks at loop boundaries, calculates edge sample offsets and detects locate/tempo discontinuities without replay bursts. Firmware timing jitter needs measured tolerances, not copied constants. Worker deadlines use monotonic time mapped from host audio timeline; they improve timing but cannot guarantee sample accuracy at the physical pins.

Prototype renders one level snapshot per caller-supplied PPQ, stateless and deterministic. Loops/locates therefore immediately change phase, but pulses shorter than the callback interval can be missed. No edge scheduler or pulse-width guarantee is claimed. At 128 frames/44.1 kHz, block resolution is 2.9025 ms before sender/controller latency. Begin hardware experiments conservatively only in a later authorized phase; find rate ceiling with measurements, not a 500 Hz assumption.

## VST2 and ARM integration contract

Implement a separate VSTPluginMain/AEffect adapter once the ABI and rights are settled. It must support lifecycle, sample rate/block size, processReplacing and legacy processing semantics, typed time acquisition with validity flags, MIDI deltaFrames, normalized stable parameters and bounded versioned state loading. Clear audio buffers. effMainsChanged/suspend should disarm through coordinator. Host callback cessation needs watchdog treatment. Invalid state must not emit output. Test multi-instance unload and host callback concurrency.

Cross-build requires pinned ARMv7 hard-float toolchain, target sysroot/glibc baseline and verified ELF dependencies. Do not assume framework Docker images match Hakai. Inspect target ABI later without changing device. Native macOS build proves portable logic only; no ARM binary, VST2 wrapper, screen skin or host ABI validation exists in Phase 0.
