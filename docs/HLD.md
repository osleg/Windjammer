# Windjammer high-level design

## Products and ownership

Windjammer is the FM-1 controller firmware. It renders the Skiff-derived layouts and owns the keys, encoders, LEDs, display timing, device settings, calibration, and firmware updates. It waits for a compatible app at boot and after disconnect. HOME-held opens local settings while waiting.

The connected app owns its sequencer, synthesis, effects, presets, track/step state, and project storage. FM-1 Instrument uses the Skiff 2.5.1 musical runtime with a larger track and engine catalogue. LOOM uses its own musical model. Another app can integrate by implementing the public protocol and supplying the data and operations it supports.

| Function | Owner |
| --- | --- |
| Editable musical state, note identity, and edit results | App |
| Physical input scans, native painters, and local settings | FM-1 |
| Page selection and synchronized edit context | Connected app and session library |
| Display cache, timed redraw, and optional USB audio output | FM-1 |
| External clock and notes routing | App after negotiated panel MIDI or its own external MIDI connection |

## Session and display

A host discovers supported versions and capabilities over USB MIDI. Discovery does not acquire ownership. One app starts a session and publishes a complete view before it can receive context-dependent edits. The device validates and atomically publishes the view. Larger pages are transferred in bounded parts. Later updates can use negotiated deltas and local partial redraw; recovery uses a new complete view.

All 12 physical button families use the native layouts. App-owned fields include control names, formatted values, units, ranges, lists, graphs, command results, and musical state. A feature may display as unavailable if the app has no implementation. While device-owned settings are open, FM-1 handles those gestures locally. The last-drawn context identifies the edit target independently of display and playhead updates. The app retires stale targets and releases the original held notes when ownership ends.

See the [public protocol](OPEN_PROTOCOL.md), [native encodings](native-ui-wire.md), and [screen coverage](native-ui-coverage.md).

## Audio and MIDI

The app renders audio. It can keep audio on the phone or send stereo PCM to FM-1 over USB. FM-1 Instrument can claim Windjammer’s optional direct USB PCM interface to send synth audio independently of the Android mixer. The FM-1 MASTER controls the received sound. MIDI control and audio return remain separately negotiated.

A compatible app may receive the FM-1’s external TRS MIDI input, including notes and clock. Android can also open an external MIDI interface for input and output. The FM-1’s TRS jack is an input. The app owns clock source, routing, transport policy and musical timing.

## Compatibility and validation

Firmware versions and app project formats are independent of the wire protocol. The release keeps the FM-1 loader identity and USB endpoint names so existing clients can connect. New musical engines belong to the app. The published controller build targets the FM-1 with a wired connection.

The firmware build checks processor and memory budgets, protocol behavior, native screen rendering, audio and USB modes. Physical acceptance adds real-device screen and gesture coverage, play/edit/save/reopen, audio timing, sustained playback, reconnects and external MIDI checks.
