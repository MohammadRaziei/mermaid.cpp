The reference SVGs of the parity tests are not stored here any more: real Mermaid 12.2.0 (npm `mermaid@12.2.0`) renders
them at build time in a headless Chromium (`puppeteer-core` + `@sparticuz/chromium`) with config
`{layout: "dagre", handDrawnSeed: 1}`, input `../merman/<name>.mmd`, svg id = `<name>`, and the fonts of `tests/fonts`
(see `tests/CMakeLists.txt`, `tools/render_all_references.mjs`). Only `parity.tsv` (which fixtures must match, which are
still `todo`) lives here.
