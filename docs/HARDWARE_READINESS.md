# Checkpoint F — preparation only

Physical backend is absent. There is no installation script, rawmidi opener/writer, service command or MPC.settings editor in the new project. The owner authorized reversible device work on 4 November. See [device validation](MPC_VALIDATION_2026-11-04.md) for the isolated ARM test and [meter procedure](CV_MULTIMETER.md) for the electrical trial.

## Entry criteria for a later single-output trial

1. Record exact MPC One model, Hakai/MPC version, host ABI and controller identity. User-reported Glass Spheres operation establishes practical feasibility only; it does not validate MPC-CV.
2. Resolve runtime dependency availability, retain the passing GNU/Zig QEMU tests, then separately validate Akai plugin loading and native skin/Q-Link behavior with outputs still disabled. Preserve the project and existing settings before any supervised installation.
3. Implement/audit discovery and stream coordination first: exact output-substream identity, ambiguous-port rejection, fd close/reuse validation, no status-flag changes on borrowed descriptors, no assumptions about host SysEx interleaving.
4. Retain the integrated capture coordinator/reservation tests, then design physical scheduling, partial shared-stream ownership, explicit signal roles and reconnect safe handshake. The in-memory sink is not a physical endpoint. Prove all fault paths offline before enabling transport.
5. Select one logical output and establish connector/tip/ring mapping from official documentation plus measurement. Do not infer physical wiring from adjacent CC numbers. Exclude native CV tracks and other plugin writers on the reserved output.
6. With Eurorack disconnected, use a meter/scope and begin at zero. Test conservative fixed voltages and measure actual range before pitch or gates. Validate slew and clamp. Stop immediately on unexpected output, stalled shutdown, wrong channel or ambiguity.
7. Only after measuring correct behavior connect an external input whose official range is known. Keep all other outputs disabled. Record scope timing and firmware-specific results; do not extrapolate to eight lanes.

## Rollback design

Before a supervised install: save the MPC session, preserve settings/plugin folder listings, and define an identifiable staged plugin directory. Rollback must remove only MPC-CV's own registration/artifacts and restore the specific prior settings, after user approval for any restart. This phase supplies no commands that perform those changes. If a future test leaves a DAC latched, software shutdown is not a reliable remedy: disconnect the test destination and follow the device's normal recovery procedure. Document failures rather than claiming a successful zero.

## Outstanding compatibility checks

Hakai 3.9.1: user reports Glass Spheres CV works; MPC-CV not tested.
Hakai 3.9: not tested.
MPC X: upstream protocol evidence only; MPC-CV not tested.
MPC One cabling: four TRS connectors/eight signals documented by Akai. Four logical routes enabled initially; full breakout mapping/calibration unverified by this project.

## Installation and electrical trial

The 4 November authorization supersedes the earlier installation hold. An isolated ARM smoke test passed. Registration in the Akai host is pending diagnosis of Community Installer and a reversible registration path. Physical emission requires confirmation that the outputs are disconnected from external modules and a validated backend.
