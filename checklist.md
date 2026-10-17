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
- [x] **S6** layout steps (simplified, NOT a dagre port: longest-path ranks, barycenter order, order-keeping relaxation for x; to be replaced by S6f): `CycleBreaker`, `Ranker`, `DummyInserter`, `CrossingMinimizer`, `CoordinateAssigner`, `EdgeRouter`, `ClusterBoxer`, `LayoutEngine`
- [x] **S7** render (tests: `test_render.cpp`; private classes are covered through the public `render_svg`, tests never include `src/`)
  - [x] S7a `SvgWriter` (escaping, number formatting, balanced tags)
  - [x] S7b `Theme` (default, dark, forest, neutral; unknown name throws)
  - [x] S7c `ShapeRenderer`s (one class per outline kind)
  - [x] S7d `EdgeRenderer` (paths, markers, labels, invisible edges)
  - [x] S7e `FlowchartRenderer` + `render_svg(source)` (diagram detection, styles, clusters, accessibility)
- [ ] **S8** facade `mermaid::render_svg(source)` is done (in S7e); still open: `mermaid_cli` target (modelled on mermaid-js/mermaid-cli, SVG only: `-i` file or `-` for stdin, `-o`, `-t` theme, `-h`; own small argument parser, no CLI library), `examples/main.cpp`
- [~] **S6f** rewrite the layout as the dagre pipeline (reference: `dagre-d3-es@7.0.14`, `layout.js` `runLayout`; one class per stage). Public API: `include/mermaid/dagre.h` (`mermaid::dagre::layout`)
  - [x] S6f-1 graph model (`dagre_graph.h`: graphlib-compatible multigraph with nesting and the same visiting order, incl. JS integer-key ordering) and `makeSpaceForEdgeLabels`
  - [x] S6f-2 `Acyclic` and `NetworkSimplexRanker`
  - [x] S6f-3 `NestingGraph`, `EdgeNormalizer`, `ParentDummyChains`, `BorderSegments`
  - [x] S6f-4 `Orderer` (init order, barycenter sweeps, crossing count, subgraph sorting, conflict resolution)
  - [x] S6f-5 `position` (Brandes-Koepf) and the coordinate system (TB/BT/LR/RL)
  - [x] S6f-6a self-edges, label placement, border nodes, edge points and node intersections (all in `dagre_layout.cpp`)
  - [ ] S6f-6b wire `mermaid::dagre::layout` into `LayoutEngine` (replace `layout_steps`, `layered_graph`) and retire the old classes
  - Verified: `tests/cpp/test_dagre.cpp` has 11 reference cases; a further 300 random graphs (all four directions, compound, labels, cycles, self-loops, multi-edges) were compared against dagre-d3-es and matched to 1e-6. The numbers come from running dagre-d3-es 7.0.14 with a small node script that is kept outside the repo.
- [ ] **S6g** mermaid layout settings: `nodeSpacing` / `rankSpacing` default 50 in mermaid code, `marginx/marginy` 8, subgraph `ranksep + 25`; reconcile with the 40 / 12 seen in merman goldens
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

## Findings from reading mermaid 11.16.0 and dagre-d3-es 7.0.14 (graphify + code)

- Mermaid lays out flowcharts with `dagre-d3-es` (`rendering-util/layout-algorithms/dagre/index.js`). The edge-label "corridor" seen in merman goldens is dagre's `makeSpaceForEdgeLabels`: `ranksep /= 2`, every edge `minlen *= 2`, so a label sits in its own rank.
- Oracle check (done outside the repo with node): feeding the merman golden node sizes of `classdef_and_inline_classes_003` (LR, nodesep 40, ranksep 40) and its two edge-label sizes to dagre gives **exactly** the golden y of all five nodes (107.11, 54.57, 159.65 after the 12 px margin) and the golden x of `in`, `parse`, `validate`. So the 9.6 px y error of the simplified layout is the missing dagre positioning (Brandes-Kopf), not a sizing error.
- Still different: x of `store`/`reject` (golden left-aligns them at 666.74, plain dagre centres them and is 48 px closer): the label rank is wider in the golden. Find how merman/mermaid pads edge-label width before layout.
- The constant 120 is not in mermaid's flowchart code (only `hachureAngle: 120`); it comes from merman or from browser measuring.
- Mermaid code: `nodeSpacing`/`rankSpacing` default 50, `marginx/marginy` 8, `diagramPadding` 8; goldens show 40 and margin 12. Unresolved (S6g).
- The fixture headers say Mermaid@11.12.2 while the reference here is 11.16.0.
