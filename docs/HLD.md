# Windjammer high-level design

## Products and ownership

Windjammer is the FM-1 controller firmware. It renders the Skiff-derived layouts and owns the keys, encoders, LEDs, display timing, device settings, calibration, and firmware updates. It waits for a compatible app at boot and after disconnect. HOME-held opens local settings while waiting.

The connected app owns its sequencer, synthesis, effects, presets, track/step state, and project storage. FM-1 Instrument uses the Skiff 2.5.1 musical runtime with a larger track and engine catalogue. LOOM uses its own musical model. Another app can integrate by implementing the public protocol and supplying the data and operations it supports.

| Function | Owner |
| --- | --- |
| Editable musical state, note identity, and edit results | App |
| Physical input scans, native painters, and local settings | FM-1 |
| Native page selection and synchronized edit context | Connected app and session library |
| Remote screen pixels, LED plane data and gesture interpretation | Remote UI host |
| Display cache, timed redraw, and optional USB audio output | FM-1 |
| External clock and notes routing | App after negotiated panel MIDI or its own external MIDI connection |

## Session and display

A host discovers supported versions and capabilities over USB MIDI. Discovery does not acquire ownership. One app starts a session in a negotiated display profile. In Native UI, the device validates and publishes a complete view before it authorizes context-dependent edits. Larger native pages are transferred in bounded parts. Later updates can use negotiated deltas and local partial redraw. Recovery uses a new complete view.

In Native UI, all 12 physical button families use the Skiff-derived layouts. App-owned fields include control names, formatted values, units, ranges, lists, graphs, command results, and musical state. A feature may display as unavailable if the app has no implementation. While device-owned settings are open, FM-1 handles those gestures locally. The last-drawn context identifies the edit target independently of display and playhead updates. The app retires stale targets and releases the original held notes when ownership ends.

See the [public protocol](OPEN_PROTOCOL.md), [native encodings](native-ui-wire.md), and [screen coverage](native-ui-coverage.md).

## Remote UI display profile

A compatible app can negotiate Remote UI capability 16 instead of Native UI capability 9. The host renders its layout and LED states and interprets raw button, key and encoder scans. Windjammer owns the physical controls, LCD timing, LED output and optional USB audio return. Remote UI supports any host renderer that implements the public profile.

The native-layout mode remains the default for FM-1 Instrument and the existing LOOM integration. Remote UI and optional Remote Compact are discovered separately on OFFER page 1. Existing clients using page 0 and the established native capabilities can continue negotiating as before.

The profile uses a 240 × 240 RGB565 framebuffer and 128 cached 16 × 16 tiles. A host uploads graphics, places tiles and presents the completed update with LEDs. Recovery invalidates the cache and requires all 225 screen tiles to be placed again. The controller keeps receiving input during updates in an active session. It reports the previous display context until the last LCD transfer for the new screen has finished. The first completed presentation enables input ownership, and its acknowledgement gates audio. See the [Remote UI wire contract](remote-ui-wire.md).

Hosts and controllers that negotiate Remote Compact capability 17 can encode repeated colors as runs and send several tile placements in one command. The original transfer format remains available to Remote UI peers without this capability.

The controller borrows 181,760 bytes for remote pixel buffers from the free end of the RAM pool when it accepts a Remote UI session. It releases them on teardown after the LCD transfer completes. Native UI leaves the workspace available. A future standalone single-engine synthesizer can claim the same workspace after audio and display ownership have been released. Neither feature may repurpose existing persistent flash regions without a separate storage migration design.

## FM-1 Instrument Remote UI demo

The Android demo imports a user-selected `.fwsc` package and executes it with the MIT-licensed `fm1-emu` core. The host translates controller scans into guest gestures, presents the guest LCD and LED states, and routes synthesized audio to the selected output. Application-private flash data is isolated by package SHA-256. The demo targets Android arm64 and x86-64 and ships without guest firmware. Windjammer remains the controller firmware throughout.

Protocol qualification covers negotiation, full replay and recovery, malformed and duplicate commands, cache replacement and physical-input handling, plus native-layout regressions. Emulated guest boot and audio generation can be tested with the core alone. End-to-end hardware acceptance covers simultaneous USB display, input and audio traffic, held controls, reconnects and playback latency. Playback performance varies with the guest, preset and workload.

## Audio and MIDI

The app renders audio. It can keep audio on the phone or send stereo PCM to FM-1 over USB. FM-1 Instrument can claim Windjammer’s optional direct USB PCM interface to send synth audio independently of the Android mixer. The FM-1 MASTER controls the received sound. MIDI control and audio return remain separately negotiated.

A compatible app may receive the FM-1’s external TRS MIDI input, including notes and clock. Android can also open an external MIDI interface for input and output. The FM-1’s TRS jack is an input. The app owns clock source, routing, transport policy and musical timing.

## Compatibility and validation

Firmware versions and app project formats are independent of the wire protocol. The release keeps the FM-1 loader identity and USB endpoint names so existing clients can connect. Musical engines and optional guest firmware execution belong to the app. The published controller build targets the FM-1 with a wired connection.

The firmware build checks processor and memory budgets, protocol behavior, native screen rendering, audio and USB modes. Physical acceptance adds real-device screen and gesture coverage, play/edit/save/reopen, audio timing, sustained playback, reconnects and external MIDI checks.
