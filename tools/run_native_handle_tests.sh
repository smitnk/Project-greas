#!/usr/bin/env bash
# Host test for the generation-checked JNI handle registry (header only), plain and under ASan/UBSan.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC="$ROOT/native/blender_gp/tests/test_handle_registry.cc"
INC=(-I"$ROOT/native/blender_gp")
g++ -std=c++17 -Wall "${INC[@]}" "$SRC" -o "$OUT/t" && "$OUT/t"
g++ -std=c++17 -g -fsanitize=address,undefined "${INC[@]}" "$SRC" -o "$OUT/t_asan" && "$OUT/t_asan"
