#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro)
# The NOTE RUSH 37 simulator (on Vector37's): the firmware's sources to WebAssembly (Emscripten), plus the page.
#   web/sim/build.sh  ->  build/sim/{index.html, worklet.js, sim.wasm}
#   then: python3 -m http.server -d build/sim 8737, open http://localhost:8737
set -e
cd "$(dirname "$0")/../.."
[ -d "$HOME/emsdk/upstream/emscripten" ] && PATH="$HOME/emsdk/upstream/emscripten:$PATH"
mkdir -p build/gen build/sim
python3 tools/gen_tables.py build/gen/mk_tables.h >/dev/null
python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
if command -v emcc >/dev/null 2>&1; then
    emcc -O2 -std=gnu11 -w -Ibuild/gen --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 \
        -sFILESYSTEM=0 -o build/sim/sim.wasm web/sim/sim.c
else                                   # (no Emscripten: clang and wasm-ld alone; web/sim/include: the libc it needs)
    clang --target=wasm32 -O2 -std=gnu11 -w -Ibuild/gen -Iweb/sim/include -nostdlib -ffreestanding -fno-builtin \
        -Wl,--no-entry -Wl,--export-dynamic -Wl,--initial-memory=33554432 -Wl,-z,stack-size=1048576 \
        -o build/sim/sim.wasm web/sim/sim.c
fi
cp web/sim/index.html web/sim/worklet.js web/sim/install.js build/sim/
for f in build/noterush37.fwsc build/noterush37-elite.fwsc build/noterush37-mke.fwsc; do   # (the installer's packages:
    [ -f "$f" ] && cp "$f" build/sim/                                                        # MODEL=pro / elite / mke sh build.sh)
done
echo "sim: build/sim ($(wc -c < build/sim/sim.wasm | tr -d ' ') B wasm)"
