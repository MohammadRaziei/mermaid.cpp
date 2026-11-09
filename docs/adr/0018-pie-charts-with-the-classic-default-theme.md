# ADR-0018: Pie charts: the classic default theme, d3's arcs, widths from the font model

- Status: Accepted
- Realised in: `include/mermaid/pie.h`, `pie_parser.h`, `pie_renderer.h`, `src/pie_parser.cpp`, `src/pie_renderer.cpp`,
  `src/d3_shape.{h,cpp}`, `tests/data/pie/`

## Context

`pie` is the third diagram type and the first with no layout engine: a circle, slices, a legend. Mermaid draws it with
d3-shape (`pie()`, `arc()`), measures the legend and the title with `getBoundingClientRect()` and sets the viewBox from
those widths. Two things are not obvious from the flowchart and sequence work: the colours, and which numbers a browser
prints.

## Decision

- **Theme.** Mermaid draws the diagrams that have no redux look with the classic `default` theme (theme-default.js:
  `pie1` is `#ECECFF`, the node border `#9370DB`), not with the redux-color theme the flowchart and sequence diagrams get
  from `default`. `theme_variables::classic_default()` holds that theme and derives `pie1 .. pie12` and the other pie
  variables with the same khroma `adjust` calls. A pie chart with another theme, `themeVariables` or `fontFamily` throws
  `UnsupportedDiagram` (those need the pie derivations of the other theme classes and the full `calculate()`).
- **Shapes.** `d3_shape` ports `d3.pie().sort(null)` and `d3.arc()` (no padding, no corner radius) with d3-path's output:
  numbers go through `Math.round(x * 1000) / 1000`, a whole circle is two half arcs, a donut slice has an inner arc. The
  slice labels use `arc.centroid`, which is not rounded, so numbers are written the way JavaScript prints a double
  (`js_number`, shortest round-trip).
- **Text widths.** The legend and title widths are the bbox widths of the sans face at 17px and 25px, from the font
  model of ADR-0017 (`svg_text_metrics::sans_bbox_width`); they decide `viewBox`, `max-width` and the legend offsets.
- **Percent labels.** `toFixed(0)` rounds a tie up, which `printf` does not; `to_fixed_0` follows JavaScript. A slice
  under 1% of the sum is not drawn, and one whose label rounds to `0%` loses its label (as in `createPieArcs`).
- **Colours in `style`.** The legend `rect` sets `fill` and `stroke` through `style`, which a browser reports as
  `rgb(r, g, b)`. The conversion from `hsl()` is the single-precision algorithm of CSS Color 4 (`143.5` stays `143.5` and
  rounds to 144). The same fixture showed `fadedEdgeLabelBackground` has to keep khroma's unrounded channels
  (`rgba(8.4999999999, ...)`), so `fade` moved into the khroma port and `fade_color` was removed.
- **Parser.** Hand-written, line by line on the shared `split_statements`: `pie [showData] [title ...]`, `title`,
  `accTitle`, `accDescr` (also the block form), `"label" : value` with the grammar's unsigned decimal. A repeated label
  keeps its first value, as `addSection` does. A frontmatter `title:` is the title unless the text sets one.

## Consequences

- 20 fixtures (`tests/data/pie/`, legend positions, donut, highlight, many slices, tiny slices, accessibility,
  frontmatter, fixed size) match Mermaid 12.2.0 with zero differences; the references are generated at build time like the others.
- Not drawn: other themes and `themeVariables` (the pie colours of dark, forest, neutral and base), `fontFamily`, a chart
  without sections (Mermaid's `Math.max()` of nothing gives an invalid viewBox).
