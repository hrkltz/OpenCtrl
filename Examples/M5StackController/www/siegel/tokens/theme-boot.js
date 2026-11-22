// Pre-paint theme bootstrap. Load as a *classic, blocking* script in <head>
// (NOT type="module" — modules are deferred and would flash the wrong theme):
//
//   <script src="%BASE_PATH%/siegel/tokens/theme-boot.js"></script>
//
// Applies the stored Light/Dark choice before first paint; "System" (or no
// stored value) leaves <html> untouched so the media query in tokens.css
// decides. Mirrors lib/theme.js — the two must agree on the storage key.
(function () {
  try {
    var t = localStorage.getItem('siegel-theme');
    if (t === 'light' || t === 'dark') document.documentElement.dataset.theme = t;
  } catch (e) { /* storage disabled — fall back to System */ }
})();
