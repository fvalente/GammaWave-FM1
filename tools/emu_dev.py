#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (QG-1) The browser emulator while you work: serves build/emu, watches the sources and rebuilds on every change
# (firmware/src, web/emu: the wasm and the page; tools/, firmware/assets: the generated headers first), and the open
# page reloads itself once the new build is in (a build that fails leaves the last one running and shows the error).
#   python3 tools/emu_dev.py [--port 8000] [--no-open]
# Needs emcc on the PATH (source ~/emsdk/emsdk_env.sh) and one ./build.sh (build/gen); nothing but the standard library.
import argparse, http.server, json, os, subprocess, sys, threading, time, webbrowser

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "build", "emu")
WASM_SRC = ["firmware/src", "web/emu"]
GEN_SRC = ["tools", "firmware/assets"]
EXT = (".c", ".h", ".html", ".js", ".mjs", ".py", ".png", ".bdf", ".ttf", ".otf", ".json", ".txt", ".wav", ".sh")
state = {"build": 0, "ok": True, "log": "", "busy": False}
lock = threading.Lock()

RELOAD = b"""<script>(function(){let b=null;const box=document.createElement('pre');
box.style.cssText='position:fixed;left:0;right:0;bottom:0;max-height:40%;overflow:auto;margin:0;padding:8px;'+
'background:#300;color:#fcc;font:12px monospace;z-index:99999;display:none';document.documentElement.appendChild(box);
async function poll(){try{const r=await fetch('/__emu_dev',{cache:'no-store'});const s=await r.json();
if(b!==null&&s.build!==b&&s.ok){location.reload();return}b=s.build;box.style.display=s.ok?'none':'block';
box.textContent=s.ok?'':'emu_dev: the build failed, the last good one keeps running\\n\\n'+s.log}catch(e){}
setTimeout(poll,800)}poll()})();</script>"""


def snapshot(dirs):
    m = {}
    for d in dirs:
        for base, subs, files in os.walk(os.path.join(ROOT, d)):
            subs[:] = [s for s in subs if s not in ("__pycache__", ".git")]
            for f in files:
                if f.endswith(EXT):
                    p = os.path.join(base, f)
                    try:
                        m[p] = os.stat(p).st_mtime_ns
                    except OSError:
                        pass
    return m


def run(cmd):
    r = subprocess.run(cmd, cwd=ROOT, shell=True, capture_output=True, text=True)
    return r.returncode == 0, (r.stdout + r.stderr)[-6000:]


def rebuild(gen):
    with lock:
        state["busy"] = True
    t = time.time()
    ok, log = True, ""
    if gen or not os.path.exists(os.path.join(ROOT, "build/gen/felucca_tables.h")):
        ok, log = run(f'"{sys.executable}" -c "import sys; sys.path.insert(0, \'tools\'); import build; build.generate()"')
    if ok:
        ok, log2 = run("sh web/emu/build.sh")
        log += log2
    with lock:
        state.update(ok=ok, log=log, busy=False)
        if ok:
            state["build"] += 1
    print(f"emu_dev: {'built' if ok else 'BUILD FAILED'} in {time.time() - t:.1f} s" + ("" if ok else "\n" + log), flush=True)


def watch():
    w, g = snapshot(WASM_SRC), snapshot(GEN_SRC)
    while True:
        time.sleep(0.5)
        w2, g2 = snapshot(WASM_SRC), snapshot(GEN_SRC)
        if w2 != w or g2 != g:
            time.sleep(0.3)                       # (an editor's save in several writes: one rebuild)
            gen = g2 != g
            w, g = snapshot(WASM_SRC), snapshot(GEN_SRC)
            rebuild(gen)


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=OUT, **k)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *a):
        pass

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/__emu_dev":
            with lock:
                body = json.dumps({k: state[k] for k in ("build", "ok", "log", "busy")}).encode()
            return self.reply(body, "application/json")
        if path in ("/", "/index.html"):
            try:
                with open(os.path.join(OUT, "index.html"), "rb") as f:
                    html = f.read()
            except OSError:
                return self.reply(b"emu_dev: no build/emu/index.html yet", "text/plain", 503)
            return self.reply(html.replace(b"</body>", RELOAD + b"</body>", 1), "text/html; charset=utf-8")
        return super().do_GET()

    def reply(self, body, ctype, code=200):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    ap = argparse.ArgumentParser(description="the emulator, rebuilt and reloaded on every change")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--no-open", action="store_true")
    a = ap.parse_args()
    if subprocess.run("command -v emcc", shell=True, capture_output=True).returncode:
        sys.exit("emu_dev: no emcc on the PATH (source ~/emsdk/emsdk_env.sh)")
    rebuild(False)
    threading.Thread(target=watch, daemon=True).start()
    srv = http.server.ThreadingHTTPServer((a.host, a.port), Handler)
    url = f"http://{a.host}:{a.port}/"
    print(f"emu_dev: {url} (watching {', '.join(WASM_SRC + GEN_SRC)}; Ctrl+C stops)", flush=True)
    if not a.no_open:
        webbrowser.open(url)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
