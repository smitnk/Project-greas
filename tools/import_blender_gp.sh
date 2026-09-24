#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRD_PARTY="${ROOT}/third_party/blender"
REPO="https://github.com/blender/blender.git"
COMMIT="e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca"

mkdir -p "${ROOT}/third_party"
if [[ -d "${THIRD_PARTY}/.git" ]]; then
  git -C "${THIRD_PARTY}" fetch --depth 1 origin "${COMMIT}"
  git -C "${THIRD_PARTY}" checkout --detach "${COMMIT}"
else
  rm -rf "${THIRD_PARTY}"
  git clone --filter=blob:none --no-checkout "${REPO}" "${THIRD_PARTY}"
  git -C "${THIRD_PARTY}" fetch --depth 1 origin "${COMMIT}"
  git -C "${THIRD_PARTY}" checkout --detach "${COMMIT}"
fi

echo "Imported upstream Blender commit ${COMMIT}"
