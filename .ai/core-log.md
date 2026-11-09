## 1 COMMIT Unreleased 2026-11-09T10:26:39-07:00

#### Coming From:

Unreleased 34677f3

#### Purpose:

Populate the empty `.ai/core-reference.md` with the externally defined facts this project depends on, in the format established by TinyTang's reference file.

#### Outcome:

`.ai/core-reference.md` was written from nothing to 35 records across twelve topics — SOC, BRD, LCD, PMU, RTC, KRN, FLS, JFS, BLE, NET, TOL and EXT — with the full structure of sections 1 to 8: operating rules, topic catalog, active routing, fast lookup index, records, record template, maintenance boundary and retired ids. Every record was audited for completeness (kind, topic_id, title, status, verified_date, statement, consequence, sources, verification) and the file was machine-checked: all six embedded YAML blocks parse, all 35 record ids are unique, all 35 routing-table references resolve to a record, and the fast index and the record set are identical in both directions. The content is drawn from work verified on hardware in this and prior sessions rather than from a fresh lookup, so most records are VERIFIED, with a small number explicitly weaker where the underlying check was a reading of a document rather than a measurement. Several records are negative results kept deliberately because each cost a session to establish: that the CO5300 will not honour a one-pixel window while returning success, that pixel data must be byte-swapped and that black hides the mistake, that one pixel is lost at the left and top edges, that firmware cannot write flash once Linux owns core 1, that the linear flash window exists only immediately before the jump to Linux, and that a non-recursive patch glob plus a swallowed error silently removed two entire patch series from the kernel build. A rule was recorded in section 1 and again in section 7 that display records may only be verified by looking at the glass, because three separate display defects in this project reported success while drawing nothing. The required `core-syntax.md` audit found that document unmodified and `core-log.md` previously empty, so there were no existing entries to correct. No build or deployment was applicable: the cycle changed documentation only.

#### Next Steps:

No validation is outstanding for this cycle. The next cycle can begin on the deferred bring-up work, of which touch (the CST9217 at I2C 0x1A, which has no driver at all) and console repaint speed remain the largest gaps; the repaint work now has a concrete lead because the band-transfer shape the controller accepts was established while drawing the border.

#### Files Modified:

None.

#### Status:

- Build: NOT RUN
- Deployment: NOT RUN
- User Test: NOT RUN

---
