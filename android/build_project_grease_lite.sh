#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/android/apply_project_grease_wiring.sh"

UPSTREAM="$ROOT/vendor/blender_android_upstream"
CONFIG="${1:-lite}"

if [ ! -f "$UPSTREAM/build_files/android/build.py" ]; then
  echo "Blender Android build.py not found." >&2
  exit 1
fi

cd "$UPSTREAM"
python3 build_files/android/build.py "$CONFIG"
