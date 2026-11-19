# Windjammer native UI wire profile

Capability 9, version 1.0, extends wire 6 with the native layouts. An app that uses these layouts requires capability 9 during negotiation. See the [ownership design](HLD.md) and [screen matrix](native-ui-coverage.md).

## Publication and transfer

An application produces one seven-bit-clean native view of at most 3072 bytes. The view is immutable during transfer. New command 9 (`VIEW_PART`) has payload `total:u14, offset:u14, bytes[1..152]`; all parts share the same session and wire revision. Offset zero starts a candidate; subsequent parts must be contiguous. Exact retries are idempotent; conflicting data, gaps, invalid total or invalid complete content are rejected. A new revision starts only at offset zero. There are two fixed 3072-byte buffers, swapped between published and staging roles. One occupies pool memory; the other occupies explicitly cleared companion scratch appended after the existing retained RAM objects. The standalone free-take log uses the existing zero-initialized RAMX region in companion builds. This preserves the pool reserve without changing retained settings/project addresses or reducing view capacity. No partial view is drawn or accepted as an edit target. Session loss clears staging and releases ownership. The existing 1500 ms progress/lease bound applies; valid incomplete chunks do not make an incomplete screen synchronized.

Core 1.0 paces parts on the 34 ms timer. When both sides select core 1.1, the controller can send PART_READY after it consumes a nonfinal fragment and releases its USB receive slot. A matching credit advances exactly one part immediately. If a credit is lost, the host repeats its last unacknowledged part after 100 ms. The controller returns the same credit for an exact retry of the last staged fragment. A full 3072-byte view needs at most 21 parts. The final part publishes only after complete validation; its successful ACK identifies command 9 and the transfer's revision. The host tracks the corresponding context as a state acknowledgement. Native views carry the active context at bytes 2–5. The [INPUT payload](OPEN_PROTOCOL.md#input-24-bytes) carries the last-drawn context. A separate capability can append the hardware MASTER reading.

Optional capability 11 (native UI idle keepalive 1.0) lets LOOM send the first screen and then only changed screens. The comparison includes labels, values, all graphics sections and the input context. After the latest complete view is acknowledged, matching idle heartbeat replies keep the connection alive. With capabilities 9 and 11 alone, a changed or unconfirmed view needs complete publication and its own acknowledgement. Older peers keep their periodic publication cadence. Device settings can redraw locally between publications.

Optional capability 12 version 1.0 adds one-frame incremental updates against the last acknowledged native revision. Use command 10 for small changes on the same screen and variant with the same encoded length. The host recovers from a missing or rejected delta acknowledgement by sending the latest complete page. The [public wire specification](OPEN_PROTOCOL.md#incremental-native-views) defines the bytes, negotiation and recovery behavior.

FM-1 uses its existing staging buffer to validate each update before publishing it. Its native renderer accumulates affected regions until the next paint. Ordinary pages separately track the header, four cards, graph and footer. Layout and palette changes trigger full paint. Overlapping overlays use their own painter.

## Native view header (256 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | Kind = 7 |
| 1 | 1 | Flags: pending=1, error=2, unavailable=4, device-owned=8; other bits zero |
| 2 | 4 | Context u28 |
| 6 | 1 | Screen ID from the catalogue below |
| 7 | 1 | Variant: 0 normal, 1 drum; EDIT-only variants: 2 analog, 3 FM operators, 4 DRUM-X sheet, 5 sample waveform, 6 drum lanes |
| 8 | 1 | Physical family: FX=0, SEL=1, ENV=2, LFO=3, EDIT=4, GLO=5, HOME=6, SAVE=7, ARP=8, SEQ=9, PLAY=10, REC=11 |
| 9 | 2 | Page index u14, zero based |
| 11 | 2 | Page count u14, positive |
| 13 | 2 | Selected part index u14 |
| 15 | 2 | Part count u14, positive |
| 17 | 2 | First visible step/item u14 |
| 19 | 1 | Visible step count 0–16 |
| 20 | 2 | Edit cursor u14; 16383 unavailable |
| 22 | 2 | Actual playhead u14; 16383 unavailable |
| 24 | 2 | Selected row u14; 16383 unavailable |
| 26 | 2 | First visible row u14 |
| 28 | 2 | Total row count u14 |
| 30 | 2 | Tempo in tenths u14; zero unavailable |
| 32 | 1 | Transport flags: playing=1, recording=2, step-entry armed=4 |
| 33 | 24 | Title, printable ASCII padded with spaces |
| 57 | 24 | Subtitle, printable ASCII padded with spaces |
| 81 | 31 | Footer/hint, printable ASCII padded with spaces |
| 112 | 144 | Four 36-byte controls |

Each control is `label[12], value[14], unit[4], normalized:u14, kind:u7, flags:u7, icon:u7, reserved:u7`. Kinds: absent=0, numeric=1, enum=2, toggle=3, action=4. Flags: enabled=1, selected/hot=2, read-only=4, bipolar=8. Reserved is zero. Absent controls have blank text and all numeric fields zero. Icons use the profile's named native icon mapping; zero is none. Display strings are application formatted; normalization only locates the native gauge.

## Typed panel sections

After the header, sections use `tag:u7, length:u14, payload[length]`. Reject duplicate tags, unknown tags, invalid counts/ranges and incompatible screen/content combinations. A view can contain several compatible sections but its total remains bounded. Empty content is explicit; an unavailable screen must not read standalone data to fill it.

- **1 Rows:** count u7 (0–16), followed by 48-byte rows: `label[16], value[16], detail[8], flags:u7, a:u14, b:u14, c:u14, reserved:u7`. Flags: selected=1, active=2, muted=4, solo=8, disabled=16, action=32, locked=64. Numeric fields are renderer-specific normalized positions/counts; meanings are defined per native screen. Lists, mixer/operator/lane sheets, routes, song and menu windows use native row painters with their own screen semantics.
- **2 Notes:** count u14 (0–256), followed by seven-byte notes: `pitch:u7, start:u14, length:u14, flags:u7, velocity:u7`. Start and length are 1/128 step display units relative to the visible window. Length is positive. Flags: selected=1, continues-left=2, continues-right=4, locked=8, skipped=16, accent=32. These are display positions, not a lossy source for note edits. Editing uses authoritative app values and revision-scoped note addresses.
- **3 Curve:** count u14 (0–256), followed by `x:u14,y:u14` normalized points. Actual app/DSP curve, waveform or transfer data; no substitute Skiff DSP law. Native screen structure decides how it is plotted.
- **4 Markers:** count u7 (0–32), followed by `position:u14, value:u14, flags:u7, label[8]`. Envelope stage, slice, phase and diagram marker semantics belong to the selected native screen.
- **5 Cells:** count u14 (0–256), followed by `value:u14, flags:u7, label[4]`. Flags: active=1, selected=2, playing=4, locked=8, skipped=16, accent=32, disabled=64. Step/feel/slicer grids and bounded overview lanes use native cell painters.
- **6 Keyboard:** 19 seven-bit bytes cover MIDI notes 0–127; unused high bits zero, followed by root:u7 (0–127). Actual scale/chord/follow highlights.
- **8 Series:** count u7 (0–6); each series has `id:u7, flags:u7, point-count:u7`, followed by `x:u14,y:u14,heat:u14` points. IDs 0–5 are unique. FM screens permit at most 12 points per series; other supported screens permit 127, within the complete-view limit. Flags: selected=1, carrier=2, active=4. Coordinates are app display data; FM heat is spectral contribution, not an instruction to simulate Skiff DSP. Allowed for EDIT 1/2, OPERATOR, ALL OPS, OP1–4 ENV, OP LEVEL and effects 6–9/32–34, with roles below.
- **9 Operator edges:** count u7 (0–36), followed by `from:u7,to:u7,flags:u7,strength:u14`; operator IDs 0–5, flags enabled=1. These encode the actual application algorithm; carrier/active masks alone are not a topology. Allowed for EDIT 1/2, OPERATOR, ALL OPS, OP1–4 ENV and OP LEVEL.
- **7 Text:** count u7 (0–16), followed by fixed 24-byte printable lines. Native layer cells, dialog/name/take/status text; no arbitrary drawing commands or coordinates.
- **13 Transient box:** fixed 67-byte payload `track:u7, kind:u7, title[24], detail[40], actions:u7`. Kind 0 is TAKE, kind 1 is UNDO/REDO. Actions bit 0 enables OCT- BACK and bit 1 enables OCT+ REDO. Text is printable ASCII padded with spaces. This box is allowed on app-owned graph screens 0–48 except MIXER, over the graph already described by that view. Its presence follows stock `ui.take_t`; the host publishes a view without the section when the timer ends, so the underlying graph is repainted. It does not change the edit target or carry raw pixels.

## Stable screen catalogue

IDs 0–46 follow the documented PAGES order captured in `params.c` at the branch baseline; the public codec uses named constants and an explicit firmware lookup, not runtime table indices. IDs are append-only. They identify native presentation templates, not LOOM engines or parameters.

`0 ENV, 1 ENV2, 2 LFO, 3 LFO2, 4 MOD, 5 FX, 6 FX_DELAY, 7 FX_REVERB, 8 DRIVE, 9 INSERT, 10 SLICER, 11 DUCK, 12 SCALE, 13 CHORD, 14 FOLLOW, 15 EDIT1, 16 EDIT2, 17 OPERATOR, 18 ALL_OPS, 19 LANES, 20 SOUND, 21 LANE_MIX, 22 SLICES, 23 SAMPLES, 24 OP1_ENV, 25 OP2_ENV, 26 OP3_ENV, 27 OP4_ENV, 28 OP_LEVEL, 29 VOICE, 30 VOICE2, 31 MIXER, 32 DELAY, 33 REVERB, 34 CHORUS, 35 GLOBAL, 36 PRESETS, 37 USER, 38 PATTERNS, 39 PROJECT, 40 ARP, 41 ARP2, 42 NOTES, 43 FEEL, 44 PATTERN, 45 AUTOMATION, 46 SONG`.

Additional IDs: `47 HOME, 48 SCOPE, 49 FX_LAYER, 50 GLO_LAYER, 51 SEL_LAYER, 52 EDIT_LAYER, 53 SEQ_LAYER, 54 REC_LAYER, 55 ARP_LAYER, 56 SHIFT, 57 MENU_DISPLAY, 58 MENU_CONTROL, 59 MENU_AUDIO, 60 MENU_SYSTEM, 61 MENU_MIDI, 62 ABOUT, 63 CONFIRM, 64 NAME, 65 UNDO, 66 TAKE, 67 TAKE_PICKER, 68 COUNT_IN, 69 UPDATE, 70 CALIBRATION, 71 RESET_SETTINGS`.

Screen identity chooses existing native layout and widgets. Layer maps and modal fields must be interpreted by the shared Skiff workflow, with destructive actions retaining native confirmation behavior. Device-owned settings/diagnostics stay authoritative on FM-1. Screens 57–62 and 70–71 require the device-owned flag and a header-only publication. On the first new request context the firmware opens its native settings UI; it then owns menu cursor, tab, preference, ABOUT and reset-confirmation actions. The base INPUT scan stays 24 bytes; byte 5 bit 6 is `DEVICE_MENU_ACTIVE`, permitted only after capability 9 negotiation. Consumed device inputs are cleared, while PLAY remains a host transport action. The application releases held musical input and freezes its menu publication while this flag is set. The transition back to clear returns to the app's HOME context. Repeated publication of a closed request context must not reopen the menu. Disconnect retires the menu cursor. Hardware calibration and USB re-enumeration are device operations and can interrupt the link; they cannot authorize writes to the app's projects.

## Screen-specific windows

For track windows, header offsets 13 and 15 carry the selected absolute part and total parts. On HOME, SCOPE, MIXER and track layers that use ROWS or CELLS for the track strip, offset 26 is the first visible part. Submit up to six consecutive tracks in the native layout. The application updates the window when the selected part moves beyond it and interprets physical input against the window that was drawn.

On PATTERN, offset 17 is the first absolute step in the grid. CELLS supplies up to 64 consecutive steps, with each row drawn in groups of 16. Shorter windows supply only their existing steps. Cursor and playhead remain absolute. NOTES and FEEL retain their 16-step view and absolute origin. The application owns the full pattern and its edit limits.

SONG may publish up to six ROWS aligned with its CELLS grid. Row field `a` is the absolute track index, and each row has 16 consecutive entry cells. The header's selected part identifies the highlighted track. The separate row fields in the header retain their entry-selection meaning. On the selected track, a nonzero `c` marks the number of consecutive COPY+ preview tiles starting at zero-based entry `b`, with `b + c` at most 16. Set `c` to zero when there is no preview. Each preview cell uses flag 1 and carries the proposed pattern number or rest, its repeats, assigned-sound flag and chain flags. The renderer outlines these tiles in text color. Preview data has display scope only.

The regular page header identifies the physical family and the current family page. Native sheet windows use cursor field 20 for the window within the sheet and selected-row field 24 for the operator/lane. Window controls must be captured with the displayed context, including independent step-held assignments.

- FM sheets use operator ROWS for app labels/ratios; `a,b` are native topology grid column/row (0–5), `c` normalized level. SERIES contains the envelope display data; EDGES contains the engine's real algorithm. Text lines 0/1 are graph captions. The OPERATOR windows are tone, envelope 1/2, keyboard scaling and more; ALL OPS windows are algorithm, pitch 1/2, LFO and depth.
- SOUND's paired sheet uses seven ROWS: label, A value, B value in detail, normalized A/B in `a,b`; eight CELLS are lane tabs. The application selects the native variant only when its source data matches that sheet.
- FX layer uses fourteen ROWS for native effect cells and six CELLS for mute windows. FX row flags mark held (1), running (2), unavailable (16), or waiting (32). GLO uses six part ROWS and three CELLS for the native wide actions. SEL, EDIT and SEQ use bounded app ROWS with explicit selection or unavailable flags; SEQ lane cells use CELLS. REC uses four ROWS and nine TEXT choice labels.
- PERFORM uses up to six part ROWS, sixteen CELLS per visible part, then sixteen scene CELLS. The row label is the displayed track number. Muted and locked flag the current mute and a queued mute. The first row's `a` is 1 in scene key mode and 0 in track key mode. Occupied/current/staged cells use active/selected/playing. The four control cards carry the temporary offsets, with 8191 as neutral and the bipolar flag enabling the active throw color away from neutral. The title includes the launch mode. Two spaces in the footer separate its left and right hints. SHIFT uses twelve ROWS with a key label and action value. Row flags distinguish ordinary actions (0), page-specific actions (1), and SHIFT combinations (2). The controls and subtitle describe the page open beneath the SHIFT map.
- Held-layer titles supply the heading text. A layer subtitle of `LOCKED` enables the native lock indicator. These layers retain the same screen ID while held and locked.
- NAME uses TEXT 0 for the entered name, TEXT 1 for its placeholder; row 0 `a,b,c` are numeric-mode, cycling-key and tap-index. Cursor is a character position 0–12.
- CONFIRM uses TEXT question, detail, consequence hint, cancel label and action label. TAKE and UNDO use title/detail plus back/redo labels; row flags identify enabled history directions.
- COUNT-IN uses four CELLS with app labels and current/future state. UPDATE uses the header cursor as displayed remaining count. Neither display authorizes a firmware update.

## Completion gate

The codec's catalogue alone is not UI completion. Each screen must use its actual native painter and source-appropriate data, with renderer parity/isolation tests and input traces. The complete screen matrix remains the acceptance list. No new build is qualified until the schema, native provider, controller, LOOM adapter, device-owned handoff and all required checks agree.

## Effect and engine display series

SERIES is also accepted by effects 6–9 and 32–34, and the existing EDIT pages. FM operator sheets, FM operator envelopes, OP LEVEL, and FM-variant EDIT retain a 12-point limit per series; other series permit up to 127 points within the same 3072-byte view bound. The original CURVE is the primary-series fallback. Series 0 is the primary waveform/response; series 1 is OSC’s secondary waveform, CHORUS’s feedback trace, DRIVE/INSERT’s waveform inset, or REVERB’s early reflections. DELAY series 0 supplies echo positions and amplitudes; FLT series 0 supplies its response. Missing traces remain empty/unavailable; firmware does not apply Skiff DSP laws to application parameter values.

SCOPE additionally accepts ROWS for app-provided per-part levels and muted/active state. SEL_LAYER accepts ROWS, KEYBOARD and TEXT for the scale grid, keyboard/root and labels. App settings such as track volume must not be represented as measured output levels.

## Native panel and external MIDI extensions

Capability 13 version 1.0 (`NATIVE_PANEL`) requires Native UI capability 9. It adds screen 72, an app-owned stock menu in the HOME family. PAGE is the tab index (display, control, audio, system, MIDI) and PAGE_COUNT is five. FIRST_ROW, ROW_COUNT and SELECTED_ROW use absolute zero-based positions within the tab. ROWS contains one to six visible rows including the selection. Each row supplies its label and value, selected, active, disabled and action flags, and its current and maximum setting. Physical scans still go to the app. Device-owned preference screens continue to use their existing IDs.

Any app-owned native view can carry display preferences in tag 11. Its first two bytes give a stock base palette (0–5) and flags for large type, disabled animation, custom colours and app-applied master gain (bits 0–3). The custom flag adds eleven RGB565 colours, each in three seven-bit limbs, least significant first. The first app-owned page chooses master gain ownership for the session. When bit 3 is set, the app applies master gain and the controller plays USB return at unity. Other sessions use the physical gain on FM-1’s return. Capability 13 extends INPUT from 24 to 26 bytes with the smoothed physical master gain (u14, 0–4096) at offset 24. FM-1 sends its initial value and subsequent changes. The app mutes new sessions until receiving the first gain.

Any app-owned native view may end with one optional LED section (tag 10, length 29) after negotiation. Its four seven-byte records give the full, dim, breathing and medium LED planes. Each starts with a seven-bit-limb button mask (three bytes, 14 labelled buttons plus bit 14 for green PLAY) and key mask (four bytes, 27 physical keys). Byte 28 selects the normal (0) or low (1) dim brightness. Both peers reject unsupported or malformed sections before committing a new view. LED-only changes update the matrix without painting the LCD.

ARP and ARP 2 optionally use section 12 with capability 13. Its header carries the native mode (0–14 in stock display order), octave count (1–4), gate, swing, probability, example flag, input note count and expanded row count. The remaining bytes are one MIDI note per displayed row (1–65). The native painter uses the same ordered rows for a single note, a chord or its idle example.

Capability 14 version 1.0 (`PANEL_MIDI`) enables standard MIDI from FM-1's TRS input onto its USB MIDI connection during a synchronized session. Channel voice, song position and realtime clock/transport retain their standard MIDI encoding. Channel voice packets are sent between complete companion SysEx frames. Realtime messages can also appear inside SysEx. The app receives the musical stream separately from control scans and labels its source TRS. Its independently selected Android MIDI device handles external USB input and output.

## Screen field bindings

| Screen | Binding used by the native painter |
| --- | --- |
| HOME | A row's label is the selected sound, kit or patch. Its detail is the engine. Compact lanes show the sound. Expanded lanes also show the engine. Row fields `a` and `b` are the lane's first visible step and its absolute playhead. An idle playhead is 16383. |
| MIXER | The row label names the sound. Fields `a` and `b` are normalized level and pan. Optional CELLS align with visible rows. A cell's value is output meter level, its active flag is record-arm and its playing flag is recording. Apps may omit CELLS when they have no live meter or recording data. |
| PRESETS, USER, PROJECT | The row label names the sound or project. Detail holds the engine abbreviation or slot label. Selected marks the visible selection, including in a wrapped preset list. In the sound browsers active marks a favorite, and disabled marks an unavailable entry. |
| PATTERNS | Each cell's value is the pattern reference count. Active marks a stored pattern. The selected row in the header gives the picked pattern index. |
| SONG | Track-major CELLS contain 16 entries per track. The value is the repeat count. Flags 1, 2, 4, 8, 16, 32, and 64 mark a stored or preview tile, the picked slot, the current entry, an assigned sound, a queued entry, a chain member, and a chain start. Label `-` denotes a rest. An empty label without flag 1 denotes an empty slot. |
| NOTES | The melodic roll's optional row labelled `ROLL VIEW V1` supplies the scale name in its value and the absolute selection endpoints in `a,b`. Its visible step CELLS use the locked flag for automation locks. Notes crossing the window use the existing continuation flags and lengths clipped to the window. |
| LFO and LFO 2 | The first marker's position is the running phase cursor within the published curve window. |
| EDIT held | TEXT line 0 supplies the currently selected sound or kit. ROWS and CELLS carry the engine choices and kit lane states. |
| Drawbar EDIT | In the normal EDIT variant, nine ROWS with `DRAWBARS` in the first row's detail select the native drawbar graph. Each label gives the footage and `a` gives the normalized drawbar level. |
| VOICE | The first marker's position is the normalized glide guide width. The glide line comes from CURVE. |
| VOICE 2 | The subtitle gives the allocation caption. The optional first row detail `DETUNE` identifies rows with detune position in `a`. Row flags 1, 2 and 4 indicate next voice, sounding and unison. The earlier CURVE encoding remains accepted. |
| SAMPLES, SLICES | CURVE can carry both waveform extrema as consecutive points at the same horizontal position. A single ordinate per position remains a supported trace. MARKERS supply loop or slice locations for the corresponding view. For SLICES the selected marker sets flag 1 and the optional first row provides duration in its value and source in its detail. |

Optional capability 15 v1.0 (`NATIVE_CHROME`) requires capability 13. It adds section 14 on regular screens 0–48 for fields surrounding the graph. Its fixed payload is 56 bytes.

Held layers and SHIFT carry the transport cursor in the base header’s `playhead` field. Use `16383` when stopped. Publishing the first step of each displayed beat keeps the header current without republishing it for every sequencer step.

| Offset | Encoding | Meaning |
| ---: | --- | --- |
| 0 | u14 | Selected track's transport step, independent of the graph cursor or preview. 16383 means unavailable. |
| 2 | u7 | Record indicator. 0 off, 1 another track, 2 selected track. |
| 3 | ASCII[8] | Octave indicator, space-padded. |
| 11 | ASCII[1] | Project badge, space if absent. |
| 12 | u7 | Project dirty flag, 0 or 1. |
| 13 | ASCII[16] | Context marker in the header. |
| 29 | ASCII[8] | Selected engine for the footer. |
| 37 | ASCII[16] | Selected sound for the footer. |
| 53, 54 | u7 each | Two control color roles per byte, at bit offsets 0 and 3. Each role is 0 default, 1 accent, 2 mid, or 3 theme. Unused bits are zero. |
| 55 | u7 | Reserved, zero. |

An app can publish these fields using any namespace. The shared host omits this section before computing a full view or delta for a peer that has not accepted capability 15. FM-1 validates both forms before publishing. The battery level comes from the controller.

### CHORD and FOLLOW stock detail rows

Screens 13 CHORD and 14 FOLLOW use the 20-byte KEYBOARD section. Bytes 0–18 encode MIDI notes 0–127 in seven-bit limbs. Byte 19 holds the root pitch class. An optional ROWS section selects the stock detail painter only when row 0's padded label is exactly `CHORD VIEW V1` or `FOLLOW VIEW V1` for the matching screen. Other ROWS and KEYBOARD-only views keep the earlier keyboard presentation. This discriminator describes a view shape and does not identify an application.

The typed ROWS section has one row for a voiced chord or follow source (mode 3), and two rows for other states. Row 0 uses its value[16] for the stock short caption, detail[8] for the source label, byte 40 for source track plus one (zero when absent), u14 at byte 41 for mode, u14 at byte 43 for the first visible C note, and u14 at byte 45 for the highlighted FOLLOW note. FOLLOW's highlighted note is either an absolute MIDI note or 128 plus a pitch class. CHORD uses KEYBOARD byte 19 for root highlighting. Mode 0 means CHORD keys or FOLLOW off, mode 1 means a kit, mode 2 means no FOLLOW chord yet, mode 3 means voiced keys, and mode 4 means CHORD strum. Mode 4 is invalid for FOLLOW. The view subtitle is the chord name for voiced modes and the first stock panel message for empty modes.

For modes 0–2, row 1's value[16] and detail[8] form one 24-byte second panel message. For mode 4, row 1 has label `STRUM`, u14 strum duration in milliseconds at byte 41 (0–100), and u14 direction at byte 43 (0 down, 1 up, 2 alternating). The renderer uses the stock strum painter with notes from KEYBOARD and those timing fields. Empty or malformed detail rows do not authorize changes to the app's sound or sequence; the earlier keyboard painter remains available for untyped views.
