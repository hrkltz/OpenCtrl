// The single home for every inline SVG icon, so the set stays consistent and
// reviewable in one place. The union of the sets the services grew separately,
// deduplicated — and, since the sync at the foot of this file, the union with
// Klotz's (hrkltz's consumer design system) as well.
//
// One family: a 16-grid with FleetManager's relative line weight (1.6 on a
// 24-grid, or about 1.067 here), currentColor and round caps/joins. Turning
// arrows use the same strokes. ICON_RUN_FILLED, ICON_STOP_FILLED,
// ICON_HOME_FILLED and ICON_MORE stay solid; ICON_THEME is half-filled.
// Icons carry a default `width`/`height` of 16, but SVG scales
// losslessly: contexts pick the rendered size via CSS on the embedded `svg`
// (controls use 16px mouse / 20px touch; rail icons use 18px / 24px).
//
// Verbs are shared across views on purpose: ICON_NEW (+) is *the* "create"
// action everywhere and ICON_DELETE is the one trash can. Add a new icon here
// rather than inlining an `<svg>` in a view. Domain-specific glyphs stay in the
// owning app's own icons module.
//
// AND SHARED ACROSS KITS, which is the point of the block at the foot of this
// file. Klotz is a separate design system with a separate scope, and the two had
// separate drawings of the same ideas until each was given what the other had.
// Shared glyph designs stay aligned across both products. Siegel's lighter
// stroke treatment is intentional; consider both kits when changing a symbol.
//
// Naming: an icon is named for what it depicts (ICON_GLOBE, ICON_SLIDERS),
// never for the surface or page that happens to use it.

// FleetManager's 1.6-unit stroke, normalized to the existing 16-unit grid.
const STROKE_WIDTH = 1.6 * 16 / 24;

// Shared turning arrows remain strokes so they inherit the family's weight.
const TURN_BACK = '<path d="M2.5 7a5.5 5.5 0 1 1 .5 3.5M2.5 3.5V7H6"/>';
const TURN_HALF = '<path d="M13.333 4.667V8H10M4 4a5.333 5.333 0 0 1 8.667 2"/>';
const RUN_PATH = 'M 4.752 3.296 a 0.704 0.704 0 0 1 1.072 -0.6 l 6.6 4.096 a 0.704 0.704 0 0 1 0 1.2 l -6.6 4.096 a 0.704 0.704 0 0 1 -1.072 -0.6 z';
const STOP_RECT = '<rect x="3.504" y="3.504" width="9" height="9" rx="1.504"/>';
// The three punctuation symbols share their ring, dots and 5.5–10.5 inner span.
const circledSymbol = (path, dotY) => `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><circle cx="8" cy="8" r="5.2"/><path d="${path}"/><circle cx="8" cy="${dotY}" r=".55" fill="currentColor" stroke="none"/></svg>`;

export const ICON_NEW = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 3.25v9.5M3.25 8h9.5"/></svg>`;
export const ICON_CHECK = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><path d="M 3.2 8.4 l 3.2 3.2 6.4 -7.2"/></svg>`;
export const ICON_CLOSE = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" width="16" height="16" stroke-linejoin="round"><path d="M 4.4 4.4 l 7.2 7.2 M 11.6 4.4 l -7.2 7.2"/></svg>`;
export const ICON_SAVE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M12.75 13.25H3.25a.5.5 0 0 1-.5-.5V3.25a.5.5 0 0 1 .5-.5h7.5l2.5 2.5v7.5a.5.5 0 0 1-.5.5z"/><path d="M5 2.75v3h5v-3"/><path d="M5 9.25h6v4H5z"/></svg>`;
export const ICON_DELETE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M2.75 4.25h10.5"/><path d="M5.75 4.25V3a.75.75 0 0 1 .75-.75h3a.75.75 0 0 1 .75.75v1.25"/><path d="M4.25 4.25 4.8 13a.9.9 0 0 0 .9.85h4.6a.9.9 0 0 0 .9-.85l.55-8.75"/><path d="M6.75 6.75v5M9.25 6.75v5"/></svg>`;
export const ICON_EDIT = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M10.75 2.75 13.25 5.25 6 12.5l-2.75.75.75-2.75z"/></svg>`;
export const ICON_COPY = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="5.75" y="5.75" width="7.5" height="7.5" rx="1.25"/><path d="M10.25 5.75V4a1.25 1.25 0 0 0-1.25-1.25H4A1.25 1.25 0 0 0 2.75 4v5a1.25 1.25 0 0 0 1.25 1.25h1.75"/></svg>`;
export const ICON_RUN = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" width="16" height="16" stroke-linecap="round"><path d="${RUN_PATH}"/></svg>`;
export const ICON_STOP = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" width="16" height="16" stroke-linecap="round">${STOP_RECT}</svg>`;
export const ICON_REFRESH = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16">${TURN_HALF}<g transform="rotate(180 8 8)">${TURN_HALF}</g></svg>`;
export const ICON_DOWNLOAD = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 2.75v7"/><path d="M4.75 6.75 8 10l3.25-3.25"/><path d="M3 12.75h10"/></svg>`;
// The inverse of ICON_DOWNLOAD (arrow up over a tray) — the "upload / replace with
// a new file" verb, e.g. re-uploading a media file in place.
export const ICON_UPLOAD = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 10.25v-7"/><path d="M4.75 6 8 2.75l3.25 3.25"/><path d="M3 12.75h10"/></svg>`;
export const ICON_KEY = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><circle cx="10.75" cy="6.75" r="2.5"/><path d="M9 8.5 3.5 14M5.25 12.25l1.25 1.25M6.5 11l1.25 1.25"/></svg>`;
export const ICON_PAYMENT_CARD = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="1.75" y="3.5" width="12.5" height="9" rx="1.5"/><path d="M1.75 6.5h12.5M4 10h2.5"/></svg>`;
// A wrapped present — a box with a lid band, a vertical ribbon, and a two-loop
// bow on top. The "gift / reward" glyph (e.g. vouchers).
export const ICON_GIFT = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M3.5 8.25v4.25a.5.5 0 0 0 .5.5h8a.5.5 0 0 0 .5-.5V8.25"/><rect x="2.25" y="5.75" width="11.5" height="2.5" rx=".5"/><path d="M8 5.75V13"/><path d="M8 5.75S7.5 2.25 5.5 2.25a1.75 1.75 0 0 0 0 3.5z"/><path d="M8 5.75S8.5 2.25 10.5 2.25a1.75 1.75 0 0 1 0 3.5z"/></svg>`;
export const ICON_DB = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><ellipse cx="8" cy="3.75" rx="4.75" ry="2"/><path d="M3.25 3.75v8.5c0 1.1 2.13 2 4.75 2s4.75-.9 4.75-2v-8.5"/><path d="M3.25 8c0 1.1 2.13 2 4.75 2s4.75-.9 4.75-2"/></svg>`;
export const ICON_TABLE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="2.25" y="3.25" width="11.5" height="9.5" rx="1"/><path d="M2.25 6.5h11.5M6.5 6.5v6.25"/></svg>`;
// A closed door in its frame — the outer frame, the inset leaf, and a solid knob
// on the right. The generic "door / open / exit" glyph.
export const ICON_DOOR = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="1.75" width="10" height="12.5" rx=".5"/><path d="M5 14.25V4.25a.5.5 0 0 1 .5-.5h5a.5.5 0 0 1 .5.5v10"/><circle cx="9" cy="9" r=".8" fill="currentColor" stroke="none"/></svg>`;

// Tree/explorer glyphs: a folder, a document, and the expand chevron (points
// right; rotate it 90° via CSS for the expanded state). ICON_FOLDER is a
// consumer's to use — a header button, a legend, a create input — and NOT
// `<siegel-tree>`'s branch glyph: a branch's glyph there is its chevron, which
// already says "there is something inside" (see siegel-tree.js).
export const ICON_FOLDER = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M1.75 12.25V4.25a1 1 0 0 1 1-1H6l1.5 1.75h5.75a1 1 0 0 1 1 1v6.25a1 1 0 0 1-1 1H2.75a1 1 0 0 1-1-1z"/></svg>`;
export const ICON_FILE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M9.5 1.75H4a.5.5 0 0 0-.5.5v11.5a.5.5 0 0 0 .5.5h8a.5.5 0 0 0 .5-.5V4.75z"/><path d="M9.5 1.75v3h3"/></svg>`;
// The "create" variants of the file/folder glyphs — the shape with a small `+`
// in its bottom-right corner. Use these for "new file" / "new folder" actions
// where ICON_NEW (a bare +) is ambiguous about what is being created.
//
// The `+` is one drawing, used twice: four units across and down, sharing
// the outline's stroke. Only its position differs to keep it clear of the shape.
//
// THE PAGE IS ICON_FILE'S PAGE, with its bottom-right corner opened exactly as
// ICON_FOLDER_ADD opens the folder's: the base runs to x=8 and stops, the right
// wall comes down from the fold to y=8 and stops, and the `+` stands in the gap.
// It was a DIFFERENT page before — the top edge stopped at x=8, the fold hung in
// mid-air as an unattached stub, and the result read as a torn sheet rather than
// as a sheet. Two glyphs a menu shows one under the other have to be the same
// document with one thing added, or the pair says the file is what changed.
//
// The cut-out is what makes the `+` a badge instead of something written on the
// page. With the outline closed around it the two read as one drawing and neither
// is legible at 16px.
export const ICON_FILE_ADD = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M9.5 1.75H4a.5.5 0 0 0-.5.5v11.5a.5.5 0 0 0 .5.5h4M12.5 8V4.75L9.5 1.75"/><path d="M9.5 1.75v3h3"/><path d="M11.5 9.5v4M9.5 11.5h4"/></svg>`;
// A page with two lines of text: a written document or report, where ICON_FILE is
// any file in a tree.
export const ICON_DOCUMENT = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M 9.333 1.333 H 4 a 1.333 1.333 0 0 0 -1.333 1.333 v 10.667 a 1.333 1.333 0 0 0 1.333 1.333 h 8 a 1.333 1.333 0 0 0 1.333 -1.333 V 5.333 z M 9.333 1.333 v 4 h 4 M 5.333 8 h 5.333 M 5.333 10.667 h 5.333"/></svg>`;
export const ICON_FOLDER_ADD = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M1.75 12.25V4.25a1 1 0 0 1 1-1H6l1.5 1.75h5.75a1 1 0 0 1 1 1V8"/><path d="M1.75 12.25a1 1 0 0 0 1 1H8"/><path d="M11.5 9.5v4M9.5 11.5h4"/></svg>`;
// A pushpin. No longer used by the kit itself: the open-page strip carries "keep
// this tab" as a double-click and shows the state in the typeface (italic while
// transient), so every tab wears the same close X and nothing draws a pin. Kept
// because "keep this" is a common app-side affordance — a favourite, a sticky row.
export const ICON_PIN = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M6.75 2.75h2.5v3.5l1.75 2h-6l1.75-2z"/><path d="M8 8.25v5"/></svg>`;
// A house outline — "set as home" (the page a rail-nav opens to by default).
// Pairs with ICON_HOME_FILLED, which marks the item that already is home; the
// two never both render on the same row.
export const ICON_HOME = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 8.25 8 3l5.5 5.25"/><path d="M4 7v6a.5.5 0 0 0 .5.5h7a.5.5 0 0 0 .5-.5V7"/></svg>`;
// The solid counterpart to ICON_HOME — "this is the current home", offered as
// the "remove as home" action on the one row that already carries it.
export const ICON_HOME_FILLED = `<svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M8 2 14 7.5v5.75a.75.75 0 0 1-.75.75H9.5V9.5h-3V14H2.75a.75.75 0 0 1-.75-.75V7.5z"/></svg>`;

// Klotz's chevron, turned right to preserve Siegel's disclosure/arrow rotations.
export const ICON_CHEVRON = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><path transform="rotate(-90 8 8)" d="M 4 6.4 l 4 4 4 -4"/></svg>`;

// A horizontal ellipsis — "the rest of the actions are in here", the overflow
// trigger for a strip that cannot show its buttons (siegel-tree's rows on a
// pointer with no hover). Filled dots stay legible at small control sizes.
export const ICON_MORE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><circle cx="3.5" cy="8" r="1.25"/><circle cx="8" cy="8" r="1.25"/><circle cx="12.5" cy="8" r="1.25"/></svg>`;

// A funnel — the universal "narrow this set down" glyph, for a control that opens
// or holds filter criteria. Distinct from ICON_SLIDERS on purpose: sliders are for
// adjusting how something is shown, a funnel is for choosing what is in it.
export const ICON_FUNNEL = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M2.25 3.25h11.5L9.25 8.25v5l-2.5-1.5v-3.5z"/></svg>`;

// Straight arrows: up, down, and the double-headed one between them — the ↑ ↓ ↕ a
// sortable heading shows (siegel-table), drawn rather than typed because iOS turns
// those characters into emoji.
export const ICON_ARROW_UP = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 13.5v-11M4.5 6 8 2.5 11.5 6"/></svg>`;
export const ICON_ARROW_DOWN = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 2.5v11M4.5 10 8 13.5 11.5 10"/></svg>`;
export const ICON_ARROW_UP_DOWN = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 2.5v11M5 5.5l3-3 3 3M5 10.5l3 3 3-3"/></svg>`;

// Theme glyphs for theme controls: sun = Light, moon = Dark.
export const ICON_SUN = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" width="16" height="16" stroke-linejoin="round"><circle cx="8" cy="8" r="2.56"/><path d="M 8 2 v 1.6 M 8 12.4 v 1.6 M 2 8 h 1.6 M 12.4 8 h 1.6 M 3.76 3.76 l 1.12 1.12 M 11.12 11.12 l 1.12 1.12 M 3.76 12.24 l 1.12 -1.12 M 11.12 4.88 l 1.12 -1.12"/></svg>`;
export const ICON_MOON = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><path d="M 12.4 9.76 A 4.96 4.96 0 0 1 6.24 3.6 a 5.2 5.2 0 1 0 6.16 6.16 z"/></svg>`;

// Nav-page glyphs (grid, sliders, globe, users, gear, person) — commonly used
// in the activity rail, which picks their size via its own CSS.
export const ICON_GRID = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="2" width="4.75" height="4.75" rx="0.75"/><rect x="9.25" y="2" width="4.75" height="4.75" rx="0.75"/><rect x="2" y="9.25" width="4.75" height="4.75" rx="0.75"/><rect x="9.25" y="9.25" width="4.75" height="4.75" rx="0.75"/></svg>`;
export const ICON_SLIDERS = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 5.25h11M2.5 10.75h11"/><circle cx="6" cy="5.25" r="1.6" fill="currentColor" stroke="none"/><circle cx="10" cy="10.75" r="1.6" fill="currentColor" stroke="none"/></svg>`;
export const ICON_GLOBE = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" width="16" height="16" stroke-linejoin="round"><circle cx="8" cy="8" r="5.6"/><path d="M 2.4 8 h 11.2"/><path d="M 8 2.4 a 8.32 8.32 0 0 1 0 11.2 8.32 8.32 0 0 1 0 -11.2"/></svg>`;
export const ICON_USERS = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M10.75 13.75v-1.25a2.6 2.6 0 0 0-2.6-2.6H4.35a2.6 2.6 0 0 0-2.6 2.6v1.25"/><circle cx="6.25" cy="4.65" r="2.6"/><path d="M14.25 13.75v-1.25a2.6 2.6 0 0 0-1.95-2.5"/><path d="M10.4 2.2a2.6 2.6 0 0 1 0 5"/></svg>`;
export const ICON_GEAR = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M 6.013 3.193 L 6.74 2.953 L 6.84 1.433 L 9.16 1.433 L 9.26 2.953 L 9.987 3.193 L 10.68 3.54 L 11.827 2.54 L 13.46 4.173 L 12.46 5.32 L 12.807 6.013 L 13.047 6.74 L 14.567 6.84 L 14.567 9.16 L 13.047 9.26 L 12.807 9.987 L 12.46 10.68 L 13.46 11.827 L 11.827 13.46 L 10.68 12.46 L 9.987 12.807 L 9.26 13.047 L 9.16 14.567 L 6.84 14.567 L 6.74 13.047 L 6.013 12.807 L 5.32 12.46 L 4.173 13.46 L 2.54 11.827 L 3.54 10.68 L 3.193 9.987 L 2.953 9.26 L 1.433 9.16 L 1.433 6.84 L 2.953 6.74 L 3.193 6.013 L 3.54 5.32 L 2.54 4.173 L 4.173 2.54 L 5.32 3.54 Z"/><circle cx="8" cy="8" r="2"/></svg>`;
export const ICON_SIGN_OUT = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M6.25 13.25H3.75a.5.5 0 0 1-.5-.5V3.25a.5.5 0 0 1 .5-.5h2.5"/><path d="M10.25 5.25 13 8l-2.75 2.75"/><path d="M13 8H6.5"/></svg>`;

// More nav/content glyphs, same family. Named for what they depict so any
// surface can reuse them: a lifebuoy, a headset, a price tag, a bar chart, a box, an
// activity/heartbeat trace, a picture, and a clock-with-rewind (history).
export const ICON_LIFEBUOY = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><circle cx="8" cy="8" r="6.25"/><circle cx="8" cy="8" r="2.5"/><path d="M3.6 3.6 6.2 6.2M9.8 9.8l2.6 2.6M12.4 3.6 9.8 6.2M6.2 9.8 3.6 12.4"/></svg>`;
// A headset: support, someone to talk to. ICON_LIFEBUOY stays for help.
export const ICON_HEADSET = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M 2.667 8.667 v -1.333 a 5.333 5.333 0 0 1 10.667 0 v 1.333 M 2.667 8 H 2 v 4 h 2.667 v -4 H 2.667 Z m 10.667 0 h 0.667 v 4 h -2.667 v -4 h 2 Z m 0 4 v 0.667 a 2 2 0 0 1 -2 2 h -2"/></svg>`;
export const ICON_TAG = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><path d="M 8.4 2.496 H 3.248 a 0.496 0.496 0 0 0 -0.496 0.496 v 5.152 a 0.496 0.496 0 0 0 0.152 0.352 l 5 5 a 0.496 0.496 0 0 0 0.704 0 l 4.848 -4.848 a 0.496 0.496 0 0 0 0 -0.704 l -5 -5 a 0.496 0.496 0 0 0 -0.352 -0.152 z"/></svg>`;
export const ICON_CHART = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M 2.667 2 v 11.333 h 11.333 M 5.333 10 v -2.667 m 3.333 2.667 V 4.667 m 3.333 5.333 V 2.667"/></svg>`;
// The dial parts around the needle so the two strokes stay distinct at 16px.
export const ICON_GAUGE = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><path d="M 2.248 11.504 A 5.752 5.752 0 0 1 9.296 5.904"/><path d="M 12.504 7.92 A 5.752 5.752 0 0 1 13.752 11.504"/><path d="M 8 11.504 11.072 6.64"/><circle cx="8" cy="11.504" r="1"/></svg>`;
export const ICON_BOX = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 2.25 13.25 5 8 7.75 2.75 5z"/><path d="M2.75 5v6L8 13.75 13.25 11V5"/><path d="M8 7.75v6"/></svg>`;
export const ICON_ACTIVITY = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M1.75 8h3l1.5-4.25L9 12.25l1.5-4.25h3.75"/></svg>`;
// The horizon alone identifies the picture; the small sun was a smudge at 16px.
export const ICON_IMAGE = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><rect x="2.248" y="3.248" width="11.504" height="9.504" rx="1"/><path d="M 2.496 11 6 8 l 2.248 2 2.752 -2.752 2.496 2.496"/></svg>`;
export const ICON_HISTORY = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16">${TURN_BACK}<path d="M 8 5.504 V 8 l 1.904 1.152"/></svg>`;
// A circled lower-case i — "show me the detail behind this row". The row-level
// counterpart to opening a record: an icon-btn wearing this in a table's last
// column is the kit's "details" affordance.
export const ICON_INFO = circledSymbol('M8 7.5v3', 5.5);
// Circled question mark — help, questions and FAQs; paired with ICON_INFO.
export const ICON_QUESTION = circledSymbol('M6.75 6.75a1.25 1.25 0 0 1 2.5 0c0 1-1.25 .9-1.25 1.75', 10.5);
export const ICON_HELP = ICON_QUESTION;
export const ICON_EXCLAMATION = circledSymbol('M8 5.5v3', 10.5);
// A movie camera — a lens body with the tape reel behind it. The "there is
// footage of this" glyph: a row-level counterpart to ICON_INFO for records that
// carry a recording, and the header glyph for a video panel. (ICON_IMAGE is the
// still-frame twin; keep them distinct where a surface offers both.)
export const ICON_CAMERA = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="1.75" y="4.75" width="8.5" height="6.5" rx="1"/><path d="M10.25 8.25l3.25-1.9v3.3l-3.25-1.9z"/></svg>`;
// A month-leaf calendar — binder rings over a header band. The "date / when"
// glyph (the date-picker trigger wears it).
export const ICON_CALENDAR = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="2.25" y="3.25" width="11.5" height="10" rx="1"/><path d="M2.25 6.5h11.5"/><path d="M5.25 1.75v2.5M10.75 1.75v2.5"/></svg>`;
// A sealed envelope — the body rect with its flap creasing down from the two top
// corners. The "mail / message" glyph, for a surface about email. The flap meets
// each corner arc at 45° rather than starting on the side wall, so the crease
// still reads as a flap at 16 px instead of as a stray chevron.
export const ICON_ENVELOPE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="2.25" y="3.75" width="11.5" height="8.5" rx="1"/><path d="M2.55 4.05 8 8.6 13.45 4.05"/></svg>`;

// Taken from Klotz (hrkltz's consumer design system) and redrawn on this
// grid. The two kits kept separate drawings of the same ideas until each was
// asked what the other had; these are the ones Siegel did not have, and the
// verbs and objects above went the other way.
//
// RESCALED, NOT PASTED. Klotz draws on a 20-grid at stroke 1.6. Every length
// here was multiplied by 16/20; stroke weight now follows FleetManager. Each
// keeps the default width and height consumers rely on. Consider Klotz too
// when changing a shared symbol; its heavier styling remains independent.
// Three bars — the mark that opens a drawer. There is no matching close
// glyph on purpose: a drawer closes by the button that opened it, and a second mark
// would be a second story about one control.
export const ICON_MENU = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M2.8 4.4h10.4M2.8 8h10.4M2.8 11.6h10.4"/></svg>`;
// A four-point spark: what a machine-learning model does. Generic on
// purpose, because the platform vendors' own intelligence glyphs are trademarks.
export const ICON_SPARK = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M8 2.4c0.48 3.2 2.4 5.12 5.6 5.6 -3.2 0.48 -5.12 2.4 -5.6 5.6 -0.48 -3.2 -2.4 -5.12 -5.6 -5.6 3.2 -0.48 5.12 -2.4 5.6 -5.6z"/></svg>`;
// The draw tool. With ICON_ERASER and ICON_POINTER, a sketch app's
// three verbs.
export const ICON_PENCIL = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M2.4 13.6l0.96 -3.2L10.4 3.36 12.64 5.6 5.6 12.64z"/><path d="M9.6 4.16L11.84 6.4"/></svg>`;
export const ICON_ERASER = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" width="16" height="16" stroke-linecap="round"><rect x="3.2" y="6.4" width="8" height="5.6" rx="1.2" transform="rotate(-20 7.2 9.2)"/><path d="M 3.2 13.6 h 9.6"/></svg>`;
export const ICON_POINTER = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M4 2.4l8 6.8 -4 0.64 2.08 4 -1.6 0.8 -2.08 -4L4 13.6z"/></svg>`;
// An OPEN wireframe volume, where ICON_BOX is a closed carton: one is a
// thing rendered, the other a thing shipped.
export const ICON_CUBE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M8 2l5.2 3v6L8 14l-5.2 -3v-6z"/><path d="M8 8v6M8 8L2.8 5M8 8l5.2 -3"/></svg>`;
// Three render modes, and a family on purpose: the same arc, each gaining
// one mark. Options that differ by DEGREE should look like it, or the operator reads
// the labels every time.
export const ICON_ARC = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M3.2 11.2c3.2 -6.4 6.4 -6.4 9.6 0" stroke-linecap="round"/></svg>`;
export const ICON_ARC_SHADOW = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M3.2 11.2c3.2 -6.4 6.4 -6.4 9.6 0" stroke-linecap="round"/><ellipse cx="8" cy="12.8" rx="4.8" ry="1.2"/></svg>`;
export const ICON_ARC_FADE = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M3.2 11.2c3.2 -6.4 6.4 -6.4 9.6 0" stroke-linecap="round" opacity=".4"/><path d="M3.2 11.2c3.2 -6.4 6.4 -6.4 9.6 0" stroke-linecap="round" stroke-dasharray="0.8 2.4"/></svg>`;
// The machine in the seats family; ICON_EMPTY and ICON_USER supply its peers.
export const ICON_ROBOT = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="3.2" y="5.6" width="9.6" height="7.6" rx="2"/><path d="M6 8.8v1.28M10 8.8v1.28M8 5.6V3.6"/><circle cx="8" cy="2.56" r="0.88"/></svg>`;
// Subjects — the marks a surface reaches for when it lets someone file
// things by topic. A starting vocabulary and NOT a taxonomy: there is no glyph for
// "politics", there is a building, and what a product means by it is its own business.
export const ICON_STAR = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M8 2.4l1.36 3.72L13.33 6.27 10.21 8.72 11.29 12.53 8 10.32 4.71 12.53 5.79 8.72 2.67 6.27 6.64 6.12z"/></svg>`;
export const ICON_HEART = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M8 13.28L3.52 8.8a3.2 3.2 0 1 1 4.48 -4.32 3.2 3.2 0 1 1 4.48 4.32z"/></svg>`;
export const ICON_BOLT = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linejoin="round" stroke-linecap="round"><path d="M9.12 2.08L4 9.12h3.52l-0.64 4.8L12 6.88h-3.52z"/></svg>`;
export const ICON_LEAF = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M3.84 12.16a8 8 0 0 1 8.8 -8.8 8 8 0 0 1 -8.8 8.8z"/><path d="M3.84 12.16L9.28 6.72"/></svg>`;
export const ICON_FLASK = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M6.08 2.24h3.84"/><path d="M6.72 2.24v4.16l-3.44 5.84A1.2 1.2 0 0 0 4.32 14.08h7.36a1.2 1.2 0 0 0 1.04 -1.84l-3.44 -5.84V2.24"/><path d="M4.96 10.08h6.08"/></svg>`;
export const ICON_BALL = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><circle cx="8" cy="8" r="5.6"/><path d="M4.32 3.68a7.2 7.2 0 0 1 0 8.64"/><path d="M11.68 3.68a7.2 7.2 0 0 0 0 8.64"/></svg>`;
export const ICON_CHIP = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="4.8" y="4.8" width="6.4" height="6.4" rx="1.12"/><path d="M6.72 2.4v2.4M9.28 2.4v2.4M6.72 11.2v2.4M9.28 11.2v2.4M2.4 6.72h2.4M2.4 9.28h2.4M11.2 6.72h2.4M11.2 9.28h2.4"/></svg>`;
export const ICON_BOOK = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M8 4.96v8"/><path d="M8 4.96C6.88 4 5.44 3.52 3.52 3.52H2.56v8h0.96c1.92 0 3.36 0.48 4.48 1.44"/><path d="M8 4.96c1.12 -0.96 2.56 -1.44 4.48 -1.44h0.96v8h-0.96c-1.92 0 -3.36 0.48 -4.48 1.44"/></svg>`;
export const ICON_FILM = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><rect x="2.4" y="3.52" width="11.2" height="8.96" rx="1.28"/><path d="M5.28 3.52v8.96M10.72 3.52v8.96"/><path d="M2.4 6.08h2.88M2.4 9.92h2.88M10.72 6.08H13.6M10.72 9.92H13.6"/></svg>`;
export const ICON_BUILDING = `<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round"><path d="M2.56 6.72L8 3.2l5.44 3.52"/><path d="M3.84 6.72v5.6M6.56 6.72v5.6M9.44 6.72v5.6M12.16 6.72v5.6"/><path d="M2.72 12.32h10.56"/></svg>`;

export const ICON_EMPTY = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" width="16" height="16" stroke-linejoin="round"><circle cx="8" cy="8" r="5.2" stroke-dasharray="2.08 2.08"/></svg>`;

export const ICON_RUN_FILLED = `<svg viewBox="0 0 16 16" fill="currentColor" width="16" height="16"><path d="${RUN_PATH}"/></svg>`;

export const ICON_STOP_FILLED = `<svg viewBox="0 0 16 16" fill="currentColor" width="16" height="16">${STOP_RECT}</svg>`;

export const ICON_THEME = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" width="16" height="16" stroke-linejoin="round"><circle cx="8" cy="8" r="5.2"/><path d="M 8 2.8 a 5.2 5.2 0 0 1 0 10.4 z" fill="currentColor" stroke="none"/></svg>`;

export const ICON_UNDO = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16">${TURN_BACK}</svg>`;

export const ICON_USER = `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="${STROKE_WIDTH}" stroke-linecap="round" stroke-linejoin="round" width="16" height="16"><path d="M 13.352 14 v -1.296 a 2.648 2.648 0 0 0 -2.648 -2.648 H 5.296 A 2.648 2.648 0 0 0 2.648 12.704 V 14"/><circle cx="8" cy="4.648" r="2.648"/></svg>`;

// Compatibility names; each shares the canonical drawing rather than a copy.
export const ICON_ACCOUNT = ICON_USER;
export const ICON_PERSON = ICON_USER;
export const ICON_DISCARD = ICON_UNDO;
export const ICON_SEAT_EMPTY = ICON_EMPTY;
export const ICON_THEME_SYSTEM = ICON_THEME;
