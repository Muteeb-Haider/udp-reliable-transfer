#!/usr/bin/env bash
set -euo pipefail
SIZE="${1:-262144}" # 256KB default
OUT="sample_data/sample.bin"
mkdir -p sample_data
head -c "$SIZE" </dev/urandom > "$OUT"
echo "Wrote $OUT ($SIZE bytes)"
