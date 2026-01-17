# OpenCtrl / Receiver

This Python 3.9 CLI tool logs HID events on macOS using a CGEventTap. By default it **suppresses** events so they do not reach the OS (safer for testing). Use `--passthrough` to allow normal OS handling.

## Requirements

- macOS (CGEventTap)
- Python 3.9
- Accessibility permission for the terminal running the script

## Setup

Please run the following commands to prepare the virtual environment with all dependencies.

```zsh
user@device Receiver % python3 -m venv ".venv/"
user@device Receiver % source ".venv/bin/activate"
user@device Receiver % python3 -m pip install --upgrade pip
user@device Receiver % python3 -m pip install -r "requirements.txt"
```

## Usage

```zsh
# Use '--catch' to suppress events so the OS won't handle them while logging.
# Caution: with suppression enabled, normal input may be blocked and might force you kill your computer with the power switch. The keyboad button 'q' is implemented to act as an kill-switch.
./Run.sh --type keyboard
```

>Note: If the script fails to create an event tap, grant Accessibility permission:
System Settings → Privacy & Security → Accessibility → enable your terminal app. For example if you're using VS Code you need to enable this for "Visual Studio Code.app".

