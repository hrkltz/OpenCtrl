#!/bin/bash

# OpenCtrl Build and Deploy Script
# Compiles a project and uploads it to the first connected board.
# Usage: ./BuildAndDeploy.sh --project <path> [options]
# Options:
#   --help, -h        Show this help message
#   --project <path>  Project to build, relative to this folder (e.g. Examples/WebController)
#   --clean           Force clean build (default is incremental)

source "$(dirname "${BASH_SOURCE[0]}")/Compile.sh"

project=""
clean_flag=()

while [[ $# -gt 0 ]]; do
    case $1 in
        --help|-h)
            echo "Usage: $0 --project <path> [options]"
            echo ""
            echo "Build an OpenCtrl project and upload it to the first connected board."
            echo ""
            echo "Options:"
            echo "  --help, -h        Show this help message"
            echo "  --project <path>  Project to build, relative to this folder"
            echo "  --clean           Force clean build (rebuilds everything)"
            echo ""
            echo "Examples:"
            echo "  $0 --project Source/XIAOESP32S3Core"
            echo "  $0 --project Examples/WebController --clean"
            exit 0
            ;;
        --project)
            project="$2"
            shift 2
            ;;
        --clean)
            clean_flag=(--clean)
            shift
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

# Keep the build output for incremental builds.
build_dir="$repo_dir/.arduino/build/${project%/}"
if [ ${#clean_flag[@]} -gt 0 ]; then
    echo "==> Starting clean build and deploy for '${project%/}'..."
else
    echo "==> Starting incremental build and deploy for '${project%/}'..."
fi

compile "$project" "$build_dir" "${clean_flag[@]}" || exit 1

find_serial_port() {
    ls /dev/cu.usbmodem* 2>/dev/null | head -n 1
}

# Check whether the ESP32-S3 ROM USB-Serial/JTAG bootloader (0x303a:0x1001) is active on USB.
is_bootloader_mode() {
    if command -v ioreg >/dev/null 2>&1; then
        ioreg -p IOUSB -w 0 -l 2>/dev/null | grep -q '"USB JTAG/serial debug unit"'
        return $?
    fi
    return 1
}

# Check whether a XIAO ESP32S3 running TinyUSB (0x2886:0x0056) is visible on USB.
is_xiao_usb_present() {
    if command -v ioreg >/dev/null 2>&1; then
        ioreg -p IOUSB -w 0 -l 2>/dev/null | grep -q '"XIAO_ESP32S3"'
        return $?
    fi
    return 1
}

# Send the 1200bps CDC line-coding + DTR/RTS reset sequence so TinyUSB reboots
# the ESP32-S3 into its ROM USB-Serial/JTAG download bootloader.
trigger_bootloader_reset() {
    local target_port="$1"
    python3 - "$target_port" <<'PY' 2>/dev/null
import fcntl, os, struct, sys, termios, time

port = sys.argv[1]
TIOCMBIS = 0x8004746c
TIOCMBIC = 0x8004746b
TIOCM_DTR = 0x002
TIOCM_RTS = 0x004

fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    attrs = termios.tcgetattr(fd)
    attrs[4] = attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    time.sleep(0.05)

    # 1200bps touch triggers USBCDC::_onLineCoding(1200) -> usb_persist_restart(RESTART_BOOTLOADER)
    attrs[4] = attrs[5] = termios.B1200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    time.sleep(0.05)

    # Also step through the TinyUSB CDC DTR/RTS reboot state machine as fallback.
    for dtr, rts in ((False, True), (True, True), (True, False), (False, False)):
        fcntl.ioctl(fd, TIOCMBIS if dtr else TIOCMBIC, struct.pack("I", TIOCM_DTR))
        fcntl.ioctl(fd, TIOCMBIS if rts else TIOCMBIC, struct.pack("I", TIOCM_RTS))
        time.sleep(0.03)
finally:
    os.close(fd)
PY
}

echo "==> Searching for connected board..."
port="$(find_serial_port)"

if [ -n "$port" ]; then
    if is_bootloader_mode; then
        echo "==> Found board on $port (already in flash mode)."
    else
        echo "==> Found board on $port (application mode). Sending reset command to enter flash mode..."
        if trigger_bootloader_reset "$port"; then
            echo "==> Waiting for board to reboot into flash mode..."
            for _ in {1..30}; do
                sleep 0.25
                new_port="$(find_serial_port)"
                if [ -n "$new_port" ] && { is_bootloader_mode || [ "$new_port" != "$port" ]; }; then
                    port="$new_port"
                    break
                fi
            done
            port="$(find_serial_port)"
            if [ -n "$port" ]; then
                echo "==> Board ready on $port."
            fi
        else
            echo "==> Warning: Could not send software reset command to $port."
        fi
    fi
fi

if [ -z "$port" ]; then
    if is_xiao_usb_present; then
        echo "==> Board is connected in USB HID mode without an active serial port."
    else
        echo "==> No /dev/cu.usbmodem* serial port detected."
    fi
    echo "==> Action required: Put the Seeed Studio XIAO ESP32S3 into flash mode:"
    echo "    1. Press and hold the BOOT (B) button"
    echo "    2. Tap the RESET (R) button"
    echo "    3. Release the BOOT (B) button"
    echo "==> Waiting up to 60s for flashable serial port (/dev/cu.usbmodem*)..."
    for _ in {1..120}; do
        port="$(find_serial_port)"
        if [ -n "$port" ]; then
            echo "==> Detected board in flash mode on $port."
            sleep 0.5
            break
        fi
        sleep 0.5
    done
    if [ -z "$port" ]; then
        echo "Error: Timed out waiting for /dev/cu.usbmodem* device."
        exit 1
    fi
fi

sketch_dir="$(find_sketch "$project")"
core_dir="$(find_core "$sketch_dir")"
echo "==> Uploading '${project%/}' to $port..."
core_cli "$core_dir" upload -p "$port" -b "$fqbn" --input-dir "$build_dir" "$sketch_dir"
if [ $? -ne 0 ]; then
    echo "Error: Upload failed."
    exit 1
fi
echo "==> Deployment complete! '${project%/}' is now running on the board."
