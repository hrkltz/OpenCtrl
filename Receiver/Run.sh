#!/bin/bash

set -euo pipefail

HRKLTZ_OPENCTRL_GIT_DIR="/Volumes/Git/GitHub/hrkltz/OpenCtrl"
SCRIPT_DIR="$HRKLTZ_OPENCTRL_GIT_DIR/Receiver"
SCRIPT_VENV_DIR="$SCRIPT_DIR/.venv"

# Function to display usage
usage() {
    echo "Usage: $0 --type <keyboard|mouse> [options]"
    echo ""
    echo "Run the OpenCtrl Receiver logger."
    echo ""
    echo "Options:"
    echo "  --type <keyboard|mouse>  Specify the type of logger to run (required)"
    echo "  --help                   Show this help message"
    echo ""
    echo "Additional options are passed to the logger script."
    echo ""
    echo "Examples:"
    echo "  $0 --type keyboard"
    echo "  $0 --type mouse --catch"
    exit 1
}

# Parse arguments
TYPE=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --type)
            if [[ $# -lt 2 ]]; then
                echo "Error: --type requires a value (keyboard or mouse)" >&2
                usage
            fi
            TYPE="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            # Collect remaining arguments to pass to python script
            break
            ;;
    esac
done

# Validate type
if [[ -z "$TYPE" ]]; then
    echo "Error: --type is required" >&2
    usage
fi

if [[ "$TYPE" != "keyboard" && "$TYPE" != "mouse" ]]; then
    echo "Error: Invalid type '$TYPE'. Must be 'keyboard' or 'mouse'." >&2
    usage
fi

# Determine script to run
if [[ "$TYPE" == "keyboard" ]]; then
    SCRIPT="$SCRIPT_DIR/keyboard_logger.py"
elif [[ "$TYPE" == "mouse" ]]; then
    SCRIPT="$SCRIPT_DIR/mouse_logger.py"
fi

# Check if script exists
if [[ ! -f "$SCRIPT" ]]; then
    echo "Error: Script $SCRIPT not found." >&2
    exit 1
fi

# Activate virtual environment
source "$SCRIPT_VENV_DIR/bin/activate"

# Run the script with remaining arguments
python3 "$SCRIPT" "$@"
