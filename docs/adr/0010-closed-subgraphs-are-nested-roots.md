# ADR-0010: A subgraph with no edge leaving it is laid out as a graph of its own

- Status: Accepted
- Realised in: 42e7820 (2026-10-20), b1f7b3a (2026-10-21)

## Context

Reading `mermaid-graphlib.js` (`adjustClustersAndEdges`, `extractor`) and the dagre `recursiveRender` showed that
Mermaid does not give every subgraph to one dagre graph. A cluster without an edge crossing its border is extracted:
it is laid out on its own (own `direction`, otherwise perpendicular to the parent), then becomes a single node of the
parent sized by its box. The first port drew all clusters in one flat graph, which made fixture 032 look completely
different (two clusters stacked, each laid out left to right).

## Decision

`LayoutBuilder` works per graph (`Scope`): leaves, clusters that stay compound, and clusters that are closed. A
cluster is closed when no edge has exactly one end below it; endpoints that are cluster ids are resolved to anchors
exactly as `getAnchorId` does, and an anchor marks its parent as having an external connection. Inner graphs use the
explicit `direction`, else the perpendicular of the parent, `ranksep + 25` and the parent's `nodesep`. The result is
`Layout::roots` (a nested `Layout` with its own coordinates, plus `box` and `origin`), drawn as nested `g.root`
groups. In the root graph, cluster boxes are the dagre compound nodes.

## Consequences

- Fixtures 010, 019 and 032 match Mermaid (zero differences); an edge between two closed clusters runs from box to box.
- Any cluster without an outside edge now lays out perpendicular to its parent even without `direction`; this is
  real Mermaid behaviour.
- Ported later (S13): `findNonClusterChild` with the common-edges rule, the safe anchor (`findSafeAnchorNode`), the
  order of edges after `adjustClustersAndEdges` (edges at a cluster move behind the rest), and the self-loop test on
  the edge as written. Not ported: `click`/links on clusters.
- `Layout::nodes` no longer lists every node of the diagram; nodes inside a root live in `roots[i].layout`.

## Alternatives considered

- Keep the flat graph and add per-cluster direction after the fact: cannot reproduce Mermaid's sizes and spacing.
