// Show a centered, UI-blocking confirmation dialog. Resolves true if confirmed,
// false if cancelled (button, Escape, or backdrop click). `danger` styles the
// confirm button as a filled red action (use for deletes and stops).
//
//   const yes = await confirmModal({ message: 'Delete this key?',
//                                    confirmLabel: 'Delete', danger: true });
//
//   message       string  — the question (assigned as textContent, never HTML).
//   confirmLabel  string  — the commit button's text (default 'OK').
//   cancelLabel   string  — the dismiss button's text (default 'Cancel').
//   danger        boolean — filled red commit button.
//
// It is the family's DISMISSAL BASELINE: an accidental dismiss (Escape, a
// backdrop click) means cancel. task-modal's own confirm step and form-dialog's
// input step behave the same way, so "click outside" never means different
// things in two dialogs that look alike.

import { createDialog } from './modal-base.js';
import { escapeHtml } from '../utilities/html.js';
import { t } from '../utilities/strings.js';

export function confirmModal({
  // Defaults, not seizures: a caller's `confirmLabel` still wins. A default
  // PARAMETER is evaluated on every call, so `t()` here reads the catalogue in
  // force when the dialog opens rather than the one at module load.
  message,
  confirmLabel = t('confirm.ok', 'OK'),
  cancelLabel = t('confirm.cancel', 'Cancel'),
  danger = false,
}) {
  return new Promise((resolve) => {
    const dialog = createDialog();
    dialog.innerHTML = `
      <form method="dialog" class="modal-body">
        <p class="modal-msg" role="status"></p>
        <div class="modal-actions">
          <button type="submit" value="cancel" data-act="cancel" class="modal-btn">${escapeHtml(cancelLabel)}</button>
          <button type="submit" value="confirm" data-act="confirm" class="modal-btn ${danger ? 'danger' : 'primary'}">${escapeHtml(confirmLabel)}</button>
        </div>
      </form>`;
    // Assign the message as text so it can't inject markup.
    dialog.querySelector('.modal-msg').textContent = message;

    // Dismiss as "cancel" when clicking the backdrop (outside the form).
    dialog.addEventListener('click', (event) => {
      if (event.target === dialog) dialog.close('cancel');
    });

    dialog.addEventListener('close', () => {
      const confirmed = dialog.returnValue === 'confirm';
      dialog.remove();
      resolve(confirmed);
    });

    document.body.appendChild(dialog);
    dialog.showModal();
  });
}
