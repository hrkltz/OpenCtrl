// A slim progress indicator with two representations of the same idea — "how
// far along". The default `bar` is a horizontal track (the shared design
// element behind task-modal's upload/batch progress); `type="spinner"` is a
// circular ring for inline or centered waits. Both read the same `value`, and
// both go indeterminate (animated) when it's absent. Importing this module
// registers <siegel-progress>.
//
//   <siegel-progress value="0.4"></siegel-progress>            // bar, 40% (value is a 0..1 ratio)
//   <siegel-progress type="spinner" value="0.4"></siegel-progress>  // ring, 40%
//   el.value = 0.4;                                             // property mirror of the attribute
//   <siegel-progress></siegel-progress>                        // no value → indeterminate (animated)
//   <siegel-progress type="spinner"></siegel-progress>         // endless spinning ring
//   el.value = null;                                            // back to indeterminate
//
//   value  number 0..1 — the fill ratio (clamped). Absent/null/NaN ⇒ indeterminate.
//   type   "bar" (default) | "spinner".
//
// Track + fill read the shared tokens (--control-border / --accent), so it themes
// with everything else. The bar height is the --progress-h custom property
// (default 6px); the spinner diameter is --spinner-size (default 28px).
// role="progressbar" with aria-valuenow when determinate. Honours
// prefers-reduced-motion (no fill transition, no indeterminate slide/spin).

import { adoptStyles } from '../utilities/styles.js';
import { adoptPreUpgrade } from '../utilities/element.js';

// Ring geometry (in the 0..100 viewBox). The circumference drives the arc's
// stroke-dasharray/offset — the determinate fill is a fraction of it.
const RING_R = 45;
const RING_C = 2 * Math.PI * RING_R;

// The fraction of the ring the indeterminate arc covers as it sweeps.
const RING_SWEEP = 0.3;

const STYLES = `
  :host {
    display: block; height: var(--progress-h, 6px);
    border-radius: 999px; background: var(--control-border); overflow: hidden;
  }
  .bar {
    height: 100%; width: 0; border-radius: inherit;
    background: var(--accent); transition: width 120ms linear;
  }
  /* No \`value\` ⇒ indeterminate: a short segment slides across the track. */
  :host(:not([value])) .bar { width: 40%; }
  @media (prefers-reduced-motion: no-preference) {
    :host(:not([value])) .bar { animation: siegel-progress-slide 1.1s ease-in-out infinite; }
  }
  @media (prefers-reduced-motion: reduce) { .bar { transition: none; } }
  @keyframes siegel-progress-slide {
    0% { transform: translateX(-105%); }
    100% { transform: translateX(255%); }
  }

  /* Spinner representation: the host becomes an inline ring with no track
     background of its own; the SVG carries the track + arc circles. */
  :host([type="spinner"]) {
    display: inline-flex; height: auto; width: auto;
    background: none; border-radius: 0; overflow: visible;
  }
  .ring {
    width: var(--spinner-size, 28px); height: var(--spinner-size, 28px);
    transform: rotate(-90deg); /* start the arc at 12 o'clock */
  }
  .ring circle { fill: none; stroke-width: 9; }
  .ring .track { stroke: var(--control-border); }
  .ring .arc {
    stroke: var(--accent); stroke-linecap: round;
    transition: stroke-dashoffset 120ms linear;
  }
  /* No \`value\` ⇒ endless spin of a fixed partial arc. */
  :host([type="spinner"]:not([value])) .ring { transform: none; }
  @media (prefers-reduced-motion: no-preference) {
    :host([type="spinner"]:not([value])) .ring { animation: siegel-progress-spin 0.9s linear infinite; }
  }
  @media (prefers-reduced-motion: reduce) { .ring .arc { transition: none; } }
  @keyframes siegel-progress-spin { to { transform: rotate(360deg); } }
`;

export class SiegelProgress extends HTMLElement {
  static observedAttributes = ['value', 'type'];

  #type = null;   // the structure currently in the shadow root ('bar' | 'spinner')
  #renderQueued = false;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, STYLES);
  }

  // A 0..1 ratio, or null/NaN for indeterminate. Reflected to the `value`
  // attribute (absent ⇒ indeterminate), which also drives the CSS.
  set value(next) {
    const n = Number(next);
    if (next == null || next === '' || !Number.isFinite(n)) this.removeAttribute('value');
    else this.setAttribute('value', String(Math.max(0, Math.min(1, n))));
  }

  get value() {
    const raw = this.getAttribute('value');
    return raw == null ? null : Number(raw);
  }

  // "bar" (default) | "spinner". Reflected to the `type` attribute.
  set type(next) {
    if (next == null || next === 'bar') this.removeAttribute('type');
    else this.setAttribute('type', String(next));
  }

  get type() {
    return this.getAttribute('type') === 'spinner' ? 'spinner' : 'bar';
  }

  connectedCallback() {
    adoptPreUpgrade(this, ['value', 'type']);
    // Stamped rather than assumed, and only when the host has not named its own
    // role — the same rule <siegel-status> follows for role="img".
    if (!this.hasAttribute('role')) this.setAttribute('role', 'progressbar');
    this.setAttribute('aria-valuemin', '0');
    this.setAttribute('aria-valuemax', '1');
    this.render();
  }

  attributeChangedCallback(name, _old, _value) {
    if (name === 'value' || name === 'type') this.render();
  }

  render() {
    if (this.#renderQueued) return;
    this.#renderQueued = true;
    queueMicrotask(() => {
      this.#renderQueued = false;
      if (this.isConnected) this.#renderNow();
    });
  }

  // Build the shape's markup once per representation, then paint the value into it.
  #renderNow() {
    const type = this.type;
    if (type !== this.#type) {
      this.#type = type;
      this.shadowRoot.innerHTML = type === 'spinner'
        ? `<svg class="ring" part="ring" viewBox="0 0 100 100" aria-hidden="true">
             <circle class="track" cx="50" cy="50" r="${RING_R}"></circle>
             <circle class="arc" part="arc" cx="50" cy="50" r="${RING_R}"></circle>
           </svg>`
        : `<div class="bar" part="bar"></div>`;
    }

    const raw = this.getAttribute('value');
    const n = raw == null ? NaN : Number(raw);
    const determinate = Number.isFinite(n);
    const ratio = determinate ? Math.max(0, Math.min(1, n)) : NaN;

    if (type === 'spinner') this.#paintRing(determinate, ratio);
    else this.#paintBar(determinate, ratio);

    if (determinate) this.setAttribute('aria-valuenow', String(ratio));
    else this.removeAttribute('aria-valuenow');
  }

  #paintRing(determinate, ratio) {
    const arc = this.shadowRoot.querySelector('.arc');
    if (determinate) {
      // Full-circumference dasharray; hide the unfilled remainder via offset.
      arc.style.strokeDasharray = String(RING_C);
      arc.style.strokeDashoffset = String(RING_C * (1 - ratio));
    } else {
      // A fixed partial arc that the spin animation sweeps around the track.
      arc.style.strokeDasharray = `${(RING_C * RING_SWEEP).toFixed(2)} ${RING_C.toFixed(2)}`;
      arc.style.strokeDashoffset = '0';
    }
  }

  #paintBar(determinate, ratio) {
    const bar = this.shadowRoot.querySelector('.bar');
    // Empty string lets the indeterminate rule (40% + slide) take over.
    bar.style.width = determinate ? `${ratio * 100}%` : '';
  }
}

if (!customElements.get('siegel-progress')) customElements.define('siegel-progress', SiegelProgress);
