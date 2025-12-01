#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA_DIR="$ROOT_DIR/data"

mapfile -d '' mermaid_files < <(find "$DATA_DIR" -type f -name '*.mermaid' -print0 | sort -z)

for mermaid_file in "${mermaid_files[@]}"; do
    svg_file="${mermaid_file%.mermaid}.svg"
    echo "Generating ${svg_file#$DATA_DIR/}"
    mmdc -i "$mermaid_file" -o "$svg_file"
done
