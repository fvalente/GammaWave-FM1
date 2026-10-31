// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// Bounds checks for the .fwsc reader: a malformed package must fail with a clear error, not hang the page.
// Run from the repo root: node web/test_pkg_bounds.mjs
//
// (QG-1 stage 2) The fixtures are built the way tools/fm1pkg_make.py builds a package: a UFW header whose
// entry list is encrypted, over a flash image whose own header, entry list, app area and cfg block are
// encrypted. The first checks parse a good one, so a fixture that is not valid up to the point where it
// is broken fails there instead of hiding behind a plausible-looking error from the parser.

import { readFileSync } from "node:fs";
import { Package, crc16 } from "./fm1pkg.js";

const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F, SKEW = BLOCKS * BLK - BLOCKS * KEEP;
let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(66)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };

function enc(buf, off, size, key = 0xFFFF) {
  for (let i = 0; i < size; i++) {
    buf[off + i] ^= key & 0xFF;
    key = ((key * 2) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
}
function w16(b, o, v) { b[o] = v & 0xFF; b[o + 1] = (v >> 8) & 0xFF; }
function w32(b, o, v) { w16(b, o, v & 0xFFFF); w16(b, o + 2, (v >>> 16) & 0xFFFF); }
function hdrFileOff(i) { return Math.floor(i / KEEP) * BLK + (i % KEEP); }

/* the app area (and the cfg block after it) are one sfc stream rooted at the app's base: 32 bytes a
 * chunk, each chunk's key k ^ (offset >> 2). enc() is its own inverse, so this both writes and reads. */
function sfcRegion(buf, base, len, key) {
  for (let p = 0; p < len; p += 32) enc(buf, base + p, Math.min(len - p, 32), key ^ (p >> 2));
}

/* a JLFS entry, 32 bytes, plaintext: hcrc over 2..31, then the fields, then the name at +16 */
function wEntry(buf, off, o = {}) {
  const { name = "", flags = 0, index = 0, dataOff = 0, size = 0, dcrc = 0 } = o;
  if (off + 32 > buf.length) return;
  const e = new Uint8Array(32);
  w16(e, 2, dcrc);
  w32(e, 4, dataOff);
  w32(e, 8, size);
  e[12] = flags;
  w16(e, 14, index);
  for (let i = 0; i < name.length && i < 16; i++) e[16 + i] = name.charCodeAt(i);
  w16(e, 0, crc16(e.subarray(2, 32)));
  buf.set(e, off);
}

/* isd_config.ini's 32 bytes: chipkeyDecode() sums them (clamped to 0xAA / 0x55) and compares
 * d[16+i] ^ d[15-i] with it, so both ends of that range are reachable by construction */
function isdBlob(key) {
  const d = new Uint8Array(32);
  if (key === 0xFFFF) {
    d.fill(0x0E, 0, 16);                                       // sum 224 >= 0xE0 -> 0xAA
    for (let i = 0; i < 16; i++) d[16 + i] = d[15 - i];        // XOR 0 < 0xAA -> bit set
  } else {
    d.fill(0x01, 0, 16);                                       // sum 16 <= 0x10 -> 0x55
    for (let i = 0; i < 16; i++) d[16 + i] = d[15 - i] ^ 0xFF; // XOR 0xFF >= 0x55 -> bit clear
  }
  return d;
}

const APP_BASE = 0x200, APP_AREA = 0x180, APP_ENTRY_OFF = 0x80, APP_ENTRY_SIZE = 0x20, CFG_LEN = 0x40;


/* a flash image. Every field a check needs to break is an option; the defaults are a valid one. */
function buildFlash(o = {}) {
  const { appBase = APP_BASE, appArea = APP_AREA, headSize = null, cfgLen = CFG_LEN,
          appEntry = true, appEntryOff = APP_ENTRY_OFF, appEntrySize = APP_ENTRY_SIZE, appEnd = true,
          entryEnd = true, noEntryList = false, fillEntries = false, key = 0xFFFF, encKey = null,
          isdName = "isd_config.ini", appHeadName = "app_dir_head", appEntryName = "app.bin",
          headerBad = false } = o;
  const hs = headSize === null ? appArea : headSize;
  const k = encKey === null ? key : encKey;                     // the key the area is really written with
  const size = o.size !== undefined ? o.size
    : Math.max(0x400, appBase + appArea + cfgLen + 32, 0x100 + 34);
  const f = new Uint8Array(size);
  /* with fillEntries every slot after the two real ones is an encrypted zero entry, so the isd blob has
   * to live in one of those (it reads as zeros: chipkeyDecode(all zero) = 0xFFFF, CRCs agree) */
  const isdOff = o.isdOff !== undefined ? o.isdOff : fillEntries ? 0x2000 : 0x100;

  if (!noEntryList) {
    wEntry(f, 0x20, { name: isdName, flags: 0x81, dataOff: isdOff, size: 34 });
    wEntry(f, 0x40, { name: appHeadName, flags: 0x81, dataOff: appBase, size: hs });
    if (entryEnd) wEntry(f, 0x60, { index: 1 });
    if (!fillEntries && isdOff + 34 <= size) {
      const blob = isdBlob(key);
      f.set(blob, isdOff);
      w16(f, isdOff + 32, crc16(blob));
    }
    for (const off of [0x20, 0x40, 0x60]) enc(f, off, 32);
  }

  /* the app area: its head, the app's own entry, the end of that list, then the app's bytes */
  wEntry(f, appBase, { name: "app_area_head", flags: 0x83, dataOff: 0, size: hs });
  if (appEntry) wEntry(f, appBase + 32, { name: appEntryName, flags: 0x82, dataOff: appEntryOff, size: appEntrySize });
  if (appEnd) wEntry(f, appBase + 64, { index: 1 });
  for (let i = 0; i < appEntrySize; i++) {
    const at = appBase + appEntryOff + i;
    if (at >= 0 && at < size) f[at] = 0xA5;                     // the app's bytes (never read as entries)
  }
  if (cfgLen && appBase + hs + 32 + cfgLen <= size) wEntry(f, appBase + hs, { name: "cfg", flags: 0x82, size: cfgLen });
  const end = Math.min(size, appBase + hs + cfgLen);
  if (end > appBase) sfcRegion(f, appBase, end - appBase, k);

  w16(f, 0, crc16(f.subarray(2, 32)) ^ (headerBad ? 0x5A5A : 0));   // the flash header, last of all
  enc(f, 0, 32);
  /* every slot of an unterminated entry list holding a valid (zero) entry: the worst case for the walk */
  if (fillEntries) {
    f.fill(0, 0x80);
    for (let off = 0x80; off + 32 <= size; off += 32) enc(f, off, 32);
  }
  return f;
}

/* a package: the 20 marker blocks and the UFW header, its entry list encrypted, the image at dataOff */
function buildPackage(flash, o = {}) {
  const { nent = 1, dataOff = 0x800, size = flash.length, dcrc = crc16(flash), type = 0,
          name = "flash.bin", hdrBad = false, listBad = false, product = "FM-1_900", cut = 0 } = o;
  const total = Math.max(BLOCKS * BLK, 0x800 + SKEW + flash.length);
  const raw = new Uint8Array(total);
  if (dataOff + SKEW + flash.length <= total) raw.set(flash, dataOff + SKEW);
  const h = new Uint8Array(BLOCKS * KEEP);
  w16(h, 8, nent);
  const hsize = 0x40 + nent * 0x50;
  if (hsize <= h.length) {
    const e = new Uint8Array(0x50);
    w16(e, 0, type);                                           // type 0: flash.bin
    w16(e, 4, dcrc);
    w32(e, 8, dataOff);
    w32(e, 12, size);
    for (let i = 0; i < name.length && i < 0x10; i++) e[0x40 + i] = name.charCodeAt(i);
    enc(e, 0, 0x50);
    h.set(e, 0x40);
    w16(h, 2, crc16(h.subarray(0x40, hsize)) ^ (listBad ? 0x5A5A : 0));
  }
  w16(h, 0, crc16(h.subarray(2, 0x40)) ^ (hdrBad ? 0x5A5A : 0));
  enc(h, 0, 0x40);
  for (let i = 0; i < Math.min(hsize, h.length); i++) raw[hdrFileOff(i)] = h[i];

  for (let i = 0; i < BLOCKS; i++)                               // the identity: one marker per block
    raw[i * BLK + KEEP] = i < product.length ? (product.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
  return cut ? raw.subarray(0, raw.length - cut) : raw;
}

const good = () => buildPackage(buildFlash());

function throws(label, fn, re) {
  let passed = false, msg = "";
  try { fn(); } catch (e) { msg = String(e && e.message || e); passed = re.test(msg); }
  ok(passed, `${label}${passed ? "" : ` (${msg || "no error"})`}`);
}
function ms(fn) { const t = process.hrtime.bigint(); fn(); return Number(process.hrtime.bigint() - t) / 1e6; }
/* --- the fixture is a valid package: without this, every check below could pass for the wrong reason --- */
{
  let p = null, err = null;
  try { p = new Package(good()); } catch (e) { err = e; }
  ok(p !== null, `package: a well-formed package parses${p ? "" : ` (${err && err.message})`}`);
  ok(!!p && p.product === "FM-1_900" && p.nent === 1 && p.flashFileOff === 0x800 + SKEW &&
     p.flashSize === 0x400 && p.appBase === APP_BASE && p.appOff === APP_BASE + APP_ENTRY_OFF &&
     p.app.size === APP_ENTRY_SIZE && p.cfgLen === CFG_LEN,
     "package: .. its layout reads back (product, flash, app.bin, cfg)");
}

/* --- the package around the image --- */
throws("package: fewer than 20 blocks is refused", () => new Package(new Uint8Array(10)), /too short/i);
throws("package: an empty file is refused", () => new Package(new Uint8Array(0)), /too short/i);
throws("package: a zero-size flash.bin is refused", () => new Package(buildPackage(new Uint8Array(0))),
       /flash image is too short/i);
throws("package: a flash with no entry-list end is refused",
       () => new Package(buildPackage(buildFlash({ noEntryList: true, size: 32 }))), /no end/i);
throws("package: an oversized UFW entry count is refused",
       () => new Package(buildPackage(buildFlash(), { nent: 0xFFFF })), /entry list is larger/i);
throws("package: flash.bin outside the package is refused",
       () => new Package(buildPackage(buildFlash(), { dataOff: 0x20000 })), /outside the package/i);
throws("package: a cut download (flash.bin past the end) is refused",
       () => new Package(buildPackage(buildFlash(), { cut: 0x200 })), /outside the package/i);
throws("package: a wrong flash.bin CRC is refused",
       () => new Package(buildPackage(buildFlash(), { dcrc: (crc16(buildFlash()) ^ 1) & 0xFFFF })),
       /flash\.bin CRC mismatch/i);
throws("package: a UFW header CRC mismatch is refused",
       () => new Package(buildPackage(buildFlash(), { hdrBad: true })), /UFW header CRC mismatch/i);
throws("package: a UFW entry-list CRC mismatch is refused",
       () => new Package(buildPackage(buildFlash(), { listBad: true })), /entry list CRC mismatch/i);
throws("package: no flash.bin entry (only ota.bin) is refused",
       () => new Package(buildPackage(buildFlash(), { type: 100, name: "ota.bin" })), /no flash\.bin/i);

/* --- the flash image inside it --- */
throws("flash: a header CRC mismatch is refused",
       () => new Package(buildPackage(buildFlash({ headerBad: true }))), /flash header CRC mismatch/i);
throws("flash: no isd_config.ini (no chip key) is refused",
       () => new Package(buildPackage(buildFlash({ isdName: "isd.ini" }))), /chip key or app_dir_head/i);
throws("flash: no app_dir_head is refused",
       () => new Package(buildPackage(buildFlash({ appHeadName: "app_area_hea" }))), /chip key or app_dir_head/i);
throws("flash: an app_dir_head past the last aligned block is refused",
       () => new Package(buildPackage(buildFlash({ appBase: 0x3D0, size: 0x3FF, cfgLen: 0 }))),
       /app_dir_head is outside/i);
throws("flash: an app area larger than the image is refused",
       () => new Package(buildPackage(buildFlash({ headSize: 0x2000 }))), /app area is outside/i);
throws("flash: no app.bin in the app area is refused",
       () => new Package(buildPackage(buildFlash({ appEntry: false }))), /app\.bin not found/i);
throws("flash: an app entry list with no end is refused",
       () => new Package(buildPackage(buildFlash({ appEnd: false, appEntryOff: 0, appEntrySize: 0 }))),
       /app entry list has no end/i);
/* the real layout: the app's bytes start right after the list, so a truncated list runs into them */
throws("flash: an app entry list cut before its end is refused",
       () => new Package(buildPackage(buildFlash({ appEnd: false }))), /JLFS header CRC mismatch/i);
throws("flash: an app.bin outside the image is refused",
       () => new Package(buildPackage(buildFlash({ appEntryOff: 0x1000 }))), /app\.bin is outside/i);
throws("flash: an app area written with another chip key is refused",
       () => new Package(buildPackage(buildFlash({ encKey: 0 }))), /JLFS header CRC mismatch/i);

/* --- a package without a cfg block still parses (the cfg is optional) --- */
{
  let p = null;
  try { p = new Package(buildPackage(buildFlash({ cfgLen: 0 }))); } catch (_) {}
  ok(!!p && p.cfgLen === 0 && p.app.size === APP_ENTRY_SIZE, "flash: no cfg block still parses (cfgLen 0)");
}

/* --- progress: a bad package is refused at once, whatever its size --- */
{
  const big = 0x80000;                                          // 512 KB, the size of the real flash
  const t1 = ms(() => new Package(buildPackage(buildFlash({ size: big, headSize: big - APP_BASE }))));
  ok(t1 < 500, `progress: a 512 KB package parses in ${t1.toFixed(1)} ms (< 500)`);
  let e1 = null, e2 = null;
  const t2 = ms(() => { try { new Package(buildPackage(buildFlash({ size: big, entryEnd: false, fillEntries: true }))); } catch (e) { e1 = e; } });
  const t3 = ms(() => { try { new Package(buildPackage(buildFlash({ size: big, headSize: big - APP_BASE, appEnd: false, appEntryOff: 0, appEntrySize: 0 }))); } catch (e) { e2 = e; } });
  const m1 = e1 ? e1.message : "no error", m2 = e2 ? e2.message : "no error";
  ok(/no end/i.test(m1) && t2 < 500,
     `progress: 512 KB with no entry-list end refused in ${t2.toFixed(1)} ms (${m1})`);
  ok(/no end/i.test(m2) && t3 < 500,
     `progress: 512 KB with no app-list end refused in ${t3.toFixed(1)} ms (${m2})`);
}

/* --- the real package, when ./build.sh (or the emulator workflow) has built one --- */
if (process.argv[2]) {
  let p = null, err = null;
  try { p = new Package(readFileSync(process.argv[2])); } catch (e) { err = e; }
  ok(!!p && p.app.size > 0, `package: ${process.argv[2]} parses (${p ? `${p.product}, ${p.app.size} B app` : err.message})`);
}

if (failed) {
  console.error(`${failed} package bounds check(s) failed`);
  process.exit(1);
}
console.log("package bounds: all checks passed");

