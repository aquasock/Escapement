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

# Absolute: several steps cd into the kernel tree and then refer to these.
B=$here/build
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
export PATH=$TC/bin:$PATH
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
# Every patch under patches/kernel, in filename order, including one level of
# subdirectory.
#
# Two separate ways this has silently lost patches:
#
#   1. The glob used to be 0001-*, which skipped later patches -- including the
#      one registering the CO5300 driver, so a clean build produced a kernel
#      with no display driver and reported success.
#   2. It was then *.patch, which does not recurse. Everything in
#      patches/kernel/axp2101/ was skipped, so a rebuild lost the AXP2101 MFD,
#      regulator and battery drivers entirely: the PMU stopped appearing in
#      /sys/class/power_supply and the only trace of it was the DTB's
#      compatible strings, which nothing bound to.
#
# Patch files are not sorted for us by the shell, so sort them explicitly, and
# report a failure instead of swallowing it. A patch that does not apply is a
# statement about the tree, not noise.
for f in $(ls -1 "$here"/patches/kernel/*.patch \
                  "$here"/patches/kernel/*/*.patch 2>/dev/null | sort); do
    [ -e "$f" ] || continue

    # A patch aimed outside the kernel tree is the port's to apply, not ours:
    # 0002-config-* edits new-files/.../devkit_c1_16m_linux.config, and
    # buildroot copies that in. Applying it here was always a no-op, so it is
    # skipped rather than tolerated, and the strict failure below then means
    # what it says for everything that does target the tree.
    case "$(grep -m1 '^--- a/' "$f" 2>/dev/null)" in
    *new-files/*)
        echo "  skipped $(basename "$f") (port-side file, applied by the port)"
        continue ;;
    esac

    if git apply --whitespace=nowarn "$f" 2>/dev/null; then
        echo "  applied $(basename "$f")"
    elif git apply --reverse --check "$f" 2>/dev/null; then
        echo "  already applied: $(basename "$f")"
    else
        # Die, do not warn. A patch that does not apply used to print one line
        # and the build carried on without it -- which is how the entire
        # AXP2101 series and the display registration once vanished from a
        # kernel that built and booted (TOL-003).
        die "patch did not apply: $(basename "$f")"
    fi
done
cd "$here"
# ---- ours: the drivers that are still under active change -------------------
#
# These three files are edited in place faster than a patch series can keep up
# with, so they are kept as whole-file copies and installed after the patch
# stacks. Everything else in patches/kernel is a real patch.
say "display driver sources"
# The AXP2101 additions that live on top of the pending series (capacity,
# voltage, the power-key cell) are whole files for the same reason: two of them
# were carried as patches whose hunk line counts did not match their bodies, and
# `git apply` rejected them silently because the loop swallowed the error. The
# result was a kernel with the DTB describing a PMU that no driver bound to.
cp "$here"/patches/kernel/axp2101/final/axp20x.c \
   "$K/drivers/mfd/axp20x.c"
cp "$here"/patches/kernel/axp2101/final/axp20x_battery.c \
   "$K/drivers/power/supply/axp20x_battery.c"
cp "$here"/patches/kernel/axp2101/final/axp20x.h \
   "$K/include/linux/mfd/axp20x.h"

cp "$here"/patches/kernel/display/final/spi-esp32.c "$K/drivers/spi/spi-esp32.c"
cp "$here"/patches/kernel/display/final/co5300.c "$K/drivers/video/fbdev/co5300.c"
# 0006-dts-side-buttons.patch was deleted rather than kept: every element it
# adds -- gpio-keys, pusher_b_pins, pmic_irq_pins, the PMU interrupt -- is
# present in the whole-file DTS below, so the patch could never apply and never
# needed to. With the loop merely warning, that went unnoticed; with it failing
# hard, it surfaced on the first clean build.
#
# The panel API's structs and bounds check, shared with scripts/co5300-selftest.py.
cp "$here"/patches/kernel/display/final/co5300_rect.h \
   "$K/drivers/video/fbdev/co5300_rect.h"

# The firmware log pipe. Its Makefile entry and dispatch arrive in
# patches/kernel/0007, but the source is carried whole here for the same reason
# as the drivers above: a whole-file copy cannot silently fail to apply.
cp "$here"/patches/kernel/display/final/esp_log_pipe.c \
   "$K/drivers/net/wireless/espressif/esp32-ng/esp_log_pipe.c"
cp "$here"/patches/kernel/display/final/esp32s3-devkit-c1.dts \
   "$K/arch/xtensa/boot/dts/esp32s3-devkit-c1.dts"

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

# The config switches are applied further down, once .config exists. Running
# scripts/config here used to work only because an earlier build had left a
# .config behind; on a clean tree it fails with "No such file or directory"
# and the build carries on with defaults.
cd "$here"

# ---- build -----------------------------------------------------------------
say "build"
cd "$K"
# Our own config, not the port's seed. The seed is a moving target that has to
# be patched again on every version bump, and a missed patch is invisible: the
# kernel builds and simply lacks the symbol. One tracked file that is copied
# verbatim is checkable, and check-kernel-config.py compares against it below.
cp "$here/patches/kernel/config/devkit_c1_16m_linux.config" .config
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
    "$here/patches/kernel/config/devkit_c1_16m_linux.config" .config \
    || die "config drifted from the board seed"
grep -q '^CONFIG_XTENSA_NOMMU_FORK=y' .config || die "fork backend not enabled"
grep -q '^CONFIG_LOCALVERSION="-forkbank"' .config || die "LOCALVERSION not set"
grep -q '^CONFIG_VECTORS_ADDR=0x40380000' .config || die "CONFIG_VECTORS_ADDR moved"
grep -q '^CONFIG_RTC_DRV_PCF85063=y' .config || die "RTC driver missing"

# The panel API's argument validation is pure arithmetic and lives in
# co5300_rect.h so it can be exercised here rather than discovered on the glass.
# A rejected rect is a caller bug; a silently accepted one that lands on the
# console is worse.
python3 "$here/scripts/co5300-selftest.py" || die "panel rect validation failed"

size=$(stat -c %s arch/xtensa/boot/xipImage)
# The kernel shares the linux partition with the Tamagotchi ROM.
#
# The ROM is written raw to flash at 0x480000 and read from userspace through
# the linear flash window at 0x42480000, which costs it no RAM at all -- see
# docs/tamagotchi.md. That means the slot is only free if the kernel stays
# below it, and a kernel that grows past 0x480000 would be silently overlapped
# by the next ROM write instead of failing. 0x140000 is where the partition and
# the kernel start, so this is the size the image must not exceed.
ROM_OFFSET=$((0x480000))
kernel_limit=$((ROM_OFFSET - 0x140000))
[ "$size" -le "$kernel_limit" ] \
    || die "xipImage $size would reach the ROM slot at 0x480000 (limit $kernel_limit bytes); move the slot or shrink the kernel"
[ "$size" -le $((0x400000)) ] || die "xipImage $size exceeds the 4 MiB linux partition"
# Capture once rather than piping into grep -q. Under `set -o pipefail`,
# `strings ... | grep -q` fails intermittently: grep exits at the first match,
# strings takes SIGPIPE, and the pipeline's status becomes 141 -- reported as
# "the fork backend did not apply" on a kernel that is perfectly fine.
kv_strings=$(strings arch/xtensa/boot/xipImage)
case $kv_strings in
    *"Linux version $KV-forkbank"*) ;;
    *) die "kernel is not the forkbank build -- the fork backend did not apply" ;;
esac

printf '\nxipImage: %s bytes (ROM slot at 0x%X, %s bytes free before it)\n' \
    "$size" "$ROM_OFFSET" "$((kernel_limit-size))"
printf '%s\n' "$kv_strings" | grep -m1 -o "Linux version [^ ]*" || true
printf '\nflash:\n  esptool.py ... write_flash 0x140000 %s/arch/xtensa/boot/xipImage\n' "$K"
