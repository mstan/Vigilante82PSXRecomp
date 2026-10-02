#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$root/build-linux-release"}
framework=${PSXRECOMP_ROOT:-"$root/psxrecomp-v4"}
python=${V82_PYTHON:-python3}
encoder=${V82_PSXAVENC:?Set V82_PSXAVENC to a native psxavenc 0.3.1 executable}
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER= \
    -DPSX_DEBUG_TOOLS=OFF -DPSXRECOMP_ROOT="$framework" \
    -DRECOMP_UI_ROOT="$root/recomp-ui" -DPython3_EXECUTABLE="$python" -DPSX_PYTHON="$python" \
    -DV82_PSXAVENC="$encoder" '-DPSXRECOMP_BIOS_STEMS=OpenBIOS;SCPH1001'
cmake --build "$build" -j "${V82_BUILD_JOBS:-8}"
ctest --test-dir "$build" --output-on-failure
