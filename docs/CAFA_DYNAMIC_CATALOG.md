# Runtime CAFA catalog surface (wire v5)

This experimental firmware is a bounded remote control surface. The companion
keeps the engine/plugin catalog, samples, parameter definitions, exact parameter
ranges and DSP. Skiff receives only the current four-control page. Adding a
companion engine does not require adding a Skiff engine enum or reflashing Skiff.
The firmware still includes its standalone engines, native UI and USB audio.
Disconnect/lease expiry returns to the untouched standalone project as before.

## Wire contract

The existing SysEx envelope, session, 14-bit revision, ACK, HELLO, heartbeat,
BYE and 1,500 ms lease rules remain. v1–v4 keep their existing lengths. v5 STATE
is exactly 156 payload bytes (167 bytes including F0/F7 and the envelope).
The existing USB receive mailbox is 640 bytes; it does not grow.

Bytes 0–91 retain the v4 layout. Page 9 is the runtime engine/control page;
pages 0–8 remain native Loom pages. On page 9:

- 20–35: engine display name, 16 ASCII bytes.
- 36–51: current bank display title, 16 ASCII bytes.
- 52–83: four control labels, 8 ASCII bytes each.
- 84–91: four normalized 14-bit control values, used for changed-control feedback.
  The companion retains the actual domains and receives relative encoder deltas.

The v5 extension is:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 92 | 2 | Zero-based bank index |
| 94 | 2 | Bank count |
| 96 | 2 | Zero-based engine index |
| 98 | 2 | Engine count |
| 100 | 48 | Four 12-byte preformatted values, including units/enum names |
| 148 | 4 | Control kinds: absent=0, numeric=1, enum=2, toggle=3, action=4 |
| 152 | 2 | View-context generation |
| 154 | 2 | Reserved, must be zero |

All integers are MIDI-safe little-endian u14. Text allows NUL or printable ASCII
32–126. Text is bounded and may be shortened on the 240-pixel display. A count
must exceed its corresponding index. Only pages 0–8 may use zero count/index
pairs; their extension can be entirely zero for compatibility. Page 3 keeps the
v4 ENV percentage bound (0–100). The complete frame is checked before any byte
of the received page, revision or lease is changed.

The engine/bank index/count pairs describe the current catalog position, not
firmware identifiers. Catalog size never changes firmware allocation: the view
and last-drawn caches each grow by 64 bytes, 128 bytes in total. Four controls
are resident regardless of the number of engines or parameters.

## Input synchronization

v5 INPUT is 22 payload bytes, not v4's 20. The final two bytes echo the generation
of the page **actually drawn**, rather than a newer page awaiting rendering.
Before the first draw, the received generation is used. The companion changes
this token on selected track/page/engine/bank/catalog/control-definition changes,
including native pages. It rejects stale-context edits/presses while processing
releases safely. STATE revision still governs snapshot ordering independently.

Metadata and displayed values are one atomic snapshot, so there is no separate
metadata packet that can be paired with a different value page. Full state ACKs,
serial ordering, timeout/reconnect and bounded nonblocking MIDI transmit remain.
No allocation, download or plugin execution is added to Skiff's audio interrupt.

## Verification

`sh tests/run_cafa_tests.sh` includes malformed/truncated v5 frames, stale
revisions, full 14-bit context wrap, arbitrary names and value strings, a
100-engine/three-bank page, all native palettes/card sizes, text-only updates,
idle redraw suppression, project isolation, and last-drawn INPUT context.

To test snapshots emitted by the actual Loom app model, run:

```
LOOM_V5_FIXTURES=/path/to/loom/build/cafa-model-vectors sh tests/run_cafa_tests.sh
sh tests/run_cafa_usb_tests.sh
```

The fixture path also runs native layout, ink and alignment lint across all
tracks, pages, palettes and card sizes. The USB suite checks playback behavior
and all 64 stock descriptor variants.

This is a proof of concept, not a hardware listening/latency qualification.
Do not flash an unverified build. See the measured target-size report for the
firmware budget and exact build flags.
