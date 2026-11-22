# OpenCtrl / Examples / WebController

Phone friendly web views for every HID input of OpenCtrl. The [firmware](./XIAOESP32S3Core) embeds the pages of [www/](./www) and serves them at `http://<device ip>/`.

## USB, Bluetooth, and Mixed Modes

Select `OPENCTRL_WEB_MODE` in [Config.hpp](XIAOESP32S3Core/Config.hpp), then rebuild. USB (1) is the default. Mode selection applies when firmware starts; it is not a live browser switch.

| Mode | Value | USB HID | Bluetooth LE HID |
| --- | --- | --- | --- |
| USB | 1 | Keyboard, mouse, gamepad, media, system, pen | Off |
| Bluetooth | 2 | None | Keyboard, mouse, gamepad, media, system, pen |
| Mixed | 3 | Keyboard, primary mouse, media, system, pen | Gamepad and second mouse |

All modes use WiFi for the browser connection. In Bluetooth and mixed modes, pair **OpenCtrl** with the target host in its Bluetooth settings. Web controls connect automatically without a pairing code. Use a trusted local network, since anyone who can reach the device can operate it. USB supplies power in Bluetooth-only mode. HID transport does not change the web controls or their JSON payloads. The main page shows the firmware's mode.

Install the [core dependencies and WiFi settings](../../Source/XIAOESP32S3Core/README.md#setup), then build from the repository root:

```bash
./Compile.sh --project Examples/WebController
./BuildAndDeploy.sh --project Examples/WebController
```

Switching profiles/transports requires a restart and USB re-enumeration. Remove cached Bluetooth pairing and pair again when the host retains an old descriptor. Bluetooth uses one peripheral identity and supports one connected Bluetooth host; different USB and Bluetooth hosts can connect simultaneously.

### Mixed Mode and Two Mice

The usual `/mouse` controls the USB mouse. The browser displays a **Bluetooth Mouse** link in mixed mode; it opens `mouse.html?mouse=secondary` and sends the same controls to `/mouse/secondary`. Returning to **Primary mouse** selects USB again. Page changes release the previous browser session; each page controls one mouse instance. The gamepad views automatically target Bluetooth in this mode. A disconnected Bluetooth host does not prevent USB input, and reconnect starts from neutral.

| Profile | Report ID | HTTP input | Mixed-mode connection |
| --- | --- | --- | --- |
| Keyboard | 1 | `/keyboard` | USB |
| Primary mouse | 2 | `/mouse` | USB |
| Gamepad | 3 | `/gamepad` | Bluetooth |
| Media keys | 4 | `/consumer` | USB |
| System control | 5 | `/system` | USB |
| Pen | 7 | `/pen` | USB |
| Second mouse | 8 | `/mouse/secondary` or `/hid/report` | Bluetooth |

Both mouse instances compose the same core profile definition with separate state. Local code can address the second with `HIDUtil::SendMouse(dx, dy, wheel, pan, buttons, 8)`. A raw API request to move it right by 10 is `{"id":8,"data":[0,10,0,0,0]}` at `/hid/report`; the bytes are buttons, X, Y, wheel, and pan. Send five zero bytes to release its buttons. All input endpoints retain the core's session ownership and heartbeat requirements.

USB and Bluetooth can connect to the same computer or different computers. Desktop systems usually combine mice into one cursor; applications with device-specific input can distinguish them. Two mice do not automatically create two cursors. Mirroring one profile through both transports remains available through the core's `HIDTransport::Both` configuration; it is distinct from these independent mouse instances.

`Config.hpp` owns preset routing, `Profiles.hpp` composes core profile values and registers them, and the sketch owns web routes. Standard keyboard registration retains USB BIOS boot protocol. Public GET `/controller` supplies mode and mouse availability metadata without requiring a pairing code.

## Views

- [index.html](./www/index.html): all inputs on one page (keyboard, mouse, gamepad, media keys, system control) and links to the views below.
- [joystick.html](./www/joystick.html): one large stick with four buttons.
- [slider.html](./www/slider.html): six faders, one per gamepad axis, which stay where you leave them.
- [keyboard.html](./www/keyboard.html): full US keyboard with sticky modifiers.
- [mouse.html](./www/mouse.html): touchpad with scroll strip and buttons.
- [pencil.html](./www/pencil.html): drawing surface sending absolute pen reports with pressure.
- [drive.html](./www/drive.html): steering wheel with paddle shifters, brake, and gas pedal.
- [fly.html](./www/fly.html): flight stick, throttle, rudder, hat, and buttons.
- [xbox.html](./www/xbox.html) and [ps5.html](./www/ps5.html): controller layouts. Double-tap a stick to click it.

Open any controller view to connect automatically. Only one browser session controls HID at a time. Keys are held while pressed; modifiers apply to the next key. Media/system buttons send a press and release. Leaving the page releases all controls, with a firmware timeout as backup. Reconnection starts at neutral and does not replay old input.

All views fit an iPhone in portrait and landscape, the controller views are made for landscape. Pages never zoom, pull to refresh, or show long-press menus, so several controls can be used at once (e.g. steering and gas). The full screen views do not scroll at all. Content stays clear of notches, camera cutouts, rounded corners, and the home indicator, while the header extends under the status bar.

### Home Screen App

Every page links [manifest.webmanifest](./www/manifest.webmanifest), so **Add to Home Screen** turns any view into an app icon which opens that view without browser bars. Add each view you use separately. A home screen app keeps its own storage, so pair it once through its **← OpenCtrl** link. The icons are rendered from [icon.svg](./www/icon.svg):

```bash
for s in 192 512; do magick -density 384 icon.svg -resize ${s}x${s} -alpha off -depth 8 -strip PNG24:icon-$s.png; done
```

The board serves plain HTTP, so there is no service worker or offline cache, which the controls could not use without the board anyway. iOS opens home screen views without browser bars. Chrome only installs apps from HTTPS origins, so on Android it adds a shortcut which may open in a regular browser tab.

## Adding a View

Every file in `www/` is embedded, so a new view only needs an HTML file whose head matches the existing views (viewport, status bar, manifest, and icons) and includes [openctrl.css](./www/openctrl.css) and [openctrl.js](./www/openctrl.js). Controls are declared with data attributes, for example `<div data-stick="x y"></div>` or `<button data-button=0>A</button>`. The list of attributes is at the top of [openctrl.js](./www/openctrl.js).

To try changes without flashing, open a page from disk with the device IP, e.g. `www/index.html?device=192.168.1.171`. The IP is remembered for the other pages.
