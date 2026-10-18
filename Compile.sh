#!/bin/bash

# OpenCtrl Compile Script
# Compiles a project without keeping any output, e.g. to check that it builds.
# Usage: ./Compile.sh --project <path> [options]
# Options:
#   --help, -h        Show this help message
#   --project <path>  Project to compile, relative to this folder (e.g. Examples/WebController)
#
# BuildAndDeploy.sh sources this file to reuse its functions.

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fqbn="esp32:esp32:XIAO_ESP32S3:USBMode=default,CDCOnBoot=cdc"

# Print the sketch folder of a project: the project itself or a subfolder, e.g. Examples/WebController/XIAOESP32S3Core.
find_sketch() {
    local candidate
    for candidate in "$repo_dir/${1%/}" "$repo_dir/${1%/}"/*/; do
        candidate="${candidate%/}"
        if [ -f "$candidate/$(basename "$candidate").ino" ]; then
            echo "$candidate"
            return 0
        fi
    done
    echo "Error: No sketch found in '$1'." >&2
    return 1
}

# Print the core of a sketch. The sketch folder is named after it, e.g. XIAOESP32S3Core.
find_core() {
    echo "$repo_dir/Source/$(basename "$1")"
}

# Run arduino-cli with the configuration of a core.
core_cli() {
    local core_dir="$1"
    shift
    (cd "$core_dir" && arduino-cli --config-file arduino-cli.yaml "$@")
}

# Write WebFiles.hpp with one byte array per file of a www folder and a table to serve them.
generate_web_files() {
    local www_files=("$1"/*)
    local i name content_type
    echo "// Generated from www/ by Compile.sh. Do not edit."
    echo "#pragma once"
    echo "#include <stddef.h>"
    echo "#include <stdint.h>"
    echo ""
    echo "struct WebFile {"
    echo "  const char* path;"
    echo "  const char* contentType;"
    echo "  const uint8_t* data;"
    echo "  size_t size;"
    echo "};"
    for i in "${!www_files[@]}"; do
        echo ""
        echo "static const uint8_t WEB_FILE_$i[] = {"
        xxd -i < "${www_files[$i]}" || return 1
        echo "};"
    done
    echo ""
    echo "static const WebFile WEB_FILES[] = {"
    for i in "${!www_files[@]}"; do
        name="$(basename "${www_files[$i]}")"
        case "$name" in
            *.html) content_type="text/html" ;;
            *.css) content_type="text/css" ;;
            *.js) content_type="text/javascript" ;;
            *.svg) content_type="image/svg+xml" ;;
            *) content_type="application/octet-stream" ;;
        esac
        echo "  {\"/$name\", \"$content_type\", WEB_FILE_$i, sizeof(WEB_FILE_$i)},"
    done
    echo "};"
}

# Compile a project into a build folder.
# Usage: compile <project> <build_dir> [arduino-cli compile flags...]
compile() {
    local project="$1"
    local build_dir="$2"
    shift 2
    local sketch_dir core_dir
    sketch_dir="$(find_sketch "$project")" || return 1
    core_dir="$(find_core "$sketch_dir")"

    # Examples use their core as library.
    local flags=("$@")
    flags+=(--libraries "$repo_dir/Libraries")
    if [ "$sketch_dir" != "$core_dir" ]; then
        flags+=(--library "$core_dir")
    fi

    # A www folder in the project is embedded as WebFiles.hpp, generated next to the build folder.
    # The folder stays the same between builds, so the compiler flags do too and builds stay incremental.
    local include_flag=""
    if [ -d "$repo_dir/${project%/}/www" ]; then
        echo "==> Embedding web assets from '${project%/}/www'..."
        mkdir -p "$build_dir.www" || return 1
        generate_web_files "$repo_dir/${project%/}/www" > "$build_dir.www/WebFiles.hpp" || return 1
        include_flag=" -I$build_dir.www"
    fi

    echo "==> Compiling '${project%/}' (core: $(basename "$core_dir"))..."
    core_cli "$core_dir" compile -b "$fqbn" --build-path "$build_dir" "${flags[@]}" \
      --build-property "compiler.cpp.extra_flags=-DWIFI_SSID=\"${HRKLTZ_OPENCTRL_WIFI_SSID}\" -DWIFI_PASSWORD=\"${HRKLTZ_OPENCTRL_WIFI_PASSWORD}\"$include_flag" \
      "$sketch_dir"
    if [ $? -ne 0 ]; then
        echo "Error: Compilation failed."
        return 1
    fi
    echo "==> Compilation finished."
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    project=""

    while [[ $# -gt 0 ]]; do
        case $1 in
            --help|-h)
                echo "Usage: $0 --project <path>"
                echo ""
                echo "Compile an OpenCtrl project without keeping any output, e.g. to check that it builds."
                echo ""
                echo "Options:"
                echo "  --help, -h        Show this help message"
                echo "  --project <path>  Project to compile, relative to this folder"
                echo ""
                echo "Examples:"
                echo "  $0 --project Source/XIAOESP32S3Core"
                echo "  $0 --project Examples/WebController"
                exit 0
                ;;
            --project)
                project="$2"
                shift 2
                ;;
            *)
                echo "Error: Unknown option '$1'"
                echo "Use '$0 --help' for usage information."
                exit 1
                ;;
        esac
    done

    if [ -z "$project" ]; then
        echo "Error: Missing --project."
        echo "Use '$0 --help' for usage information."
        exit 1
    fi

    build_dir="$(mktemp -d)" || exit 1
    trap 'rm -rf "$build_dir" "$build_dir.www"' EXIT
    compile "$project" "$build_dir"
fi
