import { densityStyles } from './density.js';

const DENSITY_CSS = densityStyles();

// Match Klotz's frame mechanics. The container name must be declared in the
// tree holding the frame so cells and application CSS can resolve it across
// shadow boundaries. The same sheet reserves space for open overlay edges.
const FRAME_CSS = `
  siegel-app { container-name: siegel-app; }
  siegel-app { --band-top: 0px; --band-bottom: 0px; --band-left: 0px; --band-right: 0px; }
  siegel-app:has(> siegel-app-cell[position="edge-top"][overlay][open]:not([hidden])) {
    --band-top: var(--app-edge, 52px);
  }
  siegel-app:has(> siegel-app-cell[position="edge-bottom"][overlay][open]:not([hidden])) {
    --band-bottom: var(--app-edge, 52px);
  }
  siegel-app:has(> siegel-app-cell[position="edge-left"][overlay][open]:not([hidden])) {
    --band-left: var(--app-edge, 52px);
  }
  siegel-app:has(> siegel-app-cell[position="edge-right"][overlay][open]:not([hidden])) {
    --band-right: var(--app-edge, 52px);
  }
`;

// Shared constructable stylesheet, adopted by every component's shadow root so
// the look stays consistent without leaking global CSS. Colors come exclusively
// from the semantic tokens in tokens/tokens.css (inherited through the shadow
// boundary), so a rebrand or theme switch never touches this file.
//
// Also the one place a stylesheet is built: `sheet(css)` caches per CSS string,
// so a component's rules are parsed once for the whole page rather than once
// per instance.

// Shared by shadow-root buttons and light-DOM modal actions. Selectors are
// internal callers' CSS selectors; content layout never supplies another skin.
export function buttonStyles(selectors = ['button']) {
  const select = (suffix) => selectors.map((selector) => selector + suffix).join(', ');
  const enabled = ':where(:not(:disabled):not([aria-disabled="true"]))';
  const neutral = ':where(:not(.primary):not(.danger):not(.success):not(.warning))';
  const variants = [
    ['primary', '--primary', '--primary-foreground'],
    ['danger', '--danger', '--accent-foreground'],
    ['success', '--good', '--accent-foreground'],
    ['warning', '--warning', '--accent-foreground'],
  ];
  return `
  ${select('')} {
    --_button-tone: var(--text);
    --_button-foreground: var(--text);
    --_button-bg: var(--surface);
    --_button-ink: var(--text);
    --_button-border: var(--divider);
    --_button-hover: var(--surface-hover);
    --_button-pressed: color-mix(in srgb, var(--text) 12%, var(--surface));
    appearance: none; box-sizing: border-box; text-decoration: none;
    -webkit-user-select: none; user-select: none; touch-action: manipulation;
    height: var(--ctl-h, 32px);
    font: inherit; font-family: var(--button-font-family, inherit);
    font-size: var(--button-font-size, 11px); font-weight: 500; line-height: 1.4;
    padding: var(--ctl-pad-y) calc(var(--ctl-pad-x) + 3px);
    border: 1px solid var(--_button-border); border-radius: 6px;
    position: relative; white-space: nowrap;
    background: var(--_button-bg); color: var(--_button-ink); cursor: pointer;
    display: inline-flex; align-items: center; justify-content: center; gap: calc(var(--ctl-gap) + 1px);
  }
  ${select(' svg')} { width: var(--icon-size); height: var(--icon-size); flex: none; }
  ${variants.map(([name, tone, foreground]) => `${select('.' + name)} {
    --_button-tone: var(${tone}); --_button-foreground: var(${foreground});
  }`).join('\n')}
  ${select(':where(.primary, .danger, .success, .warning)')} {
    --_button-bg: var(--_button-tone); --_button-ink: var(--_button-foreground);
    --_button-border: var(--_button-tone);
    --_button-hover: color-mix(in srgb, var(--_button-tone) 86%, var(--text));
    --_button-pressed: color-mix(in srgb, var(--_button-tone) 76%, var(--text));
  }
  ${select('.secondary:where(.primary, .danger, .success, .warning)')} {
    --_button-bg: var(--surface);
    --_button-ink: color-mix(in srgb, var(--_button-tone) 84%, var(--text));
    --_button-border: color-mix(in srgb, var(--_button-tone) 30%, var(--divider));
    --_button-hover: color-mix(in srgb, var(--_button-tone) 8%, var(--surface));
    --_button-pressed: color-mix(in srgb, var(--_button-tone) 15%, var(--surface));
  }
  ${select('.icon-btn')} {
    --_button-bg: transparent; --_button-ink: var(--_button-tone);
    --_button-hover: color-mix(in srgb, var(--_button-tone) 8%, var(--surface));
    --_button-pressed: color-mix(in srgb, var(--_button-tone) 15%, var(--surface));
    width: var(--ctl-h, 32px); height: var(--ctl-h, 32px); padding: 0; border: 0;
    flex: 0 0 auto;
  }
  ${select('.icon-btn' + neutral)} { --_button-tone: var(--text-muted); }
  ${select('.icon-btn.secondary')} {
    --_button-bg: var(--surface);
    border: 1px solid var(--_button-border);
  }
  ${select('.icon-btn svg')} { display: block; }
  ${select('.text-link')} {
    --_button-bg: transparent; --_button-ink: var(--_button-tone);
    --_button-hover: transparent; --_button-pressed: transparent;
    border: 0; padding-inline: 0; border-radius: 2px; text-underline-offset: 3px;
  }
  ${select('.text-link' + neutral)} { --_button-tone: var(--accent); }
  @media (hover: hover) {
    ${select(enabled + ':hover')} { background: var(--_button-hover); }
    ${select('.text-link' + enabled + ':hover')} { text-decoration: underline; }
  }
  ${select('.active' + enabled)} { background: var(--_button-hover); }
  ${select(enabled + ':active')} { background: var(--_button-pressed); }
  ${select('.text-link.active' + enabled)} { text-decoration: underline; }
  ${select(':disabled')}, ${select('[aria-disabled="true"]')} { opacity: 0.45; cursor: default; }
`;
}

// Native fields share their appearance in shadow roots and light-DOM dialogs.
export function inputStyles(scope = '') {
  const field = `${scope ? scope + ' ' : ''}:is(input, textarea, select)`;
  return `
  ${field} {
    font: inherit; font-size: var(--ctl-font-size, inherit);
    padding: var(--ctl-pad-y) var(--ctl-pad-x);
    border: 1px solid var(--control-border);
    border-radius: var(--ctl-radius);
    background: var(--surface); color: var(--text);
  }
  ${field}:is(:disabled, [readonly]) {
    background: var(--control-bg); color: var(--text-muted);
    -webkit-text-fill-color: var(--text-muted); opacity: 1;
  }
  ${field}:disabled { cursor: not-allowed; }
  ${field}:is(:disabled, [readonly])::placeholder { color: var(--text-muted); opacity: 1; }
  `;
}

const CSS = `
  /* No --bar-h here. It used to default to 36px on EVERY adopting :host, which
     is a local definition — so it beat the value a container was inheriting
     down, and a panel region that adopted this sheet would have pinned itself
     at 36px instead of taking the shell's comfortable 44 / dense 30. The one
     element that wanted the 36 (the app bar) now states it as its own fallback. */
  :host { display: block; box-sizing: border-box; }
  * { box-sizing: border-box; }

  /* \`hidden\` works on every component, for free.
     A component sets \`display\` on :host, and that author rule beats the UA's
     \`[hidden] { display: none }\` — so without this, setting \`hidden\` on a
     Siegel element does nothing at all. Every root that adopts the shared sheet
     gets the fix, which is why it lives here instead of being restated per file.
     \`!important\` is deliberate: a component's own \`:host([type="spinner"])\`
     -style rule carries the same specificity and comes later in the cascade, so
     plain \`display: none\` would lose to it. Hiding is not a look to be
     overridden — an element told to be hidden is hidden. */
  :host([hidden]) { display: none !important; }
  /* Same fix one level in: a rule here that sets \`display\` on a class (.icon-btn
     is inline-flex, siegel-pager is flex) beats the UA's [hidden] rule for any
     element inside an adopting shadow root, so components were restating this
     per selector. */
  [hidden] { display: none !important; }

  ${buttonStyles(['button', 'a.icon-btn', 'a.text-btn', 'a.text-link'])}
  ${inputStyles()}
  /* Single-line controls share geometry regardless of their label font size.
     Native toggles/ranges and multiline fields retain their own geometry. */
  :where(input:not([type="checkbox"]):not([type="radio"]):not([type="range"]):not([type="hidden"])),
  select { height: var(--ctl-h, 40px); }
  textarea { width: 100%; resize: vertical; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
  /* Native checkbox/radio/range/progress take the brand from one declaration —
     the native-first answer to theming them (their popup/picker chrome already
     follows light/dark via color-scheme in tokens.css). No custom skin until a
     control needs more than the tint. */
  input[type="checkbox"], input[type="radio"], input[type="range"], progress, meter {
    accent-color: var(--accent);
  }
  /* Form field group — the one shared shape for a labelled control row and a
     grouped section, extracted from the fieldset grids the apps built alone.
     \`.field\` is label-beside-control (the label column via --field-label-w);
     a native <fieldset> gets quiet Siegel chrome so grouping stays free. */
  .field {
    display: grid; grid-template-columns: var(--field-label-w, 160px) 1fr;
    gap: 10px; align-items: center; margin-bottom: 10px;
  }
  .field > label { color: var(--text-muted); }
  .field > input, .field > select, .field > textarea { width: 100%; }
  fieldset {
    border: 1px solid var(--divider); border-radius: 8px;
    margin: 0 0 14px; padding: 12px 14px; min-width: 0;
  }
  fieldset > legend {
    padding: 0 6px; font-size: 12px; text-transform: uppercase;
    letter-spacing: var(--tracking, 0.05em); color: var(--text-muted);
  }

  /* Validation state — :user-invalid, not :invalid, so an untouched required
     field doesn't open red; the [aria-invalid] form is what app code sets
     programmatically (formDialog's per-field errors use the same token). */
  input:user-invalid, textarea:user-invalid, select:user-invalid,
  input[aria-invalid="true"], textarea[aria-invalid="true"], select[aria-invalid="true"] {
    border-color: var(--error);
  }
  .row { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
  .muted { color: var(--text-muted); }
  .error { color: var(--error); }
  .card {
    border: 1px solid var(--divider);
    border-radius: 10px;
    padding: 16px;
    background: var(--surface);
  }
  h2 { font-size: 16px; margin: 0 0 12px; }
  h3 { font-size: 13px; text-transform: uppercase; letter-spacing: var(--tracking, 0.04em); margin: 0 0 8px; }
  ul.list { list-style: none; margin: 0; padding: 0; }
  ul.list li {
    display: flex; align-items: center; justify-content: space-between; gap: 8px;
    padding: 8px 10px; border-radius: 6px;
  }
  ul.list li:hover { background: var(--hover); }
  ul.list li.selected { background: var(--selected); }

  /* Nav list — a vertical list of selectable rows (icon + label), the shape a
     side-panel navigation uses (a document list, a section picker). Same
     hover + selected washes as \`ul.list\`, but a plain container of \`.nav-item\`
     rows (not <ul>/<li>) so it drops straight into a panel body and scrolls. */
  .nav-list { display: flex; flex-direction: column; min-height: 0; overflow: auto; }
  .nav-item {
    display: flex; align-items: center; gap: 6px;
    min-height: var(--row-h); padding: var(--ctl-pad-y) var(--ctl-pad-x); cursor: pointer; white-space: nowrap; color: inherit;
  }
  .nav-item:hover { background: var(--hover); }
  .nav-item.selected { background: var(--selected); }
  .nav-item svg { width: var(--icon-size); height: var(--icon-size); flex: none; color: var(--text-muted); }
  .nav-item .label { min-width: 0; overflow: hidden; text-overflow: ellipsis; }

  code, .mono { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }

  /* Data tables — the common denominator of the services' row tables (users,
     domains, query results): flush edges (no outer border — the container, e.g. a
     siegel-panel, frames it), divider rules between rows, and a sticky header
     that stays opaque over scrolling rows. Wrap in an \`overflow: auto\`
     container to scroll; add .interactive for FleetManager's row hover wash. */
  table.data { width: 100%; border-collapse: collapse; font-size: 13px; }
  table.data th, table.data td {
    text-align: left; height: var(--row-h); padding: var(--ctl-pad-y) var(--ctl-pad-x);
    border-bottom: 1px solid var(--divider); white-space: nowrap;
  }
  table.data th { background: var(--table-header); position: sticky; top: 0; }
  table.data tr:last-child td { border-bottom: none; }
  table.data.interactive tbody tr:hover td { background: var(--row-hover); }

  /* One designed focus ring for every interactive control, so keyboard focus is
     consistent and always visible against --surface — the browser default is
     inconsistent and washes out in some theme/browser pairings. Scoped to
     :focus-visible so it shows for keyboard/AT, not on mouse click. */
  :is(button, input, select, textarea, [tabindex], a[href]):focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 1px;
  }

  /* Spinner — the shared "known short wait" indicator (the modal's art,
     promoted here for panels, tables, and buttons). Pair with .empty-state for
     the "no data" case. The rotation is gated on prefers-reduced-motion: no
     animation for users who ask for less; the ring still reads as a spinner. */
  .spinner {
    width: 22px; height: 22px; flex: none; border-radius: 50%;
    border: 2.5px solid var(--control-border);
    border-top-color: var(--accent);
  }
  @media (prefers-reduced-motion: no-preference) {
    .spinner { animation: siegel-spin 0.8s linear infinite; }
  }
  @keyframes siegel-spin { to { transform: rotate(360deg); } }

  /* Spin an icon while its action is in flight: put \`.spinning\` on the control
     (e.g. an .icon-btn refresh) and its svg rotates. Reuses the one spin keyframe;
     honours prefers-reduced-motion. */
  @media (prefers-reduced-motion: no-preference) {
    .spinning svg { animation: siegel-spin 0.8s linear infinite; }
  }

  /* Empty state — the one convention for "nothing here yet", used identically
     in panels, tables, and trees: centered, muted, an optional icon over one
     line of text and an optional action. (A .spinner is the busy counterpart —
     spinner = a known short wait, empty state = there is no data.) */
  .empty-state {
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    gap: 8px; padding: 32px 16px; text-align: center; color: var(--text-muted);
  }
  .empty-state .glyph { color: var(--text-muted); opacity: 0.7; }
  .empty-state .glyph svg { width: 24px; height: 24px; }
  .empty-state p { margin: 0; }
`;

// One CSSStyleSheet per distinct CSS string, for the whole document.
//
// A component's rules are a module-level template literal — the same string for
// every instance — so parsing it per instance is pure waste: a table with 200
// rendered <siegel-status> cells used to parse that component's CSS 200 times.
// Constructable sheets are made to be shared, and adopting one sheet into many
// roots is the supported path.
const sheets = new Map();

export function sheet(css) {
  let cached = sheets.get(css);
  if (!cached) {
    cached = new CSSStyleSheet();
    cached.replaceSync(css);
    sheets.set(css, cached);
  }
  return cached;
}

export function styleSheet() {
  return sheet(CSS);
}

// Adopt the shared sheet plus the component's own CSS into a shadow root. This
// is the only way a component gets styles: anything worth sharing belongs in
// CSS above instead.
//
// Variadic so a component with a SECOND, caller-supplied sheet — the table's
// `.cellCss`, whose rules the app writes for its own `render` cells — can
// re-adopt the whole stack by name instead of splicing
// `shadowRoot.adoptedStyleSheets` by index and hoping the layout never changes.
// Falsy entries drop out, so `adoptStyles(root, CSS, this.#cellCss)` is the
// same call whether or not the app supplied any.
export function adoptStyles(shadow, ...extras) {
  const density = sheet(DENSITY_CSS);
  const frame = sheet(FRAME_CSS);
  // Density and frame rules also reach elements written in the document.
  const doc = shadow.ownerDocument;
  const missing = [density, frame].filter(s => !doc.adoptedStyleSheets.includes(s));
  if (missing.length) doc.adoptedStyleSheets = [...doc.adoptedStyleSheets, ...missing];
  shadow.adoptedStyleSheets = [styleSheet(), frame, ...extras.filter(Boolean).map(sheet), density];
}
