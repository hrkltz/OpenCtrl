#!/bin/bash

# XIAOESP32S3Core Build Script
# Usage: ./Build.sh [options]
# Options:
#   --help, -h    Show this help message
#   --clean       Force clean build (default is incremental)
#   --dry         Skip upload after compilation

# Default flags
clean_flag=""
dry=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Build and optionally upload the XIAOESP32S3Core project for XIAO ESP32S3."
            echo ""
            echo "Options:"
            echo "  --help, -h    Show this help message"
            echo "  --clean       Force clean build (rebuilds everything)"
            echo "  --dry         Compile only, skip upload"
            echo ""
            echo "Examples:"
            echo "  $0                # Incremental build and upload"
            echo "  $0 --clean --dry  # Clean build, no upload"
            exit 0
            ;;
        --clean)
            clean_flag="--clean"
            shift
            ;;
        --dry)
            dry=true
            shift
            ;;
        *)
            echo "Error: Unknown option '$1'"
            echo "Use '$0 --help' for usage information."
            exit 1
            ;;
    esac
done

arduino-cli --config-file arduino-cli.yaml compile $clean_flag -b esp32:esp32:XIAO_ESP32S3 \
  --build-property "compiler.cpp.extra_flags=-DWIFI_SSID=\"${HRKLTZ_OPENCTRL_WIFI_SSID}\" -DWIFI_PASSWORD=\"${HRKLTZ_OPENCTRL_WIFI_PASSWORD}\"" \
  .

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed."
    exit 1
fi

if [ "$dry" = false ]; then
    arduino-cli --config-file arduino-cli.yaml upload -p /dev/cu.usbmodem1101 -b esp32:esp32:XIAO_ESP32S3 .
    if [ $? -ne 0 ]; then
        echo "Error: Upload failed."
        exit 1
    fi
fi
