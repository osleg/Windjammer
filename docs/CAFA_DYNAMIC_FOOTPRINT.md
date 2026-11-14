# Dynamic catalog target footprint: fitting sample-light build

Measured on the JieLi pi32v2 Linux toolchain `20260730.1`, with the pinned AC79
SDK and the repository's actual FM-1 linker script. No flash/RAM limit was
increased. The fitting configuration omits factory instrument samples, as
authorized, while retaining standalone engines, UI, user sample slots, SLICE
BREAK/FUNK loops and USB audio.

The FM-1 XIP application limit is **581,564 bytes** (`0x8dfbc`).

## Successful v5 companion build

Run `tools/ci/build_cafa.sh` with `JIELI_TOOLCHAIN` and `AC79_SDK` set. It
selects FM-1, CAFA, USB playback/recording, engine balance and SLICE, disables
CDC, and sets **FELUCCA_FACTORY_SAMPLES=0**. That last flag is essential: the
historical overflowing measurements below left factory instrument samples on.
The existing sample omission option solves the flash limit. Two hot buffers
are also kept in separate BSS input sections to restore the original target
addressing cost; no DSP math, UI behavior or memory limits change.

Measured on 2026-11-14 after the hot-buffer section fix:

- Application: **495,856 / 581,564 bytes**, **85,708 bytes free**.
- data+BSS: **91,324 / 94,208 bytes**, 2,884 bytes free.
- pool: **351,128 / 360,448 bytes**, 9,320 bytes free.
- RAM code+ramx: **29,752 / 32,512 bytes**, 2,760 bytes free.
- Loader: **7,273 bytes**. Development package: **610,429 bytes** (the package
  includes the loader and packaging data; its size is not the app XIP budget).
- Factory instrument data removed: PIANO/FLUTE/SAX and their aliases. Their
  existing preset/set IDs stay reserved, preserving USR1–3 numbering. They use
  the existing no-sample fallback; phone audio and user uploads remain available.
- Built-in SLICE BREAK/FUNK ADPCM remains: **37,376 bytes**.

The dedicated **CAFA v5 companion** workflow runs on `cafa/**` pushes and
archives development binaries, licences, logs, checksums and source provenance.
It neither releases nor flashes firmware. Hardware sound, USB latency, recovery
and stack high-water checks remain unqualified.

### Target CPU gate: passes without changed budgets

The first sample-light build (`b0f6e24`, 495,884 bytes) failed the unchanged
`tests/target_budget.txt` gate: audio ISR cost 1,399 versus budget 1,266 (+11%)
and `strum_tick` 155 versus 139 (+12%), with a +10% allowed threshold.
An exact sample-light v4 build measured 1,390 and 139 respectively and passed.

The v5 catalog globals changed JieLi LLVM's global merging: the hot `sq` queue
and `wet` buffer acquired larger offsets behind unrelated globals, adding
address calculations and register pressure. Putting those two buffers in
`.bss.strum` and `.bss.fx_wet` input sections restores **ISR 1,390** and
**strum_tick 139**, exactly the v4 costs. Both still live in the original BSS
RAM region and are initialized by the same startup code. No DSP algorithm,
loop iteration count, budget entry or tolerance was changed. The complete
target-cost check now passes. These are static estimates, not hardware cycles.

### Broad host suite caveats

The stock aggregate suite was also run against the sample-light generated
assets. It is **not green**: its stock UI/SLICE expectations assume factory
samples exist, and 18 of 176 audio golden renders change (all SAMPLE/GRAIN
factory-material presets). The other 158 goldens remain unchanged, with no
reported audio-health, voice/routing failures or crashes. Golden files were
not regenerated. LeakSanitizer cannot run under this environment's ptrace;
that sanitizer stage is not a pass. The dedicated companion suites and target
budget check are the configuration-specific CI gates. Hardware validation
remains required.

## Historical factory-samples-on source baseline

Source: `f8c5bab0dc6c5c541332dcd3dc091092511cb007`, before v5 changes.

- Default standalone build succeeds: application 579,076 bytes; data+BSS
  88,760/94,208 bytes; pool 351,128/360,448; RAM code+ramx 29,408/32,512.
- The CAFA v4 flags `FELUCCA_CAFA=1 FELUCCA_UAC_OUT=1
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

The final checkpoint `a303f4f` was rebuilt with the full v4 flags above and
**fails the real target link by 2,108 bytes**: the image requires
**583,672/581,564 bytes**. This is 240 bytes above the unmodified v4 build with
the same flags, after the narrow existing-code optimization. It is not a
successful firmware build and must not be presented as a flash-ready package.

A diagnostic-only ELF retained from the failed link (without changing any
linker limit, never packaged) measures:

- data+BSS: **91,324/94,208 bytes**, 2,884 bytes remaining;
- pool: **351,128/360,448 bytes**, 9,320 bytes remaining;
- RAM code+ramx: **29,752/32,512 bytes**, 2,760 bytes remaining.

An intermediate candidate overflowed by 1,944 bytes before the last display
context/free-text changes. The final 2,108-byte result supersedes that figure.

The initial v5 candidate overflowed by 2,760 bytes; the narrow main-loop/layout
change recovered 816 bytes in the intermediate candidate. Broader noinline experiments on additional UI/main
functions worsened size and were discarded. Linker garbage collection alone
saved no space. ISR-reachable functions retain the existing optimization mode.

## Verified host behavior

- v1–v5 protocol tests, malformed-frame rejection and revision/context handling,
  including AddressSanitizer/UBSan (leak detection disabled under ptrace).
- Native renderer tests including dynamic labels/values, idle redraw suppression,
  incremental/full parity, standalone project preservation and drawn-context INPUT.
- Real Loom v5 model fixtures: 72 snapshots plus tempo endpoints and session exit.
- Native UI lint: **864 scenes, 0 layout, 0 ink, 0 alignment findings**.
- Java/C session/mailbox regression: **1,976 checks** with 34/102/170 ms firmware
  service cadence, dropped HELLO/ACK, timeout and reconnect.
- USB playback/clock-drift tests and **64 stock USB descriptor variants** pass.

Hardware sound, latency, disconnect timing and stack high-water measurement
remain untested. The sample-light configuration above passes the real link; the factory-samples-on
configuration remains over budget. Neither result is hardware qualification.
