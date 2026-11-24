#!/usr/bin/env bash
# Shared helper of the tools/run_native_*.sh host tests: sets ROOT, B, BL and OUT (temp dir, removed on exit).
# usage: source tools/native_test_lib.sh
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/third_party/blender"
BL="$B/source/blender"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

# pg_require_file <test name> <file>: a missing pinned source fails; PG_ALLOW_SKIP=1 (local only, never CI) skips.
pg_require_file() {
  [[ -f "$2" ]] && return 0
  if [[ "${PG_ALLOW_SKIP:-0}" == "1" && "${PG_REQUIRE_BLENDER_SOURCE:-0}" != "1" ]]; then
    echo "SKIP $1 (run tools/import_blender_gp.sh first)"; exit 0
  fi
  echo "ERROR: Pinned Blender source missing: $2 (set PG_ALLOW_SKIP=1 to skip locally)" >&2; exit 1
}

# pg_test <name> <asan: 0|1> <compiler> <flags and sources...>: builds and runs <name>, then with
# ASAN=1 also an ASan/UBSan build. ASAN_OPTIONS defaults to detect_leaks=0; callers may override it.
pg_test() {
  local name="$1" asan="$2" cc="$3"; shift 3
  "$cc" "$@" -lm -o "$OUT/$name"
  "$OUT/$name"
  [[ "$asan" == 1 ]] || return 0
  "$cc" -g -fsanitize=address,undefined -fno-omit-frame-pointer "$@" -lm -o "$OUT/${name}_asan"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$OUT/${name}_asan"
}
