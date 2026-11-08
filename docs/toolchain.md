# Building the toolchain

There was no Xtensa Linux compiler on the machine this was done on, and the
rebuild needs one. crosstool-NG builds it from pinned sources in about
7 minutes once the four failures below are worked through. Each one looked
like something else.

## The recipe

Pinned in `sources.lock`: crosstool-NG `bec302a0`, xtensa-dynconfig
`6615c332`, config-esp32s3 `f595d865`, GCC `f9a78b24`, binutils `6109869b`,
uClibc-ng `28290f2b`, linux headers `ffc25326`. Target sample
`xtensa-esp32s3-linux-uclibcfdpic`.

```sh
make -C xtensa-dynconfig ORIG=1 CONF_DIR=$PWD/confs esp32s3.so
(cd crosstool-NG && ./bootstrap && ./configure --enable-local && make -j)
export XTENSA_GNU_CONFIG=$PWD/xtensa-dynconfig/esp32s3.so
(cd crosstool-NG && ./ct-ng xtensa-esp32s3-linux-uclibcfdpic
                && python3 build/pin-toolchain.py .config
                && CT_PREFIX=$PWD/builds ./ct-ng build)
```

Host packages needed beyond a normal build box: `libtool-bin` (Ubuntu splits
`libtool` and the actual binary across two packages), `gperf`, `texinfo`,
`help2man`, `gawk`.

## 1. The GNU mirrors are unreachable

`ftpmirror.gnu.org` and `ftp.gnu.org` time out from this host on **both** IPv4
and IPv6, including by raw IP. The build stalls on the first tarball,
autoconf, and the progress counter keeps spinning so it looks like a slow
download rather than a dead one. Check for it with:

```sh
stat -c %s crosstool-NG/.build/tarballs/*.tmp-dl   # 0 bytes, forever
```

`scripts/functions`' `CT_Mirrors` hardcodes those hosts for `GNU` projects.
`mirrors.kernel.org` carries the same tree, so it was put first. `sourceware`
and `gcc.gnu.org` were tested and are reachable, so only GNU needed changing.

## 2. GCC 15 defaults to C23, and GMP 6.2.1 is not C23

```
configure: error: could not find a working compiler
```

GMP's configure probes with `void g(){}` then calls `g(...)` with arguments.
Under C23 an empty parameter list means *no arguments*, so the call is an
error, the probe fails, and configure concludes the compiler is broken. The
host here is GCC 15.2.

Fix: `CT_EXTRA_CFLAGS_FOR_HOST="-std=gnu17"`. Note this applies to companion
libraries only, not to the target libc — see the next one.

## 3. uClibc-ng's unifdef.c names a variable `constexpr`

```
extra/scripts/unifdef.c:204:1: error: 'constexpr' in empty declaration
```

`constexpr` became a keyword in C23. `unifdef` is a host tool, built with the
host compiler, so it hits the same problem. Six occurrences renamed to
`const_expr`.

**This is the entry that cost the most time, and the reason is worth keeping.**
The first attempt edited `.build/.../src/uClibc-ng/extra/scripts/unifdef.c`
directly. It compiled fine. But crosstool-NG **re-extracts the tarball** on a
restart, silently reverting the edit, so the identical error came back and
looked like the fix had never worked.

Extracted sources are never the place to patch. The durable channel is
`CT_LOCAL_PATCH_DIR`, which this project already points at
`new-files/toolchain-patches/` — the same place upstream keeps its own
uClibc-ng heap patch. Installed there as
`0002-unifdef-constexpr-is-a-C23-keyword.patch`, and the stale
`.uClibc-ng-*.patched` marker removed so re-extraction would happen.

Any later failure of this shape — a C23 keyword used as an identifier, or an
empty parameter list relied on to mean "unspecified" — is the same kind of
one-liner.

## 4. Watch for a process check that matches itself

`pgrep -f "gmake -rf ./ct-ng build"` matches **its own command line**, so it
reports RUNNING on a build that has exited. Liveness has to be checked some
other way: process-tree listing that excludes the checker, or the build log
still growing.

## Verifying the result

Exit status is not evidence. A build can report success and produce nothing
usable, so check the compiler itself:

```sh
builds/xtensa-esp32s3-linux-uclibcfdpic/bin/xtensa-esp32s3-linux-uclibcfdpic-gcc --version
# xtensa-esp32s3-linux-uclibcfdpic-gcc (crosstool-NG 1.25.0.183_bec302a) 14.1.0
```

and that it emits Xtensa objects:

```sh
echo 'int main(void){return 0;}' > t.c
...-gcc -mfdpic -Os -c t.c -o t.o && file t.o
# ELF 32-bit MSB relocatable, Tensilica Xtensa, version 1
```
