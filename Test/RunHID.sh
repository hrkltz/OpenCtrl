#!/bin/bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
node --test "$repo_dir/Test/web_hid.test.js"
"${CXX:-c++}" -std=c++17 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$repo_dir/Test/Stubs" -I"$repo_dir/Source/XIAOESP32S3Core" \
    -I"$repo_dir/.arduino/user/libraries/ArduinoJson/src" \
    "$repo_dir/Test/hid_native_test.cpp" \
    "$repo_dir/Source/XIAOESP32S3Core/HIDDescriptor.cpp" \
    "$repo_dir/Source/XIAOESP32S3Core/HIDUtil.cpp" \
    "$repo_dir/Source/XIAOESP32S3Core/HIDProfile.cpp" \
    "$repo_dir/Source/XIAOESP32S3Core/HIDUSBTransport.cpp" \
    "$repo_dir/Source/XIAOESP32S3Core/HIDBLETransport.cpp" \
    "$repo_dir/Examples/WebController/XIAOESP32S3Core/HIDPen.cpp" \
    -o "$build_dir/hid-tests"
"$build_dir/hid-tests"
"$build_dir/hid-tests" --custom-only
"$build_dir/hid-tests" --mouse-only
"$build_dir/hid-tests" --bluetooth-only
"$build_dir/hid-tests" --mixed-transports
"$build_dir/hid-tests" --web-usb
for web_mode in 1 2 3; do
    "${CXX:-c++}" -std=c++17 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
        -DOPENCTRL_WEB_MODE="$web_mode" \
        -I"$repo_dir/Test/Stubs" -I"$repo_dir/Source/XIAOESP32S3Core" \
        -I"$repo_dir/.arduino/user/libraries/ArduinoJson/src" \
        "$repo_dir/Test/web_controller_test.cpp" \
        "$repo_dir/Source/XIAOESP32S3Core/HIDDescriptor.cpp" \
        "$repo_dir/Source/XIAOESP32S3Core/HIDUtil.cpp" \
        "$repo_dir/Source/XIAOESP32S3Core/HIDProfile.cpp" \
        "$repo_dir/Source/XIAOESP32S3Core/HIDUSBTransport.cpp" \
        "$repo_dir/Source/XIAOESP32S3Core/HIDBLETransport.cpp" \
        "$repo_dir/Examples/WebController/XIAOESP32S3Core/HIDPen.cpp" \
        -o "$build_dir/web-$web_mode"
    "$build_dir/web-$web_mode"
done
