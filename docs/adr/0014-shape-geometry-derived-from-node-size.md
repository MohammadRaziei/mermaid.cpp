# ADR-0014: One module derives each shape's outline from the node size

- Status: Accepted
- Realised in: the commit that added `src/shape_geometry.cpp` (S12), after ADR-0012 and ADR-0010

## Context

Every shape of Mermaid's neo look (`shapes/*.ts`) fixes three things from the label: its size, its outline, and where
the label sits. They are needed in three places: the sizer (what dagre lays out), the renderer (what is drawn) and
the edge clipping (`node.intersect`, which for hexagon, lean, trapezoid, odd and subroutine is `intersectPolygon`).
The first port had the sizes in `NodeSizer`, outlines in a second `ShapeRenderer` hierarchy that no longer matched
Mermaid, and edges clipped to the box for every shape but the diamond and the circles.

## Decision

`node_sizer` holds the size formulas of every shape, each with the source file and the neo constants named. The
outline is **derived from the size**, never stored: `shape_geometry` (private) turns a shape and a node size into the
polygon points Mermaid computes (`polygon_outline_of`), the label offset (`label_offset`) and the border point of an
edge (`polygon_border_point`, a port of `intersect-polygon.js` and `intersect-line.js`, which place the polygon so
that its bounding box is the node's box). The renderer and `edge_geometry` call the same functions. Circles size their
label to the text (`ignores_min_label_width`), every other shape to at least 120. The old `ShapeRenderer` classes were
deleted.

## Consequences

- All 14 shapes of the classic syntax match Mermaid exactly (fixture `shapes_all`), and edges end on the real outline.
- A new shape is one formula in the sizer, one case in `shape_geometry` or `node_renderer`, and a reference.
- The polygon is recovered from `size`, so a shape whose outline cannot be recovered from its bounding box needs its
  own parameters passed along; none of the current ones does.
- The shapes of the `@{ shape: ... }` family beyond the classic ones are not drawn yet; unknown names become a box.

## Alternatives considered

- Store the outline in `NodeLayout`: public layout would carry drawing data and every consumer would have to keep the
  two in step.
- Keep one renderer class per shape (the earlier design): the per-shape classes duplicated the formulas of the sizer.
