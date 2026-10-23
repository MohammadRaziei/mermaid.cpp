# ADR-0015: Lay self-loops out with helper nodes, keep dagre's coordinates, size the viewBox from what is drawn

- Status: Accepted
- Realised in: the commit that added `merged_self_loop` and `measure_bounds` (S14); builds on ADR-0003 and ADR-0010

## Context

Fixtures with a self-loop (`a --> a`) did not match. Reading `dagre/index.js` of Mermaid 12.2.0 showed that it does
not give such an edge to dagre. `prepareLayoutForDagre` adds two helper nodes `<n>---<n>---1` and `---2` and replaces
the edge by three (node -> helper 1 -> helper 2 -> node, the label on the middle one). After layout
`getEdgesToRender` merges the three back into one edge whose four-point route is **synthesised** from the node's
size and the side the helpers ended up on (`getSelfLoopSide`, `getSelfLoopPoints`, `getSelfLoopLabelPosition`);
dagre's own points are not used. The helper nodes are measured once drawn, so dagre lays out with their drawn size
(an empty rect of 0.1 x 0.1, not the 10 x 10 they are created with), and they stay in the SVG as empty
`g.label.edgeLabel` groups. Two further observations came with it: the SVG `viewBox` is the `getBBox()` of what is
drawn (so its origin need not be 8), and edge paths are drawn in the order of `graph.edges()` with the merged loops
last, while the edge labels keep the graph order.

## Decision

`LayoutBuilder` does what Mermaid does: two helper nodes (0.1 x 0.1, same parent as the node) and three dagre edges
per loop (names `<node>-cyclic-special-0..2`), then one `EdgeLayout` with `self_loop = true`, the synthesised route
and the label beside it; the helpers are listed in `Layout::loop_helpers` and drawn after the nodes. Paths are
written ordinary edges first, loops after. The layout hands **dagre's own coordinates** to everything (dagre is given
`marginx` / `marginy` 8 and translates the graph itself); nothing is shifted afterwards. `Layout::bounds` is the extent
of what is drawn, the curve of a self-loop measured as the browser would (`edge_geometry::curve_bounds` of the clipped,
corner-fixed, pulled-back route), and the renderer builds the `viewBox` from it.

## Consequences

- Fixtures `s14_selfloop_multiedge` and `s14_selfloop_lr` match Mermaid with zero differences.
- Layout coordinates are no longer renormalised, so a graph whose drawn content does not reach dagre's margin gets
  a `viewBox` origin other than 0, exactly as Mermaid does.
- Not ported: several loops on one node (Mermaid reuses the same helper nodes and edge names, so only the last
  loop survives), and loops on a subgraph id (`fromCluster` / `toCluster`).
- The same pass fixed edge ids: the second link between two nodes is `L_a_b_2` (existing links + 1), the third `_3`.

## Alternatives considered

- Keep dagre's native self-loop handling: its route and the node positions differ from Mermaid's.
- Renormalise the layout to the drawn bounds: that is what the first version did; it moved the nodes away from
  where Mermaid puts them as soon as something dagre sizes is not drawn at that size.
