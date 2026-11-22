# Remote UI wire contract

Remote UI is an optional [wire-6 display profile](OPEN_PROTOCOL.md) for an app-supplied 240 × 240 RGB565 screen and LEDs. The host interprets raw panel input. The FM-1 Instrument firmware emulation demo is one client of this profile.

## Negotiation

REMOTE_UI is capability 16, version 1.0. A supporting FM-1 controller advertises it on discovery OFFER page 1. A Remote UI HELLO requests Instrument capability 1 and Remote UI capability 16 as required. If HELLO would accept both Native UI capability 9 and Remote UI capability 16, the controller returns UNSUPPORTED. The HELLO ACK reports the exact accepted set. Remote UI sessions reject STATE, VIEW_PART and VIEW_DELTA with UNSUPPORTED.

REMOTE_COMPACT is optional capability 17, version 1.0, on OFFER page 1. It is accepted only with capability 16. A required request for 17 without an accepted 16 causes HELLO to return `UNSUPPORTED`. With capability 16 alone, use ops 0–4. Ops 5 and 6 return `UNSUPPORTED` unless capability 17 was accepted.

## Commands and lease

REMOTE is command 13. Its payload fits the wire-6 156-byte limit and uses 7-bit values. An ACK echoes the request serial and carries `[13, result]`. The host waits for the matching ACK before sending the next REMOTE command. REMOTE serials advance independently of INPUT and HEARTBEAT serials. A forward difference of 1 through 8191 modulo 16384 is newer.

An accepted new command renews the 1500 ms lease. A retry of the most recently accepted serial and identical payload is idempotent and does not renew the lease. An older serial or reuse of the last accepted serial with different bytes returns INVALID. While PRESENT is transferring to the LCD, an exact duplicate waits for the original completion ACK and a newer command receives BUSY. An idle host can use ordinary HEARTBEAT and its matching ACK without sending a frame.

| Op | Payload | Effect |
| --- | --- | --- |
| 0 RESET | `[0]` | Invalidate tile slots and clear screen coverage, upload staging and pending redraws. Begin full replay. |
| 1 TILE | `[1, slot:u7, offset:u14, RGB565 pixels...]` | Stage a contiguous fragment of 1–50 pixels in tile slot 0–127. Each numeric RGB565 pixel is encoded low 7 bits, middle 7 bits, high 2 bits. Offsets are pixel indices 0–255. Slot becomes usable only after all 256 pixels arrive. |
| 2 BLIT | `[2, slot:u7, xTile:u7, yTile:u7]` | Copy a published 16×16 tile into the pending 240×240 framebuffer. Tile coordinates are 0–14. Mark its position covered and dirty. |
| 3 PRESENT | `[3, context:u28, LEDs:29 bytes]` | Flush dirty tiles to LCD, then ACK. After initial negotiation, RESET or RECOVER, all 225 tile positions must be covered. INPUT takes the new context after the LCD transfer finishes. |
| 4 RECOVER | `[4]` | Same full replay requirement as RESET. |
| 5 PACKED_TILE | `[5, slot:u7, offset:u14, records...]` | Stage a contiguous fragment of 1–256 decoded RGB565 pixels. The payload limit is 156 bytes. Tags 0–63 encode a literal of tag + 1 RGB565 pixels. Tags 64–127 encode tag − 63 copies of the following single RGB565 pixel. Each color uses the same three bytes as TILE. |
| 6 BLITS | `[6, (slot:u7, xTile:u7, yTile:u7)...]` | Apply 1–51 BLIT entries in order. Every entry must have valid coordinates and refer to a published slot before any entry is applied. |

TILE and PACKED_TILE can contribute to the same contiguous upload. An offset-zero fragment starts the only staging upload and marks its destination slot unavailable for placement. Later fragments must name the staging slot and begin at the next pixel offset. The whole fragment is validated before staging changes, and offset plus decoded pixel count must be at most 256. The slot becomes available when all 256 pixels have arrived.

The 29 LED bytes use the [native LED section layout](native-ui-wire.md). Four 7-byte records represent full, dim, breathing and medium brightness. Each record has a `buttons:u14` mask, a byte with green PLAY in bit 0, and a `keys:u28` mask with 27 physical key bits. The last byte selects normal (0) or low (1) dim brightness. After full replay, PRESENT can update only the context or LEDs without new BLITs.

## Display and input ownership

The firmware flushes at most one dirty tile per service pass and synchronizes LCD DMA before reusing its 512-byte scratch buffer. After the last transfer completes, it commits the INPUT context and schedules the LED update. The first completed PRESENT takes physical-input ownership and sends its ACK before enabling negotiated USB audio return.

Active sessions receive the standard 24-byte INPUT scan with buttons, keys, seven encoder deltas and the last completed PRESENT context. Scans can continue while an active screen update is in flight. The host maps the controls to its own gestures and treats the first held scan as a baseline. Session teardown returns the controller to its waiting state with local settings available. A device-owned update overlay ends the session, so the host must renegotiate and send a complete display again.

Remote UI claims 181,760 bytes from the unused buffer pool when it accepts HELLO. This covers the 115,200-byte framebuffer, 65,536-byte tile cache and two 512-byte tile buffers. HELLO returns BUSY if the workspace is occupied or too small. Session teardown waits for any active LCD transfer before releasing the workspace. Native UI sessions leave it available for other modes.
