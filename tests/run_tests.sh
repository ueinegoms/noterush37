#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro); NOTE RUSH 37: ueine
# Host tests (no hardware), from the repo root:  tests/run_tests.sh
#   game     NOTE RUSH 37 played through the simulated panel: the rules, the sound (the same key, the same sound),
#            the controls, the screens (build/nrshots/*.ppm)
#   engine   Vector37's synth engine: patterns, blends, aliasing, clicks, voice modes, presets, cost
#   board    the flash store and its owner tag, the update loader, the OTA entry (each model), recovery, TRS MIDI OUT
#   install  web/sim/install.js against a fake SMK-37 (node)
# The board tests read the package: build it first (sh build.sh; MODEL=elite / mke for theirs).
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT" "$OUT/wav" build/gen build/nrshots
python3 tools/gen_tables.py build/gen/mk_tables.h >/dev/null
python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
CC="${CC:-cc} -O2 -w"
fail=0
run() {                                  # run NAME CMD...: one line, its log in build/host/NAME.log
    n=$1; shift
    if "$@" >"$OUT/$n.log" 2>&1; then echo "  ok   $n"; else echo "  FAIL $n (build/host/$n.log)"; fail=1; fi
}
$CC -Ibuild/gen -o "$OUT/nr_shot" tests/nr_shot.c && run game "$OUT/nr_shot"
$CC -Ibuild/gen -o "$OUT/host" tests/host.c -lm && run engine "$OUT/host" "$OUT/wav"
PKG=build/noterush37.fwsc
[ -f $PKG ] || { echo "  (no $PKG: sh build.sh first for the board tests)"; exit $fail; }
for t in storage_test smk_flash_test owner_test recovery_test "ota_test $PKG" "ldr_test $PKG $PKG" smk_midi_out_test; do
    set -- $t
    $CC -DBOARD_SMK37=1 -Ifirmware/hal -Ifirmware/src -Ibuild/gen -o "$OUT/$1" "tests/$1.c" && run "$1" "$OUT/$1" $2 $3
done
for m in 1 2; do                         # ota.c of the MODEL=elite / mke builds: their identity and update record
    $CC -DBOARD_SMK37=1 -DT_MODEL=$m -Ifirmware/hal -Ifirmware/src -Ibuild/gen -o "$OUT/ota_test_m$m" tests/ota_test.c &&
        run "ota_test_model$m" "$OUT/ota_test_m$m" $PKG
done
if [ -f build/noterush37-elite.fwsc ] && command -v node >/dev/null 2>&1; then
    run install node tests/install_test.js
fi
[ $fail = 0 ] && echo "all ok" || echo "FAILED"
exit $fail
