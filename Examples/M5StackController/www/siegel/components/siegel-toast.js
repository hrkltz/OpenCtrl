// Non-blocking notifications: importing this module registers both elements.
//
// <siegel-toast> owns the stack and safe viewport placement. <siegel-toast-item>
// owns its lifetime; neither draws a surface or replaces the content you supply.
//
//   <siegel-toast>
//     <siegel-toast-item duration="0">
//       <siegel-panel appearance="float">
//         <siegel-panel-cell>Saved. <button>Undo</button></siegel-panel-cell>
//       </siegel-panel>
//     </siegel-toast-item>
//   </siegel-toast>
//
// Group: show(nodeOrText, { kind, duration }) returns the new item. Directly
// appending items works too. --toast-width (360px), --toast-gap (8px) and
// --toast-offset (16px) control geometry. The app frame's safe-area="auto|none"
// and --safe-area-* contract also applies here. Native manual popovers escape
// clipping ancestors; older browsers fall back to a fixed, elevated stack.
//
// Item: duration attribute/property is milliseconds (default 4000, 0 is sticky).
// kind is info | success | error; errors announce assertively, others politely.
// Hover, keyboard focus, hidden state and disconnection pause the countdown.
// pause()/resume() control an additional manual pause. dismiss() removes the item
// and emits toast-dismiss { reason: 'manual' | 'timeout' } before removal.
//
// toast({ message, kind, duration }) remains the document-level convenience API:
// it composes a floating panel with text and a close button in the first document
// <siegel-toast> (creating one if needed), and returns a dismiss() function.
// Use group.show() or markup when the application owns the content and actions.

import { adoptStyles, buttonStyles } from '../utilities/styles.js';
import { adoptPreUpgrade, emit, injectLightStyles } from '../utilities/element.js';
import { SAFE_AREA_STYLES } from '../utilities/safe-area.js';
import { ICON_CLOSE } from '../utilities/icons.js';
import { t } from '../utilities/strings.js';
import './siegel-panel.js';

const DEFAULT_DURATION = 4000;
const KINDS = ['info', 'success', 'error'];
const SYNC = Symbol('toast-sync');
const normalizeDuration = value => {
  const number = value == null ? DEFAULT_DURATION : Number(value);
  return Number.isFinite(number) ? Math.min(2147483647, Math.max(0, number)) : DEFAULT_DURATION;
};

const TOAST_STYLES = `
  ${SAFE_AREA_STYLES}
  :host {
    display: none; position: fixed; inset: auto; margin: 0;
    right: calc(var(--_safe-area-right) + var(--toast-offset, 16px));
    bottom: calc(var(--_safe-area-bottom) + var(--toast-offset, 16px));
    width: min(var(--toast-width, 360px), max(0px, calc(100dvw - var(--_safe-area-left)
      - var(--_safe-area-right) - 2 * var(--toast-offset, 16px))));
    max-height: max(0px, calc(100dvh - var(--_safe-area-top)
      - var(--_safe-area-bottom) - 2 * var(--toast-offset, 16px)));
    /* Room for the slotted surface's shadow, also when the stack must scroll. */
    padding: var(--toast-gap, 8px); border: 0; background: transparent; color: inherit;
    flex-direction: column; gap: var(--toast-gap, 8px);
    overflow: auto; overscroll-behavior: contain;
    z-index: var(--z-toast, 40); pointer-events: none;
  }
  :host([data-open]) { display: flex; }
  :host::backdrop { background: transparent; pointer-events: none; }
  slot { display: contents; }
`;

const ITEM_STYLES = `
  :host {
    display: block; flex: 0 0 auto; min-width: 0; width: 100%;
    pointer-events: auto; overflow-wrap: anywhere;
  }
  ::slotted(*) { max-width: 100%; }
  @media (prefers-reduced-motion: no-preference) {
    :host { animation: toast-enter 160ms ease-out; }
  }
  @keyframes toast-enter {
    from { opacity: 0; transform: translateY(8px); }
    to { opacity: 1; transform: none; }
  }
`;

// Only the convenience helper supplies presentation. Its content lives in the
// document, so these rules use the same scoped injection as the modal helpers.
const MESSAGE_STYLES = `
  .siegel-toast-panel { --cell-axis: row; --cell-align: center; }
  .siegel-toast-panel[data-kind="success"] { border-color: var(--good); }
  .siegel-toast-panel[data-kind="error"] { border-color: var(--error); }
  .siegel-toast-message { flex: 1 1 auto; min-width: 0; overflow-wrap: anywhere; }
  ${buttonStyles(['.siegel-toast-dismiss'])}
  .siegel-toast-dismiss:focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; }
`;

function messagePanel(message, kind) {
  injectLightStyles('toast-message', MESSAGE_STYLES);
  const panel = document.createElement('siegel-panel');
  panel.setAttribute('appearance', 'float');
  panel.className = 'siegel-toast-panel';
  panel.dataset.kind = kind;
  const cell = document.createElement('siegel-panel-cell');
  const text = document.createElement('span');
  text.className = 'siegel-toast-message';
  text.textContent = message == null ? '' : String(message);
  const close = document.createElement('button');
  close.type = 'button';
  close.className = 'siegel-toast-dismiss icon-btn';
  close.setAttribute('aria-label', t('toast.dismiss', 'Dismiss'));
  close.innerHTML = ICON_CLOSE;
  close.addEventListener('click', () => panel.closest('siegel-toast-item')?.dismiss());
  cell.append(text, close);
  panel.append(cell);
  return panel;
}

export function toast({ message, kind = 'info', duration = DEFAULT_DURATION } = {}) {
  let group = document.querySelector('siegel-toast');
  if (!group) {
    group = document.createElement('siegel-toast');
    document.body.append(group);
  }
  const safeKind = KINDS.includes(kind) ? kind : 'info';
  const item = group.show(messagePanel(message, safeKind), { kind: safeKind, duration });
  return () => item.dismiss();
}

// ===========================================================================
// <siegel-toast> — placement and stacking, without a notification surface.
// ===========================================================================
export class SiegelToast extends HTMLElement {
  static observedAttributes = ['hidden'];

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, TOAST_STYLES);
    this.shadowRoot.innerHTML = '<slot></slot>';
    this.shadowRoot.querySelector('slot').addEventListener('slotchange', () => this[SYNC]());
  }

  show(content, { kind = 'info', duration = DEFAULT_DURATION } = {}) {
    const item = document.createElement('siegel-toast-item');
    item.kind = kind;
    item.duration = duration;
    // append() preserves nodes and their handlers; strings remain plain text.
    item.append(content);
    this.append(item);
    return item;
  }

  connectedCallback() {
    if (!this.hasAttribute('role')) this.setAttribute('role', 'region');
    if (!this.hasAttribute('aria-label')) this.setAttribute('aria-label', t('toast.notifications', 'Notifications'));
    this.setAttribute('popover', 'manual');
    this[SYNC]();
  }

  attributeChangedCallback() { this[SYNC](); }

  [SYNC]() {
    const items = [...this.children].filter(child => child instanceof SiegelToastItem);
    const open = this.isConnected && !this.hidden && items.some(item => !item.hidden);
    this.toggleAttribute('data-open', open);
    // The CSS open state also works when the Popover API is missing or refuses
    // to open (for example in an inactive document).
    try {
      if (open) this.showPopover?.();
      else this.hidePopover?.();
    } catch { /* fixed-position fallback */ }
    for (const item of items) item[SYNC]();
    if (open && !this.matches(':focus-within')) this.scrollTop = this.scrollHeight;
  }
}

// ===========================================================================
// <siegel-toast-item> — arbitrary slotted content with a managed lifetime.
// ===========================================================================
export class SiegelToastItem extends HTMLElement {
  static observedAttributes = ['duration', 'kind', 'hidden'];

  #timer = null;
  #announcementTimer = null;
  #remaining = DEFAULT_DURATION;
  #started = 0;
  #hovered = false;
  #focused = false;
  #paused = false;
  #dismissed = false;
  #group = null;
  #returnFocus = null;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, ITEM_STYLES);
    this.shadowRoot.innerHTML = '<slot></slot>';
    this.addEventListener('mouseenter', () => { this.#hovered = true; this[SYNC](); });
    this.addEventListener('mouseleave', () => { this.#hovered = false; this[SYNC](); });
    this.addEventListener('focusin', event => {
      if (!this.contains(event.relatedTarget)) this.#returnFocus = event.relatedTarget;
      this.#focused = true;
      this[SYNC]();
    });
    this.addEventListener('focusout', () => queueMicrotask(() => {
      this.#focused = this.matches(':focus-within');
      this[SYNC]();
    }));
  }

  get duration() { return normalizeDuration(this.getAttribute('duration')); }
  set duration(value) {
    const duration = normalizeDuration(value);
    if (this.getAttribute('duration') !== String(duration)) {
      this.setAttribute('duration', String(duration));
      return;
    }
    this.#stopTimer();
    this.#remaining = duration;
    this[SYNC]();
  }

  get kind() { return KINDS.includes(this.getAttribute('kind')) ? this.getAttribute('kind') : 'info'; }
  set kind(value) {
    const kind = KINDS.includes(value) ? value : 'info';
    if (this.getAttribute('kind') !== kind) {
      this.setAttribute('kind', kind);
      return;
    }
    this.#syncAnnouncement();
  }

  pause() { this.#paused = true; this[SYNC](); }
  resume() { this.#paused = false; this[SYNC](); }

  dismiss(reason = 'manual') {
    if (this.#dismissed) return;
    this.#dismissed = true;
    this.#stopTimer();
    const restoreFocus = this.matches(':focus-within') ? this.#returnFocus : null;
    emit(this, 'toast-dismiss', { reason });
    this.remove();
    if (restoreFocus?.isConnected) restoreFocus.focus?.({ preventScroll: true });
  }

  connectedCallback() {
    adoptPreUpgrade(this, ['duration', 'kind']);
    if (this.#dismissed) this.#remaining = this.duration;
    this.#dismissed = false;
    this.#group = this.parentElement instanceof SiegelToast ? this.parentElement : null;
    this.#syncAnnouncement();
    // Establish the empty live region before exposing its content to AT. The
    // slot stays rendered, so arbitrary children keep their layout and focus.
    const slot = this.shadowRoot.querySelector('slot');
    slot.setAttribute('aria-hidden', 'true');
    this.#announcementTimer = setTimeout(() => {
      this.#announcementTimer = null;
      slot.removeAttribute('aria-hidden');
    }, 0);
    this[SYNC]();
    this.#group?.[SYNC]();
  }

  disconnectedCallback() {
    this.#stopTimer();
    clearTimeout(this.#announcementTimer);
    this.#announcementTimer = null;
    this.#hovered = false;
    this.#focused = false;
    const group = this.#group;
    this.#group = null;
    group?.[SYNC]();
  }

  attributeChangedCallback(name, _old, value) {
    if (name === 'duration') this.duration = value;
    else if (name === 'kind') this.kind = value;
    else {
      this[SYNC]();
      this.#group?.[SYNC]();
    }
  }

  [SYNC]() {
    this.#stopTimer();
    if (!this.isConnected || this.#dismissed || this.duration === 0 || this.hidden
      || this.#group?.hidden || this.#hovered || this.#focused || this.#paused) return;
    this.#started = performance.now();
    this.#timer = setTimeout(() => {
      this.#timer = null;
      this.#remaining = 0;
      this.dismiss('timeout');
    }, this.#remaining);
  }

  #stopTimer() {
    if (this.#timer == null) return;
    clearTimeout(this.#timer);
    this.#timer = null;
    this.#remaining = Math.max(0, this.#remaining - (performance.now() - this.#started));
  }

  #syncAnnouncement() {
    const assertive = this.kind === 'error';
    this.setAttribute('role', assertive ? 'alert' : 'status');
    this.setAttribute('aria-live', assertive ? 'assertive' : 'polite');
    this.setAttribute('aria-atomic', 'true');
  }
}

if (!customElements.get('siegel-toast')) customElements.define('siegel-toast', SiegelToast);
if (!customElements.get('siegel-toast-item')) customElements.define('siegel-toast-item', SiegelToastItem);
