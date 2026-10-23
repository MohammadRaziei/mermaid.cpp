# ADR-0004: Measure text with a generated table of browser metrics

- Status: Accepted
- Realised in: 73b3599 (2026-10-20)

## Context

Every node and label size, and therefore every layout number, comes from text width. Mermaid measures in a
browser: with kerning "Yes" is 22.84375 px at 14 px; the first approximation (Arial advances, no kerning) gave
24.12. No font may be vendored and no dependency may be added for text.

## Decision

`browser_text_measurer` implements the `TextMeasurer` interface from a table generated from Chromium by
`tools/dump_font_metrics.mjs` into `src/browser_font_metrics.cpp`: glyph advances plus 5112 kerning pairs at 14 px,
scaled linearly for other sizes, with HTML whitespace collapsing. The line height is 1.5 em (21 px). The old
`ApproxTextMeasurer` was removed. The `TextMeasurer` seam stays, so another measurer can be plugged in.

## Consequences

- Deterministic, dependency-free, and equal to Chromium for the characters and pairs in the table.
- Chromium rounds layout units to 1/64 px, so a width can differ by that much (seen: 50.578125 against 50.59375);
  the parity tolerance of 0.5 absorbs it (ADR-0008).
- Characters or pairs outside the table fall back to defaults; the table's coverage is not audited.
- Labels wider than the label box break at spaces like the browser does: `TextMeasurer::measure_wrapped` wraps
  greedily at 120 px for nodes and 200 px for edge labels (line counts checked against Chromium in the reference SVGs);
  a word wider than the box is not split, which is not verified against the browser.
- `themeVariables.fontFamily` from `%%{init}%%` changes the CSS only; it does **not** change the measurement.

## Alternatives considered

- Approximate Arial metrics: wrong by more than a pixel on short labels.
- Load a system font at run time: not reproducible across machines and needs a font library.
