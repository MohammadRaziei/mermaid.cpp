# agent.md

Guide for AI agents (and humans) working in this repository. Read it before changing anything.

mermaid.cpp turns Mermaid text into SVG in dependency-free C++17. Today it draws **flowcharts**, and the goal is to
match the SVG of **real Mermaid 12.2.0 with `layout: dagre`** (neo look). Where things live:

- `architecture.md` - pipeline and component map
- `checklist.md` - what is done, what is open, what is unverified (keep it true)
- `docs/adr/` - why the project is the way it is; read the index before proposing a different approach
- `README.md` - build, CLI and library usage

## Rules that are not negotiable

1. **TDD.** Write the failing test first, see it fail for the right reason, then make it pass with the smallest change.
2. **Clean OOP, no unnecessary extra work.** One class, one reason to change; small files; no speculative features.
3. **No third-party runtime dependencies** (ADR-0006). If something truly needs a library, stop and ask the owner;
   the answer may be a submodule or vendoring. Fonts are never vendored.
4. **No Rust, no JavaScript runtime in the repo** (ADR-0001). References (merman clone, `mermaid.js`) stay outside the
   repository and are never committed.
5. **Public vs private** (ADR-0005): `include/mermaid/` is the public API, `src/*.h` is private. Tests include only
   public headers. Do not add `src/` to the test include path.
6. **Naming** (ADR-0007): namespace `mermaid`, macros `MERMAID_*`; new code is std-like `snake_case` including type
   names; use `MERMAID_MIN` / `MERMAID_MAX`, never `std::min` / `std::max`; never name an identifier `near`, `far`,
   `small` or `big`. Do not rename old CamelCase types as a side effect.
7. **The criterion is real Mermaid 12.2.0** (ADR-0002), not merman and not your memory of how Mermaid behaves. When
   unsure, read the Mermaid source (below) or generate a reference.
8. **Say what is unverified.** Never claim a behaviour you did not compare against a reference or a test. Add it to
   the "Unverified" list in `checklist.md`.
9. **Decide, do not ask**, about commits and files: delete a file if it is useless, keep it if useful, drop it if it is
   useful but incompatible with the clean structure. Do not ask whether to commit.

## Working with the owner

- The conversation is in **Persian**; code, identifiers, comments, commit messages and these docs are in **English**.
- Style: short and direct. "Continue" means: do the next step without being asked for files or confirmation.
- Generate references yourself (see "References"); never ask the owner to run `mmdc`.
- Each step ends with: tests green -> commit -> **clean rebuild** (`rm -rf build`) and all tests again -> zip the
  whole project (with `.git`, without `build/`) -> send it. Whenever there is rendered output, also send **our SVG
  and PNG next to the real Mermaid PNG** (`ours-<n>.png`, `mermaid-js-<n>.png`). Graphify output, if any, goes in a
  separate zip.

## Build and test

```bash
cmake -S . -B build -G Ninja            # or without -G; pip install cmake ninja if missing
cmake --build build
./build/tests/cpp/test_mermaid_cpp      # all tests; or: ctest --test-dir build --output-on-failure
./build/mermaid_cli -i diagram.mmd -o out.svg
```

Tests fetch pugixml with `FetchContent` (network needed once). CI builds Linux, macOS and Windows; locally only
Linux/GCC is checked, so MSVC and Clang issues show up in CI. There is one old `-Wformat` warning in
`tests/cpp/test_css_color.cpp`.

If git has no identity, set one for the repository only (`git config user.name/user.email`), reusing the identity of
the existing commits.

## Repository map

```
include/mermaid/   public API: render.h (render_svg), flowchart*.h (model, parser, layout), dagre.h,
                   edge_geometry.h, css_*.h, text_measurer.h, browser_text_measurer.h, cli.h, error.h, macros.h
src/               implementation + private headers (dagre_*.cpp, *_renderer.cpp, flowchart_css, init_directive,
                   rough_path, theme_variables, browser_font_metrics.cpp (generated), ...)
cli/main.cpp       mermaid_cli entry point (forwards argv to cli::run)
examples/main.cpp  minimal library use
tests/cpp/         test_*.cpp (utest.h), svg_compare.h (pugixml comparer)
tests/data/merman/ fixture sources (.mmd) and merman semantic goldens
tests/data/mermaid_js_dagre/   parity.tsv (the reference SVGs are generated at build time, not committed)
tests/fonts/                 Liberation Sans (OFL): the font every reference is rendered with
tools/             Node scripts: render_reference.mjs, render_all_references.mjs, dump_font_metrics.mjs,
                   dump_theme_variables.mjs
docs/adr/          decision records
```

## References: how to compare with real Mermaid

Chromium comes from npm, no download from Google is needed:

```bash
mkdir mm && cd mm && npm init -y
npm i @sparticuz/chromium puppeteer-core mermaid@12.2.0 --no-audit --no-fund
cp ../tools/render_all_references.mjs all.mjs
node all.mjs ../tests/data/merman ../refs      # every .mmd in the directory (the build does this itself)
```

The script uses `{layout: "dagre", handDrawnSeed: 1}`, names each SVG after its file (that is also the svg id) and
post-processes like mermaid-cli (including `<br>` -> `<br />`). For one file use `render_reference.mjs <in.mmd> <out.svg> dagre <svg-id>`.

Mermaid's own source is the spec. Fetch what you need, do not vendor it:
`https://raw.githubusercontent.com/mermaid-js/mermaid/mermaid@12.2.0/packages/mermaid/src/<path>` (for example
`rendering-util/layout-algorithms/dagre/index.js`, `mermaid-graphlib.js`, `rendering-elements/clusters.js`,
`diagrams/flowchart/flowDb.ts`). Dagre is `dagre-d3-es` 7.0.14.

Rendering our side for a comparison: a six-line program that reads a `.mmd`, sets `RenderOptions::svg_id` to the
fixture name and calls `render_svg`, linked against `build/libmermaid.a`; or `mermaid_cli -I <fixture> -i ... -o ...`.
PNGs: `pip install novasvg`, then `novasvg.svg2png(svg, png, width=900, background_color=0xFFFFFFFF)`.

## Adding or promoting a fixture (parity)

1. Put the diagram in `tests/data/merman/<name>.mmd`.
2. Put its `.mmd` into `tests/data/merman/`; the build renders the reference (see below), or run the script by hand to look at it.
3. Add a row to `tests/data/mermaid_js_dagre/parity.tsv` as `todo`; the test prints the difference count.
4. Fix differences, smallest cause first (read the first lines the test prints). When the count is **0**, change the
   row to `pass`. Never mark `pass` with differences left.

## Recipes

- **New init key**: test in `test_flowchart_parser.cpp` (`flowchart_init`), add the field to `InitConfig`, read it in
  `src/init_directive.cpp`, apply it where it matters (layout or stylesheet).
- **New node shape**: read `rendering-elements/shapes/<shape>.ts` for the size formula, the markup and `intersect`;
  add the size to `node_sizer`, the outline to `shape_geometry` (polygons) or `node_renderer`, a fixture with a
  reference, and update `checklist.md` (ADR-0014).
- **New CLI option**: extend `k_options` and `parse_arguments` in `src/cli.cpp`, test in `test_cli.cpp`, README and
  `usage()` follow from the table.
- **New diagram type**: a new ADR first (parser, layout, renderer split, detection in `render_svg`); until then it
  throws `UnsupportedDiagram`.

## Traps seen before

- `SvgWriter::num` rounds to 3 decimals; use `detail::js_number` where the reference prints full JavaScript numbers
  (rough paths, cluster label transforms).
- Text widths can differ from Chromium by 1/64 px; that is inside the 0.5 parity tolerance, do not chase it.
- Order matters in dagre: node insertion order (subgraphs in reverse declaration order, then nodes) and edge order
  change tie-breaks. Keep `make_input` order unless a reference says otherwise.
- `Layout::nodes` lists only the nodes of that graph; nodes of closed subgraphs are in `Layout::roots[i].layout`
  with their own coordinates. Code that walks a layout must recurse (see `tests/cpp/test_flowchart_layout.cpp`).
- Layout coordinates are dagre's own (it is given margin 8 and translates the graph); `Layout::bounds` is what is drawn
  and the `viewBox` comes from it, so its origin may not be 0 (ADR-0015). Never renormalise the coordinates.
- Edge ids: the first link between two nodes is `L_a_b_0`, later ones `L_a_b_<existing + 1>` (so 0, 2, 3, ...).
- A self-loop is three dagre edges and two 0.1 x 0.1 helper nodes, merged into one synthesised route; paths of loops
  are written after the other paths, their labels keep graph order.
- Labels wider than 120 (nodes) or 200 (edges) wrap; their height is lines x 21 and the div becomes a `table` with
  `white-space: break-spaces`.
- `fontFamily` from `%%{init}%%` changes the CSS only, not the text measurement.
- Themes: all five (`default`, `dark`, `forest`, `neutral`, `base`) are ported; only three `themeVariables` keys are applied from `%%{init}%%`.
- `utest.h` pulls in `<windows.h>` on MSVC: no `near`, `far`, `small`, `big`; avoid `min` / `max` names.
- Reference SVGs are not committed. The build renders them with real Mermaid 12.2.0 (Node.js + npm + network;
  `<build>/references/svg`, re-run with the `mermaid_references` target), or uses `-DMERMAID_REFERENCE_DIR=<dir>`. Without
  either, the parity tests report `skipped`. The script pins the fonts of `tests/fonts` for every family, so widths do not
  depend on the machine (the 44 references rendered this way were byte-identical to the ones that used to be committed).
  CI renders them once on Linux and shares them with the macOS and Windows jobs. `mmdc` is not used: it cannot pin the
  font without adding CSS to the SVG, and its Mermaid version is not ours.

## Keeping the docs true

- After every step: update `checklist.md` (done, open, unverified), keep `parity.tsv` honest, and update
  `architecture.md` if a component appeared or changed.
- A decision that changes or a new one that matters gets an ADR (`docs/adr/0000-template.md`); supersede, do not
  rewrite history.
- When the owner adds a principle, put it here (rules above) and, if it has consequences, in an ADR.
