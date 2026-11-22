// <siegel-select> — an options-only chooser, matching Klotz's selection API.
// Importing this module also registers <siegel-select-item> and <siegel-select-filter>.
//
//   <siegel-select aria-label="View">
//     <siegel-select-item value="grid" selected>Grid</siegel-select-item>
//     <siegel-select-item value="list">List</siegel-select-item>
//   </siegel-select>
//
// options: [{id, label, icon?, hint?, endIcon?, color?, disabled?, badge?}], or direct
// item children (which take precedence). Items accept value, selected, disabled, hint,
// color and leading/trailing slots. Their selected attribute supplies initial state.
// badge: true draws an accent dot in the cell's corner; it is visual only, so state
// its meaning in the label or hint as well.
// stretch: a horizontal strip fills its host's width and every cell takes an equal
// share, icon-only cells included. Labels that no longer fit ellipsize.
// value: scalar for single selection, an array with multiple; setters are silent.
// select(id): select/toggle and emit select-change {id} or {ids} on change.
// selection-change {selected, count}: legacy event, including single re-picks so
// existing navigation rails can still collapse their column on a second press.
// orientation="vertical", icon-only, align-labels, multiple, group and disabled
// configure the strip; its size follows the shared control geometry. focus()
// enters its roving tab stop. Grouped single selects share one selection per root.
// Arrows/Home/End select single options; in multiple mode they move focus only.
// Enter/Space toggle a multiple option. Single selection supports press-and-slide
// with a mouse or pen, which ends when the strip stops being rendered or its
// top-layer surface closes. A touch pointer commits on the tap, not on the press.
// filter: case-insensitive substring of label/id; shown returns the visible options.
// Filtering preserves both the full list and its selection.
//
// Compact presentation is a native button -> siegel-popout -> siegel-panel ->
// vertical select composition. The consumer owns the trigger label and closing.
// There is no built-in type, placeholder, align, open or close().
// A select-filter's for targets a select in the same root; value/disabled are
// properties, placeholder/aria-label are forwarded to its native search input.
// Typing emits filter-change {value, shown}; ArrowDown/Enter focus the results.
// Retargeting or disconnecting releases the filter on its previous target.

import { adoptStyles } from '../utilities/styles.js';
import { escapeHtml, escapeAttr } from '../utilities/html.js';
import { adoptPreUpgrade, emit } from '../utilities/element.js';
import { ICON_CHECK } from '../utilities/icons.js';

// Dataset ids are strings; retain the caller's original id type in values/events.
function sameId(a, b) {
  if (a === b) return true;
  if (a == null || b == null) return false;
  return String(a) === String(b);
}

const STYLES = `
  :host { display: block; min-width: 0; max-width: 100%; -webkit-user-select: none; user-select: none; }
  button::before { content: none; }
  .slot:focus-visible { outline: 2px solid var(--text-muted); outline-offset: -2px; }
  .strip {
    display: flex; max-width: 100%;
    line-height: 1.5;
    --cell-h: var(--ctl-h, 40px);
    /* NO touch-action HERE ANY MORE. It was \`none\`, which is what a strip claiming
       the whole touch gesture for press-and-slide needs — and it is also what stopped
       a finger from ever scrolling a long option list, since the browser was told no
       pan could start on these cells. A finger commits on the tap now (#onDown), so
       the gesture is the browser's to interpret and panning is its business. */
  }

  .slot {
    height: auto;
    appearance: none; -webkit-appearance: none;
    flex: 1 1 auto; min-width: 0; min-height: var(--cell-h);
    display: inline-flex; align-items: center; justify-content: center; gap: var(--ctl-gap);
    margin: 0; padding: var(--ctl-pad-y) var(--ctl-pad-x);
    border: 1px solid var(--control-border);
    background: transparent; color: var(--text-muted);
    font: inherit; font-size: var(--ctl-font-size, inherit); line-height: inherit; cursor: pointer;
  }
  .slot svg { display: block; flex: none; width: var(--icon-size); height: var(--icon-size); }
  .slot .label { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }

  .slot[style] { color: var(--option-color); }

  .slot[aria-checked="true"] {
    background: var(--panel);
    color: var(--option-color, var(--control-pressed-foreground));
  }
  .slot[aria-checked="true"] :is(.label, .hint, svg) { color: inherit; }
  @media (hover: hover) {
    .slot:where(:not(:disabled)):hover {
      background: var(--panel);
      color: var(--option-color, var(--control-pressed-foreground));
    }
    .slot:where(:not(:disabled)):hover :is(.label, .hint, svg) { color: inherit; }
  }
  .slot:where(:not(:disabled)):active {
    background: transparent;
    color: var(--option-color, var(--control-pressed-foreground));
  }
  .slot:where(:not(:disabled)):active :is(.label, .hint, svg) { color: inherit; }

  :host([orientation="vertical"]) .strip { flex-direction: column; }
  :host([orientation="vertical"]) .slot { flex: none; max-width: 100%; justify-content: flex-start; }

  .strip.icons-only { width: max-content; max-width: 100%; }
  :host([orientation="vertical"]) .strip.icons-only:not(.ends) { width: var(--cell-h); }
  .strip.icons-only:not(.ends) .slot {
    flex: none; width: var(--cell-h); padding-left: 0; padding-right: 0;
    justify-content: center;
  }

  .strip {
    --strip-radius: calc(var(--ctl-radius) + var(--select-rail-pad, 4px) - 1px);
    padding: 0; gap: 0;
    border: 1px solid var(--control-border);
    border-radius: var(--strip-radius);
    background: var(--page);
  }
  .strip .slot[aria-checked] {
    min-height: 0; height: calc(var(--cell-h) - 2px);
    padding-block: 0;
    border: 0; border-radius: 0;
  }
  .strip .slot:first-child {
    border-top-left-radius: calc(var(--strip-radius) - 1px);
    border-bottom-left-radius: calc(var(--strip-radius) - 1px);
  }
  .strip .slot:last-child {
    border-top-right-radius: calc(var(--strip-radius) - 1px);
    border-bottom-right-radius: calc(var(--strip-radius) - 1px);
  }
  :host([orientation="vertical"]) .strip .slot { border-radius: 0; }
  :host([orientation="vertical"]) .strip .slot:first-child {
    border-top-left-radius: calc(var(--strip-radius) - 1px);
    border-top-right-radius: calc(var(--strip-radius) - 1px);
  }
  :host([orientation="vertical"]) .strip .slot:last-child {
    border-bottom-left-radius: calc(var(--strip-radius) - 1px);
    border-bottom-right-radius: calc(var(--strip-radius) - 1px);
  }
  .strip.icons-only:not(.ends) .slot[aria-checked] {
    width: calc(var(--cell-h) - 2px);
  }

  .strip.icons-only .slot .label {
    position: absolute; width: 1px; height: 1px;
    overflow: hidden; clip-path: inset(50%); white-space: nowrap;
  }

  .strip.dragging .slot { pointer-events: none; }

  :host([disabled]) .strip { pointer-events: none; opacity: 0.5; }

  .row { display: flex; flex-wrap: nowrap; align-items: center; justify-content: center; gap: var(--ctl-gap); min-width: 0; width: 100%; }
  :host([orientation="vertical"]) .row { justify-content: flex-start; }
  .strip.icons-only .row { justify-content: center; }
  .glyph { flex: none; display: flex; align-items: center; }
  .glyph:empty { display: none; }
  :host([align-labels][orientation="vertical"]) .strip.has-icons .glyph:empty {
    display: block; width: var(--icon-size); height: var(--icon-size);
  }
  .end { margin-inline-start: auto; flex: none; display: grid; align-items: center; color: var(--text-muted); }
  .end > * { grid-area: 1 / 1; }
  .hint { font-size: 12px; }
  .tick { display: inline-flex; color: inherit; }
  .strip.ticks .slot:not([aria-checked="true"]) .tick,
  .strip.ticks .slot[aria-checked="true"] .end > :not(.tick) { visibility: hidden; }
  .strip.icons-only.ends .slot { flex: none; width: auto; }

  /* stretch: equal shares of the host's width. An equal non-zero basis rather than
     \`flex: 1\`, so the shares stay equal without a zero basis (SCANME §8.2, rule 11). */
  :host([stretch]:not([orientation="vertical"])) .strip { width: 100%; }
  :host([stretch]:not([orientation="vertical"])) .strip .slot,
  :host([stretch]:not([orientation="vertical"])) .strip.icons-only .slot,
  :host([stretch]:not([orientation="vertical"])) .strip.icons-only:not(.ends) .slot[aria-checked] {
    flex: 1 1 var(--cell-h); width: auto; min-width: 0;
  }

  .slot { position: relative; }
  .badge {
    position: absolute; top: 4px; right: 4px;
    width: 8px; height: 8px; border-radius: 50%;
    background: var(--accent); pointer-events: none;
  }
  .slot:disabled { opacity: 0.5; cursor: default; }
  :host([disabled]) .slot { opacity: 1; }
  .slot[data-pressed] { background: transparent; color: var(--option-color, var(--control-pressed-foreground)); }
  .strip.dragging .slot:not([data-pressed]):not([aria-checked="true"]) { background: transparent; color: var(--option-color, var(--text-muted)); }
`;

const SELECT_ITEM_STYLES = `
  :host { display: none; }
`;

const SELECT_FILTER_STYLES = `
  :host { display: block; width: 100%; min-width: 0; }
  input { display: block; width: 100%; }
`;

export class SiegelSelect extends HTMLElement {
  static observedAttributes = ['disabled', 'orientation', 'icon-only', 'multiple', 'group'];

  #options = [];
  #value = null;
  #filter = '';
  #fromItems = false; // Declarative children own the option list while present.
  #declared = false; // Read selected attributes once; later edits preserve live state.
  #watch = null;
  #renderQueued = false;

  #strip = null;
  #chosenByPointer = false; // Consume the click following a pointer pick.
  #dragging = false;
  #pointer = null; // The captured pointer, so #endDrag can give it back.
  // The top-layer surface this gesture is running on, if any — see #onMove.
  #surface = null;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, STYLES);
    this.shadowRoot.innerHTML = '<div class="strip"></div>';
    this.#strip = this.shadowRoot.querySelector('.strip');
    this.shadowRoot.addEventListener('click', (event) => {
      if (this.#chosenByPointer) { this.#chosenByPointer = false; return; }
      const cell = event.target.closest?.('.slot');
      if (cell && !cell.disabled) this.select(this.#idOf(cell.dataset.id));
    });
    this.shadowRoot.addEventListener('keydown', (event) => this.#onKey(event));
    this.shadowRoot.addEventListener('pointerdown', (event) => {
      this.#chosenByPointer = false;
      this.#onDown(event);
    });
    this.shadowRoot.addEventListener('pointermove', (event) => this.#onMove(event));
    this.shadowRoot.addEventListener('pointerup', () => this.#endDrag());
    this.shadowRoot.addEventListener('pointercancel', () => this.#endDrag());
  }
  set options(value) {
    if (this.#fromItems) return;
    this.#options = Array.isArray(value) ? value : [];
    this.#adoptFirst();
    this.render();
  }
  get options() { return this.#options; }

  set value(id) {
    const list = id == null ? [] : Array.isArray(id) ? id : [id];
    this.#value = this.multiple ? [...list] : list[0] ?? null;
    this.#adoptFirst();
    if (this.#value !== null) this.#claim();
    this.#paintValue();
  }
  get value() { return Array.isArray(this.#value) ? [...this.#value] : this.#value; }
  set filter(query) {
    this.#filter = query == null ? '' : String(query);
    this.render();
  }
  get filter() { return this.#filter; }
  get shown() { return this.#shown(); }

  #shown() {
    const query = this.#filter.trim().toLowerCase();
    if (!query) return this.#options;
    return this.#options.filter((option) => String(option.label ?? '').toLowerCase().includes(query)
      || String(option.id).toLowerCase().includes(query));
  }
  get multiple() { return this.hasAttribute('multiple'); }
  set multiple(value) { this.toggleAttribute('multiple', !!value); }
  get iconOnly() { return this.hasAttribute('icon-only'); }
  set iconOnly(value) { this.toggleAttribute('icon-only', !!value); }
  get stretch() { return this.hasAttribute('stretch'); }
  set stretch(value) { this.toggleAttribute('stretch', !!value); }
  #selected(id) { return (Array.isArray(this.#value) ? this.#value : [this.#value]).some(value => sameId(value, id)); }
  #announce(changed = true) {
    const ids = Array.isArray(this.#value) ? [...this.#value] : this.#value == null ? [] : [this.#value];
    if (changed) emit(this, 'select-change', this.multiple ? { ids } : { id: this.#value });
    emit(this, 'selection-change', { selected: ids, count: ids.length });
  }

  set disabled(value) { this.toggleAttribute('disabled', !!value); }
  get disabled() { return this.hasAttribute('disabled'); }

  get orientation() { return this.getAttribute('orientation') === 'vertical' ? 'vertical' : 'horizontal'; }
  set orientation(value) { this.setAttribute('orientation', value === 'vertical' ? 'vertical' : 'horizontal'); }
  select(id) {
    const option = this.#options.find(option => sameId(option.id, id));
    if (this.disabled || !option || option.disabled) return;
    id = option.id;
    if (this.multiple) {
      const ids = Array.isArray(this.#value) ? this.#value : this.#value == null ? [] : [this.#value];
      this.#value = this.#selected(id) ? ids.filter(value => !sameId(value, id)) : [...ids, id];
      this.#paintValue();
      this.#announce();
      return;
    }
    const changed = !sameId(id, this.#value);
    this.#value = id;
    if (changed) { this.#claim(); this.#paintValue(); }
    this.#announce(changed);
  }

  focus(options) { this.#strip?.querySelector('.slot[tabindex="0"]')?.focus(options); }

  connectedCallback() {
    adoptPreUpgrade(this, ['multiple']);
    this.#syncRole();
    adoptPreUpgrade(this, ['options', 'value', 'filter', 'disabled', 'orientation', 'iconOnly', 'stretch']);
    this.#watchItems();
    this.#syncItems();
    this.render();
  }

  disconnectedCallback() {
    this.#endDrag();
    this.#watch?.disconnect();
    this.#watch = null;
  }

  attributeChangedCallback(name, _old, value) {
    if (name === 'multiple') {
      this.#syncRole();
      // A pre-upgrade value still belongs to its pending property adoption.
      if (!Object.hasOwn(this, 'value')) this.value = this.#value;
      this.render();
      return;
    }
    this.render();
  }
  #syncRole() { this.setAttribute('role', this.multiple ? 'group' : 'radiogroup'); }
  #watchItems() {
    if (this.#watch) return;
    this.#watch = new MutationObserver((records) => {
      if (records.every((record) => record.type === 'attributes' && record.target === this)) return;
      this.#syncItems();
    });
    this.#watch.observe(this, {
      childList: true, subtree: true, characterData: true, attributes: true,
    });
  }
  #itemElements() {
    customElements.upgrade(this);
    return [...this.children].filter((el) => el.localName === 'siegel-select-item');
  }
  #syncItems() {
    const items = this.#itemElements();
    const had = this.#fromItems;
    this.#fromItems = items.length > 0;
    if (this.#fromItems) {
      const options = items.map((item) => item.option);
      this.#options = options;
      if (!this.#declared) {
        this.#declared = true;
        this.#adoptDeclared(items, options);
      }
    } else if (had) {
      this.#options = [];
    } else {
      return;
    }
    this.#adoptFirst();
    this.render();
  }
  #adoptDeclared(items, options) {
    const held = Array.isArray(this.#value) ? this.#value.length > 0 : this.#value != null;
    if (held) return;
    const picked = options.filter((_, i) => items[i].hasAttribute('selected')).map((option) => option.id);
    if (!picked.length) return;
    this.#value = this.multiple ? picked : picked[0];
  }
  #adoptFirst() {
    if (!this.#options.length) return;
    if (this.multiple) return;
    if (this.hasAttribute('group')) return;
    if (!this.#options.some((option) => sameId(option.id, this.#value))) {
      this.#value = (this.#options.find((option) => !option.disabled) ?? this.#options[0]).id;
    }
  }
  render() {
    if (this.#renderQueued) return;
    this.#renderQueued = true;
    queueMicrotask(() => {
      this.#renderQueued = false;
      if (this.isConnected) this.#renderNow();
    });
  }

  #paintColors(nodes, options) {
    options.forEach((option, i) => {
      if (option.color) nodes[i]?.style.setProperty('--option-color', option.color);
    });
  }
  #renderNow() {
    const shown = this.#shown();
    const anyIcon = this.#options.some((o) => o.icon);
    const iconsOnly = anyIcon && this.hasAttribute('icon-only');
    const ticks = this.multiple;
    const ends = ticks || this.#options.some((option) => option.hint || option.endIcon);
    const anyGlyph = anyIcon || ticks || this.#options.some((option) => option.endIcon);
    this.#strip.classList.toggle('has-icons', anyGlyph);
    this.#strip.classList.toggle('ends', ends);
    this.#strip.classList.toggle('ticks', ticks);
    this.#strip.classList.toggle('icons-only', iconsOnly);
    this.#strip.classList.toggle('empty', !shown.length);

    this.#strip.innerHTML = shown.map((option) => {
      const on = this.#selected(option.id);
      const label = escapeHtml(option.label ?? option.id);
      const content = option.endIcon
        ?? (option.hint ? `<span class="hint">${escapeHtml(option.hint)}</span>` : '');
      const end = content || ticks
        ? `<span class="end">${content}${ticks ? `<span class="tick" aria-hidden="true">${ICON_CHECK}</span>` : ''}</span>`
        : '';
      return `<button class="slot" type="button" role="${this.multiple ? 'checkbox' : 'radio'}"
                data-id="${escapeAttr(String(option.id))}" ${this.disabled || option.disabled ? 'disabled aria-disabled="true"' : ''}
                aria-checked="${on}" tabindex="-1">${option.badge ? '<span class="badge" aria-hidden="true"></span>' : ''}
                <span class="row">${anyIcon ? `<span class="glyph">${option.icon ?? ''}</span>` : ''}<span class="label">${label}</span>${end}</span>
              </button>`;
    }).join('');
    this.#paintColors(this.#strip.querySelectorAll('.slot'), shown);
    this.#restStop();
  }
  #paintValue() {
    if (!this.#strip) return;
    for (const cell of this.#strip.querySelectorAll('.slot')) {
      cell.setAttribute('aria-checked', String(this.#selected(cell.dataset.id)));
    }
    this.#restStop();
  }
  #restStop() {
    const open = [...this.#strip.querySelectorAll('.slot:not(:disabled)')];
    if (!open.length) return;
    const stop = (this.multiple
      ? open.find((cell) => cell.tabIndex === 0)
      : open.find((cell) => cell.getAttribute('aria-checked') === 'true')) ?? open[0];
    for (const cell of this.#strip.querySelectorAll('.slot')) {
      cell.tabIndex = cell === stop ? 0 : -1;
    }
  }
  #claim() {
    const group = this.getAttribute('group');
    if (group === null) return;
    if (this.multiple) return;
    const root = this.getRootNode();
    if (!root.querySelectorAll) return;
    for (const other of root.querySelectorAll(`siegel-select[group="${CSS.escape(group)}"]`)) {
      if (other !== this && !other.multiple) other.#clear();
    }
  }

  #clear() {
    if (this.#value === null) return;
    this.#value = null;
    this.#paintValue();
  }
  #idOf(datasetId) {
    const option = this.#options.find((o) => String(o.id) === datasetId);
    return option ? option.id : datasetId;
  }

  #onKey(event) {
    const cell = event.target.closest?.('.slot');
    if (cell) this.#onStripKey(event, cell);
  }
  #onStripKey(event, current) {
    if (this.disabled) return;
    const cells = [...this.#strip.querySelectorAll('.slot:not(:disabled)')];
    if (!cells.length) return;
    const i = cells.indexOf(current);
    let next = null;
    switch (event.key) {
      case 'ArrowRight': case 'ArrowDown': next = cells[(i + 1) % cells.length]; break;
      case 'ArrowLeft': case 'ArrowUp': next = cells[(i - 1 + cells.length) % cells.length]; break;
      case 'Home': next = cells[0]; break;
      case 'End': next = cells[cells.length - 1]; break;
      case 'Enter': case ' ':
        event.preventDefault();
        this.select(this.#idOf(current.dataset.id));
        return;
      default: return;
    }
    event.preventDefault();
    if (this.multiple) {
      for (const cell of this.#strip.querySelectorAll('.slot')) {
        cell.tabIndex = cell === next ? 0 : -1;
      }
    } else {
      this.select(this.#idOf(next.dataset.id));
    }
    next.focus();
  }

  #onDown(event) {
    if (this.disabled || event.button !== 0) return;
    if (!event.target.closest?.('.strip')) return;
    // A FINGER COMMITS ON THE TAP, NOT ON THE TOUCH DOWN, and the click listener
    // above is what then does the work — #chosenByPointer stays false, so nothing
    // swallows it. Two things make a touch pointer different from a mouse, and both
    // of them are decided at touch END rather than at touch start:
    //
    //   A TAP CANNOT BE TOLD FROM A SCROLL until the finger lifts. Committing on the
    //   press means a list that cannot be scrolled by dragging it: the first touch
    //   picks. A dropdown of forty devices is exactly that list.
    //
    //   AND THE CLICK ARRIVES SOMEWHERE ELSE. After `touchend` the browser sends the
    //   compatibility mouse events — including `click` — at whatever is under the
    //   finger AT THAT MOMENT. Commit on the press and the compact composition hides
    //   its Popout before the finger lifts, so that click lands on the page beneath:
    //   the option was chosen AND whatever sat behind the panel was pressed too.
    //   Nothing in here could swallow it, because by then it is not ours.
    //
    // Press-and-slide is a MOUSE gesture for that reason, and stays one. A pen is
    // precise and hovers, so it slides too, and its click goes astray the same way
    // on an iPad: the APPLE PENCIL's click is also sent to what is under the tip as
    // it lifts. The kit's press guard stops that one (utilities/press.js, held by
    // the popout, the popover and the frame); only a finger waits for the tap.
    if (event.pointerType === 'touch') return;
    if (this.multiple) {
      const cell = event.target.closest('.slot');
      if (cell && !cell.disabled) {
        this.#chosenByPointer = true;
        this.select(this.#idOf(cell.dataset.id));
      }
      return;
    }
    this.#chosenByPointer = true;
    this.#dragging = true;
    this.#surface = this.#floatingSurface();
    this.#strip.classList.add('dragging');
    this.#pointer = event.pointerId;
    this.#strip.setPointerCapture(event.pointerId);
    this.#pickAt(event, true);
  }

  // A GESTURE CANNOT OUTLIVE THE SURFACE IT IS ON, and one routinely tries to.
  // The compact composition (SCANME §5.5) hides the Popout the moment a single
  // pick is announced — and that announcement happens on `pointerdown`, from
  // inside #onDown, while this press is still running. The pointer is CAPTURED
  // on the strip, so capture (which bypasses hit-testing entirely) goes on
  // delivering every move to a list the user can no longer see. What they get
  // is a value they never pressed: releasing a mouse drifts it a few pixels,
  // usually UP towards the trigger, and the cell nearest a point above a
  // vertical list is the FIRST one. Hence "it randomly picks the first option".
  //
  // The test is the surface's own state and not its geometry, because geometry
  // lies for as long as the exit transition runs: `hidePopover()` clears
  // :popover-open synchronously, but --t-wash keeps the sheet displayed, and
  // measured, for another 120ms. `[popover]` is the platform's, not Siegel's —
  // this knows that it sits on a dismissable surface, not which element floats it.
  #onMove(event) {
    if (!this.#dragging) return;
    if (this.#surface && !this.#surface.matches(':popover-open')) { this.#endDrag(); return; }
    if (!this.#rendered()) { this.#endDrag(); return; }
    this.#pickAt(event);
  }

  // Whether the strip has a box at all. An unrendered element is not somewhere
  // small or somewhere off screen — it is NOWHERE, and every rect it reports is
  // the same empty one at the viewport origin.
  #rendered() { return this.#strip?.getClientRects().length > 0; }

  // The nearest top-layer surface above the strip, walking the FLAT tree so the
  // walk crosses shadow boundaries instead of stopping at them: the sheet that
  // holds a compact select belongs to Popout's shadow root, several hosts up.
  // Read once per gesture — it cannot change while a pointer is down.
  #floatingSurface() {
    for (let node = this.#strip; node; node = node.assignedSlot
      ?? (node.parentNode instanceof ShadowRoot ? node.parentNode.host : node.parentNode)) {
      if (node.nodeType === 1 && node.hasAttribute('popover')) return node;
    }
    return null;
  }
  #pressing(cell) {
    if (!this.#strip) return;
    for (const other of this.#strip.querySelectorAll('.slot[data-pressed]')) {
      if (other !== cell) other.removeAttribute('data-pressed');
    }
    cell?.setAttribute('data-pressed', '');
  }

  // Take back what you took (SCANME §8.2). Capture is released here rather than
  // left to the implicit release on pointerup, because a gesture can now end
  // before the pointer does — and a strip still holding the pointer would keep
  // taking moves away from whatever the user has moved on to.
  #endDrag() {
    this.#dragging = false;
    this.#surface = null;
    if (this.#pointer !== null) {
      if (this.#strip?.hasPointerCapture(this.#pointer)) this.#strip.releasePointerCapture(this.#pointer);
      this.#pointer = null;
    }
    this.#strip?.classList.remove('dragging');
    this.#pressing(null);
  }
  #pickAt(event, press = false) {
    const vertical = this.orientation === 'vertical';
    const point = vertical ? event.clientY : event.clientX;
    let best = null;
    let bestDistance = Infinity;
    for (const cell of this.#strip.querySelectorAll('.slot:not(:disabled)')) {
      const box = cell.getBoundingClientRect();
      // NO BOX, NO CANDIDATE. getBoundingClientRect() answers 0×0 at 0,0 for an
      // unrendered cell, which is not a position — but the arithmetic below
      // cannot tell that from a cell genuinely sitting in the corner, so every
      // hidden cell measures as equally near and the FIRST one wins the race.
      // That is how a press on the fifth option committed the first: the panel
      // this list lives in had closed on the press, and the drag that followed
      // resolved against a strip that was no longer on screen.
      if (!box.width && !box.height) continue;
      const low = vertical ? box.top : box.left;
      const high = vertical ? box.bottom : box.right;
      const distance = point < low ? low - point : point > high ? point - high : 0;
      if (distance === 0) { best = cell; break; }
      if (distance < bestDistance) { bestDistance = distance; best = cell; }
    }
    this.#pressing(best);
    if (best && (press || !this.#selected(best.dataset.id))) this.select(this.#idOf(best.dataset.id));
  }
}

export class SiegelSelectItem extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, SELECT_ITEM_STYLES);
  }
  get option() {
    const label = this.#label();
    return {
      id: this.getAttribute('value') ?? label,
      label,
      icon: this.#slotted('leading'),
      endIcon: this.#slotted('trailing'),
      hint: this.getAttribute('hint') ?? undefined,
      color: this.getAttribute('color') ?? undefined,
      disabled: this.hasAttribute('disabled'),
    };
  }
  #label() {
    return [...this.childNodes]
      .filter((node) => !(node.nodeType === Node.ELEMENT_NODE && node.hasAttribute('slot')))
      .map((node) => node.textContent)
      .join('')
      .replace(/\s+/g, ' ')
      .trim();
  }
  #slotted(name) {
    return [...this.children].find((el) => el.getAttribute('slot') === name)?.innerHTML || undefined;
  }
}

export class SiegelSelectFilter extends HTMLElement {
  static observedAttributes = ['for', 'placeholder', 'aria-label', 'disabled'];

  #input = null;
  #target = null;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, SELECT_FILTER_STYLES);
    this.shadowRoot.innerHTML = '<input type="search" autocomplete="off" spellcheck="false">';
    this.#input = this.shadowRoot.querySelector('input');
    this.shadowRoot.addEventListener('input', () => this.#apply(true));
    this.shadowRoot.addEventListener('keydown', (event) => {
      if (event.key !== 'ArrowDown' && event.key !== 'Enter') return;
      const strip = this.#resolve();
      if (!strip) return;
      event.preventDefault();
      strip.focus();
    });
  }

  set value(next) {
    this.#input.value = next == null ? '' : String(next);
    this.#apply(false);
  }
  get value() { return this.#input.value; }

  set disabled(value) { this.toggleAttribute('disabled', !!value); }
  get disabled() { return this.hasAttribute('disabled'); }

  focus(options) { this.#input.focus(options); }

  connectedCallback() {
    adoptPreUpgrade(this, ['value', 'disabled']);
    this.#apply(false);
  }
  disconnectedCallback() { this.#release(); }

  attributeChangedCallback(name, _old, value) {
    if (!this.shadowRoot) return;
    if (name === 'for') {
      this.#release();
      this.#apply(false);
      return;
    }
    if (name === 'disabled') this.#input.disabled = value != null;
    else if (value == null) this.#input.removeAttribute(name);
    else this.#input.setAttribute(name, value);
  }
  #resolve() {
    const id = this.getAttribute('for');
    if (!id) return null;
    const root = this.getRootNode();
    const found = root.getElementById?.(id) ?? null;
    return found && 'filter' in found ? found : null;
  }

  #release() {
    if (this.#target) this.#target.filter = '';
    this.#target = null;
  }

  #apply(announce) {
    const strip = this.#resolve();
    if (this.#target && this.#target !== strip) this.#release();
    this.#target = strip;
    if (!strip) return;
    strip.filter = this.#input.value;
    if (announce) emit(this, 'filter-change', { value: this.#input.value, shown: strip.shown.length });
  }
}
if (!customElements.get('siegel-select-item')) {
  customElements.define('siegel-select-item', SiegelSelectItem);
}
if (!customElements.get('siegel-select')) customElements.define('siegel-select', SiegelSelect);
if (!customElements.get('siegel-select-filter')) {
  customElements.define('siegel-select-filter', SiegelSelectFilter);
}
