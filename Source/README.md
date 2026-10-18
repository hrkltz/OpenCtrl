# OpenCtrl / Source

Reusable source modules live here. A source module should provide the shared runtime for a platform or transport and avoid depending on one specific example.

## Modules

- `XIAOESP32S3Core`: XIAO ESP32S3 firmware exposing composable standard and custom HID profiles over USB and Bluetooth LE, controlled through an authenticated web API. Also used as library by the XIAO ESP32S3 examples.
- `SoftCore`: placeholder for host/software-only controller logic.
