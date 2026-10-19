# MPC-CV — Phase 0

Independent local research and offline C++17 prototype for an eight-output modulation platform targeting MPC One / Hakai. **Physical output is not implemented. This is not an installable VST2 plugin.**

Run:

```sh
make test
./build/simulate > build/demo.csv
```

The core implements generic sine/triangle LFOs, transport gate, eight-step sequence, deterministic random hold, candidate 14-bit CC encoding, conflict-rejecting ownership model, bounded SPSC queue and partial-write cursor model. Four logical outputs start enabled; eight are supported. Every engine starts disarmed. Stop or invalid transport produces zero snapshots.

- [Source audit and compatibility findings](docs/RESEARCH.md)
- [Architecture and production integration contract](docs/ARCHITECTURE.md)
- [Test coverage and future acceptance gates](docs/VALIDATION.md)
- `include/mpccv/core.hpp`: independently authored core
- `src/simulate.cpp`: offline CSV demo
- `tests/core_test.cpp`: executable tests
- `research/`: isolated reference material with upstream licenses and pinned inventories; never part of the build

No repository was created remotely, no code published, no PR opened, no plugins installed and no MPC accessed or modified. The supplied prompt ended mid-sentence; this implementation covers the visible Phase 0 requirements without inventing missing requirements.

## Consolidation — 19 October 2026

Official repository: https://github.com/carballocba/MPC-CV-LAB-

The original [research plan](docs/RESEARCH_PLAN.md) remains unchanged as historical evidence. The initiator now reports successful Glass Spheres CV control on MPC One/Hakai; that is practical user evidence, not validation of this project's backend. Phase 0 is preserved under `archive/phase0/`. Upstream source remains locally available under ignored `research/glass-spheres-source/`; inventories and MIT notices are tracked without redistributing upstream artwork or deployment scripts.
