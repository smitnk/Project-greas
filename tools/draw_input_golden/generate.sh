#!/usr/bin/env bash
# Regenerates native/blender_gp/tests/draw_input_golden.txt from the Kotlin reference engine.
# Needs a Kotlin compiler: kotlinc on PATH, or a Gradle distribution (GRADLE_LIB=<gradle>/lib).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/tools/draw_input_golden/LegacyGpBrushStrokeEngine.kt" "$ROOT/tools/draw_input_golden/GenerateGolden.kt")
if command -v kotlinc >/dev/null; then
  kotlinc "${SRC[@]}" -include-runtime -d "$OUT/g.jar"
  java -cp "$OUT/g.jar" com.smitnk.projectgrease.editor.GenerateGoldenKt "$ROOT/native/blender_gp/tests/draw_input_golden.txt"
else
  LIB="${GRADLE_LIB:-$(dirname "$(readlink -f "$(command -v gradle)")")/../lib}"
  CP=$(ls "$LIB"/*.jar | tr '\n' ':')
  STD=$(ls "$LIB"/kotlin-stdlib-[0-9]*.jar | head -1)
  java -cp "$CP" org.jetbrains.kotlin.cli.jvm.K2JVMCompiler -no-stdlib -cp "$STD" -d "$OUT/c" "${SRC[@]}"
  java -cp "$OUT/c:$STD" com.smitnk.projectgrease.editor.GenerateGoldenKt "$ROOT/native/blender_gp/tests/draw_input_golden.txt"
fi
echo "wrote native/blender_gp/tests/draw_input_golden.txt"
