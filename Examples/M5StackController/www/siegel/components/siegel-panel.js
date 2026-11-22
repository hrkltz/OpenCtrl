// The panel as TWO elements that ship together; importing this module registers
// both.
//
//   <siegel-panel>       — the SURFACE. Owns the fill, the border, the radius and
//                          the elevation, and stacks its regions in a column.
//   <siegel-panel-cell>  — one REGION of it. Says WHERE it is (`position`) and
//                          owns the padding, the gap, the layout axis and the
//                          scrolling that follow from that.
//
// Like the app frame, a panel is divided into placed regions. App cells own
// structure only; panel cells also own internal spacing. Appearance lives on
// the panel itself, including when it is placed inside an app cell.
//
//   <!-- a card in content: one region, and no position needed to say so -->
//   <siegel-panel>
//     <siegel-panel-cell>…the body…</siegel-panel-cell>
//   </siegel-panel>
//
//   <!-- a card with a header and a footer: the reason the split exists -->
//   <siegel-panel>
//     <siegel-panel-cell position="edge-top"><span class="title">Documents</span></siegel-panel-cell>
//     <siegel-panel-cell scroll><siegel-tree></siegel-tree></siegel-panel-cell>
//     <siegel-panel-cell position="edge-bottom"><span class="muted">3 documents</span></siegel-panel-cell>
//   </siegel-panel>
//
//   <!-- a region of the app frame: its contents choose their presentation -->
//   <siegel-app-cell position="edge-top">
//     <siegel-panel appearance="flat">
//       <siegel-panel-cell position="edge-top"><span class="title">Northwind</span></siegel-panel-cell>
//     </siegel-panel>
//   </siegel-app-cell>
//
// <siegel-panel>
//   appearance  'outline' (DEFAULT) | 'flat' | 'float'
//   density     mouse | touch forces a geometry; absent, the input device decides.
//   heading     "micro" for an uppercase label, independent of density.
//   fill        boolean — inside a `layout="sections"` body, this section takes
//               the remaining height.
//
// <siegel-panel-cell>
//   position    'edge-top' | 'content-center' | 'edge-bottom' — OPTIONAL; no
//               position means 'content-center', and writing it out says the
//               same thing.
//   flush       boolean — drop the padding, for a region framing something that
//               brings its own.
//   scroll      boolean — THIS region scrolls. On the cell and not on the panel:
//               a header and a footer have to stay pinned while the body moves,
//               which is only possible if the body is the scroller.
//   loading     boolean — a centred spinner OVERLAYS this region and its content
//               is hidden in place, so the card keeps its size while it
//               refreshes and a reload never makes it jump.
//   selecting   boolean — swap the default slot for the `alt` slot and tint the
//               bar: the multi-select contextual header. You rarely set it — the
//               PANEL toggles it from any `selection-change` inside it.
//   layout      'sections' — stack flush child panels as one card's several
//               independently headed regions.
//
// APPEARANCES ADD FEATURES: flat is transparent; outline adds a visible border
// and radius; float adds the --panel fill. Outline is the default. No appearance
// casts a shadow: a card stands off the canvas by its fill and its hairline alone.
// <siegel-popout> adds --shadow-pop to the panel it holds, which lies over the page.
// Outer spacing and reserved border space are identical across appearances.
// Inner spacing is also independent of appearance. --panel-pad is
// the shared inset for the body, header and footer. --panel-gap separates items
// in each region. --cell-pad / --cell-gap override defaults; flush removes padding.
// Each nested panel sets its own spacing. Dense controls spacing and bar/control sizes.
// Applications can override the background and padding through ordinary CSS.
//
// THE CELL PLACES AND SHAPES; IT NEVER DRAWS THE SURFACE. Everything that makes
// this look like a card belongs to <siegel-panel>. The one thing a cell does
// paint is its own DIVIDER — a header's bottom rule, a footer's top — because
// that line is about the relationship between two regions rather than about the
// card, so it has to appear only when both regions exist.
//
// AND THE SHAPE CAN BE RETUNED FROM OUTSIDE, through five custom properties
// every cell CONSUMES and none declares: --cell-axis / --cell-align / --cell-h /
// --cell-pad / --cell-gap. Set any of them on the panel from an outer-tree rule
// — which beats a :host rule — and the cells follow. `position` picks the
// DEFAULTS those five fall back to and nothing more, so a bar is retunable
// exactly as far as a body is. That is not a hook nobody uses: it is how
// an app turns a bar into a rail by choosing its axis and alignment.
//
// (They are --cell-* and not --bar-*: --bar-h and its family are the TYPE and
// metrics of a chrome bar, which density switches and which a body has no use
// for. The two channels do different jobs and both are live — --cell-* is
// geometry, --bar-* is the bar's own look.)
//
// <siegel-panel> has no render pipeline — the cells place themselves, so the
// surface is a skeleton of three slots — and the cell's is one method wide.

import { adoptStyles } from '../utilities/styles.js';
import { adoptPreUpgrade } from '../utilities/element.js';
import './siegel-progress.js'; // registers <siegel-progress>, used by `loading`

// Every position that is a real slot. `content-center` is absent because it IS
// the default slot: a cell with no position, and a cell that writes it out, are
// the same cell.
const PLACES = ['edge-top', 'edge-bottom'];

// What `appearance` accepts. Anything else falls back to the default rather than
// being trusted, so there is one door and one answer.
const APPEARANCES = ['flat', 'outline', 'float'];

// ===========================================================================
// <siegel-panel>
// ===========================================================================
const PANEL_STYLES = `
  :host {
    /* A column of regions: header, body, footer, in that order whatever order
       they were written in. */
    display: flex; flex-direction: column; min-width: 0;
    color: var(--text);

    /* ---- outline: the default ---- */
    background: transparent;
    border: 1px solid var(--divider);
    border-radius: 8px;
    /* Kept for every appearance: it contains in-flow overflow, which is what
       stops a body spilling its card. It was never what confined a
       \`position: fixed\` descendant, so dropping it on \`flat\` would buy
       nothing and risk exactly that spill. */
    overflow: hidden;
  }

  /* Flat keeps the border space so appearances share the same geometry. */
  :host([appearance="flat"]) {
    border-color: transparent;
    border-radius: 0;
  }

  /* ---- float: outline plus a filled surface, and no shadow ---- */
  :host([appearance="float"]) {
    background: var(--panel);
  }

  /* Header typography is independent of density. */
  :host([heading="micro"]) {
    --bar-font-size: 11px; --bar-transform: uppercase; --bar-tracking: 0.04em;
    --bar-color: var(--text-muted); --bar-title-weight: 400;
  }

`;

export class SiegelPanel extends HTMLElement {
  // No observedAttributes, and `appearance` in particular is NOT one. The rule
  // that an attributeChangedCallback delegates to its setter (SCANME §8.2.7)
  // assumes the setter does not write the attribute it was told about — and this
  // one reflects, so the pair would loop: setAttribute fires the callback even
  // when the value is unchanged, which calls the setter, which sets it again.
  // Nothing here needs to react anyway: the appearance is read by CSS off the
  // attribute, and the getter reads the same attribute.
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, PANEL_STYLES);
    this.shadowRoot.innerHTML = `
      <slot name="edge-top"></slot>
      <slot></slot>
      <slot name="edge-bottom"></slot>`;
    this.addEventListener('selection-change', this.#onSelectionChange);
  }

  set appearance(value) {
    if (APPEARANCES.includes(value)) this.setAttribute('appearance', value);
    else this.removeAttribute('appearance');
  }
  get appearance() {
    const asked = this.getAttribute('appearance');
    return APPEARANCES.includes(asked) ? asked : 'outline';
  }

  set fill(value) { this.toggleAttribute('fill', !!value); }
  get fill() { return this.hasAttribute('fill'); }

  connectedCallback() { adoptPreUpgrade(this, ['appearance', 'fill']); }

  // A named handler, not an inline arrow: it is the one piece of real behaviour
  // in this element, and it deserves to be findable.
  //
  // THE CONTEXTUAL HEADER, wired once here instead of in every app: anything
  // inside this panel that reports a selection (<siegel-table selectable>, and
  // anything else emitting the same event) switches the header cell into
  // `selecting` while something is selected, and back when nothing is.
  //
  // Gated on the header actually HAVING `alt` content, because `selecting` swaps
  // the default slot for that one and a header with none would go blank.
  // Supplying alt content IS the opt-in, which is why there is no attribute.
  //
  // ONLY THE NEAREST PANEL ANSWERS. Panels nest — a card divides itself into
  // flat sections — and `selection-change` bubbles composed, so every panel up
  // the tree sees a section's selection too. Both halves below are about that:
  // the innermost panel in the path claims it and its ancestors stand down, and
  // the header is looked up among the cells THIS panel owns. Without either, a
  // selectable table in a section would tint the page's header, or reach down
  // and tint the section's.
  //
  // A <siegel-select> IS NOT A SELECTION. It speaks `selection-change` because
  // that is the kit's one vocabulary for "these ids are chosen", not because
  // picking a value is a selection MODE. `type="strip"` makes it unmissable
  // rather than merely wrong: a strip always has a selection, so the header
  // would enter selecting state at first paint and never leave.
  #onSelectionChange = (event) => {
    const path = event.composedPath();
    if (path.some((node) => node.localName === 'siegel-select')) return;
    const nearest = path.find((node) => node instanceof SiegelPanel);
    if (nearest !== this) return;
    const header = this.#headerCell();
    if (!header || !header.querySelector('[slot="alt"]')) return;
    header.selecting = (event.detail?.count ?? 0) > 0;
  };

  // THE HEADER THIS PANEL OWNS, found by WHAT A NODE IS rather than by what it is
  // called. <siegel-panel-cell position="edge-top"> is a SiegelPanelCell under another tag name
  // (siegel-panel.js), so a `siegel-panel-cell[position="edge-top"]`
  // selector would silently miss every card written the older way — which is most
  // of them. `instanceof` covers both and anything either is subclassed into.
  //
  // The walk stops at a nested panel, because a section's chrome is the section's:
  // a selectable table inside a flat child must tint that child's bar, never the
  // surrounding card's. Shallow by construction — a card's chrome is a bar or two,
  // not a page of content.
  #headerCell() {
    const walk = (node) => {
      for (const child of node.children) {
        if (child instanceof SiegelPanel) continue;
        if (child instanceof SiegelPanelCell && child.position === 'edge-top') return child;
        const found = walk(child);
        if (found) return found;
      }
      return null;
    };
    return walk(this);
  }
}

// ===========================================================================
// <siegel-panel-cell>
// ===========================================================================
const CELL_STYLES = `
  /* The column form, and the no-position form, which are the same form. Every
     value is CONSUMED with a fallback and never declared, so an outer-tree rule
     on the panel can retune the lot — see the header. --cell-h defaults to 0
     because it lands on min-height, and a scrolling flex child needs that at
     zero to be allowed to shrink below its content.

     Bodies share their panel's inset and gap. Tables, trees and content that
     supplies its own inset opt out explicitly with flush. */
  :host {
    flex: 1 1 auto; min-width: 0;
    min-height: var(--cell-h, 0px);
    display: flex;
    flex-direction: var(--cell-axis, column);
    align-items: var(--cell-align, stretch);
    padding: var(--cell-pad, var(--panel-pad, 16px));
    gap: var(--cell-gap, var(--panel-gap, 8px));
    /* The anchor for \`loading\`'s overlay. */
    position: relative;
  }

  /* A BAR. THE SAME FIVE KNOBS, with a bar's values in the FALLBACKS rather than
     in the declarations — so a bar is retunable exactly as far as a body is, and
     \`position\` sets the default shape rather than locking one in. That
     distinction is load-bearing in both directions: a status strip that wants to
     be 26px says so with --cell-h. An app can set --cell-h: 0 when a
     containing app cell already fixes the thickness. App cells do not set
     panel geometry themselves.

     --bar-h and not a literal: a bar's height and the thickness of a frame's
     edge are one number saying one thing, so a bar written inside a frame and
     one written outside come out the same, and density moves both.

     \`flex: none\` is the one thing NOT offered as a knob, because it is what
     makes this a bar rather than a region: a header a greedy body can stretch is
     not a header. */
  :host([position="edge-top"]),
  :host([position="edge-bottom"]) {
    flex: none;
    min-height: var(--cell-h, var(--bar-h, 52px));
    flex-direction: var(--cell-axis, row);
    align-items: var(--cell-align, center);
    gap: var(--cell-gap, var(--panel-gap, 8px));
    font-size: var(--bar-font-size, 14px);
    text-transform: var(--bar-transform, none);
    letter-spacing: var(--tracking, var(--bar-tracking, normal));
    color: var(--bar-color, var(--text));
  }

  /* Header, body and footer share the same inset and spacing tokens. */
  :host([position="edge-top"]),
  :host([position="edge-bottom"]) {
    padding: var(--cell-pad, 0 var(--panel-pad, 16px));
  }

  /* THE DIVIDER IS THE CELL'S, and it is the one thing a cell draws. It belongs
     to the relationship between two regions rather than to the card, so a panel
     with no footer must not get a rule floating above its own bottom edge —
     which is only expressible if the region that would be separated owns it. */
  :host([position="edge-top"]) { border-bottom: 1px solid var(--divider); }
  :host([position="edge-bottom"]) { border-top: 1px solid var(--divider); }

  :host([flush]) { padding: 0; }
  :host([scroll]) { overflow: auto; }

  /* A SECTIONED BODY is the reusable "one card, several independently headed
     regions" composition. Direct child panels keep their natural height unless
     one opts into [fill], which receives the rest. That section's own body then
     bounds an unbounded tree or table and lets it scroll without moving the
     fixed sections above or below it.

     The divider belongs to the relationship between adjacent sections, hence it
     lives here rather than on every flat child: without it, a section whose
     predecessor has no footer starts with no rule at their shared edge. */
  :host([layout="sections"]) ::slotted(siegel-panel) { flex: 0 0 auto; }
  :host([layout="sections"]) ::slotted(siegel-panel[fill]) { flex: 1 1 0; min-height: 0; }
  :host([layout="sections"]) ::slotted(siegel-panel:not(:first-child)) {
    border-top: 1px solid var(--divider);
  }

  /* A <siegel-tree> slotted straight into a body IS that body's scroll region:
     it fills the card and scrolls inside it. Without this the tree is
     display: block at its content height, so a long list stretches the card
     instead of scrolling — and because the card is usually in a fixed-height
     region, "stretches" means rows disappear off the frame. Scoped to the tree
     on purpose: it is the one kit component that is a list of unbounded length.
     A consumer wanting natural height still wins, since a rule in the tree's own
     outer tree beats a ::slotted() rule from in here. */
  ::slotted(siegel-tree) { flex: 1 1 auto; min-height: 0; overflow: auto; }

  /* A card holding a table puts that table's <siegel-pager> in the footer — one
     bar at the bottom instead of two stacked ones. It takes the free space so
     its range sits left and its controls right, with the footer's own text
     beside it. */
  :host([position="edge-bottom"]) ::slotted(siegel-pager) { flex: 1 1 auto; min-width: 0; }

  /* The "title + trailing actions" convention. No flex on .title in a BODY —
     the .spacer is the explicit "push the rest away" element and two growing
     items would fight over the same free space — but a bar's title does grow,
     because that is what puts trailing actions on the right. \`flex: 1 1 auto\`
     and never \`flex: 1\`: a zero basis gives the item a hypothetical main size
     of 0, which WebKit is entitled to take as the bar's intrinsic width
     (SCANME §8.2.11). */
  ::slotted(.title) {
    min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
    font-weight: var(--bar-title-weight, 600);
  }
  :host([position^="edge-"]) ::slotted(.title) { flex: 1 1 auto; }
  ::slotted(.spacer) { flex: 1 1 auto; min-width: 0; min-height: 0; }

  /* Contextual (selection) header: the \`alt\` slot is hidden until [selecting],
     then it replaces the normal content and the bar tints with the selection
     wash. The slot's display flips between contents (laid out) and none
     (removed); the assigned controls keep their caller-scoped styles. */
  slot[name="alt"] { display: none; }
  :host([selecting]) slot:not([name]) { display: none; }
  :host([selecting]) slot[name="alt"] { display: contents; }
  :host([selecting]) { background: var(--selected); }

  /* \`loading\` — a built-in busy state: a centred spinner OVERLAYS the region
     and the content is hidden in place (not removed), so the card keeps its size
     while it refreshes. The min-height keeps room for the spinner when the
     content is empty or short (the very first load). <siegel-progress
     type="spinner"> owns its own animation and honours prefers-reduced-motion. */
  .busy { position: absolute; inset: 0; display: none; align-items: center; justify-content: center; padding: 24px; }
  :host([loading]) { min-height: 72px; }
  :host([loading]) slot { visibility: hidden; }
  :host([loading]) .busy { display: flex; }
  .busy siegel-progress { --spinner-size: 22px; }
`;

export class SiegelPanelCell extends HTMLElement {
  static observedAttributes = ['position', 'loading', 'layout'];

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    adoptStyles(this.shadowRoot, CELL_STYLES);
    this.shadowRoot.innerHTML = `
      <slot></slot>
      <slot name="alt"></slot>`;
  }

  set position(value) {
    if (PLACES.includes(value)) this.setAttribute('position', value);
    else this.removeAttribute('position');
  }
  get position() {
    const asked = this.getAttribute('position');
    return PLACES.includes(asked) ? asked : 'content-center';
  }

  set flush(value) { this.toggleAttribute('flush', !!value); }
  get flush() { return this.hasAttribute('flush'); }

  set scroll(value) { this.toggleAttribute('scroll', !!value); }
  get scroll() { return this.hasAttribute('scroll'); }

  set selecting(value) { this.toggleAttribute('selecting', !!value); }
  get selecting() { return this.hasAttribute('selecting'); }

  set loading(value) { this.toggleAttribute('loading', !!value); }
  get loading() { return this.hasAttribute('loading'); }

  set layout(value) {
    if (value === 'sections') this.setAttribute('layout', value);
    else this.removeAttribute('layout');
  }
  get layout() { return this.getAttribute('layout') || ''; }

  connectedCallback() {
    adoptPreUpgrade(this, ['position', 'flush', 'scroll', 'selecting', 'loading', 'layout']);
    this.#place();
    if (this.loading) this.#busy();
  }

  attributeChangedCallback(name, _old, _value) {
    if (name === 'position') this.#place();
    if (name === 'loading') this.#busy();
  }

  // The spinner is built THE FIRST TIME a region is told it is loading, and never
  // for one that is not. It is the only part of this element that costs a child
  // element, and a card is mostly regions that will never load — this kit's own
  // gallery holds six hundred cells, of which a handful ever show a spinner.
  // Building it up front made every bar carry a <siegel-progress> it would not
  // use.
  //
  // `aria-busy` goes the same way: present and true while the region is busy,
  // gone when it is not, rather than a permanent "false" on every bar in the app.
  #busy() {
    if (!this.loading) {
      this.removeAttribute('aria-busy');
      return;
    }
    this.setAttribute('aria-busy', 'true');
    if (this.shadowRoot.querySelector('.busy')) return;
    const busy = document.createElement('div');
    busy.className = 'busy';
    busy.setAttribute('part', 'busy');
    busy.setAttribute('aria-hidden', 'true');
    busy.innerHTML = '<siegel-progress type="spinner"></siegel-progress>';
    this.shadowRoot.append(busy);
  }

  // The cell sets its OWN `slot`, exactly as <siegel-app-cell> does: that is what
  // lets a panel's regions be written in whatever order explains the card, and
  // why <siegel-panel> needs no script to sort them. An unknown position falls
  // through to the default slot rather than being trusted.
  #place() {
    const asked = this.getAttribute('position');
    if (PLACES.includes(asked)) this.setAttribute('slot', asked);
    else this.removeAttribute('slot');
  }
}

if (!customElements.get('siegel-panel')) customElements.define('siegel-panel', SiegelPanel);
if (!customElements.get('siegel-panel-cell')) customElements.define('siegel-panel-cell', SiegelPanelCell);
