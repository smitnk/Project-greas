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

# L2: the .blend reader's zstd/gzip libraries at the exact versions and hashes Blender 3.6.23 pins
# (build_files/build_environment/cmake/versions.cmake: ZSTD_VERSION 1.5.0, ZLIB_VERSION 1.2.13).
fetch_pinned() { # $1 dir name, $2 url, $3 hash tool, $4 hash
  local dir="${ROOT}/third_party/$1" tgz="${ROOT}/third_party/$1.tar.gz"
  [[ -f "${dir}/.pg_verified" ]] && return 0
  rm -rf "${dir}" "${tgz}"
  curl -fsSL --retry 3 "$2" -o "${tgz}"
  echo "$4  ${tgz}" | "$3" -c -
  mkdir -p "${dir}"
  tar -xzf "${tgz}" -C "${dir}" --strip-components=1
  rm -f "${tgz}"
  touch "${dir}/.pg_verified"
}
fetch_pinned zstd-1.5.0 https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz \
  sha256sum 5194fbfa781fcf45b98c5e849651aa7b3b0a008c6b72d4a0db760f3002291e94
fetch_pinned zlib-1.2.13 https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz \
  md5sum 9b8aa094c4e5765dabf4da391f00d15c
echo "Imported zstd 1.5.0 and zlib 1.2.13"
