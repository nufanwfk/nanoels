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
allowing optional and future IDs. Runtime skin identity and contract-version
negotiation are outside contract version 1.

## UART contract for the H5 adapter

The H5 openHASP backend requires a matching generic UART transport extension
on the panel. It does not use a Nextion serial emulator. H5 sends ordinary
LF-terminated openHASP commands, including the `jsonl` and `page` examples
above. The panel sends compact LF-terminated state messages:

```text
event p1b48 down
event p1b48 release
page 2
ready 1
```

All buttons are momentary (`toggle:false`). The controller owns ON/OFF,
axis enable state, mode selection, units, numeric entry, and workflow state.
Press/release handling matters for jog and stop behavior. The transport forwards
physical release/cancellation events and page changes; `ready 1` requests a
full display refresh after panel startup or a layout reload. Widget events use
their openHASP object topic, not JSONL tags or Nextion component numbers.

The transport extension must remain generic: NanoELS field and action mappings
belong in this repository's `mapping.json` and firmware adapter. Full framing,
buffer limits, lifecycle rules, and startup behavior are documented in
[`../DISPLAY_ADAPTER.md`](../DISPLAY_ADAPTER.md).

Both display backends use H5's Serial1 at 115200 baud. Do not run H5's Nextion
TFT uploader against openHASP; upload JSONL and fonts through openHASP instead.

## Provenance

The layout was derived from `kachurovskiy/nanoels` commit
`8aa938936bc8e0051c7eb3cf14a173e1f1bc649a`.

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
