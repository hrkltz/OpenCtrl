# [OpenCtrl](https://github.com/hrkltz/OpenCtrl)

OpenCtrl is an open-source playground for building custom user interfaces and control surfaces which are fast to prototype, easy to extend, and friendly to tinkerers. The goal is a small ecosystem of reusable modules and examples that help you go from idea to working input in minutes. 

## Structure

- `Source/`: reusable source modules. A source module owns the common input/output implementation for a platform, for example the XIAO ESP32S3 HID web API firmware.
- `Examples/`: concrete examples built on top of the cores, such as phone friendly web views, flight and driving controllers, or the steering wheel prototype.
- `Libraries/`: optional hardware libraries shared by examples, separate from the HID cores.
- `Test/`: host-side tools for observing and testing generated HID input.

## Build

Firmware projects are built from this folder. `--project` is the project folder, e.g. `Source/XIAOESP32S3Core` or `Examples/WebController`.

```bash
# Compile and upload to the first connected board. Add --clean for a full rebuild.
./BuildAndDeploy.sh --project Examples/WebController

# Only check that a project compiles, without keeping any output.
./Compile.sh --project Examples/WebController
```

The sketch is the project folder itself or a subfolder named after its core (e.g. `Examples/WebController/XIAOESP32S3Core`), which is then built with that core from `Source/` as library. A `www/` folder in the project is embedded into the firmware.

The XIAO ESP32S3 core shares profile definitions between USB and Bluetooth LE, with per-instance routing and independent recovery. [WebController](Examples/WebController) provides USB, Bluetooth, and mixed configurations, including a USB mouse and a second BLE mouse on one board.

The HTTP controller API requires the 0–99 pairing code shown on the OLED or USB serial console. It is easy to guess, so only use it on a trusted local network. Browser controls pair once per board restart and release automatically if their session is lost. See the [core API](Source/XIAOESP32S3Core/README.md#usage) for authenticated requests and custom HID profiles.

The Arduino examples keep their CLI package and library cache out of git. Running the setup commands in a core README recreates the ignored local state under `.arduino/`, `data/`, `downloads/`, or `user/` as needed.

Contributions, experiments, and new cores/modules are welcome.
