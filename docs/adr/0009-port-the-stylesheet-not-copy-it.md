# ADR-0009: Build the stylesheet from ported theme variables and a CSS compiler

- Status: Accepted (limited to the default theme, see Consequences)
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
- **Only the `default` theme uses this path.** `dark`, `forest` and `neutral` still go through the older, simplified
  classic stylesheet and are not checked against Mermaid; `mermaid_cli -t` exposes them anyway (`checklist.md`, S11).

## Alternatives considered

- Embed the reference CSS as a string: not reusable for other themes or overrides, and a copy of generated output.
