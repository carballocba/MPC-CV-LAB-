# Source audit — 18 October 2026

## Provenance and scope

Primary: https://github.com/padbangers/glass-spheres at `1fe3b0a17b14dc07d327e5408d7aaf438715ce44`.
Framework: https://github.com/sd88me/mpc-vst-plugins at `539c1f28be92a60e33514788cc63105640791693`.
Glass Spheres pins framework `818f574720e45c2ce125bf1629d8c8cdc45bbcba`, not the current framework HEAD. Its patches must not be assumed applicable to HEAD.

Both repositories were downloaded read-only. Tracked-file inventories and SHA-256 hashes are in `research/`. The complete primary working-tree snapshot is retained there, including documentation, DSP, tests, patches and tools; the uninitialized framework submodule is recorded by commit above. No reference build/deploy/injection scripts were executed. Detailed review concentrated on every requested file, transport patches, build path, test path, framework wrapper/interface and timing documentation. Inventory coverage does not mean exhaustive semantic review of every vendored DSP or framework tooling file.

## Evidence matrix

References below are relative to the pinned primary snapshot unless explicitly called framework. Repository documentation is author-reported hardware evidence, not an independent measurement by this project.

| Topic | Source observation | Consequence for MPC-CV |
|---|---|---|
| Discovery | `src/cv_out.cc::FindPrivatePort` scans `/proc/self/fd`, matches `/dev/snd/midiC`, calls rawmidi INFO ioctl and accepts output streams with `Private` substring | Do not hardcode card 2/subdevice 1. Substring is too permissive for production; ambiguous candidates must fail closed. Borrowed descriptors can close or be reused. |
| Encoding | `SenderLoop` constructs `bf,2*j,v>>7,bf,2*j+1,v&127` for eight zero-based outputs | Adjacent proprietary CC pairs, not conventional CC n/n+32. Full status on each CC. Offline encoder covers entire 14-bit range. |
| Voltage | `cv_out.h::VoltsToCode` rounds volts times 3276.8 and clamps at 16383 | Candidate unipolar transfer. Maximum nominal voltage is 4.9996948 V. Invalid floating point needs explicit handling. Not a measured accuracy or resolution guarantee. |
| Ownership | `Claim` overwrites owner, `Owns` compares pointer; `engine.cc::AssignJack` can reclaim | Latest claim wins within this shared library. No coordination with native CV tracks, other plugins/libraries/processes. Use explicit conflict rejection. |
| Instances | `AddInstance` starts one thread; last `RemoveInstance` joins | Shared globals are scoped to the loaded library image. Startup failure is not surfaced; count/lifetime assumptions need validation. |
| Sender | Dirty-bit exchange, latest wanted values, one packed write then 2 ms sleep | Intermediate states including complete short gates can disappear. Coalescing is appropriate only for continuous levels. |
| RT safety | `Set` avoids I/O and explicit mutex; `Flush` signals pthread condition | No proof of bounded RT signaling. `owner`, `wanted`, running/quit flags have concurrent non-atomic accesses; volatile is not synchronization. Mutex on writer does not synchronize unlocked reader. |
| Transport | Patch `0001`, `SetParam(host_transport)`, `MakeTransportClock`, `Render` | PPQ and tempo arrive via formatted strings each block. Clock uses PPQ plus sample offset. Reset on play rising edge; loop/locate handling is limited. Missing host time can leave stale state. |
| Rate | Sender imposes >=2 ms after each write; source README reports 108 updates/s accepted and ceiling unknown | 500 batches/s is an approximate software upper bound, not demonstrated hardware throughput. 128/44100 is 2.9025 ms, about 344.5 callbacks/s. Latency includes block, wake, write and controller delays. |
| Failure | Only negative write with EBADF invalidates fd; dirty bits already consumed | EAGAIN, EINTR, EIO, short and zero writes lose unsent bytes/updates. EBADF does not requeue. Reused valid fd can escape detection. Missing fd with dirty work can spin until retry time. |
| Shutdown | Release queues zero; final removal sleeps 6 ms then requests exit and joins | No drain acknowledgement. Zero may never arrive; blocking write can prevent join. Process death cannot guarantee reset of latched DAC. |
| Tools | `cv/cvsniff.c` intercepts writes/ioctls/ALSA, logs under mutex and detached FIFO injector | Intrusive investigation tool; not suitable for audio path or shipping backend. Captures show traffic, not calibrated voltages. No use on device in Phase 0. |
| Testing | `test/sim_test.cc` replaces cv_out; CLAUDE says stale relative to MIDI/modulation | It does not exercise real sender concurrency, fd handling, physical latency or failure recovery. |
| Framework | HEAD `wrapper/engine.h`, `vst2_wrap.c` support song position callback and serialize engine calls with mutex | Valuable ABI reference, not automatically RT-safe. Build an independent typed host adapter; avoid string transport and locks in processing. |

## MPC One versus MPC X

Akai explicitly specifies four TRS CV/Gate connectors and eight total outputs with Y cables:
https://support.akaipro.com/en/support/solutions/articles/69000816149-akai-pro-mpc-one-frequently-asked-questions

The official MPC OS guide describes TRS-to-dual-TS breakout operation:
https://cdn.inmusicbrands.com/Software/36/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.6.pdf

Glass Spheres README's assertion that only MPC X has CV jacks is contradicted by Akai. Its `tested.json` records MPC X / MPC OS 3.9.1, not MPC One and not a named Hakai build. Identical counts do not establish controller routing, ioctl ABI, connector mapping, calibration, permissible traffic or firmware behavior. Do not infer tip/ring mapping from adjacent CC numbers; verify mapping separately. Current use of four signals does not alter the eight-output model.

Framework `docs/MIDI_TIMING.md` reports Force / MPC OS 3.9.1 timing measurements: PPQ jitter, backwards tempo excursions and loop boundary straddling. These are useful hypotheses for tests, not MPC One guarantees. No source reviewed establishes full Hakai 3.9 versus 3.9.1 compatibility.

## Licensing and independence

New implementation in `include/`, `src/`, `tests/` is independently written here; it has no dependency on either repository, Marbles, stmlib, ALSA or a VST SDK. Reference source is isolated under `research/glass-spheres-source`, is never built, and retains upstream notices. Both top-level MIT texts are preserved as `research/*-LICENSE.txt`. If substantial code is reused later, carry its copyright and full permission notice with it; vendored code has separate notices. Glass Spheres top-level LICENSE has a stale Clouds/Parasites footer; its actual Marbles/stmlib notices must also be retained. VST2 SDK/header redistribution rights require separate review before packaging; an upstream framework MIT license does not settle third-party SDK rights. No license is selected here for the user's newly authored project code.
