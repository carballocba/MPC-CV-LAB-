# Validation — 19 October 2026

## Results

| Category | Result | Evidence/limits |
|---|---|---|
| Native | **PASS** `make -B check` | Apple clang 16, macOS arm64. Builds core, backend, graph, dynamic VST host, RT guard and lab simulator. |
| ASan / UBSan | **PASS** entire `make -B check` with both sanitizers | Includes plugin and dynamic host instrumented together; see `evidence/asan-ubsan.log`. |
| TSan | **PASS** queue stress and dynamic VST host with concurrent automation | Both plugin/host instrumented; native only. |
| Eight-output capture | **PASS / SIMULATED** | 1379 snapshots over exactly 176400 sample evaluations; eight codes match every CC byte; final all-zero packet. No device output. |
| RT allocation guard | **PASS** C++ new/delete tripwire | 1000 callbacks at 1/128/2048 frames with parameter changes. Not a proof of all libc behavior or ARM deadlines. |
| ARMv7 cross-build | **PASS build only** | Zig 0.15.2, Linux ARM EABI5 hard-float; plugin plus core/backend/graph test executables. ABI size and atomic lock-free assertions compiled. |
| ARM/QEMU execution | **BLOCKED / NOT RUN** | qemu-arm and ARM runtime sysroot are not provisioned. No claim about tests running on ARM. |
| Native skin | **PASS generation / SIMULATED preview** | Six tabs, seven Q-Link views, 44 matching parameters, no geometry warnings. |
| MPC GUI/Q-Links | **PENDING hardware** | Native files generated; no touch/knob/meter behavior verified on MPC. |
| Physical CV | **NOT TESTED** | Physical backend absent; no device opened, plugin installed, service restarted or settings changed. |

Exact compiler, source hashes and summary: `evidence/results.json`. ELF SHA-256/dependencies/symbols: `evidence/armv7-elf.json`. Local generated artifacts remain under ignored `build/` and `ui/native/build/`; binaries are not committed or released. GitHub CI is configured separately; local PASS does not claim a successful remote run.

## Coverage

Core: all 8 × 16384 MIDI code combinations; 0/2/5 V conversion, nonfinite and negative input; near-5 V rounding regression plus 100001 voltage samples; default disarm, start/stop, invalid transport and four/eight enable masks.

Concurrency: SPSC full/empty/wrap and 100000 ordered frames across two threads; no assumption of multiple producers. Host automation uses independent atomic parameters and is exercised concurrently with processing. Chunk producers are serialized outside audio.

Backend model: short/zero writes, EINTR-equivalent, EAGAIN-equivalent, disconnect, fatal latch, reconnect disarm, in-flight replacement refusal, shutdown drain then zero packet, timeout fault, ownership conflicts and quarantined release acknowledgement. No rawmidi/device worker is exercised.

Graph/state: limits, invalid references, cycle rejection, deterministic sources, sample-evaluated fast gates (128 rises and falls/second), fanout, per-route limits, transactional preset application, canonical scene round trip, every truncated prefix and checksum corruption, scene application disarms.

VST: actual `dlopen`/entry/export checks, separate instances, lifecycle/unload, clear replacing audio, additive legacy silence, parameters and display strings, read-only meters, host time missing/stop, in-block loop wrap, virtual route reassignment, corrupt state rejection, state restore and concurrent automation. The harness uses the same documented ABI declarations; it is not an independent MPC loader.

## Reproduce

```sh
make -B check
make -B check CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -fsanitize=address,undefined'
```

Assertions are mandatory in all C++ tests; they reject NDEBUG. Zig release optimization defines NDEBUG by default, so the ARM build script explicitly uses `-UNDEBUG` for tests.

For TSan, compile `tests/core_test.cpp` with `-fsanitize=thread`, then build `src/vst/plugin.cpp` as a dynamic library and `tests/vst_host_test.cpp` with the same sanitizer; invoke the host with that library. TSan availability varies by platform. Native build output is Mach-O on Mac and must never be mistaken for ARM Linux.

For ARM:

```sh
ZIG=/absolute/path/to/zig-0.15.2/zig tools/build_armv7.sh
```

The official aarch64 macOS archive was obtained over validated TLS from https://ziglang.org/download/0.15.2/zig-aarch64-macos-0.15.2.tar.xz and verified against SHA-256 `3cc2bab367e185cdfb27501c4b30b1b0653c28d9f73df8dc91488e66ece5fa6b` from https://ziglang.org/download/index.json. It was unpacked temporarily, not installed globally. The script requires an explicitly supplied reviewed executable and never downloads or deploys automatically.

The ELF reports dependencies on libm.so.6, libpthread.so.0, libc.so.6, libdl.so.2 and ld-linux-armhf.so.3, with referenced GLIBC_2.4 and GLIBC_2.16 symbols. Availability on the actual Hakai image and runtime C++ behavior remain unverified; a `2.31` build target is not firmware compatibility certification.

## Checkpoints

- A: consolidated on official-main history; historical research file unchanged; original Phase 0 archived; original tests reproduced.
- B: hardened offline models pass; real coordinator/thread/discovery/edge transport remains pending.
- C: bounded virtual graph, backend interface, generic profile contract, typed preset application and basic scene serialization implemented. JSON device/preset loading remains pending.
- D: independent VST2 skeleton integrated with core; native host PASS; ARM build PASS; ARM/Hakai execution pending.
- E: native UI inputs, generated skin, Q-Link maps, read-only diagnostics and offline previews implemented. Live MPC interaction untested; device/scene browsers are informational foundations.
- F: protocol risks, one-output entry criteria and rollback plan documented. This checkpoint ends before deployment.

## Remaining gates

A production backend still needs verified discovery/shared-stream serialization, timestamped gate edge queue, overflow policy, watchdog, bounded real worker shutdown and native-track/other-plugin conflict management. Add fault tests for those concrete implementations rather than treating the current models as substitutes.

Next independent step: provision a verified Linux ARM sysroot/QEMU environment and run both ARM unit tests and a Linux VST host harness. Review binary-runtime notices before release packaging. Only after a separately authorized hardware phase should one output be measured for map, voltage, settling, pulse timing and failure behavior. Details are in `HARDWARE_READINESS.md` and `RT_AUDIT.md`.
