#!/bin/bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
"${CXX:-c++}" -std=c++17 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$repo_dir/Test/Stubs" -I"$repo_dir/Libraries/M5StackInputs/src" \
    "$repo_dir/Test/m5_units_test.cpp" \
    "$repo_dir/Libraries/M5StackInputs/src/M5StackInputs.cpp" \
    "$repo_dir/Libraries/M5StackInputs/src/DeviceIO.cpp" \
    "$repo_dir/Libraries/M5StackInputs/src/InputUtils.cpp" \
    -o "$build_dir/unit-tests"
"$build_dir/unit-tests"
for flight in 0 1; do
    for transport in 1 2; do
        "${CXX:-c++}" -std=c++17 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
            -DTEST_FLIGHT="$flight" -DOPENCTRL_CONTROLLER_TRANSPORT="$transport" \
            -I"$repo_dir/Test/Stubs" -I"$repo_dir/Source/XIAOESP32S3Core" \
            -I"$repo_dir/Libraries/M5StackInputs/src" \
            -I"$repo_dir/Libraries/ControllerCalibration/src" \
            -I"$repo_dir/.arduino/user/libraries/ArduinoJson/src" \
            "$repo_dir/Test/m5_inputs_test.cpp" \
            "$repo_dir/Libraries/M5StackInputs/src/M5StackInputs.cpp" \
            "$repo_dir/Libraries/M5StackInputs/src/DeviceIO.cpp" \
            "$repo_dir/Libraries/M5StackInputs/src/InputUtils.cpp" \
            "$repo_dir/Libraries/ControllerCalibration/src/ControllerCalibration.cpp" \
            "$repo_dir/Source/XIAOESP32S3Core/HIDDescriptor.cpp" \
            "$repo_dir/Source/XIAOESP32S3Core/HIDProfile.cpp" \
            "$repo_dir/Source/XIAOESP32S3Core/HIDUtil.cpp" \
            "$repo_dir/Source/XIAOESP32S3Core/HIDUSBTransport.cpp" \
            "$repo_dir/Source/XIAOESP32S3Core/HIDBLETransport.cpp" \
            -o "$build_dir/inputs-$flight-$transport"
        "$build_dir/inputs-$flight-$transport"
    done
done
