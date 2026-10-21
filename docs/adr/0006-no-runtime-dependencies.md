# ADR-0006: No third-party runtime dependencies; vendoring needs the owner's approval first

- Status: Accepted
- Realised in: throughout

## Context

The library must live entirely in the C++ ecosystem: the standard library, CMake and the standard toolchain.

## Decision

No runtime dependency. Needed functionality is written here: a JSON reader (ADR-0011), a CSS compiler (ADR-0009),
a command-line parser (ADR-0013), the dagre and rough.js ports. If something is truly needed, it is added as a
submodule or vendored only after consulting the owner. Fonts are not vendored (ADR-0004).

Test-only tools are allowed: `utest.h` is vendored under `tests/utest/`, and pugixml is fetched with
`FetchContent` for the SVG comparison helper; neither is linked into the library. CLI11 was tried and removed.

## Consequences

- A fresh checkout builds with `cmake` and a C++17 compiler; the CI matrix covers Linux, macOS and Windows.
- More code to own (JSON, CSS, argument parsing), each kept small and tested.
- Configuring the tests needs network access once, for pugixml.

## Alternatives considered

- nlohmann/json, CLI11, a CSS library: convenient, but each is a dependency the owner did not want.
