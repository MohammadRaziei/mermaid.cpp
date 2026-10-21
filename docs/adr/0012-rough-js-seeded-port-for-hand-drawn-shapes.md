# ADR-0012: Port rough.js path generation with its seeded random stream

- Status: Accepted
- Realised in: c4da82f (2026-10-20)

## Context

In the neo look of 12.2.0 the stadium (and other shapes) are drawn with rough.js: a `<g class="basic label-container
outer-path">` with two paths whose coordinates depend on a pseudo-random stream seeded by `handDrawnSeed`.

## Decision

`src/rough_path.cpp` repeats rough.js `path()` for polygons, including the seeded generator, and `js_number`
formats numbers like JavaScript's `Number#toString`. `RenderOptions::hand_drawn_seed` defaults to 1, the value the
references use (ADR-0002). A seed of 0 is treated as 1.

## Consequences

- Fixtures 002 and 012 match exactly, including path data.
- Only the polygon case is ported; other rough.js primitives (ellipse, curve) are not.
- Shapes other than box, diamond, cylinder and stadium still use older outline code (`checklist.md`).

## Alternatives considered

- Draw smooth polygons: visibly different and never equal to the reference.
