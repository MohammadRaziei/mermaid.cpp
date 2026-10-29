# Architecture decision records

Short records of decisions that shape the project: what was decided, why, what it costs. Newest knowledge wins:
a decision that changes gets a new ADR that supersedes the old one (the old file stays, its status changes).
Copy `0000-template.md` for a new record and add it to the table.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-full-cpp-port-no-rust-no-js.md) | Full C++ port; no Rust or JavaScript at build or run time | Accepted |
| [0002](0002-reference-is-mermaid-12-2-0-dagre.md) | Reference is real Mermaid 12.2.0 with `layout: dagre` (not merman, not ELK) | Accepted |
| [0003](0003-port-dagre-d3-es-faithfully.md) | Port dagre-d3-es stage by stage, one class per stage | Accepted |
| [0004](0004-browser-text-metrics-table.md) | Text measured with a generated table of browser metrics | Accepted |
| [0005](0005-public-and-private-headers.md) | `include/mermaid/` public, `src/*.h` private, tests use the public API | Accepted |
| [0006](0006-no-runtime-dependencies.md) | No runtime dependencies; vendoring needs the owner's approval | Accepted |
| [0007](0007-naming-and-portability.md) | `mermaid` / `MERMAID_*`, snake_case, `MERMAID_MIN/MAX`, Windows-safe names | Accepted |
| [0008](0008-svg-parity-testing.md) | Parity tests: XML-tree comparison against real Mermaid SVGs | Accepted |
| [0009](0009-port-the-stylesheet-not-copy-it.md) | Stylesheet built from ported theme variables and a CSS compiler | Accepted (default theme only) |
| [0010](0010-closed-subgraphs-are-nested-roots.md) | A subgraph with no edge leaving it is laid out as its own graph | Accepted |
| [0011](0011-own-json-reader-for-init-directives.md) | Own small JSON reader for `%%{init}%%` | Accepted |
| [0012](0012-rough-js-seeded-port-for-hand-drawn-shapes.md) | rough.js path port with the seeded random stream | Accepted |
| [0013](0013-cli-svg-only-own-argument-parser.md) | `mermaid_cli`: SVG only, own parser, logic in a public header | Accepted |
| [0014](0014-shape-geometry-derived-from-node-size.md) | One module derives each shape's outline from the node size | Accepted |
| [0015](0015-self-loops-and-dagre-coordinates.md) | Self-loops via helper nodes; dagre coordinates kept; viewBox from drawn bounds | Accepted |
| [0016](0016-themes-computed-like-mermaid.md) | Themes computed like Mermaid, khroma's colour arithmetic ported | Accepted |
