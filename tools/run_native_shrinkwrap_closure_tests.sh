#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT/tools/android_gp_source_manifest.txt"
BLENDER="$ROOT/third_party/blender/source/blender"
MAP="$ROOT/native/blender_gp/shrinkwrap/DEPENDENCY_MAP.md"

test -f "$MANIFEST"
test -f "$MAP"
test -d "$BLENDER"

required_manifest=(
  "CXX|third_party/blender/source/blender/blenkernel/intern/shrinkwrap.cc"
  "CXX|third_party/blender/source/blender/blenkernel/intern/bvhutils.cc"
  "C|third_party/blender/source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_shrinkwrap.c"
)

for entry in "${required_manifest[@]}"; do
  grep -Fxq "$entry" "$MANIFEST" || {
    echo "missing manifest root: $entry" >&2
    exit 1
  }
done

# First exact Blender 3.6.23 source boundaries for the D2 Shrinkwrap closure.
# This is a source-closure guard, not a completion claim.
required_sources=(
  "blenkernel/intern/shrinkwrap.cc"
  "blenkernel/intern/bvhutils.cc"
  "gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_shrinkwrap.c"
)
for rel in "${required_sources[@]}"; do
  test -f "$BLENDER/$rel" || {
    echo "pinned Blender source missing after import: $rel" >&2
    exit 1
  }
done

# D2 is evaluated-mesh parity. Reject accidental base-mesh-only wording.
if grep -Eiq 'base-mesh shortcut|base mesh shortcut|base-mesh-only' "$MAP"; then
  echo "D2 invariant violated: dependency map contains a base-mesh-only shortcut" >&2
  exit 1
fi

# Armature is explicitly outside the current L1 blocker scope.
if grep -Eiq '^C(XX)?\\|.*armature' "$MANIFEST"; then
  echo "Armature source entered the Shrinkwrap L1 closure unexpectedly" >&2
  exit 1
fi

# Duplicate source paths would make the closure non-deterministic.
awk -F'\\|' '
  /^[[:space:]]*(C|CXX)\\|/ {
    if (++seen[$2] > 1) {
      print "duplicate manifest source: " $2 > "/dev/stderr"
      bad=1
    }
  }
  END { exit bad }
' "$MANIFEST"

echo "Shrinkwrap D2 source-closure preflight: PASS"
echo "  pinned roots: shrinkwrap.cc, bvhutils.cc, MOD_gpencil_legacy_shrinkwrap.c"
echo "  evaluated-mesh requirement: retained"
echo "  Armature exclusion: retained"
