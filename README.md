# [OpenCtrl](https://github.com/hrkltz/OpenCtrl)

OpenCtrl is an open-source playground for building custom user interfaces and control surfaces which are fast to prototype, easy to extend, and friendly to tinkerers. The goal is a small ecosystem of reusable modules and examples that help you go from idea to working input in minutes. 

## Structure

- `Source/`: reusable source modules. A source module owns the common input/output implementation for a platform, for example the XIAO ESP32S3 HID web API firmware.
- `Examples/`: concrete examples built on top of the cores, such as curl scripts, a browser driving page, or the steering wheel prototype.
- `Test/`: host-side tools for observing and testing generated HID input.

The Arduino examples keep their CLI package and library cache out of git. Running the setup commands in a core README recreates the ignored local state under `.arduino/`, `data/`, `downloads/`, or `user/` as needed.

Contributions, experiments, and new cores/modules are welcome.
