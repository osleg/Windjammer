# Dynamic catalog target footprint: measured checkpoint

Measured on the JieLi pi32v2 Linux toolchain `20260730.1`, with the pinned AC79
SDK and the repository's actual FM-1 linker script. No flash/RAM limit was
increased, no standalone engine/sample/UI was removed, and USB audio remains on.

The FM-1 XIP application limit is **581,564 bytes** (`0x8dfbc`).

## Source-branch baseline

Source: `f8c5bab0dc6c5c541332dcd3dc091092511cb007`, before v5 changes.

- Default standalone build succeeds: application 579,076 bytes; data+BSS
  88,760/94,208 bytes; pool 351,128/360,448; RAM code+ramx 29,408/32,512.
- The documented CAFA v4 flags `FELUCCA_CAFA=1 FELUCCA_UAC_OUT=1
  FELUCCA_CDC=0 FELUCCA_ENGINE_BALANCE=1` **fail the real target link**:
  XIP overflows by **1,868 bytes**. The source branch is already over budget.
- The same v4 build with `FELUCCA_ENGINE_BALANCE=0` still overflows by
  **1,380 bytes**. Disabling that optional calibration alone does not solve it.

## v5 implementation checkpoint

The received and last-drawn page caches each grow from 92 to 156 bytes:
**128 bytes of fixed additional cache storage**, independent of catalog size.
The transmit scratch buffer grows by two bytes; each input scratch buffer grows
by two. There is no heap catalog, plugin binary, sample download or new audio-ISR
work on the firmware. The existing 640-byte SysEx mailbox already fits v5.

A measured v5 candidate with the full v4 flags above overflowed XIP by
**1,944 bytes**, after adding CAFA's main-loop functions to existing minsize /
noinline selection and separating its received view from compiler-merged hot
globals. Subsequent small display-context/free-text changes have host-test
coverage; the final target rebuild and further size recovery are still pending
at this checkpoint. This is not a successful firmware build and must not be
presented as a flash-ready package.

The initial v5 candidate overflowed by 2,760 bytes; the narrow main-loop/layout
change recovered 816 bytes. Broader noinline experiments on additional UI/main
functions worsened size and were discarded. Linker garbage collection alone
saved no space. ISR-reachable functions retain the existing optimization mode.

## Verified host behavior

- v1–v5 protocol tests, malformed-frame rejection and revision/context handling.
- Native renderer tests including dynamic labels/values, idle redraw suppression,
  incremental/full parity, standalone project preservation and drawn-context INPUT.
- Real Loom v5 model fixtures: 72 snapshots plus tempo endpoints and session exit.
- Native UI lint: **864 scenes, 0 layout, 0 ink, 0 alignment findings**.
- Java/C session/mailbox regression: **1,976 checks** with 34/102/170 ms firmware
  service cadence, dropped HELLO/ACK, timeout and reconnect.
- USB playback/clock-drift tests and **64 stock USB descriptor variants** pass.

Hardware sound, latency, disconnect timing and stack high-water measurement
remain untested. Existing source-branch overflow means feasibility cannot be
claimed from the fixed-size protocol design alone; the real link must fit.
