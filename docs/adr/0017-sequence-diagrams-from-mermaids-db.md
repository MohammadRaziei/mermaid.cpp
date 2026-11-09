# ADR-0017: Sequence diagrams start from what Mermaid's sequenceDb holds

- Status: Accepted
- Realised in: `include/mermaid/sequence.h`, `sequence_parser.h`, `src/sequence_parser.cpp`, `tools/sequence_db.mjs`,
  `tests/data/sequence/`

## Context

`sequenceDiagram` is the second diagram type. Unlike the flowchart it has no dagre: actors sit in a row and messages are
stacked down, with a layout of its own (`sequenceRenderer.ts`, `svgDraw.js`). Mermaid's parser is a jison grammar with a
case-insensitive lexer (`participant`, `box`, `create`, `autonumber`, `rect`, ten arrow tokens, notes, links...). The
SVG parity method of the flowchart (reference SVGs generated at build time, ADR-0012) applies to the renderer, but the
parser needs a smaller, faster oracle that tells *where* a difference starts.

## Decision

- The model mirrors sequenceDb: a list of actors in order of first appearance, boxes, and one flat list of messages
  whose `type` is Mermaid's `LINETYPE` number (notes, fragments, activations and autonumber are messages too), numbered
  by position. A `-` or `+` in an arrow adds the `ActiveEnd` / `ActiveStart` message after the signal, as Mermaid does.
- The parser is a hand-written, line-oriented reader (statements split at `;`, comments at `#` and `%%`), not a port of
  the jison tables. Keywords are matched case-insensitively. Message text follows `parseMessage`: a leading `wrap:` or
  `nowrap:` is a flag, everything else the text.
- The oracle is `tools/sequence_db.mjs`: real Mermaid 12.2.0 in a browser, `mermaidAPI.getDiagramFromText`, then
  `db.getActors()`, `getMessages()`, `getBoxes()`, the created and destroyed actors, the title. Its JSON is committed next
  to each fixture (`tests/data/sequence/*.mmd|json`): it is semantic data, not a rendering, so the rule against
  committing reference SVGs does not apply. `tests/cpp/test_sequence_parser.cpp` compares field by field.
- `json_reader` left `init_directive.cpp` for `src/json.h` (arrays are kept now) so the tests and later the config of
  other diagrams use the same reader.

## Not yet

`link`/`links`/`properties`/`details` statements, `participant X@{ "type": ... }` configuration, the `/|-`-style
half-arrows, entity codes (`#35;`) in texts, and the sequence-specific init keys. They fail with a `ParseError` or are
read as plain text until a fixture shows Mermaid's behaviour.

## Consequences

Every parser change is checked against Mermaid's own db in milliseconds; a failing fixture names the message index and
field. Rendering (actor boxes, lifelines, message arrows and text, activation bars, notes, fragments, autonumber) is the
next step and has its own fixtures against real SVGs.

## Addendum: text is measured in the browser's default font

Mermaid's `calculateTextDimensions` measures every text twice (`sans-serif` and the configured family) and keeps the
second unless the first is strictly bigger in width, height and line height. In the neo look the sequence font families are
empty, so the second measurement uses the browser default, "Times New Roman" (a serif face), while the text is drawn in the
sans face of the stylesheet. Real Mermaid behaves this way, so the layout must too (found by deriving per-character widths
from reference renderings: `a` = 7.1 px at 16 px is Times, not Arial). The port keeps the design units of both faces
(`svg_text_metrics_data.h`, generated from the fonts by `tools/dump_font_units.py`) and computes what Chromium reports at any font
size: the length is the advances plus the `kern` pairs rounded up to 1/64 px, the box adds the ink of the glyphs rounded outwards
(Skia's bounds), the height is the rounded hhea ascent and descent widened by the ink. This was fitted and then checked against
getComputedTextLength() and getBBox() for 5280 strings (sizes 6 to 48, both faces) without one difference; it replaced a 16 px table
that could not be scaled. The port decides like Mermaid does;
`tests/fonts` pins "Times New Roman" to Liberation Serif so the references do not depend on the machine's fonts.

