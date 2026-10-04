#!/usr/bin/env bash
# Host tests for the GLES stroke outline (native/blender_gp/project_grease_stroke_outline.c), plain
# and under ASan/UBSan, plus a drift guard on the Blender shader lines the joins follow. Set
# PG_REQUIRE_BLENDER_SOURCE=1 (CI) to fail, not skip, when the Blender source is missing.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHADER="$ROOT/third_party/blender/source/blender/draw/intern/shaders/common_gpencil_lib.glsl"
if [[ -f "$SHADER" ]]; then
  for line in \
    'vec2 miter_tan = safe_normalize(line_adj + line);' \
    'float miter_dot = dot(miter_tan, line_adj);' \
    'const float miter_limit = 0.5; /* cos(60°) */' \
    'bool miter_break = (miter_dot < miter_limit);'; do
    grep -qF -- "$line" "$SHADER" || { echo "Blender shader drift: missing '$line'" >&2; exit 1; }
  done
  echo "miter expressions match the pinned Blender source"
elif [[ "${PG_ALLOW_SKIP:-0}" == "1" && "${PG_REQUIRE_BLENDER_SOURCE:-0}" != "1" ]]; then
  echo "SKIP shader drift check (run tools/import_blender_gp.sh first)"
else
  echo "ERROR: Pinned Blender source missing: $SHADER (set PG_ALLOW_SKIP=1 to skip locally)" >&2; exit 1
fi
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_stroke_outline.c" "$ROOT/native/blender_gp/project_grease_stroke_outline.c")
INC=(-I"$ROOT/native/blender_gp"); WARN=(-Wall -Wextra -Wno-unused-parameter)
cc -std=gnu11 -O1 "${WARN[@]}" "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/t"; "$OUT/t"
cc -std=gnu11 -g -fsanitize=address,undefined -fno-omit-frame-pointer "${WARN[@]}" "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/ta"
ASAN_OPTIONS=detect_leaks=1 "$OUT/ta"
