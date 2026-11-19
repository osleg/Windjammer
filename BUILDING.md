# Building Windjammer

The release target is the M-VAVE FM-1 controller profile. It uses the JieLi pi32v2 toolchain on Linux x86-64, Python 3.12, and a host C compiler. WSL on Windows can run the Linux build.

## Build and verify

Use the same pinned inputs as the [GitHub Actions workflow](.github/workflows/firmware.yml). Run the following commands from the repository root after activating a Python 3.12 environment.

```sh
python3 -m pip install --require-hashes --only-binary=:all: -r tools/ci/requirements.txt
bash tools/ci/get_dependencies.sh "$PWD/.toolchains/windjammer"
set -a
. .toolchains/windjammer/paths.env
set +a
bash tools/ci/build_cafa.sh
```

`get_dependencies.sh` verifies the toolchain archive and the three bundled JieLi SDK inputs before using them. If you already have the pinned inputs, set `JIELI_TOOLCHAIN` and `AC79_SDK` to those directories.

The build produces the installable firmware at `build/felucca.fwsc`. The controller profile stages the package, checksums, license notices, build logs, and test results under `build/ci/windjammer/`. The checks cover the controller-only image, audio return, native UI, CAFA negotiation and input, and USB descriptors. See [the protocol](docs/OPEN_PROTOCOL.md) for the app integration contract.

## Install

### Browser

Back up music and settings using tools supported by your current firmware. Connect the powered FM-1 directly to your computer with a USB data cable. Open the [Windjammer web installer](https://osleg.github.io/Windjammer/webapp/installer/) in desktop Chrome or Edge, allow MIDI access, and follow the install prompts. Keep the FM-1 powered and connected until its restart is confirmed. Hold HOME and open ABOUT to check the installed version.

GitHub Pages serves the verified package from the latest regular [Windjammer release](https://github.com/Osleg/Windjammer/releases/latest). The installer checks the package against its published checksum and uses the FM-1 identity to check device compatibility.

### Command line

1. Download the Windjammer `.fwsc` file and `SHA256SUMS` from [Releases](https://github.com/Osleg/Windjammer/releases). Compare the package hash with the listed value.
2. Back up any projects, samples, or settings you want to keep using your current firmware’s tools.
3. Connect the powered FM-1 directly with a USB data cable. On Windows, run the installer with Windows Python so it can open the MIDI port.
4. With `mido` and `python-rtmidi` available in that Python environment, use the commands below. Keep the FM-1 powered and connected while it writes.

```sh
python3 tools/fm1_install.py --info
python3 tools/fm1_install.py /path/to/Windjammer-FM-1-TAG.fwsc
python3 tools/fm1_install.py --info
```

The device retains the `FM-1_700` package identity and Felucca MIDI endpoint name for compatibility. Confirm `--info` reports a running FM-1 after the restart.

An early controller build can enumerate its MIDI port while failing to answer an ordinary identity query. In that case provide the exact MIDI input name and a verified `FM-1_700` package. The installer prompts before recovery and verifies the device’s identity before serving firmware data.

```sh
python3 tools/fm1_install.py /path/to/Windjammer-FM-1-TAG.fwsc --recover-identity --port "EXACT MIDI INPUT NAME"
```

If the device cannot boot to USB, see the [FM-1-transporter recovery project](https://github.com/kurogedelic/FM-1-transporter).

## Workflows and releases

Pushes to `main` and pull requests run the controller build and checks. A version tag such as `v0.9.0` runs the same gates before publishing a firmware release with checksums and licenses. The tag identifies the corresponding source and appears on the controller’s ABOUT page. Plain builds stage artifacts for download in the workflow run. The Pages workflow updates the web installer after a successful tagged firmware release.

## Upstream development tools

The source history includes Skiff’s standalone build, SMK-37 profile, browser editor, and emulator. They are reference tools for the shared code. See the [upstream build guide](https://github.com/illuminus5/skiff/blob/main/BUILDING.md) and [SMK-37 notes](SMK37.md) for those targets.
