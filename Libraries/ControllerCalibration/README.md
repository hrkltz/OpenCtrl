# OpenCtrl / Libraries / ControllerCalibration

Settings API and persistent storage for local hardware controllers: live readings, axis calibration, optional input filtering, and optional input-to-HID bindings. [M5StackController](../../Examples/M5StackController) composes it with `M5StackInputs` and the HID core, and serves its own settings page. The hardware library stays independent of networking and storage; the HID core requires no calibration-specific changes.

The service includes only the hardware library's value utilities (`InputUtils.hpp`), without depending on its GPIO, I2C, or unit classes. Bind Joystick2 `x`/`y` directly, or `&fader.Calibration()` / `&angle.Calibration()` with millivolt bounds, and supply the corresponding raw values from the sensor loop. Relative encoder deltas first need a controller-owned bounded position; calibrating them as absolute potentiometer values would lose that distinction.

## Composition

1. `Configure(namespace, axes, filter, mapping)`: up to 32 stable axis IDs, labels, pointers to `AxisCalibration`, and valid raw bounds (negative bounds suit positions counted from a start). Configuration snapshots the factory defaults. Optionally pass a stable `ControllerFilterSettings*` and a `ControllerMapping`.
2. `Load()` before `Begin()`. It applies a complete valid calibration record and returns whether it did. Filter and binding records load independently, even without saved calibration. Missing or invalid records keep the configured defaults.
3. Start WiFi, add the page and any controller routes to `Server()`, then call `Begin(title)` and show `Address()` to the user.
4. In the sensor loop, call `Service()` and publish readings with `UpdateInputs(readings, count, diagnostics)` (or `UpdateRaw(index, value, online)` for one axis). Send neutral HID while `Active()` is true; otherwise map physical input normally.

Only `Service()` applies settings and writes flash, so new calibration, filters, and bindings take effect between two cycles of the input loop. HTTP state requests allocate their buffers before locking, copy settings and readings under a short lock, and build JSON after releasing it. `UpdateInputs()` accepts exactly one `CalibrationRawReading` per configured axis and can include diagnostics in the same publication; null or incomplete batches are ignored. Set `sampled` false when an input was deliberately skipped; GET state then reports `sampled:false,online:false`. A failed read is `sampled:true,online:false`. Axis bindings, the filter settings, and the binding table must stay alive for the service's lifetime, and the input loop owns access to them.

## Bindings

A `ControllerMapping` lists the controller's sources (`ControllerSource`: id, label, and an `Axis`, `Button`, or `Relative` signal), targets (`ControllerTarget`: id, label, `Axis`, `Button`, or `Motion` output), a pointer to the active `std::vector<ControllerBinding>`, and the maximum table size. A binding is a source index, a target index, and a mode:

| Mode | Meaning |
| --- | --- |
| `normal`, `reversed` | The whole axis, as is or reversed. |
| `positive`, `negative` | One half of the axis: e.g. a pedal mapped to a full output range, or a button pressed past half travel. |

`ControllerBindingValid(signal, output, mode)` defines which combinations are allowed: axes drive axes in every mode, motion in `normal`/`reversed`, and buttons in `positive`/`negative`; buttons drive only buttons, in `normal`. Relative sources (e.g. an encoder's turn) follow the axis rules; the controller decides what a position, a movement, or a step means. Configuration, HTTP requests, and stored records are all validated with it. The library stores and validates the table; the consuming controller converts readings into HID state.

Optionally, the mapping also holds **ports**: `units` lists the unit types a port can hold (`ControllerUnit`: id, label; index 0 is usually "nothing"), `ports` points to a `std::vector<uint8_t>` with one unit index per port, and every source names its `port` and the `units` bitmask that provide it. A binding then must also fit the unit on its source's port. Units and bindings are saved, loaded, and applied together, so a port's unit and its bindings always match.

## API

GET `/api/calibration` returns settings, live readings, capture state, storage status, and the last completed operation. Mutation routes are POST `/api/calibration/start`, `/capture`, `/stop-capture`, `/preview`, `/save`, `/reset`, and `/end`. Preview and Save accept `{version:1,axes:[{id,minimum,center,maximum,deadzone,inverted},...]}` with every configured axis; other commands need no payload. Each axis needs `minimum < center < maximum` within its raw bounds, a finite deadzone from 0 inclusive to 1 exclusive, and a boolean inversion.

Starting setup sets `Active()`; the consuming controller pauses HID. Capture records extrema at the sensor loop's rate, so brief endpoint visits between browser polls are kept. Preview changes RAM only; Save and Reset write one versioned record. End discards changes since the last successful save. A session without state requests expires after 30 seconds and behaves like End.

With filter settings configured, GET state includes `filter: {mode,alpha,saved}`. POST `/api/calibration/filter` accepts `{version:1,mode:"off"|"median-3"|"ema",alpha:0.05..1}`, also outside setup. Defaults are Median-3 and alpha 0.35. Filtering itself belongs to the consuming controller.

With a mapping configured, GET state includes `mapping: {saved,ports,bindings:[{source,target,mode},...]}` (`ports` lists unit IDs when ports are configured), and GET `/api/calibration/options` returns the static `{maximum,units:[{id,label}],sources:[{id,label,signal,port,units}],targets:[{id,label,output}]}` once instead of with every poll. POST `/api/calibration/mapping` accepts `{version:1,ports:[unitId,...],bindings:[{source,target,mode},...]}` with up to `maximum` bindings and, when ports are configured, one known unit per port. Unknown IDs, invalid combinations, sources their port's unit does not provide, wrong types, and oversized tables return 400. Bindings may also change during setup. A successful write replaces the whole table.

Controllers can publish `ControllerInputDiagnostics` with `UpdateInputs()` or `UpdateDiagnostics()`: cumulative `readFailures`, `recoveredReads`, `inputLossCycles`, and `sendFailures`, plus optional `timing: {cycleCount,lastWorkMicroseconds,maxWorkMicroseconds,overrunCycles,pollMicroseconds}` when `pollMicroseconds` is nonzero. GET state then includes an optional `diagnostics` object. Counters stay in RAM and do not change any settings.

Accepted commands return 202 with an operation ID. Poll the state until `completedOperation` matches; `operationStatus` and `message` identify the outcome. Malformed settings return 400, missing mutation headers 403, and inactive or conflicting commands 409. Storage failures complete with 503 and keep the previous settings. One command can be outstanding.

Every POST requires `X-OpenCtrl-Calibration: 1`. No CORS permissions are granted, so a third-party browser page cannot supply this header without a denied preflight. There is no pairing code: anyone who can reach the controller can change its settings, so use a trusted local network.

Records use the keys `settings`, `filter`, and `mapping` in the configured Preferences namespace. The library compiles with the bundled ESP32 WiFi/Preferences libraries and the core's ArduinoJson/ESP Async WebServer dependencies.
