// Shared HTML-escaping helpers for views that build markup via template
// strings.
//
// THE RULE: escape or textContent, never raw. Anything interpolated into
// innerHTML that did not originate as a literal in this repo — names, ids, row
// data, error messages, a caller's label — goes through one of these, or is
// assigned as `.textContent` afterwards.
//
// Both escape quotes, so picking the wrong one is a style mistake and never an
// injection: `escapeHtml` in an attribute position used to leave `"` intact,
// which is one typo away from breaking out of the attribute. `escapeAttr`
// remains the convention for attribute positions — it says what the value is
// for — but the two are now interchangeable as far as safety goes.
//
// Neither is safe for unquoted attributes, inside <script>/<style>, or for a
// URL in href/src: quote every attribute, and validate URLs separately.

export function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) =>
    ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

export function escapeAttr(s) {
  return String(s).replace(/[&<>"']/g, (c) =>
    ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}
