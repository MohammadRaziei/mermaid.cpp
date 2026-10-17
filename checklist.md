# Checklist

Workflow per step: failing test first -> implement -> green -> commit -> zip.

## Steps

- [x] **S0** scaffold: git (`master`), layout mirroring liburlparser, CMake, vendored `utest.h`, version API (TDD)
- [x] **S1** geometry value types (`Point`, `Size`, `Rect`) + `TextMeasurer` interface + `ApproxTextMeasurer` (Arial metrics, 14px, 21px line)
- [ ] **S1b** (on hold) accurate `TextMeasurer` using the standard approach for fonts; no specific font is vendored and no extra dependency may be added without the owner's approval. Approach to be agreed first
- [x] **S2** flowchart `Model` + `StyleParser`
- [x] **S3** shape and edge scanning (folded into the parser)
- [x] **S4** flowchart `Parser` (statements, chains, `&`, subgraphs, class/style/linkStyle), merman semantic goldens
- [x] **S5** `NodeSizer` (rect and diamond from goldens; other shapes approximate)
- [x] **S6** layout steps (simplified, not a dagre port: longest-path ranks, barycenter order, order-keeping relaxation for x): `CycleBreaker`, `Ranker`, `DummyInserter`, `CrossingMinimizer`, `CoordinateAssigner`, `EdgeRouter`, `ClusterBoxer`, `LayoutEngine`
- [x] **S7** render (tests: `test_render.cpp`; private classes are covered through the public `render_svg`, tests never include `src/`)
  - [x] S7a `SvgWriter` (escaping, number formatting, balanced tags)
  - [x] S7b `Theme` (default, dark, forest, neutral; unknown name throws)
  - [x] S7c `ShapeRenderer`s (one class per outline kind)
  - [x] S7d `EdgeRenderer` (paths, markers, labels, invisible edges)
  - [x] S7e `FlowchartRenderer` + `render_svg(source)` (diagram detection, styles, clusters, accessibility)
- [ ] **S8** facade `mermaid::render_svg(source)` is done (in S7e); still open: `mermaid_cli` target (modelled on mermaid-js/mermaid-cli, SVG only: `-i` file or `-` for stdin, `-o`, `-t` theme, `-h`; own small argument parser, no CLI library), `examples/main.cpp`
- [ ] **S6b** clusters stay in their own x-range across layers (today a node outside a subgraph can end up inside its box; see `stress_flowchart_deeply_nested_clusters_019`)
- [ ] **S6d** `%%{init: ...}%%` directives: flowchart `nodeSpacing` / `rankSpacing` and `themeVariables` (needs a small JSON reader of our own, tests/fixtures 010, 032, 064)
- [ ] **S6e** per-subgraph `direction` (fixture 019)
- [ ] **S6c** calmer edges in dense graphs (shared horizontal segments overlap, no curves, back edges cross a lot; see `stress_flowchart_dense_parallel_edges_002`)
- [ ] **S9** golden layout parity with merman (`*.layout.golden.json`): `basic` and `class_style` match exactly; the other fixtures only have structural checks so far
- [ ] **S10** CI, docs (Doxygen like liburlparser), benchmarks, coverage dashboard

## Later / optional

- [ ] Python bindings (nanobind, as in liburlparser)
- [ ] Other diagram types: sequence, class, state, ER, gantt, pie, ...
- [ ] Edges to subgraphs, `click`, `@{ shape: ... }`, Markdown strings

## Open questions (found while reading merman fixtures)

- Upstream SVGs (merman `fixtures/upstream-svgs/flowchart`) use font-size **14px**,
  label height **21px** (1.5 em) and Arial metrics (CSS lists `"Recursive Variable", arial`;
  the font was not installed). Edge label "No" measured 17.90625 px (= Arial 14px);
  "Yes" measured 22.84 px, i.e. Arial **with kerning** (24.1 px without).
  -> an accurate measurer must apply kerning; `ApproxTextMeasurer` cannot.
- Node label `foreignObject` widths in the same SVGs are 120 (fixed?) - find out why.

- Solved in part: the merman layout goldens use the **neo look** sizing in
  `merman-render/src/flowchart/node.rs`: rect = (text_w + 32, text_h + 24), diamond side =
  text_w + text_h + 2*padding (padding 15). With text_h = 21 both goldens (152 x 45 and
  171 x 171) fit **only if text_w = 120 for every label** ("Start", "OK", "Fail", "Choice").
  So labels in these goldens measure as a fixed 120 px wide. Not yet found where the 120
  comes from (merman has `max_width_px: Some(120.0)` in `svg_label_artifact.rs`); find out
  before S5/S9. Rank gap = 40. Tests must assert golden numbers, not guessed constants.

## Parity with merman layout goldens (node size / node position, within 1 px)

| fixture | nodes | size ok | position ok |
|---|---|---|---|
| basic | 4 | 4 | 4 |
| class_style | 2 | 2 | 2 |
| amp_groups_edge_ids_034 | 5 | 5 | 5 |
| classdef_and_inline_classes_003 | 5 | 4 | 0 (x matches, y is 9.6 px off) |
| comments_and_directives_010 | 5 | 5 | 0 (init directive, cluster) |
| deeply_nested_clusters_019 | 10 | 10 | 0 (subgraph directions, clusters) |
| dense_parallel_edges_002 | 9 | 8 | 0 |
| directives_comments_blank_lines_032 | 4 | 4 | 0 (init spacing) |
| edge_label_position_064 | 3 | 3 | 0 (init spacing) |
| escape_sequences_and_quotes_012 | 5 | 3 | 4 |
