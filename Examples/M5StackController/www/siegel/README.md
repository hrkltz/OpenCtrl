# OpenCtrl / Examples / M5StackController / www / siegel

A vendored subset of [OpenHanse Siegel](https://github.com/OpenHanse/Siegel), the design system the settings page is built with. Do not edit these files here; change Siegel and copy them again.

- Source: `src/` of Siegel at commit `26c57431664787ff8c0a2bff8310604b5a81c661`, paths unchanged so the relative imports keep working.
- Contents: `tokens/`, and the import closure of `siegel-panel`, `siegel-select`, `siegel-switch`, `siegel-chip`, `siegel-toast`, and `utilities/styles.js`.
- The page maps `@siegel/` to `/siegel/` in its import map.

To use another component, copy it and every module it imports (recursively) from Siegel's `src/`. The build embeds this folder gzipped; Markdown files are not embedded.
