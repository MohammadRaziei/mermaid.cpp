# ADR-0009: Build the stylesheet from ported theme variables and a CSS compiler

- Status: Accepted (all five themes since S11)
- Realised in: 060f710, 7963ac1 (2026-10-19)

## Context

Mermaid 12.2's output carries a ~22 KB `<style>` generated from theme variables, per-diagram rules, user `classDef`s
and `%%{init}%%` overrides, then compiled by stylis (namespace prefix, nesting, `&`, minification).

## Decision

Do not paste the CSS text. `theme_variables` holds the values (dumped from Mermaid by
`tools/dump_theme_variables.mjs`), `flowchart_css` builds the rules as Mermaid's `getStyles` does, and the public
`css_compiler` (stylis-like) and `css_color` (hex/rgb/hsl, `fade`, the browser's `rgb()` form) produce the same
text. `classDef`, `linkStyle` and init theme variables flow through the same path.

## Consequences

- Overrides (`fontFamily`, `clusterBkg`, `clusterBorder`, a user CSS file) work without special cases.
- The compiled text must match stylis formatting (for example `fill:red;`).
- Mermaid inserts the `classDef` rules into a browser `CSSStyleSheet`, so their values come back as Chromium
  serialises them; `browser_value` reproduces what was seen: hex colours as `rgb()`, `stroke-dasharray` as a comma list
  (`5 5` -> `5,5`), multi-word font names quoted. Other properties are written as given, which is unverified.
- Since S11 every theme (`default`, `dark`, `forest`, `neutral`, `base`) uses this path. The variables were stored
  tables at first; ADR-0016 replaced them by Mermaid's own derivations (khroma ported), so `themeVariables` overrides work.

## Alternatives considered

- Embed the reference CSS as a string: not reusable for other themes or overrides, and a copy of generated output.
