# Staged bench test and evidence sheet

Status: preparation only. No physical tests have been performed. Record PCB
revision, module marking, firmware commit/build flags, layout version and each
result below. Keep motor/spindle power disconnected for display and UART tests.

## 1. Identify and preserve

- Photograph both sides of DIS02050A, revision marking and expansion labels.
- Back up existing firmware/configuration/filesystem and document recovery steps.
- Confirm actual flash/PSRAM size and the revision-specific vendor schematic.
- The proposed openHASP profile targets V1.2/V1.3 STC-controlled hardware only;
  a matching product name alone is insufficient. Full firmware build is pending.
- Confirm expansion pin routing, DIP switch position and conflicts with microphone,
  USB, wireless modules and other peripherals before selecting RX/TX.

## 2. Panel alone

- Boot with no controller connected. Verify 800 × 480 orientation, RGB colors,
  stable image, PSRAM allocation and touch accuracy at center and all corners.
- Test brightness 0, 1, 50%, 100%, display off/on and power-cycle persistence.
- Load mono.ttf, pages.jsonl and all `/icons/*.png`; verify every icon and label.
- Tap directly on each icon and its button label: both must reach the same target.
- Inspect both pages and local BACK. Use demo.cmd only for disconnected preview;
  reload normal initial values before connecting H5.
- Record free heap/PSRAM before/after repeatedly switching pages; check logs for
  image decode errors and missing files. Confirm logs stay on the console UART.

## 3. UART, still with motion power disconnected

Use confirmed **3.3 V TTL UART**, crossed TX/RX and common ground; do not connect
RS-232 signals or 5 V logic to ESP32 pins. H5 TX43 → panel RX; panel TX → H5 RX44.
Power each board according to its own requirements; do not assume a header power
pin can supply the other board. Configure panel serial.json with verified pins,
UART1, 115200 baud and enabled=true; restart. H5 Screen type=openHASP, Save/restart.

Capture bytes with a logic analyzer or appropriately connected UART monitor:

| Test | Expected evidence |
| --- | --- |
| Panel starts after H5 | `ready 1` then page + all 18 fields resent |
| H5 starts after panel | Current page/fields appear; no demo/stale values |
| Hold/release p1b48 | One logical press then release, original action retained |
| Drag off button | Release/lost cancels press; no held action |
| Duplicate up/release | No duplicate machine action |
| Page switch while held | Held action cancelled |
| Panel reboot/layout reload | Held action cancelled on ready; full redraw |
| Local BACK | Correct main page; no unrelated machine command |
| 13 mode buttons | Correct labels/actions, including XGEAR/JOY/SLOT |
| Limits, numeric keypad, all jogs | Compare mapping.json and actual event IDs |
| Non-ASCII/long values | Degree symbol correct; expected values fit fields |
| Malformed/oversize line | Ignored/resynchronized; next valid command works |
| MQTT also enabled | Existing MQTT functionality continues independently |
| TFT upload in openHASP mode | Rejected before any uploader bytes are sent |

Disconnect the cable while a jog is held **only with motion power isolated**.
Record the known missing-release behavior. There is no heartbeat watchdog;
do not treat a passing restart test as proof of cable-loss safety.

## 4. Regression and eventual motion

Retest the original Nextion backend, default configuration and its uploader
with the actual Nextion panel. Verify saved screen selection survives reboot.
Only after the owner reviews the results and stop behavior should powered motion
be considered: use the machine's established commissioning procedure, minimal
travel, clear work area and an independently verified emergency stop. This
checklist is not approval for an unattended motion or cutting test.

## Result record (copy per test)

Date / tester / PCB revision / build commit / firmware configuration / layout
commit / test name / expected / actual / pass-fail / serial capture / photograph /
remaining issue. Stop at a failed prerequisite and retain logs before changing it.
