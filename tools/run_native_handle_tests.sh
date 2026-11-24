#!/usr/bin/env bash
# Host test for the generation-checked JNI handle registry (header only), plain and under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
ASAN_OPTIONS=detect_leaks=1 pg_test test_handle_registry 1 g++ -std=c++17 -Wall -I"$ROOT/native/blender_gp" "$ROOT/native/blender_gp/tests/test_handle_registry.cc"
