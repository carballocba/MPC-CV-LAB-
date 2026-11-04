#!/bin/sh
# Builds a local portable test package. It does not connect to the MPC.
set -eu
cd "$(dirname "$0")/.."
: "${MPC_VST_FRAMEWORK:?Set to the reviewed Open MPC framework checkout}"
export MPC_VST_FRAMEWORK
tools/build_skin.sh
python3 tools/inspect_elf.py build/armv7/mpc_cv.so >/dev/null
python3 "$MPC_VST_FRAMEWORK/tools/release.py" \
    --so build/armv7/mpc_cv.so \
    --skin 'ui/native/build/skin/carballocba - VST - MPC-CV LAB' \
    --entry ui/native/build/pluginlist-entry.xml \
    --id mpc-cv-lab --version 0.1.0-experimental \
    --repo carballocba/MPC-CV-LAB- \
    --about 'Local ARMv7 host-load candidate with in-memory CV capture' \
    --out build/akai-candidate
