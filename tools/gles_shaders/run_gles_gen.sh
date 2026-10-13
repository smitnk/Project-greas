#!/usr/bin/env bash
# L8: builds the GLES 3.2 shader generator from Blender 3.6.23's GPU shader code and writes the
# Grease Pencil draw-engine shaders to native/blender_gp/gles/generated (or $1). With GLSLANG (path
# of a glslang validator) every stage is validated as OpenGL ES 3.20. --check: regenerate into a
# temporary directory and fail if the committed shaders differ.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/../native_host_closure.sh" "GLES shader generator"
COMMITTED="$ROOT/native/blender_gp/gles/generated"
CHECK=0; [[ "${1:-}" == "--check" ]] && { CHECK=1; shift; }
DEST="${1:-$COMMITTED}"; [[ $CHECK == 1 ]] && DEST="$OUT/gles_check"
G="$OUT/gles"; mkdir -p "$G/gen" "$G/dtc"
python3 -I "$ROOT/tools/gles_shaders/gen_lists.py" "$BL" "$G/gen" > "$G/glsl.txt"
gcc -O1 -w -o "$G/datatoc" "$BL/datatoc/datatoc.c"
DOBJ=()
while read -r f; do
  n="$(basename "$f")"; (cd "$(dirname "$f")" && "$G/datatoc" "$n" "$G/dtc/$n.c"); gcc -c -w "$G/dtc/$n.c" -o "$G/dtc/$n.o"; DOBJ+=("$G/dtc/$n.o")
done < "$G/glsl.txt"
python3 -I "$ROOT/tools/gles_shaders/make_backend.py" "$B" "$ROOT/native/blender_gp/gles/pg_gles_shader_gen.cc" --check
GI=(-I"$G/gen" -I"$BL/gpu" -I"$BL/gpu/intern" -I"$BL/gpu/opengl")
while read -r d; do GI+=(-I"$d"); done < <(find "$BL/draw" "$BL/gpu" "$BL/compositor" -type d -not -path "*/tests*")
GX=(-std=c++17 -funsigned-char -fno-strict-aliasing -ffp-contract=off -DNDEBUG -w)
for s in "$BL/gpu/intern/gpu_shader_create_info.cc" "$BL/gpu/intern/gpu_shader_dependency.cc" "$ROOT/native/blender_gp/gles/pg_gles_shader_gen.cc"; do
  g++ "${GX[@]}" "${GI[@]}" "${INC[@]}" -c "$s" -o "$G/$(basename "$s").o"
done
g++ "$G"/*.cc.o "${DOBJ[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$G/pg_gles_shader_gen" > "$G/link.txt" 2>&1 || {
  grep -oE "(undefined reference to|multiple definition of) \`[^']*'" "$G/link.txt" | sort | uniq -c | sort -rn | head -30; exit 1; }
mkdir -p "$DEST"; FAILED=0
INFOS=(gpencil_geometry gpencil_layer_blend gpencil_mask_invert gpencil_depth_merge gpencil_antialiasing_stage_0
       gpencil_antialiasing_stage_1 gpencil_antialiasing_stage_2 gpencil_fx_composite gpencil_fx_colorize
       gpencil_fx_blur gpencil_fx_transform gpencil_fx_glow gpencil_fx_rim gpencil_fx_shadow gpencil_fx_pixelize
       overlay_edit_gpencil_wire overlay_edit_gpencil_point)
for info in "${INFOS[@]}"; do
  "$G/pg_gles_shader_gen" "$info" "$DEST/$info.vert.glsl" "$DEST/$info.frag.glsl"
  # ADAPTED: OpenGL ES has no 1D textures; the weight ramp (G_draw.weight_ramp, 256x1) is a 2D texture.
  if [[ "$info" == overlay_edit_gpencil_* ]]; then
    sed -i 's/uniform sampler1D weightTex;/uniform sampler2D weightTex;/; s/texture(weightTex, t)/texture(weightTex, vec2(t, 0.5))/' "$DEST/$info.vert.glsl" "$DEST/$info.frag.glsl"
  fi
  if [[ -n "${GLSLANG:-}" ]]; then
    if "$GLSLANG" -S vert "$DEST/$info.vert.glsl" > "$G/$info.v.txt" 2>&1 && "$GLSLANG" -S frag "$DEST/$info.frag.glsl" > "$G/$info.f.txt" 2>&1; then echo "GLES 3.20 OK: $info"; else echo "GLES 3.20 FAIL: $info"; grep -h ERROR "$G/$info.v.txt" "$G/$info.f.txt" | head -3; FAILED=1; fi
    # the OpenGL ES 3.1 path (pg_gles_source_for_api): ESSL 3.10 with GL_EXT_texture_buffer
    for st in vert frag; do
      sed '1s/^#version 320 es$/#version 310 es\n#extension GL_EXT_texture_buffer : require/' "$DEST/$info.$st.glsl" > "$G/$info.310.$st"
      if "$GLSLANG" -S $st "$G/$info.310.$st" > "$G/$info.310.$st.txt" 2>&1; then :; else echo "GLES 3.10 FAIL: $info $st"; grep -h ERROR "$G/$info.310.$st.txt" | head -3; FAILED=1; fi
    done
  fi
done
[[ $FAILED == 0 ]]
python3 -I "$ROOT/tools/gles_shaders/embed.py" "$DEST" "${INFOS[@]}"
if [[ $CHECK == 1 ]]; then
  diff -r "$COMMITTED" "$DEST" > "$OUT/gles_diff.txt" || {
    echo "ERROR: native/blender_gp/gles/generated differs from what Blender 3.6.23's shader code produces:" >&2
    head -40 "$OUT/gles_diff.txt" >&2; exit 1; }
  echo "GLES shaders are up to date (${#INFOS[@]} shaders)"
fi
