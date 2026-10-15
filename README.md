# mermaid.cpp

Mermaid diagrams in dependency-free C++17: parse -> layout -> SVG, no JavaScript runtime.

> Status: early. Step 0 done (scaffold + version API). See `checklist.md`.

## Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Coverage (needs `lcov`): `cmake --build build --target mermaidcpp_coverage`.

## Use as a CMake dependency

```cmake
find_package(mermaidcpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE mermaidcpp::mermaidcpp)
```

(or `add_subdirectory` / `FetchContent`; the target name is the same.)

## Docs

- `architecture.md` - design and component map
- `checklist.md` - roadmap and open questions
- `vendor/README.md` - third-party reference material
