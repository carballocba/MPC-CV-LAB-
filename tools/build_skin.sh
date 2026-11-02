#!/bin/sh
# Build skin artifacts offline using an explicitly supplied reviewed framework checkout.
set -eu
cd "$(dirname "$0")/.."
: "${MPC_VST_FRAMEWORK:?Set MPC_VST_FRAMEWORK to the reviewed framework checkout}"
expected=26e4581dee9ed128a5b34e306134f1fc97b7ff0e
[ "$(git -C "$MPC_VST_FRAMEWORK" rev-parse HEAD)" = "$expected" ] || { echo 'Unreviewed framework commit' >&2; exit 1; }
mkdir -p build
python3 tools/generate_ui.py
cc -O2 -I"$MPC_VST_FRAMEWORK/tools/vendor/force-shadow/tools" "$MPC_VST_FRAMEWORK/tools/shadow_art.c" -lm -o build/shadow_art
export SHADOW_ART="$PWD/build/shadow_art"
python3 "$MPC_VST_FRAMEWORK/tools/gen_vst.py" ui/native/vst.json
cp "$MPC_VST_FRAMEWORK/LICENSE" 'ui/native/build/skin/carballocba - VST - MPC-CV LAB/FRAMEWORK-LICENSE.txt'
cp "$MPC_VST_FRAMEWORK/tools/vendor/force-shadow/LICENSE" 'ui/native/build/skin/carballocba - VST - MPC-CV LAB/FORCE-SHADOW-LICENSE.txt'
python3 "$MPC_VST_FRAMEWORK/tools/studio.py" preview 'ui/native/build/skin/carballocba - VST - MPC-CV LAB/Plugin Skins' -o "$PWD/build/ui-preview-%d.png"
