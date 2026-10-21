# ADR-0003: Port the dagre-d3-es pipeline stage by stage, one class per stage

- Status: Accepted
- Realised in: 4dee627 (2026-10-18); closed subgraphs in b1f7b3a (2026-10-21)

## Context

The first layout was a simplification (longest-path ranks, barycentre ordering). It could not reproduce Mermaid's
positions, label ranks or compound-node (subgraph) boxes. Mermaid lays flowcharts out with `dagre-d3-es` 7.0.14.

## Decision

Port `layout.js` (`runLayout`) faithfully, as separate classes: `Acyclic`, `NetworkSimplexRanker`, `NestingGraph`,
`EdgeNormalizer`, `ParentDummyChains`, `BorderSegments`, `Orderer`, Brandes-Köpf `position`, plus self-edges, label
placement, border nodes and edge points. The graph container is graphlib-compatible (multigraph, nesting, the same
visiting order including JavaScript's integer-key ordering), because tie-breaking depends on that order. The public
API is `mermaid::dagre::layout` (`include/mermaid/dagre.h`); `LayoutEngine` adapts the flowchart model to it.

Mermaid's own glue around dagre is ported too: compound boxes, and rendering a subgraph with no edge leaving it as a
graph of its own (ADR-0010).

## Consequences

- Results equal dagre's, including tie-breaks. Verified: 11 reference cases in `tests/cpp/test_dagre.cpp`, and 300
  random graphs (four directions, compound nodes, labels, cycles, self-loops, multi-edges) compared with
  `dagre-d3-es` 7.0.14 to 1e-6 (done once with a Node script that is not kept in the repo).
- More code than a simplified layout, and it follows dagre's quirks on purpose.
- `dagre.h` is public because it is tested and usable on its own.

## Alternatives considered

- Improve the simplified layout: no end in sight for matching dagre by approximation.
- Use ELK-like layered layout: see ADR-0002.
