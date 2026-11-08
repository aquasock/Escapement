#!/usr/bin/env bash
# Build the XIP kernel for the T-Watch Ultra.
#
# This reproduces the release kernel, which is Linux 7.2.4 plus THREE separate
# patch stacks:
#
#   1. the port's 62 board patches          (new-files/board/.../patches/linux)
#   2. Escapement's own patches             (patches/kernel)
#   3. the fork backend                     (experiments/mmu-poc/fork)
#
# Step 3 is the one that is easy to miss, and missing it produces a kernel that
# builds, boots, and then breaks at login. That was learned the hard way:
#
#   Caught unhandled exception in 'bash' (pc = 0x3df30388)
#   bash could not fork with 4432 KiB free; switching this session to dash.
#
# This port has no working fork() without it -- NOMMU has no MMU to give a
# child its own address space, so the banking patches ARE the fork
# implementation. The tell is the version string: the release reports
# "7.2.4-forkbank" and a kernel built without the stack reports a bare git
# describe.
#
# Usage: ./scripts/build-kernel.sh [--clean]
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

B=build
K=$B/kernel/linux-7.2.4
PROJ=$B/Linux-on-esp32-S3
FORK=$PROJ/experiments/mmu-poc/fork
TC=$B/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic
KV=7.2.4
KVER_SHA=01710ee01737dac492f1bae52becd057e08d20d11589089aa06accff415c28dd

say() { printf '\n=== %s\n' "$*"; }
die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -x "$TC/bin/xtensa-esp32s3-linux-uclibcfdpic-gcc" ] \
    || die "no toolchain. Run ./scripts/build-toolchain.sh first (docs/toolchain.md)."
export XTENSA_GNU_CONFIG=$B/toolchain/xtensa-dynconfig/esp32s3.so
export PATH=$here/$TC/bin:$PATH
CROSS=xtensa-esp32s3-linux-uclibcfdpic-

# ---- source ----------------------------------------------------------------
say "source"
mkdir -p $B/kernel
if [ ! -f $B/kernel/linux-$KV.tar.xz ]; then
    # kernel.org, not a GNU mirror: ftp.gnu.org is unreachable from some hosts.
    curl -sSL -o $B/kernel/linux-$KV.tar.xz \
        "https://mirrors.kernel.org/pub/linux/kernel/v${KV%%.*}.x/linux-$KV.tar.xz"
fi
echo "$KVER_SHA  $B/kernel/linux-$KV.tar.xz" | sha256sum -c - \
    || die "kernel tarball hash mismatch"

if [ "${1:-}" = "--clean" ] || [ ! -d "$K" ]; then
    rm -rf "$K"
    tar -C $B/kernel -xf $B/kernel/linux-$KV.tar.xz
    cd "$K" && git init -q . && git add -A >/dev/null && \
        git -c user.email=x@y -c user.name=x commit -qm pristine
fi
cd "$here"

# ---- stack 1: the port's board patches -------------------------------------
say "board patches (62)"
cd "$K"
for f in "$PROJ"/new-files/board/espressif/esp32s3/patches/linux/*.patch; do
    git apply --whitespace=nowarn "$f" 2>/dev/null || true
done
cd "$here"

# ---- stack 2: ours ---------------------------------------------------------
say "escapement patches"
cd "$K"
for f in "$here"/patches/kernel/0001-*.patch; do
    git apply --whitespace=nowarn "$f" 2>/dev/null || true
done
cd "$here"
# The config patch edits the *seed config*, not the kernel tree -- buildroot
# copies it in. Applying it here to the tree is a no-op by design.
(cd "$PROJ" && git apply --whitespace=nowarn "$here/patches/kernel/0002-"*.patch 2>/dev/null || true)

# ---- stack 3: the fork backend ---------------------------------------------
say "fork backend"
cd "$K"
# Order matters and is the project's own: base, scheduler, reclaim, quiet-trace,
# then the exchange patches that FORK_SWAP_BANKS=1 builds in.
for p in kernel.patch real/scheduler.patch reclaim.patch quiet-trace.patch \
         swap-banks.patch switch-latency.patch switch-irqs-on.patch \
         mmu-swap.patch bank-budget.patch; do
    if patch --force --dry-run -R -p1 < "$FORK/$p" >/dev/null 2>&1; then
        echo "  already applied: $p"
    else
        patch --batch --forward -p1 --no-backup-if-mismatch < "$FORK/$p" >/dev/null \
            || die "$p did not apply"
        echo "  applied: $p"
    fi
done

# Without these two the backend is compiled out and fork() silently falls back
# to something that cannot work on NOMMU.
./scripts/config --enable XTENSA_NOMMU_FORK --set-str LOCALVERSION '-forkbank'
# RTC_NVMEM defaults to RTC_CLASS and drags in the whole NVMEM framework. The
# PCF85063A uses none of it and the board config ships NVMEM off.
./scripts/config --disable RTC_NVMEM --disable NVMEM
cd "$here"

# ---- build -----------------------------------------------------------------
say "build"
cd "$K"
cp "$PROJ/new-files/board/espressif/esp32s3/devkit_c1_16m_linux.config" .config
./scripts/config --enable XTENSA_NOMMU_FORK --set-str LOCALVERSION '-forkbank'
./scripts/config --disable RTC_NVMEM --disable NVMEM
make ARCH=xtensa CROSS_COMPILE=$CROSS olddefconfig >/dev/null

# CONFIG_EXTRA_FIRMWARE builds regulatory.db in; buildroot normally supplies it.
if [ ! -f firmware/regulatory.db ]; then
    mkdir -p firmware
    tmp=$(mktemp -d)
    curl -sSL -o "$tmp/wr.tar.xz" \
        https://mirrors.kernel.org/pub/software/network/wireless-regdb/wireless-regdb-2026.09.03.tar.xz
    tar -C "$tmp" -xf "$tmp/wr.tar.xz"
    cp "$tmp"/wireless-regdb-*/regulatory.db "$tmp"/wireless-regdb-*/regulatory.db.p7s firmware/
    rm -rf "$tmp"
fi

make ARCH=xtensa CROSS_COMPILE=$CROSS -j"$(nproc)" xipImage

# ---- checks that matter ----------------------------------------------------
say "checks"
python3 "$FORK/check-kernel-config.py" \
    "$PROJ/new-files/board/espressif/esp32s3/devkit_c1_16m_linux.config" .config \
    || die "config drifted from the board seed"
grep -q '^CONFIG_XTENSA_NOMMU_FORK=y' .config || die "fork backend not enabled"
grep -q '^CONFIG_LOCALVERSION="-forkbank"' .config || die "LOCALVERSION not set"
grep -q '^CONFIG_VECTORS_ADDR=0x40380000' .config || die "CONFIG_VECTORS_ADDR moved"
grep -q '^CONFIG_RTC_DRV_PCF85063=y' .config || die "RTC driver missing"

size=$(stat -c %s arch/xtensa/boot/xipImage)
[ "$size" -le $((0x400000)) ] || die "xipImage $size exceeds the 4 MiB linux partition"
strings arch/xtensa/boot/xipImage | grep -q "Linux version $KV-forkbank" \
    || die "kernel is not the forkbank build -- the fork backend did not apply"

printf '\nxipImage: %s bytes (limit 4194304, %s free)\n' "$size" $((0x400000-size))
strings arch/xtensa/boot/xipImage | grep -m1 -o "Linux version [^ ]*"
printf '\nflash:\n  esptool.py ... write_flash 0x140000 %s/arch/xtensa/boot/xipImage\n' "$K"
