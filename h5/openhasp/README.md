# NanoELS H5 layouts for openHASP

800 × 480 landscape layouts for the CrowPanel Advance DIS02050A project.
These are the screen definitions and the field/event mapping for H5's
openHASP adapter. They do not implement a Nextion serial emulator or establish
board support for openHASP.

## Install the layouts

1. Start with a working openHASP installation for your exact board, configured
   for 800 × 480 landscape with PSRAM and TrueType font support. This layout
   targets the documented openHASP 0.7.0 interface; board firmware is not included.
2. Upload `mono.ttf` and `pages.jsonl` to the root of the display filesystem
   through openHASP's file editor. Keep those filenames unchanged.
3. Select `/pages.jsonl` as the startup layout if necessary and reboot/reload it.
4. Send `page 1` to show the main screen; send `page 2` to inspect the mode menu.
   The display's startup page should be 1.
5. With H5's openHASP adapter and its generic UART transport extension enabled,
   the machine buttons publish normal openHASP events to H5. The added BACK
   button on page 2 already returns locally to page 1.

Upload the two files individually; the ZIP itself is a distribution bundle.
No picture backgrounds or icon-font downloads are required. The bundled
DejaVu Sans Mono font supplies both text and navigation symbols.

## What was recreated

| Nextion screen | openHASP screen | Contents |
| --- | --- | --- |
| page 0 | page 1 | Status, mode, pitch, units, multistart, pitch adjustment, step size, spindle data, X/Y/Z positions and limits, six jog buttons, prompt line, numeric keypad, stop, backspace, start/confirm |
| page 1 | page 2 | GEAR, TURN, FACE, CONE, CUT, THREAD, ELLIPSE, GCODE, ASYNC, Y, XGEAR, JOY, SLOT |

Page 0 is deliberately unused: openHASP reserves it for objects shared by all
pages. Component IDs on the main screen are retained. Mode touch IDs are also
retained. The main layout includes Y even for a two-axis lathe, matching the
original; H5 sends blank Y values when that axis is inactive.

H5 uses the same main screen for all machining modes. It changes `bMode` and
the `t3` prompt rather than loading separate threading/turning setup pages.
Examples include `Set all stops`, `5 passes?`, `External?`, `Go?`, and
`Pass 2 of 5`. These strings remain controller-owned. Machine configuration
belongs to H5's Web UI, so no invented machine-settings screens are included.

The layout follows the Nextion component geometry and the documented screen
image. Intentional adaptations:

- Native widgets replace bitmap backgrounds and transparent touch overlays.
- DejaVu Sans Mono replaces Nextion `.zi` fonts. Sizes are adjusted for fit.
- Main-screen symbols use comparable Unicode glyphs. Small spindle captions
  use STEP, RPM, REV, and a degree symbol.
- The mode selector preserves its three-column order and white/blue coding;
  the decorative machining pictograms are omitted in this first conversion.
- The keypad uses the newer project order 1–9, then 0. The older annotated
  README screenshot shows 0 first.
- Limit glyphs and their numeric values both emit the corresponding limit
  action, without overlapping invisible widgets.
- Live values start blank or `--`, with `Waiting for NanoELS`, instead of
  showing plausible but stale example machine values.
- BACK is a local addition on the mode page. Other page changes remain
  controller-driven, preserving H5's stop-before-mode-selection behavior.

## Updating values

`mapping.json` maps every live Nextion name to an openHASP object. For example,
Nextion `tZ.txt="0.125"` corresponds to this openHASP command:

```text
jsonl {"page":1,"id":21,"text":"0.125"}
```

Other examples:

```text
jsonl {"page":1,"id":3,"text":"ON"}
jsonl {"page":1,"id":4,"text":"THREAD"}
jsonl {"page":1,"id":5,"text":"11"}
jsonl {"page":1,"id":6,"text":"TPI"}
jsonl {"page":1,"id":2,"text":"5 passes?"}
page 2
```

These are the commands that H5's selected openHASP adapter sends. The supplied
`demo.cmd` only sets illustrative display values; it does not send machine
commands. Restore live values before using a connected controller.

## Skin contract

A skin is a local panel asset bundle: its `pages.jsonl`, fonts, and any images
it uses travel together. Selecting another skin is a layout reload or panel
restart operation; live switching is not part of this contract. Send `ready 1`
after the replacement layout is loaded so H5 refreshes its live values.

[`mapping.json`](mapping.json) is the authoritative NanoELS contract. Its
canonical component names, page/object IDs, and meanings are permanent once
published. Skin authors may rearrange appearance and may add new IDs, but must
not repurpose a canonical ID.

Contract version 1 requires no output field. Its only required control is the
visible, enabled, clickable, momentary `mStop` at `p1b23`; it must not have a
local `action`. Every other mapped field or control is optional. A skin may,
for example, omit Y-axis values and controls on a two-axis machine. H5 may send
an update for an omitted field; the panel ignores it because no such object is
loaded. An omitted control cannot send an event.

An older H5 ignores unknown future controls. A valid `down` event from one
first releases any held known control, then does nothing else; unknown release,
long, hold, and changed events are ignored. This preserves safe release
semantics without requiring the controller to know every optional control.

Validate a skin before loading it:

```sh
python3 h5/openhasp/tools/validate_skin.py --pages /path/to/pages.jsonl
```

The validator is deliberately offline and uses only Python's standard library.
It enforces the current required baseline and duplicate page/object IDs, while
allowing optional and future IDs. Layout identity/version metadata carried from
JSONL tags is deferred until the generic openHASP transport can expose it.

## Event contract for the H5 adapter

Every actionable machine widget has a `tag` containing its `nextion_page`
and `nextion_id`. `mapping.json` also records the H5 action constant.

For a bridge that emulates Nextion touch packets:

- openHASP `down` → `65 PP II 01 FF FF FF` (bytes in hexadecimal).
- openHASP `up` or `release` → `65 PP II 00 FF FF FF`.
- Ignore `long` and `hold`; do not translate them into repeated presses.
- Track the active press, and send its release using the original page and
  ID even if H5 changes the page while the finger is down.
- Do not forward the local BACK action to NanoELS.

For example, main-screen Z-left jog is `p1b48`, tagged Nextion page 0,
component 48 (hex 30). Its press packet is `65 00 30 01 FF FF FF`.
Release is `65 00 30 00 FF FF FF`.

Use the explicit mappings for limit values: `p1b13` and `p1b36` both map to
Nextion ID 36, etc. H5's `HEX_TO_KEYCODE` does not assign actions to several
original numeric limit component IDs; it uses the overlaid hotspot IDs.

All buttons are momentary (`toggle:false`). The controller owns ON/OFF,
axis enable state, mode selection, units, numeric entry, and workflow state.
Press/release handling matters for jog and stop behavior.

The H5 display-output subset found in the audited source is:

| H5 output | Future integration behavior |
| --- | --- |
| `<name>.txt="..."` | Map name to `.text` on the corresponding openHASP object |
| `page 0` | openHASP `page 1` |
| `page 1` | openHASP `page 2` |
| `play 0,0,0` | Optional local beep; not a layout operation |

H5 terminates commands with three `0xFF` bytes and uses 115200 baud. Its font
uses byte `0xDF` as a degree glyph; a future parser must explicitly convert
that byte to Unicode `°` (UTF-8 C2 B0), rather than treating it as normal Latin-1.
Do not run H5's Nextion TFT uploader against openHASP. Upload JSONL/fonts through
openHASP instead. Serial event routing is separate work: JSONL alone does not
make openHASP emit the Nextion packets above.

## Provenance

Audited upstream `kachurovskiy/nanoels`, commit
`8aa938936bc8e0051c7eb3cf14a173e1f1bc649a`, retrieved 2026-09-27.
This is current upstream, not a confirmed match to your installed H5 version.

- [H5 source](https://github.com/kachurovskiy/nanoels/blob/8aa938936bc8e0051c7eb3cf14a173e1f1bc649a/h5/h5.ino):
  `updateDisplay`, `buildDisplayMessage`, `HEX_TO_KEYCODE`,
  `processNextionMessage`, `setModeFromUi`, and display startup/error handling.
- [H5 manual](https://github.com/kachurovskiy/nanoels/blob/8aa938936bc8e0051c7eb3cf14a173e1f1bc649a/h5/README.md):
  Touchscreen and usage sections, including annotated main-screen image.
- [Nextion project](https://github.com/kachurovskiy/nanoels/blob/8aa938936bc8e0051c7eb3cf14a173e1f1bc649a/h5/screen/h5.HMI):
  readable attribute records provided names, IDs and geometry. The binary also
  contains older duplicate editor records. The complete main component run
  labelled `NanoEls TFT20260511` and the mode run containing SLOT were selected,
  then cross-checked against the current firmware's field names and event IDs.
  This is targeted attribute extraction, not a full Nextion file-format decoder.
- `h5/screen/h5.fig` thumbnail: visual reference for current mode order/colors.
- [openHASP pages](https://www.openhasp.com/0.7.0/design/pages/),
  [objects/events](https://www.openhasp.com/0.7.0/design/objects/),
  [fonts](https://www.openhasp.com/0.7.0/design/fonts/),
  [styling](https://www.openhasp.com/0.7.0/design/styling/).

## Validation and limits

Checked JSONL parsing, unique IDs, screen bounds, bundled glyph coverage,
all 18 live text fields, all 41 main action IDs, and all 13 mode action IDs
against H5 source. Inspected offline renders of both pages.

The PNGs and `preview.html` are geometry/font previews, not captures from
openHASP. The bundled layout has now been loaded on the target panel and basic
UART operation and visual rendering have been confirmed; `mono.ttf` is required
for the intended sizing and alignment. The complete control matrix, held-touch
recovery, link interruption, and machine operation have not yet been exhaustively
tested. Long or unusually large numeric strings can exceed the original field
widths; the supplied fields use crop mode. Verify your expected ranges on
hardware before relying on the readouts.

`source-components.json` preserves the extracted source geometry for future
refinement. `validation.txt` records what was and was not checked.
Upstream NanoELS and bundled-font notices are included in the license files.
