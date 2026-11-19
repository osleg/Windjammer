#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/cafa-tests
CC=${CC:-cc}
$CC -std=c99 -Wall -Wextra ${CAFA_TEST_CFLAGS:-} tests/cafa_protocol_test.c -o build/cafa-tests/protocol
build/cafa-tests/protocol
$CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/cafa_wire6_test.c -lm -o build/cafa-tests/wire6
build/cafa-tests/wire6
$CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/cafa_panel_midi_test.c -lm -o build/cafa-tests/panel-midi
build/cafa-tests/panel-midi
$CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/cafa_diag_test.c -lm -o build/cafa-tests/diag
build/cafa-tests/diag
$CC -std=c99 -Wall -Wextra ${CAFA_TEST_CFLAGS:-} tests/marker_detector_test.c -o build/cafa-tests/marker-detector
build/cafa-tests/marker-detector
$CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/controller_identity_test.c -lm -o build/cafa-tests/controller-identity
build/cafa-tests/controller-identity
$CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/cafa_surface_test.c -lm -o build/cafa-tests/surface

$CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/cafa_native_ui_test.c -lm -o build/cafa-tests/native-ui
ASAN_OPTIONS=detect_leaks=0 build/cafa-tests/native-ui
build/cafa-tests/surface

${CC:-cc} -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src tests/cafa_audio_test.c -lm -o build/cafa-tests/audio
ASAN_OPTIONS=detect_leaks=0 build/cafa-tests/audio

if [ -n "${LOOM_V5_FIXTURES:-${LOOM_V4_FIXTURES:-}}" ]; then
    $CC -O1 -w ${CAFA_TEST_CFLAGS:-} -Ibuild/gen -Ifirmware/src -Itests tests/loom_native_lint.c -lm -o build/cafa-tests/loom-lint
    ASAN_OPTIONS=detect_leaks=0 build/cafa-tests/loom-lint "${LOOM_V5_FIXTURES:-${LOOM_V4_FIXTURES:-}}"
fi
