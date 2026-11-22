// The seam that lets an app replace the words Siegel renders. It is a lookup and
// nothing else.
//
//   import { t } from '@siegel/utilities/strings.js';
//   t('table.noRows', 'No rows match the filters.')
//   t('select.selectAllCount', (p) => `Select all (${p.count})`, { count: 12 })
//
//   setStrings({
//     'table.noRows': 'Keine Zeilen entsprechen den Filtern.',
//     'select.selectAllCount': (p) => `Alle auswählen (${p.count})`,
//   });
//
// WHY THIS IS SO SMALL. Siegel is about a QUARTER of a real app's visible text —
// 86 user-visible string sites here against 221 in a real consumer's frontend. So
// Siegel cannot solve translation and must not pretend to: an app that translates
// its own text needs Siegel to stop being the part it cannot reach, and nothing
// more. Catalogue format, locale negotiation, fallback chains and message syntax
// all stay with the app, which owns the other three quarters and has already
// chosen how it does them.
//
// THE ENGLISH LIVES AT THE CALL SITE, as the fallback argument. That buys a lot
// for one convention: there is no catalogue to keep in sync, no missing-key hole
// (an unset key renders English, not `table.noRows`), the source stays readable,
// and the sweep can land file by file instead of in one commit.
//
// AN OVERRIDE MAY BE A FUNCTION of the params, which is what makes the map above
// the primitive and JSON a convenience on top of it. A language with six plural
// forms can always write a function and call `pluralRules()` from
// utilities/locale.js itself, with no syntax in between.
//
// TWO WAYS TO GET A CATALOGUE, and the second one is built out of the first:
//
//   a `.js` module      — an object literal, functions allowed, no format at all.
//                         Nothing to parse, nothing to learn, nothing to version.
//   a `.json` file      — one file per language, `parseStrings()` turns it into the
//                         map above. Serve it, cache it, hand it to a translator.
//
// JSON cannot carry a function, so the JSON path DOES need a syntax — the exact
// thing this module set out to avoid. It is held to two rules and no more, because
// a message format is a syntax you can never delete once catalogues exist in the
// wild (see `parseStrings` for the full statement of the boundary):
//
//   "{name}"      is replaced from the params
//   "key.one" / "key.other" / … collapse into one plural-selecting entry
//
// Anything a translator cannot express in those two rules is a sign the string
// wants to be a function, and the `.js` path is right there.
//
// KEYS ARE API. Siegel has no version to deprecate through, so a renamed key
// silently falls back to English in every app that overrode the old one. Name them
// `component.thing` and treat a rename as a breaking change.
//
// WHAT NOT TO WRAP — the deny-list is bigger than it looks, and getting it wrong
// breaks behaviour rather than wording:
//   - anything compared against `event.key` ('Home', 'ArrowLeft', 'Escape', …).
//     the app frame and `siegel-date-picker` each use 'Home' as BOTH a key
//     literal and a user-facing label, in the same file.
//   - any `role=` or `aria-*` VALUE (the token, not the label).
//   - anything in a `CustomEvent` detail, or any `localStorage` value.
//   - the ~25 labels that are already host-overridable properties
//     (`.emptyLabel`, the modal label options, `.categories`). Those have a better
//     mechanism already; do not seize them.

import { escapeHtml, escapeAttr } from './html.js';
import { pluralRules, setLocale, emitLanguageChange } from './locale.js';

// Replace, not merge. A caller that wants merge semantics owns its own object and
// passes the whole thing — which keeps "what is overridden" answerable by reading
// one call site rather than by replaying a sequence of them.
let overrides = Object.create(null);

// Announces LANGUAGE_CHANGED, so components already on screen repaint. `t()` is
// read inside every `render()` and never captured at module scope, so the lookup
// was always live — what was missing is anyone asking again. See the block above
// LANGUAGE_CHANGED in utilities/locale.js.
export function setStrings(map) {
  overrides = Object.create(null);
  if (map) for (const [key, value] of Object.entries(map)) overrides[key] = value;
  emitLanguageChange();
}

export function strings() {
  return { ...overrides };
}

// Always returns a string. `fallback` is the English, and may itself be a function
// of the params — same shape an override takes, so a count-bearing string reads
// the same whether or not anyone has translated it.
export function t(key, fallback, params) {
  const found = Object.prototype.hasOwnProperty.call(overrides, key) ? overrides[key] : fallback;
  const value = typeof found === 'function' ? found(params ?? {}) : found;
  return value == null ? '' : String(value);
}

// ESCAPING IS THE REASON THESE TWO EXIST. Today's English is trusted because it is
// a literal in this repo's source; an app-supplied string is DATA, and most of the
// call sites interpolate into a template literal bound for innerHTML. Reach for
// `textContent`/`setAttribute` where the site allows it — that is the platform's
// own escaping and cannot be forgotten — and for these where it does not.
export const tHtml = (key, fallback, params) => escapeHtml(t(key, fallback, params));
export const tAttr = (key, fallback, params) => escapeAttr(t(key, fallback, params));

// ---------------------------------------------------------------------------
// JSON catalogues
// ---------------------------------------------------------------------------
// One flat file per language, which is the shape a translator, a TMS and an HTTP
// cache all already understand:
//
//   {
//     "$comment":               "Siegel — Deutsch",
//     "app.signOut":            "Abmelden",
//     "table.noRowsFiltered":   "Keine Zeilen entsprechen den Filtern.",
//     "select.selectAllCount.one":   "Eine auswählen",
//     "select.selectAllCount.other": "Alle {count} auswählen"
//   }
//
// WHERE THE SYNTAX STOPS, stated once so the next person does not extend it:
// `{name}` interpolation and `.category` plural suffixes. No nesting, no `select`
// (gender), no offsets, no date/number skeletons inside a string, and NO ESCAPE
// for a literal `{` — a string that needs one is a `.js` entry, which costs a
// function and buys the whole language. Every one of those omissions is a syntax
// that could never be removed afterwards, and each has a zero-syntax alternative
// already: the map `setStrings` takes is the primitive, and this is sugar.
//
// Everything else stays exactly where it was. No fetching happens in `parseStrings`,
// no locale is negotiated, no fallback CHAIN is walked (a key the file omits falls
// back to the English at its call site, which is the whole design), and nothing is
// escaped here — `tHtml`/`tAttr` do that at the boundary where the context is known.

// CLDR's six. `other` exists in every language and is the required member; a
// language with one form has only `other`, English adds `one`, Polish uses
// `one`/`few`/`many`/`other`, Arabic all six.
const PLURAL_CATEGORIES = new Set(['zero', 'one', 'two', 'few', 'many', 'other']);

// Detection is separate from replacement so the `g` flag's `lastIndex` — which is
// stateful across `.test()` calls and has cost more than one afternoon — never
// touches the predicate.
const HAS_PLACEHOLDER = /\{\w+\}/;
const PLACEHOLDER = /\{(\w+)\}/g;

// A plain string stays a plain string: the wrapper only appears where there is
// something to substitute, so the common key costs no call.
//
// An ABSENT param leaves `{name}` standing rather than rendering empty. A visible
// `{count}` is a bug report from the UI; a silently swallowed one is a string that
// reads fine and says the wrong thing.
//
// A param is inserted with `String()` and DELIBERATELY NOT through `numberFormat` —
// do not "fix" this. Running numbers through the formatter would group a year into
// `2,026`, and there is nothing in a placeholder name to distinguish a year from a
// quantity. It also keeps the two catalogue paths identical: the `.js` fallback
// `(p) => \`Select all (${p.count})\`` interpolates with `String()` too, so a key
// cannot change shape by being moved between them. A param that needs a locale
// shape is formatted by its caller, which is the only place that knows what the
// number MEANS.
function interpolate(text) {
  if (!HAS_PLACEHOLDER.test(text)) return text;
  return (params) => text.replace(PLACEHOLDER, (whole, name) =>
    (params && Object.prototype.hasOwnProperty.call(params, name) ? String(params[name]) : whole));
}

// The count comes from `params.count` BY CONVENTION — one name, stated here, so a
// catalogue never has to declare which param is the number. Siegel's own
// count-bearing keys all use it; an app key that wants a different name wants a
// `.js` function.
//
// The category is selected at CALL time, not at parse time, against whatever
// `<html lang>` says right then. That is what makes a catalogue survive being
// loaded a beat before the attribute it belongs to is written.
function selectPlural(forms) {
  return (params) => {
    const count = Number(params?.count);
    const category = Number.isFinite(count) ? pluralRules().select(count) : 'other';
    const chosen = forms[category] ?? forms.other;
    return typeof chosen === 'function' ? chosen(params) : chosen;
  };
}

// A JSON catalogue → the map `setStrings` takes. Pure: no fetch, no global write.
//
//   setStrings(parseStrings(await response.json()))
//
// `$`-prefixed keys are metadata and are skipped, so a file can carry its own
// `$comment` / `$locale` without them becoming lookups. A non-string value is
// skipped with a warning rather than thrown on — one bad line in a translated file
// must not take down the page, and English is always standing behind it.
export function parseStrings(json) {
  const map = Object.create(null);
  const plurals = new Map();                     // base key → { category: string|fn }

  for (const [key, value] of Object.entries(json ?? {})) {
    if (key.startsWith('$')) continue;
    if (typeof value !== 'string') {
      console.warn(`[siegel] catalogue key "${key}" is ${typeof value}, not a string — skipped.`);
      continue;
    }
    const cut = key.lastIndexOf('.');
    const tail = cut > 0 ? key.slice(cut + 1) : '';
    if (PLURAL_CATEGORIES.has(tail)) {
      const base = key.slice(0, cut);
      let forms = plurals.get(base);
      if (!forms) plurals.set(base, (forms = Object.create(null)));
      forms[tail] = interpolate(value);
    } else {
      map[key] = interpolate(value);
    }
  }

  for (const [base, forms] of plurals) {
    // No `other` means this was never a plural set — most likely a real key whose
    // last segment happens to collide with a category name. Put the entries back
    // verbatim instead of eating them: a wrong guess here would delete a string.
    if (forms.other === undefined) {
      for (const [category, form] of Object.entries(forms)) map[`${base}.${category}`] = form;
      continue;
    }
    map[base] = selectPlural(forms);
  }

  return map;
}

// Fetch a catalogue and put it in force — the whole of a language switch, in one
// call:
//
//   await loadStrings(href('/i18n/siegel.de.json'), { lang: 'de', locale: 'de-DE' })
//
// `href()` is `utilities/http.js` and is the CALLER's job, not this function's: a
// catalogue may be a static asset next to the page, an API response, or a blob
// URL, and a helper that silently prefixed a mount path would be wrong for two of
// those. This just fetches what it is given.
//
// The locale is applied FIRST so plural selection is already answering in the new
// language when the strings land. Both writes announce, both repaints coalesce
// into one paint (component `render()` is queued, not immediate), so the ordering
// costs nothing.
//
// A FAILURE LEAVES ENGLISH STANDING and returns `false`. It does not throw, and
// this is the one place in the kit where swallowing an error is the right answer:
// the English at every call site IS the fallback, so a 404 catalogue degrades to a
// completely usable page. Rejecting instead would abort whatever boot sequence
// called it and leave the app with no UI at all — a strictly worse outcome than
// the wrong language.
export async function loadStrings(url, { lang, locale } = {}) {
  try {
    const response = await fetch(url, { credentials: 'same-origin' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const json = await response.json();
    if (lang !== undefined || locale !== undefined) setLocale({ lang, locale });
    setStrings(parseStrings(json));
    return true;
  } catch (error) {
    console.warn(`[siegel] catalogue "${url}" did not load — staying on English.`, error);
    return false;
  }
}
