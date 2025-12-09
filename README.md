# mermaid.cpp

<img src="docs/images/mermaid.cpp.svg" alt="mermaid.cpp logo" width="120" />

A **headless**, **lightweight** C++17 implementation of the [Mermaid](https://mermaid.js.org/) diagramming library that runs without a browser, Node.js, or any external dependencies.

## Project Goals

- **Port Mermaid‑js to modern C++** – feature‑by‑feature, following the original JavaScript architecture.
- **Zero external runtime** – no browser, no Node, no JavaScript engine. Pure C++17.
- **Easy language bindings** – clean C‑style API for seamless integration with Python, Rust, Go, .NET, etc.
- **Production‑ready** – robust error handling, comprehensive test suite, and deterministic SVG output.
- **Educational value** – a reference implementation that demonstrates how a complex diagramming engine can be built in a systems language.

## Architecture

The library follows the same pipeline as Mermaid‑js:

```
Lexer → Parser → AST → Layout → SVG Renderer
```

- **Lexer** – tokenizes the Mermaid syntax (sequence diagrams, flowcharts, etc.)
- **Parser** – builds an abstract syntax tree (AST) using the grammar of each diagram type.
- **AST** – a visitor‑based tree representing the diagram’s logical structure.
- **Layout** – computes coordinates, sizes, and spacing for all visual elements.
- **Renderer** – generates standard‑compliant SVG with exact pixel‑perfect matching to the original Mermaid output.

Design patterns used: Composite, Visitor, Strategy, Factory.

## Current Status

We are porting **sequence diagrams** first, as they are the most widely used diagram type. The work is done in small, test‑driven increments:

1. Study the corresponding JavaScript function in `./mermaidjs/packages/mermaid/src/`.
2. Add (or locate) a `.mermaid` test file in `./mermaidcpp/tests/data/`.
3. Generate the golden SVG reference using the online Mermaid editor (via `update_golden_svgs_online.sh`).
4. Write a failing doctest that compares our SVG output with the golden SVG.
5. Implement the minimal C++ code needed to pass the test.
6. Commit and repeat for the next feature.

### Progress Checklist – Sequence Diagrams

#### Core Infrastructure
- [x] Lexer for sequence diagram syntax
- [x] Parser for participants, messages, notes, activations, blocks
- [x] AST nodes (ParticipantNode, MessageNode, NoteNode, ActivationNode, BlockNode, SequenceDiagramNode)
- [x] Basic layout algorithm (participant positioning, message stacking)
- [x] SVG renderer with style/defs from golden SVGs
- [x] Visitor pattern for rendering
- [x] CLI tool (`mermaidcpp-cli`) to render `.mermaid` files to `.svg`

#### Participants & Messages
- [x] Regular participants (rectangles with labels)
- [x] Actors (stick‑figure icons)
- [x] Simple messages (solid arrows)
- [x] Dotted messages (dashed arrows)
- [x] Destroy messages (cross‑head arrows)
- [x] Self‑messages (curved arrows)
- [x] Message text positioning

#### Activations
- [x] Activation rectangles (basic, multiple, destroy)
- [x] Activation on source participant (left‑to‑right)
- [x] Activation on target participant (right‑to‑left)
- [x] Activation on both participants
- [x] Activation with destroy sequence
- [x] Activation suppression where golden SVG expects empty groups

#### Blocks (structured control flow)
- [x] Loop block (`loop … end`)
- [x] Alt/Else block (`alt … else … end`)
- [x] Parallel block (`par … and … end`)
- [ ] Opt block (`opt … end`)
- [ ] Critical block (`critical … end`)
- [ ] Break block (`break … end`)
- [ ] Rect block (`rect … end`)

#### Notes
- [x] Note parsing (`Note over A: text`)
- [x] Note rendering (rectangle + text)
- [ ] Note positioning layout (currently hard‑coded; needs proper layout algorithm)

#### Aliases & Advanced Features
- [ ] Participant aliases (`participant A as Alice`)
- [ ] Grouping (`box`/`end`)
- [ ] Automatic numbering
- [ ] Styling directives (`style`, `classDef`)
- [ ] Hyperlinks & tooltips

#### Layout Refinements
- [ ] Dynamic width/height calculation for notes and actors
- [ ] Proper spacing for complex diagrams
- [ ] Support for very long participant labels
- [ ] Multi‑line note text

### Test Coverage

As of today, **22 out of 24** sequence‑diagram golden tests pass. The remaining failures are:

1. `actor_basic.mermaid` – actor layout dimensions mismatch
2. `note_basic.mermaid` – note positioning layout not yet implemented
3. `notes_and_messages.mermaid` – same note layout issue
4. `simple_alias.mermaid` – alias support missing
5. `mixed_features.mermaid` – combination of unsupported features
6. `complex_seq_diagram.mermaid` – many advanced features not yet ported

## Getting Started

### Build

```bash
cd mermaidcpp
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

### Use the CLI

```bash
./mermaidcpp-cli input.mermaid output.svg
```

### Embed in Your Code

```cpp
#include "mermaid.h"

std::string svg = mermaid::render_to_svg("sequenceDiagram\nAlice->>Bob: Hello");
```

## Why mermaid.cpp?

- **Performance** – orders of magnitude faster than launching a browser or Node process.
- **Deterministic output** – same input always produces the same SVG, ideal for CI/CD and documentation generation.
- **Small footprint** – static linking yields a single executable with no runtime dependencies.
- **Easy to bind** – a simple C API (`mermaid_render_to_svg`) makes it trivial to create wrappers for Python, Ruby, Rust, Go, Java, C#, etc.
- **Educational** – a clean, modular C++17 codebase that shows how a real‑world diagramming engine works under the hood.

## Roadmap

1. **Complete sequence diagrams** – finish all missing features (opt, critical, break, rect, aliases, note layout).
2. **Flowcharts** – port the second most popular diagram type.
3. **Other diagram types** – class diagrams, state diagrams, Gantt charts, etc.
4. **C API & language bindings** – provide official Python, Rust, and Go packages.
5. **WASM build** – compile to WebAssembly for browser‑side usage without JavaScript.

## Contributing

We welcome contributions! Please follow the TDD workflow:

1. Pick a failing test (or add a new `.mermaid` test).
2. Generate the golden SVG using the online editor (see `tests/tools/update_golden_svgs_online.sh`).
3. Write a doctest in `tests/test_sequence_diagram_svg.cpp`.
4. Implement the missing functionality in the C++ code.
5. Ensure all existing tests still pass.
6. Submit a pull request.

## License

MIT – same as the original Mermaid‑js project.

## Acknowledgments

- The [Mermaid](https://mermaid.js.org/) team for creating an amazing open‑source diagramming language.
- All contributors who help port this complex system to C++.

---

*“Diagrams as code” – now in C++.*
