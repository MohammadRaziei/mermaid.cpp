# ADR-0011: Read `%%{init}%%` with a small JSON reader of our own

- Status: Accepted
- Realised in: 5f4f5a9 (2026-10-20)

## Context

`%%{init: {...}}%%` (also `initialize`, also multi-line) changes spacing and theme variables. Fixtures 010, 032 and
064 use it. A JSON library would be a dependency (ADR-0006).

## Decision

`src/init_directive.cpp` has a small recursive-descent JSON reader (objects, arrays, strings, numbers, literals;
nesting limited to 32) and `parse_init_directives`, which merges every directive in source order (later wins) into
`flowchart::InitConfig` stored in `Model::config`. Read today: `theme`, every `themeVariables` entry (after
Mermaid's sanitising, ADR-0016), `fontFamily`, and `flowchart.nodeSpacing`, `rankSpacing`, `curve`. Invalid JSON is ignored, as Mermaid only logs it.
Settings from the diagram text override the options given to `LayoutEngine`.

## Consequences

- No dependency and no surprises with malformed input.
- Other keys (`look`, `layout`, `htmlLabels`, ...) are ignored for now.
- `\uXXXX` escapes are kept as written, not decoded; no value read today needs them.

## Alternatives considered

- A JSON library: rejected by ADR-0006.
- Regular expressions per key: fragile with nesting and multi-line directives.
