#!/usr/bin/env bash
# L2 host test: Blender 3.6.23 FileReaders (the .blend reader's file / zstd / gzip layer) on the
# pinned zstd 1.5.0 and zlib 1.2.13 from tools/import_blender_gp.sh, plain and under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "FileReader test"
ZS="$ROOT/third_party/zstd-1.5.0/lib"
ZL="$ROOT/third_party/zlib-1.2.13"
[[ -f "$ROOT/third_party/zstd-1.5.0/.pg_verified" && -f "$ZL/.pg_verified" ]] || {
  echo "ERROR: run tools/import_blender_gp.sh first (pinned zstd/zlib missing)" >&2; exit 1; }
LINC=(-I"$ZS" -I"$ZL" -funsigned-char -fno-strict-aliasing -ffp-contract=off -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE) # platform_unix.cmake:575
# Reader closure: exactly the Android manifest entries (decompression only).
L2_SRC=(
  "$BL/blenlib/intern/filereader_file.c" "$BL/blenlib/intern/filereader_gzip.c"
  "$BL/blenlib/intern/filereader_memory.c" "$BL/blenlib/intern/filereader_zstd.c"
  "$BL/blenlib/intern/BLI_mmap.c" "$BL/blenlib/intern/storage.c" "$BL/blenlib/intern/fileops.c"
  "$ZS/common/debug.c" "$ZS/common/entropy_common.c" "$ZS/common/error_private.c"
  "$ZS/common/fse_decompress.c" "$ZS/common/xxhash.c" "$ZS/common/zstd_common.c"
  "$ZS/decompress/huf_decompress.c" "$ZS/decompress/zstd_ddict.c"
  "$ZS/decompress/zstd_decompress.c" "$ZS/decompress/zstd_decompress_block.c"
  "$ZL/adler32.c" "$ZL/crc32.c" "$ZL/inffast.c" "$ZL/inflate.c" "$ZL/inftrees.c" "$ZL/zutil.c"
)
# Test-only: the zstd compressor used to write Blender-format fixtures (not in the Android closure).
W_SRC=("$ZS"/compress/*.c "$ZS/common/pool.c" "$ZS/common/threading.c")
build() { # $1 suffix, rest extra flags
  local sfx="$1"; shift; local objs=()
  for s in "${L2_SRC[@]}" "${W_SRC[@]}"; do
    o="$OUT/l2_$(basename "$s")$sfx.o"; gcc "${CF[@]}" "$@" "${LINC[@]}" "${INC[@]}" -c "$s" -o "$o"; objs+=("$o")
  done
  gcc -std=gnu11 -Wall "$@" "${LINC[@]}" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/test_filereader.c" -o "$OUT/test_filereader$sfx.o"
  g++ "$@" "$OUT/test_filereader$sfx.o" "${objs[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_filereader$sfx"
}
build ""
timeout 120s "$OUT/test_filereader" "$OUT"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 120s "$OUT/test_filereader_asan" "$OUT"
