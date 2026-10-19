#!/bin/sh
# Offline cross-build only. ZIG must name a separately verified Zig 0.15.2 executable.
set -eu
cd "$(dirname "$0")/.."
: "${ZIG:?Set ZIG to the absolute path of verified Zig 0.15.2}"
[ "$("$ZIG" version)" = "0.15.2" ] || { echo 'Expected Zig 0.15.2' >&2; exit 1; }
mkdir -p build/armv7 build/zig-cache build/zig-global
export ZIG_GLOBAL_CACHE_DIR="$PWD/build/zig-global"
export ZIG_LOCAL_CACHE_DIR="$PWD/build/zig-cache"
"$ZIG" c++ -target arm-linux-gnueabihf.2.31 -mcpu=cortex_a9 -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -shared -fPIC src/vst/plugin.cpp -o build/armv7/mpc_cv.so
for test_name in core_test backend_test graph_test; do
    "$ZIG" c++ -target arm-linux-gnueabihf.2.31 -mcpu=cortex_a9 -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -UNDEBUG "tests/$test_name.cpp" -o "build/armv7/$test_name"
done
python3 tools/inspect_elf.py build/armv7/mpc_cv.so > build/armv7/elf-report.json
cat build/armv7/elf-report.json
