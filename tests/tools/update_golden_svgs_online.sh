#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA_DIR="$ROOT_DIR/data"

KROKI_URL="https://kroki.io/mermaid/svg"

mapfile -d '' mermaid_files < <(find "$DATA_DIR" -type f -name '*.mermaid' -print0 | sort -z)

for mermaid_file in "${mermaid_files[@]}"; do
    svg_file="${mermaid_file%.mermaid}.svg"

    echo "Generating (online) ${svg_file#$DATA_DIR/}"

    http_code=$(curl -sS -w "%{http_code}" \
        -H "Content-Type: text/plain" \
        --data-binary @"$mermaid_file" \
        "$KROKI_URL" \
        -o "$svg_file")

    if [[ "$http_code" != "200" ]]; then
        echo "❌ Failed to generate SVG for: $mermaid_file (HTTP $http_code)"
        rm -f "$svg_file"
        exit 1
    fi

    if ! head -n 1 "$svg_file" | grep -q "<svg"; then
        echo "❌ Invalid SVG output for: $mermaid_file"
        rm -f "$svg_file"
        exit 1
    fi
done

echo "✅ All SVG golden files generated successfully using online API."
