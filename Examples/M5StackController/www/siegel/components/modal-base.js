// Shared chrome for the modal helpers (confirm-modal, task-modal, form-dialog).
//
// Modals are app-level <dialog> elements shown via showModal(): they live in
// the browser's top layer — centered over the whole window with a backdrop
// that blocks all interaction beneath, regardless of which component asked.
// Because <dialog> sits in the light DOM, its styles are injected once into
// <head> (class-scoped, so nothing leaks) rather than adopted into a shadow
// root — through the one shared injector every light-DOM surface in the kit
// uses (see utilities/element.js), not a private copy per file.
//
// THE FAMILY'S CONVENTIONS, so the three dialogs read as one thing:
//   • `data-act="…"` is the hook attribute on every action node. One vocabulary
//     — `data-role` used to mean the same thing in form-dialog and `data-act` in
//     task-modal, so the same button had two names depending on which helper
//     drew it.
//   • Status text (pending / error / success) sits in a LIVE REGION: `.modal-msg`
//     and `.step-status` carry `role="status"`, or `role="alert"` when they hold
//     an error. A modal that swaps its body between steps otherwise announces
//     nothing at all to a screen reader — the operator is told the dialog is
//     working, or that it failed, only if they can see it.
//   • Colours come from tokens. No `rgba()`, no `#fff`, no `black`.

import { FONT_FAMILY, injectLightStyles } from '../utilities/element.js';
import { buttonStyles, inputStyles } from '../utilities/styles.js';

const STYLES = `
    dialog.siegel-modal {
      padding: 0;
      border: 1px solid var(--control-border);
      border-radius: 8px;
      background: var(--surface);
      color: var(--text);
      max-width: 24rem;
      width: calc(100% - 2rem);
      box-shadow: 0 12px 32px color-mix(in srgb, var(--shadow-color) 32%, transparent);
      font: 14px/1.5 ${FONT_FAMILY};
    }
    dialog.siegel-modal.wide { max-width: 32rem; }
    dialog.siegel-modal::backdrop {
      background: color-mix(in srgb, var(--page) 55%, transparent);
      backdrop-filter: blur(1px);
    }
    /* Dialogs are light DOM, so they miss the * { box-sizing: border-box }
       reset that adoptStyles injects into every component's shadow root.
       Without this, a width:100% input adds its padding+border on top of
       the content width and overflows the right padding — the body then
       looks lopsided (full padding left, none right). Scope the reset to
       the modal so it can't leak into the page. */
    .siegel-modal *, .siegel-modal *::before, .siegel-modal *::after { box-sizing: border-box; }
    .siegel-modal .modal-body { padding: 18px 20px; }
    .siegel-modal .modal-msg { margin: 0 0 16px; }
    .siegel-modal .modal-msg.pending { margin: 0; }
    .siegel-modal .modal-msg.error { color: var(--error); }
    .siegel-modal .modal-title { margin: 0 0 14px; font-size: 15px; font-weight: 600; }
    .siegel-modal .modal-actions { display: flex; justify-content: flex-end; gap: 8px; }
    .siegel-modal .modal-row { display: flex; align-items: center; gap: 12px; }
    ${buttonStyles(['.siegel-modal .modal-btn'])}
    ${inputStyles('.siegel-modal')}
    .siegel-modal .modal-btn:focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; }
    .siegel-modal .modal-btn[hidden] { display: none; }
    /* Mirrors the shared .spinner in utilities/styles.js — kept here too because
       dialogs are light DOM and don't adopt the constructable sheet. Rotation
       is gated on prefers-reduced-motion. */
    .siegel-modal .modal-spinner {
      width: 22px; height: 22px; flex: none; border-radius: 50%;
      border: 2.5px solid var(--control-border);
      border-top-color: var(--accent);
    }
    @media (prefers-reduced-motion: no-preference) {
      .siegel-modal .modal-spinner { animation: siegel-modal-spin 0.8s linear infinite; }
    }
    @keyframes siegel-modal-spin { to { transform: rotate(360deg); } }
    /* Spacing only for a <siegel-progress> dropped into the pending state — the
       bar's own visuals live in that component. Hidden until a task reports a
       ratio (see task-modal's progress()), so [hidden] collapses the margin too. */
    .siegel-modal siegel-progress { margin-top: 14px; }
    .siegel-modal siegel-progress[hidden] { display: none; }
`;

export function ensureModalStyles() {
  injectLightStyles('modal', STYLES);
}

// A styled top-layer <dialog>, ready to fill and showModal().
export function createDialog({ wide = false } = {}) {
  ensureModalStyles();
  const dialog = document.createElement('dialog');
  dialog.className = `siegel-modal${wide ? ' wide' : ''}`;
  return dialog;
}

// The live-region role a status line should carry: an error interrupts
// (assertive), anything else waits its turn (polite). One helper so all three
// dialogs classify the same way.
export function statusRole(isError) {
  return isError ? 'alert' : 'status';
}
