// Custom-element plumbing every component needs and none should hand-roll:
// the pre-upgrade property guard, the one event shape, and the light-DOM style
// injector the <dialog>/toast family shares.
//
//   import { adoptPreUpgrade, emit } from '../utilities/element.js';
//
// None of this is Siegel-specific policy — it is the Web-Components boilerplate
// that, left per-file, drifts. Keeping it here is what makes "every component
// looks the same" mechanical rather than a review note.

// ---------------------------------------------------------------------------
// Pre-upgrade property adoption
// ---------------------------------------------------------------------------

// A property written before the element upgraded lands as an OWN data property
// on the instance, which then shadows the class accessor forever — the setter
// never runs and the value is silently ignored. That happens routinely: markup
// is parsed, an app assigns `el.items = [...]`, and only later does the module
// defining the element load.
//
// Call this at the TOP of connectedCallback with every property the element
// takes from a host, INCLUDING public class fields (`authenticate`, `confirm`,
// `loadChildren`, …) — a class field is written by the constructor at upgrade
// time and would otherwise clobber an early assignment outright.
export function adoptPreUpgrade(el, names) {
  for (const name of names) {
    if (!Object.prototype.hasOwnProperty.call(el, name)) continue;
    const value = el[name];
    delete el[name];
    el[name] = value;
  }
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// The one event shape: bubbles and composed, so an app can listen at the shell
// level across every shadow boundary between it and the source. Returns the
// dispatch result (false when a cancelable event was prevented), so a caller
// that wants to be overridable can pass `cancelable`.
//
// Who may emit: METHODS and user gestures. A property setter is programmatic
// state, not an announcement — assigning `.selected` must not fire
// `selection-change`, or a host that mirrors its own event re-enters itself.
// The documented exceptions are listed in SCANME §8.
export function emit(el, type, detail = null, options = null) {
  return el.dispatchEvent(new CustomEvent(type, {
    detail,
    bubbles: true,
    composed: true,
    ...options,
  }));
}

// ---------------------------------------------------------------------------
// Light-DOM styles
// ---------------------------------------------------------------------------

// The font shorthand's family half, shared so the light-DOM surfaces (which
// can't inherit through a shadow root the way components do) stay on the one
// stack `tokens.css` gives <body>. Callers pick their own size/line-height —
// a toast is deliberately a step smaller than a dialog.
export const FONT_FAMILY = 'system-ui, -apple-system, Segoe UI, Roboto, sans-serif';

const injected = new Set();

// Inject a class-scoped stylesheet into <head>, once per name.
//
// Dialogs, toasts and their kin live in the browser's top layer or directly
// under <body> — outside every shadow root — so they cannot adopt a
// constructable sheet the way components do. This is the sanctioned exception
// to "shadow DOM for chrome": scope every rule under the feature's own class so
// nothing leaks into the page.
export function injectLightStyles(name, css) {
  if (injected.has(name)) return;
  injected.add(name);
  const style = document.createElement('style');
  style.dataset.siegel = name;
  style.textContent = css;
  document.head.appendChild(style);
}
