# OpenCtrl / XIAOESP32S3Core

A USB HID controller hub for XIAO ESP32S3, providing keyboard, mouse, gamepad, pen, and touch inputs via web API. Displays the IP address on an OLED screen.

## Part List

- 1x [XIAO ESP32S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- 1x USB-C to USB-C or USB-A cable
- 1x [Seeed Studio XIAO Expansion Board](https://www.seeedstudio.com/Seeeduino-XIAO-Expansion-board-p-4746.html) (optional)

## Setup

```bash
# Get the board package for the XIAO ESP32S3.
arduino-cli --config-file arduino-cli.yaml core update-index
arduino-cli --config-file arduino-cli.yaml core install esp32:esp32

# Choose the right FQBN for your specific board. You may need to update the Build.sh script.
arduino-cli --config-file arduino-cli.yaml board listall xiao
Board Name        FQBN
XIAO_ESP32C3      esp32:esp32:XIAO_ESP32C3
XIAO_ESP32C5      esp32:esp32:XIAO_ESP32C5
XIAO_ESP32C6      esp32:esp32:XIAO_ESP32C6
XIAO_ESP32S3      esp32:esp32:XIAO_ESP32S3
XIAO_ESP32S3_PLUS esp32:esp32:XIAO_ESP32S3_Plus

# Install libraries.
## HTTP server
arduino-cli --config-file arduino-cli.yaml lib install "Async TCP@3.4.10" "ArduinoJson@7.4.2" "ESP Async WebServer@3.9.4"
## Display
arduino-cli --config-file arduino-cli.yaml lib install U8g2@2.35.30

```

>Note: You need to set your WiFi SSID and password! You can either do this directly in the source code or use an environment variable. Stick to the second option if you plan to share your code to avoid leaking your network secrects.

## Usage

Now you ready to use the [build](./Build.sh) script to actually compile and upload OpenCtrl to your XIAO ESP32S3. After uploading, the device connects to WiFi and displays the IP on the OLED. Send POST requests to the endpoints:

- `/mouse`: `{"dx": int, "dy": int, "wheel": int, "buttons": int}`
- `/keyboard`: `{"modifiers": int, "keys": [int, ...]}`
- `/gamepad`: `{"buttons": int, "x": int, "y": int}`
- `/pen`: `{"tip": bool, "inRange": bool, "x": int, "y": int, "pressure": int}`
- `/touch`: `{"tip": bool, "inRange": bool, "contactId": int, "x": int, "y": int, "contactCount": int}`
