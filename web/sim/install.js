// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (tools/fm1_install.py)
// NOTE RUSH 37: the same install in the browser (Web MIDI with SysEx; Chrome / Edge), ported from fm1_install.py.
// Step 1: the running firmware reads parts of the package and starts the update loader; step 2: the loader reads
// the whole package and writes it; then the keyboard restarts and its identity is checked. In both steps the device
// asks (SysEx read requests on the logical image) and we answer. Only our own packages (the Felucca loader in them,
// the files' CRCs good), only to an SMK-37 (Pro / Elite / MKE-P37), and a device left in another firmware's update
// mode is not touched.
(function (root) {
  "use strict";
  const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
  const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
  const FINISH_CHECK = 0xE0000000, FINISH_WRITE = 0xF0000000, MAXDATA = 512;
  const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F, MAX_BLOCKS = 64;
  const LOADER_MARK = "FELUCCA-LOADER-1";
  const PORT_RE = /fm-1|smk|elite|mke-p37|starry|felucca|melodee|fomni|x0x|makkuro|snoop|vector|vox|ota|composite|sinco|usb-midi|note ?rush/i;
  const OWN_LOADER_RE = /^ota-(SMK-37 Pro|SMK-37 Elite|MKE-P37)_[79]\d\d$/i;
  const SMK_RE = /^(ota-)?(SMK-37([ +].*)?|MKE-P37|STARRYKEY-37.*)$/i;
  const DELAY = { open: 300, start: 2000, reply: 10, loader: 3000, reboot: 3000, retry: 1000, poll: 1000, hs: 1000,
                  idle_check: 8000, idle_write: 180000, wait_loader: 30000, wait_reboot: 40000 };
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  class InstallError extends Error { constructor(code, msg) { super(msg); this.code = code; } }

  // ------------------------------------------------------------ wire format ---
  function pack7(data) {
    const out = []; let acc = 0, nb = 0;
    for (const b of data) { acc |= b << nb; nb += 8; while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } }
    if (nb) out.push(acc & 0x7F);
    return out;
  }
  function unpack7(s) {
    const out = []; let acc = 0, nb = 0;
    for (const b of s) { acc |= (b & 0x7F) << nb; nb += 7; while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; } }
    return out;
  }
  function response(addr, data, fl = 0) {
    const n = data.length, body = [0x00, 0x59, 0x30, (n + 8) & 0xFF, ((n + 8) >> 8) & 0xFF, 0, fl,
      addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, n & 0xFF, (n >> 8) & 0xFF, 0];
    for (const b of data) body.push(b);
    let sum = 0;
    for (let i = 6; i < body.length; i++) sum += body[i];
    body.push(~sum & 0xFF);
    return [0xF0, ...pack7(body), 0xF7];
  }
  function parseRequest(p) {        // -> [flags, addr, len] or null
    if (p.length < 2 || p[0] !== 0xF0 || p[p.length - 1] !== 0xF7) return null;
    const u = unpack7(p.slice(1, -1));
    if (u.length !== 15 || u[0] !== 0x00 || u[1] !== 0x59 || u[2] !== 0x30) return null;
    let sum = 0;
    for (let i = 6; i < 14; i++) sum += u[i];
    if ((~sum & 0xFF) !== u[14]) return null;
    return [u[6], (u[7] | u[8] << 8 | u[9] << 16 | u[10] << 24) >>> 0, u[11] | u[12] << 8 | u[13] << 16];
  }
  function parseIdentity(p) {       // -> {text, model, loader} or null
    if (p.length < 2 || p[0] !== 0xF0 || p[p.length - 1] !== 0xF7) return null;
    const d = unpack7(p.slice(1, -1));
    if (d.length !== 34 || d[0] !== 0x00 || d[1] !== 0x59 || d[2] !== 0x11) return null;
    let txt = "";
    for (let i = 6; i < 33 && d[i]; i++) txt += String.fromCharCode(d[i]);
    const m = /^([^_]+)_(\d+)$/.exec(txt);
    return m ? { text: txt, model: m[1], loader: /^ota-/i.test(m[1]) } : null;
  }
  const modelOf = (text) => text.split("_")[0].replace(/^ota-/i, "");

  // --------------------------------------------------------------- package ---
  const idChar = (m, i) => { const c = (m - i - 1) & 0xFF; return c >= 0x20 && c < 0x7F; };
  function identityBlocks(raw) {
    const marker = (k) => (k + 1) * BLK <= raw.length ? raw[k * BLK + KEEP] : null;
    let n = BLOCKS, m;
    while (n < MAX_BLOCKS && (m = marker(n)) !== null && m !== 0x7D && idChar(m, n)) n++;
    return n > BLOCKS && marker(n) === 0x7D ? n : BLOCKS;
  }
  function blockCount(raw) {
    let n = identityBlocks(raw);
    while (n < MAX_BLOCKS && (n + 1) * BLK <= raw.length && raw[n * BLK + KEEP] === 0x7D) n++;
    return n;
  }
  function logicalImage(raw, blocks = BLOCKS) {
    const out = new Uint8Array(raw.length - blocks);
    let o = 0;
    for (let i = 0; i < blocks; i++) { out.set(raw.subarray(i * BLK, i * BLK + KEEP), o); o += KEEP; }
    out.set(raw.subarray(blocks * BLK), o);
    return out;
  }
  function deinterleave(raw) {
    const n = blockCount(raw), image = logicalImage(raw, n);
    if (n !== BLOCKS && packageDamage(image)) {
      const alt = logicalImage(raw, BLOCKS);
      if (!packageDamage(alt)) return [alt, BLOCKS];
    }
    return [image, n];
  }
  function productOf(raw) {
    if (raw.length < BLOCKS * BLK) throw new InstallError("badpkg", "not a package (too short)");
    const ib = identityBlocks(raw), n = ib === BLOCKS ? BLOCKS : Math.min(ib, deinterleave(raw)[1]);
    let s = "";
    for (let i = 0; i < n; i++) { const m = raw[i * BLK + KEEP]; if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF); }
    return s;
  }
  function crc16(data, crc = 0) {   // CRC-16/XMODEM
    for (const b of data) {
      crc ^= b << 8;
      for (let k = 0; k < 8; k++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
    }
    return crc;
  }
  function dec(b) {                 // JieLi ENC stream cipher (its own inverse)
    const out = Uint8Array.from(b); let key = 0xFFFF;
    for (let i = 0; i < out.length; i++) { out[i] ^= key & 0xFF; key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF; }
    return out;
  }
  const rd = (b, o, n) => { let v = 0; for (let i = n - 1; i >= 0; i--) v = v * 256 + b[o + i]; return v; };
  function packageDamage(image) {   // what is wrong (header, file list, the installed files' CRCs), or null
    if (image.length < 0x40) return "too short";
    const hdr = dec(image.subarray(0, 0x40));
    if (crc16(hdr.subarray(2, 0x40)) !== rd(hdr, 0, 2)) return "header CRC";
    const count = rd(hdr, 8, 2);
    if (count < 1 || count > 8 || image.length < 0x40 + 0x50 * count) return "file list";
    const lst = image.subarray(0x40, 0x40 + 0x50 * count);
    if (crc16(lst) !== rd(hdr, 2, 2)) return "file list CRC";
    for (let i = 0; i < count; i++) {
      const e = dec(lst.subarray(i * 0x50, (i + 1) * 0x50)), off = rd(e, 8, 4), size = rd(e, 12, 4), typ = rd(e, 0, 2);
      if (off + size > image.length) return "a file cut short";
      if ((typ === 0 || typ === 100) && crc16(image.subarray(off, off + size)) !== rd(e, 4, 2)) return "a file's CRC";
    }
    return null;
  }
  function hasMark(raw) {
    const m = Array.from(LOADER_MARK, (c) => c.charCodeAt(0));
    outer: for (let i = 0; i + m.length <= raw.length; i++) {
      for (let k = 0; k < m.length; k++) if (raw[i + k] !== m[k]) continue outer;
      return true;
    }
    return false;
  }
  function loadPackage(raw) {       // -> {product, image}
    const product = productOf(raw);
    if (!/^[^_]+_\d+$/.test(product)) throw new InstallError("badpkg", `not a package (identity ${product})`);
    if (!hasMark(raw)) throw new InstallError("badpkg", "no Felucca update loader in this package");
    const [image] = deinterleave(raw), bad = packageDamage(image);
    if (bad) throw new InstallError("badpkg", `damaged package (${bad}): nothing was written`);
    return { product, image };
  }

  // ------------------------------------------------------------------ MIDI ---
  // a link: one input/output pair with a SysEx queue. midi: {inputs() -> [{name, port}], outputs() -> [...]} where
  // a port is a Web MIDI port (or a test's fake one: open(), close(), send(), addEventListener, state)
  class Link {
    constructor(name, inp, out) {
      this.name = name; this.inp = inp; this.out = out; this.q = []; this.wait = null;
      this.rx = (ev) => {
        const d = Array.from(ev.data);
        if (d[0] !== 0xF0) return;
        if (this.wait) { const w = this.wait; this.wait = null; w(d); } else this.q.push(d);
      };
      inp.addEventListener("midimessage", this.rx);
    }
    get lost() { return this.inp.state === "disconnected"; }
    send(p) { try { this.out.send(p); return true; } catch (e) { return false; } }
    read(ms) {
      if (this.q.length) return Promise.resolve(this.q.shift());
      return new Promise((res) => {
        const t = setTimeout(() => { this.wait = null; res(null); }, Math.max(ms, 0));
        this.wait = (d) => { clearTimeout(t); res(d); };
      });
    }
    drain() { this.q.length = 0; }
    close() { this.inp.removeEventListener("midimessage", this.rx); }
  }
  async function handshake(link, tries = 3) {
    link.drain();
    for (let i = 0; i < tries; i++) {
      if (!link.send(HS_QUERY)) return null;
      const end = Date.now() + DELAY.hs;
      for (let left; (left = end - Date.now()) > 0;) {
        const p = await link.read(left);
        if (!p) break;
        const id = parseIdentity(p);
        if (id) return id;
      }
    }
    return null;
  }
  function pairOutput(name, outs) {
    const same = outs.filter((o) => o.name === name);
    if (same.length) return same[0].port;
    const base = (s) => s.replace(/\s+\d+$/, ""), b = outs.filter((o) => base(o.name) === base(name));
    return b.length === 1 ? b[0].port : outs.length === 1 ? outs[0].port : null;
  }
  // the first SMK-37-like port that answers the handshake (and want(identity)): {link, id, name}
  async function find(midi, want, all) {
    const outs = midi.outputs();
    for (const { name, port } of midi.inputs()) {
      if (!all && !PORT_RE.test(name)) continue;
      const out = pairOutput(name, outs);
      if (!out) continue;
      let link;
      try { if (port.open) await port.open(); if (out.open) await out.open(); link = new Link(name, port, out); }
      catch (e) { continue; }
      await sleep(DELAY.open);
      const id = await handshake(link, 2);
      if (id && (!want || want(id))) return { link, id, name };
      link.close();
    }
    return null;
  }
  async function waitFor(midi, want, ms) {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const d = await find(midi, want, false);
      if (d) return d;
      await sleep(DELAY.retry);
    }
    return null;
  }
  async function serve(link, image, finish, idle, progress) {   // -> [served, finished, lost]
    let served = 0, last = Date.now();
    for (;;) {
      const p = await link.read(DELAY.poll);
      if (!p) {
        if (link.lost) return [served, false, true];
        if (Date.now() - last > idle) return [served, false, false];
        continue;
      }
      const r = parseRequest(p);
      if (!r) continue;
      const [fl, addr, n] = r;
      last = Date.now();
      if (addr === FINISH_CHECK || addr === FINISH_WRITE) {
        link.send(response(addr, Array.from("success\0", (c) => c.charCodeAt(0))));
        if (addr === finish) return [served, true, false];
        continue;
      }
      if (n > MAXDATA || addr + n > image.length)
        throw new InstallError("badreq", `the keyboard asked for ${addr.toString(16)}+${n}, outside the package`);
      await sleep(DELAY.reply);
      if (!link.send(response(addr, image.subarray(addr, addr + n), fl))) return [served, false, true];
      served++;
      progress(served, addr + n);
    }
  }
  // running firmware -> loader -> the new firmware. dev: from find(); step(kind, value) reports progress
  async function install(midi, dev, pkg, step = () => {}) {
    const { image, product } = pkg;
    if (!SMK_RE.test(modelOf(dev.id.text)) || !SMK_RE.test(modelOf(product))) {
      dev.link.close();
      throw new InstallError("model", `o teclado é ${dev.id.text}, o pacote é ${product}`);
    }
    if (dev.id.loader && !OWN_LOADER_RE.test(dev.id.text)) {
      dev.link.close();
      throw new InstallError("foreign", `o teclado está no modo de update de outro firmware (${dev.id.text}): ` +
                                        "termine esse update com o atualizador dele e tente de novo");
    }
    let ota = dev;
    if (!dev.id.loader) {            // step 1: the running firmware checks the package and starts the loader
      step("start", dev.id.text);
      dev.link.send(UPGRADE);
      await sleep(DELAY.start);
      const [served, finished, lost] = await serve(dev.link, image, FINISH_CHECK, DELAY.idle_check, (k) => step("check", k));
      dev.link.close();
      if (!finished) throw new InstallError(lost ? "lost" : "stopped",
        `o teclado ${lost ? "desconectou" : "parou de responder"} depois de ${served} leituras; nada foi gravado`);
      step("loader");
      await sleep(DELAY.loader);
      ota = await waitFor(midi, (i) => i.loader, DELAY.wait_loader);
      if (!ota) throw new InstallError("noloader", "o loader de update não apareceu: replugue o cabo USB e instale de " +
                                                   "novo (o teclado fica em modo update até terminar)");
    }
    step("write", 0);                // step 2: the loader reads and writes the whole package, then restarts
    ota.link.send(UPGRADE);
    await sleep(DELAY.start);
    const [served, finished, lost] = await serve(ota.link, image, FINISH_WRITE, DELAY.idle_write,
      (k, end) => step("write", Math.min(99, Math.floor(end * 100 / image.length))));
    ota.link.close();
    if (!finished) throw new InstallError(lost ? "lost" : "stopped",
      `o loader ${lost ? "desconectou" : "parou de responder"} depois de ${served} leituras: replugue o cabo e ` +
      "instale de novo pra terminar");
    step("write", 100);
    step("reboot");
    await sleep(DELAY.reboot);
    const back = await waitFor(midi, (i) => !i.loader, DELAY.wait_reboot);
    if (!back) throw new InstallError("noreturn", "o teclado não voltou: desligue e ligue de novo");
    back.link.close();
    if (back.id.text !== product) throw new InstallError("mismatch", `gravado, mas o teclado diz ${back.id.text} (esperado ${product})`);
    step("done", back.id.text);
    return back.id.text;
  }
  // the package for a device: by the model it reports (a loader's: the same)
  function packageFor(idText) {
    const m = modelOf(idText).toUpperCase();
    return m === "SMK-37 PRO" ? "noterush37.fwsc" : m === "MKE-P37" ? "noterush37-mke.fwsc"
         : m === "SMK-37 ELITE" ? "noterush37-elite.fwsc" : null;
  }
  // Web MIDI as the midi object find() takes
  function webMidi(access) {
    const list = (m) => Array.from(m.values()).filter((p) => p.state !== "disconnected").map((p) => ({ name: p.name || "", port: p }));
    return { inputs: () => list(access.inputs), outputs: () => list(access.outputs) };
  }

  root.NR37Install = { InstallError, DELAY, pack7, unpack7, response, parseRequest, parseIdentity, productOf, deinterleave,
                       packageDamage, loadPackage, find, install, packageFor, webMidi, modelOf };
  if (typeof module !== "undefined") module.exports = root.NR37Install;
})(typeof window !== "undefined" ? window : globalThis);
