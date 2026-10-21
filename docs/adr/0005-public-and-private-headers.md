# ADR-0005: `include/mermaid/` is the public API; `src/*.h` is private and tests use only the public API

- Status: Accepted
- Realised in: throughout; stated as the owner's principle 3

## Context

Implementation classes (renderers, CSS helpers, dagre stages) change often. Tests that include them freeze the
internal structure.

## Decision

Everything in `include/mermaid/` is public, installed and what examples, tests and consumers use. Headers in `src/`
are private and not installed; the test target does not have `src/` on its include path. Private classes are covered
through the public entry points (`render_svg`, `LayoutEngine`, `flowchart::Parser`, ...). A component is made public
only when it is useful on its own and tested directly: `dagre.h`, `edge_geometry.h`, `css_compiler.h`,
`css_color.h`, `browser_text_measurer.h`, `cli.h`.

## Consequences

- Refactoring inside `src/` does not break tests as long as behaviour holds.
- Some details are only tested indirectly, through rendered SVG.
- A private header that turns out to be wanted by tests is a signal to make a small public API, not to relax the rule.

## Alternatives considered

- Expose everything and test classes directly: faster to write, brittle to change.
