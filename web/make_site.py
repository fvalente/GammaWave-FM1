#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site (GitHub Pages):

  index.html                  redirect to the installer (the old URL keeps working)
  firmware/felucca-VER.fwsc   the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)
  webapp/installer/index.html index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  src/                        not touched

  web/make_site.py build/felucca-X.Y.fwsc X.Y OUT_DIR [PREV_DIR]

PREV_DIR (JIANT 0.6.46): what web/fetch_prev.py downloaded from the live site, its firmware/versions.json and its
packages: up to KEEP_OLD earlier versions (the newest build of each) stay installable next to the current one
(firmware/versions.json, the installer's version list).

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package; the device must report it after
the install.
"""
import base64
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
KEEP_OLD = 5
SAFE = re.compile(r"felucca-[A-Za-z0-9.-]+\.fwsc")


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def base_of(version):
    """v0.6.46-abc1234 -> v0.6.46: the builds of one version"""
    return version.split("-")[0]


def earlier(prev, version, product):
    """[(entry, file)]: PREV_DIR's versions, newest first, one per version, not this one, at most KEEP_OLD, each
    package there and with this device's identity"""
    out, seen = [], {base_of(version)}
    try:
        listed = json.loads((prev / "versions.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return out
    for e in listed if isinstance(listed, list) else []:
        v, f = str(e.get("version", "")), str(e.get("file", ""))
        src = prev / f
        if not SAFE.fullmatch(f) or base_of(v) in seen or not src.is_file():
            continue
        if product_of(src.read_bytes()) != product:
            continue
        seen.add(base_of(v))
        out.append(({"version": v, "file": f}, src))
        if len(out) >= KEEP_OLD:
            break
    return out


def with_csp(html):
    """(QG-1) a strict Content-Security-Policy: only the page's own inline script (by its hash) runs, nothing is
    fetched from elsewhere (the packages are same-origin), no plugins, no <base>, no forms"""
    m = re.search(r'<script type="module">(.*?)</script>', html, re.S)
    if not m or html.count("<script") != 1:
        raise SystemExit("index_pkg.html: expected exactly one inline module script (the CSP hashes it)")
    h = base64.b64encode(hashlib.sha256(m.group(1).encode("utf-8")).digest()).decode()
    csp = (f"default-src 'none'; script-src 'sha256-{h}'; style-src 'unsafe-inline'; img-src 'self' data: blob:; "
           "connect-src 'self'; font-src 'self' data:; media-src 'self' blob:; object-src 'none'; base-uri 'none'; "
           "form-action 'none'")
    meta = f'<meta http-equiv="Content-Security-Policy" content="{csp}">\n<meta name="referrer" content="no-referrer">\n'
    return html.replace('<meta charset="utf-8">\n', '<meta charset="utf-8">\n' + meta, 1)


def main(pkg, version, out, prev=None):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8"))
    name = f"felucca-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    old = earlier(Path(prev), version, product) if prev else []
    # (QG-1) each package with its SHA-256 and size: the installer refuses bytes that do not match
    def digest(b):
        return {"sha256": hashlib.sha256(b).hexdigest(), "size": len(b)}
    versions = [{"version": version, "file": name, **digest(raw)}] + \
        [{**e, **digest(src.read_bytes())} for e, src in old]
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name, **digest(raw),
                       "versions": [{"version": e["version"], "pkg": "../../firmware/" + e["file"],
                                     "sha256": e["sha256"], "size": e["size"]} for e in versions]})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    html = with_csp(html)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for f in fw.glob("felucca-*.fwsc"):            # the current package and the earlier ones kept, nothing else
        f.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    if (HERE.parent / "FM-1.fwsc").is_file():
        shutil.copy(HERE.parent / "FM-1.fwsc", fw / "FM-1.fwsc")
    for e, src in old:
        shutil.copy(src, fw / e["file"])
    (fw / "versions.json").write_text(json.dumps(versions, indent=1) + "\n", encoding="utf-8")
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Quantum GammaWave (QG-1) licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    shutil.copy(HERE / "editor.html", ed / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    (out / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Quantum GammaWave (QG-1)</title>'
        '<meta http-equiv="refresh" content="0; url=webapp/installer/">'
        '<a href="webapp/installer/">Quantum GammaWave (QG-1) Installer</a>\n', encoding="utf-8")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})"
          + (f", earlier: {' '.join(e['version'] for e, _ in old)}" if old else ""))


if __name__ == "__main__":
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    main(*sys.argv[1:5])
