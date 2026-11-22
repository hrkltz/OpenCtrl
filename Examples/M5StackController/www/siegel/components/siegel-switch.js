// Importing this module registers <siegel-switch>.
// Shared sibling contract: checked/disabled attributes and properties; confirm(next)
// returns boolean | Promise<boolean> (false cancels, null/undefined allows).
// pending is read-only; confirmation temporarily locks input and sets aria-busy.
// A state write, hook replacement or disconnect invalidates pending confirmation.
// Both toggle-change and switch-change emit { checked } once per accepted change.
// Rejections emit confirmation-error { error }; programmatic state writes are silent.
// <siegel-switch> is a draggable native checkbox with role="switch".
// Label it with aria-label. focus() delegates to the native input.
// The application owns confirmation UI and persistence:
//   sw.confirm = next => confirmModal({ message: next ? 'Allow writes?' : 'Restrict writes?' });
//   sw.addEventListener('toggle-change', ({ detail }) => save(detail.checked));
// Legacy callbacks returning confirmation-options objects remain supported.

import { adoptStyles } from '../utilities/styles.js';
import { adoptPreUpgrade, emit } from '../utilities/element.js';
import { confirmModal } from './confirm-modal.js';

const DRAG_SLOP = 4;

const STYLES = `
  :host { display: inline-flex; align-items: center; min-height: var(--ctl-h); vertical-align: middle; cursor: pointer; }
  label { display: inline-flex; align-items: center; min-height: var(--ctl-h); cursor: pointer; }
  /* The input IS the control (native semantics, native focus); the skin is
     drawn on it directly, so there is no decorative twin to keep in sync. */
  input {
    appearance: none; -webkit-appearance: none; margin: 0; cursor: pointer;
    /* Track: sized off the density knob so a touch pointer enlarges it. */
    width: calc(var(--switch-h) * 1.8);
    height: calc(var(--switch-h));
    border: 1px solid var(--control-border);
    border-radius: 999px;
    background: var(--control-bg);
    position: relative; touch-action: none; padding: 0;
    transition: background 120ms ease, border-color 120ms ease;
  }
  /* Thumb */
  input::before {
    content: ""; position: absolute; top: 1px; left: 1px;
    width: calc(var(--switch-h) - 4px);
    height: calc(var(--switch-h) - 4px);
    transform: translateX(var(--knob-x, 0px));
    border-radius: 50%;
    background: var(--text-muted);
    transition: transform 120ms ease, background 120ms ease;
  }
  input:checked { background: var(--accent); border-color: var(--accent); --knob-x: calc(var(--switch-h) * 0.8); }
  input:checked::before {
    background: var(--accent-foreground);
  }
  input.dragging::before { transition: none; }
  input:disabled { opacity: 0.5; cursor: default; }
  @media (prefers-reduced-motion: reduce) {
    input, input::before { transition: none; }
  }
`;

export class SiegelSwitch extends HTMLElement {
  static observedAttributes = ['checked', 'disabled', 'aria-label'];
  #input;
  #confirm = null;
  #request = null;
  #rect = null;
  #pointer = null;
  #travel = 0;
  #x = 0;
  #moved = false;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, STYLES);
    this.shadowRoot.innerHTML = '<label><input type="checkbox" role="switch" /></label>';
    this.#input = this.shadowRoot.querySelector('input');
    const label = this.shadowRoot.querySelector('label');
    label.addEventListener('click', (event) => {
      if (event.target !== label) return;
      event.preventDefault();
      this.#input.focus();
      this.#input.click();
    });
    this.#input.addEventListener('pointerdown', (event) => this.#onDown(event));
    this.#input.addEventListener('pointermove', (event) => this.#onMove(event));
    this.#input.addEventListener('pointerup', (event) => this.#onUp(event));
    this.#input.addEventListener('pointercancel', () => this.#endDrag());
    this.#input.addEventListener('lostpointercapture', () => this.#endDrag());
    // Pointer release commits once; native activation handles keyboard and AT.
    this.#input.addEventListener('click', (event) => {
      if (event.detail !== 0) event.preventDefault();
    });
    this.#input.addEventListener('change', () => this.#commit(this.#input.checked));
  }

  set checked(value) { this.#cancel(); this.toggleAttribute('checked', !!value); }
  get checked() { return this.hasAttribute('checked'); }
  set disabled(value) { this.#cancel(); this.toggleAttribute('disabled', !!value); }
  get disabled() { return this.hasAttribute('disabled'); }
  set confirm(value) { this.#cancel(); this.#confirm = typeof value === 'function' ? value : null; }
  get confirm() { return this.#confirm; }
  get pending() { return this.#request !== null; }
  focus(options) { this.#input.focus(options); }

  connectedCallback() {
    adoptPreUpgrade(this, ['confirm', 'checked', 'disabled']);
    this.#sync();
  }
  disconnectedCallback() { this.#cancel(); }
  attributeChangedCallback(name) {
    if (name !== 'aria-label') this.#cancel();
    this.#sync();
  }

  #sync() {
    this.#input.checked = this.checked;
    this.#input.disabled = this.disabled || this.pending;
    if (this.pending) this.setAttribute('aria-busy', 'true');
    else this.removeAttribute('aria-busy');
    const label = this.getAttribute('aria-label');
    if (label == null) this.#input.removeAttribute('aria-label');
    else this.#input.setAttribute('aria-label', label);
  }
  #cancel() {
    this.#request = null;
    this.#endDrag();
    this.#sync();
  }

  #onDown(event) {
    if (this.#input.disabled || event.button !== 0 || this.#rect) return;
    this.#pointer = event.pointerId;
    this.#input.setPointerCapture(event.pointerId);
    this.#rect = this.#input.getBoundingClientRect();
    this.#travel = this.#rect.width - this.#rect.height;
    this.#moved = false;
    this.#x = this.checked ? this.#travel : 0;
    this.#input.classList.add('dragging');
    this.#paint();
  }
  #onMove(event) {
    if (!this.#rect || event.pointerId !== this.#pointer) return;
    const home = this.checked ? this.#travel : 0;
    this.#x = Math.min(this.#travel, Math.max(0, event.clientX - this.#rect.left - this.#rect.height / 2));
    if (Math.abs(this.#x - home) > DRAG_SLOP) this.#moved = true;
    this.#paint();
  }
  #onUp(event) {
    if (!this.#rect || event.pointerId !== this.#pointer) return;
    const next = this.#moved ? this.#x > this.#travel / 2 : !this.checked;
    this.#endDrag();
    void this.#commit(next);
    event.preventDefault();
  }
  #endDrag() {
    const pointer = this.#pointer;
    this.#pointer = null;
    this.#rect = null;
    this.#moved = false;
    if (pointer !== null && this.#input.hasPointerCapture(pointer)) this.#input.releasePointerCapture(pointer);
    this.#input.classList.remove('dragging');
    this.#input.style.removeProperty('--knob-x');
  }
  #paint() { this.#input.style.setProperty('--knob-x', `${this.#x}px`); }

  async #commit(next) {
    if (this.disabled || this.pending || next === this.checked) { this.#sync(); return; }
    if (this.#confirm) {
      const request = {};
      this.#request = request;
      this.#sync();
      try {
        let decision = await this.#confirm(next);
        if (this.#request !== request || !this.isConnected) return;
        if (decision && typeof decision === 'object') decision = await confirmModal(decision);
        if (this.#request !== request || !this.isConnected) return;
        if (decision === false) return;
      } catch (error) {
        if (this.#request === request) emit(this, 'confirmation-error', { error });
        return;
      } finally {
        if (this.#request === request) { this.#request = null; this.#sync(); }
      }
    }
    this.checked = next;
    const detail = { checked: next };
    emit(this, 'toggle-change', detail);
    emit(this, 'switch-change', detail);
  }
}

if (!customElements.get('siegel-switch')) customElements.define('siegel-switch', SiegelSwitch);
