#!/usr/bin/env bash
# tamacore-sync.sh - keep the two copies of the emulator honest.
#
#   ./scripts/tamacore-sync.sh check    # fail when the copies differ
#   ./scripts/tamacore-sync.sh sync     # copy this tree's originals over
#   ./scripts/tamacore-sync.sh list     # what gets compared
#
# WHY THERE ARE TWO COPIES AT ALL. The emulator's portable half lives in this
# repository, under patches/harness, where `tamacore-host.sh` compiles it
# straight out of the tree so the host harness and the firmware cannot drift.
# ESP-IDF cannot reference sources outside its own project directory, so the
# firmware build needs its own copies inside
# ports/twatchultra/components/tamacore.
#
# That is a real duplication and it will rot unless something checks it. The
# failure mode is the worst kind: a fix applied to the host copy, verified on
# the host, and silently absent from the firmware -- which then behaves exactly
# as it did before the fix, and the natural conclusion is that the fix was wrong.
#
# So the direction is fixed (this tree is the original, the port is the copy),
# the file list is explicit rather than a wildcard, and `check` is meant to run
# before any firmware build that claims to include an emulator change.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

# Find the port rather than assume where it is: this project has lived on two
# different mounts. A sibling checkout is tried first because that is how these
# two repositories are laid out, and $HOME/GIT second because that is the usual
# place on a machine that has them on internal storage.
if [ -n "${TMC_PORT:-}" ]; then
    PORT=$TMC_PORT
elif [ -d "$here/../tinydesk-twatch/ports/twatchultra/components/tamacore" ]; then
    PORT=$here/../tinydesk-twatch/ports/twatchultra/components/tamacore
else
    PORT=$HOME/GIT/tinydesk-twatch/ports/twatchultra/components/tamacore
fi

# src:relative/to/this/tree -> dst:relative/to/the/component
FILES=(
    "patches/harness/splb32/splb32.c:splb32.c"
    "patches/harness/splb32/splb32.h:splb32.h"
    "patches/harness/splb32/splb32_sound.c:splb32_sound.c"
    "patches/harness/splb32/splb32_sound.h:splb32_sound.h"
    "patches/harness/splb32/ht24lc08.c:ht24lc08.c"
    "patches/harness/splb32/ht24lc08.h:ht24lc08.h"
    "patches/harness/splb32/con_v3_ir.c:con_v3_ir.c"
    "patches/harness/splb32/con_v3_ir.h:con_v3_ir.h"
    "patches/harness/segmap-1x/segmap.c:segmap.c"
    "patches/harness/segmap-1x/segmap.h:segmap.h"
    "patches/harness/core0/render.h:render.h"
    "patches/harness/core0/tmc_core.c:tmc_core.c"
    # The header is the component's only public one, so it lands elsewhere.
    "patches/harness/core0/tmc_core.h:include/tmc_core.h"
)

check() {
    local bad=0 src dst

    if [ ! -d "$PORT" ]; then
        echo "no component at $PORT" >&2
        echo "set TMC_PORT if the tinydesk checkout is somewhere else." >&2
        return 2
    fi

    for pair in "${FILES[@]}"; do
        src=${pair%%:*}
        dst=$PORT/${pair#*:}

        if [ ! -f "$dst" ]; then
            echo "MISSING  ${pair#*:}"
            bad=$((bad + 1))
        elif ! cmp -s "$src" "$dst"; then
            echo "DIFFERS  ${pair#*:}"
            bad=$((bad + 1))
        fi
    done

    if [ "$bad" -ne 0 ]; then
        echo "$bad file(s) out of sync; run: ./scripts/tamacore-sync.sh sync" >&2
        return 1
    fi

    echo "in sync (${#FILES[@]} files)"
}

sync() {
    [ -d "$PORT" ] || { echo "no component at $PORT" >&2; return 2; }
    mkdir -p "$PORT/include"

    local pair dst
    for pair in "${FILES[@]}"; do
        dst=$PORT/${pair#*:}
        cp "${pair%%:*}" "$dst"
        echo "copied ${pair#*:}"
    done
    check
}

list() {
    local pair
    for pair in "${FILES[@]}"; do
        printf '%s\n  -> %s/%s\n' "${pair%%:*}" "$PORT" "${pair#*:}"
    done
}

case "${1:-check}" in
    check) check ;;
    sync) sync ;;
    list) list ;;
    *) sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
