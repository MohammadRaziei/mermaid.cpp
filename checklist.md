# Checklist

Workflow per step: failing test first -> implement -> green -> commit -> zip.

## Steps

- [x] **S0** scaffold: git (`master`), layout mirroring liburlparser, CMake, vendored `utest.h`, version API (TDD)
- [x] **S1** geometry value types (`Point`, `Size`, `Rect`) + `TextMeasurer` interface + `ApproxTextMeasurer` (Arial metrics, 14px, 21px line)
- [ ] **S1b** (on hold) accurate `TextMeasurer` using the standard approach for fonts; no specific font is vendored and no extra dependency may be added without the owner's approval. Approach to be agreed first
- [ ] **S2** flowchart `Model` + `StyleParser`
- [ ] **S3** `ShapeScanner` and `EdgeScanner`
- [ ] **S4** flowchart `Parser` (statements, chains, `&`, subgraphs, class/style/linkStyle), merman semantic goldens
- [ ] **S5** `NodeSizer` (one strategy per shape)
- [ ] **S6** layout steps: `CycleBreaker`, `Ranker`, `DummyInserter`, `CrossingMinimizer`, `CoordinateAssigner`, `EdgeRouter`, `ClusterBoxer`, `LayoutEngine`
- [ ] **S7** render: `SvgWriter`, `Theme`, `ShapeRenderer`s, `EdgeRenderer`, `FlowchartRenderer`
- [ ] **S8** facade `Mermaid::render`, CLI, `examples/main.cpp`
- [ ] **S9** golden layout parity with merman (`*.layout.golden.json`)
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

- The merman layout goldens for `A[Start] --> B{Choice}` give rect = **152 x 45**,
  rank gap = **40**, diamond = **171 x 171**. These do not match the defaults
  assumed in the first prototype (padding 15, 54 px rect, gap 50). Before S5/S9,
  determine which config + text measurer produced the goldens and make
  `NodeSizer` / `LayoutOptions` reproduce them. Tests must assert golden
  numbers, not guessed constants.
- `v0` prototype (flat parser/layout/svg, 1.5k lines) is kept outside the repo
  as a behavioural reference to port from, test-first.
