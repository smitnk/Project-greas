#!/usr/bin/env bash
# Run ./gradlew (from android/) up to 3 times: CI once failed on a transient Maven HTTP 429.
for i in 1 2 3; do
  ./gradlew "$@" && exit 0
  [ "$i" -lt 3 ] && { echo "gradlew failed (attempt $i/3), retrying" >&2; sleep $((i * 20)); }
done
exit 1
