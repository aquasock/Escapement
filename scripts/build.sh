#!/usr/bin/env bash
# Build Escapement's core-0 firmware: ESP-IDF on core 0, for a T-Watch Ultra.
#
# Everything heavy lives under build/ and is gitignored. This script is the
# reproducible path; each step is idempotent so re-running it after a failure
# picks up where it stopped.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

# Pinned to what upstream release 0.9.1 was built from. Changing any of these
# means the kernel/rootfs in images/ may no longer match.
PROJECT_REV=0.9.1
ESP_HOSTED_REV=e2f19edab6c5bec988346906412cb8e7443661db
IDF_REV=24394052fcc462fe27935b4cf15ea1f3e98549c1

# Absolute on purpose: the configure step cds into the app directory before
# sourcing export.sh, and a relative path stops resolving at that point.
B=$here/build
PROJ=$B/Linux-on-esp32-S3
HOSTED=$B/esp-hosted
NA=$HOSTED/esp_hosted_ng/esp/esp_driver/network_adapter
IDF=$HOSTED/esp_hosted_ng/esp/esp_driver/esp-idf
export IDF_TOOLS_PATH=$B/idf-tools

say() { printf '\n=== %s\n' "$*"; }
die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

for t in git cmake ninja python3; do
    command -v $t >/dev/null || die "missing $t"
done
mkdir -p "$B" "$IDF_TOOLS_PATH"

# ---- sources ---------------------------------------------------------------
say "sources"
if [ ! -d "$PROJ" ]; then
    git clone --depth 1 --branch "$PROJECT_REV" \
        https://github.com/paulneja/Linux-on-esp32-S3.git "$PROJ"
fi

if [ ! -d "$HOSTED" ]; then
    git clone https://github.com/jcmvbkbc/esp-hosted.git "$HOSTED"
    git -C "$HOSTED" checkout --detach "$ESP_HOSTED_REV"
fi
[ -d "$HOSTED" ] || die "esp-hosted missing"
# An exact rev-parse match is wrong once local commits exist on top: the pin
# is the base, not the tip. What has to hold is that the pinned revision is an
# ancestor, so the tree still contains exactly what 0.9.1 was built from.
if ! git -C "$HOSTED" merge-base --is-ancestor "$ESP_HOSTED_REV" HEAD 2>/dev/null; then
    git -C "$HOSTED" cat-file -e "$ESP_HOSTED_REV^{commit}" 2>/dev/null \
        || git -C "$HOSTED" fetch --all --quiet
    git -C "$HOSTED" checkout --detach "$ESP_HOSTED_REV"
fi
git -C "$HOSTED" merge-base --is-ancestor "$ESP_HOSTED_REV" HEAD \
    || die "esp-hosted does not descend from $ESP_HOSTED_REV"

say "submodule: esp-idf fork"
# Full clone: --depth here produced a tree whose submodules would not
# initialise, and the failure surfaced as a cmake error, not a git one.
if [ ! -d "$IDF/.git" ] || [ ! -f "$IDF/CMakeLists.txt" ]; then
    git -C "$HOSTED" submodule update --init "$IDF_REV" -- \
        esp_hosted_ng/esp/esp_driver/esp-idf 2>/dev/null || true
    if [ ! -f "$IDF/CMakeLists.txt" ]; then
        git -C "$HOSTED" submodule update --init esp_hosted_ng/esp/esp_driver/esp-idf
    fi
fi
[ "$(git -C "$IDF" rev-parse HEAD)" = "$IDF_REV" ] || die "esp-idf is not at $IDF_REV"

# ---- patches ---------------------------------------------------------------
say "patches"

# Apply a patch only if it is not already in place. Checking the forward and
# reverse application is what makes re-runs safe: a plain "already applied"
# grep would break the moment the patch or its comment changed.
apply_once() {
    local dir=$1 patch=$2
    if git -C "$dir" apply --check "$patch" 2>/dev/null; then
        git -C "$dir" apply "$patch"
        echo "applied $(basename "$patch")"
    elif git -C "$dir" apply --check --reverse "$patch" 2>/dev/null; then
        echo "already applied: $(basename "$patch")"
    else
        die "$(basename "$patch") neither applies nor is applied -- tree is dirty?"
    fi
}

# Upstream's own firmware patch (BLE provisioning, flash access, the Linux
# loader), then ours on top.
apply_once "$HOSTED/esp_hosted_ng" "$PROJ/patches/02-firmware-network-adapter.patch"
cp -n "$PROJ/new-files/esp-hosted/network_adapter/partition_table.esp32s3.16m8r" \
      "$PROJ/new-files/esp-hosted/network_adapter/sdkconfig.defaults.esp32s3.16m8r" \
      "$NA/" 2>/dev/null || true
apply_once "$HOSTED/esp_hosted_ng" "$here/patches/0001-watch-quad-psram.patch"
apply_once "$HOSTED/esp_hosted_ng" "$here/patches/0002-cmdline-ttyGS3-console.patch"

# esp_driver/CMakeLists.txt runs `git reset --hard` on every cmake call, so a
# local commit is the only thing that survives configuration.
say "commit locally (survives git reset --hard)"
git -C "$HOSTED/esp_hosted_ng" add -A
# Commit only when something is actually staged: a clean tree makes
# `git commit` exit non-zero, and under `set -e` that aborted a good build.
# The submodule's modified content does not count as a change worth committing.
if ! git -C "$HOSTED/esp_hosted_ng" diff --cached --quiet; then
    env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL \
        -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL \
        git -C "$HOSTED/esp_hosted_ng" \
        -c user.email="local@escapement" -c user.name="escapement" \
        commit -q -m "escapement: applied (local-only, never push)"
    echo "committed"
else
    echo "nothing to commit"
fi

# The patched wifi blobs carry esp_wifi_connect_internal and friends. IDF's
# stock ones do not, and the resulting link error names wifi symbols while the
# real cause is this missing copy.
say "wifi libraries"
cp -f "$HOSTED/esp_hosted_ng/esp/esp_driver/lib/esp32s3/"*.a \
      "$IDF/components/esp_wifi/lib/esp32s3/"

# ---- toolchain -------------------------------------------------------------
say "toolchain (idf fork, into $IDF_TOOLS_PATH)"
if [ ! -x "$IDF_TOOLS_PATH/tools/xtensa-esp32s3-elf"/*/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-gcc ]; then
    ( cd "$IDF" && python3 tools/idf_tools.py install )
fi
if [ ! -d "$IDF_TOOLS_PATH/python_env" ]; then
    ( cd "$IDF" && python3 tools/idf_tools.py install-python-env )
fi

# ---- build -----------------------------------------------------------------
say "configure"
cd "$NA"
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null 2>&1 || die "export.sh failed"

idf.py set-target esp32s3 >/dev/null
cp sdkconfig.defaults.esp32s3.16m8r sdkconfig

say "build"
idf.py build

# ---- the check that actually matters --------------------------------------
say "vectors address"
NM="$IDF_TOOLS_PATH/tools/xtensa-esp32s3-elf/"*/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm
got=$($NM build/network_adapter.elf | awk '/space_for_vectors/ {print $1}' | tr 'a-f' 'A-F')
want=$(grep '^CONFIG_VECTORS_ADDR=' \
       "$PROJ/new-files/board/espressif/esp32s3/devkit_c1_16m_linux.config" \
       | cut -d= -f2 | sed 's/^0x//' | tr 'a-f' 'A-F')
printf 'space_for_vectors = 0x%s, kernel expects 0x%s\n' "$got" "$want"
if [ "$got" != "$want" ]; then
    die "vectors address moved. The kernel is linked against 0x$want.
Any change to internal RAM use moves this symbol. Either raise the buffer's
alignment in linux_boot.c until it lands back on 0x$want, or rebuild the
kernel with a matching CONFIG_VECTORS_ADDR. Do not flash this firmware."
fi

say "ok"
ls -l build/network_adapter.bin
printf '\nflash the app only:\n  ./scripts/flash.sh --app\n'
