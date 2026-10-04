#!/usr/bin/env bash
# Host fuzzing of the native edit-command routing under ASan + UBSan (bug-hunt environment).
# The real bridge, backend and pinned Blender 3.6.23 legacy GP code are compiled with the
# sanitizers; any memory error or undefined behaviour aborts with a report, and the fuzzer fails on
# a corrupted document. usage: tools/run_native_fuzz.sh [iterations] [seeds...]
set -euo pipefail
# clang: -fsanitize-address-globals-dead-stripping lets --gc-sections drop the unused Blender code
# (with gcc the ASan global metadata keeps every function alive and the closure does not link).
export PG_HOST_CC="${PG_HOST_CC:-clang}" PG_HOST_CXX="${PG_HOST_CXX:-clang++}"
export PG_SANITIZE_FLAGS="${PG_SANITIZE_FLAGS:--fsanitize=address,undefined -fno-sanitize=vptr,function -fno-sanitize-recover=undefined -fsanitize-address-globals-dead-stripping -fno-omit-frame-pointer -g -O1}"
# Upstream Blender 3.6.23 computes &gps->dvert[i] on a null dvert it never dereferences
# (BKE_gpencil_stroke_uniform_subdivide): pointer-overflow is off for upstream sources only.
export PG_SANITIZE_UPSTREAM_EXCLUDE="${PG_SANITIZE_UPSTREAM_EXCLUDE-pointer-overflow}"
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "native fuzz"
ITER="${1:-20000}"; shift || true
SEEDS=("$@"); [[ ${#SEEDS[@]} -gt 0 ]] || SEEDS=(20240501 7 1234)
BSRC_CXX=(
  "$ROOT/native/blender_gp/project_grease_gp_bridge.cpp"
  "$ROOT/native/blender_gp/project_grease_gp_backend.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_fill.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_primitive.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_eraser.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_sculpt.cpp"
)
BSRC_C=(
  "$ROOT/native/blender_gp/project_grease_legacy_sbuffer.c"
  "$ROOT/native/blender_gp/project_grease_blender_primitive.c"
  "$ROOT/native/blender_gp/project_grease_blender_select.c"
  "$ROOT/native/blender_gp/project_grease_blender_eraser.c"
  "$ROOT/native/blender_gp/project_grease_document_state.c"
)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/f_$(basename "$s").o"; "$HCXX" "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/f_$(basename "$s").o"; "$HCC" "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
"$HCXX" "${XF[@]}" -Wall -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/test_fuzz_regressions.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -fuse-ld=lld -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_fuzz_regressions"
"$HCXX" "${XF[@]}" -Wall -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/fuzz_edit_commands.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -fuse-ld=lld -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/fuzz_edit_commands"
# One allocation over 512 MB or a process over 3 GB is a bug report (with the stack), not an OOM kill.
if [[ -n "${PG_FUZZ_BIN:-}" ]]; then cp "$OUT/fuzz_edit_commands" "$PG_FUZZ_BIN"; echo "fuzzer binary: $PG_FUZZ_BIN"; exit 0; fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1:halt_on_error=1:max_allocation_size_mb=512:hard_rss_limit_mb=3072:allocator_may_return_null=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
# The regressions for the bugs the fuzzer found, with LeakSanitizer on.
ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:halt_on_error=1" "$OUT/test_fuzz_regressions"
for seed in "${SEEDS[@]}"; do
  "$OUT/fuzz_edit_commands" "$ITER" "$seed"
done
