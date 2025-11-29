#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA_DIR="$ROOT_DIR/tests/data"

shopt -s nullglob

for mermaid_file in "$DATA_DIR"/*.mermaid; do
    svg_file="${mermaid_file%.mermaid}.svg"
    echo "Generating ${svg_file##*/}"
    mmdc -i "$mermaid_file" -o "$svg_file"
done
