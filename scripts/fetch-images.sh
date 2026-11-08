#!/usr/bin/env bash
# Fetch the upstream release image Escapement is built against.
#
# The image is 16 MiB and is not tracked: it is someone else's release, it
# changes with theirs, and a blob that size does not belong in a git history.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"
mkdir -p images

REL=${1:-0.9.1}
BASE="https://github.com/paulneja/Linux-on-esp32-S3/releases/download/$REL"

command -v gh >/dev/null || { echo "needs gh (or curl, edit this script)" >&2; exit 1; }

echo "=== $REL"
gh release download "$REL" -R paulneja/Linux-on-esp32-S3 \
    --pattern 'linux-esp32s3-native-full.bin' --pattern 'SHA256SUMS' \
    --dir images --clobber

cd images
grep 'linux-esp32s3-native-full.bin' SHA256SUMS > .check
sha256sum -c .check
rm -f .check

size=$(stat -c %s linux-esp32s3-native-full.bin)
[ "$size" = 16777216 ] || { echo "wrong size: $size" >&2; exit 1; }
echo "ok"
