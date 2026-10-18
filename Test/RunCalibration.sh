#!/bin/bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
"${CXX:-c++}" -std=c++17 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$repo_dir/Test/Stubs" -I"$repo_dir/Libraries/M5StackInputs/src" \
    -I"$repo_dir/Libraries/ControllerCalibration/src" \
    -I"$repo_dir/.arduino/user/libraries/ArduinoJson/src" \
    "$repo_dir/Test/calibration_test.cpp" \
    "$repo_dir/Libraries/M5StackInputs/src/InputUtils.cpp" \
    "$repo_dir/Libraries/ControllerCalibration/src/ControllerCalibration.cpp" \
    -o "$build_dir/calibration-tests"
"$build_dir/calibration-tests"
