#!/usr/bin/env bash
# Host tests for the presenter's vertex-color mixing (native/blender_gp/project_grease_gp_color.h),
# plain and under ASan/UBSan, plus a drift guard: the Blender shader lines the mix is based on must
# still be present in the pinned source. Set PG_REQUIRE_BLENDER_SOURCE=1 (CI) to fail, not skip,
# when the Blender source has not been imported.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SHADER="$ROOT/third_party/blender/source/blender/draw/engines/gpencil/shaders/gpencil_vert.glsl"
if [[ -f "$SHADER" ]]; then
  for line in \
    'mixed_col.rgb = mix(mixed_col.rgb, vert_col.rgb, vert_col.a * gpVertexColorOpacity);' \
    'mixed_col.a *= vert_strength * gpLayerOpacity;' \
    'gpencil_color_output(fill_col, fcol_decode, 1.0, gp_mat._fill_texture_mix);'; do
    if ! grep -qF -- "$line" "$SHADER"; then
      echo "Blender shader drift: expected line not found in gpencil_vert.glsl:" >&2
      echo "  $line" >&2
      exit 1
    fi
  done
  echo "shader expressions match the pinned Blender source"
elif [[ "${PG_REQUIRE_BLENDER_SOURCE:-0}" == "1" ]]; then
  echo "Pinned Blender source missing: $SHADER" >&2
  exit 1
else
  echo "SKIP shader drift check (run tools/import_blender_gp.sh first)"
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_gp_color.c")
INC=(-I"$ROOT/native/blender_gp")
WARN=(-Wall -Wno-unused-parameter)
cc -std=gnu11 "${WARN[@]}" "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_gp_color"
"$OUT/test_gp_color"
cc -std=gnu11 -g -fsanitize=address,undefined -fno-omit-frame-pointer "${WARN[@]}" "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_gp_color_asan"
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_gp_color_asan"
