// Shared chip API: type="button|label", selected, disabled, action-start/end,
// action-start/end-label; camelCase properties reflect the action attributes.
// Body clicks never toggle automatically. chip-action { side } is a separate action.
// data-tab opts into button mode when type is absent; tabTarget exposes its native button.
// Explicit type chooses the same behavior in both kits; omitted type preserves legacy defaults.
// <siegel-chip> — a compact, non-interactive label or summary.
// Importing this module registers the element. Content wraps when space is tight.
//
//   <siegel-chip><span slot="label">Total Products:</span>28</siegel-chip>
//
// The optional label slot uses muted text; the default slot holds the content.
// Inherits Siegel density and theme. Use type="button" for selection and actions.

import { adoptStyles } from '../utilities/styles.js';
import { adoptPreUpgrade, emit } from '../utilities/element.js';

const STYLES = `
  :host {
    display: inline-flex; vertical-align: middle;
    box-sizing: border-box; min-width: 0; max-width: 100%;
    border: 1px solid var(--divider); border-radius: var(--ctl-radius);
    background: var(--control-bg); color: var(--text);
    font-size: 12px; line-height: 1.5; font-variant-numeric: tabular-nums;
  }
  :host([hidden]) { display: none; }
  .chip {
    display: flex; flex-wrap: wrap; align-items: center;
    gap: 0 var(--ctl-gap); min-width: 0;
    min-height: calc(var(--ctl-h) - 8px); box-sizing: border-box;
    padding: 2px var(--ctl-pad-x);
    overflow-wrap: anywhere;
  }
  slot[name="label"] { color: var(--text-muted); }
  .value { min-width: 0; }
  /* The host's 1px border sits outside this box, so the chip as a whole is --ctl-h. */
  .interactive { flex-wrap: nowrap; height: calc(var(--ctl-h) - 2px); padding: 0; gap: 0; }
  :host([selected]) { background: var(--panel); }
  :host([disabled]) { opacity: 0.5; }
  .body, .act { appearance: none; border: 0; background: transparent; color: inherit; font: inherit; margin: 0; cursor: pointer; }
  .body::before, .act::before { content: none; }
  .body { display: inline-flex; align-items: center; gap: var(--ctl-gap); flex: 1 1 auto; min-width: 0; height: 100%; padding: 0 var(--ctl-pad-x); border-radius: inherit; }
  .body:hover, .act:hover { background: var(--hover); }
  .body:disabled, .act:disabled { cursor: default; }
  .label { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .act { flex: none; width: var(--icon-btn-size); height: var(--icon-btn-size); padding: 0; margin: 0 var(--ctl-gap); }
  .glyph { display: inline-flex; }
  .glyph ::slotted(*), .act ::slotted(*) { width: var(--icon-size); height: var(--icon-size); }

`;

export class SiegelChip extends HTMLElement {
  static observedAttributes = ['type', 'variant', 'selected', 'disabled', 'data-tab',
    'action-start', 'action-end', 'action-start-label', 'action-end-label'];
  #shape = null;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, STYLES);
    this.shadowRoot.addEventListener('click', (event) => {
      const action = event.composedPath().find(node => node instanceof Element && node.matches('.act'));
      if (!action) return;
      event.stopPropagation();
      if (!this.disabled) emit(this, 'chip-action', { side: action.dataset.side });
    });
  }

  set type(value) { this.setAttribute('type', value === 'label' ? 'label' : 'button'); }
  get type() {
    const type = this.getAttribute('type');
    return type === 'label' || type === 'button' ? type : this.hasAttribute('data-tab') ? 'button' : 'label';
  }
  set selected(value) { this.toggleAttribute('selected', !!value); }
  get selected() { return this.hasAttribute('selected'); }
  set disabled(value) { this.toggleAttribute('disabled', !!value); }
  get disabled() { return this.hasAttribute('disabled'); }
  set actionStart(value) { this.toggleAttribute('action-start', !!value); }
  get actionStart() { return this.hasAttribute('action-start'); }
  set actionEnd(value) { this.toggleAttribute('action-end', !!value); }
  get actionEnd() { return this.hasAttribute('action-end'); }
  set actionStartLabel(value) { this.setAttribute('action-start-label', value ?? ''); }
  get actionStartLabel() { return this.getAttribute('action-start-label') || 'Action'; }
  set actionEndLabel(value) { this.setAttribute('action-end-label', value ?? ''); }
  get actionEndLabel() { return this.getAttribute('action-end-label') || 'Dismiss'; }
  get tabTarget() { return this.shadowRoot.querySelector('button.body'); }
  focus(options) { this.tabTarget?.focus(options); }

  connectedCallback() {
    adoptPreUpgrade(this, ['type', 'selected', 'disabled', 'actionStart', 'actionEnd', 'actionStartLabel', 'actionEndLabel']);
    this.render();
  }
  attributeChangedCallback() { if (this.isConnected) this.render(); }

  // State paints synchronously in place; only mode or action placement changes structure.
  render() {
    const shape = `${this.type}:${this.actionStart}:${this.actionEnd}`;
    if (shape !== this.#shape) this.#build(shape);
    this.#dress();
  }
  #build(shape) {
    const focused = this.shadowRoot.activeElement;
    const focusSide = focused?.dataset.side;
    const hadFocus = !!focused;
    this.#shape = shape;
    if (this.type === 'label') {
      this.shadowRoot.innerHTML = '<span class="chip static" part="chip"><slot name="label"></slot><span class="value"><slot></slot></span></span>';
    } else {
      const action = side => `<button class="act ${side}" type="button" data-side="${side}"><slot name="${side}"></slot></button>`;
      const glyph = side => `<span class="glyph ${side}" aria-hidden="true"><slot name="${side}"></slot></span>`;
      this.shadowRoot.innerHTML = `<div class="chip interactive" part="chip">
        ${this.actionStart ? action('start') : ''}
        <button class="body" type="button">
          ${this.actionStart ? '' : glyph('start')}
          <span class="label"><slot name="label"></slot><slot></slot></span>
          ${this.actionEnd ? '' : glyph('end')}
        </button>
        ${this.actionEnd ? action('end') : ''}
      </div>`;
    }
    this.#dress();
    emit(this, 'tab-target-change');
    if (hadFocus) (this.shadowRoot.querySelector(focusSide ? `.act.${focusSide}` : '.body') ?? this.tabTarget)?.focus();
  }
  #dress() {
    const body = this.tabTarget;
    if (!body) return;
    body.disabled = this.disabled;
    if (this.hasAttribute('data-tab')) body.removeAttribute('aria-pressed');
    else body.setAttribute('aria-pressed', String(this.selected));
    for (const side of ['start', 'end']) {
      const action = this.shadowRoot.querySelector(`.act.${side}`);
      if (!action) continue;
      action.disabled = this.disabled;
      action.setAttribute('aria-label', side === 'start' ? this.actionStartLabel : this.actionEndLabel);
      if (this.hasAttribute('data-tab')) action.tabIndex = this.selected ? 0 : -1;
      else action.removeAttribute('tabindex');
    }
  }
}

if (!customElements.get('siegel-chip')) customElements.define('siegel-chip', SiegelChip);
