# Experimental Nextion serial bridge

This is display-side code for a custom openHASP build. It translates the H5
Nextion protocol locally on the CrowPanel ESP32. H5 continues to use its existing
display UART; no NanoELS motion-control firmware changes or MQTT broker are
required by this adapter.

**Status:** the portable C++11 protocol core is host-tested. The openHASP adapter
has been checked against upstream source but has not been compiled into a
DIS02050A firmware image or exercised with LVGL, UART, or physical touch hardware.
It is not a ready-to-flash firmware build and does not establish board support.

## Files

- `nanoels_bridge.h`: bounded streaming parser, command translation and touch
  lifecycle. No Arduino, LVGL, JSON-library, heap-allocation or network dependency.
- `nanoels_mapping.h`: generated field, touch and page allowlists.
- `generate_mapping.py`: regenerates that header from `../mapping.json`.
- `my_custom.cpp` / `my_custom.h`: openHASP custom-code adapter for Arduino ESP32.
- `tests/`: executable host tests and layout/mapping consistency checks.

## Protocol behavior

H5 sends commands at **115200 baud, 8N1**, terminated by `FF FF FF`. The parser
accepts split frames and multiple frames in one read. Its maximum command length
is 512 bytes excluding the terminator. Oversized or damaged frames are discarded
through the next complete terminator, rather than executing their trailing text.
Unknown commands are counted and ignored; the next framed command can recover.
There is no partial-frame timeout: H5 can legitimately remain silent.

| Input | Behavior |
| --- | --- |
| Allowlisted `<name>.txt="..."` | `jsonl` update of the mapped object's `text` |
| `page 0` / `page 1` | openHASP `page 1` / `page 2` |
| `play 0,0,0` | Optional callback; the supplied adapter is silent |
| Byte `DF` in text | UTF-8 degree symbol `C2 B0` |
| `connect`, baud changes, TFT uploader commands, other commands | Rejected; no fabricated Nextion acknowledgement |

Text supports H5's audited ASCII output plus its degree byte. JSON control
characters and backslashes are escaped. Other high bytes and embedded quotes
are rejected. This is the audited H5 command subset, not a general Nextion
interpreter or UTF-8 input protocol.

The event path accepts exact `pNbN` topics from the generated allowlist. The
layout's `tag` is descriptive metadata, not permission to transmit arbitrary
component IDs. Numeric limits use their hotspot aliases. Local BACK and labels
without machine actions send no UART packets.

- `down`: one `65 PP II 01 FF FF FF` packet.
- `up`, `release`, or `lost`: one `65 PP II 00 FF FF FF` for the active widget.
- `long`, `hold`, `changed`, unknown events and duplicate `down`: no repeat.
- A different `down` releases the old key before pressing the new one.
- A stale release from a different widget cannot release a newer active key.
- Before a controller-requested page switch, release the active key with its
  **original Nextion page and ID**. A later physical release is ignored. This
  avoids depending on a hidden LVGL object to produce its final touch event.
- A local page-state notification also cancels any active press.

`cancelTouch()` is available for object reload or graceful shutdown. In the
audited openHASP revision, generic buttons suppress publication of `lost`; they
normally emit `release` afterward. Acceptance of `lost` in the core also supports
adapters that explicitly forward cancellation. Physical drag-off, hidden-object,
and reload behavior still need runtime verification. Do not reload layouts while
holding a machine control.

## Install into an openHASP source checkout

First establish a working DIS02050A board build with the display, touch, PSRAM,
and TrueType support. This adapter intentionally does not guess UART GPIOs.

1. Copy these four files into openHASP's `src/custom/`:
   `my_custom.cpp`, `my_custom.h`, `nanoels_bridge.h`, `nanoels_mapping.h`.
   If custom hooks already exist, merge their bodies; do not define them twice.
2. Enable `HASP_USE_CUSTOM` and supply these build definitions in the active
   `include/user_config_override.h`:

   ```cpp
   #define HASP_USE_CUSTOM 1
   // Define NANOELS_UART_NUMBER as an unused hardware UART (1 or 2).
   // Define NANOELS_UART_RX and NANOELS_UART_TX as verified free GPIO numbers.
   ```

   Compilation deliberately fails until all three UART definitions are set.
   UART 0 is excluded to keep console output off the H5 link. Check that the chosen
   UART is not already owned by another library and that the GPIOs do not conflict
   with LCD, touch, flash, PSRAM, SD or other board peripherals. The custom pin
   hook reserves those pins from openHASP GPIO configuration; it cannot validate
   the board's electrical assignments.
3. Build and flash using that board's openHASP environment. This repo does not
   yet supply the board environment or a verified pin selection.
4. Load `../pages.jsonl` and `../mono.ttf` as described in the parent README.
   Keep the numeric page IDs and leave the pages unnamed; the adapter expects
   `p1b48`-style state topics, not named-page topics.
5. The adapter starts its dedicated UART at 115200 8N1 and selects page 1. It
   polls up to 256 bytes each main-loop iteration with a 2048-byte RX buffer.
   Display dispatch and event handling must remain on openHASP's main thread.
   Serial debug logging must use another port.

`custom_state_subtopic` receives widget events locally even when MQTT is
disabled/disconnected. No MQTT subscription, HTTP loop or extra bridge computer
is used. The optional beep callback is unconnected pending a verified board
audio interface.

## Startup and connection limits

Bring the display and layout up before starting H5. H5 caches display values and
does not necessarily resend unchanged fields after a display-only reboot.
If the display starts late, restart H5 while the machine is idle to obtain its
initial field updates. A future explicit refresh handshake would require H5
changes; the bridge does not simulate button presses to force refreshes.

The current H5 serial protocol has no heartbeat or motion-release watchdog.
A lost UART connection or panel power failure during a held jog can lose its
release packet. This adapter cannot guarantee a stop over a broken connection.
It is not an emergency-stop implementation. A normal idle link is also silent,
so a receive timeout alone would not prove disconnection. Controller-side
watchdog/handshake work remains separate.

The adapter tracks one active touch and does not implement multi-touch,
Nextion TFT upload, raw Nextion acknowledgements, or arbitrary Nextion scripts.

## Host verification

From the NanoELS repository root:

```sh
python3 h5/openhasp/bridge/tests/run_tests.py
```

Python 3 and a C++11 compiler are required. `CXX` and `CXXFLAGS` may override
the compiler/options. For a compiler with address/undefined-behavior sanitizers:

```sh
CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 h5/openhasp/bridge/tests/run_tests.py
```

The runner checks the generated mapping against its source and checks tags
against `pages.jsonl`. It compiles the actual core with warnings treated as
errors and tests all 18 fields and 60 mapped widgets (54 Nextion page/ID pairs plus
six numeric limit aliases), chunk boundaries, concatenated frames, degree/JSON
escaping, empty and maximum-length strings, malformed frame recovery, long
presses, duplicate releases, stale releases and page-switch cancellation.
Temporary binaries are removed automatically. This does **not** compile or test
the Arduino/openHASP adapter.

After editing `../mapping.json`, regenerate and rerun:

```sh
python3 h5/openhasp/bridge/generate_mapping.py
python3 h5/openhasp/bridge/tests/run_tests.py
```

## Source references and remaining checks

Adapter hooks were inspected against openHASP commit
[`7bc826e8cf4dbfbaa1e4a702f2de02684ef41a7f`](https://github.com/HASwitchPlate/openHASP/tree/7bc826e8cf4dbfbaa1e4a702f2de02684ef41a7f):
`src/custom/my_custom_template.h`, `src/hasp/hasp_dispatch.cpp`,
`src/hasp/hasp_event.cpp`, `src/hasp/hasp_object.cpp`, and `src/main.cpp`.
H5's serial subset was rechecked against this fork's base commit
[`6e1e8ce07182827dbf8aa313edbb235fb83b14fb`](https://github.com/nufanwfk/nanoels/tree/6e1e8ce07182827dbf8aa313edbb235fb83b14fb).
The layout provenance remains in the parent README.

Remaining integration work is the DIS02050A board/pin configuration, a real
embedded build, and bench verification of display updates, touch timing,
drag-off releases, page changes, startup order, UART throughput and reset/loss
behavior. Begin with simulated H5 data and captured outgoing touch packets
before enabling machine motion. Hardware results must be recorded separately
from these host tests.
