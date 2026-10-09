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

# D2 is evaluated-mesh parity. Reject only affirmative base-mesh-only
# implementation claims; exclusion/documentation wording is valid.
if grep -Eiq '(^|[[:space:][:punct:]])(uses?|using|implemented[[:space:]]+with|falls?[[:space:]]+back[[:space:]]+to|only[[:space:]]+uses?)[[:space:]]+(a[[:space:]]+)?base[-[:space:]]mesh([[:space:]-]+only)?([[:space:][:punct:]]|$)' "$MAP"; then
  echo "D2 invariant violated: dependency map contains an affirmative base-mesh-only implementation claim" >&2
  exit 1
fi

# Armature is explicitly outside the current L1 blocker scope.
if grep -Eiq '^C(XX)?[|].*armature' "$MANIFEST"; then
  echo "Armature source entered the Shrinkwrap L1 closure unexpectedly" >&2
  exit 1
fi

# Duplicate source paths would make the closure non-deterministic.
awk -F'[|]' '
  /^[[:space:]]*(C|CXX)[|]/ {
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
