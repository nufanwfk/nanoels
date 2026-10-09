# H5 display backends

H5 includes both Nextion and openHASP wire formats. Select **Machine Config →
Display → Screen type**, then **Save and restart**. Nextion is the default for
existing installations. The stored setting is `displayType` (`0` Nextion,
`1` openHASP); an invalid saved value falls back to Nextion. Adding this key does
not change the config schema version or erase existing machine settings.

The active backend is fixed at boot, so saving config cannot switch wire
formats partway through a command. `/config` includes `activeDisplayType` and
`/status` includes `Display.type` for the running backend. Older config clients
that omit `displayType` preserve its stored value.

Both backends use H5's existing **Serial1, 115200 baud, 8N1, RX GPIO44 / TX
GPIO43**. These are H5 pins, not CrowPanel pin assignments. The adapter does not
configure any panel hardware.

## Scope and status

This change implements **the H5 side only**. It includes no openHASP firmware
extension, board port, or new display layouts. Stock openHASP's interactive
console is not the bidirectional event transport described below. The earlier
experimental panel-side Nextion translator is a different architecture and
must not be paired with this openHASP backend.

- Existing H5 machining logic and button actions are reused.
- `setText()` retains its call interface; page selection and beeps use backend
  helpers instead of constructing and reparsing Nextion command strings.
- `display_mapping.h` contains fixed mappings matching `openhasp/mapping.json`.
- Nextion commands keep their three `FF` terminators. openHASP commands end in LF.
- The receiver has one fixed 512-byte line buffer. Touch events use three
  space-separated tokens: `event <object-topic> <event-name>`. No incoming JSON
  parsing or additional library is required. Malformed/truncated lines,
  extra tokens, unknown topics and unsupported events are ignored.
- UART commands from the keypad/display tasks are serialized with a mutex.
  No display parsing or backend selection is added to the step-generation loop.

## Generic UART transport contract, version 2

The future panel extension should transport ordinary openHASP commands and
state messages. It must not contain NanoELS field or action mappings.

### H5 → panel

One ordinary openHASP command per LF-terminated line. For example:

```text
jsonl {"page":1,"id":21,"text":"0.125"}
jsonl {"page":1,"id":10,"text":"90°"}
page 2
```

The extension passes each complete line to openHASP's command dispatcher on
its main/UI thread. It should support frames up to **1023 bytes including LF**
(1022 bytes of command text); H5's 1024-byte output buffer also holds a NUL.
H5 never sends a partially encoded command if that buffer would overflow.

Text is JSON-escaped. H5's legacy Nextion degree byte `DF` becomes UTF-8 `C2 B0`
only in the openHASP backend. Logical Nextion pages 0/1 map to openHASP pages 1/2.
The JSONL layouts and fonts remain managed on the panel. There is no template
selector in H5: rearrange/resize the existing widgets while retaining page IDs,
names/IDs, and event behavior. Moving them between pages requires updating the
fixed mappings and relevant UI behavior.

### Panel → H5

Touch events use `event <object-topic> <event-name>`. Page and readiness
notifications have two tokens. Fields use exactly one ASCII space; each line
ends in LF (CRLF is also accepted):

```text
event p1b48 down
event p1b48 release
page 2
ready 1
```

This version replaces the earlier JSON event payload format; update both ends
of the link together. The earlier format is not accepted. Panel-to-H5 messages
contain no MQTT packet framing or JSON. H5-to-panel JSONL commands are unchanged.

Forward the object subtopic (`p1b48`), not a full MQTT topic. Keep pages unnamed
so openHASP uses numeric object topics. The panel extension extracts the event
name using openHASP's existing JSON library; tags and other metadata stay off
this UART. Non-event state messages remain MQTT-only, except page notifications.
Do not mix console prompts, command echoes or diagnostic logs into the UART.
MQTT may continue operating independently on the panel.

The maximum input is **512 bytes before LF**, including an optional final CR.
Oversized lines and lines containing NUL are discarded through the next LF;
their suffix is never interpreted as a new command. Unknown state topics are
ignored. There is no timeout that reinterprets a partially received line.

`down` triggers a press; `up`, `release` and `lost` trigger a release. `long`,
`hold` and `changed` do not repeat machine actions. Only the allowlisted widget
topic selects the H5 action; the wire format carries no tags. Numeric limit aliases
map to their existing hotspot actions. Local BACK has no H5 button action; its
normal page-state notification updates H5's remembered page.

H5 tracks one active openHASP widget. Duplicate presses/releases are ignored.
A new widget press first releases the previous one; an old widget's delayed
release cannot release the newer press. Page changes cancel the active touch
using its original page/action, including when H5 itself requests the change.
The future extension must forward physical release/cancellation events; this
code cannot reconstruct a missing release on an otherwise silent connection.

### Startup and layout reload

After the panel is ready to accept commands and its objects are loaded, send:

```text
ready 1
```

Send a leading LF before this notification (`\nready 1\n`) to discard any
incomplete pre-reset line in H5's receiver.
This is a **proposed generic transport notification**, not an existing stock
openHASP message. Repeat it after a panel restart or layout reload. H5 releases
any previously held panel button, restores its remembered page and forces all
18 live text fields to refresh on the next display update. It does not change
machine mode, position, spindle state or ON/OFF state. For reliable initial
ordering, send `ready 1` before forwarding touch/page events from the fresh panel.
An initial ready sent before H5's UART is listening may be lost; bring up H5
first, or resend the notification once H5 is running. H5 also makes its ordinary
initial display update without requiring a handshake.

Ready is a refresh request, **not a heartbeat or an acknowledgement of delivery**.
There is no broker, MQTT packet framing, connection watchdog, or retransmission
protocol on this link. A wire/power failure during a held jog can lose its release;
the panel reset notification only helps once communication resumes. ESTOP ends
H5's normal keypad/display tasks, so display-only restart recovery during an
ESTOP is not provided by this adapter.

## Display-specific functions

Nextion beep behavior is retained. The openHASP backend is silent because no
portable panel audio command has been established.

Nextion TFT uploads are disabled in the web UI and rejected by the server while
openHASP is active, including the multipart END/ABORT paths. Upload JSONL/fonts
through the panel's normal file interface. H5 firmware upload remains available.

## Planned work / TODO

- [ ] **Deferred JSONL layout metadata:** after the openHASP UART transport can
  emit opaque metadata from a reserved hidden object's standard `tag`, teach the
  openHASP backend to accept that optional bounded message before `ready 1`.
  Use it for the layout/skin identity and display-contract version needed when a
  future contract adds required fields or controls. Layouts and panel firmware
  that send no metadata must retain today's behavior, and older NanoELS versions
  must be able to ignore the new message. This does not put ordinary widget tags
  on the event wire and is not part of the near-term offline skin validator.

## Verification

Run the host tests from the repository root:

```sh
python3 h5/tests/run_display_tests.py
```

They require Python 3 and a C++11 compiler (`CXX`/`CXXFLAGS` can override it).
They test the protocol header and compile the actual H5 adapter functions with
a fake UART/mutex. Fixtures come from the existing layout mapping: all 18 text
fields and 60 widget mappings are exercised for both backends, along with outgoing
JSON escaping, malformed-frame recovery, duplicate/stale releases, page cancellation,
ready refresh and output suppression during TFT upload. Temporary binaries are
removed automatically.

To use address and undefined-behavior sanitizers:

```sh
CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 h5/tests/run_display_tests.py
```

Host tests cannot establish touch timing, electrical compatibility, or behavior
on the actual panel. Panel firmware integration and hardware testing remain
unfinished. See [Self-compile firmware](README.md#self-compile-firmware-with-arduino-ide)
for the full H5 build; no new libraries are needed for this adapter.

### Build validation (token event version)

Both the version 2 adapter and unchanged main (`6e1e8ce`) compiled for ESP32-S3 using
Arduino-ESP32 3.0.7, WebSockets 2.6.1 and PS2KeyAdvanced 1.0.9, with 16 MB
flash, the `default_8MB` partition scheme and OPI PSRAM.

| Build | Flash bytes | Static RAM bytes |
| --- | ---: | ---: |
| Unchanged main | 1,240,913 | 51,856 |
| Both display backends | 1,245,457 | 52,160 |
| Increase | 4,544 | 304 |

This measures the complete adapter change against main, not two separate
single-backend builds. Static RAM excludes task-stack and runtime heap usage.
Host tests also passed with address and undefined-behavior sanitizers. No
firmware has been flashed or tested on hardware as part of this validation.
