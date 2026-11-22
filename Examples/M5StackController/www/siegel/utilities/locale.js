// The one place Siegel decides which locale to format in, and the cache that
// makes asking cheap.
//
// The problem it solves: `Intl` DOES NOT READ `<html lang>`. Passing `undefined`
// means "whatever this browser is set to", so before this module a table, a chart
// and a date picker on one page could each render the same day differently
// depending on the operator's laptop. Every Intl construction in the kit now goes
// through here instead.
//
//   import { dateTimeFormat, numberFormat } from '@siegel/utilities/locale.js';
//   dateTimeFormat({ year: 'numeric', month: '2-digit', day: '2-digit' }).format(d)
//
// TWO KNOBS, on <html>, and the split is deliberate:
//
//   lang="de"                  the UI LANGUAGE — what the words are in
//   data-siegel-locale="de-DE" the FORMAT SHAPE — separators, order, clock
//
// A consumer that wants German number shapes under English prose sets only the
// second. Folding them into one would force `lang="de"` on an English page, which
// tells every screen reader to pronounce English text with a German voice — a
// real regression for a real user in exchange for one fewer attribute.
//
// Plural selection follows `lang`, not the format locale: the category decides
// which WORD you write, so it belongs with the language.
//
// Nothing here negotiates. The app already wrote `lang` and knows what it serves;
// `Intl.*.supportedLocalesOf` filters against the runtime's ICU data rather than
// against your catalogue, so it could not help anyway. Picking a locale from an
// `Accept-Language` header is server work — see rustlet-http.
//
// DELIBERATELY NOT HERE: `Intl.ListFormat` and `RelativeTimeFormat` (no consumer),
// `Intl.DurationFormat` (a hard TypeError below ~85% of browsers), `getTextInfo()`
// for direction (`getComputedStyle(el).direction` reflects what actually rendered,
// including `dir="auto"`), and `Temporal` (Safari ships it in no version — this is
// recorded so it is not re-litigated every year).
//
// SWITCHING AT RUNTIME is `setLocale()` plus one event — see the LANGUAGE_CHANGED
// block below for why an event is needed at all when the caches invalidate
// themselves.

// Formatter caches are keyed by locale and self-invalidate: every accessor reads
// the attribute again and drops the cache when it changed. That is what makes a
// runtime locale switch free — no MutationObserver, no listener registry, no
// re-render plumbing, and no boot script. The read is a property access on
// documentElement; constructing an Intl formatter is orders of magnitude dearer,
// which is the whole reason this cache exists.
const CACHES = new Map();

// Options come from source literals, so key order is stable in practice — but
// sorting costs nothing and stops two spellings of the same options from building
// two formatters.
function optionsKey(options) {
  if (!options) return '';
  return JSON.stringify(options, Object.keys(options).sort());
}

function memo(kind, Ctor, locale, options) {
  let entry = CACHES.get(kind);
  if (!entry || entry.locale !== locale) {
    entry = { locale, map: new Map() };
    CACHES.set(kind, entry);
  }
  const key = optionsKey(options);
  let made = entry.map.get(key);
  if (!made) {
    made = new Ctor(locale, options);
    entry.map.set(key, made);
  }
  return made;
}

// An invalid tag makes EVERY Intl constructor throw RangeError, so a typo in one
// `lang` attribute would take down every formatted cell on the page. Validate
// once here and fall through to the next source instead.
function canonical(tag) {
  if (typeof tag !== 'string') return null;
  const trimmed = tag.trim();
  if (!trimmed) return null;
  try {
    return Intl.getCanonicalLocales(trimmed)[0] ?? null;
  } catch {
    return null;
  }
}

// The runtime's own choice, as a real tag rather than `undefined` — so callers
// always have a string to compare and cache on.
const runtimeLocale = () => new Intl.DateTimeFormat().resolvedOptions().locale;

// The UI language: `<html lang>`, canonicalised. Falls back to the browser's own
// locale when the document does not say — which is the pre-existing behaviour, so
// a page that never sets `lang` renders exactly as it did before this module.
export function uiLocale() {
  return canonical(document.documentElement.lang) ?? runtimeLocale();
}

// The format shape: `<html data-siegel-locale>`, else the UI language. This is the
// knob a consumer reaches for to pin `04.08.2026` and `1.234,5` without touching
// what language the page claims to be in.
export function formatLocale() {
  return canonical(document.documentElement.dataset.siegelLocale) ?? uiLocale();
}

export const numberFormat = (options) => memo('number', Intl.NumberFormat, formatLocale(), options);
export const dateTimeFormat = (options) => memo('datetime', Intl.DateTimeFormat, formatLocale(), options);
export const collator = (options) => memo('collator', Intl.Collator, formatLocale(), options);
// Language, not format shape — see the header.
export const pluralRules = (options) => memo('plural', Intl.PluralRules, uiLocale(), options);

// ---------------------------------------------------------------------------
// Switching at runtime
// ---------------------------------------------------------------------------
// TWO SEPARATE FACTS, and conflating them cost this kit a documented hack.
//
//   1. The formatter caches above are CORRECT after a switch with no help from
//      anyone: every accessor re-reads the attribute and drops its cache when the
//      tag moved. No MutationObserver, no invalidation call.
//   2. Nothing REPAINTS. A component that already rendered holds finished text in
//      its shadow root, and correct formatters nobody asks again change nothing on
//      screen.
//
// (1) is why this module has no observer. (2) is why it has this event. The old
// advice — "write the attribute and repaint whatever is on screen,
// `table.rows = table.rows` is enough" — is (2) pushed onto the app as a trick
// that re-assigns a model to provoke a paint. It works and it is unreadable, and
// it does not scale past the two components a demo happens to hold.
//
// LANGUAGE_CHANGED is the one signal for BOTH axes — the words (`utilities/strings.js`)
// and the shapes (this module) — because a language switch almost always moves both
// and two events would mean two repaints of the same nodes. Every component that
// formats through this module or reads `t()` listens for it and re-renders; an app
// listens to the same event for its own text, which is the larger three quarters.
//
// It is a `window` event and NOT composed-bubbling from an element (the `emit()`
// shape in utilities/element.js) because it has no source node: the language did not
// change *at* some point in the tree, it changed for the document.
export const LANGUAGE_CHANGED = 'siegel:language';

// Announce that the words or the format shape moved. Idempotent and cheap — the
// listeners are component `render()`s, which are coalesced, so calling
// `setLocale()` and `setStrings()` back to back costs one paint and not two.
export function emitLanguageChange() {
  window.dispatchEvent(new CustomEvent(LANGUAGE_CHANGED));
}

// Write the two `<html>` knobs and announce it. `null` REMOVES an attribute, which
// is how you fall back to the browser's own choice; a key left out is left alone,
// so `setLocale({ locale: 'de-DE' })` re-shapes numbers without touching what
// language the page claims to be in.
//
// Validation is deliberately NOT here. `canonical()` already refuses a bad tag on
// every read, so a typo degrades to the next source instead of throwing — and an
// attribute that says exactly what the caller asked for is easier to debug than one
// silently corrected on the way in.
export function setLocale({ lang, locale } = {}) {
  const root = document.documentElement;
  if (lang !== undefined) {
    if (lang == null || lang === '') root.removeAttribute('lang');
    else root.setAttribute('lang', String(lang));
  }
  if (locale !== undefined) {
    if (locale == null || locale === '') delete root.dataset.siegelLocale;
    else root.dataset.siegelLocale = String(locale);
  }
  emitLanguageChange();
}

// ---------------------------------------------------------------------------
// Calendar days vs instants
// ---------------------------------------------------------------------------
// A bare `YYYY-MM-DD` is a calendar day. `new Date` parses it as UTC midnight, so
// formatting it in local time lands on the previous day at any negative UTC
// offset. These three keep that distinction in one place so a formatter and a
// filter can never disagree about which day a value is on.

const DAY_ONLY = /^\d{4}-\d{2}-\d{2}$/;

export const isDayString = (value) => typeof value === 'string' && DAY_ONLY.test(value);

export function dayToUtc(iso) {
  const [y, m, d] = iso.split('-').map(Number);
  return Date.UTC(y, m - 1, d);
}

// Format a day-only ISO string without ever letting it become an instant.
export function formatDay(iso, options) {
  return dateTimeFormat({ ...options, timeZone: 'UTC' }).format(dayToUtc(iso));
}

// ---------------------------------------------------------------------------
// Week start
// ---------------------------------------------------------------------------
// 1 = Monday … 7 = Sunday, matching CLDR. Real values: 7 for en-US, ja-JP, pt-BR,
// hi-IN and he-IL; 6 for ar-EG and fa-IR; 1 across most of Europe.
//
// Three-legged on purpose. `getWeekInfo()` is the standard shape; Chrome 99-129
// shipped the same data as a non-callable `weekInfo` GETTER; Firefox shipped
// neither for a long while. The final fallback is Monday, which is what this kit
// hardcoded before — so on an engine with no week data the behaviour is exactly
// what it always was and this cannot regress.
export function firstDayOfWeek() {
  try {
    const loc = new Intl.Locale(formatLocale());
    const info = typeof loc.getWeekInfo === 'function' ? loc.getWeekInfo() : loc.weekInfo;
    const first = info?.firstDay;
    if (Number.isInteger(first) && first >= 1 && first <= 7) return first;
  } catch {
    /* no week data on this engine — Monday, as before */
  }
  return 1;
}
