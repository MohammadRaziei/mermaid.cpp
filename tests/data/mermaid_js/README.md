# Reference SVGs (real Mermaid 12.2 browser output)

`<name>.svg` is the SVG Mermaid produced in a browser for `../merman/<name>.mmd`. They were copied
from merman's `fixtures/upstream-svgs/flowchart/` (merman: MIT OR Apache-2.0, see
`../merman/LICENSE-*`). Note: merman's *own* renderer output is different from these files; only
these upstream files are used as the reference.

`parity.tsv` lists each fixture with `pass` (the test requires `svg_compare` to report no
difference) or `todo` (the number of differences is printed, nothing fails).
