#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Target-side cost estimate of the hot DSP code, from the pi32v2 objdump listing tools/build.py writes
(build/felucca.dis). For each engine's render function and the audio ISR: the instructions
inside loops (every address range closed by a backward branch: the per-sample loops and what they
contain), and the hardware divides and calls in them. cost = the loop instructions, each weighted 4 per
level of nesting (an inner loop runs several times per sample) and 1 + 8 for a divide (many cycles). A static count, not a cycle count: it changes only when the compiled code changes,
so it is exact run to run, and it catches a render loop that grew (more work per sample, a new divide,
something not inlined any more). Compared with tests/target_budget.txt at +10 %. The host
CPU check (regress.c) is the primary one; this one sees the target compiler's code.

What it measures: the work per sample of one function's own loops, as the target compiler emitted it.
What it does not: a callee's loops (they are that callee's line: DRUM_PATH adds the path's lines up, it
still does not run them), the cost of the calls themselves, memory / cache / DMA behaviour, or the
device's real load (a cycle count needs the hardware: none of these numbers is a measurement on an FM-1,
and none of them is a CPU margin).
  tests/target_budget.py [DIS [BUDGET]]      BUDGET_UPDATE=1 rewrites BUDGET (reviewed changes only)"""
import os
import re
import sys

FUNCS = ["analog_render", "digital_render", "digital_render_legacy", "digital_render_custom", "phase_render", "lofi_render", "formant_render",
         "wheel_render", "wheel_block", "analog_ext", "analog_morph", "analog_dirty", "lofi_byte_render",   # (JIANT 0.4: SYNC RING SAW3,
         # BYTE; 1.0-GW: TUBE FOLD)
         "drum_render", "noise_render", "fm6_render", "fm6_op_run", "fm6_op_fb",
         "dx_run", "dx_layer", "dx_run_sn",                 # DRUM-X (drumx_voice.c): a hit, osc + noise; (0.6.49) LAYER;
         # (1.0-GW) SNARE / CLAP
         "drum_out", "drum_side_ramp",                       # (QG-1 audit) the hit's output, and IMAGE pan moving:
         # noinline, on their own so drum_render keeps only PUNCH's loop; both were outside the budget
         "drum_side_add", "drum_add", "drum_mix",                       # (JIANT 0.6.48) DRUM-X IMAGE and SENDs
         "slicer_bus",                                      # (JIANT 0.6.3) the SLICER, one on a bus
         "fm1_alnk0_irq", "fm1_timer5_irq",               # the audio ISR; TIMER5: the key / LED scan (hal/fm1_input.h)
         "uac_tap48",                                       # the USB audio input at 48 kHz: the 44.1 -> 48 resampler (usb.c)
         "click_render",                                    # the metronome's click (click.c), while it sounds
         "mod_begin", "mod_voice", "mod_end",                 # the modulation matrix (mod.c), called when active
         "perf_begin", "perf_mute", "perf_pre", "perf_block", "perf_master",   # the FX layer (perform.c), when busy
         "rev_room", "rev_spring"]                # the reverb bus (fx.c): REVERB TYPE ROOM / SPRING
# built only with FELUCCA_FM4=1 (DIGITAL, src/eng_digital.c; not in the default build, so not in BUDGET): absent,
# they are skipped; present, checked against these (their budget lines until the engine was retired in 1.0)
OPTIONAL = {"digital_render": 12, "digital_render_legacy": 333, "digital_render_custom": 558,
            "uac_tap48": 504}           # (absent from a FELUCCA_UAC_48K=0 or FELUCCA_UAC=0 build)
# (QG-1 audit) the DRUM path as a whole: one hit's render, its output (plain / IMAGE / SEND) and the bus. The
# per-function lines can each sit under their budget while the path grows (a call added, work moved between them),
# so the total is printed every run; BUDGET_UPDATE=1 writes its baseline as DRUM_PATH. A path member that is
# missing already fails the FUNCS check above.
DRUM_PATH = ["dx_run", "dx_layer", "dx_run_sn", "drum_render", "drum_out", "drum_side_ramp",
             "drum_side_add", "drum_add", "drum_mix"]
TOL = 0.10                      # exact (no noise): small edits pass, a grown render loop does not
DIV_W = 8                       # a divide weighs 1 + 8 instructions
NEST = 4                        # an instruction in a loop inside a loop weighs 4, two deep 16, ...
MAXD = 4

dis = sys.argv[1] if len(sys.argv) > 1 else "build/felucca.dis"
budget = sys.argv[2] if len(sys.argv) > 2 else "tests/target_budget.txt"
LABEL = re.compile(r"^([A-Za-z_][A-Za-z_0-9.]*):$")
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
TARGET = re.compile(r"goto -?\d+ <[^>]*: ([0-9a-f]+) >")


def functions(path):
    """{name: [(addr, text)]}: a function runs to the next label not starting with '.' (the compiler's
    jump-table labels are inside functions); jump-table data ('< n : 0x.. >') is left out"""
    out, cur = {}, None
    with open(path) as f:
        for line in f:
            m = LABEL.match(line.strip())
            if m:
                name = m.group(1)
                if not name.startswith("."):
                    cur = out.setdefault(name, []) if name in FUNCS else None
                continue
            m = INSN.match(line)
            if cur is not None and m and not m.group(3).lstrip().startswith("<"):
                cur.append((int(m.group(1), 16), m.group(3).strip()))
    return out


def cost(insns):
    """loops: one span per loop head (the farthest backward branch to it); an instruction inside d
    spans weighs NEST ** (d - 1) (an inner loop runs several times per pass of the outer one)"""
    lo = insns[0][0]
    heads = {}
    for a, t in insns:
        m = TARGET.search(t)
        if m and lo <= int(m.group(1), 16) < a:
            h = int(m.group(1), 16)
            heads[h] = max(heads.get(h, a), a)
    n = divs = calls = 0
    w = 0
    for a, t in insns:
        d = min(sum(1 for h, e in heads.items() if h <= a <= e), MAXD)
        if not d:
            continue
        k = NEST ** (d - 1)
        n += 1
        if re.search(r"= r\d+ / r\d+", t):
            divs += 1
            k *= 1 + DIV_W
        calls += t.startswith("call")
        w += k
    return {"insns": len(insns), "loop": n, "div": divs, "call": calls, "cost": w}


def main():
    if not os.path.exists(dis):
        print(f"target: skip ({dis} missing: run ./build.sh)")
        return 0
    fns = functions(dis)
    res = {n: cost(fns[n]) for n in FUNCS if fns.get(n)}
    missing = [n for n in FUNCS if n not in res and n not in OPTIONAL]
    base = {}
    if os.path.exists(budget):
        for line in open(budget):
            p = line.split()
            if len(p) >= 2 and not line.startswith("#"):
                base[p[0]] = int(p[1])
    if os.environ.get("BUDGET_UPDATE"):
        with open(budget, "w") as f:
            f.write("# FELUCCA target cost budget (tests/target_budget.py): instructions in the loops of each\n"
                    f"# function in build/felucca.dis, x{NEST} per nesting level, divides x{1 + DIV_W}. The check allows "
                    f"+{TOL * 100:.0f} %.\n# Rewritten by BUDGET_UPDATE=1.\n")
            for n, r in res.items():
                if n not in OPTIONAL:
                    f.write(f"{n} {r['cost']}\n")
            path = [n for n in DRUM_PATH if n in res]
            if path:
                f.write(f"DRUM_PATH {sum(res[n]['cost'] for n in path)}\n")
        print(f"target: budget {budget} rewritten ({len(res)} functions)")
    fail = 0
    for n in missing:
        print(f"target: FAIL {n} not found in {dis} (renamed? inlined? update FUNCS)")
        fail += 1
    for n, r in res.items():
        b = base.get(n, OPTIONAL.get(n))
        state = "ok"
        if b is None:
            # (QG-1 stage 2) a function in FUNCS with no line used to pass as "no budget": a new render loop
            # would then run unchecked. One has to be measured and reviewed once, by hand or BUDGET_UPDATE=1
            if os.environ.get("BUDGET_UPDATE"):
                state = "no budget (written by BUDGET_UPDATE=1)"
            else:
                state = "FAIL no budget line (measure it, then BUDGET_UPDATE=1; review the cost first)"
                fail += 1
        elif not os.environ.get("BUDGET_UPDATE"):
            if r["cost"] > b * (1 + TOL):
                over = "any loop" if b == 0 else f"+{(r['cost'] / b - 1) * 100:.0f} %"
                state = f"OVER BUDGET ({over}, limit +{TOL * 100:.0f} %)"
                fail += 1
            elif b and r["cost"] < b * (1 - TOL):
                state = f"note: {(r['cost'] / b - 1) * 100:.0f} % (BUDGET_UPDATE=1 to keep it)"
        print(f"target: {n:15s} {r['insns']:5d} instructions, {r['loop']:4d} in loops, {r['div']} divides, "
              f"{r['call']:2d} calls there: cost {r['cost']:5d} (budget {b if b is not None else '-'}) {state}")
    path = [n for n in DRUM_PATH if n in res]
    if path:
        total = sum(res[n]["cost"] for n in path)
        b = base.get("DRUM_PATH")
        state = "ok"
        if b is None:
            if os.environ.get("BUDGET_UPDATE"):
                state = "no budget (written by BUDGET_UPDATE=1)"
            else:                               # (QG-1 stage 2) the path as a whole must be watched too
                state = "FAIL no DRUM_PATH line (measure it, then BUDGET_UPDATE=1)"
                fail += 1
        elif not os.environ.get("BUDGET_UPDATE"):
            if total > b * (1 + TOL):
                over = "any cost" if b == 0 else f"+{(total / b - 1) * 100:.0f} %"
                state = f"OVER BUDGET ({over}, limit +{TOL * 100:.0f} %)"
                fail += 1
            elif b and total < b * (1 - TOL):
                state = f"note: {(total / b - 1) * 100:.0f} % (BUDGET_UPDATE=1 to keep it)"
        print(f"target: DRUM path        (hit, out, IMAGE / SEND, bus): cost {total:5d} "
              f"(budget {b if b is not None else '-'}) {state}")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
