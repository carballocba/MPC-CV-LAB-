# Optional in-memory VST capture

This diagnostic path runs the actual plugin graph through its producer queue and one module-private worker. It never opens MIDI/CV devices, sends host MIDI, accesses the network, or installs/registers a plugin. Default plugin creation starts no capture session and no worker. Capture state is not restored by a preset or scene.

## Control API

Declarations: `include/mpccv/vst/capture_api.hpp`. Resolve the exported functions from the same library that created the live Effect. These calls take control/registry mutexes or create/join a worker, so **never invoke them from processing**.

| Function | Behavior |
|---|---|
| mpc_cv_capture_start(effect, mask) | Reserve nonzero logical-output mask 1–255 and send a priming zero packet to memory before publishing the session to audio. Conflicts are rejected. |
| mpc_cv_capture_status(effect, status) | Return a control snapshot; active/fault status concerns the capture pipeline, separately from graph telemetry. |
| mpc_cv_capture_copy(effect, records, capacity) | Copy up to 512 most recent records, oldest first; non-destructive, not a stream drain. Caller owns the buffer. |
| mpc_cv_capture_stop(effect, status) | Detach from future callbacks. Return Busy if an existing reader still holds the session; retry from control after it ends. Otherwise finish a partial packet, discard unsent work, append reserved-output zeros and detach the worker slot. |

Start/stop results: 1 success, 0 invalid argument/effect, -1 output conflict, -2 busy, -3 unavailable. Stop success is not an electrical acknowledgement: inspect safe_acknowledged and fault. A successful stop retains an immutable last-session history until a subsequent successful start or plugin destruction. A failed conflicting start preserves that archive.

The diagnostic structs are experimental API v2, independent of the unchanged VST ID/52 parameter indices and MCVS v1 scene format. CaptureStatus is 44 bytes and reports version=2. CaptureRecord is 72 bytes. Compile callers with the matching header; these are native ABI records, not a disk/network format. Their size assertions compile on both native and ARM targets.

## Records and counters

Record role 0 is priming zero, 1 a musical event, 2 closing/fault zero. Housekeeping packets use sample=0; musical event timestamps use the plugin's monotonic processed-sample counter, not PPQ. Sequence is per-session and increases per emitted packet. Bytes contain only the selected output CC pairs. Counter/sequence arithmetic is uint32 and eventually wraps; the four-second demo does not cross that limit.

History is a bounded 512-record ring. Overwriting old diagnostic records is counted separately from primary FIFO delivery; it is not silent loss of queued events. Clients needing a complete trace must collect often enough and check sequence continuity. `accepted` counts producer events enqueued, `delivered` includes priming/safe packets as well. While audio publishes concurrently these counters can be sampled at different moments; an exact drain target requires a quiesced producer, a healthy active session and known priming count.

The offline demo uses an accepted-event target after bounded render batches instead of treating a short period of unchanged delivery as an acknowledgement. This prevents scheduler-dependent history loss and ensures the final CSV includes all requested musical packets before stop. Stop deliberately aborts work not yet sent rather than playing stale gates during teardown.

## Real-time handoff

Audio holds a lock-free pointer/read-count guard for one callback. Closing first removes the published pointer; memory stays owned until the reader count reaches zero. The callback never takes a registry mutex, allocates, starts a thread, calls a device, wakes a condition variable, or joins. Concurrent parameter setters remain independent atomic updates.

Direct gate-source routes publish each changed level at its sample timestamp; other routes publish changed levels at block end. Disabled/disarmed routes emit their zero change at block start. Gates routed through arbitrary processors are currently treated as continuous unless the final source itself is a Gate node; explicit destination signal-role metadata is still needed before promising transformed-gate edge fidelity. All publication is bounded and queue overflow faults instead of silently replacing gate edges.

A capture worker drains records as fast as available and preserves logical timestamps. It does **not** pace electrical pulses against wall time. The worker polls with a 1 ms sleep, checks heartbeat/timeouts, and uses a bounded in-memory sink; its speed is not evidence about MPC Private bandwidth or physical jitter.

## Registry and module lifetime

Up to eight sessions use disjoint nonzero masks and unique owner tokens. Reservation spans the whole requested mask even if some graph lanes are disabled. One shared worker is created on first start and joined after last detach. Concurrent control threads serialize lifecycle changes; each Effect still has exactly one audio producer. Native CV tracks, unrelated plugins, other library versions and other processes do not participate in these leases.

POSIX worker ownership is explicit. The registry is heap-owned by the module and deleted by an explicit module finalizer. This avoids registering a function-local static destructor for process exit in a no-SDK Zig shared build that lacks CRT glue to finalize those callbacks at dlclose. An ELF symbol value of zero is relative to load address and is not, by itself, proof of a null runtime DSO handle. ELF inspection now requires `.fini_array` for capture builds. Tests run repeated dlopen/capture/implicit-stop/dlclose **and normal process exit**. No unload test is skipped and the module is not pinned to hide the failure.

The host must quiesce audio and all control calls before effClose/dlclose. The capture stop API may race an audio callback and return Busy; destroying the entire Effect concurrently is not supported by the VST lifetime contract. A join or startup failure is reported; no real-time deadline or recovery from a broken host lifecycle is promised.

## Reproduce without installation

```sh
make check
make vst-demo
# output: build/vst-capture.csv
python3 tools/validate_vst_capture.py build/vst-capture.csv
tools/run_tsan.sh
```

The demo loads the built plugin into a fake host, renders four logical seconds over eight outputs, collects packets outside render, sends transport stop and closes capture. Validation checks sequence continuity, codes/ranges, all eight channels, 64 gate transitions at their expected logical samples and final zeros. Neither the demo nor the capture API invokes the Community Installer.
