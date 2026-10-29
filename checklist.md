# Checklist

Workflow per step: failing test first -> implement -> green -> commit -> clean rebuild and all tests -> zip
(see `agent.md`). Decisions behind the steps are in `docs/adr/`. Legend: `[x]` done, `[~]` partly done, `[ ]` open.

## Status

- 199 tests pass (Linux/GCC; CI also builds macOS and Windows).
- Parity with real Mermaid 12.2.0 (`layout: dagre`, neo look): **19 of 19 fixtures at zero differences**
  (`tests/data/mermaid_js_dagre/parity.tsv`). That is the whole parity net; everything outside those fixtures is
  listed under "Unverified".
- Supported: flowcharts (`graph` / `flowchart`); every other diagram type throws `UnsupportedDiagram`. All 14 shapes of the classic syntax are drawn in the neo markup.
- `mermaid_cli` renders SVG from files or stdin.

## Done

- [x] **S0** scaffold: git (`master`), layout mirroring liburlparser, CMake, vendored `utest.h`, version API
- [x] **S1** geometry value types, `TextMeasurer` interface
- [x] **S1c** `browser_text_measurer`: table generated from Chromium by `tools/dump_font_metrics.mjs` (advances,
  5112 kerning pairs, 14 px, linear scaling), HTML whitespace collapsing; the Arial approximation was removed
  (ADR-0004)
- [x] **S2** flowchart `Model` + style declaration parsing (`style_css`)
- [x] **S3** shape and edge scanning (folded into the parser)
- [x] **S4** flowchart `Parser` (statements, chains, `&`, subgraphs, class/style/linkStyle), merman semantic goldens
- [x] **S5** `NodeSizer` (formulas of `shapes/*.ts`, see S12)
- [x] **S6** first simplified layout; retired in S6f-6b
- [x] **S6f** layout as the dagre pipeline (`dagre-d3-es` 7.0.14, one class per stage; ADR-0003)
  - [x] S6f-1 graph model (graphlib-compatible multigraph, same visiting order incl. JS integer-key ordering),
    `makeSpaceForEdgeLabels`
  - [x] S6f-2 `Acyclic`, `NetworkSimplexRanker`
  - [x] S6f-3 `NestingGraph`, `EdgeNormalizer`, `ParentDummyChains`, `BorderSegments`
  - [x] S6f-4 `Orderer`
  - [x] S6f-5 `position` (Brandes-Koepf) and the TB/BT/LR/RL coordinate system
  - [x] S6f-6a self-edges, label placement, border nodes, edge points, node intersections
  - [x] S6f-6b `LayoutEngine` runs `mermaid::dagre::layout`
  - Verified: 11 reference cases in `test_dagre.cpp`, plus 300 random graphs (all four directions, compound, labels,
    cycles, self-loops, multi-edges) compared with `dagre-d3-es` to 1e-6 (Node script kept outside the repo)
- [x] **S7** render
  - [x] S7a `SvgWriter`; S7b `Theme`; S7c node outlines (the first `ShapeRenderer` classes were replaced in S12); S7d `EdgeRenderer`; S7e `FlowchartRenderer` +
    `render_svg(source)` (diagram detection, styles, clusters, accessibility)
  - [x] S7f edges in the neo markup of Mermaid 12.2 (`edge_geometry`: clip, `fixCorners`, arrow offsets, `curveBasis`;
    ids, classes, `data-*`, `-margin` markers, coloured marker copies, HTML edge labels); `linkStyle` follows
    `flowDb.updateLink`
  - [x] S7g parity moved to real Mermaid 12.2.0 with `layout: dagre` (ADR-0002, ADR-0008). Numbers: node/rank
    spacing 50, margin 8, no extra label pad, label position centre, `viewBox` = graph size from 0,0, edge routes
    keep the point on the empty label rank, unlabelled edges get an empty `edgeLabel`
  - [x] S7h per-edge `curve` (linear/basis/cardinal; other names draw basis), cylinder in neo markup, duplicate
    classes kept, repeated style properties collapse
  - [x] S7i stadium as a rough.js path with the seeded generator (`rough_path.cpp`, `RenderOptions::hand_drawn_seed`,
    default 1; ADR-0012)
  - [x] S7j clusters in neo markup: box = dagre compound node, svg-prefixed ids, `data-look`, `data-color-id`
    (declaration order, depth first, modulo palette size), `foreignObject` label, box widened to label + padding
  - [x] S7k `%%{init}%%` (`init` / `initialize`, multi-line, later wins, invalid JSON ignored; own JSON reader;
    ADR-0011): every `themeVariables` entry (ADR-0016), `fontFamily`, `flowchart.nodeSpacing|rankSpacing`
  - [x] S7l `A@{ label: "..." }`: `markdown-node-label` class (labelType defaults to markdown unless `text` /
    `string`), each `@{}` block advances the vertex counter once more (ids jump by 2)
- [x] **S6b** clusters stay in their own x-range: the dagre compound boxes hold exactly their members. Closed by
  tests (two constructed graphs, TB and LR, plus fixtures 010, 019, 032) after a 74-check scan found no violation
- [x] **S6c** calmer edges in dense graphs: solved by the dagre port; fixture 002 passes
- [x] **S6d** init directives (done in S7k)
- [x] **S6e** per-subgraph `direction` and closed subgraphs as nested roots (ADR-0010): own `direction` or perpendicular
  to the parent, `ranksep + 25`, parent `nodesep`, drawn as nested `g.root`; edges between closed clusters run box to
  box. 032 passes
- [x] **S8** `mermaid::render_svg(source)` and the `mermaid_cli` target (ADR-0013), `examples/main.cpp`
- [x] **S12** the remaining classic shapes in neo markup: round, subroutine, circle, double circle, hexagon, odd (rough.js),
  lean right/left, trapezoid, inverted trapezoid. Sizes from `shapes/*.ts`, outlines derived from the node size in
  `shape_geometry`, edges clipped to polygon outlines (`intersect-polygon.js`), circles sized to the text; fixture
  `shapes_all` (14 nodes) passes with zero differences; the old `ShapeRenderer` classes were deleted (ADR-0014)
- [x] (superseded) **S6g** layout spacing "40 / 12 / 20": those were ELK artefacts (ADR-0002)
- [x] (superseded) **S9** golden layout parity with merman: merman is not the criterion any more; its goldens remain
  as parser and sizing checks

## Open

Ordered by what is likely to matter next.

- [x] **S11** themes `dark`, `forest`, `neutral`, `base` in the neo look: variables come from Mermaid's own theme code
  (computed with a port of khroma, ADR-0016), the stylesheet is the same
  ported path, the classic themes have 16px text (24px lines), no colour slots, a gradient (`insertLookDefs`) and a white
  drop-shadow flood for dark. 10 fixtures (basic + rich, five themes) pass. The init `theme` key and `-t` give the same
  picture. `themeVariables` overrides derive other colours like Mermaid (`calculate` + `updateColors`, khroma ported;
  19 fixtures `t_*`, incl. the sanitiser: unknown keys such as `contrast` are dropped, odd values emptied)
- [x] **S12b** the `@{ shape: ... }` family (150 names, 53 handlers in Mermaid 12.2.0). All 53 handlers are
  drawn, verified against the real SVG (46 fixtures `s12b_*`, all `pass`, each with a short label and a label that
  wraps where the shape has one): the 14 classic shapes, the paper shapes (`doc`, `docs`, `lin-doc`, `tag-doc`, `flag`),
  `delay`, `notch-pent`, `sl-rect`, `notch-rect`, `lin-rect`, `div-rect`, `curv-trap`, `h-cyl`, `lin-cyl`, `st-rect`,
  `tag-rect`, `bow-rect`, `win-pane`, `bolt`, `cloud`, `bang`, the braces (`brace`, `brace-r`, `braces`), `hourglass`,
  `tri`, `flip-tri`, `text`, `fork`, `folder` (a plain six-point `<path>`, `polygon_outline::plain_path`), `bucket` (a group with a path and the rim
  ellipse, drawn off the centre of its node box: `drawing_shift` moves the bounds, edges clip to 26 sampled points), `console` and `browser` (a rounded rect plus decoration in the accent colour,
  edges clip as for a rect), `person` (a body rect and a head circle, 24 + 4 x 12 outline points), `datastore` (a rect with
  `stroke-dasharray="w h"`), `sm-circ` and the circles `f-circ`, `fr-circ`, `cross-circ` (rc.circle of rough.js with
  roughness 0 is ported: `rough_circle`). How they are built: several rough.js drawings
  (`polygon_outline::layers`: nested `<g>`, `rc.polygon` plain fill with `fill-rule="evenodd"`, `mergePaths`, stroke-only,
  open and multi-run paths); sizes and outlines derive from the label box, not from the size, so no rounding is lost;
  `lin-rect` and `div-rect` clip edges to the node box (`intersect.rect`), the others to their outline; `cloud` and
  `bang` are the exact bounding box of their arcs and the viewBox follows the drawn path; the node of the braces is the
  union of all drawings, and the viewBox follows where the invisible box really is (it bulges 1.9 left of a `brace` node;
  fixtures `s12b_brace_styled`, `s12b_brace_ends`, `s12b_brace_ends_rl` have a brace at each end of the graph).
  Audited once against Mermaid (not kept as fixtures, 371 throwaway files): each of the 53 shapes first and last in LR and TB (212
  files, all identical), each with a `style` statement and with a `classDef` (106 files), and each with a long wrapping label in LR (53 files, all identical). The audit found: the state circles
  (`sm-circ`, `fr-circ`) ignore the node's classes; `f-circ`, `fr-circ`, `cross-circ` take fill, stroke, width and dash from
  the style map, `fork` only width and dash (its colours are `lineColor`); the empty label of `hourglass` carries the label
  colour; the group of `lin-rect` carries the raw `style` text. The 14 shapes that differed are fixtures `s12b_style_*` and
  `s12b_class_*`. An unknown name is an error, as in
  Mermaid: `No such shape: <name>.` (the lookup is exact, `Rect` fails). Every alias of the 53 groups maps to the same
  shape as its short name (unit test `shape_aliases_are_the_same_shape_as_their_short_name`; 20 aliases such as
  `lean-right`, `manual`, `priority`, `double-circle`, `stateStart` had been boxes before). The fork size follows the direction of
  the nearest subgraph that declares one (`resolveNodeDir`; fixture `s12b_fork_in_lr_subgraph`).
- [x] **S13** edges to and from subgraph ids. An end at a subgraph id is drawn against the cluster box and the route
  is cut at its border (`toCluster` / `fromCluster`, `cutPathAtIntersect`; `data-points` keeps the route before the cut);
  loops on a closed subgraph (helpers in the parent graph) and on an open cluster (drawn around the compound box); both
  directions, subgraph to subgraph, nested, labelled, LR. A node and its own cluster (`a --> g`, `g --> b` with both
  inside `g`): the anchor is `findNonClusterChild` (first leaf with no edge in common with the cluster), a self-loop is
  decided on the edge as written (so `g --> b` becomes a plain `b --> b` that dagre loops itself), a one-point route is
  `M x,yZ` with the arrow offset added to x. The safe anchor (`findSafeAnchorNode`) and the graphlib order of edges (an
  edge at a cluster moves behind the rest) are ported. 11 fixtures pass (`s14_mixed_clusters`, `s14_subgraph_loop`,
  `s13_*`, incl. `s13_node_into_own_cluster`, `s13_own_cluster_lr`, `s13_own_cluster_nested`, `s13_safe_anchor`,
  `s13_edge_order`). Not done: `click`/links.
- [x] **S13b** Markdown labels: a "`...`" string (it may span lines) is a Markdown label on nodes, edges and subgraph titles:
  bold, italic, both (`**` `__` `*` `_` `***`) and line breaks, with `<strong>` / `<em>` markup (`markdown_label`), measured
  run by run with metric tables of the four faces (`dump_font_metrics.mjs`, Liberation Sans of `tests/fonts`). The
  wrapped table is as wide as the longest word and the blank after it (`break-spaces`), for plain labels too. 3 fixtures
  pass. Not done: other Markdown (lists, links, code, headings), escapes with a backslash, `htmlLabels: false` text.
  A literal `\n` and `<br>` are line breaks in plain labels of nodes, edges and subgraph titles (`decode_label`, one
  `SvgWriter::paragraph` writes them), not in a Markdown string (fixture `s12b_label_newline`).
- [~] **S14** more fixtures. Done (26 of 26 of the original list): BT, RL, self-loops, edge styles incl. invisible, styled
  shapes, long labels, every curve (`flowchart.curve` init key; one fixture per curve plus a per-edge one), `linkStyle`
  colour on edge labels, classes on subgraphs (`setClass` ignores unknown ids and does not create nodes), several loops on
  one node (they share helper ids and edge keys, so only the last is drawn), a word wider than the label box (the
  wrapped table grows to the longest word), mixed open and closed clusters. Not yet covered: `style` on a subgraph id,
  `class` on an edge id, `linkStyle` on more shapes than the one fixture.
- [ ] **S10** CI is a 3-OS build and test (`.github/workflows/cmake.yml`). Open: Doxygen docs (like liburlparser),
  benchmarks, coverage dashboard (`mermaid_coverage` target exists, needs `lcov`)
- [ ] Init keys not read yet: `look`, `layout`, `htmlLabels` (`theme`, `themeVariables`, `fontFamily`, `flowchart.curve|nodeSpacing|rankSpacing` are read)
- [ ] `animate`
- [ ] Markdown inside labels (`**bold**`, `*italic*`, line breaks); only plain text matches
- [ ] `click`, links, tooltips; icons/images; accessibility tags beyond the basic ones
- [ ] Text: table coverage is not audited; `fontFamily` does not change measurement; a different font needs a new table
- [ ] ELK: the 12.2.0 default layout is ELK; matching plain `mmdc` output needs an ELK port. No decision taken
- [ ] Rename the older CamelCase types to `snake_case` (one dedicated step; ADR-0007)
- [ ] MSVC and macOS are verified by CI only; the one `-Wformat` warning in `tests/cpp/test_css_color.cpp`
- [ ] Python bindings (nanobind, as in liburlparser)
- [ ] Other diagram types: sequence, class, state, ER, gantt, pie, ... (needs an ADR first)

## Parity fixtures (all `pass`)

| fixture | exercises |
|---|---|
| basic | boxes, diamond, labelled edges, TD layout |
| class_style | `classDef`, `class`, `style` |
| stress_flowchart_amp_groups_edge_ids_034 | `&` groups, edge ids, `linkStyle` colours |
| stress_flowchart_classdef_and_inline_classes_003 | inline classes, cylinder, LR with labels |
| stress_flowchart_dense_parallel_edges_002 | dense and parallel edges, stadium (rough.js) |
| stress_flowchart_escape_sequences_and_quotes_012 | escapes and quotes in labels, stadium |
| stress_flowchart_comments_and_directives_010 | cluster with leaving edges, `%%{init}%%` theme variables |
| stress_flowchart_deeply_nested_clusters_019 | nested clusters, per-subgraph direction |
| stress_flowchart_directives_comments_blank_lines_032 | closed clusters as nested roots, edge between clusters, init spacing |
| stress_flowchart_edge_label_position_064 | `@{ label }` nodes, edge label positions, init spacing |
| shapes_all | all 14 classic shapes in one LR chain, edges into polygons and circles |
| s14_direction_bt, s14_direction_rl | BT and RL, edge labels on clipped paths |
| s14_styled_shapes | `classDef` / `style` on round, circle, hexagon, odd, lean, subroutine, double circle; dash lists |
| s14_long_labels | node labels that wrap at 120, `<br/>`, circle sized to wrapped text |
| s14_long_edge_label | edge label that wraps at 200 |
| s14_selfloop_multiedge, s14_selfloop_lr | self-loops (labelled or not), parallel edges `L_a_b_2`, TD and LR |
| s14_edge_styles | dotted, thick, invisible, `--o`, `--x`, `<-->`, long edges, labelled dotted/thick |

## Unverified (implemented from the Mermaid source, no reference SVG covers it)
- `h-cyl` and `lin-cyl` clip edges to the node box, where Mermaid has an `intersect` on the ellipse: fan-in and fan-out fixtures in
  TB and LR (`s12b_hcyl_edges`, `s12b_hcyl_edges_lr`, `s12b_lincyl_edges`) agree, so no adjustment was needed there. The classic
  cylinder does need it and has it (`border_point`, cylinder.ts `intersect`; fixtures `s12b_cylinder_edges` and `_tb`).

- `folder`, `bucket`, `console`, `browser`, `person`, `datastore`: user `style fill` / `stroke` and the dark, forest and neutral themes (the bucket rim, the console glyph and the browser bar take
  `nodeBorder`, falling back to `lineColor`; only the default theme was compared)
- `linkStyle` label colours, dotted dash pattern, thick class, every curve other than the default
- the "safe anchor" and `fromCluster` / `toCluster` paths of cluster edges (not implemented)
- `-t dark|forest|neutral` output
- a word wider than the label box (the browser does not split it; we do not either), several self-loops on one node, self-loops
  on subgraph ids
- `mermaid_cli` on Windows; temporary-file handling in `test_cli.cpp` on Windows
- the stadium polygon bounding box: Mermaid's node is 0.02 narrower than the formula (within tolerance)

## Findings worth keeping

- **Reference.** Mermaid 12.2.0 defaults to **ELK**, not dagre (`layout` in `config.schema.yaml`); merman's
  `upstream-svgs` are ELK output. References for this project are generated with `layout: dagre` (ADR-0002).
- **Look.** Default look is `neo` with `redux-color` theme variables: font `"Recursive Variable", arial, sans-serif`,
  text colour `#28253D`, a ~22 KB `<style>`, svg `class="flowchart"`; viewBox = content bounds + 8.
- **Sizing.** Font 14 px, label height 21 px (1.5 em). Node width 120 is `flowchart.minNodeWidth` (default 120, new in
  12.x): narrower labels widen to it and the node padding is added (120 + 32 = 152). Rect = text + (32, 24); diamond
  side = text width + text height + 2 * 15. Cylinder: `ry = rx / (2.5 + w / 50)`, height adds `3 * ry`.
- **Text.** "Yes" is 22.84375 px with kerning in Chromium (24.12 without). Chromium rounds layout units to 1/64 px.
- **Dagre numbers.** `nodesep` / `ranksep` 50, `marginx/marginy` 8, label position centre (`labelpos: 'c'`), edge label
  rank from `makeSpaceForEdgeLabels` (`ranksep /= 2`). Inner graphs of closed clusters: `ranksep + 25`.
- **Cluster order.** dagre receives subgraphs in reverse declaration order, then nodes; `data-color-id` numbers
  clusters in declaration order, depth first. In the SVG a closed cluster is a nested `g.root` inside `g.nodes`, first.
- **Vertex counter.** Every `addVertex` call advances the counter that names `flowchart-<id>-<n>`, new vertex or not;
  an `@{ ... }` block calls it once more.
- **Edge ids.** First link between two nodes `L_a_b_0`, later ones `L_a_b_<existing + 1>`.
- **Wrapping.** Node labels wrap at 120 px, edge labels at 200 px; a wrapped label is a `display: table` div of that width
  with `white-space: break-spaces`, its height lines x 21.
- **Self-loops.** Two helper nodes (0.1 x 0.1 once measured) and three edges for dagre; a synthesised 4-point route on the
  side the helpers landed; helpers drawn as empty `g.label.edgeLabel`; viewBox from the drawn bounds (ADR-0015).
- **CSSOM.** `classDef` rules go through a browser stylesheet: `rgb()` colours, `stroke-dasharray: 5 5` becomes `5,5`,
  `font-family: Arial Black` becomes `"Arial Black"`; a comma ends a declaration.
- **Portability.** `utest.h` pulls in `<windows.h>` on MSVC: never name identifiers `near`, `far`, `small`, `big`;
  use `MERMAID_MIN` / `MERMAID_MAX` (ADR-0007).
- **History.** Older notes (merman golden tables, the ELK investigation, early look differences) are in git history
  before the commit that rewrote this file.
