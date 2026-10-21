# mermaid.cpp

Mermaid diagrams in dependency-free C++17: parse -> layout -> SVG, no JavaScript runtime.

> Status: early. Step 0 done (scaffold + version API). See `checklist.md`.

## Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Coverage (needs `lcov`): `cmake --build build --target mermaid_coverage`.

## Command line

`mermaid_cli` renders a diagram to SVG (options follow mermaid-js/mermaid-cli; only SVG output):

```bash
build/mermaid_cli -i diagram.mmd -o diagram.svg -t dark
echo 'flowchart LR; A-->B' | build/mermaid_cli -i - -o -     # stdin to stdout
build/mermaid_cli --help
```

`-i` (`-` is stdin), `-o` (`-` is stdout; default `<input>.svg`), `-t` theme, `-b` background, `-I` svg id,
`-C` extra CSS file, `-e svg`, `-q`, `-V`, `-h`. Exit code 0, or 1 with the message on stderr.

## Use as a CMake dependency

```cmake
find_package(mermaid CONFIG REQUIRED)
target_link_libraries(app PRIVATE mermaid::mermaid)
```

(or `add_subdirectory` / `FetchContent`; the target name is the same.)

## Docs

- `architecture.md` - design and component map
- `checklist.md` - roadmap and open questions
