// SPDX-License-Identifier: GPL-3.0-only
// web/sim/install.js against a fake SMK-37 Elite speaking the update protocol: found, checked, the loader, the
// whole package written byte for byte, the identity after; a foreign loader and an FM-1 refused.
//   MODEL=elite sh build.sh && node tests/install_test.js
const ROOT = require("path").resolve(__dirname, "..");
const fs = require('fs'), I = require(ROOT + '/web/sim/install.js');
for (const k of Object.keys(I.DELAY)) I.DELAY[k] = Math.min(I.DELAY[k], k.startsWith('idle') || k.startsWith('wait') ? 3000 : (k === 'hs' || k === 'poll' ? 200 : 5));
const raw = new Uint8Array(fs.readFileSync(ROOT + '/build/noterush37-elite.fwsc'));
const pkg = I.loadPackage(raw), image = pkg.image;
const ident = (t) => { const d = [0, 0x59, 0x11, 0, 0, 0]; for (let i = 0; i < 27; i++) d.push(i < t.length ? t.charCodeAt(i) : 0); d.push(0); return [0xF0, ...I.pack7(d), 0xF7]; };
const req = (addr, n, fl = 0) => { const u = [0, 0x59, 0x30, 0, 0, 0, fl, addr & 255, (addr >>> 8) & 255, (addr >>> 16) & 255, (addr >>> 24) & 255, n & 255, (n >> 8) & 255, 0];
  let s = 0; for (let i = 6; i < 14; i++) s += u[i]; u.push(~s & 255); return [0xF0, ...I.pack7(u), 0xF7]; };
class Port { constructor(n) { this.name = n; this.state = 'connected'; this.l = []; } addEventListener(e, f) { this.l.push(f); } removeEventListener(e, f) { this.l = this.l.filter((x) => x !== f); } emit(d) { setTimeout(() => this.l.forEach((f) => f({ data: d })), 1); } }
let ports = [], writes = 0, checks = 0, bad = 0, written = new Uint8Array(image.length), mode = 'run';
function device(name, idText, onUpgrade) {
  const inp = new Port(name), out = new Port(name);
  out.send = (p) => {
    if (p.length === 10 && p[1] === 0x00 && p[2] === 0x32) inp.emit(ident(idText));
    else if (p.length === 6 && p[1] === 0x22) onUpgrade(inp);
    else { const u = I.unpack7(p.slice(1, -1)); dev.last = u; if (dev.wait) { const w = dev.wait; dev.wait = null; w(u); } }
  };
  const dev = { inp, out };
  ports.push(dev);
  return dev;
}
const ask = (dev, addr, n) => new Promise((res) => { dev.wait = res; dev.inp.emit(req(addr, n)); });
function vanish(dev) { dev.inp.state = 'disconnected'; ports = ports.filter((p) => p !== dev); }
async function serveAll(dev, from, to, step, finish, count) {
  for (let a = from; a < to; a += step) {
    const n = Math.min(step, to - a), u = await ask(dev, a, n), data = u.slice(14, 14 + n);
    for (let i = 0; i < n; i++) if (data[i] !== image[a + i]) { bad++; break; }
    if (count) written.set(data, a);
    count ? writes++ : checks++;
  }
  const u = await ask(dev, finish, 0);
  return String.fromCharCode(...u.slice(14, 21));
}
const run = device('NoteRush SMK37', 'SMK-37 Elite_700', async (inp) => {
  const ok = await serveAll(run, 0, 4096, 512, 0xE0000000, false);
  setTimeout(() => { vanish(run); const ldr = device('NoteRush SMK37 Update', 'ota-SMK-37 Elite_700', async () => {
    const ok2 = await serveAll(ldr, 0, image.length, 512, 0xF0000000, true);
    setTimeout(() => { vanish(ldr); device('NoteRush SMK37', pkg.product, () => {}); }, 20);
  }); }, 20);
});
device('Some Synth', 'X_1', () => {});                    // (another device: never probed)
const midi = { inputs: () => ports.map((p) => ({ name: p.inp.name, port: p.inp })), outputs: () => ports.map((p) => ({ name: p.out.name, port: p.out })) };
(async () => {
  const dev = await I.find(midi);
  console.log('found', dev.name, dev.id.text, 'package', I.packageFor(dev.id.text));
  const steps = [];
  const r = await I.install(midi, dev, pkg, (k, v) => { if (k !== 'check' && !(k === 'write' && v % 25)) steps.push(k + (v !== undefined ? ':' + v : '')); });
  const same = written.every((b, i) => b === image[i]);
  console.log('result', r, '| steps', steps.join(' '), '| check reads', checks, 'write reads', writes, '| bad', bad, '| written == image', same);
  // refusals: a foreign loader, another model
  for (const t of ['ota-SMK-37 Pro_016', 'FM-1_906']) {
    try { await I.install(midi, { link: { close() {} }, id: I.parseIdentity(ident(t)), name: 'x' }, pkg); console.log(t, 'NOT refused'); }
    catch (e) { console.log(t, 'refused:', e.code); }
  }
  process.exit(bad || !same ? 1 : 0);
})();
