# SPDX-License-Identifier: GPL-3.0-only
# Pocket GammaWave (GW-1) cheat sheets: docs/cheatsheet/controls.html and sound.html,
# rendered to PNG and PDF via tools/render_cheatsheets.mjs:
#   python3 tools/gen_cheatsheets.py && node tools/render_cheatsheets.mjs
import html, os

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(R, 'docs', 'cheatsheet')
VERSION = 'v1.1.1-GW'

HEAT = ['#22d3ee', '#3b82f6', '#8b5cf6', '#d946ef', '#ef4444', '#ff7a00', '#facc15', '#f4f1ea']

CSS = """
@font-face { font-family: Chakra; src: url('../../assets/fonts/ChakraPetch-Medium.ttf'); font-weight: 500; }
@font-face { font-family: Chakra; src: url('../../assets/fonts/ChakraPetch-SemiBold.ttf'); font-weight: 600; }
@font-face { font-family: Chakra; src: url('../../assets/fonts/ChakraPetch-Bold.ttf'); font-weight: 700; }
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { background: #000; }
body { width: 1600px; color: #f4f1ea; font-family: Chakra, sans-serif; font-weight: 500; font-size: 17px;
       line-height: 1.32; padding: 36px 56px 40px; }
header { display: flex; align-items: center; gap: 36px; }
header img { height: 150px; }
header h1 { font-size: 56px; font-weight: 700; letter-spacing: 1px; line-height: 1; color: #ffb800; }
header h2 { font-size: 25px; font-weight: 500; color: #00d4ff; margin-top: 8px; }
header .ver { font-size: 18px; color: #b8b2c8; margin-top: 6px; }
.rule { height: 4px; margin: 22px 0 26px; border-radius: 2px;
        background: linear-gradient(90deg, #ffb800, #ff5500, #ef4444, #d946ef, #8b5cf6, #00d4ff, #facc15, #f4f1ea); }
.grid { display: grid; gap: 22px; }
.g2 { grid-template-columns: 1fr 1fr; } .g3 { grid-template-columns: 1fr 1fr 1fr; } .g4 { grid-template-columns: repeat(4, 1fr); }
.card { background: #100c16; border-radius: 12px; padding: 18px 20px 18px 24px; position: relative; }
.card::before { content: ''; position: absolute; left: 0; top: 14px; bottom: 14px; width: 5px; border-radius: 3px;
                background: var(--c); }
.card h3 { font-size: 23px; font-weight: 700; letter-spacing: .5px; color: var(--c); margin-bottom: 10px; }
.card h4 { font-size: 17px; font-weight: 600; color: #f4f1ea; margin: 12px 0 4px; letter-spacing: .4px; }
.sec { font-size: 30px; font-weight: 700; letter-spacing: 1px; margin: 30px 0 14px; display: flex; align-items: center; gap: 14px; }
.sec span { flex: 1; height: 2px; background: #2a2236; }
.k { display: inline-block; background: #2b2633; color: #f4f1ea; border-radius: 6px; padding: 0 7px; margin: 1px 1px;
     font-weight: 600; font-size: .92em; white-space: nowrap; }
.k.a { background: #ff7a00; color: #120a00; } .k.v { background: #b870ff; color: #12081c; }
.dim { color: #8f88a3; }
p + p { margin-top: 6px; }
table { width: 100%; border-collapse: collapse; }
td, th { text-align: left; vertical-align: top; padding: 5px 8px 5px 0; border-bottom: 1px solid #241d2e; }
th { color: #8f88a3; font-weight: 600; font-size: 15px; letter-spacing: .5px; }
tr:last-child td { border-bottom: 0; }
td.n { color: var(--c); font-weight: 700; white-space: nowrap; width: 1%; padding-right: 14px; }
.knobs { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; margin-bottom: 4px; }
.col { display: flex; flex-direction: column; gap: 22px; }
.knobs span { background: #1c1624; border-radius: 6px; text-align: center; padding: 2px 0; font-weight: 600; font-size: 15px; }
.knobs span.e { color: #4a4258; }
.pages td.p { white-space: nowrap; width: 1%; padding-right: 12px; font-weight: 600; }
.kb { position: relative; height: 150px; margin: 8px 0 4px; }
.kb .w { position: absolute; bottom: 0; height: 92px; border-radius: 6px; background: #1f1a27; border: 1px solid #2f2839;
         font-size: 12.5px; font-weight: 600; text-align: center; padding: 4px 2px 0; line-height: 1.1; color: #d8d2e4; }
.kb .w b, .kb .b b { display: block; font-size: 11px; color: #6f6882; font-weight: 600; margin-bottom: 2px; }
.kb .b { position: absolute; top: 0; height: 54px; border-radius: 6px; background: #2b2633; font-size: 12px; font-weight: 600;
         text-align: center; padding: 3px 1px 0; line-height: 1.05; color: #f4f1ea; }
.kb .on { background: var(--c); color: #0b0710; border-color: transparent; }
.kb .on b { color: rgba(0, 0, 0, .55); }
footer { margin-top: 30px; color: #8f88a3; font-size: 16px; display: flex; justify-content: space-between; }
ul { padding-left: 18px; } li { margin: 2px 0; }
"""

WHITE = ['F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4', 'C5', 'D5', 'E5', 'F5', 'G5']
BLACK = [('F#3', 0), ('G#3', 1), ('A#3', 2), ('C#4', 4), ('D#4', 5), ('F#4', 7), ('G#4', 8), ('A#4', 9), ('C#5', 11),
         ('D#5', 12), ('F#5', 14)]   # (name, the white key it follows)


def e(s):
    return html.escape(s)


def k(*names, cls=''):
    return ''.join(f'<span class="k {cls}">{e(n)}</span>' for n in names)


def keyboard(whites, blacks, width=692):
    """whites / blacks: dicts note -> label (only those set are lit)"""
    ww = width / 16.0
    out = ['<div class="kb">']
    for i, n in enumerate(WHITE):
        lab = whites.get(n, '')
        out.append(f'<div class="w{" on" if lab else ""}" style="left:{i * ww:.1f}px;width:{ww - 3:.1f}px">'
                   f'<b>{n}</b>{e(lab)}</div>')
    for n, after in BLACK:
        lab = blacks.get(n, '')
        x = (after + 1) * ww - ww * 0.36
        out.append(f'<div class="b{" on" if lab else ""}" style="left:{x:.1f}px;width:{ww * 0.72:.1f}px">'
                   f'<b>{n[:-1]}</b>{e(lab)}</div>')
    out.append('</div>')
    return ''.join(out)


def card(title, color, body, extra=''):
    return f'<div class="card" style="--c:{color}" {extra}><h3>{e(title)}</h3>{body}</div>'


def knobs(*labels):
    return '<div class="knobs">' + ''.join(
        f'<span class="{"e" if l == "-" else ""}">{e(l)}</span>' for l in labels) + '</div>'


def page(title, sub, body, name):
    doc = f"""<!doctype html><html><head><meta charset="utf-8"><title>POCKET GAMMAWAVE (GW-1) · {e(title)}</title>
<style>{CSS}</style></head><body>
<header><img src="signature.png" alt="Pocket GammaWave signature"><div>
<h1>POCKET GAMMAWAVE · {e(title)}</h1><h2>{e(sub)}</h2>
<div class="ver">firmware {VERSION} · M-VAVE FM-1 · gammawaveaudio.com · shadowtune.itch.io/gammawave · fvalente.github.io/GammaWave-FM1</div></div></header>
<div class="rule"></div>
{body}
<footer><span>GPL-3.0 · Companion to GammaWave Desktop VST (gammawaveaudio.com) · Built on Felucca, JIANT &amp; SLOOP</span>
<span>POCKET GAMMAWAVE (GW-1) {VERSION}</span></footer>
</body></html>"""
    with open(os.path.join(OUT, name), 'w', encoding='utf-8') as f:
        f.write(doc)


def sec(t):
    return f'<div class="sec">{e(t)}<span></span></div>'


# ------------------------------------------------------------------------------------------------ 1. CONTROLS ---
def controls():
    b = []
    b.append(sec('THE PANEL'))
    rows = [
        ('MASTER', 'Master volume.'),
        ('SELECT', '<b>Scrolls the section\'s pages</b> (HOME: the 49-genre generator). Held with a button, its <b>dice</b> (OK keeps, CANCEL back): HOME the generator, '
                   'EDIT the sound, SEQ the sequence, LFO the LFO, FX the effects. GLO + SELECT: the tempo. In the generator: its pages, the reel, the SET (round).'),
        ('PRESETS', 'Browses the sounds (factory and user). On step, list and song pages: the cursor / the row. In the generator: NOW and the <b>TAKES</b> you saved (scroll, round: a take plays on the next bar; <b>OCT+ held: the keys shape NOW</b>). '
                    '<b>Hold a page button + PRESETS: jump through its pages.</b>'),
        ('ALGORITHM', 'Selects the track from any page: T1–T4, or T1–T6 with TRACKS 6. In the generator: new instruments for the piece (the notes kept).'),
        ('KNOB 1–4', 'The four columns on screen. Fast sweeps the whole range, slow fine-tunes. With a layer held: '
                     'the layer\'s.'),
        ('OCT− · OCT+', 'The keyboard\'s octave (both together: back to 0). On action pages and dialogs: back / confirm.'),
        ('PLAY', 'Start / stop. GLO + PLAY: restart from the top.'),
        ('REC', 'Arms live recording on the selected track (notes and knob moves, overdub).'),
        ('HOME', 'Tap: the ecosystem. <b>Hold: the MENU.</b>'),
        ('KEYS', '27 keys play the selected track in its scale; with a layer held they become that layer\'s.'),
    ]
    t = '<table>' + ''.join(f'<tr><td class="n">{e(n)}</td><td>{d}</td></tr>' for n, d in rows) + '</table>'
    pb = [
        ('ENV', 'ENV · ENV DEST', '—'),
        ('LFO', 'LFO · LFO DEST · MOD · MSEQ · MSEQ 2', 'MACROS M1–M4 (stay latched)'),
        ('FX', 'TRACK · SLICER · DELAY · REVERB · CHORUS · MASTER (two quick taps: next section)',
         'PUNCH-IN FX'),
        ('SCL', 'SCL · SCL 2 · CHORD', 'ROOT by key'),
        ('EDIT', 'EDIT 1 · EDIT 2 · FILTER · VOICE  (FM6: OPERATOR · OP ENV · OPERATOR 2; DRUM: KIT · KIT 2 · KIT 3 · X-MOD · FILTER · SOUND 1–4 · LEVELS 1–2 · STEREO)', 'INIT · RECALL · sound mutes'),
        ('GLO', 'MIXER · GLOBAL · SYSTEM', 'MUTE · SOLO · LEVEL · TAP'),
        ('SAVE', 'PRESETS · USER · PROJECT · ARRANGE · TOOLS', 'SONG layer'),
        ('ARP', 'ARP · ARP 2', '—'),
        ('SEQ', 'STEP · NOTE · TIMING · STRETCH · COMP · COMP 2 · WHEN · PATTERN · SHIFT · HUMAN · PLAYHEADS · AUTOMATION · AUTO LIST · MUTATE', 'SEQ TOOLS'),
        ('REC', '(transport)', 'CLEAR · CLICK'),
    ]
    t2 = ('<table class="pages"><tr><th>BUTTON</th><th>TAP: ITS PAGES, ONE PER TAP</th><th>HOLD</th></tr>' +
          ''.join(f'<tr><td class="n">{e(n)}</td><td>{e(p)}</td><td class="dim">{e(h)}</td></tr>' for n, p, h in pb) +
          '</table>')
    b.append('<div class="grid g2">' + card('KNOBS AND TRANSPORT', HEAT[0], t) +
             card('PAGE BUTTONS', HEAT[2], t2 + '<p style="margin-top:10px">' + k('BUTTON', cls='v') + ' + ' +
                  k('PRESETS', cls='v') + ' jumps through its pages (no wrap). ' + k('SELECT', cls='v') + ' alone scrolls the section\'s pages.</p>') + '</div>')

    b.append(sec('LAYERS · HOLD A BUTTON'))
    fx_w = {'F3': 'REP 1/8', 'G3': 'REP 1/16', 'A3': 'REP 1/32', 'B3': 'LPF', 'C4': 'HPF', 'D4': 'OCT−', 'E4': 'OCT+',
            'F4': '½ TEMPO', 'G4': 'DEC−', 'A4': 'DEC+', 'B4': 'STUT 1/8', 'C5': 'STUT 1/16', 'D5': 'ATK+', 'E5': 'ARP',
            'F5': 'RAND', 'G5': 'ERASE'}
    fx_b = {'F#3': 'M1', 'G#3': 'M2', 'A#3': 'M3', 'C#4': 'M4', 'D#4': 'M5', 'F#4': 'M6', 'A#4': 'TGT'}
    glo_w = {'F3': 'SOLO 1', 'G3': 'SOLO 2', 'A3': 'SOLO 3', 'B3': 'SOLO 4', 'C4': 'SOLO 5', 'D4': 'SOLO 6',
             'E4': 'ALL', 'F4': 'TAP', 'G4': 'UNDO'}
    glo_b = {'F#3': 'M1', 'G#3': 'M2', 'A#3': 'M3', 'C#4': 'M4', 'D#4': 'M5', 'F#4': 'M6', 'G#4': 'KICK',
             'A#4': 'SNR', 'C#5': 'HAT', 'D#5': 'PERC'}
    sv_w = {'F3': 'PLAY A', 'G3': 'PLAY B', 'A3': 'PLAY C', 'B3': 'PLAY D', 'C4': 'STORE A', 'D4': 'STORE B',
            'E4': 'STORE C', 'F4': 'STORE D', 'D5': 'LOOP/ SONG', 'E5': 'SONG REC', 'G5': 'SONG'}
    ed_w = {'F3': 'INIT', 'G3': 'RECALL'}
    ed_b = {'F#3': 'BD', 'G#3': 'SD', 'A#3': 'CP', 'C#4': 'CH', 'D#4': 'OH', 'F#4': 'TM', 'G#4': 'RS', 'A#4': 'CB'}
    b.append('<div class="grid g2">')
    b.append(card('FX · PUNCH-IN EFFECTS', HEAT[5], keyboard(fx_w, fx_b) +
                  '<p>A key plays its effect while held (MENU FX LATCH ON: toggles); hold FX and let go untouched: <b>LOCKED</b> '
                  'open, FX again unlocks; OCT- all off. In on the next 1/16. Black 1–6 mute T1–T6, A#4 picks which tracks the MIDI ones '
                  'touch (all · synths · drums). With REC armed and playing they record into the section\'s lane; G5 '
                  'erases it.</p><h4>KNOBS</h4>' + knobs('FILTER', 'CRUSH', 'THROW', 'DEPTH')))
    b.append(card('GLO · MIX', HEAT[1], keyboard(glo_w, glo_b) +
                  '<p>Black 1–6 mute T1–T6 (latched), 7–10 the DRUM-X groups. White: solo while held, E4 unmutes all, '
                  'F4 tap tempo, G4 <b>UNDO</b> the last big change (again: redo), A4 <b>FREEZE</b> MUTATE. SELECT: the tempo. PLAY: restart.</p><h4>KNOBS</h4>' +
                  knobs('LEVEL T1', 'LEVEL T2', 'LEVEL T3', 'LEVEL T4') +
                  '<p class="dim" style="margin-top:4px">T5 or T6 selected: the knobs are T3–T6.</p>'))
    b.append(card('SAVE · SONG', HEAT[6], keyboard(sv_w, {}) +
                  '<p>PLAY A–D on the next bar (several in one hold: a quick chain), STORE the section into A–D. '
                  'OCT+ stores the playing section, OCT− recalls it. On the SONG page KNOB 4 stores a scene.</p>'
                  '<h4>KNOBS</h4>' + knobs('SONG 1–8', '-', '-', '-')))
    b.append(card('EDIT · SOUND', HEAT[3], keyboard(ed_w, ed_b) +
                  '<p>INIT: the factory sound (DRUM: the kit too). RECALL: the sound stored in the section. On a DRUM '
                  'track black 1–8 mute each DRUM-X sound.</p><h4>KNOBS</h4>' + knobs('EDIT 1', 'EDIT 2', 'EDIT 3', 'EDIT 4')))
    tk_w = {'F3': 'DRUM', 'G3': 'BASS', 'A3': 'CHRD', 'B3': 'ARP', 'C4': 'LEAD', 'D4': 'TEX', 'E4': 'ALL SND',
            'F4': 'HARM', 'G4': 'DROP DRUM', 'A4': 'DROP BASS', 'B4': 'DROP CHRD', 'C5': 'DROP ARP', 'D5': 'DROP LEAD',
            'E5': 'DROP TEX', 'F5': 'MIX', 'G5': 'VARY'}
    tk_b = {'F#3': 'KIT', 'G#3': 'BASS SND', 'A#3': 'PAD SND', 'C#4': 'LEAD SND', 'D#4': 'TEX SND', 'F#4': 'ARP SND',
            'G#4': 'ARR −', 'A#4': 'ARR +', 'C#5': 'ORIG', 'D#5': 'JAM', 'F#5': 'CODE'}
    b.append(card('GEN · OCT+ HELD: SHAPE NOW', HEAT[5], keyboard(tk_w, tk_b) +
                  '<p>GEN (OCT− + PLAY), any page but the SET&rsquo;s. The piece playing is <b>NOW</b>: hold <b>OCT+</b> and the keys '
                  'are silent and change NOW, on the next bar (stopped: at once) &mdash; <b>no take</b> (SAVE keeps NOW as one). The '
                  'whole screen maps the keys in their parts&rsquo; colours and says the key down in full. Left half: white F3–D4, let '
                  'go: that part&rsquo;s notes made again (KNOB 4 on a section: there only); the black key beside it: its <b>sound</b>. '
                  'Right half, mirrored: G4–E5 <b>drop</b> the same parts while held. <b>Hold a part or sound key ½ s: lock it</b>. '
                  'On a take&rsquo;s card: the key <b>borrows</b> its part. C#5 <b>ORIG</b>: back to the take NOW came from. D#5 '
                  '<b>JAM</b>: NOW changes by itself (held ½ s: every 4, 8, 16, 32 bars). F#5: the <b>share code</b>. Held to '
                  'read the map, let go: nothing.</p>'
                  '<h4>KNOBS ON THE REEL</h4>' + knobs('TEMPO', 'KEY', 'ENERGY', 'SECTION')))
    ln_w = {n: str(i + 1) for i, n in enumerate(WHITE)}
    reel = [
        ('SAVE', '<b>NOW kept as a take</b> (the next number). OCT+ held + SAVE: type a <b>share code</b>.'),
        ('PRESETS', 'NOW, then the takes, round: a take plays on the next bar. Stopped: OCT+ or PLAY.'),
        ('K1 · K2 · K3', '<b>TEMPO</b> at once; <b>KEY</b>, <b>ENERGY</b>: NOW made again once the knob rests.'),
        ('ALGORITHM', 'New instruments for NOW, the notes kept.'),
        ('GLO + G4 · A4', '<b>UNDO</b> NOW a step (8) · forward. At its start: the music before GEN.'),
        ('EDIT', 'Delete the take (OCT+ yes). Held: every take.'),
        ('REC', 'Playing: record a <b>SET</b> of the takes you play.'),
        ('SELECT', 'The SET page: K1 entry (playing: it next bar), K2 take, K3 bars, PLAY.'),
    ]
    tr = '<table>' + ''.join(f'<tr><td class="n">{e(a)}</td><td>{d}</td></tr>' for a, d in reel) + '</table>'
    b.append(card('GEN · NOW, THE TAKES · OCT− HELD: PLAY THEM', HEAT[6], keyboard(ln_w, {}) +
                  '<p>Hold <b>OCT−</b>: the white keys play <b>takes 1–16</b> on the next bar (stopped: at once), like clips; '
                  'the screen shows the one playing and the next. A tap of OCT− still goes back.</p>' + tr +
                  '<p style="margin-top:8px">NOW&rsquo;s card says TAKE n or EDITED and where it came from; a take&rsquo;s card its ID, '
                  'its parent and what changed. What a control did shows in a band at the foot of the screen. The takes '
                  '(32), the SET and JAM are kept over power-off.</p>'))
    b.append('</div>')
    small = [
        ('LFO · MACROS', HEAT[2], 'Knobs M1–M4; they stay latched when let go (LFO again: off). F3 rolls new routes, '
                                  'G3 clears them (only tracks that sound). LFO + SELECT: the track\'s LFO diced.', ('M1', 'M2', 'M3', 'M4')),
        ('SCL · SCALE', HEAT[0], 'Any key sets the ROOT. ROOT and SCALE are every melodic track\'s (and RESO\'s).',
         ('ROOT', 'SCL', 'OFS', 'PIT')),
        ('SEQ · TOOLS', HEAT[4], 'On the SEQ pages: F3 CLEAR · G3 REVERSE · A3 B3 SHIFT · C4 RANDOM · D4 COOK · E4 BEAT · F4 CLR AUTO (the automation).',
         ('LEN', 'DIV', 'SWING', 'GATE')),
        ('REC · RECORD', '#ef4444', 'F3 CLEAR the track (at once; GLO + G4 undoes it), G3 the metronome CLICK.', ('CLICK', '-', '-', '-')),
    ]
    b.append('<div class="grid g4" style="margin-top:22px">' + ''.join(
        card(t, c, f'<p>{e(d)}</p><h4>KNOBS</h4>' + knobs(*kn)) for t, c, d, kn in small) + '</div>')

    b.append(sec('COMBOS & [OCT−] SHIFT DECK'))
    combos = [
        (k('OCT−', cls='a') + ' + ' + k('ARP', cls='a'), '<b>HEARTH MACROS (1/6–6/6)</b>: Gravity, Singularity, Photosphere, Wraith Scream, Formant Vowel, Solar Eclipse — drive type and amount, cutoff, resonance (4–6: SCREAM / VOWEL filter). Undo: OCT− + GLO.'),
        (k('OCT−', cls='a') + ' + ' + k('PLAY', cls='a'), '<b>49-GENRE SONG GENERATOR (GEN)</b>: Trap ATL, UK Drill, Dark Trap, Rage Trap, house, techno, IDM, ambient… KNOB 1 genre, KNOB 2 seed, KNOB 3 energy, KNOB 4 form; SELECT pages (parts, SONG: tempo key scale length, FEEL: space fills wet grit), ALGORITHM new instruments, the piece playing is NOW (every change shapes it, no take), SAVE keeps it as a take; PRESETS the TAKES reel (kept over power-off: scroll to play one, K1 tempo, K2 key, K3 energy, K4 a section, EDIT deletes; GLO + G4 / A4 NOW back / forward; hold OCT+: F3..D4 a part made again, black keys its sound, G4..E5 drop a part while held, C#5 back to the take, D#5 JAM, F#5 the share code; hold OCT−: the white keys play takes 1–16; REC a SET); OCT+ makes, OCT− back, PLAY auditions.'),
        (k('OCT−', cls='a') + ' + ' + k('ENV / SCL', cls='a'), '<b>SEQ &gt; MUTATE (1-CLICK)</b>: jumps to MUTATE; if it was off, switches it on (Sound 2 bars, Pattern 4 bars, Amount 50%).'),
        (k('OCT−', cls='a') + ' + ' + k('HOME / SAVE', cls='a'), '<b>HYPERSPACE VISUALIZER</b>: 4 animated scenes (Supernova, Wraith 3D, Warp Tunnel, Matrix FFT); OCT+ next scene, OCT− leaves.'),
        (k('OCT−', cls='a') + ' + ' + k('SEQ · EDIT · FX · LFO', cls='v'), '<b>1-HAND DICE</b>: rolls Sequence, Sound Patch, Track FX, or LFO on the active track.'),
        (k('OCT−', cls='a') + ' + ' + k('GLO', cls='v'), '<b>UNDO</b>, again: REDO (same as GLO + G4).'),
        (k('HOME · EDIT · SEQ · LFO · FX', cls='v') + ' + ' + k('SELECT', cls='v'), 'HOME: the 49-genre generator (GEN). The others dice: sound · sequence · LFO · effects (KEEP? OK / CANCEL).'),
        (k('PAGE BUTTON', cls='v') + ' + ' + k('PRESETS', cls='v'), 'Jump through that button\'s pages.'),
        (k('REC') + ' + ' + k('FX') + k('EDIT') + k('ENV') + k('LFO') + k('SCL') + k('ARP') + k('GLO'),
         'Clear that part\'s automation on the selected track (FX: the punch-in lane too).'),
        (k('SEQ') + ' + ' + k('REC'), 'CLEAR ALL SEQUENCES? (OCT+ confirms).'),
        (k('SAVE') + ' + ' + k('OCT+') + ' / ' + k('OCT−'), 'Store / recall the playing section.'),
        (k('GLO') + ' + ' + k('PLAY'), 'Restart from the top.'),
        (k('GLO') + ' + ' + k('SELECT'), 'Tempo (from any page).'),
        (k('STEP HELD', cls='a') + ' + ' + k('KNOB'), 'A parameter lock on that step (roll, grid).'),
        (k('HOME', cls='a') + ' held', 'MENU: knob accel, FX latch, hold time, USB level, palettes, animation…'),
    ]
    t3 = '<table>' + ''.join(f'<tr><td style="white-space:nowrap;width:1%;padding-right:18px">{a}</td><td>{d}</td></tr>'
                             for a, d in combos) + '</table>'
    b.append(card('GAMMAWAVE [OCT−] SHIFT SHORTCUTS & BUTTON COMBOS', HEAT[7], t3))

    b.append(sec('EVERY PAGE · KNOB 1 – 4'))
    fams = [
        ('ENV', HEAT[0], [('ENV', 'ATK DEC SUS REL'), ('ENV DEST', 'FILTER PITCH SHAPE LOOP')]),
        ('LFO', HEAT[1], [('LFO', 'RATE WAVE PHASE FADE'), ('LFO DEST', 'PITCH FILTER SHAPE AMP'), ('MOD', 'SLOT SRC DEST AMT'),
                          ('MSEQ', 'STEP LEVEL LEN SLEW'), ('MSEQ 2', 'DIV - - -')]),
        ('FX', HEAT[5], [('FX', 'DIST CHORUS DLY REV'), ('DIST', 'DIST TYPE TONE -'), ('AUTO PAN', 'DEPTH RATE SYNC WAVE'), ('SLICER', 'MODE PAT RATE DEPTH'),
                         ('SL BUS', 'T1 T2 T3 T4'), ('SL BUS 2', 'T5 T6 FX -'), ('SL PITCH', 'STEP PITCH LEN QUANT'),
                         ('DLY', 'TIME FEEDBK TONE MIX'), ('DLY 2', 'WIDE PITCH SPRAY MOD'), ('DLY 3', 'TYPE PING - -'),
                         ('REVERB', 'TYPE SIZE DAMP PRE/AIR'), ('REVERB 2', 'MOD RATE FILT WIDE'),
                         ('CHORUS', 'RATE DEPTH STEREO -'), ('MASTER', 'CLIP PUNCH DUCK REL')]),
        ('EDIT', HEAT[3], [('EDIT 1', 'E1 E2 E3 E4'), ('EDIT 2', 'E5 E6 E7 E8'), ('RETRIG', 'RTRG - - -'), ('OPERATOR', 'OP RATIO FINE LEVEL'),
                           ('OP ENV', 'OP STAGE RATE LEVEL'), ('OPERATOR 2', 'OP MODE DETUNE VEL'), ('FILTER', 'TYPE CUT RES DRIVE'),
                           ('VOICE', 'VOICE GLIDE MODE PRIO'), ('KIT 3', 'BP FEEDBK - -'),
                           ('X-MOD', 'RATE DEPTH SHAPE RAND'), ('LEVELS', 'KICK SNARE CLAP HATCL'),
                           ('LEVELS 2', 'HATOP TOM RIM BELL'), ('SOUND', 'SOUND SIDE PITCH PMOD'),
                           ('SOUND 2', 'SOUND DECAY NOISE COLOR'), ('SOUND 3', 'SOUND MODE PM.TYPE DRIVE'),
                           ('SOUND 4', 'SOUND LAYER FEEDBK SEND'), ('STEREO', 'IMAGE - - -')]),
        ('SCL · ARP', HEAT[2], [('SCL', 'ROOT SCALE QUANT TRANS'), ('SCL 2', 'ALL TR - - -'), ('CHORD', 'CHORD VOICING STRUM -'), ('ARP', 'MODE RATE OCT GATE'),
                                ('ARP 2', 'SWING PROB HOLD ORDER')]),
        ('SEQ', HEAT[4], [('STEP', 'STEP NOTE TIME FLAG'), ('STEP · DRUM', 'STEP LANE HIT ACC'), ('COMP', 'STEP TYPE VAL VAL'), ('COMP 2', 'STEP TYPE VAL VAL'), ('WHEN', 'STEP A B LEN'), ('PATTERN', 'LEN DIV SWING GATE'), 
                          ('AUTOMATION', 'PLAY EVENT LOCK CLEAR'), ('AUTO LIST', 'ROW STEP PARAM VALUE'),
                          ('MUTATE', 'SOUND PATTERN AMOUNT MODE'), ('PLAYHEADS', 'HEAD RATE DIR INTVL'), ('NOTE', 'FOCUS PITCH MOVE LEN'), ('TIMING', 'STEP OFFSET REC Q -'), ('STRETCH', 'x/÷ REPEAT NUDGE LEN'), ('HUMAN', 'VEL TIME GATE -'), ('SHIFT', 'OFFSET PITCH CHANCE C.LEN')]),
        ('GLO · SAVE', HEAT[6], [('MIXER', 'LEVEL PAN REV MUTE'), ('GLOBAL', 'BPM SWING CLOCK TUNE'),
                                 ('SYSTEM', 'TRACKS LOAD ROUTE CPU'), ('PRESETS', 'No. ENG FAV LIST'),
                                 ('USER', 'SLOT LOAD ERASE SAVE'), ('PROJECT', 'PROJ SECT LOAD SAVE'), ('ARRANGE', 'ROW SECT BARS SCENE'),
                                 ('TOOLS', 'CLEAR SEQ · INIT SOUND · DEL ROW · CLEAR SONG · NEW PROJECT · CPU TEST: K1 picks, OCT+')]),
    ]

    def ptable(rows):
        out = '<table class="pages">'
        for p, ks in rows:
            parts = ks.split(' ')
            if len(parts) == 4:
                cells = knobs(*parts)
            else:
                cells = f'<span class="dim">{e(ks)}</span>'
            out += f'<tr><td class="p">{e(p)}</td><td>{cells}</td></tr>'
        return out + '</table>'
    cards = {n: card(n, c, ptable(r)) for n, c, r in fams}
    b.append('<div class="grid g3"><div class="col">' + cards['ENV'] + cards['LFO'] + cards['SCL · ARP'] + '</div>' +
             cards['FX'] + cards['EDIT'] + '</div>')
    b.append('<div class="grid g2" style="margin-top:22px">' + cards['SEQ'] + cards['GLO · SAVE'] + '</div>')
    page('CONTROLS', 'every button, knob, layer, combo and page', ''.join(b), 'controls.html')


# ------------------------------------------------------------------------------------------- 2. SOUND & FX ---
def sound():
    b = []
    b.append(sec('ENGINES · EDIT 1 / EDIT 2'))
    eng = [
        ('ANALOG', HEAT[1], 'WAVE DETUNE MIX NOISE', 'CUT RES DRIVE INT',
         'SAW SQR TRI SIN PWM <b>SYNC RING SAW3</b>; INT: osc 2 ±24 st. Its filter is per voice.'),
        ('FM6', HEAT[3], 'ALG FB MOD RATIO', 'MEG VMOD DETUNE SLOT',
         'Dexed\'s 6 operators, 32 algorithms drawn, .syx patches (SLOT), MOD / RATIO move the modulators. '
         '<b>OPERATOR · OP ENV · OPERATOR 2</b>: each operator on the device (ratio / fixed, fine, level, its envelope, '
         'mode, detune, velocity). <b>SLOT B1–B32</b>: a 32-voice .syx sent from the web editor.'),
        ('PHASE', HEAT[2], 'WAVE WAVE2 DCW ENV', 'DETUNE LINE SUB FB',
         'Phase distortion (CZ); <b>FB</b> feeds the output back into the phase: hard edge to growl.'),
        ('LOFI', HEAT[0], 'CHIP WAVE DUTY CRUSH', '- VIB BEND MASK',
         'Chip: PLS TRI SAW NOIS WRAM. <b>BYTE</b>: ALGO VAR · MOTN GRIT BEND LOOP. <b>FLOAT</b> (floatbeat): '
         'ALGO VAR · MOTN FBK BEND LOOP; CHIP sets FULL / 4BIT / 1BIT.'),
        ('VOICE', HEAT[6], 'VOWEL VOWEL2 TALK SHIFT', 'BUZZ BREATH Q RAND', 'Formant voice (after klattsch): vowels, talk, breath.'),
        ('WHEEL', HEAT[5], 'REG SUB BODY TOP', 'PERC CLICK DRIVE ROTOR', 'Drawbar organ: registrations, percussion, rotary.'),
        ('NOISE', HEAT[4], 'MODE COLOR FREQ PEAK', 'TRK DENSE DRIFT CRUSH', 'Coloured and metallic noise, tracked or free.'),
        ('DRUM-X', '#ff7a00', 'MORPH TUNE TONE DECAY', 'NOISE FM ENV FOLD DRIVE',
         '8 sounds BD SD CP CH OH TM RS CB, sides A / B: <b>MORPH</b> morphs the kit. The kit first: <b>KIT 3</b> BP FEEDBK, <b>X-MOD</b> RATE DEPTH SHAPE '
         'RAND. Then a sound (KNOB 1 the sound): SIDE PITCH PMOD · DECAY NOISE COLOR · MODE PM.TYPE DRIVE · <b>LAYER</b> (no note) FEEDBK '
         '<b>SEND</b> (CHO / DLY / REV). Last LEVELS and <b>STEREO</b> IMAGE (a knob through the kit\'s stereo placings).'),
    ]
    t = '<table><tr><th>ENGINE</th><th>EDIT 1</th><th>EDIT 2</th><th>WHAT TO KNOW</th></tr>'
    for n, c, e1, e2, d in eng:
        t += (f'<tr><td class="n" style="--c:{c}">{e(n)}</td><td style="width:22%">{knobs(*e1.split())}</td>'
              f'<td style="width:22%">{knobs(*e2.split())}</td><td>{d}</td></tr>')
    t += '</table>'
    b.append(card('EIGHT ENGINES, ALL SYNTHESIS', HEAT[7], t))
    b.append('<div class="grid g3" style="margin-top:22px">')
    b.append(card('SPECTROGRAM FILTER & WRAITH BAR', HEAT[3], knobs('TYPE', 'CUT', 'RES', 'DRV') +
                  '<p style="margin-top:8px"><b>LP · HP · BP · COMB · VOWEL · SCREAM</b>, drawn over the audio-reactive spectrum. '
                  'The Wraith ribbon follows CUT through six zones (<b>FLU · WHI · VOX · CHR · BEL · SCR</b>; vowels <b>OO · OH · AH · EH · EE</b> with VOWEL / SCREAM or the VOICE engine). '
                  '<b>OCT− + ARP</b> steps through 6 Hearth macros.</p>'))
    b.append(card('VOICE · PLAYING', HEAT[2], knobs('VCE', 'GLD', 'GLMOD', 'PRIO') +
                  '<p style="margin-top:8px">POLY · MONO · LEG · UNI; glide and its mode; LAST / LOW / HIGH priority. '
                  '<b>8 voices</b> shared by the six tracks (MONO and LEGATO keep their note).</p>'))
    b.append(card('49-GENRE GENERATOR & DICE', HEAT[5], '<p><b>OCT− + PLAY</b> (or SELECT on HOME): 49-Genre Song Generator '
                  'including <b>TRAP ATL</b>, <b>UK DRILL</b> (3-3-2 tresillo bounce), <b>DARK TRAP</b> (808 cowbells), '
                  '<b>RAGE TRAP</b>, house, techno, IDM, ambient… KNOB 1 genre, SELECT its pages (SONG: tempo, key, scale, length; FEEL: space for a voice, fills, wet, grit), OCT+ makes NOW, SAVE keeps it as a <b>take</b> (hold OCT+: the keys remake a part, a sound, a section; hold OCT−: the keys play the takes; JAM; SET; share codes). <b>OCT− + ENV</b>: 1-click <b>SEQ &gt; MUTATE</b> (ORBIT / DRIFT; on the page OCT− FREEZE, OCT+ KEEP). '
                  '<b>OCT− + GLO</b>: Undo.</p>'))
    b.append('</div>')

    b.append(sec('MODULATION'))
    b.append('<div class="grid g3">')
    b.append(card('ENV · LFO', HEAT[0],
                  '<h4>ENV</h4>' + knobs('ATK', 'DEC', 'SUS', 'REL') + '<h4>ENV DEST</h4>' + knobs('FLT', 'PIT', 'SHP', 'LOOP') +
                  '<p style="margin-top:6px">LOOP: held, the envelope cycles from the sustain: an ADSR-shaped LFO.</p>'
                  '<h4>LFO</h4>' + knobs('RATE', 'WAVE', 'PHS', 'FADE') + '<p class="dim">SIN TRI SAW SQR S&amp;H</p>'
                  '<h4>LFO DEST</h4>' + knobs('PIT', 'FLT', 'SHP', 'AMP')))
    b.append(card('MOD MATRIX · 4 SLOTS A TRACK', HEAT[2],
                  knobs('SLOT', 'SRC', 'DST', 'AMT') +
                  '<h4>SOURCES</h4><p>' + k('LFO', 'ENV', 'VEL', 'KEY', 'RAND', 'MODW', 'AT', 'EXPR', 'M1', 'M2', 'M3', 'M4',
                                             'STEP') + '</p>'
                  '<h4>DESTINATIONS</h4><p>' + k('PITCH', 'CUT', 'SHP', 'AMP', 'PAN', 'DIST', 'CHO', 'DLY', 'REV', 'RATE',
                                                  'VIB', 'E1…E8', 'CLIP', 'PNCH') + '</p>'
                  '<p style="margin-top:6px">E1…E8: the engine\'s own EDIT knobs. CLIP / PNCH: the master.</p>'))
    b.append(card('MSEQ · MACROS', HEAT[1],
                  '<h4>MSEQ · A CV SEQUENCER PER TRACK</h4>' + knobs('STEP', 'LVL', 'LEN', 'SLEW') +
                  '<p style="margin-top:6px">16 levels, DIV on MSEQ 2, in time with the transport. Use it from MOD, '
                  'SRC STEP.</p><h4>MACROS M1–M4</h4><p>Hold LFO: the knobs are M1–M4, latched until LFO again. Each '
                  'session starts with 2 random routes a track; F3 rolls new ones, G3 clears. By hand: MOD, '
                  'SRC M1…M4. Saved with the project.</p>'))
    b.append('</div>')
    b.append('<div class="grid g3" style="margin-top:22px">')
    b.append(card('SEQUENCER', HEAT[4],
                  '<p>64 steps a track, up to 4 notes a step, piano roll and drum grid. <b>Hold a step + turn a knob</b>: '
                  'a parameter lock. Step recording: hold a chord + KNOB 1 its length; + ARP: arpeggiated (K1 rate, K2 order, K3 oct). COMP / COMP 2: two components a step (CHANCE, RATCH, ARP NOTES / SEQ / SCALE, TRIG, RAND, OCT, TONAL, BEND, ECHO, PULSE, JUMP, REV: TIMES x1 x2 1:2, FILL, KILL); a TRIG or FILL wins; RATCH / ECHO: DIV; WHEN: each one\'s loops (ALWAYS 1:2 2:2 SKIP) and the step\'s LEN. SHIFT\'s CHANCE thins the whole track; C.LEN gives the components a length of their own (they drift over the steps). REC + knobs while playing: '
                  'automation (64 records a section, MOTION page plays / clears them, AUTO LIST edits them).</p>'
                  '<h4>PATTERN · SHIFT</h4>' + knobs('LEN', 'DIV', 'SWG', 'GATE') + knobs('OFS', 'PIT', 'CHN', 'CLEN')))
    b.append(card('SCALE · ARP', HEAT[0],
                  '<p><b>ROOT and SCALE</b> are every melodic track\'s: CHR MAJ MIN DOR MIX PEN MPEN HARM PHRY LYD LOC MEL '
                  'BLUES WHOLE DIMHW DIMWH. QNT snaps keys or the sequence, TRN transposes, <b>ALL TR</b> (SCL 2) moves '
                  'every sequence along the scale. <b>CHORD</b>: a key plays a chord (TRIAD 7TH ADD9 SUS2 SUS4 in key, '
                  'MAJ MIN 7 MAJ7 MIN7 POWER), VOICING LEAD leads each to the next, STRUM as a harp.</p><h4>ARP MODES</h4><p class="dim">OFF UP DN UPDN RND ORD REPEAT DNUP '
                  'UP+8 CONV DIVG PINKY THUMB WALK CHORD <b style="color:#f4f1ea">TRNS</b> (the keys transpose the '
                  'sequence)</p>'))
    b.append(card('SONGS', HEAT[6],
                  '<p><b>8 projects × 4 sections (A–D)</b>, a song of up to 16 rows by bars (SAVE › ARRANGE; END: STOP, x2–x8, LOOP), SLOOP style. '
                  'Each row can hold a <b>scene</b>: mutes, DRUM-X group mutes, macros and MIDI punch-ins. SAVE held '
                  'plays sections live on the next bar, stores them, records the song (SONG REC). Each section keeps its '
                  'sounds, steps, motion, kit and its SLICER.</p>'))
    b.append('</div>')

    b.append(sec('EFFECTS'))
    b.append('<div class="grid g3">')
    b.append(card('PER TRACK', HEAT[5], '<h4>FX · SENDS</h4>' + knobs('DST', 'CHO', 'DLY', 'REV') +
                  '<h4>DIST</h4>' + knobs('DST', 'TYPE', 'TONE', '-') +
                  '<p style="margin-top:6px">SOFT · HARD · FOLD · CRUSH · RECT.</p>'
                  '<h4>AUTO PAN</h4>' + knobs('DEPTH', 'RATE', 'SYNC', 'WAVE') +
                  '<p style="margin-top:6px">The track\'s pan swung L–R around its PAN. SYNC <b>BEAT</b>: RATE 4 BARS … 1/32, '
                  'in time with the bar; <b>FREE</b>: in Hz. SIN · TRI · SAW · SQR · S&amp;H, never a click.</p>'))
    b.append(card('SLICER · ONE ON A BUS', HEAT[3], knobs('SLCR', 'PAT', 'RATE', 'DEPTH') +
                  '<p style="margin-top:6px">OFF · GATE · STUT, 16 patterns, 1/8 … 32T. <b>SL BUS</b>: T1–T6 and '
                  '<b>FX</b> (the delay / reverb / chorus returns) ON / OFF: everything on = the whole mix chopped after '
                  'the effects. <b>SL PITCH</b>: the repeats\' 16-step pitch (±24 st, tape style, QNT SCL to the scale).</p>'))
    b.append(card('DELAY · CHORUS', HEAT[1], '<h4>DLY</h4>' + knobs('TIME', 'FDBK', 'TONE', 'MIX') +
                  '<h4>DLY 2</h4>' + knobs('WIDE', 'PITCH', 'SPRY', 'MOD') + '<h4>DLY 3</h4>' + knobs('TYPE', 'PING', '-', '-') +
                  '<p style="margin-top:6px">Synced or free (5 ms – 1.48 s); PITCH / SPRY: a grain delay.</p>'
                  '<h4>CHORUS</h4>' + knobs('RATE', 'DEPTH', 'STER', '-') +
                  '<p style="margin-top:6px">STER: 0 as always, down to mono, up to twice as wide.</p>'))
    b.append('</div>')
    rv = [
        ('ROOM', 'Classic room', 'decay', 'darkens', 'chorus on the tail', 'its speed', 'stereo'),
        ('SPRING', 'Amp spring tank', 'length · decay', 'darkens', 'wobble', 'wobble speed', 'stereo'),
        ('SHIMR', 'Up an octave on every pass; <b>PRE → AIR</b> (±24 st: down darker and lower, up airier and higher)', 'decay', 'darkens', 'how much shimmer', '—', '—'),
        ('RESO', '4 strings tuned to the scale; <b>PRE → PITCH</b> (0–14 steps up), <b>DAMP → FOLW</b> (OFF / T1–T6: the '
                 'chord follows that track\'s notes)', 'how long they ring', 'FOLW', 'past half: the chord opens', '—', '—'),
        ('CLOUD', 'Granular wash; SIZE max <b>freezes</b> it; <b>PRE → AIR</b> (±24 st: the cloud down / up, darker / airier)', 'decay · freeze', 'darkens', 'grains pitched + chorus',
         'grain density', 'grain spread'),
    ]
    t = ('<table><tr><th>TYPE</th><th>WHAT IT IS</th><th>SIZE</th><th>DAMP</th><th>MOD</th><th>RATE</th><th>WIDE</th></tr>' +
         ''.join(f'<tr><td class="n">{e(a)}</td><td>{b_}</td><td>{e(c)}</td><td>{e(d)}</td><td>{e(f)}</td><td>{e(g)}</td>'
                 f'<td>{e(h)}</td></tr>' for a, b_, c, d, f, g, h in rv) + '</table>')
    b.append('<div class="grid g2" style="margin-top:22px;grid-template-columns:2fr 1fr">')
    b.append(card('REVERB · FIVE MODELS', HEAT[2], knobs('TYPE', 'SIZE', 'DAMP', 'PRE') + knobs('MOD', 'RATE', 'FILT', 'WIDE') +
                  '<div style="margin-top:10px">' + t + '</div>'))
    b.append(card('MASTER', HEAT[6], knobs('CLIP', 'PNCH', 'DUCK', 'REL') +
                  '<p style="margin-top:8px"><b>CLIP</b> saturates without getting louder. <b>PNCH</b> drives the drums: '
                  'denser body. <b>DUCK</b>: the kick pulls the rest down (REL its release). An always-on '
                  '<b>leveler</b> (slow 2:1, −9…+6 dB) before the limiter keeps patches at a steady volume.</p>'
                  '<h4>PUNCH-IN FX (FX HELD)</h4><p>REPEAT 1/8 · 1/16 · 1/32, LPF, HPF; on the notes: OCT−/+, ½ TEMPO, '
                  'DEC−/+, STUTTER, ATK+, ARP, RANDOM. Knobs: FILTER CRUSH THROW DEPTH. Recordable per section.</p>'))
    b.append('</div>')
    page('SOUND · FX · MOD', 'engines, effects, modulation, sequencer and songs', ''.join(b), 'sound.html')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    controls()
    sound()
    print('docs/cheatsheet/controls.html, sound.html')
