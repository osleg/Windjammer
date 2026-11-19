#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build Pages from the published FM-1 controller package and standalone browser tools.

  python3 web/make_site.py Windjammer-FM-1-v0.9.0.fwsc v0.9.0 OUT_DIR

Build the standalone tools first with web/conv/build.sh and
EMU_VERSION=<tag> EMU_PREVIEW=0 sh web/emu/build.sh.
"""
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """Decode the package's identity markers (fm1pkg.js productOf)."""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def main(pkg, version, out):
    if not re.fullmatch(r"v[0-9][A-Za-z0-9._-]{0,14}", version):
        raise SystemExit(f"{version!r}: expected a Windjammer release tag")
    name = f"Windjammer-FM-1-{version}.fwsc"
    raw = Path(pkg).read_bytes()
    if len(raw) < BLOCKS * BLK or product_of(raw) != "FM-1_700" or b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: expected the published FM-1_700 package with its update loader")
    out = Path(out)
    conv = HERE.parent / "build" / "conv" / "conv.wasm"
    if not conv.exists():
        raise SystemExit(f"{conv}: no converter build (sh web/conv/build.sh)")
    emu = HERE.parent / "build" / "emu"
    page = (emu / "index.html").read_text(encoding="utf-8") if (emu / "index.html").exists() else ""
    if f">{version}<" not in page or not (emu / "felucca.wasm").is_file():
        raise SystemExit(f"{emu}: no standalone emulator built with EMU_VERSION={version} EMU_PREVIEW=0")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((HERE / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js"))
    sha256 = hashlib.sha256(raw).hexdigest()
    meta = json.dumps({"version": version, "product": "FM-1_700", "pkg": name, "sha256": sha256})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("@VERSION@", version).replace("@PACKAGE@", name)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for pattern in ("windjammer-*.fwsc", "Windjammer-FM-1-*.fwsc"):
        for old in fw.glob(pattern):
            old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    (fw / name).write_bytes(raw)
    (fw / "manifest.json").write_text(meta + "\n", encoding="utf-8")
    (fw / "SHA256SUMS").write_text(f"{sha256}  {name}\n", encoding="utf-8")
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Windjammer licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    shutil.copy(HERE / "editor.html", ed / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    (ed / "conv").mkdir(exist_ok=True)              # the standalone editor's archive converter
    shutil.copy(HERE / "conv" / "conv.js", ed / "conv" / "conv.js")
    shutil.copy(conv, ed / "conv" / "conv.wasm")
    if (HERE / "fonts").is_dir():                   # ((Skiff 1.0) the editor's Barlow Condensed, its OFL beside it)
        shutil.copytree(HERE / "fonts", ed / "fonts", dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("InterTight*"))   # (the Learn page's, not the editor's)
    shutil.rmtree(out / "learn", ignore_errors=True)  # ((Skiff) Learn Skiff: ../panel.js and ../fonts/, as in web/)
    shutil.copytree(HERE / "learn", out / "learn", ignore=shutil.ignore_patterns("local"))
    shutil.copy(HERE / "panel.js", out / "panel.js")
    shutil.copytree(HERE / "fonts", out / "fonts", dirs_exist_ok=True)
    shutil.rmtree(out / "try", ignore_errors=True)
    shutil.copytree(emu, out / "try")
    landing = (HERE / "index.html").read_text(encoding="utf-8").replace("@VERSION@", version)
    (out / "index.html").write_text(landing, encoding="utf-8")
    shutil.copy(HERE / "site.css", out / "site.css")
    print(f"site: {out}: webapp/installer, webapp/editor, learn, try, firmware/{name} ({len(raw)} B, FM-1_700)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
