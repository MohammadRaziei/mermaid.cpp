# ADR-0002: The correctness reference is real Mermaid 12.2.0 with `layout: dagre`

- Status: Accepted
- Realised in: 3a7c34e (2026-10-19), bfed967; supersedes the earlier use of merman goldens as the criterion

## Context

Early work compared against merman's golden layouts and its upstream SVGs. Reading the Mermaid 12.2.0 source showed:

- the default layout of 12.2.0 is **ELK**; merman's `upstream-svgs` were produced with that default, so numbers such
  as rank gap 40 or margin 12 were ELK artefacts (the old step S6g), not dagre values;
- plain dagre (what flowcharts have used for years, and what the dagre port reproduces) gives different edge routes
  and, for some graphs, different node positions;
- merman follows 12.2.0 but is itself an independent implementation, so it can differ from `mmdc`.

## Decision

The final criterion is the SVG that **Mermaid 12.2.0 itself** produces with `{layout: "dagre", handDrawnSeed: 1}`
in headless Chromium, in the default **neo** look with the `redux-color` theme variables. References live in
`tests/data/mermaid_js_dagre/`; `tools/render_all_references.mjs` regenerates them (see ADR-0008). merman's
goldens stay as semantic and structural checks of the parser and as sizing hints, never as the target.

## Consequences

- Output matches `mmdc` only when it is configured with `layout: dagre`. Matching the default 12.2.0 output would
  need an ELK port; that decision has not been taken (`checklist.md`).
- Bumping the reference version means regenerating the references and re-reading the changed Mermaid source.
- Where a number looks odd, the Mermaid source (fetched from GitHub, never vendored) decides, not merman.

## Alternatives considered

- Keep merman's goldens as the target: rejected, they encode ELK behaviour and merman-specific choices.
- Port ELK now: a large piece of work with no fixture demanding it yet.
