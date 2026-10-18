// OpenCtrl shared web view logic. Controls are bound by data attributes once the page has loaded:
//
//   data-status                Shows the connection status.
//   data-key=<usage>           Keyboard key held while pressed, combined with active modifiers.
//   data-modifier=<bit>        Keyboard modifier toggle (1 Ctrl, 2 Shift, 4 Alt, 8 GUI), cleared after the next key.
//   data-type=<input id>       Types the text of that input element (US layout on the host).
//   data-touchpad              Relative touchpad: drag moves, a short tap clicks.
//   data-scrollpad             Drag up or down to turn the mouse wheel.
//   data-mouse=<bit>           Mouse button click (1 left, 2 right, 4 middle).
//   data-mouse-hold=<bit>      Mouse button toggle, e.g. to drag.
//   data-wheel=<steps>         Mouse wheel steps, positive scrolls up.
//   data-button=<index>        Gamepad button (0 to 31), held while pressed.
//   data-hat=<value>           Gamepad hat direction (1 up, clockwise to 8 up-left), held while pressed.
//   data-hatpad                Fills the element with all eight hat directions.
//   data-stick="<x> <y>"       Gamepad stick for two axes, springs back to center.
//                              data-stick-button=<index> clicks the stick (e.g. L3) on double-tap and hold.
//   data-fader=<axis>          Gamepad axis by touch position (bottom/left -127, top/right 127). Optional:
//                              data-horizontal, data-value=<start value>, data-spring=<value to return to on release>.
//   data-steering=<axis>       Steering wheel, rotates up to 135 degrees each way, springs back to center.
//   data-consumer=<usage>      Media key tap, e.g. 0xCD play/pause.
//   data-system=<usage>        System control tap (1 power off, 2 standby, 3 wake). data-confirm=<question> asks first.
//   data-pen                   Pen surface, mapped to the whole host screen.
//   data-pen-toggle=<flag>     Toggles the pen "barrel" button or "eraser".
//
// Gamepad axes: x, y (left stick), z, rz (left and right trigger), rx, ry (right stick).

// Pages served by the device talk to it directly. Pages opened from disk (for development) use ?device=<ip>, which is remembered.
const deviceUrl = (() => {
    if (location.protocol !== 'file:') return '';
    let device = new URLSearchParams(location.search).get('device');
    try {
        if (device) localStorage.setItem('openctrl-device', device);
        else device = localStorage.getItem('openctrl-device');
    } catch (error) {
        // Storage is unavailable, so the device has to be passed every time.
    }
    return device ? `http://${device}` : '';
})();

// Each page controls one mouse instance; navigation releases the previous session.
const secondaryMouse = new URLSearchParams(location.search).get('mouse') === 'secondary';
const mousePath = secondaryMouse ? 'mouse/secondary' : 'mouse';

async function loadControllerInfo() {
    try {
        const response = await fetch(`${deviceUrl}/controller`, { cache: 'no-store' });
        if (!response.ok) return;
        const info = await response.json();
        document.querySelectorAll('[data-connection-mode]').forEach((element) => element.textContent = info.mode);
        document.querySelectorAll('[data-secondary-mouse]').forEach((element) => element.hidden = info.secondaryMouse !== true || secondaryMouse);
        document.querySelectorAll('[data-primary-mouse]').forEach((element) => element.hidden = !secondaryMouse);
        document.querySelectorAll('[data-mouse-name]').forEach((element) =>
            element.textContent = secondaryMouse ? 'Bluetooth mouse' : info.primaryMouse);
    } catch (error) { /* Pairing/heartbeat handles network errors. */ }
}

function setStatus(text) {
    document.querySelectorAll('[data-status]').forEach((element) => element.textContent = text);
}

const tokenKey = `openctrl-token:${deviceUrl || location.origin}`;
let pairingToken = '';
try { pairingToken = localStorage.getItem(tokenKey) || ''; } catch (error) { /* Optional storage. */ }
function newSession() {
    return Array.from(crypto.getRandomValues(new Uint8Array(16)), (byte) => byte.toString(16).padStart(2, '0')).join('');
}
let sessionId = newSession();
let sessionActive = false;
let generation = 0;
let heartbeatBusy = false;
let sending = false;
const reports = [];

async function request(path, body, session = sessionId, keepalive = false) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 1000);
    try {
        return await fetch(`${deviceUrl}/${path}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${pairingToken}`,
                'X-OpenCtrl-Session': session },
            body: JSON.stringify(body), signal: controller.signal, keepalive,
        });
    } finally {
        clearTimeout(timeout);
    }
}

function stopSession(message, release = true) {
    const previousSession = sessionId;
    const wasActive = sessionActive;
    sessionActive = false;
    ++generation;
    sessionId = newSession();
    for (const report of reports.splice(0)) report.resolve(false);
    resetControls();
    setStatus(message);
    if (release && wasActive) request('release', {}, previousSession, true).catch(() => {});
}

async function pumpReports() {
    if (sending) return;
    sending = true;
    try {
        while (reports.length && sessionActive) {
            const report = reports.shift();
            if (Date.now() - report.createdAt > 1000) {
                report.resolve(false);
                stopSession('Connection too slow — reconnect');
                break;
            }
            const version = generation;
            try {
                const response = await request(report.path, report.body);
                if (version !== generation) { report.resolve(false); continue; }
                if (!response.ok) {
                    report.resolve(false);
                    stopSession(response.status === 401 ? 'Pair with controller' :
                        response.status === 409 ? 'Controller is in use' : `Report failed (${response.status})`);
                    break;
                }
                setStatus('Connected');
                report.resolve(true);
            } catch (error) {
                report.resolve(false);
                if (version === generation) stopSession('Offline — controls released');
            }
        }
    } finally {
        sending = false;
    }
}

// Preserve discrete transitions; only replace adjacent continuous states with
// identical buttons/contact flags. Every queued body is an immutable snapshot.
function post(path, body, transition = null) {
    if (!sessionActive) { setStatus('Pair with controller'); return Promise.resolve(false); }
    const snapshot = JSON.parse(JSON.stringify(body));
    const previous = reports[reports.length - 1];
    if (transition !== null && previous?.path === path && previous.transition === transition) {
        previous.body = snapshot;
        return previous.promise;
    }
    if (reports.length >= 128) {
        stopSession('Connection too slow — reconnect');
        return Promise.resolve(false);
    }
    let resolve;
    const promise = new Promise((done) => resolve = done);
    reports.push({ path, body: snapshot, transition, resolve, promise, createdAt: Date.now() });
    pumpReports();
    return promise;
}

function createSender(path, state) {
    const flags = path === 'gamepad' ? ['buttons', 'hat'] : ['tip', 'inRange', 'barrel', 'eraser'];
    return () => post(path, state, flags.map((key) => state[key]).join(':'));
}

async function heartbeat() {
    if (heartbeatBusy || !pairingToken || document.hidden) return;
    heartbeatBusy = true;
    const version = generation;
    try {
        const response = await request('heartbeat', {});
        if (version !== generation) return;
        if (response.ok) {
            if (!sessionActive) resetControls();
            sessionActive = true;
            setStatus('Connected');
        } else {
            stopSession(response.status === 401 ? 'Pair with controller' :
                response.status === 409 ? 'Controller is in use' : 'Controller unavailable');
        }
    } catch (error) {
        if (version === generation) stopSession('Offline — controls released');
    } finally {
        heartbeatBusy = false;
    }
}

function addPairingControls() {
    const form = document.createElement('form');
    form.className = 'pairing';
    form.innerHTML = '<span data-connection-mode></span><label>Pair with controller <input type=password inputmode=numeric autocomplete=off maxlength=2 placeholder="Code 0–99 from display or USB console" aria-label="Pairing code" required pattern="[0-9]{1,2}"></label> <button>Pair</button>';
    form.addEventListener('submit', (event) => {
        event.preventDefault();
        stopSession('Pairing…');
        pairingToken = String(Number(form.querySelector('input').value.trim()));
        try { localStorage.setItem(tokenKey, pairingToken); } catch (error) { /* Optional storage. */ }
        heartbeat();
    });
    document.querySelector('header').after(form);
    heartbeat();
    setInterval(heartbeat, 500);
}

document.addEventListener('visibilitychange', () => {
    if (document.hidden) stopSession('Paused — controls released');
    else heartbeat();
});
window.addEventListener('pagehide', () => stopSession('Disconnected'));
window.addEventListener('blur', () => stopSession('Paused — controls released'));
window.addEventListener('focus', heartbeat);

function clamp(value, min = -127, max = 127) {
    return Math.max(min, Math.min(max, value));
}

// Calls down(event) on press and up() on release, for touch, pen, and mouse.
function onHold(element, down, up) {
    let pointer = null;
    element.addEventListener('pointerdown', (event) => {
        if (pointer !== null || !sessionActive) return;
        pointer = event.pointerId;
        element.setPointerCapture(pointer);
        element.classList.add('on');
        down(event);
    });
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
        element.addEventListener(type, (event) => {
            if (pointer !== event.pointerId) return;
            pointer = null;
            element.classList.remove('on');
            up();
        });
    }
    element.addEventListener('openctrl-reset', () => pointer = null);
}

function onDrag(element, move, release) {
    let pointer = null;
    element.addEventListener('pointerdown', (event) => {
        if (pointer !== null || !sessionActive) return;
        pointer = event.pointerId;
        element.setPointerCapture(pointer);
        move(event);
    });
    element.addEventListener('pointermove', (event) => {
        if (pointer === event.pointerId) move(event);
    });
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
        element.addEventListener(type, (event) => {
            if (pointer !== event.pointerId) return;
            pointer = null;
            release();
        });
    }
    element.addEventListener('openctrl-reset', () => { pointer = null; release(); });
}

// --- Keyboard ---
let keyModifiers = 0;
const heldKeys = new Map();

function takeModifiers() {
    const taken = keyModifiers;
    keyModifiers = 0;
    document.querySelectorAll('[data-modifier]').forEach((button) => button.classList.remove('on'));
    return taken;
}

function keyboardState(extraKey, extraModifiers = 0) {
    const keys = Array.from(heldKeys.keys());
    if (extraKey !== undefined && !keys.includes(extraKey)) keys.push(extraKey);
    let modifiers = extraModifiers;
    for (const value of heldKeys.values()) modifiers |= value;
    return { modifiers, keys };
}

function bindKey(button) {
    const key = Number(button.dataset.key);
    onHold(button, () => {
        if (heldKeys.size >= 6 && !heldKeys.has(key)) { setStatus('Six keys maximum'); return; }
        heldKeys.set(key, takeModifiers());
        post('keyboard', keyboardState());
    }, () => {
        heldKeys.delete(key);
        post('keyboard', keyboardState());
    });
}

async function tapKey(key, modifiers = takeModifiers()) {
    const press = keyboardState(key, modifiers);
    if (press.keys.length > 6) { setStatus('Six keys maximum'); return false; }
    const results = await Promise.all([post('keyboard', press), post('keyboard', keyboardState())]);
    return results.every(Boolean);
}

// US layout: characters on the same key as their unshifted counterpart share the usage.
const plainKeys = { ' ': 44, '\n': 40, '\t': 43, '-': 45, '=': 46, '[': 47, ']': 48, '\\': 49, ';': 51, "'": 52, '`': 53, ',': 54, '.': 55, '/': 56 };
const shiftedKeys = { '!': 30, '@': 31, '#': 32, '$': 33, '%': 34, '^': 35, '&': 36, '*': 37, '(': 38, ')': 39, '_': 45, '+': 46, '{': 47, '}': 48, '|': 49, ':': 51, '"': 52, '~': 53, '<': 54, '>': 55, '?': 56 };

function charToKey(char) {
    const code = char.charCodeAt(0);
    if (char >= 'a' && char <= 'z') return { key: 4 + code - 97, shift: false };
    if (char >= 'A' && char <= 'Z') return { key: 4 + code - 65, shift: true };
    if (char >= '1' && char <= '9') return { key: 30 + code - 49, shift: false };
    if (char === '0') return { key: 39, shift: false };
    if (char in plainKeys) return { key: plainKeys[char], shift: false };
    if (char in shiftedKeys) return { key: shiftedKeys[char], shift: true };
    return null;
}

async function typeText(text) {
    const modifiers = takeModifiers();
    for (const char of text) {
        const mapped = charToKey(char);
        if (mapped && !await tapKey(mapped.key, modifiers | (mapped.shift ? 2 : 0))) break;
    }
}

// --- Mouse ---
const mouse = { buttons: 0, moveX: 0, moveY: 0 };
const mouseSpeed = 1.5;

// Movement and button changes share the same ordered request queue.
function sendMouseMove() {
    const dx = clamp(Math.round(mouse.moveX), -4096, 4096);
    const dy = clamp(Math.round(mouse.moveY), -4096, 4096);
    if (!dx && !dy) return;
    mouse.moveX -= dx;
    mouse.moveY -= dy;
    post(mousePath, { dx, dy, buttons: mouse.buttons });
}

function clickMouse(button) {
    return Promise.all([
        post(mousePath, { buttons: mouse.buttons | button }),
        post(mousePath, { buttons: mouse.buttons }),
    ]);
}

function bindTouchpad(touchpad) {
    let last = null;
    let downAt = 0;
    let travel = 0;
    touchpad.addEventListener('pointerdown', (event) => {
        if (!sessionActive || last) return;
        touchpad.setPointerCapture(event.pointerId);
        last = { x: event.clientX, y: event.clientY, id: event.pointerId };
        downAt = Date.now();
        travel = 0;
    });
    touchpad.addEventListener('pointermove', (event) => {
        if (!last || last.id !== event.pointerId) return;
        const dx = event.clientX - last.x;
        const dy = event.clientY - last.y;
        last = { x: event.clientX, y: event.clientY, id: event.pointerId };
        travel += Math.abs(dx) + Math.abs(dy);
        mouse.moveX += dx * mouseSpeed;
        mouse.moveY += dy * mouseSpeed;
        sendMouseMove();
    });
    touchpad.addEventListener('pointerup', (event) => {
        if (!last || last.id !== event.pointerId) return;
        // A short tap without movement is a left click.
        if (last && travel < 5 && Date.now() - downAt < 250) clickMouse(1);
        last = null;
    });
    for (const type of ['pointercancel', 'lostpointercapture', 'openctrl-reset']) {
        touchpad.addEventListener(type, () => last = null);
    }
}

function bindScrollpad(scrollpad) {
    const stepSize = 20;
    let lastY = null;
    let rest = 0;
    onDrag(scrollpad, (event) => {
        if (lastY !== null) rest += lastY - event.clientY;
        lastY = event.clientY;
        const steps = Math.trunc(rest / stepSize);
        if (!steps) return;
        rest -= steps * stepSize;
        post(mousePath, { wheel: clamp(steps), buttons: mouse.buttons });
    }, () => {
        lastY = null;
        rest = 0;
    });
}

// --- Gamepad ---
const gamepad = { x: 0, y: 0, z: -127, rz: -127, rx: 0, ry: 0, hat: 0, buttons: 0 };
const sendGamepad = createSender('gamepad', gamepad);

function setGamepad(values) {
    Object.assign(gamepad, values);
    sendGamepad();
}

function bindGamepadButton(button) {
    const bit = 1 << Number(button.dataset.button);
    onHold(button, () => setGamepad({ buttons: (gamepad.buttons | bit) >>> 0 }),
        () => setGamepad({ buttons: (gamepad.buttons & ~bit) >>> 0 }));
}

function bindHat(button) {
    const value = Number(button.dataset.hat);
    onHold(button, () => setGamepad({ hat: value }), () => {
        if (gamepad.hat === value) setGamepad({ hat: 0 });
    });
}

function fillHatpad(hatpad) {
    const layout = [[8, '&nwarr;'], [1, '&uarr;'], [2, '&nearr;'], [7, '&larr;'], [0, ''], [3, '&rarr;'], [6, '&swarr;'], [5, '&darr;'], [4, '&searr;']];
    for (const [value, label] of layout) {
        const element = document.createElement(value ? 'button' : 'span');
        element.innerHTML = label;
        if (value) element.dataset.hat = value;
        hatpad.append(element);
    }
}

function bindStick(stick) {
    const [xAxis, yAxis] = stick.dataset.stick.split(' ');
    const clickBit = 'stickButton' in stick.dataset ? 1 << Number(stick.dataset.stickButton) : 0;
    let releasedAt = 0;
    const knob = document.createElement('div');
    knob.className = 'knob';
    stick.append(knob);
    const setStick = (normalizedX, normalizedY) => {
        const travel = (stick.offsetWidth - knob.offsetWidth) / 2;
        knob.style.transform = `translate(${normalizedX * travel}px, ${normalizedY * travel}px)`;
        setGamepad({ [xAxis]: Math.round(normalizedX * 127), [yAxis]: Math.round(normalizedY * 127) });
    };
    // A double-tap held down clicks the stick. Registered first, so the click goes out with the first move.
    stick.addEventListener('pointerdown', () => {
        if (!sessionActive || !clickBit || Date.now() - releasedAt > 300) return;
        stick.classList.add('on');
        gamepad.buttons = (gamepad.buttons | clickBit) >>> 0;
    });
    onDrag(stick, (event) => {
        const rect = stick.getBoundingClientRect();
        let normalizedX = (event.clientX - rect.left - rect.width / 2) / (rect.width / 2);
        let normalizedY = (event.clientY - rect.top - rect.height / 2) / (rect.height / 2);
        const length = Math.hypot(normalizedX, normalizedY);
        if (length > 1) {
            normalizedX /= length;
            normalizedY /= length;
        }
        setStick(normalizedX, normalizedY);
    }, () => {
        releasedAt = Date.now();
        stick.classList.remove('on');
        gamepad.buttons = (gamepad.buttons & ~clickBit) >>> 0;
        setStick(0, 0);
    });
}

function bindFader(fader) {
    const axis = fader.dataset.fader;
    const horizontal = 'horizontal' in fader.dataset;
    const spring = fader.dataset.spring;
    const setValue = (value) => {
        fader.style.setProperty('--amount', (value + 127) / 254);
        setGamepad({ [axis]: value });
    };
    // Show and use the start value without sending it.
    gamepad[axis] = Number(fader.dataset.value ?? spring ?? 0);
    fader.style.setProperty('--amount', (gamepad[axis] + 127) / 254);
    onDrag(fader, (event) => {
        const rect = fader.getBoundingClientRect();
        const amount = horizontal ? (event.clientX - rect.left) / rect.width : (rect.bottom - event.clientY) / rect.height;
        setValue(Math.round(clamp(amount, 0, 1) * 254 - 127));
    }, () => {
        if (spring !== undefined) setValue(Number(spring));
    });
}

function bindSteering(wheel) {
    const axis = wheel.dataset.steering;
    const maxAngle = 135;
    let rotation = 0;
    let lastAngle = null;
    const angleOf = (event) => {
        const rect = wheel.getBoundingClientRect();
        return Math.atan2(event.clientY - rect.top - rect.height / 2, event.clientX - rect.left - rect.width / 2) * 180 / Math.PI;
    };
    const setRotation = (value) => {
        rotation = clamp(value, -maxAngle, maxAngle);
        wheel.style.transform = `rotate(${rotation}deg)`;
        setGamepad({ [axis]: Math.round(rotation / maxAngle * 127) });
    };
    onDrag(wheel, (event) => {
        const angle = angleOf(event);
        if (lastAngle !== null) {
            // Normalize to -180..180 so crossing the left side does not jump.
            setRotation(rotation + ((angle - lastAngle + 540) % 360) - 180);
        }
        lastAngle = angle;
    }, () => {
        lastAngle = null;
        setRotation(0);
    });
}

// --- Pen ---
const pen = { tip: false, inRange: false, barrel: false, eraser: false, x: 0, y: 0, pressure: 0 };
const penToggles = { barrel: false, eraser: false };
const sendPen = createSender('pen', pen);

function bindPen(surface) {
    const update = (event, inRange) => {
        if (!sessionActive) return;
        const rect = surface.getBoundingClientRect();
        const tip = inRange && (event.buttons & 1) !== 0;
        Object.assign(pen, {
            tip,
            inRange,
            barrel: penToggles.barrel || (event.buttons & 2) !== 0,
            eraser: penToggles.eraser || (event.buttons & 32) !== 0,
            x: Math.round(clamp((event.clientX - rect.left) / rect.width, 0, 1) * 32767),
            y: Math.round(clamp((event.clientY - rect.top) / rect.height, 0, 1) * 32767),
            // Devices without pressure report 0.5 while pressed, some report 0.
            pressure: tip ? Math.round((event.pressure || 0.5) * 1023) : 0,
        });
        sendPen();
    };
    surface.addEventListener('pointerdown', (event) => {
        if (!sessionActive) return;
        surface.setPointerCapture(event.pointerId);
        update(event, true);
    });
    surface.addEventListener('pointermove', (event) => update(event, true));
    // A finger leaves the range when lifted, a pen or mouse keeps hovering.
    surface.addEventListener('pointerup', (event) => update(event, event.pointerType !== 'touch'));
    for (const type of ['pointerleave', 'pointercancel']) {
        surface.addEventListener(type, (event) => update(event, false));
    }
    surface.addEventListener('lostpointercapture', (event) => {
        if (pen.tip) update(event, false);
    });
}

function resetControls() {
    keyModifiers = 0;
    heldKeys.clear();
    Object.assign(mouse, { buttons: 0, moveX: 0, moveY: 0 });
    Object.assign(gamepad, { x: 0, y: 0, z: -127, rz: -127, rx: 0, ry: 0, hat: 0, buttons: 0 });
    Object.assign(pen, { tip: false, inRange: false, barrel: false, eraser: false, pressure: 0 });
    Object.assign(penToggles, { barrel: false, eraser: false });
    document.querySelectorAll('.on').forEach((element) => element.classList.remove('on'));
    document.querySelectorAll('[data-stick], [data-fader], [data-steering], [data-key], [data-button], [data-hat], [data-touchpad]')
        .forEach((element) => element.dispatchEvent(new Event('openctrl-reset')));
    document.querySelectorAll('[data-fader]').forEach((element) =>
        element.style.setProperty('--amount', (gamepad[element.dataset.fader] + 127) / 254));
}

// --- Binding ---
document.addEventListener('contextmenu', (event) => {
    // Prevent long-press menus on controls.
    if (event.target.closest('button, [data-touchpad], [data-scrollpad], [data-pen], [data-stick], [data-fader], [data-steering]')) event.preventDefault();
});

document.addEventListener('DOMContentLoaded', () => {
    const each = (selector, bind) => document.querySelectorAll(selector).forEach(bind);
    const onClick = (selector, handler) => each(selector, (element) => element.addEventListener('click', () => {
        if (sessionActive) handler(element);
    }));

    addPairingControls();
    loadControllerInfo();
    each('[data-hatpad]', fillHatpad);

    onClick('[data-modifier]', (button) => {
        keyModifiers ^= Number(button.dataset.modifier);
        button.classList.toggle('on', (keyModifiers & Number(button.dataset.modifier)) !== 0);
    });
    each('[data-key]', bindKey);
    onClick('[data-type]', (button) => typeText(document.getElementById(button.dataset.type).value));
    each('[data-type]', (button) => {
        document.getElementById(button.dataset.type).addEventListener('keydown', (event) => {
            if (event.key === 'Enter') button.click();
        });
    });

    each('[data-touchpad]', bindTouchpad);
    each('[data-scrollpad]', bindScrollpad);
    onClick('[data-mouse]', (button) => clickMouse(Number(button.dataset.mouse)));
    onClick('[data-mouse-hold]', (button) => {
        mouse.buttons ^= Number(button.dataset.mouseHold);
        button.classList.toggle('on', (mouse.buttons & Number(button.dataset.mouseHold)) !== 0);
        post(mousePath, { buttons: mouse.buttons });
    });
    onClick('[data-wheel]', (button) => post(mousePath, { wheel: Number(button.dataset.wheel), buttons: mouse.buttons }));

    each('[data-button]', bindGamepadButton);
    each('[data-hat]', bindHat);
    each('[data-stick]', bindStick);
    each('[data-fader]', bindFader);
    each('[data-steering]', bindSteering);

    onClick('[data-consumer]', (button) => {
        post('consumer', { usage: Number(button.dataset.consumer) });
        post('consumer', { usage: 0 });
    });
    onClick('[data-system]', (button) => {
        if (button.dataset.confirm && !confirm(button.dataset.confirm)) return;
        post('system', { usage: Number(button.dataset.system) });
        post('system', { usage: 0 });
    });

    each('[data-pen]', bindPen);
    onClick('[data-pen-toggle]', (button) => {
        const flag = button.dataset.penToggle;
        penToggles[flag] = !penToggles[flag];
        button.classList.toggle('on', penToggles[flag]);
    });
});
