# Device profile format v1

Profiles describe external inputs; they never enable hardware or claim measured compatibility. `schema_version` must be 1. `id`, `name` and `verification` are nonempty strings. `inputs` is a list of 1–32 records with unique `id` and nonempty `label`.

Each input defines `signal_type` (`continuous`, `pitch`, `gate` or `trigger`), `polarity` (`unipolar` or `bipolar`), finite `minimum_volts < maximum_volts`, optional finite `threshold_volts` within that range, finite nonzero `scale` and finite `offset_volts`. Nullable `default_virtual_source` refers to a virtual source ID; nullable `default_logical_output` is an integer 1–8. These are suggested assignments, not automatic routing. Profiles can describe bipolar devices even though the candidate MPC transport cannot generate negative voltage. The route must explicitly transform and clamp to 0–5 V.

The generic profile is validated by `tools/validate_assets.py`. No specific Clouds/Kammerl, Pamela, Phazerville or Data Bender profile is claimed. Their variants, firmware, input impedance, ranges and thresholds require official documentation and separate validation. The VST does not yet load JSON profiles; this is an extensible file contract and generic fixture, not a device browser.
