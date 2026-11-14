#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Reproducible v5 companion: keep engines/UI/audio, omit factory instrument PCM.
# Development artifacts only. This script never flashes or publishes a release.
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT="$PWD/build/ci/cafa"
mkdir -p "$OUT"
export BOARD=fm1 FELUCCA_FACTORY_SAMPLES=0 FELUCCA_CAFA=1 FELUCCA_UAC_OUT=1
export FELUCCA_CDC=0 FELUCCA_ENGINE_BALANCE=1 FELUCCA_UAC=1 FELUCCA_SLICE=1
{
    printf 'Source commit: '; git rev-parse HEAD
    printf 'Toolchain: '; "$JIELI_TOOLCHAIN/pi32v2/bin/clang" --version
    env | grep -E '^(BOARD|FELUCCA_[A-Z0-9_]+)=' | sort
    printf 'Experimental only: no hardware listening, latency or recovery qualification.\n'
    printf 'Factory instrument samples omitted; user slots and SLICE loops retained.\n'
} > "$OUT/config.txt"
# Never package a stale binary if linking fails.
rm -f build/felucca.{bin,elf,dis,fwsc} build/loader/ota.bin
./build.sh 2>&1 | tee "$OUT/build.log"
cp build/felucca.{bin,elf,dis,fwsc} "$OUT/"
cp build/loader/ota.bin "$OUT/"
readelf -SW build/felucca.elf > "$OUT/sections.txt"
cp LICENSE LICENSING.md docs/CAFA_DYNAMIC_CATALOG.md docs/CAFA_DYNAMIC_FOOTPRINT.md "$OUT/"
cp -R LICENSES "$OUT/"
find assets -type f \( -iname '*attribution*' -o -iname '*license*' -o -name 'CREDITS.txt' \) -print |
    while IFS= read -r file; do
        mkdir -p "$OUT/source-attributions/$(dirname "$file")"
        cp "$file" "$OUT/source-attributions/$file"
    done
printf 'Corresponding source: https://github.com/%s/tree/%s\n' \
    "${GITHUB_REPOSITORY:-osleg/skiff}" "$(git rev-parse HEAD)" > "$OUT/SOURCE.txt"
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
check target-budget python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
check cafa-tests sh tests/run_cafa_tests.sh
check usb-tests sh tests/run_cafa_usb_tests.sh
(cd "$OUT" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)

exit "$FAIL"
