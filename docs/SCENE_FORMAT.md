# Offline scene/preset authoring

`tools/scene_tool.py` is a standard-library-only control-plane utility. It does not talk to MPC, install a plugin, or call the render callback. It generates the existing v1 MCVS scene bytes accepted by the C++ scene loader. It is not an MPC program (.xpl) file or installer.

```sh
python3 tools/scene_tool.py compile scenes/generic-performance.json build/generic.mcvs
python3 tools/scene_tool.py decode build/generic.mcvs build/generic-decoded.json
python3 tools/scene_tool.py preset scenes/generic-performance.json presets/sine.engine.json 3 build/modified-scene.json
python3 tools/scene_tool.py compile build/modified-scene.json build/modified.mcvs
```

Preset node indices are zero-based; 3 means virtual node 4. Applying a preset replaces that node definition with preset/default parameters, then validates the whole graph. It does not change output routes or alter the source JSON. The resulting files must still be supplied through an appropriate host chunk control path; there is no file browser or automatic import in the VST UI.

A performance scene has `schema_version: 1`, `kind: performance_scene`, a 0–255 enabled mask, `sync: true`, four nullable node IDs in `controls`, up to sixteen `nodes`, and eight `routes`. Controls are stored metadata, not dynamic Q-Link remapping. The example enables only the first four logical outputs.

Node fields/defaults are declared in `NODE_DEFAULT`: engine type, optional input/modulator IDs, rate, scale, offset, modulation amount, slew, quantum, eight steps and seed. All known C++ kinds are supported. Virtual nodes can share routes, modulate rates, feed mix/slew/quantizer nodes, and be reused without a physical-output identity. Cycles and missing references are rejected. Routes contain an optional source, scale, offset and 0–5 V interval.

Engine presets use `kind: engine_preset`, `schema_version: 1` and an `engine` object with those same node fields. Device profiles remain a separate JSON contract in `profiles/FORMAT.md`; this tool does not infer an external module's electrical requirements or load device profiles into VST.

JSON inputs are capped at 64 KiB; binary input at 4096 bytes. Unknown versions/fields, duplicate keys, nonfinite numbers, booleans used as integers, invalid references, range violations, reversed voltage limits and unsupported independent clocks fail before output replacement. Binary length/checksum/topology are revalidated during decode. The FNV checksum detects corruption, not malicious tampering.

Writes use a temporary file beside the destination, flush/fsync its contents, then atomic rename. Invalid inputs leave the destination unchanged. This is not a full multi-file transaction or a power-loss guarantee for directory metadata. All filesystem work stays outside audio.

`make check` runs six Python tests and a native C++ roundtrip. CI also runs the byte-identical roundtrip through C++ ARM under QEMU for both GNU and Zig toolchains. The binary stores numeric graph state, not arbitrary JSON labels or profile/browser metadata.
