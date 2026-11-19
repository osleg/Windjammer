#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
"${CC:-cc}" -O1 -w -DFELUCCA_CONTROLLER=1 -Ibuild/gen -Ifirmware/src tests/cafa_native_ui_test.c -lm -o build/cafa-tests/controller-native-ui
ASAN_OPTIONS=detect_leaks=0 build/cafa-tests/controller-native-ui
