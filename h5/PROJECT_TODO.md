# H5 / openHASP project checklist

Last updated: 2026-09-27 UTC. Working notes will be updated with completed work
and evidence. The physical panel is CrowPanel Advance 5-inch **DIS02050A**.

- [ ] **1. Physical hardware testing** — [bench plan](docs/HARDWARE_TEST_PLAN.md)
  prepared. Owner must identify the PCB, run staged tests and retain evidence.
- [x] **2. Mode icons implemented** — 13 original SVG/PNG pictograms,
  reproducible previews and preserved action IDs/touch rectangles. Hardware
  PNG decoding and icon click-through remain part of item 1.
- [x] **3. Contribution process investigated** — [proposal/split plan](docs/CONTRIBUTING_PLAN.md)
  prepared for both projects. Full panel builds and physical tests remain gates;
  no upstream outreach or PR submission has occurred.
- [ ] **4. Owner review of NanoELS** — [guided implementation review](docs/IMPLEMENTATION_REVIEW.md)
  prepared, including the fixed-mapping/template limitation and size measurements.
- [x] **5. Panel-side Nextion compatibility assessed** — [assessment](https://github.com/nufanwfk/openHASP/blob/codex/crowpanel-advance-profile/docs/nextion-compatibility-assessment.md)
  covers the NanoELS subset, portable architecture, reset limitation and legal
  questions. Decision/implementation deferred to the owner.
- [ ] **6. DIS02050A support** — [experimental V1.2/V1.3 profile and board helper](https://github.com/nufanwfk/openHASP/blob/codex/crowpanel-advance-profile/docs/crowpanel-advance.md)
  prepared from vendor examples; host checks and helper compilation pass.
  Exact PCB identification, complete firmware build and hardware validation remain.

## Follow-up delivery

- NanoELS: `codex/display-project-followup` (icons, checklist and review/test docs).
- openHASP: `codex/crowpanel-advance-profile` (experimental board changes and
  panel-side compatibility assessment), based on `codex/generic-uart`.
- Neither main branch was changed; no firmware was flashed.

## Existing delivered foundation

- H5 branch `codex/h5-openhasp-adapter`, commit `77c7fe8`: runtime backend
  selection, token events, Nextion default. Full ESP32-S3 build and host tests pass.
- openHASP branch `codex/generic-uart`, commit `e955c8c`: generic serial extension
  on release `0.7.0-rc13`, portable hardware interface, ESP32 implementation.
- Panel host/sanitizer/compatibility tests pass; full firmware build remains
  blocked by a toolchain dependency download, and exact panel hardware unverified.
- No heartbeat/link-loss watchdog. A disconnected held jog can lose its release.

## Decisions reserved for the owner

- Supply the PCB revision and module markings/photo before selecting revision-
  specific wiring or flashing a new board build.
- Review the H5 adapter against the desired user-configurable template scope.
- Choose whether to pursue a Nextion compatibility library after the assessment.
- Approve eventual upstream outreach/submissions and any motion test on hardware.
