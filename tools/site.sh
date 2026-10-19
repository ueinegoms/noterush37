#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ueine
# NOTE RUSH 37: the site (GitHub Pages, from docs/ on main): the simulator with the installer, the packages, and the
# README's screenshots (from the game test, at 2x). Build the packages first:
#   sh build.sh && MODEL=elite sh build.sh && MODEL=mke sh build.sh && sh tools/site.sh
set -e
cd "$(dirname "$0")/.."
for f in build/noterush37.fwsc build/noterush37-elite.fwsc build/noterush37-mke.fwsc; do
    [ -f "$f" ] || { echo "site: no $f (build the three packages first)"; exit 1; }
done
sh web/sim/build.sh
mkdir -p docs/img build/nrshots
find build/nrshots -name '*.ppm' -delete
cc -O2 -w -Ibuild/gen -o build/nr_shot tests/nr_shot.c
build/nr_shot
cp build/sim/index.html build/sim/worklet.js build/sim/install.js build/sim/sim.wasm build/sim/*.fwsc docs/
touch docs/.nojekyll
python3 - <<'PY'
import glob, os
from PIL import Image
shots = {os.path.basename(p)[3:-4]: p for p in glob.glob("build/nrshots/*.ppm")}
keep = ["title", "ask-nome", "ask-pauta", "ask-ouvido", "lightning", "intro", "over-pauta", "name", "board",
        "sound", "vector", "setup"]
for k in keep:
    Image.open(shots[k]).resize((480, 480), Image.NEAREST).save(f"docs/img/screen-{k}.png", optimize=True)
row = ["title", "ask-nome", "ask-pauta", "lightning", "sound", "vector"]   # the banner
b = Image.new("RGB", (len(row) * 240 + (len(row) + 1) * 12, 264), (27, 0, 147))
for i, k in enumerate(row):
    b.paste(Image.open(shots[k]), (12 + i * 252, 12))
b.resize((b.width * 2, b.height * 2), Image.NEAREST).save("docs/img/banner.png", optimize=True)
print("site: docs/ (%d screenshots)" % len(keep))
PY
