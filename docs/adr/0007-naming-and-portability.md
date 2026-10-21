# ADR-0007: Naming and portability rules

- Status: Accepted
- Realised in: b648070 (2026-10-16), 4043721 (2026-10-18), ea4a12f

## Context

The first namespace was `mermaidcpp`. The project must build with GCC, Clang and MSVC; `<windows.h>` defines the
macros `min`, `max`, `near`, `far`, ... and `utest.h` includes it on MSVC.

## Decision

- Namespace `mermaid`, macros `MERMAID_*`, CMake target `mermaid::mermaid`.
- New code uses std-like `snake_case` for types, functions and variables (`css_compiler`, `node_renderer`). Older
  CamelCase types stay until a dedicated rename step.
- Use `MERMAID_MIN` / `MERMAID_MAX` (`macros.h`) instead of `std::min` / `std::max`, which break when a Windows
  header leaks `min` / `max`.
- Never name an identifier `near`, `far`, `small` or `big` (CI on windows-latest caught `near` and `small`).
- Documentation files are lower case: `checklist.md`, `architecture.md`, `agent.md`.
- Code and commit messages are in English.

## Consequences

- Style is mixed until the rename step; reviewers should not "fix" CamelCase in unrelated commits.
- Windows/MSVC is verified by CI only; local development here is Linux/GCC.

## Alternatives considered

- `std::min` / `std::max` with `NOMINMAX`: depends on every consumer defining it.
