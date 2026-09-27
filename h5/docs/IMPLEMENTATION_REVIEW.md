# H5 adapter review guide

Review base: `77c7fe8` on `codex/h5-openhasp-adapter`. This follow-up changes
layouts and documentation, not H5 motion or display adapter code.

## Start here

1. `h5.ino`: search `CFG_DISPLAY_TYPE`, `activeDisplayBackend`, and `displayType`.
   Web UI **Screen type** selects Nextion (0, default) or openHASP (1). Save and
   restart applies it; changing a setting does not switch protocols mid-stream.
2. `display_mapping.h`: the fixed contract of 18 named text fields and 60 object
   mappings, including limit aliases and the local BACK action.
3. `display_protocol.h`: portable serialization and bounded receive parsing.
   `textCommand` converts text, including the Nextion degree byte, to the chosen
   backend. Nextion retains three 0xFF terminators; openHASP uses newline commands.
4. `h5.ino`: `setText` → `textCommand` → `writeScreenBytes` → Serial1. A transmit
   mutex prevents concurrent tasks interleaving commands. `setScreenPage` handles
   logical pages 0/1; openHASP physical pages are 1/2.
5. `readScreenEvent` → `screenTouchState` → `screenTouchKeycode` →
   `processKeypadEvent`: mapped touch events enter the existing key/action logic.
   Machine state and workflow remain controller-owned.
6. `refreshScreenIfRequested`: panel `ready 1` forces all fields to refresh even
   when the controller's cached values have not changed. Page/ready transitions
   cancel a held key; duplicate/stale releases are filtered.
7. Read `tests/run_display_tests.py` and its C++ fixtures, then run it from the
   repository root with `python3 h5/tests/run_display_tests.py`.

## Check the implementation against your goals

| Goal | Current result | Review decision |
| --- | --- | --- |
| Keep Nextion working | Default backend retained; both backends compiled | Test existing panel and TFT upload |
| Work-alike display functions in H5 | Existing calls serialize through a backend | Is this the intended abstraction? |
| Keep openHASP near stock | Opt-in generic UART extension; one layout-ready hook | Accept this small core hook? |
| Rearrange existing primitives | Positions, sizes, colors, fonts, decoration can change in JSONL | Keep IDs/pages for compatibility |
| Configurable templates | Layout loaded on panel; H5 mapping is currently fixed | No named template selector or arbitrary page remapping yet |
| Portable panel serial feature | Protocol and framing separated from UART HAL | Board support is separate |
| Reconnection recovery | Panel-ready forces redraw; page/ready cancels held key | No heartbeat or cable-loss watchdog |
| Existing feedback | Text and touch supported | openHASP beep not implemented |

A template can rearrange the same primitives without new widgets or machine
fields. Arbitrary new pages or changed IDs need a revised mapping/selection
contract; that broader template manager has not been delivered.

## Evidence and limits

The adapter's host tests and ASan/UBSan passed. The full H5 ESP32-S3 build passed:
1,245,457 bytes flash and 52,160 bytes static RAM, versus main's 1,240,913 and
51,856 (+4,544/+304). These are that build's linked sizes, not runtime heap or
worst-case stack costs. Both implementations are compiled; backend selection
is runtime configuration. Hardware behavior remains unverified.

The TFT uploader is blocked in openHASP mode. Serial is 3.3 V UART, 115200 8N1,
Serial1 RX44/TX43 on H5, not SPI. A cable failure while a jog is held can lose
its release; ready recovery helps after reboot but is not a link-loss failsafe.
Review that limitation before powered motion testing.
