# ADR-0008: Test against real Mermaid SVGs with an XML-tree comparison

- Status: Accepted
- Realised in: 0cd9060 (2026-10-18), 3a7c34e (2026-10-19)

## Context

"Looks the same" is not a test. The target is Mermaid 12.2.0 output (ADR-0002), whose SVG contains tens of
elements per node, generated ids, a 22 KB stylesheet and base64 edge data.

## Decision

`tests/cpp/svg_compare.h` (pugixml, tests only) compares two SVGs as trees: same elements, attribute names and
text; numbers inside any attribute may differ by 0.5; `data-points` is base64-decoded and compared as numbers.
`tests/data/mermaid_js_dagre/parity.tsv` marks every fixture `pass` (must have zero differences) or `todo` (the test
prints the count so progress is visible); a fixture is promoted only when it reaches zero. Sources are
`tests/data/merman/*.mmd`; references come from real Mermaid in headless Chromium (npm `@sparticuz/chromium`,
`puppeteer-core`, `mermaid@12.2.0`) through `tools/render_reference.mjs` and `tools/render_all_references.mjs`, with
the same post-processing as mermaid-cli. A separate test checks the frame of the document with `<style>` and
`g.root` content pruned.

## Consequences

- A strong regression net: all 19 fixtures pass.
- Coverage is only those 19 fixtures; unchecked areas are listed in `checklist.md`.
- A tolerance of 0.5 can hide smaller errors; the text-measurement rounding is the known cause of such gaps.
- `mermaid.render()` returns an HTML string (`<br>`); mermaid-cli serialises the DOM, so the scripts turn `<br>` into
  `<br />` to get well-formed XML like the real tool.
- Regenerating references needs Node and network, so it is not part of the normal build.

## Alternatives considered

- Byte-for-byte SVG equality: breaks on harmless float noise.
- Rasterised image comparison: hides structural differences. PNGs are produced for human review only.
