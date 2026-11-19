#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Build and validate Windjammer controller firmware for the M-VAVE FM-1.
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT="$PWD/build/ci/windjammer"
mkdir -p "$OUT/source-attributions" build/cafa-tests
rm -f "$OUT/Windjammer-FM-1.fwsc" "$OUT/SHA256SUMS"
export BOARD=fm1 FELUCCA_FACTORY_SAMPLES=0 FELUCCA_CAFA=1 FELUCCA_UAC_OUT=1 FELUCCA_CONTROLLER=1
export FELUCCA_CDC=0 FELUCCA_CPU2=0 FELUCCA_UAC=1 FELUCCA_SLICE=0
COMMIT=$(git rev-parse HEAD)
{
    printf 'Windjammer controller firmware for M-VAVE FM-1 (FM-1_700)\n'
    printf 'Source commit: %s\n' "$COMMIT"
    printf 'Toolchain: '; "$JIELI_TOOLCHAIN/pi32v2/bin/clang" --version
    env | grep -E '^(BOARD|FELUCCA_[A-Z0-9_]+)=' | sort
    printf 'Controller profile: the companion app owns music, and FM-1 owns controls, display and USB return.\n'
} > "$OUT/config.txt"
# Never package a stale binary if linking fails.
rm -f build/felucca.{bin,elf,dis,fwsc} build/loader/ota.bin
./build.sh 2>&1 | tee "$OUT/build.log"
cp build/felucca.{bin,elf,dis} "$OUT/"
cp build/loader/ota.bin "$OUT/"
readelf -SW build/felucca.elf > "$OUT/sections.txt"
"$JIELI_TOOLCHAIN/common/bin/objdump" -t build/felucca.elf > "$OUT/symbols.txt"
cp LICENSE LICENSING.md "$OUT/"
cp -R LICENSES "$OUT/"
find assets -type f \( -iname '*attribution*' -o -iname '*license*' -o -name 'CREDITS.txt' \) -print |
    while IFS= read -r file; do
        mkdir -p "$OUT/source-attributions/$(dirname "$file")"
        cp "$file" "$OUT/source-attributions/$file"
    done
printf 'Corresponding source: %s/%s/tree/%s\n' \
    "${GITHUB_SERVER_URL:-https://github.com}" "${GITHUB_REPOSITORY:-Osleg/Windjammer}" "$COMMIT" > "$OUT/SOURCE.txt"
FAIL=0
# Collect independent checks even if one fails; never hide the nonzero result.
check() {
    local name="$1"; shift
    if "$@" 2>&1 | tee "$OUT/$name.log"; then
        echo PASS > "$OUT/$name.status"
    else
        echo FAIL > "$OUT/$name.status"
        FAIL=1
    fi
}
check controller-image python3 tests/controller_image_test.py build/felucca.dis "$OUT/symbols.txt"
check controller-audio sh tests/run_controller_audio_test.sh
check controller-native-ui sh tests/run_controller_native_ui_test.sh
check cafa-tests sh tests/run_cafa_tests.sh
check usb-tests sh tests/run_cafa_usb_tests.sh
check installer-tests python3 tests/install_test.py
check package-identity python3 - <<'PY'
from tools.fm1_install import files_ok, load_package

product, image = load_package("build/felucca.fwsc", force=False)
if product != "FM-1_700":
    raise SystemExit(f"expected FM-1_700, got {product}")
if not files_ok(image):
    raise SystemExit("controller package failed CRC validation")
print(f"Validated controller package: {product}")
PY
if (( FAIL == 0 )); then
    cp build/felucca.fwsc "$OUT/Windjammer-FM-1.fwsc"
fi
(cd "$OUT" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)

exit "$FAIL"
