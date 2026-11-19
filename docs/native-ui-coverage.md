# Windjammer native UI coverage

The catalogue covers Windjammer’s native layouts, FM-1 Instrument’s Skiff-based runtime, and the separate LOOM adapter. See [controller validation](../BUILDING.md#build-and-verify) for build checks.

The user's confirmed requirement is **Skiff layouts and interactions, app-provided content** across all 12 function buttons. Skiff must accept protocol-backed content, and the protocol must cover every native screen. Values, names, page/control labels, ranges, units, choices and musical data come from the connected app. Skiff supplies the actual screen structures, widgets and interaction conventions.

## Function buttons and page families

The canonical source is `firmware/src/params.c`, `PAGES[]` and `page_desc`, with conditional visibility in `ui.c:page_visible` and navigation in `ui.c:page_step`/`ui_input.c`. The table names native reference screens, not hard-coded connected-application content.

| Physical button | Native page/action coverage | Content the protocol must be able to represent |
|---|---|---|
| FX | FX, DELAY, REVERB, DRIVE, INSERT, SLICER, DUCK | Track sends and effect controls, bus relationships, transfer/sweep curves, slicer pattern and ducking envelope/source activity |
| SEL | SCALE, CHORD, FOLLOW | Root/scale membership, keyboard highlights, chord/voicing, follow source and resulting notes |
| ENV | ENV, ENV 2 | Envelope stages and actual curve/time behavior, selected stage/control and availability |
| LFO | LFO, LFO 2, MOD | Waveform/phase, timing/sync/polarity, routing rows, sources/destinations and amounts |
| EDIT | EDIT 1, EDIT 2, OPERATOR, ALL OPS, LANES, SOUND, LANE MIX, SLICES, SAMPLES, OP1 ENV, OP2 ENV, OP3 ENV, OP4 ENV, OP LEVEL, VOICE, VOICE 2 | Engine-specific native graph/sheet variants, operator data, row selection, lanes and mix, waveform/slice markers, sample entries, envelopes and voice allocation |
| GLO | MIXER, DELAY, REVERB, CHORUS, GLOBAL | Complete part windows, selected part, levels/pans/mute/solo, buses and global controls; GLO-held SELECT tempo gesture |
| HOME | All-tracks overview; scope | Track lanes, selection band, musical position, real playheads, scope data; HOME-held SHIFT/menu behavior |
| SAVE | PRESETS, USER, PATTERNS, PROJECT | Browsers, current selection, occupancy, metadata, actions, save/load/rename/delete state and results; held undo |
| ARP | ARP, ARP 2 | Actual arpeggiator settings and rhythmic/note preview; held PERFORM layer |
| SEQ | NOTES, FEEL, PATTERN, AUTOMATION, SONG | Visible notes with independent lengths/selection, drum lanes, cursor/range/playhead, feel, pattern properties, automation entries and song/chain windows |
| PLAY | Native transport action and modifier behavior | Authoritative play/stop/restart state, position and contextual actions; no invented PLAY page |
| REC | Native arm/record action and REC settings layer | Arm/record/count-in state, recording options, pending results and recorded-state readback; no invented REC page |

There are 47 descriptors in `PAGES[]`, excluding HOME. Engine-conditional EDIT pages and melodic/drum variants remain separate coverage cases even when they share a drawing primitive. Native subpage visibility must be reconciled with application capabilities; the existence of a protocol representation does not imply an app has the corresponding engine or operation.

## Additional screens and overlays

| Native surface | Required coverage |
|---|---|
| Held/locked layers | FX, GLO, SEL, EDIT, SEQ, REC and ARP/PERFORM maps; their contextual keys, knobs, selection, latch/lock state and hints |
| SHIFT | HOME-held control map, combinations, selection extension and release behavior |
| Menu | DISPLAY, CONTROL, AUDIO, SYSTEM and MIDI tabs, scrolling rows and selected values/actions |
| Device information/actions | ABOUT, calibration, settings reset, CPU/clock and hardware preferences; retain explicit device ownership |
| Confirmation | Native confirmation dialog, safe default selection, action state and completion/failure |
| Naming | Native name editor, cursor/character state and commit/cancel for applicable save/rename workflows |
| Undo | Native undo window and authoritative action/result |
| Takes | Take box, free-take picker, selected take and commit/cancel/results where supported |
| Transient overlays | Count-in, operation progress/status, update countdown and feedback with their correct owner |
| Contextual sheets | Scrolling operator/drum sheets, row-to-knob assignment and native SELECT/PRESETS behavior |

Hardware-local settings and firmware update/calibration data remain owned by FM-1. Application projects, presets, notes, automation, arrangement and rendering remain owned by the connected application. The screen contract must identify scope and support explicitly; drawing an application page must not borrow data from the device's standalone project.

## Local implementation and acceptance boundary

[Native UI capability 9](native-ui-wire.md) defines the base screen catalogue, native variants and bounded typed content. LOOM now uses `NativeLoomSurface` through `OpenMidiConnection`; it requires the native capability. FM-1 Instrument uses the same public native protocol with its own app namespace and stock Skiff state. Native panel capability 13 adds its stock menu. Optional capability 15 supplies the contextual header, footer and control card colors. Earlier companions can keep using their existing profiles.

| Native family | Firmware integration | App data and operation boundary |
|---|---|---|
| Header/cards/footer | Existing native painters in `ui_blocks.c`/`ui_draw.c` | Formatted labels, values, units, normalized gauges, independent transport position, record state, project badge, engine/sound footer and control colors |
| ENV/LFO/DUCK/SCOPE | Shared original curve/theme painters in `ui_native_curves.c` | App curves/markers; no local DSP substitution or fake live meters |
| Effects, global, voice, ARP, analog | Original `ui_pgraph.c` paint loops and widgets | Explicit curves/series/cells; unavailable geometry stays unavailable |
| FM/DX sheets and diagrams | `ui_fm6.c`, `ui_dx.c`, native routing/node primitives | App operator curves/topology and paired rows; bounded windows |
| HOME/MIXER | Shared native lane/strip painters | HOME lanes identify the selected sound, with the engine beneath it on an expanded lane. MIXER strips use sound, level, pan, record status and activity. LOOM windows can scroll to tracks 7 and 8. |
| SEQ | Native note bars, roll/grid, steps, feel, event/song painters | Independent note lengths, selected notes/steps, cursor, locks, lanes and playhead |
| Browsers | Native preset/user/pattern/project painters | App factory sounds and saved material; explicit load/save/rename/delete callbacks |
| Held/SHIFT layers | Existing key-cell, performance and shift widgets | App labels/actions; distinct ordinary and step-held knob assignments |
| NAME/CONFIRM/UNDO/TAKE/status | Existing dialog/name/take/count-in/update painters | App-owned text/state; unsupported actions disabled |
| Device settings | Existing native settings/menu/about/calibration/reset workflow | Device owns preferences and captures menu input; PLAY stays app-owned |

The renderer test suite exercises every descriptor, HOME/SCOPE, EDIT variants and drum views, app-content changes, poisoned standalone-state isolation, track-8 windows and native overlays. NAME palette comparisons and the existing ENV comparisons check real native pixels. The app tests cover every page for every bundled engine plus physical navigation/edit/modal and stale-context traces. These checks do not prove live display timing, every theme's visual alignment, or physical pocket operation.

LOOM reads live recording back into the project. It still reports undo/takes, live scope/voice activity, unsupported SEQ tools, and missing engine/effect preview geometry as unavailable. Available musical controls continue to edit real app state. ARP's current diagram is labelled RHYTHM PREVIEW; it does not claim to show a measured note stream. The protocol can represent richer content without a LOOM-specific firmware build.

The [paired session check](https://github.com/Osleg/FM-1-Instrument/blob/main/instrument-demo/tools/run-paired-session-check.sh) sends all 73 catalogue cases through the Android session framing and firmware mailbox to the actual painters on a simulated controller. It compares published and committed views, including app-owned overlays and device-owned handoff requests. For HOME it also compares the six track-label regions with an independent stock render before and after a same-engine preset change. Some engine-specific catalogue entries are conditional in the stock build.

The held-control regression also starts from button scans and advances the stock hold timer. It covers FX, GLO, SEL, EDIT, SEQ, ARP and HOME. HOME opens SHIFT, and SHIFT+REC opens REC SET. Held SAVE invokes undo on the underlying page. PLAY keeps its transport and layer actions.

Physical acceptance covers all function buttons, repeated-page navigation, held/locked layers, normal and step-held assignments, chord selection, stopped entry, saved-project reopening, device settings, reconnect, and audio with the phone in the pocket.

## Source anchors

- `DESIGN.md`: native visual and control rules, page traversal, layers and scope.
- `firmware/src/params.c`: families, graph kinds and all 47 page descriptors.
- `firmware/src/ui.c`: page visibility/navigation, action pages and native UI state.
- `firmware/src/ui_input.c`: physical interactions and semantic actions.
- `firmware/src/ui_draw.c` and `ui_graph.c`: native draw pipeline and graph dispatch.
- `firmware/src/ui_layer.c`, `ui_menu.c`, `menu_items.c`, `ui_name.c`, `ui_take.c`, `ui_blocks.c`: overlays, menus, naming, takes and native components.
- `docs/OPEN_PROTOCOL.md` and `docs/native-ui-wire.md`: negotiated profiles, full native encoding, bounds and ownership.

Build and hardware qualification are recorded separately; the catalogue alone is not evidence of UI parity.
