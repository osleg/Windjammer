#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
"${CC:-cc}" -O1 -w -Ibuild/gen -Ifirmware/src tests/controller_audio_test.c -lm -o build/cafa-tests/controller-audio
build/cafa-tests/controller-audio
