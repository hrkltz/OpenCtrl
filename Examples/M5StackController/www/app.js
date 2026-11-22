// M5StackController settings: for each PaHub port, 1. its unit, 2. its setup, 3. its HID inputs.
import { adoptStyles } from '@siegel/utilities/styles.js';
import { escapeHtml } from '@siegel/utilities/html.js';
import { toast } from '@siegel/components/siegel-toast.js';
import '@siegel/components/siegel-panel.js';
import '@siegel/components/siegel-select.js';
import '@siegel/components/siegel-switch.js';
import '@siegel/components/siegel-chip.js';

const NAMES = {0x29: 'ToF / Color sensor', 0x3c: 'OLED display', 0x40: 'Encoder U135 / Scroll U186', 0x41: '8Encoder',
  0x43: '8Angle', 0x52: 'Joystick U024-C', 0x5f: 'CardKB', 0x61: 'PbHub', 0x63: 'Joystick2 U024-V2'};
const hex = a => '0x' + a.toString(16).padStart(2, '0');
const UNIT_LABELS = {none: 'None', joystick2: 'Joystick2', encoder: 'Encoder', scroll: 'Scroll'};
// Elements per unit: [source suffix, label, calibrated axis suffix or null].
const ELEMENTS = {
  none: [],
  joystick2: [['x', 'X', 'x'], ['y', 'Y', 'y'], ['button', 'Click', null]],
  encoder: [['turn', 'Turn', 'turn'], ['button', 'Click', null]],
  scroll: [['turn', 'Turn', 'turn'], ['button', 'Click', null]],
};
// Units a scanned address suggests.
const DETECTED = {0x63: ['joystick2'], 0x40: ['encoder', 'scroll']};
const FILTER_HELP = {
  'off': 'Direct readings with no additional software smoothing.',
  'median-3': 'Rejects isolated spikes using three readings. Adds about 4 ms of delay during steady movement at 250 Hz.',
  'ema': 'Smooths small fluctuations. 0.05 is the strongest smoothing; 1 gives no EMA smoothing. Lower values also respond more slowly.',
};
// Mirrors ControllerBindingValid() in the firmware; relative sources bind like axes.
function modes(signal, output) {
  if (signal === 'button') return output === 'button' ? ['normal'] : [];
  if (output === 'motion') return ['normal', 'reversed'];
  if (output === 'button') return ['positive', 'negative'];
  return ['normal', 'reversed', 'positive', 'negative'];
}
function modeLabel(mode, signal, output) {
  if (signal === 'button') return 'Pressed';
  const turn = signal === 'relative';
  if (output === 'button') return turn ? `Press per step ${mode === 'positive' ? '+' : '−'}` : `Past half toward ${mode === 'positive' ? '+' : '−'}`;
  if (output === 'motion') return (turn ? 'Per step' : 'By deflection') + (mode === 'reversed' ? ', reversed' : '');
  return {normal: 'Full range', reversed: 'Full range, reversed', positive: 'Positive half → full range (pedal)',
    negative: 'Negative half → full range (pedal)'}[mode];
}
function targetGroup(t) {
  return t.id.startsWith('gamepad.button') ? 'Gamepad buttons' : t.id.startsWith('gamepad.') ? 'Gamepad axes' :
    t.id.startsWith('mouse.') ? 'Mouse' : 'Keyboard';
}
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

const CSS = `
  :host { display: block; height: 100%; overflow: auto; box-sizing: border-box; padding: 16px;
    padding: max(16px, env(safe-area-inset-top)) max(16px, env(safe-area-inset-right)) max(16px, env(safe-area-inset-bottom)) max(16px, env(safe-area-inset-left)); }
  .page { max-width: 72rem; margin: 0 auto; display: grid; gap: 16px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 22rem), 1fr)); gap: 16px; align-items: start; }
  .title { font-weight: 600; flex: 1 1 auto; }
  .muted { color: var(--text-muted); }
  p { margin: 0 0 8px; }
  h3 { font-size: 11px; text-transform: uppercase; letter-spacing: .04em; color: var(--text-muted); margin: 16px 0 8px; }
  h3:first-of-type { margin-top: 12px; }
  .toolbar { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
  .toolbar label { display: inline-flex; gap: 8px; align-items: center; }
  /* The PaHub: ports 0–2 above the hub, 3–5 below, each a tile that opens its port. */
  .hub { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 8px; margin-bottom: 12px; }
  .hub-body { grid-column: 1 / -1; display: flex; flex-wrap: wrap; justify-content: space-between; gap: 4px 12px;
    align-items: center; padding: 10px 12px; border: 1px dashed var(--divider); border-radius: 8px; background: var(--control-bg); }
  .port-tile { height: auto; min-height: 88px; width: 100%; display: grid; align-content: start; justify-content: stretch; justify-items: start; gap: 2px;
    padding: 8px 10px; text-align: left; white-space: normal; overflow-wrap: anywhere; }
  .port-tile[aria-pressed="true"] { outline: 2px solid var(--accent); outline-offset: -2px; }
  .tile-port { font-size: 11px; text-transform: uppercase; letter-spacing: .04em; color: var(--text-muted); }
  .tile-unit { font-weight: 600; }
  .tile-live, .tile-meta { font-size: 12px; color: var(--text-muted); font-weight: 400; }
  .field { --field-label-w: 7rem; }
  .fields label { display: grid; gap: 2px; font-size: 12px; color: var(--text-muted); }
  .fields input { width: 100%; min-width: 0; box-sizing: border-box; }
  .axis { border-top: 1px solid var(--divider); padding-top: 8px; margin-top: 8px; }
  .axis:first-of-type { border-top: 0; margin-top: 0; padding-top: 0; }
  .axis-head { display: flex; justify-content: space-between; gap: 8px; flex-wrap: wrap; }
  meter { width: 100%; margin: 4px 0 8px; }
  .fields { display: grid; grid-template-columns: repeat(auto-fit, minmax(5.5rem, 1fr)); gap: 8px; }
  .switch-row { display: flex; gap: 8px; align-items: center; margin-top: 6px; }
  .element { margin: 8px 0; }
  .element-head { display: flex; justify-content: space-between; align-items: center; gap: 8px; }
  .assignment { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr) auto; gap: 6px; margin: 6px 0; }
  .assignment select { width: 100%; min-width: 0; }
  dl { display: grid; grid-template-columns: 1fr auto; gap: 4px 16px; margin: 0; }
  dd { margin: 0; text-align: right; font-variant-numeric: tabular-nums; }
  siegel-select[stretch] { display: block; }
  @container (width < 30rem) { .assignment { grid-template-columns: 1fr; } }
`;

class M5Controller extends HTMLElement {
  #root = this.attachShadow({mode: 'open'});
  #state = null; #options = null; #ports = null; #connected = false;
  #busy = false; #capturing = false;
  #cal = []; #calLoaded = false; #calDirty = false;
  #filterLoaded = false; #filterDirty = false;
  #draft = null; #mappingDirty = false;
  #selected = null; // The port whose unit, setup, and inputs are shown.
  // Unit choosers turn into a vertical list where four labels no longer fit side by side;
  // touch density uses larger labels than a mouse.
  #narrow = new ResizeObserver(entries => {
    const minimum = matchMedia('(pointer: coarse)').matches ? 380 : 260;
    for (const entry of entries) entry.target.setAttribute('orientation', entry.contentRect.width < minimum ? 'vertical' : 'horizontal');
  });

  connectedCallback() {
    adoptStyles(this.#root, CSS);
    this.#root.innerHTML = `
      <div class="page">
        <siegel-panel appearance="float">
          <siegel-panel-cell position="edge-top"><span class="title">M5StackController</span><siegel-chip id="connection">Connecting</siegel-chip></siegel-panel-cell>
          <siegel-panel-cell>
            <p id="message" role="status">Connecting to your controller…</p>
            <p class="muted">Select a port on the PaHub, choose what is connected to it, set it up, and assign HID inputs to its elements. Only ports with HID inputs are read while you play.</p>
          </siegel-panel-cell>
        </siegel-panel>
        <siegel-panel appearance="float">
          <siegel-panel-cell position="edge-top"><span class="title">PaHub</span><siegel-chip id="setup-state">Controls running</siegel-chip></siegel-panel-cell>
          <siegel-panel-cell>
            <div id="hub" class="hub" role="group" aria-label="PaHub ports"></div>
            <p id="ports-summary" class="muted">Start setup to scan the PaHub.</p>
            <p id="setup-help" class="muted"></p>
            <div class="toolbar">
              <button id="start" class="primary" disabled>Start setup</button>
              <button id="capture" disabled>Capture range</button><button id="center" disabled>Set centers</button>
              <button id="preview" disabled>Preview</button><button id="save" disabled>Save setup</button>
              <button id="reset" class="danger secondary" disabled>Reset defaults</button>
              <button id="finish" class="primary" disabled>Done / resume controls</button>
              <button id="rescan" disabled>Rescan ports</button>
            </div>
          </siegel-panel-cell>
          <siegel-panel-cell position="edge-bottom">
            <div class="toolbar"><button id="mapping-save" class="primary" disabled>Apply &amp; save ports and inputs</button><span id="mapping-status" class="muted" role="status"></span></div>
          </siegel-panel-cell>
        </siegel-panel>
        <div id="ports"></div>
        <div class="grid">
          <siegel-panel appearance="float" id="filter-settings" hidden>
            <siegel-panel-cell position="edge-top"><span class="title">Input filtering</span></siegel-panel-cell>
            <siegel-panel-cell>
              <siegel-select id="filter-mode" stretch aria-label="Filter"></siegel-select>
              <div class="field" style="margin-top:12px"><label for="filter-alpha">EMA response</label><input id="filter-alpha" type="number" min="0.05" max="1" step="0.01" value="0.35" required></div>
              <p id="filter-help" class="muted" style="margin-top:8px"></p>
              <p class="muted">Filtering applies to joystick axes. Encoder steps are exact and need none.</p>
              <div class="toolbar"><button id="filter-save" disabled>Apply &amp; save</button><span id="filter-status" class="muted" role="status"></span></div>
            </siegel-panel-cell>
          </siegel-panel>
          <siegel-panel appearance="float" id="input-diagnostics" hidden>
            <siegel-panel-cell position="edge-top"><span class="title">Input connection health</span></siegel-panel-cell>
            <siegel-panel-cell>
              <dl>
                <dt>Sensor read errors</dt><dd id="read-errors">0</dd>
                <dt>Reads recovered on retry</dt><dd id="recovered-reads">0</dd>
                <dt>Input-loss cycles</dt><dd id="input-loss-cycles">0</dd>
                <dt>HID send failures</dt><dd id="send-failures">0</dd>
              </dl>
              <div id="loop-timing" hidden>
                <h3>Input loop timing</h3>
                <dl>
                  <dt>Target cycle time (µs)</dt><dd id="poll-microseconds">0</dd>
                  <dt>Last processing time (µs)</dt><dd id="last-work-microseconds">0</dd>
                  <dt>Maximum processing time (µs)</dt><dd id="max-work-microseconds">0</dd>
                  <dt>Cycles over target</dt><dd id="overrun-cycles">0</dd>
                  <dt>Measured cycles</dt><dd id="cycle-count">0</dd>
                </dl>
              </div>
              <p class="muted" style="margin-top:8px">Counts reset on restart. Input loss releases all controls to neutral; a successful read retry avoids that. Processing time excludes the planned wait between cycles.</p>
            </siegel-panel-cell>
          </siegel-panel>
        </div>
        <p id="persistence" class="muted">Reading settings…</p>
      </div>`;
    const $ = id => this.#$(id);
    $('filter-mode').options = [{id: 'off', label: 'Off'}, {id: 'median-3', label: 'Median-3'}, {id: 'ema', label: 'EMA'}];
    $('filter-mode').addEventListener('select-change', () => { this.#filterDirty = true; this.#renderFilter(); });
    $('filter-alpha').addEventListener('input', () => { this.#filterDirty = true; this.#renderFilter(); });
    $('filter-save').onclick = () => {
      if (!$('filter-alpha').checkValidity()) { this.#notify('Choose an EMA response from 0.05 to 1.', true); return; }
      this.#command('filter', {version: 1, mode: $('filter-mode').value, alpha: Number($('filter-alpha').value)});
    };
    $('mapping-save').onclick = () => this.#command('mapping', {version: 1, ports: this.#draft.ports, bindings: this.#draft.bindings});
    $('rescan').onclick = () => this.#rescan();
    $('start').onclick = () => this.#command('start');
    $('finish').onclick = () => { this.#capturing = false; this.#command('end'); };
    $('preview').onclick = () => { try { this.#command('preview', this.#payload()); } catch (error) { this.#notify(error.message, true); } };
    $('save').onclick = () => { try { this.#command('save', this.#payload()); } catch (error) { this.#notify(error.message, true); } };
    $('reset').onclick = () => { if (confirm('Replace the saved setup with default settings?')) this.#command('reset'); };
    $('center').onclick = () => this.#setCenters();
    $('capture').onclick = () => this.#capture();
    this.#poll();
  }

  #$(id) { return this.#root.getElementById(id); }
  #source(id) { return this.#options.sources.find(s => s.id === id); }
  #target(id) { return this.#options.targets.find(t => t.id === id); }
  #axisIndex(id) { return this.#state.axes.findIndex(a => a.id === id); }

  // The message line keeps the current state; toasts report finished operations.
  #notify(text, error = false, announce = false) {
    this.#$('message').textContent = text;
    if (error || announce) toast({message: text, kind: error ? 'error' : 'success', duration: error ? 8000 : 4000});
  }

  #controls() {
    const $ = id => this.#$(id), state = this.#state, active = this.#connected && state?.active;
    $('connection').textContent = !this.#connected ? 'Offline' : 'Connected';
    $('setup-state').textContent = active ? 'Setup: controls paused' : 'Controls running';
    $('setup-help').textContent = active ?
      'Joysticks: Capture range, move the sticks through full travel, Finish capture, release them, Set centers. Encoders: set how many steps the position may move from where it starts. Preview to test, Save setup to keep it, then Done / resume controls.' :
      'Start setup to see live readings from every port, scan the hub, and edit ranges. Controls pause until Done / resume controls.';
    $('start').disabled = !this.#connected || this.#busy || active;
    for (const id of ['capture', 'center', 'finish', 'preview', 'save', 'reset', 'rescan']) $(id).disabled = !active || this.#busy;
    $('save').disabled ||= !state?.storage || this.#capturing;
    $('center').disabled ||= this.#capturing;
    $('preview').disabled ||= this.#capturing;
    for (const el of this.#root.querySelectorAll('.axis input')) el.disabled = !active || this.#busy;
    for (const el of this.#root.querySelectorAll('.axis siegel-switch')) el.toggleAttribute('disabled', !active || this.#busy);
    const editable = this.#connected && this.#options && this.#draft && !this.#busy;
    for (const el of this.#root.querySelectorAll('.assignment select, .assignment button, .use-detected')) el.disabled = !editable;
    for (const el of this.#root.querySelectorAll('.unit')) el.toggleAttribute('disabled', !editable);
    for (const el of this.#root.querySelectorAll('.assign')) el.disabled = !editable || this.#draft.bindings.length >= this.#options.maximum;
    $('mapping-save').disabled = !editable || !state?.storage || !this.#mappingDirty;
    $('filter-settings').hidden = !this.#connected || !state?.filter;
    $('input-diagnostics').hidden = !this.#connected || !state?.diagnostics;
    $('loop-timing').hidden = !this.#connected || !state?.diagnostics?.timing;
    $('filter-mode').toggleAttribute('disabled', !this.#connected || this.#busy);
    $('filter-alpha').disabled = !this.#connected || this.#busy || $('filter-mode').value !== 'ema';
    $('filter-save').disabled = !this.#connected || this.#busy || !state?.storage || !this.#filterDirty;
  }
  #mappingStatus() {
    this.#$('mapping-status').textContent = !this.#state?.storage ? 'Storage unavailable; ports and inputs cannot be saved.' :
      this.#mappingDirty ? 'Unsaved changes.' : this.#state.mapping.saved ? 'Applied and saved on this controller.' : 'Nothing saved yet.';
  }

  // Ports: cards rebuild when the draft or setup fields change; live values update in place.
  #loadMappingDraft() {
    this.#draft = {ports: [...this.#state.mapping.ports], bindings: this.#state.mapping.bindings.map(b => ({...b}))};
    this.#mappingDirty = false;
    this.#renderPorts();
  }
  #changed() { this.#mappingDirty = true; this.#renderPorts(); }
  #setUnit(port, unit) {
    this.#draft.ports[port] = unit;
    // Drop inputs of elements the new unit does not have.
    this.#draft.bindings = this.#draft.bindings.filter(b => this.#source(b.source).port !== port || this.#source(b.source).units.includes(unit));
    this.#changed();
  }
  #select(id, label, groups, value, onChange) {
    const list = document.createElement('select');
    list.id = id;
    list.setAttribute('aria-label', label);
    for (const [group, entries] of groups) {
      const parent = group ? document.createElement('optgroup') : list;
      if (group) { parent.label = group; list.append(parent); }
      for (const [v, text] of entries) {
        const option = document.createElement('option');
        option.value = v; option.textContent = text;
        parent.append(option);
      }
    }
    list.value = value;
    list.addEventListener('change', () => onChange(list.value));
    return list;
  }
  #axisBlock(i, turn) {
    const axis = this.#state.axes[i], values = this.#cal[i], block = document.createElement('div');
    block.className = 'axis';
    const name = axis.label.replace(/^Port \d+ · /, '');
    block.innerHTML = `<div class="axis-head"><strong>${escapeHtml(turn ? 'Turn position' : name)}</strong>
      <span class="muted">Raw <output id="raw-${i}">—</output> · <output id="value-${i}">—</output></span></div>
      <meter id="fill-${i}" min="-1" max="1" value="0" aria-label="${escapeHtml(axis.label)}"></meter><div class="fields"></div>`;
    const fields = turn ? [['minimum', 'Minimum (steps)'], ['maximum', 'Maximum (steps)']] :
      [['minimum', 'Minimum'], ['center', 'Center'], ['maximum', 'Maximum']];
    for (const [key, label] of [...fields, ['deadzone', 'Deadzone (%)']]) {
      const text = document.createElement('label'), input = document.createElement('input');
      input.type = 'number'; input.id = `${key}-${i}`; input.required = true;
      text.textContent = label;
      if (key === 'deadzone') {
        Object.assign(input, {min: 0, max: 99.99, step: '0.01', value: Number((values.deadzone * 100).toFixed(2))});
        input.addEventListener('input', () => { values.deadzone = Number(input.value) / 100; this.#edited(); });
      } else {
        // A turn starts at 0, so its ends lie on either side of it.
        Object.assign(input, {step: '1', value: values[key],
          min: turn && key === 'maximum' ? 1 : axis.rawMinimum, max: turn && key === 'minimum' ? -1 : axis.rawMaximum});
        input.addEventListener('input', () => { values[key] = Number(input.value); this.#edited(); });
      }
      text.append(input);
      block.querySelector('.fields').append(text);
    }
    const toggle = document.createElement('siegel-switch'), row = document.createElement('div'), text = document.createElement('span');
    const invert = turn ? 'Invert direction' : 'Invert axis';
    toggle.id = `inverted-${i}`; toggle.checked = values.inverted; toggle.setAttribute('aria-label', `${invert} ${axis.label}`);
    toggle.addEventListener('toggle-change', event => { values.inverted = event.detail.checked; this.#edited(); });
    row.className = 'switch-row'; text.textContent = invert;
    row.append(toggle, text);
    block.append(row);
    return block;
  }
  #edited() { this.#calDirty = true; this.#$('message').textContent = 'Unsaved setup changes. Preview to test, or Save setup to keep them.'; this.#render(); }
  #renderPorts() {
    if (!this.#options || !this.#state || !this.#draft || !this.#calLoaded) return;
    const groups = new Map();
    for (const t of this.#options.targets) {
      const group = targetGroup(t);
      if (!groups.has(group)) groups.set(group, []);
      groups.get(group).push([t.id, t.label]);
    }
    const targetGroups = [...groups];
    const inputs = port => this.#draft.bindings.filter(b => this.#source(b.source).port === port).length;
    this.#selected ??= Math.max(0, this.#draft.ports.findIndex(unit => unit !== 'none'));
    // The hub: ports 0–2, the hub itself, then ports 3–5.
    const hub = this.#$('hub');
    hub.replaceChildren();
    for (const [port, unit] of this.#draft.ports.entries()) {
      if (port === 3) {
        const body = document.createElement('div');
        body.className = 'hub-body';
        body.innerHTML = '<strong>PaHub</strong><span id="hub-status" class="muted"></span><span class="muted">↔ XIAO ESP32S3 (I2C)</span>';
        hub.append(body);
      }
      const tile = document.createElement('button');
      tile.className = 'port-tile'; tile.id = `tile-${port}`;
      tile.setAttribute('aria-pressed', String(port === this.#selected));
      const count = inputs(port);
      tile.innerHTML = `<span class="tile-port">Port ${port}</span><span class="tile-unit">${escapeHtml(unit === 'none' ? 'Nothing' : UNIT_LABELS[unit])}</span>
        <span class="tile-live" id="tile-live-${port}"></span><span class="tile-meta">${count ? `${count} HID input${count > 1 ? 's' : ''}` : unit === 'none' ? '' : 'No HID inputs'}</span>`;
      tile.onclick = () => { this.#selected = port; this.#renderPorts(); };
      hub.append(tile);
    }
    // The selected port: 1. unit, 2. setup, 3. HID inputs.
    const container = this.#$('ports');
    this.#narrow.disconnect();
    container.replaceChildren();
    for (const [port, unit] of this.#draft.ports.entries()) {
      if (port !== this.#selected) continue;
      const card = document.createElement('siegel-panel');
      card.setAttribute('appearance', 'float');
      card.id = `port-${port}`;
      card.innerHTML = `<siegel-panel-cell position="edge-top"><span class="title">Port ${port}</span></siegel-panel-cell><siegel-panel-cell></siegel-panel-cell>`;
      const body = card.querySelector('siegel-panel-cell:not([position])');
      const found = document.createElement('p');
      found.id = `found-${port}`; found.className = 'muted';
      body.append(found);
      // 1. Unit
      const step1 = document.createElement('h3');
      step1.textContent = '1. Connected unit';
      const units = document.createElement('siegel-select');
      units.id = `unit-${port}`; units.className = 'unit';
      units.setAttribute('stretch', ''); units.setAttribute('aria-label', `Unit connected to port ${port}`);
      units.options = this.#options.units.map(u => ({id: u.id, label: UNIT_LABELS[u.id] || u.label}));
      units.value = unit;
      units.addEventListener('select-change', event => this.#setUnit(port, event.detail.id));
      this.#narrow.observe(units);
      const use = document.createElement('button');
      use.className = 'use-detected text-link'; use.id = `use-${port}`; use.hidden = true;
      use.onclick = () => this.#setUnit(port, use.dataset.unit);
      body.append(step1, units, use);
      if (unit !== 'none') {
        // 2. Setup
        const step2 = document.createElement('h3');
        step2.textContent = '2. Setup';
        body.append(step2);
        for (const [, , axisId] of ELEMENTS[unit]) if (axisId) body.append(this.#axisBlock(this.#axisIndex(`port${port}.${axisId}`), axisId === 'turn'));
        const click = document.createElement('p');
        click.innerHTML = `Click <siegel-chip id="click-${port}">Not checked</siegel-chip>`;
        body.append(click);
        // 3. HID inputs per element
        const step3 = document.createElement('h3');
        step3.textContent = '3. HID inputs';
        body.append(step3);
        for (const [element, label] of ELEMENTS[unit]) {
          const id = `port${port}.${element}`, source = this.#source(id);
          const group = document.createElement('div');
          group.className = 'element'; group.id = `element-${port}-${element}`;
          const head = document.createElement('div'), title = document.createElement('strong'), assign = document.createElement('button');
          head.className = 'element-head'; title.textContent = label;
          assign.className = 'assign text-link'; assign.id = `assign-${port}-${element}`; assign.textContent = '+ Assign HID input';
          assign.onclick = () => {
            this.#draft.bindings.push({source: id, target: source.signal === 'button' ? 'gamepad.button1' : 'gamepad.x', mode: 'normal'});
            this.#changed();
          };
          head.append(title, assign);
          group.append(head);
          for (const [i, binding] of this.#draft.bindings.entries()) {
            if (binding.source !== id) continue;
            const row = document.createElement('div');
            row.className = 'assignment';
            const targets = source.signal === 'button' ?
              targetGroups.map(([g, e]) => [g, e.filter(([t]) => this.#target(t).output === 'button')]).filter(([, e]) => e.length) : targetGroups;
            row.append(this.#select(`target-${i}`, `${label} target`, targets, binding.target, v => {
              binding.target = v;
              const allowed = modes(source.signal, this.#target(v).output);
              if (!allowed.includes(binding.mode)) binding.mode = allowed[0];
              this.#changed();
            }));
            const output = this.#target(binding.target).output;
            row.append(this.#select(`mode-${i}`, `${label} mode`, [[null, modes(source.signal, output).map(m => [m, modeLabel(m, source.signal, output)])]],
              binding.mode, v => { binding.mode = v; this.#changed(); }));
            const remove = document.createElement('button');
            remove.textContent = 'Remove'; remove.id = `remove-${i}`; remove.className = 'danger secondary';
            remove.onclick = () => { this.#draft.bindings.splice(i, 1); this.#changed(); };
            row.append(remove);
            group.append(row);
          }
          body.append(group);
        }
      }
      container.append(card);
    }
    this.#mappingStatus();
    this.#updateLive();
    this.#controls();
  }
  #updateLive() {
    const state = this.#state, ports = this.#ports;
    if (!state || !this.#draft) return;
    for (const [i, axis] of state.axes.entries()) {
      const raw = this.#$(`raw-${i}`);
      if (!raw) continue;
      raw.textContent = axis.online ? axis.raw : '—';
      this.#$(`value-${i}`).textContent = axis.sampled === false ? 'Not checked' : axis.online ? Math.round(axis.value * 100) + '%' : 'Unit disconnected';
      this.#$(`fill-${i}`).value = axis.online ? axis.value : 0;
    }
    for (const [port, unit] of this.#draft.ports.entries()) {
      const scan = ports?.ports[port], devices = ports?.scanned ? scan.devices : null;
      // A tile shows what its unit reports right now.
      const axis = kind => state.axes[this.#axisIndex(`port${port}.${kind}`)];
      const percent = a => Math.round(a.value * 100) + '%';
      const read = unit !== 'none' && axis(unit === 'joystick2' ? 'x' : 'turn').sampled !== false;
      const online = read && axis(unit === 'joystick2' ? 'x' : 'turn').online;
      const live = this.#$(`tile-live-${port}`);
      if (live) live.textContent = unit === 'none' ? (devices?.length ? 'Found: ' + devices.map(a => NAMES[a] || hex(a)).join(', ') : '') :
        !read ? 'Not read' : !online ? 'Disconnected' :
        (unit === 'joystick2' ? `X ${percent(axis('x'))} · Y ${percent(axis('y'))}` : `Position ${axis('turn').raw}`) + (scan?.pressed ? ' · click' : '');
      const found = this.#$(`found-${port}`);
      if (found) found.textContent = !devices ? 'Not scanned yet.' : devices.length ? 'Found: ' + devices.map(a => hex(a) + ' ' + (NAMES[a] || 'Unknown device')).join(', ') : 'Found: nothing';
      const click = this.#$(`click-${port}`);
      if (click) click.textContent = scan?.pressed === true ? 'Pressed' : scan?.pressed === false ? 'Released' : 'Not checked';
      const suggested = (devices || []).flatMap(a => DETECTED[a] || []);
      const use = this.#$(`use-${port}`);
      if (use) {
        use.hidden = !suggested.length || suggested.includes(this.#draft.ports[port]);
        use.dataset.unit = suggested[0] || '';
        use.textContent = `Use detected ${UNIT_LABELS[suggested[0]] || 'unit'}`;
      }
    }
    const status = this.#$('hub-status');
    if (status) status.textContent = !ports?.scanned ? 'Not scanned' : ports.hub ? `Found at ${hex(ports.hubAddress)}` : `Not found at ${hex(ports.hubAddress)}`;
    this.#$('ports-summary').textContent = !ports?.scanned ? (state.active ? 'Scanning…' : 'Start setup to scan the PaHub.') :
      ports.hub ? `PaHub at ${hex(ports.hubAddress)}.` + (ports.direct.length ? ' Direct bus: ' + ports.direct.map(a => hex(a) + ' ' + (NAMES[a] || 'device')).join(', ') + '.' : '') + (state.active ? '' : ' Start setup to rescan.') :
      `No PaHub found at ${hex(ports.hubAddress)}. Check its wiring, or change hubAddress in Config.hpp.`;
  }
  async #rescan() {
    try {
      const response = await fetch('/api/ports/scan', {method: 'POST', headers: {'X-OpenCtrl-Calibration': '1'}});
      if (!response.ok) throw new Error(await response.text());
      this.#notify('Rescanning ports…');
    } catch (error) { this.#notify(error.message, true); }
  }

  // Setup: joystick calibration and encoder turn ranges.
  #loadCalibration() {
    this.#cal = this.#state.axes.map(a => ({minimum: a.minimum, center: a.center, maximum: a.maximum, deadzone: a.deadzone, inverted: a.inverted}));
    this.#calLoaded = true; this.#calDirty = false;
    this.#renderPorts();
  }
  #payload() {
    for (const input of this.#root.querySelectorAll('.axis input[type=number]'))
      if (!input.checkValidity()) { input.reportValidity(); throw new Error('Complete every setup field within its allowed range.'); }
    const axes = this.#state.axes.map((axis, i) => ({id: axis.id, ...this.#cal[i]}));
    for (const axis of axes)
      if (![axis.minimum, axis.center, axis.maximum].every(Number.isInteger) || !(axis.minimum < axis.center && axis.center < axis.maximum) ||
          !Number.isFinite(axis.deadzone) || axis.deadzone < 0 || axis.deadzone >= 1)
        throw new Error('Each axis needs minimum < center < maximum, and a deadzone below 100%.');
    return {version: 1, axes};
  }
  #setCenters() {
    for (const [i, axis] of this.#state.axes.entries()) if (!axis.id.endsWith('.turn') && axis.online) this.#cal[i].center = axis.raw;
    this.#calDirty = true;
    this.#renderPorts();
    this.#notify('Centers captured. Preview or Save setup.');
  }
  async #capture() {
    if (!this.#capturing) { await this.#command('capture'); return; }
    await this.#command('stop-capture');
    if (this.#state.capturing) return;
    let updated = 0;
    for (const [i, axis] of this.#state.axes.entries())
      if (!axis.id.endsWith('.turn') && axis.observed && axis.observedMaximum - axis.observedMinimum >= 2) {
        this.#cal[i].minimum = axis.observedMinimum; this.#cal[i].maximum = axis.observedMaximum; updated++;
      }
    this.#calDirty = true;
    this.#renderPorts();
    this.#notify(updated ? 'Range captured. Release the sticks, choose Set centers, then Preview or Save setup.' : 'No travel detected. Previous limits were kept.', !updated);
  }

  #loadFilter() {
    this.#$('filter-mode').value = this.#state.filter.mode;
    this.#$('filter-alpha').value = this.#state.filter.alpha;
    this.#filterLoaded = true; this.#filterDirty = false;
    this.#renderFilter();
  }
  #renderFilter() {
    const state = this.#state;
    this.#$('filter-help').textContent = FILTER_HELP[this.#$('filter-mode').value] || '';
    this.#$('filter-status').textContent = !state?.storage ? 'Storage unavailable; settings cannot be saved.' : this.#filterDirty ? 'Unsaved filter changes.' :
      state?.filter?.saved ? 'Applied and saved on this controller.' : 'Using the default filter: Median-3.';
    this.#controls();
  }

  #render() {
    const state = this.#state;
    if (!state) return;
    this.#$('persistence').textContent = !state.storage ? 'Storage unavailable' : this.#calDirty ? 'Unsaved setup changes' : state.saved ? 'Setup saved on this controller' : 'Using default setup';
    if (state.diagnostics) for (const [id, key] of [['read-errors', 'readFailures'], ['recovered-reads', 'recoveredReads'], ['input-loss-cycles', 'inputLossCycles'], ['send-failures', 'sendFailures']])
      this.#$(id).textContent = state.diagnostics[key];
    if (state.diagnostics?.timing) for (const [id, key] of [['poll-microseconds', 'pollMicroseconds'], ['last-work-microseconds', 'lastWorkMicroseconds'],
        ['max-work-microseconds', 'maxWorkMicroseconds'], ['overrun-cycles', 'overrunCycles'], ['cycle-count', 'cycleCount']])
      this.#$(id).textContent = state.diagnostics.timing[key];
    if (state.filter) { if (!this.#filterLoaded) this.#loadFilter(); else this.#renderFilter(); }
    this.#mappingStatus();
    this.#updateLive();
    this.#controls();
  }
  async #refresh() {
    if (!this.#options) {
      const response = await fetch('/api/calibration/options', {cache: 'no-store'});
      if (!response.ok) throw new Error('Could not read the controller options');
      this.#options = await response.json();
    }
    const [response, portResponse] = await Promise.all([fetch('/api/calibration', {cache: 'no-store'}), fetch('/api/ports', {cache: 'no-store'})]);
    if (!response.ok || !portResponse.ok) throw new Error('Could not read controller settings');
    const previous = this.#state, reconnecting = !this.#connected;
    this.#state = await response.json();
    this.#ports = await portResponse.json();
    this.#connected = true;
    this.#capturing = this.#state.capturing;
    this.#$('capture').textContent = this.#capturing ? 'Finish capture' : 'Capture range';
    if (reconnecting) this.#notify(this.#state.message);
    if (!this.#calLoaded) this.#loadCalibration();
    if (!this.#draft) this.#loadMappingDraft();
    // Done discards unsaved previews, so reload the fields from the controller.
    if (previous?.active && !this.#state.active) { this.#capturing = false; this.#loadCalibration(); }
    this.#render();
  }
  async #command(action, body = {}) {
    if (this.#busy) return;
    this.#busy = true;
    this.#controls();
    try {
      const response = await fetch('/api/calibration/' + action, {method: 'POST',
        headers: {'Content-Type': 'application/json', 'X-OpenCtrl-Calibration': '1'}, body: JSON.stringify(body)});
      if (!response.ok) throw new Error(await response.text());
      const queued = await response.json();
      for (let attempt = 0; attempt < 60; attempt++) {
        await delay(100);
        await this.#refresh();
        if (this.#state.completedOperation !== queued.operation) continue;
        if (this.#state.operationStatus >= 400) throw new Error(this.#state.message);
        this.#notify(this.#state.message, false, true);
        if (action === 'filter') this.#loadFilter();
        if (action === 'mapping') this.#loadMappingDraft();
        if (['start', 'save', 'reset', 'end'].includes(action)) this.#loadCalibration();
        return;
      }
      throw new Error('No confirmation from controller. Reconnect and check the saved settings.');
    } catch (error) {
      this.#notify(error.message, true);
    } finally {
      this.#busy = false;
      this.#render();
    }
  }
  async #poll() {
    try { await this.#refresh(); }
    catch (error) {
      if (this.#connected) this.#notify('Connection lost. Check that you and the controller are on the same WiFi.', true);
      this.#connected = false;
      this.#controls();
    }
    setTimeout(() => this.#poll(), 250);
  }
}
customElements.define('m5-controller', M5Controller);
