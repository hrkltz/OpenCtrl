// Shared by the app frame, top-layer dialogs/popouts and toast stacks. Read the browser's current
// insets on every platform; hosts may supply CSS lengths when a native wrapper
// has better information. Public overrides inherit; resolved values stay local
// so an embedded frame can opt out without changing its parent.
export const SAFE_AREA_STYLES = `
  :host {
    --_safe-area-top-clearance: 0px;
    --_safe-area-top: max(0px, var(--safe-area-top, calc(env(safe-area-inset-top, 0px)
      + max(0px, var(--safe-area-top-clearance, var(--_safe-area-top-clearance))))));
    --_safe-area-right: max(0px, var(--safe-area-right, env(safe-area-inset-right, 0px)));
    --_safe-area-bottom: max(0px, var(--safe-area-bottom, env(safe-area-inset-bottom, 0px)));
    --_safe-area-left: max(0px, var(--safe-area-left, env(safe-area-inset-left, 0px)));
  }

  /* iOS 27 Home Screen apps can blur content below the reported top inset.
     Reserve a small, configurable clearance in installed iOS/iPadOS web apps.
     This is a compatibility allowance, not a measured status-bar height: it
     also applies on older iOS releases. Regular tabs, native WKWebView hosts,
     Android and desktop browsers keep the normal zero-clearance default.
     An explicit --safe-area-top remains the complete inset; none bypasses both. */
  @supports (-webkit-touch-callout: none) {
    @media (display-mode: standalone) {
      :host { --_safe-area-top-clearance: 16px; }
    }
  }

  :host([safe-area="none"]) {
    --safe-area-top: 0px;
    --safe-area-right: 0px;
    --safe-area-bottom: 0px;
    --safe-area-left: 0px;
  }
`;
