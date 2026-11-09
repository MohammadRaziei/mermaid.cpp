# Checklist

Workflow per step: failing test first -> implement -> green -> commit -> clean rebuild and all tests -> zip
(see `agent.md`). Decisions behind the steps are in `docs/adr/`. Legend: `[x]` done, `[~]` partly done, `[ ]` open.

## Status

- 311 tests pass (Linux/GCC; CI also builds macOS and Windows).
- Sequence diagrams: **118 of 119 fixtures at zero differences** (`tests/data/sequence/parity.tsv`, neo and classic look, init keys, font size; the one `todo` is the classic database).
- Pie charts: **20 of 20 fixtures at zero differences** (`tests/data/pie/parity.tsv`, ADR-0018).
- Parity with real Mermaid 12.2.0 (`layout: dagre`, neo look): **19 of 19 fixtures at zero differences**
  (`tests/data/mermaid_js_dagre/parity.tsv`). That is the whole parity net; everything outside those fixtures is
  listed under "Unverified".
- Supported: flowcharts (`graph` / `flowchart`), sequence diagrams and pie charts; every other diagram type throws `UnsupportedDiagram`. All 14 shapes of the classic syntax are drawn in the neo markup.
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
- [x] Init key `layout: dagre` needs no code: `init_layout_dagre` is a `pass` fixture (0 differences; other `layout` values are not ported)
- [x] Init key `look: classic` for all 53 shapes (sizes, outlines, label offsets), edges (plain markers, no dash animation),
  clusters, classDef, stylesheet: fixtures `init_look_classic*` (`_all_shapes`, `_new_shapes`, `_subgraph`,
  `_html_labels_false`, `_html_labels_false_new_shapes`) are `pass`; `look` other than classic/neo is not supported
  (handDrawn exists as a separate path). `flowchart.padding` (init key, any value) is read and threaded through sizer, outlines
  and label offsets: fixtures `init_padding_*` (neo, classic, SVG text, all 53 shapes at padding 25) are `pass`; `fontSize` 20 is
  `pass` in both label modes (`init_font20*`). Also read and `pass`: `flowchart.wrappingWidth` (node labels wrap there, the 120 minimum stays;
  edge labels keep 200), `diagramPadding`, `useMaxWidth: false`, `nodeSpacing`/`rankSpacing`, `curve`, `markdownAutoWrap`.
  Known gap: a label whose text is within ~0.1 px of the wrapping width (Mermaid reports it as exactly the width and wraps)
- [x] Init keys `htmlLabels: false` and `flowchart.htmlLabels: false` (svg_label.h/.cpp, ADR not needed: a port of createFormattedText):
  node, edge and cluster labels as `<text>`/`<tspan>` incl. wrap, `<br/>`, Markdown; node labels read `htmlLabels` alone, edge and
  cluster labels and the classDef rules also `flowchart.htmlLabels`. 12 fixtures `init_*html_labels_false*` are `pass`
  (the text box's right edge comes from a per-glyph overhang table measured in Chromium, Liberation Sans 14px; all 53 shapes
  checked in `init_html_labels_false_new_shapes`; fontSize other than 14 is unchecked)
- [x] Read already: `theme`, `themeVariables`, `fontFamily`, `flowchart.curve|nodeSpacing|rankSpacing`
- [ ] `animate`
- [ ] Markdown inside labels (`**bold**`, `*italic*`, line breaks); only plain text matches
- [ ] `click`, links, tooltips; icons/images; accessibility tags beyond the basic ones
- [ ] Text: table coverage is not audited; `fontFamily` does not change measurement; a different font needs a new table
- [ ] ELK: the 12.2.0 default layout is ELK; matching plain `mmdc` output needs an ELK port. No decision taken
- [ ] Rename the older CamelCase types to `snake_case` (one dedicated step; ADR-0007)
- [ ] MSVC and macOS are verified by CI only; the one `-Wformat` warning in `tests/cpp/test_css_color.cpp`
- [ ] Python bindings (nanobind, as in liburlparser)
- [x] Pie charts (ADR-0018): all five legend positions, `donutHole`, `textPosition`, `highlightSlice`, `useMaxWidth`, title, `showData`, accessibility,
  frontmatter title; the classic default theme only
- [ ] Pie: the other themes and `themeVariables` (their `pie1..12` derivations), `fontFamily`, a chart without sections (all throw `UnsupportedDiagram`)
- [ ] Other diagram types: class, state, ER, gantt, journey, ... (each needs an ADR first)

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
- [ ] `look: handDrawn` (fixture `init_look_hand_drawn`, `todo`, 55 differences): a third pipeline, not a parameter of the
  other two. Measured against Mermaid 12.2: nodes are `rough-node` groups (`data-look="handDrawn"`) drawn with rough.js
  (seeded), edges become rough paths with class `transition` (no `stroke`/`fill`/dash attributes, no `-margin` marker),
  and the viewBox starts at the rough outline's extent (0.27 / 0.19) instead of 0 / 0. Needs the draw-call order of
  rough.js per element (seed per element) to match the jitter exactly; the neo shapes already use `rough_path`

## Sequence diagrams (ADR-0017)

- [x] Model (`sequence.h`) and parser (`sequence_parser`): participants/actors with `as`, `box`, `create`/`destroy`, ten arrow
  types incl. `+`/`-` activation, `activate`/`deactivate`, notes, `loop`/`opt`/`alt`/`par`/`critical`/`break`/`rect`,
  `autonumber [start [step]]|off`, `title`, `accTitle`, `accDescr`; 9 fixtures + 3 behaviour tests match Mermaid's db
- [ ] Parser gaps: `link`/`links`/`properties`/`details`, `participant X@{...}`, half-arrows (`-|\`...), entity codes, `par_over`
  fixture, implicit actors in notes, box colour names, wrap flags on actors
- [x] Renderer, first slice (`sequence_renderer`): participants and `actor` figures, lifelines, mirrored actors, the ten arrow types,
  messages to oneself, plain one-line message texts, neo look, default theme only; fixtures `seq_basic`, `seq_arrows`,
  `seq_three`, `seq_wide`, `seq_empty_text`, `seq_actors`, `seq_self`, `seq_self_edge`, `seq_implicit_actors` are `pass` (tests/data/sequence/parity.tsv, references generated like the flowchart's)
- [x] Text widths: `utils.calculateTextDimensions` measures with an empty font family in the neo look, i.e. the browser default
  serif face, and keeps it unless Liberation Sans is bigger in width, height and line height (`svg_text_metrics`, computed
  from the faces' design units generated by `tools/dump_font_units.py`: any whole px size from 6 to 48, exact against
  getComputedTextLength/getBBox for 5280 strings); `tests/fonts` pins "Times New Roman" to Liberation Serif for the reference renderer
- [x] Renderer, second slice: `<br/>` rows in messages, notes and actor-less self messages, notes (left of / right of / over, widening
  the gaps), activations (`+`/`-`, `activate`/`deactivate`, nested bars; `bounds.updateBounds` pulls open bars up), JavaScript's
  `Math.round` for positions; fixtures `seq_notes`, `seq_notes_wide`, `seq_multiline`, `seq_activation`, `seq_act_*`, `seq_mixed` are `pass`
- [x] Renderer, fragments: `loop`, `opt`, `alt`/`else`, `par`/`and`, `critical`/`option`, `break` (frame, dashed section lines, label box,
  `[title]` wrapped to the width the frame needs with `utils.wrapLabel`, section titles; `bounds.updateBounds` grows every open
  frame by `n` boxMargins) and `rect` backgrounds (lowered to the front, nested); fixtures `seq_fragments`, `seq_frag_nested`,
  `seq_frag_title` (wrapped titles, empty title = zero-width space), `seq_frag_activation`, `seq_frag_rect` are `pass`.
  `calculateTextDimensions` now keeps the sans measurement only when it is strictly bigger in width, height *and* line height
  (the tallest row). Not yet: a word wider than its frame (`breakString`), `par_over` oddities, `rect` without a colour
- [x] Renderer, `autonumber [start [step]]|off`: the index (`start || index`, step rounded to 1/100 after every arrow-type message) is a
  `sequencenumber` marker circle plus a `sequenceNumber` text at the arrow's start (font 12/9/7px by digit count); the line starts
  6px later (12px bidirectional), a bidirectional self loop starts 10px right; no layout effect; fixtures `seq_autonumber`,
  `seq_autonumber_mix` are `pass`. Mermaid's grammar takes whole numbers only (a step of `0.5` is read as 0, i.e. unchanged)
- [x] Renderer, `title` and `accTitle`/`accDescr`: the title is a bare `<text x y=-25>` appended last (x = half the bounds - 2 x
  diagramMarginX), the viewBox gains 40px on top; `<title>`/`<desc>` and `aria-labelledby`/`aria-describedby` on the svg; a bare
  `&` in the title is written `&amp;` (DOMPurify); fixtures `seq_title_acc`, `seq_title_multi` are `pass`
- [x] Renderer, `box` (group of actors): room for its actors and title (`calculateActorMargins`: margin = 5 + half of what the title
  needs beyond actors + 80 - 10), box positions in `addActorRenderingData`, a rect (stroke `rgb(0,0,0, 0.5)`) plus centred title lowered
  to the front, the last box first; a title shifts the actors down; colours via CSS (`rgba`/`hsla` too), no colour = `transparent`
  fill with the whole line as title (parser fixed); fixtures `seq_box`, `seq_box_color`, `seq_box_plain`, `seq_box_mixed` are `pass`.
  Found on the way: the sans overhang of the last glyph at 16px is its own table (`kSansOverhang`), a lone `Q` is 17px high
- [x] Renderer, `create`/`destroy`: a created actor leaves half its width extra (not when the `create` is before the first message:
  Mermaid tests the index for truth), the message stops 21px (stick figure) or half width + 3 short of its box, the box is centred on the
  message line (`starty`/`stopy`), the lifeline starts or stops there, the mirror box of a destroyed actor sits at its `stopy`; fixtures
  `seq_create_destroy`, `seq_create_destroy_mixed` (inside a loop, with activation and a self message) are `pass`
- [x] Renderer, `wrap:`/`nowrap:`: messages (wrapped to max(bounded + 20, 150), no longer widening the gap by their text), notes (per
  placement), participant labels (width 150), `utils.breakString` (a word wider than the line is cut with hyphens, also in fragment
  titles); multi-line participant labels (`<br/>`): one `<text>` per row at the centre, tspan dy = i*16 - (n-1)*8; the parser reads
  `wrap:` after the trimmed colon; fixtures `seq_wrap`, `seq_wrap_notes`, `seq_actor_labels` are `pass`
- [x] Renderer, `box` titles that wrap (`wrap:`, to total - 20) or break (`<br/>`, written `<br>` by DOMPurify, one `<text>` per row),
  `rect` without a colour (fill `hsl(0, 0%, 96%)`, the default theme's); fixtures `seq_box_wrap`, `seq_rect_plain` are `pass`
- [ ] Renderer, the rest: themes other than the default, `look: classic`
- [x] Sequence init keys: `%%{init}%%` `sequence.{width, height, actorMargin, boxMargin, boxTextMargin, noteMargin, wrapPadding, activationWidth,
  labelBoxWidth, labelBoxHeight, diagramMarginX, diagramMarginY, bottomMarginAdj, mirrorActors, wrap, showSequenceNumbers, useMaxWidth}`
  and the top-level `wrap`, later directives win (`sequence::Config`, `Drawing::conf`); `messageMargin` is read by nothing in Mermaid 12.2's
  sequence renderer, so it is ignored; fonts, `messageAlign`, `rightAngles`, `hideUnusedParticipants` throw `UnsupportedDiagram`;
  `mirrorActors: false` has no mirror boxes, lifelines keep 2000 (a destroyed actor's ends at its box middle), no `bottomMarginAdj`;
  `useMaxWidth: false` is `width`/`height` instead of `style`; fixtures `seq_init_*` (6) are `pass`
- [x] Sequence themes (default `redux-color`, `dark`, `forest`, `neutral`, `base`, and `themeVariables`): the sequence variables derive in
  `theme_variables.cpp` as each theme's updateColors() does (`theme_variables::sequence`); no `style` on boxes of non-redux themes, a
  figure gets `stroke: rgb(actorBorder)`; the drop shadow's flood colour is `#000000` for redux, `#FFFFFF` else; `parse_css_color` converts
  hsl in single precision like the browser (221.5 -> 221). `look: classic`: actor height 65 (or the wrapped text's), rx 3, no
  `data-look`/filter/drop shadow defs, no 3px arrow offset, no +30 viewBox, classic figure (glyph from the box top, lifeline at +80, label at
  +35), label box height 20; other looks throw. Fixtures `seq_theme_*` (6) and `seq_look_classic*` (2) are `pass`
- [x] Sequence parser gaps closed: entity codes (`#35;` -> `ﬂ°°35¶ß` in the model like Mermaid, decoded where drawn; widths are measured on
  the encoded text, so the metrics know `ﬂ`; a table of common names, others stay `&name;`), `par_over`, `link`/`links`/`properties`
  (`details` is a no-op in Mermaid 12.2; `link` cuts at the first `@` and drops the character before it; JSON keys keep their order,
  `json_value::order`); drawn: the `class` property (replaces `actor`, fill `#EDF2AE`) and the popup menu `actorN_popup` under an actor with
  links (widens the viewBox, script-like URLs lose the href); a menu on a stick figure and the `icon` property throw. Fixtures
  `seq_entities`, `seq_par_over`, `seq_links`, `seq_links_wide`, `seq_links_classic` are `pass`; parser-only `seq_links_more`
- [x] `participant X@{ "type": .., "alias": .. }` (also `actor`, `create`; JSON or `key: value` pairs; alias stands in for a missing `as`):
  `actor`/`participant` types draw, any other type (boundary, control, entity, database, collections, queue) threw `UnsupportedDiagram`
  (before, the metadata was read as part of the id); fixtures `seq_meta_type`, `seq_meta_create` are `pass`
- [x] Sequence actor types `collections` (box + copy 6px lower left, shadow on the group; neo and classic), `queue` (sideways cylinder, neo and
  classic), `control`, `entity`, `database`, `boundary` (glyph in the 44px band, neo look: `neo_bands`, `write_glyph`); every type also with
  themes, `mirrorActors: false`; fixtures `seq_type_*` (13) are `pass`. Path `d` strings are written without the template's leading/trailing blanks
  (the reference serialisation trims them)
- [x] Sequence classic look of `control`, `entity` and `boundary`: the glyph is measured with getBBox() (44px) and the actor's height set to it plus the label
  box height (twice for a control), which the mirrored copy, the footer bump and a destroyed lifeline then use; the top copy keeps the old height
  (`classic_glyph`, `classic_glyph_height`). `create`/`destroy` of every glyph type in both looks (the arrow is sized to 36px for a control, entity and
  database, not for a boundary, as in `adjustCreatedDestroyedData`). Fixtures `seq_classic_*` and `seq_cd_*` are `pass`
- [ ] Sequence: the classic `database` (throws `UnsupportedDiagram`; the cylinder's getBBox height carries the float32 noise of its arcs, e.g.
  220.00001525878906, which depends on the actor width; `seq_cd_classic_database` stays `todo`). A link menu on a stick figure or a control, entity or
  boundary also throws: Mermaid's own drawPopup fails on them (`rectData` is null)
- [x] Sequence init keys `rightAngles` (self messages are `M H V H` paths, no boxMargin before them), `messageAlign` and `noteAlign`
  (drawText anchors: left = x + margin / start, right = x + width - margin / end), `hideUnusedParticipants` (actors no statement names are
  neither placed nor drawn; widths and margins still count them; element ids and palette colours use the visible index; a box with no
  visible actor is not drawn); fixtures `seq_init_right_angles*`, `seq_init_message_align_*`, `seq_init_note_align`, `seq_init_hide_unused*` are `pass`
- [x] Sequence font size: the top-level `fontSize` (number or `"14px"`, whole px 6 to 48) sets the size of actors, notes, messages, boxes,
  fragments and link menus: every measurement, `font-size` style, the rows' `dy` of a multi-line actor label and the drawn text heights
  follow it. Mermaid's `setConf` copies the top-level `fontSize` over `actorFontSize`/`noteFontSize`/`messageFontSize`, so those keys (and
  `sequence.fontSize`) change nothing, also in the reference; fixtures `seq_font_size_*` (11, neo and classic, wrap, box, links, autonumber) are `pass`
- [ ] Sequence: `actor|note|messageFontFamily/Weight` (throw `UnsupportedDiagram`; they do change the reference), fractional or other-unit font
  sizes, `look: handDrawn`, a link URL without a path (`https://example.com` becomes `https://example.com/` in Mermaid)


- [x] Sequence half arrows (`-|\`, `-|/`, `-\\`, `-//` and the reverse `/|-`, `\|-`, `//-`, `\\-`, plus the dotted `--` forms; LineType 41..48 / 51..58):
  parser tokens, line shortening (stick and reverse heads do not shorten, solid reverse heads move the start by 3), markers
  (reverse = mirror head at the start), autonumber geometry for reverse arrows (x2 before the circle, circle at the other end, self path starts 10
  right); fixtures `seq_half_arrows*` are `pass`, parser oracle for the plain and dotted ones
- [x] Sequence central connection `A->>()B`, `A()->>B`, `A()->>()B` (LineType 59..61): the message carries `central`, the parser adds the activation
  messages (59 for the end, 60 for the start; margins count only `activate`, the drawing walk both), start offset +4 (-6 more for a leftward
  bidirectional), circles in a `<g>` after the line (±16.5 only with an `autonumber` statement, not with `showSequenceNumbers`), numbered line
  geometry; fixtures `seq_central*` are `pass`, parser oracle for two of them
