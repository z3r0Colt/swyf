#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/app/src/main/cpp/llama.cpp"
if [ -d "$DEST/.git" ]; then echo 'llama.cpp source already installed'; exit 0; fi
mkdir -p "$(dirname "$DEST")"
git clone --depth 1 --branch b11869 https://github.com/ggml-org/llama.cpp.git "$DEST"
