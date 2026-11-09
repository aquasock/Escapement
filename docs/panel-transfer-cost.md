# Panel transfer cost — measured 2026-11-09

Measurements taken on the watch with `CO5300_IOC_BENCH`, which times the same
pixel payload twice: once with CS asserted so the panel receives it, and once
with CS never asserted so the peripheral clocks the bytes out into nothing. The
difference is the panel's contribution; the rest is the driver and the CPU.

## The numbers

```
shape      bytes   chunks   CS asserted   CS not asserted   panel share
288x2       1152       18        197 us          193 us          4 us
288x64     36864      576       6572 us         6470 us        102 us
```

## What follows

**Per-transfer fixed cost is effectively zero.** Fitting the two points:

```
slope       = (6572 - 197) / (576 - 18) = 11.42 us per 64-byte chunk
intercept   = 197 - 18 * 11.42          = -8.6 us  (negative, i.e. 0 within noise)
```

Cost is proportional to bytes and nothing else. The command and address phases,
CS assert and release, the RAMWR command and the completion poll together cost
less than the measurement noise at this scale. A dirty-rectangle design does not
pay a per-call penalty.

**The panel is not the bottleneck.** Its contribution is 0.18–0.22 µs per chunk
against 11.42 µs total: about **1.6 %**. Removing the panel from the circuit
would not make transfers meaningfully faster, so the panel is not gating the bus.

**Throughput is 5.6 MB/s, 56 % of the quad wire.** A 64-byte chunk is 6.4 µs of
wire time at 20 MHz over four lanes (10 MB/s). We spend 11.42 µs on it. The
missing 5 µs is per-chunk CPU work: 64 bytes is 16 `memcpy_toio` writes to the
FIFO, plus a register write for the length, plus the `cpu_relax()` poll for
completion. That is what DMA would remove.

## Implication for the 190 ms frame figure

`LCD-005` records a 320x400 frame at ~190 ms. At 11.42 µs per chunk, 4000 chunks
of a 320x400 frame come to **45.7 ms**, which is 4.2x faster than that record.

**These do not reconcile and the difference is not explained.** Candidates, none
confirmed:

- `LCD-005` was measured before the CS-handling fix in `co5300_blit`, where CS
  was released before a single pixel was sent.
- The 190 ms was a whole render loop, including work outside the transfer.
- The bench writes from `p->tx` with no `fb` copy, while a real frame costs a
  copy out of `p->fb` first.

`LCD-005` should be re-measured through the real repaint path before either
number is quoted again. Recording the discrepancy is more useful than picking
one.

## What this means for a second emulator platform

The panel is ruled out: it was timed, then removed from the circuit, and the
figure barely moved. The remaining cost is the driver's per-chunk CPU work, and
that is fixable.

- A 288x192 update at 1x is 1728 chunks, about 20 ms, about 50 fps. Comfortable.
- At 3x zoom it is 864x576 = 15,552 chunks, about 178 ms, about 5.6 fps. Not.

So the zoom level rather than the platform is the thing to decide. DMA is worth
roughly the CPU's share: 5 µs of every 11.42 µs, which caps the gain at about
**2x** -- not the 5-10x an earlier estimate assumed. That estimate assumed all
the missing time was FIFO poking; this measurement shows the panel is free but
does not break the 5 µs down further, so 2x is the ceiling from removing the
CPU from the path entirely.

## Reproducing

```sh
/home/panel-test -b 288,64,8      # shape, iterations
dmesg | grep "co5300 bench"
```

Each iteration is a real transfer, so keep `iters` small: 8 iterations of
288x64 is about 100 ms of bus time, but a wide/tall shape at high `iters`
blocks the console for as long as it runs.

**Caveat learned the hard way:** run it in the background
(`nohup ... &`). A bench that blocks leaves the calling shell unable to run
anything, and on this NOMMU kernel the blocked process cannot be interrupted
with Ctrl-C — only a chip reset clears it.
