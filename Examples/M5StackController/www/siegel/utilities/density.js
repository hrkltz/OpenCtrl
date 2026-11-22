// Shared control geometry, one preset per input device. tokens.css picks one
// automatically from `pointer: coarse`; `density="mouse|touch"` forces one on a subtree.
export const DENSITIES = {
  "mouse": {
    "ctl-h": 32,
    "ctl-font-size": 13,
    "button-font-size": 11,
    "icon-size": 16,
    "ctl-pad-x": 8,
    "ctl-pad-y": 3,
    "ctl-gap": 6,
    "panel-pad": 16,
    "panel-gap": 12,
    "frame-pad": 6,
    "app-edge": 44,
    "ctl-radius": 4,
    "icon-btn-size": 24,
    "switch-h": 20,
    "bar-h": 44,
    "row-h": 32,
    "select-rail-pad": 3,
    "rail-icon-size": 18
  },
  "touch": {
    "ctl-h": 44,
    "ctl-font-size": 16,
    "button-font-size": 16,
    "icon-size": 20,
    "ctl-pad-x": 12,
    "ctl-pad-y": 6,
    "ctl-gap": 8,
    "panel-pad": 16,
    "panel-gap": 12,
    "frame-pad": 6,
    "app-edge": 56,
    "ctl-radius": 6,
    "icon-btn-size": 40,
    "switch-h": 28,
    "bar-h": 56,
    "row-h": 44,
    "select-rail-pad": 4,
    "rail-icon-size": 24
  }
};

export function densityStyles() {
  return Object.entries(DENSITIES).map(([name, tokens]) => {
    const declarations = Object.entries(tokens).map(([key, value]) => `--${key}: ${value}px;`).join(" ");
    return `[density="${name}"], :host([density="${name}"]) { ${declarations} }`;
  }).join("\n");
}
