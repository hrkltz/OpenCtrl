# OpenCtrl / Examples / Steering001Controller / XIAOESP32S3Core

This is the first version of a controller styled steering wheel controller to be used on the couche. As a first prototype the physical parts are done with LEGO Technic parts and some custom 3D prints for mounting the electronic components.

## Part List

- 1x [XIAO ESP32S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- 1x USB-C to USB-C or USB-A cable
- 1x [Seeed Studio XIAO Expansion Board](https://www.seeedstudio.com/Seeeduino-XIAO-Expansion-board-p-4746.html) (optional)
- 1x [SparkFun Linear 3D Hall-Effect Sensor - TMAG5273 (Qwiic)](https://www.sparkfun.com/sparkfun-linear-3d-hall-effect-sensor-tmag5273-qwiic.html)

## Setup

This version is based on the general XIA ESP32-S3 implementation. It just requires an additional lib for handling the 3D Hall effect sensor.

```bash
arduino-cli --config-file arduino-cli.yaml lib install "SparkFun TMAG5273 Arduino Library@2.0.0"
```
