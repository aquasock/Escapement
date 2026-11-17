# Documentation

Two kinds of document live here, and mixing them up is the way to waste a
session. **Current** documents describe what runs on the watch now. **Archive**
documents describe the Linux line this project started on and abandoned on
2026-11-11 — accurate about what was measured, wrong about what the watch does
today.

As a rough test: a document that talks about an init script, a kernel partition,
`fbcon`, `jffs2` or a bash login is from the Linux line. The watch runs TinyDesk
on ESP-IDF with `tdsh`. The table below is the authority, not the test.

The firmware's own documentation is not here — it is
`ports/twatchultra/README.md` in the fork
(`/run/media/vash/GIT/tinydesk-twatch`), and it is the authority on the port.

## Current

| Document | What it answers |
| --- | --- |
| `nes-accuracy.md` | How correct, not just how fast, the NES core is. 14 of 22 AccuracyCoin pages, 68 pass, 35 fail; every passing group is an opcode, every failing one is timing or bus accuracy. |
| `gbc-scoping.md` | Whether Game Boy Color is worth attempting, projected from measurement. Read it with its own correction: it predicted 28-45 fps and the port measured 116. |
| `pokemini-scoping.md` | The same question for Pokemon Mini, and the third candidate platform. |

The last two are scoping documents written *before* their ports existed. Their
per-core measurements still hold; their conclusions have been overtaken by the
`gbcbench` and `pmbench` shell commands, which measure on the watch itself.

## Archive — written during the Linux line

Kept because they contain the only measurements of parts of this board, and
because the reason the line was abandoned is worth having. **None of this
describes the running system** — but "archive" here means provenance, not
worthlessness. Two are still load-bearing: `panel-geometry.md` is the measurement
the current desktop is laid out inside, and `panel-transfer-cost.md`'s numbers are
what justify the port's rectangle-based flushes. The rest describe code that no
longer runs.

| Document | Still worth reading for |
| --- | --- |
| `bring-up.md` | How the board was first made to boot at all, including the three traps that cost time. Historical result. |
| `cores.md` | The two-core split and what may not move across it. Written for the Linux arrangement; the reasoning about the shared cache and the flash-write constraint still applies to any code on this board. |
| `panel-geometry.md` | **The chamfer measurement.** Where the 320x400 rectangle comes from and why it is inset 45 px. Still exactly current — the port's desktop sits in that rectangle. |
| `panel-reservations.md` | A specification for the four strips around the framebuffer. Superseded in practice by the port's `edges.h`, which is what actually defines the margins. |
| `panel-api.md` | The ioctl spec for drawing outside the framebuffer. Not implemented; the port paints margins inside its own display driver instead. |
| `panel-transfer-cost.md` | What a flush costs on this glass, measured with `CO5300_IOC_BENCH`. The numbers behind the port's rectangle-based dirty tracking. |
| `bluetooth-keyboard.md` | Five separate defects that had to be fixed before a BLE keyboard could type, including one that failed silently while looking like success. The firmware HID host now lives in the TinyDesk fork, but these defects are the same ones. |
| `bluetooth-bond.md` | Why a bond cannot be stored by the firmware after Linux starts, and the split that worked around it. Historical: the fork keeps its bonds in NVS instead. |
| `tamagotchi.md` | The Tamagotchi V3 emulator's speed, in detail: why the ROM lives in flash, what an emulated instruction actually costs, the core's move to fixed point, and a parked ~1.5 s wait entering the time screen. Written for the Linux harness; the core is now in the fork and driven by `tama`. |
| `toolchain.md` | The Xtensa Linux compiler that had to be built. Historical — the fork uses ESP-IDF's own toolchain. |
| `watch-recovery.md` | **The recovery rule.** A full 16 MiB image, never a saved kernel: a kernel that was known to boot was flashed back and hung too, because the damage was elsewhere. The specific commands are Linux-era; the rule is not. |

## Scripts, by the same split

In `scripts/`, these are still live:

- `watch-backup.sh` — reads the whole 16 MiB flash and writes a hash beside it.
  That is the recovery artefact; nothing else restores the partition layout. It
  uses the old toolchain's Python (`build/idf-tools/python_env/idf5.1_py3.14_env`,
  esptool 4.13) rather than the current one, which works only because that
  directory is still on disk — worth knowing before anyone deletes `build/`.
- `tamacore-host.sh` — runs the Tamagotchi core on the development machine and
  checks its audio envelope. The firmware's sound trigger depends on it.
- `tamacore-sync.sh` — fails when the fork's copy of the core drifts from this
  repository's, which is the only thing keeping the two in step.

The rest target the Linux image and will not build anything the watch can run.
`build-kernel.sh`, `build-userspace.sh`, `build.sh`, `flash.sh`, `probe.sh`,
`fetch-images.sh`, `install-tamacore.sh`, `build-tamacore.sh` and
`build-tamafb.sh` belong to the abandoned line. The benchmark harnesses
(`gbcbench.sh`, `nesbench.sh`, `pmbench.sh`, `build-corebench.sh`) still build
host binaries and remain the record of how the cores were chosen, but the
measurement that counts is now the shell command on the watch.
`pmbench.sh target` is explicitly documented as *not* producing anything the
watch can run — it links a Linux ELF, and there is no ELF loader.

## Where the reasons are

`.ai/core-log.md` is the decision log: every cycle, what was tried, what broke,
and what the evidence was. `.ai/core-reference.md` is the same knowledge indexed
by question — it has an entry for "why does a module still run but decode
garbage after an app rebuild", which is not a question anyone expects to have.
