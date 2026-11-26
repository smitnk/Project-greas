#!/usr/bin/env bash
# .blend open / save (native/blender_gp/project_grease_blend_io.cc: Blender 3.6.23 readfile.cc / writefile.cc
# port) built with the real pinned closure: DNA (makesdna output + dna_genfile.c reconstruct), the L2
# FileReaders (zstd 1.5.0 / zlib 1.2.13), BKE gpencil, the app's rig (pg_arm_from_json).
# BLEND_IO_BLENDER=<blender 3.6.23 binary>: the real-Blender round trip (tools/blend_io_reference):
#   Blender makes ref.blend -> app opens it (compared exactly with Blender's dump) -> app saves app.blend ->
#   Blender opens app.blend -> its dump compared with the original. Without it: the self round trip only.
# BLEND_IO_OUT=<dir> keeps the files (CI uploads them).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "blend io"
ZS="$ROOT/third_party/zstd-1.5.0/lib"
ZL="$ROOT/third_party/zlib-1.2.13"
[[ -f "$ROOT/third_party/zstd-1.5.0/.pg_verified" && -f "$ZL/.pg_verified" ]] || {
  echo "ERROR: run tools/import_blender_gp.sh first (pinned zstd/zlib missing)" >&2; exit 1; }
LINC=(-I"$ZS" -I"$ZL" -funsigned-char -fno-strict-aliasing -ffp-contract=off -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)
X_SRC=(
  "$BL/makesdna/intern/dna_genfile.c" "$BL/blenlib/intern/endian_switch.c" "$BL/makesdna/intern/dna_defaults.c"
  "$B/release/datafiles/userdef/userdef_default.c" "$B/release/datafiles/userdef/userdef_default_theme.c"
  "$BL/blenlib/intern/filereader_gzip.c" "$BL/blenlib/intern/filereader_memory.c" "$BL/blenlib/intern/filereader_zstd.c"
  "$BL/blenlib/intern/fileops.c" "$BL/blenlib/intern/storage.c" "$BL/blenlib/intern/BLI_mmap.c" "$BL/blenlib/intern/filereader_file.c"
  "$ZS/common/debug.c" "$ZS/common/entropy_common.c" "$ZS/common/error_private.c" "$ZS/common/fse_decompress.c"
  "$ZS/common/xxhash.c" "$ZS/common/zstd_common.c" "$ZS/decompress/huf_decompress.c" "$ZS/decompress/zstd_ddict.c"
  "$ZS/decompress/zstd_decompress.c" "$ZS/decompress/zstd_decompress_block.c"
  "$ZL/adler32.c" "$ZL/crc32.c" "$ZL/inffast.c" "$ZL/inflate.c" "$ZL/inftrees.c" "$ZL/zutil.c"
)
XOBJS=()
for s in "${X_SRC[@]}"; do o="$OUT/bio_$(basename "$s").o"; gcc "${CF[@]}" "${LINC[@]}" "${INC[@]}" -c "$s" -o "$o"; XOBJS+=("$o"); done
g++ "${XF[@]}" "${LINC[@]}" "${INC[@]}" "$ROOT/native/blender_gp/tests/test_blend_io.cc" "${XOBJS[@]}" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_blend_io" > "$OUT/link.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
# the reader / writer (included by the test) also under ASan/UBSan: a .blend is untrusted input
g++ "${XF[@]}" -g -fsanitize=address,undefined -fno-omit-frame-pointer "${LINC[@]}" "${INC[@]}" "$ROOT/native/blender_gp/tests/test_blend_io.cc" \
  "${XOBJS[@]}" "${OBJS[@]}" -fsanitize=address,undefined -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_blend_io_asan" > "$OUT/link_asan.txt" 2>&1 || {
  grep -E "error|undefined reference" "$OUT/link_asan.txt" | head -20; exit 1; }
SAN=(env ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1)
KEEP="${BLEND_IO_OUT:-$OUT/files}"; mkdir -p "$KEEP"; KEEP="$(cd "$KEEP" && pwd)"
[[ -n "${BLEND_IO_KEEP_BIN:-}" ]] && cp "$OUT/test_blend_io" "$BLEND_IO_KEEP_BIN"
timeout 120s "$OUT/test_blend_io" self "$KEEP"
"${SAN[@]}" timeout 300s "$OUT/test_blend_io_asan" self "$KEEP"
if [[ -n "${BLEND_IO_BLENDER:-}" ]]; then
  REF="$ROOT/tools/blend_io_reference/blender_blend_io_reference.py"
  run_blender() { "$BLEND_IO_BLENDER" -b --factory-startup --python-exit-code 1 --python "$REF" -- "$@" > "$KEEP/blender_$1.log" 2>&1 || {
    tail -30 "$KEEP/blender_$1.log"; exit 1; }; }
  run_blender make "$KEEP/ref.blend" "$KEEP/ref.json"
  timeout 120s "$OUT/test_blend_io" import "$KEEP/ref.blend" "$KEEP/ref.json" "$KEEP"
  "${SAN[@]}" timeout 300s "$OUT/test_blend_io_asan" open "$KEEP/ref.blend" "$KEEP/asan.project.json"
  # Blender's zstd-compressed save of the same file opens to the same document (FileReader zstd layer)
  "${SAN[@]}" timeout 300s "$OUT/test_blend_io_asan" open "$KEEP/ref_zstd.blend" "$KEEP/zstd.project.json"
  cmp "$KEEP/app.project.json" "$KEEP/zstd.project.json" && echo "BLEND-IO zstd: compressed file opens to the identical document"
  run_blender dump "$KEEP/app.blend" "$KEEP/app.json"
  timeout 120s "$OUT/test_blend_io" compare "$KEEP/ref.json" "$KEEP/app.json"
  echo "BLENDER-RUN blend io: Blender 3.6.23 made ref.blend, the app opened it and saved app.blend, Blender opened app.blend: all compared"
fi
