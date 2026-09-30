#!/usr/bin/env bash
#
# extract_grease_pencil.sh
#
# Pulls ONLY the Grease Pencil source out of Blender's official repository at a
# release tag (default: v3.6.23) and writes it as plain .txt files.
#
#   bash extract_grease_pencil.sh
#
# Environment variables (all optional):
#   TAG=v3.6.23         release tag to extract
#   OUT=<dir>           output folder (must not already contain files)
#   BLENDER_DIR=<path>  use an existing Blender checkout instead of downloading
#   REPO=<git url>      use this remote instead of the built-in mirrors
#
# Needs bash + git (>= 2.25). Downloads tens of MB, not the multi-GB repo.

set -euo pipefail

TAG="${TAG:-v3.6.23}"
PINNED="e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca"
OUT="${OUT:-./blender_${TAG#v}_grease_pencil_txt}"

if [ -n "${REPO:-}" ]; then
  MIRRORS=("$REPO")
else
  MIRRORS=("https://github.com/blender/blender.git" "https://projects.blender.org/blender/blender.git")
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
LIST="$WORK/files.lst"

if [ -d "$OUT" ] && [ -n "$(ls -A "$OUT" 2>/dev/null)" ]; then
  echo "ERROR: $OUT already exists and is not empty (remove it or set OUT=...)."
  exit 1
fi

select_gp() {
  grep -E '^(source|release/scripts)/' |
    grep -Ei 'gpencil|grease_?pencil|shader_?fx' |
    grep -Ei '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl|glsl|osl|py|cmake|txt)$' |
    LC_ALL=C sort
}

if [ -n "${BLENDER_DIR:-}" ]; then
  SRC="$BLENDER_DIR"
  (cd "$SRC" && find source release/scripts -type f 2>/dev/null) | select_gp > "$LIST" || true
else
  SRC="$WORK/blender"
  ok=0
  for url in "${MIRRORS[@]}"; do
    echo ">> Fetching $TAG from $url (tree only, no history) ..."
    rm -rf "$SRC"
    if git clone --quiet --depth 1 --branch "$TAG" --filter=blob:none --no-checkout "$url" "$SRC"; then
      ok=1
      break
    fi
  done

  if [ "$ok" != 1 ]; then
    echo "ERROR: could not fetch tag $TAG."
    exit 1
  fi

  git -C "$SRC" -c core.quotepath=false ls-tree -r --name-only HEAD | select_gp > "$LIST" || true
  if [ -s "$LIST" ]; then
    echo ">> Downloading only the $(wc -l < "$LIST" | tr -d ' ') matching files ..."
    git -C "$SRC" config core.sparseCheckout true
    mkdir -p "$SRC/.git/info"
    sed 's|^|/|' "$LIST" > "$SRC/.git/info/sparse-checkout"
    git -C "$SRC" read-tree -mu HEAD
  fi
fi

[ -s "$LIST" ] || { echo "ERROR: no Grease Pencil files found."; exit 1; }

COMMIT="$(git -C "$SRC" rev-parse HEAD 2>/dev/null || echo unknown)"
if [ "$COMMIT" != "unknown" ] && [ "$COMMIT" != "$PINNED" ]; then
  echo "WARNING: source is at commit $COMMIT, not the pinned $PINNED"
fi

mkdir -p "$OUT"
INDEX="$OUT/00_INDEX.txt"

group_of() {
  case "$1" in
    source/blender/blenkernel/*)                                    echo 01_kernel ;;
    source/blender/makesdna/*|source/blender/makesrna/*)            echo 02_dna_rna ;;
    source/blender/editors/*)                                       echo 03_editors ;;
    source/blender/draw/*|source/blender/gpu/*)                     echo 04_draw_engine_and_gpu_shaders ;;
    source/blender/gpencil_modifiers/*|source/blender/shader_fx/*)  echo 05_modifiers_lineart_shaderfx ;;
    source/blender/io/*)                                            echo 06_io_export ;;
    release/scripts/*)                                              echo 07_python_ui ;;
    *)                                                              echo 08_other ;;
  esac
}

{
  echo "Blender $TAG - Grease Pencil source extract"
  echo "Commit:    $COMMIT"
  echo "License:   unmodified Blender sources (GPL-2.0-or-later, see each file header)"
  echo "Selection: text files under source/ and release/scripts/ whose path mentions"
  echo "           gpencil, grease_pencil or shader_fx"
  echo
  printf '%-34s %8s  %s\n' GROUP LINES PATH
} > "$INDEX"

while IFS= read -r f; do
  g="$(group_of "$f")"
  n="$(wc -l < "$SRC/$f" | tr -d ' ')"
  {
    echo
    echo "################################################################################"
    echo "# FILE: $f"
    echo "# Blender $TAG  |  $n lines"
    echo "################################################################################"
    cat "$SRC/$f"
  } >> "$OUT/$g.txt"
  printf '%-34s %8s  %s\n' "$g" "$n" "$f" >> "$WORK/index.rows"
  case "$f" in *.txt) dest="$f" ;; *) dest="$f.txt" ;; esac
  mkdir -p "$OUT/files/$(dirname "$f")"
  cp "$SRC/$f" "$OUT/files/$dest"
done < "$LIST"

LC_ALL=C sort -s -k1,1 "$WORK/index.rows" >> "$INDEX"

echo
echo "Done - Blender $TAG ($COMMIT): $(wc -l < "$LIST" | tr -d ' ') files"
for g in "$OUT"/[0-9][0-9]_*.txt; do
  printf '  %-42s %6s KB\n' "$(basename "$g")" "$(( $(wc -c < "$g") / 1024 ))"
done
echo "Output: $OUT   (one .txt per source file in $OUT/files/)"
