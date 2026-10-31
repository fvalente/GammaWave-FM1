#!/usr/bin/env python3
"""Rewrite the Web Studio mock device's preset tables (web/editor.html, const ENG ... presets: [...]) from the
firmware's own description (build/host/desc.json, tests/descdump.c), so the simulator lists exactly the device's
presets. Run after adding or changing presets: python3 tools/sync_editor_mock.py [build/host/desc.json]"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
desc = json.loads(Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/desc.json").read_text())
page = ROOT / "web/editor.html"
html = page.read_text()
start = html.index("  const ENG = [")


def block(text, at):
    """[open, close) of the bracketed list opening at text[at] == '['"""
    depth = 0
    for i in range(at, len(text)):
        depth += {"[": 1, "]": -1}.get(text[i], 0)
        if depth == 0:
            return at, i + 1
    raise SystemExit("unbalanced brackets")


def preset(p):
    env = p["env"]
    args = [json.dumps(p["name"]), json.dumps(p["e"]).replace(",", ", ")]
    if env != [10, 70, 90, 60] or p["mono"] or p["pat"]:
        args.append(json.dumps(env).replace(",", ", "))
    if p["mono"] or p["pat"]:
        args.append(str(p["mono"]))
    if p["pat"]:
        args.append(str(p["pat"]))
    return f"P({', '.join(args)})"


changed = 0
for eng in desc["ENG"]:
    if not eng.get("name") or eng["name"] == "-":
        continue
    key = f'{{ name: "{eng["name"]}"'
    at = html.find(key, start)
    if at < 0:
        continue
    lst = html.find("presets: [", at)
    nxt = html.find("{ name: ", at + len(key))
    if lst < 0 or (0 <= nxt < lst):
        continue
    a, b = block(html, lst + len("presets: "))
    items = [preset(p) for p in eng["presets"]]
    lines, cur = [], "        "
    for it in items:
        if len(cur) + len(it) + 2 > 150 and cur.strip():
            lines.append(cur.rstrip())
            cur = "        "
        cur += it + ", "
    if cur.strip():
        lines.append(cur.rstrip().rstrip(","))
    new = "[\n" + "\n".join(lines) + "]" if items else "[]"
    if html[a:b] != new:
        html = html[:a] + new + html[b:]
        changed += 1
page.write_text(html)
print(f"sync_editor_mock: {changed} engine preset lists rewritten in {page.relative_to(ROOT)}")

# the factory PATTERNS[] (engines.c), one per line
html = page.read_text()
at = html.index("  const PATTERNS = [") + len("  const PATTERNS = ")
a, b = block(html, at)
new = "[\n" + ",\n".join("    " + json.dumps(p, separators=(",", ":")) for p in desc["PATTERNS"]) + "\n  ]"
if html[a:b] != new:
    page.write_text(html[:a] + new + html[b:])
    print("sync_editor_mock: PATTERNS rewritten")
