# ADR-0013: `mermaid_cli` is SVG-only, with its own argument parser and its logic in a public header

- Status: Accepted
- Realised in: 7a16c5b (2026-10-21)

## Context

The owner wants a command-line target modelled on the mermaid-js/mermaid-cli README, producing SVG only. CLI11 was
tried and rejected (ADR-0006).

## Decision

`include/mermaid/cli.h` declares `cli::options`, `cli::parse_arguments`, `cli::usage` and `cli::run(args, in, out,
err)`; `src/cli.cpp` implements them; `cli/main.cpp` only forwards `argv`. This keeps the CLI testable without
starting a process (ADR-0005). Options: `-i` (`-` is stdin), `-o` (`-` is stdout; default `<input>.svg`, `out.svg`
for stdin), `-t`, `-b`, `-I`, `-C`, `-e svg`, `-q`, `-V`, `-h`, long forms and `--opt=value`. Exit code 0, or 1 with
the message on stderr (a syntax error includes the line).

## Consequences

- Differences from mermaid-cli: `-i` is required (no implicit stdin); `-e png|pdf` is rejected; no markdown input;
  no `-c`, `-p`, `-w`, `-H`, `-s` (they only matter for PNG or puppeteer).
- `-t dark|forest|neutral` is accepted but those themes are not ported to the neo look (ADR-0009).
- Tested through temporary files and streams; not exercised on Windows locally.

## Alternatives considered

- CLI11: a dependency (ADR-0006).
- Logic in `main.cpp`: not testable without a subprocess.
