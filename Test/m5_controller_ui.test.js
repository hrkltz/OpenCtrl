const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {chromium} = require('playwright');

const www = path.join(__dirname, '../Examples/M5StackController/www');
const types = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css'};
const units = [{id: 'none', label: 'Nothing'}, {id: 'joystick2', label: 'Joystick2 U024-V2'}, {id: 'encoder', label: 'Encoder U135'}, {id: 'scroll', label: 'Scroll U186'}];
const sources = Array.from({length: 6}, (_, port) => [['x', 'X', 'axis', ['joystick2']], ['y', 'Y', 'axis', ['joystick2']],
  ['turn', 'turn', 'relative', ['encoder', 'scroll']], ['button', 'click', 'button', ['joystick2', 'encoder', 'scroll']]]
  .map(([id, label, signal, provided]) => ({id: `port${port}.${id}`, label: `Port ${port} · ${label}`, signal, port, units: provided}))).flat();
const targets = [
  ...['x', 'y', 'z', 'rz', 'rx', 'ry'].map(a => ({id: 'gamepad.' + a, label: 'Gamepad ' + a, output: 'axis'})),
  ...Array.from({length: 16}, (_, i) => ({id: 'gamepad.button' + (i + 1), label: 'Gamepad button ' + (i + 1), output: 'button'})),
  {id: 'mouse.x', label: 'Mouse X', output: 'motion'}, {id: 'mouse.y', label: 'Mouse Y', output: 'motion'},
  {id: 'mouse.wheel', label: 'Mouse wheel', output: 'motion'}, {id: 'mouse.left', label: 'Mouse left', output: 'button'},
  {id: 'mouse.right', label: 'Mouse right', output: 'button'}, {id: 'mouse.middle', label: 'Mouse middle', output: 'button'},
  ...['w', 'a', 's', 'd', 'space', 'enter', 'left', 'right'].map(k => ({id: 'key.' + k, label: 'Key ' + k, output: 'button'})),
];
const saved = {version: 1, ports: ['joystick2', 'encoder', 'none', 'none', 'none', 'none'],
  bindings: [{source: 'port1.turn', target: 'key.right', mode: 'positive'}]};

// Polls until the check passes; locators already reach into the page's shadow DOM.
async function until(check, timeout = 5000) {
  const end = Date.now() + timeout;
  for (;;) {
    try { if (await check()) return; } catch {}
    if (Date.now() > end) throw new Error('Timed out waiting for ' + check);
    await new Promise(resolve => setTimeout(resolve, 50));
  }
}

test('PaHub ports, units, setup, HID inputs, filters, and mobile layout', async () => {
  const browser = await chromium.launch({headless: true, executablePath: process.env.OPENCTRL_CHROMIUM});
  try {
    const context = await browser.newContext({viewport: {width: 1280, height: 900}});
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    let failSave = false, lastFilter = null, lastAxes = null, lastMapping = null, operation = 0, scans = 0;
    const state = {
      title: 'M5StackController', active: false, storage: true, saved: false, capturing: false,
      completedOperation: 0, operationStatus: 200, message: 'Ready', version: 1,
      mapping: {saved: false, ports: ['joystick2', 'none', 'none', 'none', 'none', 'none'], bindings: []},
      filter: {mode: 'median-3', alpha: 0.35, saved: false},
      diagnostics: {readFailures: 3, recoveredReads: 4, inputLossCycles: 1, sendFailures: 0,
        timing: {cycleCount: 100, lastWorkMicroseconds: 900, maxWorkMicroseconds: 5000, overrunCycles: 2, pollMicroseconds: 4000}},
      axes: Array.from({length: 18}, (_, i) => {
        const port = Math.floor(i / 3), kind = ['x', 'y', 'turn'][i % 3], turn = kind === 'turn';
        return {id: `port${port}.${kind}`, label: `Port ${port} · ${turn ? 'turn' : kind.toUpperCase()}`,
          rawMinimum: turn ? -32768 : 0, rawMaximum: turn ? 32767 : 65535, minimum: turn ? -24 : 0, center: turn ? 0 : 32768,
          maximum: turn ? 24 : 65535, deadzone: turn ? 0 : 0.05, inverted: false, raw: turn ? 0 : 32768, online: true, sampled: true, value: 0};
      }),
    };
    const ports = {scanned: false, hub: false, hubAddress: 0x70, direct: [], ports: Array.from({length: 6}, () => ({devices: [], pressed: null}))};
    let baseline = structuredClone(state.axes);
    await page.route('http://controller.test/**', async route => {
      const request = route.request(), url = new URL(request.url());
      if (request.method() === 'GET' && !url.pathname.startsWith('/api/')) {
        const file = path.join(www, url.pathname === '/' ? 'index.html' : url.pathname);
        return fs.existsSync(file) ? route.fulfill({contentType: types[path.extname(file)], body: fs.readFileSync(file)}) : route.fulfill({status: 404});
      }
      if (request.method() === 'GET') {
        if (url.pathname === '/api/calibration/options') return route.fulfill({json: {maximum: 8, units, sources, targets}});
        if (url.pathname === '/api/ports') return route.fulfill({json: ports});
        return route.fulfill({json: state});
      }
      assert.equal(request.headers()['x-openctrl-calibration'], '1');
      if (url.pathname === '/api/ports/scan') { scans++; return route.fulfill({status: 202, body: ''}); }
      state.completedOperation = ++operation;
      state.operationStatus = 200;
      if (url.pathname.endsWith('/filter')) {
        lastFilter = request.postDataJSON();
        if (failSave) { state.operationStatus = 503; state.message = 'Storage write failed; filter settings were not changed'; }
        else { state.filter = {...lastFilter, saved: true}; state.message = 'Filter settings applied and saved'; }
      } else if (url.pathname.endsWith('/mapping')) {
        lastMapping = request.postDataJSON();
        if (failSave) { state.operationStatus = 503; state.message = 'Storage write failed; ports and bindings were not changed'; }
        else { state.mapping = {saved: true, ports: lastMapping.ports, bindings: lastMapping.bindings}; state.message = 'Ports and bindings applied and saved'; }
      } else if (url.pathname.endsWith('/start')) {
        baseline = structuredClone(state.axes);
        state.active = true; state.message = 'Setup active; HID output paused';
      } else if (url.pathname.endsWith('/end')) {
        state.axes = structuredClone(baseline);
        state.active = false; state.message = 'HID output resumed';
      } else if (url.pathname.endsWith('/preview') || url.pathname.endsWith('/save')) {
        lastAxes = request.postDataJSON();
        state.axes = state.axes.map((axis, i) => ({...axis, ...lastAxes.axes[i]}));
        state.saved = url.pathname.endsWith('/save');
        if (state.saved) baseline = structuredClone(state.axes);
        state.message = state.saved ? 'Calibration saved' : 'Preview applied; not saved';
      }
      return route.fulfill({status: 202, json: {operation}});
    });
    await page.goto('http://controller.test/');
    const $ = id => page.locator('#' + id);
    const text = async id => (await $(id).textContent()).trim();
    await $('filter-settings').waitFor({state: 'visible'});
    await until(async () => await text('connection') === 'Connected');

    // Diagnostics and live readings of the joystick on port 0.
    assert.equal(await text('read-errors'), '3');
    assert.equal(await text('input-loss-cycles'), '1');
    assert.equal(await text('poll-microseconds'), '4000');
    state.diagnostics.timing.cycleCount = 200;
    await until(async () => await text('cycle-count') === '200');
    const timing = state.diagnostics.timing;
    delete state.diagnostics.timing;
    await $('loop-timing').waitFor({state: 'hidden'});
    state.diagnostics.timing = timing;
    await $('loop-timing').waitFor({state: 'visible'});
    state.axes[0].sampled = false; state.axes[0].online = false;
    await until(async () => await text('value-0') === 'Not checked');
    state.axes[0].sampled = true;
    await until(async () => await text('value-0') === 'Unit disconnected');
    state.axes[0].online = true; state.axes[0].value = 0.5;
    await until(async () => await text('value-0') === '50%');
    state.axes[0].value = 0;

    // Filters.
    assert.equal(await $('filter-alpha').isDisabled(), true);
    await $('filter-mode').getByText('EMA', {exact: true}).click();
    assert.match(await text('filter-help'), /0\.05 is the strongest smoothing/);
    await $('filter-alpha').fill('0.5');
    failSave = true;
    await $('filter-save').click();
    await until(async () => (await text('message')).startsWith('Storage write failed'));
    assert.equal(await text('filter-status'), 'Unsaved filter changes.');
    failSave = false;
    await $('filter-save').click();
    await until(async () => (await text('filter-status')).startsWith('Applied and saved'));
    assert.deepEqual(lastFilter, {version: 1, mode: 'ema', alpha: 0.5});

    // The PaHub shows every port; the first port with a unit is selected.
    assert.equal(await $('tile-0').getAttribute('aria-pressed'), 'true');
    assert.match(await text('tile-0'), /Port 0\s*Joystick2\s*X 0% · Y 0%\s*No HID inputs/);
    assert.match(await text('tile-4'), /Port 4\s*Nothing/);
    assert.equal(await text('hub-status'), 'Not scanned');

    // 1. A port's unit decides its setup and elements; only the selected port is shown.
    assert.equal(await $('port-0').getByText('2. Setup').count(), 1);
    assert.equal(await $('minimum-0').count(), 1);
    assert.equal(await $('minimum-2').count(), 0); // A joystick has no turn.
    await $('tile-1').click();
    assert.equal(await $('port-0').count(), 0);
    assert.equal(await $('port-1').getByText('2. Setup').count(), 0);
    await $('unit-1').getByText('Encoder', {exact: true}).click();
    await $('element-1-turn').waitFor();
    assert.match(await text('tile-1'), /Port 1\s*Encoder/);
    assert.equal(await $('element-1-x').count(), 0);
    assert.equal(await $('center-5').count(), 0); // A turn starts at 0, so it has no center field.
    assert.equal(await $('minimum-5').getAttribute('max'), '-1');
    assert.equal(await text('mapping-status'), 'Unsaved changes.');

    // 3. HID inputs: targets and modes follow the element.
    await $('tile-0').click();
    await $('assign-0-x').click();
    await $('target-0').selectOption('key.w');
    assert.deepEqual(await $('mode-0').locator('option').evaluateAll(o => o.map(x => x.value)), ['positive', 'negative']);
    await $('mode-0').selectOption('negative');
    await $('assign-0-button').click();
    assert.equal(await $('target-1 option[value="gamepad.x"]').count(), 0); // A click only drives buttons.
    assert.deepEqual(await $('mode-1').locator('option').evaluateAll(o => o.map(x => x.textContent)), ['Pressed']);
    assert.match(await text('tile-0'), /2 HID inputs/);
    await $('tile-1').click();
    await $('assign-1-turn').click();
    await $('target-2').selectOption('mouse.wheel');
    assert.deepEqual(await $('mode-2').locator('option').evaluateAll(o => o.map(x => x.textContent)), ['Per step', 'Per step, reversed']);
    await $('target-2').selectOption('key.right');
    assert.equal(await $('mode-2 option').first().textContent(), 'Press per step +');
    await $('tile-0').click();
    await $('remove-1').click();
    await $('tile-1').click();
    assert.equal(await $('target-1').inputValue(), 'key.right');
    // Changing a unit drops the inputs of elements it does not have.
    await $('tile-0').click();
    await $('unit-0').getByText('Scroll', {exact: true}).click();
    await $('element-0-turn').waitFor();
    assert.equal(await $('target-0').count(), 0);
    await $('unit-0').getByText('Joystick2', {exact: true}).click();
    assert.match(await text('tile-0'), /No HID inputs/);

    // Units and inputs are saved together.
    failSave = true;
    await $('mapping-save').click();
    await until(async () => await text('message') === 'Storage write failed; ports and bindings were not changed');
    assert.equal(state.mapping.ports[1], 'none');
    failSave = false;
    await $('mapping-save').click();
    await until(async () => (await text('mapping-status')).startsWith('Applied and saved'));
    assert.deepEqual(lastMapping, saved);
    await page.reload();
    await $('tile-1').click();
    await $('element-1-turn').waitFor();
    assert.equal(await $('target-0').inputValue(), 'key.right');

    // Setup: scan results on the hub, detected units, and editing ranges.
    assert.match(await text('ports-summary'), /Start setup to scan/);
    assert.equal(await $('minimum-5').isDisabled(), true);
    await $('start').click();
    await until(async () => !(await $('finish').isDisabled()));
    Object.assign(ports, {scanned: true, hub: true, direct: [0x3c]});
    ports.ports[0] = {devices: [0x63], pressed: false};
    ports.ports[1] = {devices: [0x40], pressed: false};
    ports.ports[2] = {devices: [0x63], pressed: null};
    ports.ports[4] = {devices: [0x40, 0x41], pressed: null};
    await until(async () => (await text('ports-summary')).includes('0x3c OLED display'));
    assert.equal(await text('hub-status'), 'Found at 0x70');
    assert.equal(await text('tile-live-2'), 'Found: Joystick2 U024-V2');
    assert.match(await text('found-1'), /0x40 Encoder U135 \/ Scroll U186/);
    assert.equal(await $('use-1').isVisible(), false); // The chosen encoder matches 0x40.
    ports.ports[1].pressed = true;
    await until(async () => await text('click-1') === 'Pressed');
    assert.match(await text('tile-live-1'), /Position 0 · click/);
    await $('rescan').click();
    await until(async () => await text('message') === 'Rescanning ports…');
    assert.equal(scans, 1);

    state.axes[5].raw = 7; state.axes[5].value = 7 / 24;
    await until(async () => await text('raw-5') === '7');
    await $('minimum-5').fill('1');
    await $('maximum-5').fill('48');
    await $('inverted-5').click();
    await $('preview').click();
    assert.equal(lastAxes, null); // A turn's minimum must lie below its start.
    await $('minimum-5').fill('-1');
    await $('tile-2').click();
    await $('use-2').click();
    await $('element-2-x').waitFor();
    assert.match(await text('tile-2'), /Joystick2/);
    assert.equal(await $('mapping-save').isDisabled(), false); // Units can be applied during setup too.
    await $('minimum-6').fill('1000');
    await $('center-6').fill('32000');
    await $('maximum-6').fill('65536');
    await $('deadzone-6').fill('2.5');
    await $('preview').click();
    assert.equal(lastAxes, null); // Out-of-sensor-range values are rejected before sending.
    await $('maximum-6').fill('64000');
    await $('preview').click();
    await until(async () => await text('message') === 'Preview applied; not saved');
    assert.deepEqual(lastAxes.axes[6], {id: 'port2.x', minimum: 1000, center: 32000, maximum: 64000, deadzone: 0.025, inverted: false});
    assert.deepEqual(lastAxes.axes[5], {id: 'port1.turn', minimum: -1, center: 0, maximum: 48, deadzone: 0, inverted: true});
    await $('save').click();
    await until(async () => await text('message') === 'Calibration saved');
    await $('finish').click();
    await until(async () => !(await $('start').isDisabled()));
    await until(async () => await $('deadzone-6').isDisabled());
    assert.equal(await $('deadzone-6').inputValue(), '2.5');

    ports.hub = false;
    await until(async () => (await text('ports-summary')).startsWith('No PaHub found at 0x70'));
    assert.equal(await text('hub-status'), 'Not found at 0x70');
    for (const width of [320, 390, 1280]) {
      await page.setViewportSize({width, height: 844});
      // With a mouse, the four unit labels fit side by side down to a 260px chooser.
      await until(async () => await $('unit-2').getAttribute('orientation') === (width === 320 ? 'vertical' : 'horizontal'));
      assert.equal(await page.locator('m5-controller').evaluate(el => el.scrollWidth <= el.clientWidth), true);
    }
    if (process.env.OPENCTRL_UI_SCREENSHOT) {
      await page.setViewportSize({width: 1280, height: 900});
      await page.locator('m5-controller').evaluate(el => { el.style.height = 'auto'; el.style.overflow = 'visible'; });
      await page.screenshot({path: process.env.OPENCTRL_UI_SCREENSHOT, fullPage: true});
    }
    assert.deepEqual(errors, []);
  } finally { await browser.close(); }
});
