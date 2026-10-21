# ADR-0001: Port Mermaid to C++ completely; no Rust and no JavaScript at build or run time

- Status: Accepted
- Realised in: from the scaffold (S0); namespace fixed in b648070 (2026-10-16)

## Context

The starting points were [merman](https://github.com/Latias94/merman) (Mermaid in Rust, with a C FFI) and the
`mermaid` npm package (JavaScript). The goal is a dependency-free C++17 library that turns Mermaid text into SVG.
A first attempt vendored the merman-ffi C headers; they held no logic to port, so they were removed again.

## Decision

Write a full C++ port. Behaviour is taken from the Mermaid source and from the dependencies it uses
(`dagre-d3-es`, `rough.js`, `stylis`), re-designed as OOP C++ rather than transliterated.
merman and `mermaid.js` are *references only*: clones stay outside the repository, `mermaid.js` (from `npm pack`) is
never committed, the repository contains no `.rs` file and no vendored Rust or JS runtime. If something cannot be
made independent of Rust or JS it is ported to C++.

Work starts with flowcharts; other diagram types come later (`checklist.md`, "Later").

## Consequences

- Everything Mermaid gets from libraries has to be ported here: the dagre layout (ADR-0003), the rough.js path for
  hand-drawn shapes (ADR-0012), a stylis-like CSS compiler (ADR-0009), JSON reading for `%%{init}%%` (ADR-0011).
- The library builds with CMake and a C++17 compiler only (see ADR-0006).
- Correctness is judged against real Mermaid output (ADR-0002), so a reference renderer is needed for tests, but
  it is a development tool, not a dependency.

## Alternatives considered

- Bind to merman through its C FFI: needs a Rust toolchain; the project is meant to live entirely in the C++
  ecosystem.
- Embed a JavaScript engine and run `mermaid.js`: a large runtime dependency and the opposite of the goal.
