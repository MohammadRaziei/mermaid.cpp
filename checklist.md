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
- [x] **S6** first simplified layout (longest-path ranks, barycenter order); retired in S6f-6b, replaced by the dagre pipeline
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
  - [x] S6f-6b `LayoutEngine` runs `mermaid::dagre::layout`; `layout_steps` / `layered_graph` removed. Edge labels get `edge_label_pad` (20) on every side in the label rank (matches the basic and 003 goldens; cause not confirmed), cylinder size follows the golden formula, collinear edge points are dropped. 87 tests pass
  - Verified: `tests/cpp/test_dagre.cpp` has 11 reference cases; a further 300 random graphs (all four directions, compound, labels, cycles, self-loops, multi-edges) were compared against dagre-d3-es and matched to 1e-6. The numbers come from running dagre-d3-es 7.0.14 with a small node script that is kept outside the repo.
- [x] **S7f** edges wired to `edge_geometry` (neo markup of Mermaid 12.2 `edges.js`): `edges edgePaths` group, ids with svg prefix, classes, `style` (dash array, pathStyle), `data-*`, `-margin` markers, coloured marker copies after root, HTML edge labels. `linkStyle` follows `flowDb.updateLink` (default first, indexed replaces, `fill:none`). Parity tests ignore `d`/`data-points`/`style` numbers because the reference is ELK. Not verified against a real SVG: invisible (`~~~`) edges (still not drawn; the code suggests Mermaid draws a path), label colours from linkStyle, dotted dash pattern, thick class
- [ ] **S6g** mermaid layout settings. Findings (Mermaid 12.2.0 source read, plus the real-browser SVGs in merman `fixtures/upstream-svgs/flowchart`):
  - Code defaults: `flowchart.nodeSpacing` / `rankSpacing` 50 (`schemas/config.schema.yaml`, `flowRenderer-v3-unified.ts`), dagre `marginx/marginy` 8 (`dagre/index.js`, `prepareLayoutForDagre`), `diagramPadding` 8, subgraph `ranksep + 25`.
  - The real-browser SVG of `basic` has exactly the golden node positions (A 184,34.5; B 184,182.5; C 88,391.5; D 280,391.5; viewBox `4 4 360 418`), so the merman goldens equal real Mermaid output. Drawn node rects equal the golden sizes (152 x 45).
  - Open: the gap between ranks is 40 (A -> B) and the label rank is 40 taller than the label on each side, which the code defaults (50) do not explain. The mechanism (a look/theme setting, or node sizes handed to dagre differing from the drawn ones) is not found yet. Until then `LayoutOptions` keeps 40 / 12 / `edge_label_pad` 20 because they reproduce real output.
  - Cylinder: `ry = rx / (2.5 + w / 50)`, height adds `3 * ry` (read in `shapes/cylinder.ts`; same as the golden).
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
| classdef_and_inline_classes_003 | 5 | 5 | 3 (x of `store`/`reject` differ) |
| comments_and_directives_010 | 5 | 5 | 0 (init directive, cluster) |
| deeply_nested_clusters_019 | 10 | 10 | 0 (subgraph directions, clusters) |
| dense_parallel_edges_002 | 9 | 8 | 0 |
| directives_comments_blank_lines_032 | 4 | 4 | 0 (init spacing) |
| edge_label_position_064 | 3 | 3 | 0 (init spacing) |
| escape_sequences_and_quotes_012 | 5 | 3 | 4 |

(Recomputed after S6f-6b with the dagre layout; tolerance 1 px.)

## Findings from reading mermaid 11.16.0 and dagre-d3-es 7.0.14 (graphify + code)

- Mermaid lays out flowcharts with `dagre-d3-es` (`rendering-util/layout-algorithms/dagre/index.js`). The edge-label "corridor" seen in merman goldens is dagre's `makeSpaceForEdgeLabels`: `ranksep /= 2`, every edge `minlen *= 2`, so a label sits in its own rank.
- Oracle check (done outside the repo with node): feeding the merman golden node sizes of `classdef_and_inline_classes_003` (LR, nodesep 40, ranksep 40) and its two edge-label sizes to dagre gives **exactly** the golden y of all five nodes (107.11, 54.57, 159.65 after the 12 px margin) and the golden x of `in`, `parse`, `validate`. So the 9.6 px y error of the simplified layout is the missing dagre positioning (Brandes-Kopf), not a sizing error.
- Still different: x of `store`/`reject` (golden left-aligns them at 666.74, plain dagre centres them and is 48 px closer): the label rank is wider in the golden. Find how merman/mermaid pads edge-label width before layout.
- **Reference version: Mermaid 12.2.0** (latest release; tags 12.0.0, 12.1.0, 12.2.0; `main` is 12.2.0). merman follows 12.2.0 too (its README says so), so its goldens are 12.2.0 behaviour. The first analysis was done on 11.16.0; `dagre-d3-es` is 7.0.14 in both, so the dagre port is unaffected. In 12.2.0 the layout code (`dagre/index.js`) was split into a measure and a render phase and the subgraph-title margin handling changed; the dagre call itself is the same.
- The constant 120 is `flowchart.minNodeWidth` (default 120, new in 12.x; absent in 11.16.0): labels narrower than that are widened to it and the node padding is added on top (120 + 32 = 152, the width of `in`/`parse` in the goldens). `wrappingWidth` is 120 too. `NodeSizer` should read both from the config instead of hard-coding `min_label_width`.
- Mermaid 12.2.0 and merman defaults: `nodeSpacing`/`rankSpacing` 50, dagre `marginx/marginy` 8, `diagramPadding` 8; the goldens show 40 and 12. Still unexplained (S6g): find what config the golden generation uses.
- The fixture headers say Mermaid@11.12.2; that is only a comment, the goldens come from merman (12.2.0).

## Portability

- Tests include `utest.h`, which pulls in `<windows.h>` on MSVC: never name identifiers `near`, `far`, `small`, `big` (macros there). CI (windows-latest) caught `near` in `test_dagre.cpp` and `small` in `test_approx_text_measurer.cpp`.

## Look of the output (found by comparing with real Mermaid SVGs)

Real Mermaid 12.2 output (`fixtures/upstream-svgs/flowchart/*.svg` in merman, rendered in a browser) versus `mermaid::render_svg`:

- Real default look is `neo` (`data-look="neo"`): font `"Recursive Variable", arial, sans-serif`, text colour `#28253D`, a `<style>` with the edge animation keyframes, svg `class="flowchart"`, svg id = diagram id (`basic`).
- viewBox of the real SVG is `4 4 360 418` for `basic` (content bounds 12..356 plus `diagramPadding` 8); ours is `0 0 368 426`.
- Edge paths stop short of the node by the marker offset (`M184,57L184,93`, node top at 97).
- Ours still draws the classic theme (`#ECECFF` fill, `#9370DB` stroke, trebuchet). Theme/look work is a separate step (take colours from Mermaid's `themes/theme-default.js`, neo styles from `rendering-elements/shapes`).
- `mmdc` itself cannot run in the sandbox (no browser). The browser SVGs above are the comparison source.

## SVG parity with Mermaid (tests/data/mermaid_js, tests/cpp/test_svg_parity.cpp)

- `svg_compare.h` (pugixml via FetchContent, tests only): same element tree, same attribute names, same text, numbers inside any attribute may differ by 0.5. `parity.tsv` marks each fixture `pass` (must match) or `todo` (differences are printed).
- Current state: `basic`, `003`, `034` are `todo` (8 top-level differences each, the comparison stops at the first mismatching child).
- What the real 12.2 output does that ours does not yet (read from `basic.svg`):
  - svg: `xmlns:xlink`, `class="flowchart"`, viewBox = content bounds + 8 padding (`4 4 360 418`), `max-width` from it.
  - `<style>` is 22 KB: the full neo theme CSS (port `getStyles` / neo theme from Mermaid source, not copy the text).
  - one `<g>` holding the markers first, then `<g class="root">` with `clusters`, `edges edgePaths`, `edgeLabels`, `nodes`.
  - 16 marker definitions (`pointEnd`, `pointEnd-margin`, circle, cross, ...).
  - edges are routed orthogonally with rounded corners (`Q` arcs) and carry `data-edge`, `data-et`, `data-id`, `data-points` (base64 JSON of the points), `data-look="neo"`, `marker-end="...-margin"`, and a dash style; edges stop 4 px short of the node for the marker. Edge labels are `foreignObject` HTML in a `labelBkg` div.
  - Next steps, one per commit, each lowering the difference count: skeleton and viewBox, markers, style CSS, node shapes, edges, labels, then flip fixtures to `pass`.

## The default layout of Mermaid 12.2.0 is ELK, not dagre (found while comparing edges)

- `schemas/config.schema.yaml` (root `layout`): "Defaults to `elk`, which is bundled with mermaid; the `tiny` build falls back to `dagre`". `flowRenderer-v3-unified.ts` takes the root `layout`, and `rendering-util/render.ts` registers `elk` and `dagre`. Flowchart default look is `neo`, theme `redux-color`.
- merman generates its `fixtures/upstream-svgs` with `@mermaid-js/mermaid-cli` 12.0.0 + `mermaid` 12.2.0 and a config containing only `handDrawnSeed` (`tools/mermaid-cli/mermaid-config.json`), so those SVGs are ELK output.
- Evidence in the SVG: edge points are orthogonal with the bend 20 px below the source (`basic`: (158.83,268) (158.83,288) (88,288) (88,369)), and out-edges of one node are fanned out (x = 184 -/+ 25.17). Plain dagre-d3-es 7.0.14 on the same graph gives diagonal points ((119.65,264) (84,314.5) (84,365)); our dagre port gives the same, so the port is right and the engine differs.
- Node positions of simple chains agree between dagre and ELK (that is why `basic` matched); `store` / `reject` in fixture 003 differ.
- Consequences: (1) references for the dagre engine must be generated with `layout: dagre` (`%%{init: {"layout": "dagre"}}%%` or frontmatter `config: {layout: dagre}`); (2) matching default `mmdc` output needs an ELK layered port (large, not started); (3) layout-independent parity work (svg skeleton, markers, CSS, shapes, labels) is unaffected.
- Plan: keep dagre as the implemented engine and honour `layout: dagre`; do the layout-independent SVG steps first; ELK is a separate decision.

### Parity steps done

- [x] frame of the document (`basic`): svg attributes and viewBox (bounds + 8), 12 real markers (`pointEnd`, `pointStart`, `...-margin`, circle, cross), the two drop-shadow `<defs>`, group order. Test: `svg_parity.svg_frame_matches_mermaid_js_outside_style_and_root_content` (uses `SvgComparer::prune`). `parity.tsv` differences for `basic`: 8 -> 69, because the comparison now descends into `g.root` (the count rises while the structure becomes comparable; it is not a regression).
- Left for later: `003` viewBox height (ELK layout), `034` linkStyle colours add coloured copies of the markers (reference `g` has 16 children = 12 markers + 3 coloured + root).
- [ ] next: `<style>` CSS, node shapes (`g.node`: rect with `label-container`, `foreignObject` HTML labels), edges (`data-*`, rounded path, marker-end margin), edge labels, clusters.
- [x] nodes (`basic`): `NodeRenderer` writes the neo markup: group `node default` with DOM id `<svg>-flowchart-<id>-<n>` (`Node::dom_index` = Mermaid's vertex counter: every addVertex call counts, also `A & B` members and `style` statements), `data-look`, `transform`, local-coordinate outline (rect `basic label-container`, diamond polygon with `translate(-s/2+0.5, s/2)`), HTML label (`foreignObject` > div `display: table` > span `nodeLabel` > p) as in `createText.ts`; classDef colours as `!important` styles and the browser-computed `rgb()` colour on the div. Test: `svg_parity.nodes_of_basic_match_mermaid_js`. Not ported yet: the other shapes (cylinder, circle, stadium ... still the old outline in local coordinates), label wrapping at 120 px, duplicated class names (`cold cold`).
- [ ] next: `<style>` CSS, edges, edge labels, clusters.
- [x] stylesheet (`basic`, `003` with classDefs): `css_compiler` (stylis-like: namespace prefix, nesting, `&`, comments, minification), `css_color` (hex/rgb/hsl, `fade`, browser `rgb()` form), `theme_variables::redux_color()` (values dumped from Mermaid's own `theme-redux-color.js` with node + esbuild + khroma, not re-derived), `flowchart_css` (templates in the order of Mermaid's `styles.ts`, `flowchart/styles.ts`, `globalStyles.ts`; palette sections per colour slot; classDef rules `> *`, `span`, `tspan` in first-definition order via `Model::class_def_order`). Tests: `svg_parity.stylesheet_*`. Only theme `default` (redux-color, neo) uses it; `dark`, `forest`, `neutral` still use the old simple sheet (dump their variables the same way to port them).
- Naming: new code uses snake_case (std style), including type names (`css_compiler`, `svg_comparer`, `node_renderer`); the older CamelCase types are left until a dedicated rename step.
- [ ] next: edges (`flowchart-link` classes and `data-*`, `-margin` markers, rounded paths), edge labels (HTML `labelBkg`), clusters, then the other shapes.
- [~] edges: `edge_geometry` (public header, 7 tests, values from the real `basic.svg`): `clip_to_nodes`, `border_point` (box, diamond, circle), `shorten_ends` (arrow offset 4), `rounded_path` (radius 5, exact `generateRoundedPath`), `rounded_length`, `points_attribute` (base64 JSON). **Not wired into `edge_renderer` yet.** Spec read from `rendering-elements/edges.js` / `edgeMarker.ts` / `utils/lineWithOffset.ts`: see HANDOFF section 4.
