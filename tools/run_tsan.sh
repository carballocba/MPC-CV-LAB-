#!/bin/sh
# Concurrency audit with an existing compiler/runtime; never installs tools.
set -eu
cd "$(dirname "$0")/.."
compiler=${CXX:-clang++}
output="$PWD/build/tsan-current"
mkdir -p "$output"
for test_name in core_test event_pipeline_test capture_test; do
    set -- "tests/$test_name.cpp"
    if [ "$test_name" = capture_test ]; then set -- "$@" src/capture.cpp; fi
    "$compiler" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -fsanitize=thread "$@" -o "$output/$test_name"
    "$output/$test_name"
done
if [ "$(uname -s)" = Darwin ]; then
    set -- -dynamiclib
else
    set -- -shared -fPIC -Wl,--exclude-libs,ALL
fi
"$compiler" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -fsanitize=thread "$@" -fvisibility=hidden src/vst/plugin.cpp src/capture.cpp -o "$output/mpc_cv.so"
for test_name in vst_host_test vst_capture_test; do
    set -- "tests/$test_name.cpp"
    if [ "$(uname -s)" != Darwin ]; then set -- "$@" -ldl; fi
    "$compiler" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -fsanitize=thread "$@" -o "$output/$test_name"
    "$output/$test_name" "$output/mpc_cv.so"
done
