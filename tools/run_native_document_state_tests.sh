#!/usr/bin/env bash
# Host tests for save/load state (native/blender_gp/project_grease_document_state.c), plain and
# under ASan/UBSan.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_document_state.c" "$ROOT/native/blender_gp/project_grease_document_state.c")
INC=(-I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp")
WARN=(-Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter)
cc -std=gnu11 "${WARN[@]}" "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_document_state"
"$OUT/test_document_state"
cc -std=gnu11 -g -fsanitize=address,undefined -fno-omit-frame-pointer "${WARN[@]}" "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_document_state_asan"
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_document_state_asan"
