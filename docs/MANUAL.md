# QUANTUM GAMMAWAVE (QG-1) — User Manual

**Firmware v1.2.1-GW for the M-VAVE FM-1 · [gammawaveaudio.com](https://gammawaveaudio.com/) · [Buy GammaWave VST](https://shadowtune.itch.io/gammawave)**

> *Versión en español: [MANUAL.es.md](MANUAL.es.md).*

This manual is for musicians who have just installed Quantum GammaWave (QG-1) [fork of Pocket GammaWave] and want to know how to play it, build songs with it
and, above all, **save their work without losing anything**. It follows the firmware as it is in v1.2.1-GW: every
button, knob and message is named exactly as the device shows it.

If you only read one chapter, read [12. Saving and loading](#12-saving-and-loading). What GW-1 adds to the firmware it
is built on (the `[OCT−]` shift deck, the generator's new genres, the VOWEL / SCREAM filters, the new drive types, the
GammaWave screens) is gathered in [17. GW-1 additions](#17-gw-1-additions).

## Contents

0. [Quick start](#0-quick-start)
1. [The panel](#1-the-panel)
2. [The screen](#2-the-screen)
3. [Pages and layers](#3-pages-and-layers)
4. [Engines](#4-engines)
5. [DRUM-X](#5-drum-x)
6. [The sequencer](#6-the-sequencer)
7. [ARRANGE: songs, sections and scenes](#7-arrange-songs-sections-and-scenes)
8. [Dice and UNDO](#8-dice-and-undo)
9. [Effects and master](#9-effects-and-master)
10. [Modulation and macros](#10-modulation-and-macros)
11. [Performance](#11-performance)
12. [Saving and loading](#12-saving-and-loading)
13. [System: TRACKS, LOAD and the MENU](#13-system-tracks-load-and-the-menu)
14. [MIDI and USB](#14-midi-and-usb)
15. [Web tools: emulator, installer, editor, backup](#15-web-tools-emulator-installer-editor-backup)
16. [Troubleshooting and FAQ](#16-troubleshooting-and-faq)
17. [GW-1 additions](#17-gw-1-additions)

---

## 0. Quick start

### Install

1. Open the **web installer** in Chrome, Edge or Opera: <https://fvalente.github.io/GammaWave-FM1/webapp/installer/>
2. Connect the FM-1 **directly** to the computer with a USB data cable.
3. **Coming from another custom firmware (Felucca, baudgirl or any other)?** Return to the official V15 firmware
   first, with the installer's **Return to official V15 first** button (it also saves a complete backup). Writing
   Pocket GammaWave straight over another custom firmware has left an FM-1 unable to start.
4. Press **Install**. It first checks the FM-1's firmware and writes only over official V15, JIANT or Pocket GammaWave
   (anything else: it sends you to Return to official V15 first). Don't unplug the cable while it writes.

Pocket GammaWave has no user samples: if you come from Felucca, its user samples (USR1–3) are lost on install. Keep your WAV
files.

### Your first sound (1 minute)

1. Power on. The GammaWave splash draws itself, then you land on **HOME**: the selected track's filter curve over a
   live spectrum, the Hearth sun, and the strip of tracks T1–T6.
2. Turn **ALGORITHM** to pick a track (T1–T6). With the default **TRACKS 4** it stops at T4: T5 and T6 appear only with TRACKS 6 (see
   [13](#13-system-tracks-load-and-the-menu)).
3. Turn **PRESETS** to browse sounds. Play the keys.
4. Turn **KNOB 1–4**: on HOME they are the four most useful controls of the track's engine.
5. Don't like it? Hold **EDIT** and turn **SELECT**: the sound is rolled into a new variation and you hear it at once.
   **OCT+ OK** keeps it, **OCT− CANCEL** brings the sound back.

### Your first song (5 minutes)

1. On **HOME**, turn **SELECT** (or hold **OCT−** and press **PLAY**). The **GENERATOR** screen opens. Turn **KNOB 1**
   to pick a genre (or leave it on RANDOM) and press **OCT+**: a whole piece appears — engines, sounds, parts, scale,
   tempo and effects. Press **PLAY** to hear it, **HOME** to leave the screen.
2. Not your thing? Press **OCT+** again: the same settings give a new seed, so every press is a new piece. To go back to
   the music before: hold **GLO** and press **G4** (UNDO).
3. Like it? **Stop** (PLAY), then press **SAVE** twice to reach **SAVE > PROJECT** (the first SAVE press opens
   USER, the second PROJECT). Turn **KNOB 4** to the right so it shows **[OCT+]**, then press **OCT+**. Type a name with the keys (or keep
   the one offered) and press **OCT+** again. The screen says **SAVED**.
4. Make a variation (change some steps, mute a track) and save it into section **B**: on SAVE > PROJECT turn
   **KNOB 2** to **B**, then KNOB 4 and OCT+ as before.
5. Press **SAVE** again to reach **ARRANGE**. Turn **KNOB 2** on the **+ ADD** row to add a row, set its section
   with KNOB 2 and its length with **KNOB 3** (bars). Add a row for A, one for B. Press **PLAY**: the song plays the
   rows in order.

That's the whole loop: **make music → save it into a section → chain sections in ARRANGE**.

---

## 1. The panel

The names below are the ones the firmware uses (the HARDWARE CALIBRATION screen asks for them by these names).

### Encoders and knobs

| Control | What it does |
| --- | --- |
| **SELECT** (encoder) | Alone: **scrolls through the pages of the section you are in** (EDIT's pages on an EDIT page, FX's on an FX page …), one a detent, no wrap; on **HOME** it opens the **GENERATOR** (as HOME + SELECT; with MENU > GEN > SELECT ON HOME TEMPO it turns the tempo instead). With a button held, that button's **dice**: **HOME** + SELECT the **GENERATOR**, **EDIT** + SELECT the sound, **SEQ** + SELECT the sequence, **LFO** + SELECT the LFO, **FX** + SELECT the effects. **GLO** + SELECT: the tempo. Any other page button + SELECT: its pages. |
| **ALGORITHM** (encoder) | Selects the track from any page: T1–T4, or T1–T6 with TRACKS 6. In the MENU: switches tabs. |
| **PRESETS** (encoder) | On HOME, PRESETS and the sound pages: browses sounds (every engine's factory presets, then your user presets). On step pages (STEP, COMP, PATTERN, AUTOMATION, SHIFT): moves the step cursor. On list pages (USER, PROJECT, ARRANGE, TOOLS, AUTO LIST): moves the selection. **Hold a page button and turn SELECT** (or PRESETS) to jump straight through that button's pages. |
| **KNOB 1–4** | The four cards (columns) on screen. With a layer held, the layer's own knobs. Turning fast sweeps the whole range, slowly fine-tunes (MENU > KNOB ACCEL turns this off). |
| **MASTER** | The output volume. |

### Buttons

| Button | Tap | Hold |
| --- | --- | --- |
| **HOME** | Back to HOME. | About 0.7 s: opens the **MENU**. |
| **ENV** | ENV pages. | (no layer) |
| **LFO** | LFO pages. | **MACRO** layer: KNOB 1–4 are M1–M4. |
| **FX** | FX pages (two quick taps: next FX section). | **Punch-in FX** layer. |
| **SCL** | SCL pages. | **SCL** layer: keys set the root. |
| **EDIT** | EDIT pages (on STEP: clears the step; on USER / PROJECT: rename). | **EDIT** layer: INIT, RECALL, DRUM-X sound mutes. |
| **GLO** | MIXER → GLOBAL → SYSTEM → MIXER. | **GLO** layer: mutes, solos, levels, tap tempo, UNDO. |
| **SAVE** | SAVE pages (first press: USER). | **SAVE** layer: play/store sections live. |
| **ARP** | ARP pages. | (no layer) |
| **SEQ** | SEQ pages. | **SEQ TOOLS** layer (on STEP, PATTERN, AUTOMATION and SHIFT). |
| **PLAY** | Start / stop. | — |
| **REC** | Arms / disarms recording on the selected track. | **REC** layer: CLEAR, CLICK. |
| **OCT− / OCT+** | Keyboard octave (−3 to +3; both together: back to 0). On action pages and in dialogs: **OCT−** = back / NO, **OCT+** = do it / YES. | — |

### The keys

27 keys from **F3 to G5** (16 white, 11 black). They play the selected track. With a layer held they become that
layer's shortcuts; on the DRUM grid they are steps and lanes; on the NAME screen they type.

### Useful combinations

| Combination | Result |
| --- | --- |
| **GLO** held + **PLAY** | Restart from the top without stopping (internal clock). |
| **GLO** held + **SELECT** | Tempo, from any page. |
| **GLO** held + **G4** | **UNDO** the last big change (again: redo). |
| **SEQ** held + **REC** | **CLEAR ALL SEQUENCES?** — all six tracks' steps and automation, and the punch-in lane. |
| **REC** held + **FX / EDIT / ENV / LFO / SCL / ARP / GLO** | Clears that part's automation on the selected track (GLO: the level and pan automation of all tracks; FX: also the punch-in lane). No dialog. |
| **SAVE** held + **OCT+** / **OCT−** | Store the music into the section playing / recall that section as stored. |
| **HOME / EDIT / SEQ / LFO / FX** held + **SELECT** | HOME: the GENERATOR. The others, their dice: the sound / the sequence / the LFO / the effects (see [8](#8-dice-and-undo)). |
| **OCT−** held alone, then **ARP / PLAY / ENV / HOME / SEQ / EDIT / FX / LFO / GLO** | The GW-1 shift deck: Hearth macros, the GENERATOR, the MUTATE page, the visualizer, the dice, UNDO (see [17](#17-gw-1-additions)). |
| **ENV / SCL / ARP / SAVE** held + **SELECT**, any page button + **PRESETS** | Jump through that button's pages. |
| A step held (SEQ > STEP) + **KNOB 1–4** | A parameter lock on that step. |
| **OCT− + OCT+** held at power-on | HARDWARE CALIBRATION. |
| **OCT− + OCT+** held 5 s while running | Update mode (UBOOT), with a countdown from 2 s; let go to cancel. |

---

## 2. The screen

- **Header (top)**: play state, the metronome, the **BPM** (lit while it changes), the octave (or the song row
  while a song plays), the **load meter** (five small bars next to the battery: how much of the audio time the sound
  takes now) and the USB icon. Messages appear here too ("SAVED", "T1 CLEARED"...), sometimes with a second line.
- **Cards**: the four columns under the header are KNOB 1–4. A value that is automated or locked is marked; a dim card
  means "nothing here now".
- **Page name**: changing page (or track) by hand shows the page's name over the footer for a moment, with the track and
  its engine at the right; on HOME the track's engine, "T1 HOME" small. The header, the cards and the graph stay in sight.
- **Granules**: a few dots drift behind the panel's graph, as on HOME (LOAD FULL with ANIM ON). They move only when the panel redraws anyway
  (a knob, the LFO, the envelope), so they cost no CPU.
- **Panel (middle)**: the page's graph — on the EDIT pages two live scopes over the Wraith ribbon (the DRUM kit's
  eight pads on a DRUM track), an envelope, the LFO running, the step grid, a list...
- **Footer**: the page name and its place in its family (e.g. **DLY 2/2**), or key hints. On the song pages
  (PROJECT, ARRANGE, TOOLS, GLOBAL, SYSTEM) it shows the project and its name instead of the track's sound.

**Colours** work like a thermal camera: cold (cyan, blue) is little, hot (orange, yellow, white) is a lot. Violet is
what's selected, orange what's active.

**HOME** has three bays. On top, the selected track's **filter response curve** drawn over a 28-bar spectrum display
that moves with the audio. Bottom left, the **Hearth sun** with its orbiting moon, growing and flaring with the
overall level. Bottom right, the track's four HOME knobs as small dials and the **T1–T6 strip**: each track's number
with a level bar; the selected track is highlighted, a muted one stays dark, a track asleep (TRACKS 4) is drawn as
muted. On the **MIXER**, asleep tracks are greyed and say OFF.

**Dialogs** ask before anything destructive: **OCT− NO**, **OCT+ YES**.

---

## 3. Pages and layers

### How navigation works

- **Tap** a page button: its pages. Tap again: the next page of that family. From another family you return to the
  page you were last on.
- **HOME** returns to HOME.
- **FX** is long, so it is split into sections: TRACK (FX, DIST, AUTO PAN), DELAY (DLY, DLY 2), REVERB (REVERB, REVERB 2),
  CHORUS, SLICER (SLICER, SL BUS, SL BUS 2, SL PITCH), MASTER. **Two quick taps** on FX jump to the next section.
- **Turn SELECT** to scroll through the pages of the section you are in (EDIT's on an EDIT page, FX's on an FX page),
  one a detent, stopping at its first and last; the page's name shows over the footer for a moment. On HOME it opens
  the GENERATOR.
- **Hold a page button and turn PRESETS** to go through its pages forwards or back (no wrap); let go and you stay
  there. ENV, SCL, ARP and SAVE do the same with SELECT; HOME + SELECT opens the GENERATOR, EDIT, SEQ, LFO and FX +
  SELECT are their dice, GLO + SELECT the tempo.
- **Hold** a page button (past the HOLD time, MENU > HOLD, 0.4 s by default) to open its **layer**: the keys and
  knobs change function while you hold it, and a map shows what each key does. Pressing a key or turning a knob
  opens it at once. Let go and you're back where you were.
- Some pages only appear when they apply: FM6's operator pages on an FM6 track, DRUM-X's pages on a DRUM track,
  RETRIG on LOFI BYTE/FLOAT, and with **LOAD ECO** the SLICER and CHORUS pages are hidden.

### Every page and its four knobs

"—" means an empty card. "Action" cards show **[OCT+]** when picked (turn the knob right); press OCT+ to do it.

**ENV** (per track)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| ENV | ATK | DEC | SUS | REL |
| ENV DEST | FILTER | PITCH | SHAPE | LOOP |

**LFO** (per track)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| LFO | RATE | WAVE | PHASE | FADE |
| LFO DEST | PITCH | FILTER | SHAPE | AMP |
| MOD | SLOT (1–4) | SRC | DEST | AMT |
| MSEQ | STEP (1–16) | LEVEL | LEN | SLEW |
| MSEQ 2 | DIV | — | — | — |

**FX** (TRACK pages are per track; the rest are global)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| FX | DIST | CHORUS (send) | DLY (send) | REV (send) |
| DIST | DIST | TYPE | TONE | — |
| AUTO PAN | DEPTH | RATE | SYNC | WAVE |
| SLICER | MODE | PAT | RATE | DEPTH |
| SL BUS | T1 | T2 | T3 | T4 |
| SL BUS 2 | T5 | T6 | FX | — |
| SL PITCH | STEP | PITCH | LEN | QUANT |
| DLY | TIME | FEEDBK | TONE | MIX |
| DLY 2 | WIDE | PITCH | SPRAY | MOD |
| DLY 3 | TYPE (ANALOG / DIGITAL) | PING (OFF / ON) | — | — |
| REVERB | TYPE | SIZE | DAMP (RESO: FOLLOW) | PRE (SHIMMER/CLOUD: AIR; RESO: PITCH) |
| REVERB 2 | MOD | RATE | FILT | WIDE |
| CHORUS | RATE | DEPTH | STEREO | — |
| MASTER | CLIP | PUNCH | DUCK | REL |

**SCL**

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| SCL | ROOT | SCALE | QUANT | TRANS |
| SCL 2 | ALL TR | — | — | — |

**EDIT** (per track; which pages appear depends on the engine)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | Shown on |
| --- | --- | --- | --- | --- | --- |
| EDIT 1 | E1 | E2 | E3 | E4 | every engine (see [4](#4-engines)) |
| EDIT 2 | E5 | E6 | E7 | E8 | every engine |
| RETRIG | RTRG | — | — | — | LOFI with WAVE on BYTE or FLOAT |
| MORPH | MORPH | — | — | — | ANALOG (SAW..SIN), PHASE, LOFI (up to WRAM) |
| OPERATOR | OP | RATIO / FIXED | FINE | LEVEL | FM6 |
| OP ENV | OP | STAGE | RATE | LEVEL | FM6 |
| OPERATOR 2 | OP | MODE | DTUN | VEL | FM6 |
| FILTER | TYPE | CUT | RES | DRIVE (ANALOG) / DIST (others) | every engine |
| KIT 3 | BP | FEEDBK | — | — | DRUM (the whole kit) |
| X-MOD | RATE | DEPTH | SHAPE | RAND | DRUM (the whole kit) |
| SOUND | SOUND | SIDE | PITCH | PMOD | DRUM (one sound) |
| SOUND 2 | SOUND | DECAY | NOISE | COLOR | DRUM (one sound) |
| SOUND 3 | SOUND | MODE | PM.TYPE | DRIVE | DRUM (one sound) |
| SOUND 4 | SOUND | LAYER | FEEDBK | SEND | DRUM (one sound) |
| LEVELS | KICK | SNARE | CLAP | HATCL | DRUM |
| LEVELS 2 | HATOP | TOM | RIM | BELL | DRUM |
| STEREO | IMAGE | — | — | — | DRUM (the whole kit) |
| VOICE | VOICE | GLIDE | MODE | PRIO | every engine |

**GLO** (MIXER is per track; GLOBAL and SYSTEM are global)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| MIXER | LEVEL | PAN | REV | MUTE (right ON, left OFF) |
| GLOBAL | BPM | SWING | CLOCK | TUNE |
| SYSTEM | TRACKS | LOAD | ROUTE | CPU (%) |

**SAVE**

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| PRESETS | No. (the sound) | ENG (the engine) | FAV (mark a favourite) | LIST (ALL / FAV) |
| USER | SLOT (U01–U32) | LOAD (action) | ERASE (action) | SAVE (action) |
| PROJECT | PROJ (1–8) | SECT (A–D) | LOAD (action) | SAVE (action) |
| ARRANGE | ROW | SECT (on END: STOP / x2–x8 / LOOP) | BARS | SCENE |
| TOOLS | any knob or PRESETS picks from the list: CLEAR SEQ, INIT SOUND, DELETE ROW, CLEAR SONG, NEW PROJECT, CPU TEST, CLEAR PROJ | | | |

**ARP** (per track)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| ARP | MODE | RATE | OCT | GATE |
| ARP 2 | SWING | PROB | HOLD | ORDER |

**SEQ** (per track)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| STEP (melodic: piano roll) | STEP | NOTE | TIME (NOTE / TIE / REST) | FLAG (– / ACC / SLD / A+S) |
| STEP (DRUM: grid) | STEP | LANE | HIT | ACC |
| NOTE | FOCUS | PITCH | MOVE | LEN |
| TIMING | STEP | OFFSET (−8/16 … +7/16) | REC Q | — |
| STRETCH | STRETCH (/8 … /2, x2 … x8) | REPEAT (x2 … x8, TO 64) | NUDGE (now) | LEN (before > after) |
| COMP | STEP | TYPE | (its value) | (its value; RATCH / ECHO: DIV) |
| COMP 2 | STEP | TYPE (the second component) | (its value) | (its value; ECHO: DIV) |
| WHEN | STEP | A (WHEN) | B (WHEN) | LEN |
| PATTERN | LEN | DIV | SWING | GATE |
| SHIFT | OFFSET | PITCH | CHANCE | C.LEN |
| HUMAN | VEL | TIME | GATE | — |
| PLAYHEADS | HEAD | RATE | DIR | INTERVAL |
| AUTOMATION | PLAY (ON/OFF) | EVENT (count) | LOCK (count) | CLEAR (action) |
| AUTO LIST | ROW | STEP | PARAM | VALUE |
| MUTATE | SOUND (every) | PATTERN (every) | AMOUNT | MODE |

### The layers (hold the button)

**FX — punch-in effects** (details in [11](#11-performance))

| Keys | Effect |
| --- | --- |
| F3 G3 A3 | REPEAT 1/8, 1/16, 1/32 |
| B3 / C4 | LPF / HPF |
| D4 / E4 | OCT− / OCT+ |
| F4 | 1/2 TEMPO |
| G4 / A4 | DEC− / DEC+ |
| B4 / C5 | STUTTER 1/8 / 1/16 |
| D5 | ATK+ |
| E5 | ARP |
| F5 | RANDOM |
| G5 | erases the punch-in lane |
| Black keys 1–6 (F#3 G#3 A#3 C#4 D#4 F#4) | mute T1–T6 while held |
| A#4 | which tracks the note effects touch: ALL / SYN / DRM |
| G#4 / F#5 | VIBRATO (±½ semitone, ~5 Hz) / DIVE (two octaves down over a beat): on the notes, recorded in the lane |
| C#5 / D#5 | GATE (each 1/16 shut for its second half; DEPTH: how much) / SWEEP (the low-pass down and back up over a bar) |
| **Knobs** | FILTER, CRUSH, THROW, DEPTH |

**GLO — mix**

| Keys | Action |
| --- | --- |
| Black keys 1–6 | MUTE T1–T6 (latched: press again to unmute) |
| Black keys 7–10 (G#4 A#4 C#5 D#5) | DRUM-X group mutes: KICK, SNARE, HAT, PERC |
| F3–D4 | SOLO T1–T6 while held |
| E4 | unmute everything (tracks and drum groups) |
| F4 | TAP tempo (from the third tap; internal clock only) |
| G4 | **UNDO** (again: redo) |
| PLAY | restart from the top |
| SELECT | tempo |
| **Knobs** | LEVEL T1–T4 (with T5 or T6 selected: T3–T6) |

**SCL** — any key sets the ROOT to its note name. Knobs: ROOT, SCALE, OFFSET, PITCH (the last two are SEQ > SHIFT's).

**EDIT** — F3 **INIT** (the track's sound back to its engine's own defaults (a plain INIT, not a preset), dry: every effect send at 0; FM6 the init
voice, OP1 alone at full level; on DRUM the factory DRUM-X kit too), G3 **RECALL** (the sound as the current section stored it; on DRUM with its kit). Both ask first and leave the
steps alone. On a DRUM track, black keys 1–8 mute each DRUM-X sound (BD SD CP CH OH TM RS CB). Knobs: EDIT 1's four.

**SEQ TOOLS** (only on STEP, PATTERN, AUTOMATION and SHIFT) — F3 CLEAR, G3 REVERSE, A3 / B3 SHIFT left / right,
C4 RANDOM, D4 COOK, E4 BEAT (DRUM), F4 CLR AUTO (the track's automation and locks; the steps stay). On a DRUM track the
lane tools: G4 CLR, A4 REV, B4 FILL, C5 RND on the lane picked with black keys 1–8. **REC** (while SEQ is held): CLEAR
ALL. Knobs: LEN, DIV, SWING, GATE. These tools have no undo.

**REC** — F3 **CLEAR** the selected track's steps and automation (no dialog; UNDO can bring it back), G3 **CLICK**
(OFF / REC / ON). KNOB 1: CLICK.

**SAVE — song layer** (details in [7](#7-arrange-songs-sections-and-scenes))

| Keys | Action |
| --- | --- |
| F3 G3 A3 B3 | play section A / B / C / D (next bar while playing; at once when stopped). Several in one hold: a quick chain. |
| C4 D4 E4 F4 | STORE the music into A / B / C / D |
| D5 | LOOP / SONG: what PLAY plays |
| E5 | SONG REC |
| G5 | go to the ARRANGE page |
| OCT+ / OCT− | store into / recall the section playing |
| **KNOB 1** | the project (1–8); applied when you let go of SAVE, stopped only |

**LFO — macros**: KNOB 1–4 are M1–M4. Let go and they stay **latched** (the header says [LFO] LATCH, and KNOB 1–4
keep controlling the macros on every page) until you press LFO again. F3 rolls new routes, G3 clears them.
(LFO + SELECT rolls the track's LFO, with KEEP / CANCEL: see the dice.)

---

## 4. Engines

Eight engines, all synthesis, no samples. Any engine can go on any track. Pick them with **PRESETS** (one list over
every engine) or with KNOB 2 (ENG) on SAVE > PRESETS. Each track's four HOME knobs are listed too.

**Smooth changes**: switching a track to another engine fades the old one out (about 12 ms) and what you are holding
(keys, or the notes the sequencer holds) goes on in the new engine, fading in; nothing is cut. Changing a list value
(WAVE, CHIP, a type) while notes sound makes a short dip (about 3 ms) instead of a click; the notes go on.

| Engine | EDIT 1 | EDIT 2 | HOME knobs |
| --- | --- | --- | --- |
| **ANALOG** | WAVE, DETUNE, MIX, NOISE | CUT, RES, DRIVE, INT | CUT, RES, ATK, REL |
| **FM6** | ALG, FB, MOD, RATIO | MEG, VMOD, DETUNE, SLOT | MOD, RATIO, MEG, SLOT |
| **PHASE** | WAVE, WAVE2, DCW, ENV | DETUNE, LINE, SUB, FB | DCW, ENV, DETUNE, REL |
| **LOFI** | CHIP, WAVE, DUTY, CRUSH | —, VIB, BEND, MASK | WAVE, DUTY, CRUSH, REL |
| **VOICE** | VOWEL, VOWEL2, TALK, SHIFT | BUZZ, BREATH, Q, RAND | VOWEL, BUZZ, BREATH, TALK |
| **WHEEL** | REG, SUB, BODY, TOP | PERC, CLICK, DRIVE, ROTOR | REG, PERC, DRIVE, ROTOR |
| **NOISE** | MODE, COLOR, FREQ, PEAK | TRK, DENSE, DRIFT, CRUSH | FREQ, PEAK, COLOR, REL |
| **DRUM** (DRUM-X) | MORPH, TUNE, TONE, DECAY | NOISE, FM ENV, FOLD, DRIVE | MORPH, TUNE, TONE, DECAY |

- **ANALOG**: two oscillators. WAVE: SAW, SQR, TRI, SIN, PWM, SYNC, RING, SAW3, TUBE, FOLD. **INT** sets oscillator
  2's interval (±24 semitones). Its filter is per voice. **TUBE** and **FOLD** are sines made dirty inside the
  oscillator, before the filter, so the fundamental stays (a distorted 808 keeps its sub): TUBE through GammaWave's
  Hearth tube curve (warm, even harmonics), FOLD through WRAITH's Blast wavefolder (a growl, metal). How dirty is
  **MORPH** plus ENV / LFO → **SHAPE**: with ENV → SHAPE up, a note is dirtiest on its hit and cleans up as it rings.
- **FM6**: a six-operator FM engine (Dexed's msfa), 32 algorithms drawn on screen, four voices per track. The EDIT 1/2
  knobs are macros over the patch. **SLOT**: the factory patches F1–F8, OWN (an edited or imported patch) and B1–B32
  (a 32-voice DX7 bank sent from the web editor; empty slots are skipped; until you send one, B1–B11 hold
  **GW-1 KEYS**, built in: see below). **OPERATOR / OP ENV / OPERATOR 2** edit each
  of the six operators on the device (KNOB 1 picks the operator); editing makes the patch the track's own (SLOT OWN),
  saved with the project. A bank voice you pick is copied into the track, so projects keep it even if you send
  another bank later.
- **PHASE**: phase distortion. WAVE / WAVE2: SAW, SQR, PLS, DSIN, SPLS, RSAW, RTRI, RTRP. LINE: MIX or RING.
  **FB** feeds the output back into the phase.
- **LOFI**: chip sounds. CHIP: 4BIT, 4B/2, 8BIT, 1BIT, STEP. WAVE: PLS, TRI, SAW, NOIS, WRAM, **BYTE**, **FLOAT**.
  - **BYTE** (bytebeat): KNOB 3 becomes **ALGO** (32 formulas), KNOB 4 **VAR** (mutates the formula); EDIT 2 becomes
    **MOTION, GRIT, BEND, LOOP**. MOTION speeds the formula's rhythm up or down (64 = as written), GRIT is a rate
    crusher, BEND folds time, LOOP repeats a tiny stretch (high LOOP turns the noise into a tuned note).
  - **FLOAT** (floatbeat): CHIP becomes the resolution (4BIT, 4B/2, FULL, 1BIT), KNOB 3 **ALGO** (32 formulas made of
    sines), KNOB 4 **VAR**; EDIT 2: **MOTION, FEEDBK, BEND, LOOP**. Hold a key: the formula plays in that key.
  - **RETRIG** page (BYTE / FLOAT): **RTRG ON** restarts the formula from the top on every note.
- **MORPH** (EDIT > MORPH, on ANALOG, PHASE and LOFI): the wave slides continuously toward the next ones (on ANALOG
  SAW > SQR > TRI > SIN, on PHASE toward its next shape, on LOFI toward the next wave up to WRAM). 0 = the wave as
  chosen. On ANALOG and LOFI the **SHAPE** modulation moves the morph too, so an LFO or envelope sweeps the wave. The
  page draws one cycle of the morphed wave and says which two waves it blends.
- **VOICE**: a formant voice (after klattsch): vowels, talk, breath.
- **WHEEL**: a drawbar organ: registrations (REG), percussion (PERC: OFF, 2ND, 3RD, 2SOFT, 3SOFT, 2SLOW, 3SLOW),
  key click, drive, ROTOR (OFF, SLOW, FAST).
- **NOISE**: coloured and metallic noise. MODE: ANLG, DUST, LFSR, META (DUST is only the crackle: at DENS 0 it still crackles softly). PEAK is its own resonance.
- **DRUM**: DRUM-X, see [5](#5-drum-x).

### Ambient & Keys presets (1.2-GW)

Twenty-one presets for pads, drones, strings and keys (issue #17), on the engines above. Find them with **PRESETS**
(each engine's come at the end of its list). The pads move by themselves: a slow LFO, sometimes a random modulation,
on their filter, brightness or pitch, set by the preset. Turn ENV > ATK and REL to make a pad slower or tighter.

| Engine | Presets |
| --- | --- |
| **ANALOG** | **EVOLVE PAD** (three saws, a slow LFO on the filter, each note its own mix: it drifts), **SOLINA** (a string machine, the ensemble chorus), **DEEP DRONE** (a fifth that breathes) |
| **PHASE** | **CRYSTAL PAD** (its brightness swept slowly), **HAZE STRINGS** |
| **VOICE** | **SOFT CHOIR** (an "oo" that opens a little), **ANGEL AIR** (breath in the air) |
| **WHEEL** | **DRONE ORGAN** (flutes, the slow rotor) |
| **NOISE** | **OCEAN** (a slow swell of filtered noise), **DUST HAZE** |
| **FM6** | **SOFT EP**, **REED PIANO**, **FM PIANO**, **GLASS KEYS**, **CRYSTAL BELL**, **KALIMBA**, **VIBES**, **MUSIC BOX**, **WARM PAD**, **GLASS PAD**, **CELESTA** |

- **GW-1 KEYS**: the FM6 presets play the voices of a bank built into the firmware, on **B1–B11** (SLOT). It is there
  until you send a bank of your own from the web editor; your bank replaces it (send it again to get it back:
  `web/banks/gw1-keys.syx` is the same bank as a file). Velocity opens their brightness: play softly for a darker sound.
- FM6 runs four voices per track, so a held chord of four notes is its limit; for long pads with many notes, ANALOG,
  PHASE or VOICE are lighter.
- The **GENERATOR does not pick these presets** (a share code makes the same piece as before 1.2). Make a piece, then
  load them on its tracks with PRESETS: the sequence stays.

### FILTER (every engine)

EDIT > **FILTER**: **TYPE** LP, HP, BP, **COMB** (a comb tuned to the note you play; CUT moves it, RES is how much
it rings), **VOWEL** (a vocal colour: low-pass and band-pass blended with a saturated notch) or **SCREAM** (a
resonant filter with a saturated core, for screaming leads and basses), **CUT**, **RES**, and KNOB 4: on ANALOG its own DRIVE, on every other engine the track's DIST. On ANALOG
the filter is per voice; on LOFI the filter page drives its per-voice filter; on the others it is a filter on the
track that follows LFO DEST / ENV DEST FILTER.

### VOICE (every engine)

EDIT > **VOICE**: **VOICE** POLY, MONO, LEG (legato), UNI (unison); **GLIDE**; **MODE** RATE or TIME (how glide is
measured); **PRIO** LAST, LOW, HIGH. The six tracks share a budget of 8 voices; FM6 plays at most four per track.

---

## 5. DRUM-X

DRUM-X is a synthesized kit of eight sounds, **BD SD CP CH OH TM RS CB**, generated in real time.

- Every sound has two sides, **A** and **B**. **MORPH** (KIT, KNOB 1, also on HOME) moves the whole kit between them:
  the knob to play live.
- The pages go **the whole kit first, then one sound, then the volumes and the stereo** (0.6.58):
  - **KIT** (EDIT 1): MORPH, TUNE, TONE, DECAY. **KIT 2** (EDIT 2): NOISE, **FM ENV** (FM struck with each hit and gone
    in a few milliseconds: a beater, a click, a metal edge on the attack, the tail clean), **FOLD** (a wavefolder after
    the envelope: a bright attack, a clean tail), DRIVE.
  - **KIT 3**: **BP**, a band-pass on the metal sounds (the hats, the rim, the cowbell), OFF or 15 centres from about
    1.5 to 9 kHz; **FEEDBK**, every sound's oscillator fed back into itself (OFF, 1–15: warm, growl, grit, noise).
  - **X-MOD**: **RATE** (OFF, 4BAR … 1/32, in time), **DEPTH** (how far MORPH swings), **SHAPE** (SINE, TRI, SAW, RAMP,
    S&H), **RAND** (each hit gets its own MORPH and a little tune).
  - **FILTER**: the kit's.
  - **SOUND 1–4**, one sound each: KNOB 1 is always **SOUND** (which one; striking a key also picks it).
    - **SOUND**: SIDE (A / B), PITCH, PMOD (how far the pitch falls).
    - **SOUND 2**: DECAY, NOISE, COLOR, of the side picked on SOUND (KNOB 1 shows it: "SD B").
    - **SOUND 3**: **MODE** (wave SIN / FM / MTL / BEL, noise filter LP / BP / HP, "+" for snap), **PM.TYPE** (how the
      pitch moves: DECAY, LONG, NOISE, SINE), **DRIVE** (ON / OFF).
      - MODE's last four steps are not CLASSIC sounds but **SNARE**, **SNARE+**, **CLAP** and **CLAP+**: a produced
        trap snare and clap, built at full rate and fitted to producer one-shots.
      - Each is a tuned body, a stick click and two bands of rattle, held at full level and then pushed into a
        clipper. On them, the sound's values mean:
        - **PITCH**: the body's note.
        - **PMOD**: how far above it the body starts (0–12 semitones, gone in ~4 ms).
        - **DECAY**: how long it holds (0–63 ms).
        - **NOISE**: body against rattle.
        - **COLOR**: the rattle's band (600 Hz – 4.8 kHz).
      - "+" (SNAP) cuts the tail short, gated. CLAP strikes its rattle three times; on CLAP, **PMOD** sets how far
        apart (from ~4 ms, tight, to ~12 ms, wide) instead of the body's start.
      - KNOB 3 becomes **CLIP** (OFF, 1–15: how hard it hits the clipper). PM.TYPE, LAYER and FEEDBK don't apply (shown as "-").
      - The trap genres use them for their snare and clap.
    - **SOUND 4**: **LAYER** (OFF, 1–15): a second oscillator at an out-of-tune interval, frequency modulated by the
      sound, more as it goes up: from a metal edge to a clang with no note, so the sound melts into the rhythm and stays
      out of the harmony (the factory kit has it on the snare, the hats and the cowbell); **FEEDBK** (OFF, 1–15): that
      sound's own (added to KIT 3's); **SEND**: that hit alone into one effect, OFF then CHO, DLY, REV with 16 amounts
      each, on one knob ("REV 75%"), on top of the track's own sends. The dice (EDIT + SELECT on a DRUM track, the GENERATOR) set
      LAYER and FEEDBK boldly. Each sound with a LAYER costs a little CPU (about as FEEDBK).
  - **LEVELS / LEVELS 2**: the level of each sound (KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL).
  - **STEREO**: **IMAGE** (automatable) sweeps the kit through its placings in the stereo field. 0 (MONO): every sound
    in the middle; the first turns open it; from there each step is another placing (each sound moves on its own
    curve), so turn it until the image sits right. The kick stays near the middle. DRV, DIST and the FILTER work on the
    kit's sum, so with them the image narrows (the page draws the sounds grey).
  - KIT 3, X-MOD, LAYER, FEEDBK and SEND are saved with the section's kit.
- **Mutes**: GLO held + black keys 7–10 mute the groups KICK, SNARE, HAT, PERC; EDIT held + black keys 1–8 mute each
  sound.
- **EDIT held + SELECT** rolls a new kit around the factory one. **EDIT held + F3 (INIT)** brings the factory kit
  back.

The kit is saved with each section of a project (not with user presets — see [12](#12-saving-and-loading)).

---

## 6. The sequencer

Each track has its own **64-step** pattern with its own length (LEN), division (DIV), swing and gate, so patterns
of different lengths turn against each other.

### Writing steps

- **SEQ > STEP** on a melodic track is a **piano roll**. KNOB 1 moves the step cursor, KNOB 2 transposes the step,
  KNOB 3 sets NOTE / TIE / REST, KNOB 4 sets accent / slide. A step holds up to four notes.
- **Step recording**: on STEP, while **stopped**, press **REC** (the screen says KEYS WRITE STEPS). Play a note or a
  chord: it goes into the cursor step, and when you let go the cursor moves on. Not armed, the keys just play.
  - **Length**: while the keys are held, turn **KNOB 1**: the note or chord lasts that many steps (1–16, ties after it).
    It is written when you let go, and the cursor jumps past it.
  - **Arpeggio**: while the keys are held, press **ARP**: the chord is written as an arpeggio into the roll. KNOB 1 is
    now its resolution (1/16, 1/8, 1/8., 1/4: a note every 1–4 steps), KNOB 2 the order (UP, DOWN, UP-DN, RND), KNOB 3
    the octaves (1–3). The screen shows the settings; letting go writes it, the cursor after it. KNOB 4 (and 2–3
    without ARP) still makes parameter locks on the step.
- **EDIT** (tap) on STEP clears the cursor step.
- On a **DRUM** track, STEP is a **grid**: white keys are the 16 steps of the page, black keys 1–8 pick the lane
  (and play it), black key 9 held is ACC (white keys then toggle accents), black keys 10 / 11 page down / up when
  LEN is over 16. Knobs: STEP, LANE, HIT, ACC.

### Live recording

Press **REC** anywhere (not on STEP while stopped): the track is armed and the transport starts. What you play is
recorded into the pattern, quantized to its steps, overdubbing. Knob moves on the selected track are recorded as
**automation**: from then on that value moves by itself as the pattern plays. The first time a knob gets automation the
header says so, with the REC chord that clears it (`AUTOMATED. REC+ENV CLR`: hold REC and press ENV), and its card
shows an **A** (in the card's knob on the GAMMA look, a chip after the label on the others). SEQ held + F4 (CLR AUTO)
clears all of the track's automation; SEQ > AUTOMATION's PLAY OFF keeps it but stops it playing. MENU > AUDIO has the **CLICK** (OFF / REC / ON), **CLICK LEVEL** and **COUNT-IN** (OFF, 1 BAR, 2 BARS).

### Pattern pages

- **COMP** (step components, OP-Z style): up to two per step (**COMP** the first, **COMP 2** the second, the same
  way; 0.6.82). KNOB 1 picks the step, KNOB 2 its **TYPE**, KNOB 3 / 4 its values. The row on top marks every step that has one (orange ARP, violet TRIG, gray CHANCE, theme RATCH); below,
  the step's component drawn (the arpeggio's ladder, the note playing lit; TRIG's loops).
  - **CHANCE** (0–100 %) and **RATCH** (x1–x4): the step's own chance and ratchet (on COMP only). RATCH's KNOB 4 is
    its **DIV**: STEP shares the hits out over the step (as before); 1/2, 1/3, 1/4, 1/6, 1/8 put them that part of a
    step apart from its start, as many as fit (RATCH x3 at 1/8: three quick hits, then silence).
  - **ARP NOTES / SEQ / SCALE**: the step plays an arpeggio, **RATE** notes in its length (x2 x3 x4 x6 x8), and goes
    on over the TIEs after it (its division is RATE, its duration the step and its TIEs). NOTES: the step's notes,
    low to high; SEQ: the pattern's melody, in the order the pattern plays it (0.6.82; it was sorted like NOTES);
    both over 1–4 octaves. SCALE: the scale up from the step's lowest note, 3, 5, 8 or 15 degrees. KNOB 3 the **ORDER** (UP, DOWN,
    UP-DN, RND), KNOB 4 **RATE** and range together.
  - **TRIG**: the step plays only on some loops of the pattern: 1:2, 2:2, 1:3, 2:3, 3:3, 1:4 … 4:4, 1:8, FIRST, !1ST
    (every loop but the first).
  - **RAND**: the note moves up or down the scale at random, up to RANGE degrees (1–7).
  - **OCT**: an octave: +1, −1, +1 every other loop, −1 every other loop, ALT (up and down by turns), RND.
  - **TONAL**: from this step to the end of the loop every note moves SHIFT degrees (−7 to +7) along the scale.
  - **BEND**: the note glides BEND semitones (−12 to +12) over the step.
  - **ECHO**: the step repeats x2–x8 in its length, each repeat softer. KNOB 4 its **DIV**, as RATCH's: STEP shares
    the repeats out over the step, 1/2 … 1/8 sets how far apart they are.
  - **PULSE**: the note plays again on the empty steps after it, EVERY 1–4 steps, until the next note.
  - **JUMP**: after this step the pattern goes on from TO STEP.
  - **REV**: from this step the pattern plays backwards to step 1, then forwards again (ping-pong).
  - **TIMES** (KNOB 4, JUMP and REV): **ALWAYS** turns the pattern every time; **x1** once a pass, then the pattern plays
    on past it to its end (a section played twice, then the rest); **x2** twice a pass; **1:2** once, every other pass.
    A pass ends where the pattern wraps by itself.
  - **FILL**: the step plays only while **FX** is held.
  - **KILL**: the step first cuts everything the track is sounding (long releases, held notes), then plays.
  - **Two on one step**: both play (an ECHO with an OCT: the repeats an octave up; a RAND with an OCT: both moves).
    One that keeps the note silent wins: a **TRIG** or **FILL** in either place decides whether the step plays at all,
    before anything else. Of two that do the same job, the first wins (an ARP over an ECHO, two ARPs: COMP's).
  - **WHEN** (its own page after COMP 2): KNOB 2 / 3 the loops the first (A) and second (B) component play on:
    **ALWAYS**, **1:2** (the 1st, 3rd, … loop), **2:2** (the 2nd, 4th, …), **SKIP** (kept, but not played: an
    on/off switch). KNOB 4 the step's **LEN**: GATE (the track's) or 1/16 … 15/16 of the note (or of each RATCH /
    ECHO / ARP part) sounding.
  Projects keep them. **On a drum kit** (0.6.83) they work too: OCT, RAND, TONAL tune the hits (RAND and TONAL in
  the track's scale), BEND bends each hit over the step; ARP NOTES plays the step's sounds one after another, SEQ
  the pattern's sounds in the order they play, SCALE retriggers the step's hits each part a degree further up.
- **A long chord slide** (0.6.83): the glide into a chord lasts as long as that chord, its TIEs included; GLIDE sets
  how much of it (127: all of it). A chord held over TIEs on slow steps (e.g. DIV 1/4 at 120 BPM: ten steps, 5 s)
  slides that slowly and smoothly; new notes fade in as they glide.
- **PATTERN**: LEN (1–64), DIV, SWING, GATE.
- **Moving a note or a chord** that came in early or late: **NOTE**, KNOB 1 FOCUS on the step, KNOB 3 **MOVE** moves
  it sideways a step at a time (FOCUS ALL: the whole chord with its TIEs; one note focused: that note alone).
- **STRETCH** (0.6.85, after TIMING): reshapes the whole sequence of the selected track. Turn a knob to pick, then
  **OCT+** does it (SAVE held: UNDO brings it back). KNOB 4 shows the LEN before and after.
  - **KNOB 1 STRETCH**: **x2 … x8** spreads the steps that many times apart (LEN 4 x4: 16), a held note held all the
    way, short ones kept short; past 64 steps the end is cut. **/2 … /8** squeezes it: of each group of steps the first
    one that plays stays (LEN 32 /2: 16). The automation and locks move with their steps.
  - **KNOB 2 REPEAT**: copies steps 1..LEN after themselves, **x2 … x8** times or **TO 64** (as many whole copies as
    fit), automation and locks too. Four notes become a 64-step bed to record more notes over (live REC overdubs).
  - **KNOB 3 NUDGE**: the whole sequence one step right / left at once, round inside LEN (as TOOLS A3 / B3).
- **TIMING** (0.6.88, right after NOTE): each step's own place off the grid. KNOB 1 the step, KNOB 2 its **OFFSET** in
  sixteenths of a step, from −8/16 (half a step early) to +7/16 (late); **EDIT** puts it back on the grid. KNOB 3
  **REC Q**, the track's recording quantize: 100 % (the default) live recording snaps to the grid; lower, the step keeps
  that much of where you played it (0 %: all of it), early or late. Early steps really sound early: the step before
  ends that much sooner and the grid is kept. The piano roll draws the notes where they play. A chord shares its
  step's offset; moving, stretching or repeating steps keeps it; saved with the project.
- **HUMAN** (0.6.87, after SHIFT): the track plays like a person, each note a little different every time round;
  the steps stay as written. **VEL**: each note's velocity up or down at random (up to ±38 at 100 %). **TIME**: each
  step starts a little late at random (up to 30 ms, never more than a third of the step, never early; a chord's notes
  stay together, a STRUM keeps its spacing). **GATE**: each note's length longer or shorter (up to ±40 %). 0 % plays
  exactly as written (the default, and every project before). Kept when you change the sound; automatable.
- **AUTOMATION**: PLAY turns the track's automation on or off, EVENT and LOCK count the records, CLEAR erases them
  (OCT+, with a dialog).
- **AUTO LIST**: every automation event and lock as a list. KNOB 1 the row, 2 the step, 3 the parameter, 4 the value.
- **MUTATE**: the track changes by itself, in time with the clock, so a texture can evolve for minutes.
  **SOUND** and **PATTERN**: every OFF, 1, 2, 4, 8, 16 or 32 bars (the change lands on the bar's one). **AMOUNT**: how
  much (turning one on sets 50 %). **MODE**: **ORBIT** varies around the anchor (the music as it was when MUTATE came
  on, when PLAY started, when you pressed **OCT+** here: KEEP, or when you last wrote or cleared steps), so it never
  loses itself, and a sequence you clear stays cleared; an empty pattern is never filled by MUTATE; **DRIFT** goes on from
  what it is now. SOUND changes the engine's values, the filter, its envelope, DEC, REL and the sends (never LEVEL, PAN
  or MUTE); PATTERN moves notes along the scale, flips accents, adds or takes a note off the beat (never on it); on
  DRUM it flips hits, never the kick. **OCT−** here (or **GLO + A4**, FREEZE) stops every mutation until pressed again.
  The page shows each track's next 32 bars (orange: SOUND, violet: PATTERN). **OCT− held + ENV** jumps here from
  HOME or a parameter page (see [17](#17-gw-1-additions)).
- **PLAYHEADS** (SEQ, page 4; it was FUGUE), after the Fugue Machine: three more **playheads** (heads 2, 3, 4) read the same steps
  as the track's sequencer. KNOB 1 **HEAD** picks one, KNOB 2 **RATE** (OFF, 1/4, 1/3, 1/2, 2/3, x1, 3/2, x2, x3, x4 of
  the track's), KNOB 3 **DIR** (FWD, REV, PING, RND), KNOB 4 **INTVL**, its interval in degrees of the scale (−14 to +14;
  a drum track keeps its lanes). A head plays each step's notes with the step's velocity, accent and chance and the
  track's GATE (no components, locks or ratchets: those are the sequencer's): canons, counterpoint, patterns phasing
  against themselves. The page draws the steps, the sequencer's playhead and each head where it is. Saved with the
  section; the heads start and stop with the transport.
- **SHIFT** (the last SEQ page): **OFFSET** (−32 to +32 steps), **PITCH** (±24 semitones; not on drums) and
  **CHANCE**: the whole track's chance, 100 % plays every step as written, lower thins it out (each step's own chance,
  from the dice or the editor, scaled). All three can be automated; OFFSET and PITCH are also on the SCL layer's
  KNOB 3 / 4. **C.LEN** (KNOB 4): the step components as a lane of their own length. OFF: each step plays its own
  component. 1–64: the components of steps 1..C.LEN are read in turn, one per step the track plays, so with C.LEN 3 over
  a 4-step pattern each component lands one step earlier every loop (polymeter: the components drift over the notes).
  The COMP page marks the lane and where it is; components past LEN still play when C.LEN reaches them.
  On **+ ADD LOCK**, OCT+ adds a lock; on a record, OCT+ switches it between LOCK and AUTO; **EDIT** deletes it.

### Parameter locks

On **SEQ > STEP**, **hold a step** (a grid key, or the note being entered on the roll) and turn a knob: that step gets
its own value of the parameter that knob edits on the sound page you visited last. Tap EDIT while holding the step to
clear its locks. Automation events and locks share **64 records per section** (all tracks together); when they run out
the screen says **AUTOMATION FULL**.

### Scale

**ROOT, SCALE and QUANT are global**: one scale for the whole song. Whatever track you set them on (knob, SCL layer,
MIDI, a project load), every melodic track follows it at once; drum tracks have none. Each track keeps its own TRANS.

- SCALE: CHR, MAJ, MIN, DOR, MIX, PEN, MPEN, HARM, PHRY, LYD, LOC, MEL, BLUES, WHOLE, DIMHW, DIMWH.
- QUANT: OFF; SNAP (keys snap to the scale); WHITE (the white keys walk the scale, black keys are silent); SEQ (as SNAP,
  and the sequencer's notes snap too as they play).
- **SCL 2 > ALL TR** (it was called STRN) moves every sequence up or down the scale (±14 degrees), on every track at
  once. It is global and saved with the project: if your notes sound lower or higher than the piano roll shows, look
  here. When it is not 0 the piano roll says **ALL TR** and its value, and PLAY says it too. 0 = off.
- The RESO reverb and the SLICER's pitch steps follow the scale too.

### CHORD (0.6.81)

**SCL > CHORD** makes a key play a chord on the track (after the Omnichord and the Orchid):

- **CHORD**: OFF; in the scale (every chord in key, nothing to think): **TRIAD**, **7TH**, **ADD9**, **SUS2**,
  **SUS4**, built on the key's degree (a key outside the scale takes the scale note below; CHR: the major scale of
  ROOT); shapes on any key: **MAJ**, **MIN**, **7**, **MAJ7**, **MIN7**, **POWER**.
- **VOICING**: CLOSE (root position); **LEAD** (each chord takes the inversion nearest the last one: voice leading, the
  Orchid's smoothness); OPEN (the second tone an octave up); +BASS (the root an octave down added).
- **STRUM**: 0 all at once; up from there, the notes come in one after the other, low to high (up to ~63 ms apart), as
  a harp.

Letting the key go ends every note of its chord (those still to strum never come). The chord records into the step,
live and in step recording alike (0.6.83: step recording took the key's note alone), and the ARP arpeggiates it. The
keys of every note sounding light up, the chord's whole shape too: a way to see and learn the chords. With **STRUM**
(0.6.84) a recorded chord goes whole into the one step where the key went down (its later notes no longer spill into
the next step), and the sequencer strums every chord step back the same way, low to high (within half the step's
gate). STRUM at 0: the steps' chords as before, all at once. A drum track, MONO, LEGATO and UNISON play the root alone. MIDI notes play as they come.

### ARP

ARP: MODE, RATE, OCT (1–4), GATE. ARP 2: SWING, PROB, HOLD, ORDER (NOTE / PLAY). Modes: OFF, UP, DN, UPDN, RND, ORD,
REPEAT, DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK, CHORD, **TRNS** (the keys and incoming MIDI transpose the track's
sequence by their distance from C4, without playing notes). **Key latch**: with MODE OFF, **HOLD ON** keeps the notes
sounding after you let go; the next chord replaces them.

### Clearing

| To clear | Do |
| --- | --- |
| The selected track's steps and automation | **REC held + F3**, or SAVE > TOOLS > CLEAR SEQ, or SEQ TOOLS F3 |
| All six tracks' steps, automation and the punch-in lane | **SEQ held + REC**, then OCT+ |
| **Everything** (NEW PROJECT): every track its engine's default sound (dry, FM6 the init voice), no steps or automation, the mix, effects, tempo and SLICER at their defaults, the song empty, macros at 0, the factory DRUM-X kit. Saved projects, user presets, MENU, CLOCK, TUNE and MIDI stay | **MENU > SYSTEM > INIT SYSTEM** or **SAVE > TOOLS > 5 NEW PROJECT**, then OCT+ (UNDO brings the music back) |
| **The project's stored sections A–D and its song** (CLEAR PROJECT, 1.2.1-GW): the sections free again, so LOAD shows them empty and the GENERATOR's SONG form can fill them; the music playing stays. No UNDO | **SAVE > TOOLS > 7 CLEAR PROJ**, then OCT+ |
| Only one part's automation on the selected track | **REC held + FX / EDIT / ENV / LFO / SCL / ARP / GLO** |
| *(not a clear)* A heavy test song, to measure the CPU on the device and listen for clicks (every live track chords of four with two step components a step and long glides, the kit on every lane, every FX on, 160 BPM) | **SAVE > TOOLS > 6 CPU TEST**, OCT+, then PLAY (UNDO brings your music back) |
| The track's automation, keeping the steps | SEQ TOOLS **F4 CLR AUTO**, or SEQ > AUTOMATION > CLEAR |

A track clear says **T3 CLEARED** with **SEQ+REC: ALL TRACKS** under it as a reminder of the all-tracks combo.

---

## 7. ARRANGE: songs, sections and scenes

### The pieces

- A **project** is a song. There are **8 projects**.
- Each project has **4 sections, A–D**. A section is a full snapshot of the music: every track's sound, steps and
  automation, the DRUM-X kit and its mutes, the punch-in lane, the SLICER, the macros, plus the tempo and global effects.
- Each project has one **arrangement**: up to **16 rows**, each a section played for **1–64 bars**, optionally with a
  **scene**, and an **END** rule.

That's 32 sections in total (8 × 4).

### The ARRANGE page (SAVE > ARRANGE)

The song as a column of rows. Each row shows its section chip (A–D, each in its own colour), its bars as blocks that
fill as they play, its bar count and a dot if it has a scene. A row pointing to a section with nothing stored says
**EMPTY**. Below the rows: **+ ADD** (with how many of the 16 rows are used) and **END**. On the right, the whole song as
one strip with the play head, and the total bars.

| Knob | On a row | On + ADD | On END |
| --- | --- | --- | --- |
| KNOB 1 (or PRESETS) | choose the row | | |
| KNOB 2 | its section A–D; turned left past A: **DELETE SONG ROW?** | adds a row (same section as the row above, 1 bar) | **STOP**, **x2 … x8**, **LOOP** |
| KNOB 3 | its bars (1–64) | adds a row | |
| KNOB 4 | right: store a **scene** (SCENE STORED); left: clear it | | |

- **PLAY** (or OCT+) on ARRANGE plays the song from the top; again: stop.
- When the song ends (or you stop it), the music you had before pressing PLAY comes back (SONG END).
- END: **STOP** plays the song once, **x2–x8** that many times, **LOOP** forever.
- If a row points to an empty section the song won't start: the screen says **PATTERN A EMPTY** (or B, C, D).
- While a song plays, step editing is blocked (**STOP TO EDIT**).
- To delete a row you can also use SAVE > TOOLS > DELETE ROW (the row ARRANGE has selected). To delete the whole
  arrangement: TOOLS > CLEAR SONG (**CLEAR SONG ORDER?**). Neither can be undone.
- The arrangement is saved automatically (about 2 seconds after a change, while stopped). See [12](#12-saving-and-loading).

### Scenes

A scene is stored per row and recalls, when that row starts: the track mutes (T1–T6), the DRUM-X group mutes, the
macros M1–M4 and the punch-in note effects being held. It stays in effect through the following rows that have none.
Scenes apply when a song plays, not in a quick chain. SONG REC also stores one per row as it begins.

### Playing sections live (SAVE layer)

Hold **SAVE**:

- **F3–B3**: play section A–D. While playing it comes in on the **next bar**; stopped, it loads at once.
- Tap **two or more** of F3–B3 in one hold: a **quick chain** — they play in turn, each for its longest pattern's
  length, round and round, starting on the next bar (stopped: at once).
- **C4–F4**: **store** the music now into A–D (works while playing; see [12](#12-saving-and-loading) for when it
  reaches flash).
- **OCT+**: store into the section playing (PLAY A SECTION FIRST if none). **OCT−**: recall it as stored.
- **D5**: **LOOP / SONG** — in SONG mode, PLAY from any page plays the arrangement.
- **E5**: **SONG REC** — the sections you play live (and their bars) are written into the arrangement's rows, with a
  scene each. It replaces the current rows.
- **G5**: jump to ARRANGE.
- **KNOB 1**: pick another project; it switches when you let go of SAVE (stopped only) and **loads its section A**.

---

## 8. Dice and UNDO

### GENERATOR — the song generator (HOME + SELECT)

Open it with **SELECT** on HOME, **HOME held + SELECT** from any page, or **OCT− held + PLAY** (GW-1). The screen,
**GENERATOR 1/5**, has five pages of four settings: **SELECT** turns the page — a ring of seven: the five settings
pages, the **TAKES** reel and the **SET** page (below) — **KNOB 1–4** set the four rows, **ALGORITHM** gives the piece
new instruments (below), **PRESETS** opens the **TAKES** reel (below).

The same buttons mean the same thing on every page: **OCT+** makes (the piece, the take under the cursor, a SET
entry), **OCT−** goes back one page (the SET page → the reel → the settings → closed), **HOME** closes from any page,
**SAVE** keeps the piece playing as a take. What a control did shows **in a band at the foot of the screen** for a
second and a half (**TEMPO 144 BPM**, **ON THE NEXT BAR — NEW DRUMS**), over the footer only: the take's card, the
song's rows and the settings stay in sight; a change waiting for the next bar shows a bar filling up.

**NOW and takes.** The piece playing is **NOW**. Everything on this screen changes NOW — OCT+ (a new piece),
ALGORITHM, the reel's KEY and ENERGY, the OCT+ keys, JAM — and **nothing makes a take but SAVE**. A take is a piece
you chose to keep: SAVE gives it the next number on the reel and an ID. So you can turn, try and throw away freely, and
the reel holds only what you kept.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| 1/5 | **GENRE** (RANDOM or one of 49) | **SEED** (0000–9999) | **ENERGY** (0–100 %) | **FORM**: LOOP, PARTS, SONG |
| 2/5 | **DRUMS** | **BASS** | **CHORDS** | **ARP** |
| 3/5 | **LEAD** | **COMPLEX** (0–100 %) | **SWING** (0–100 % or AUTO) | **HUMAN** (0–100 %) |
| 4/5 SONG | **TEMPO** (AUTO or 60–200 BPM) | **KEY** (AUTO or C–B) | **SCALE** (AUTO, MIN, MAJ, DOR, PHRY, HARM, MIX, LYD) | **LENGTH** (AUTO, SHORT, LONG, EPIC) |
| 5/5 FEEL | **SPACE** (OFF–100 %) | **FILLS** (OFF–100 % or AUTO) | **WET** (DRY–200 % or AUTO) | **GRIT** (CLEAN–200 % or AUTO) |

The part rows (DRUMS … LEAD) go from OFF through 10–100 % to **AUTO** (the genre decides). SONG and FEEL refine any
genre; left on AUTO / OFF the genre decides, as before:

- **TEMPO**: AUTO is the genre's range (ENERGY sets the place in it); turned up from AUTO it starts at the song's
  tempo, then sets the piece's BPM. MENU > BPM LOCK still wins.
- **KEY**, **SCALE**: the piece's key and mode instead of the seed's and the genre's.
- **LENGTH** (FORM PARTS or SONG): the song's rows half as long, twice, three times, in whole loops of each section.
- **SPACE**: room for a voice. The melody and the arp lose the end of every two bars (the singer's answer) and some of
  their off-beats, and the music sits lower under the drums (melody, arp −4 dB, pad −3 dB at 100 %).
- **FILLS**: the drum rolls. OFF: none at all; 10–100 %: the rolls as made replaced by snare rolls on a phrase's last
  beat (the 4th bar's always, the 2nd's from 50 %, every bar's from 80 %), faster (1/64) from 70 %.
- **WET**: the reverb and delay sends, as a share of what the genre gives (DRY: none, 100 %: as made, 200 %: twice).
- **GRIT**: the master clip, the bass's distortion and drive and the drums' drive, as a share of the genre's
  (CLEAN: none, 200 %: twice; WRAITH's BLAST on an 808 stays a blend, its sub kept).

**OCT+** makes the piece (the music playing: on the next bar); **ALGORITHM** gives it **new instruments**: every
role's sound picked again (the notes stay; a locked sound stays), made by itself once you stop turning (about half a
second; playing, on the next bar), turned back the sounds before. A **LOOP** piece always plays as a loop (an earlier
PARTS / SONG piece no longer leaves its song playing); **PLAY** starts and stops it so you can hear it without leaving
the screen; **HOME** (or OCT−) closes the screen. The screen shows the take NOW is (**TAKE #0A3D TRAP ATL 0202**) and
the header says **GEN:** and the piece.

**UNDO for NOW**: **GLO held + G4** steps NOW back (its last 8 states: each change, each take played), **GLO held +
A4** forward again — on the next bar while playing; the tempo stays as it is. At NOW's first state GLO + G4 is the
device's UNDO (the music from before the GENERATOR; again: forward), as is OCT− held + GLO outside the screen. If the
music changed outside the GENERATOR since (a project loaded, the song edited), NOW starts afresh when the screen opens.

**The piece is deterministic**: the same genre, seed and settings always make the same piece, and each part draws on
its own stream, so turning one part's row leaves the others as they were. **OCT+ with nothing changed** throws a new
seed. The screen opens on RANDOM each time.

#### TAKES — NOW and the reel, the songwriter (GW-1)

**PRESETS** opens the **TAKES** reel on **NOW** (the turn that opens it only opens it: nothing is cued, the music stays);
turned on: NOW first, then takes 1 … n (**TAKE 2 OF 3** in the title), round. Each card shows:

- **NOW**'s card: **NOW** and the genre; on the right **TAKE 2** when NOW is that take as saved, else **EDITED**; under
  it **the take it came from and what changed last** (**FROM TAKE 1: ENERGY**; a new piece: **A NEW PIECE**);
- a take's card: its **ID** (**#0A3D**, never given to another take while this one is on the reel) and the genre;
  **NOW** when it is the music, **NEXT** when it waits for the next bar; on the right the take it was made from and
  what changed (**FROM 0A3C: LEAD SND**);
- the trap flavour (ATLANTA, PI'ERRE, METRO, DRILL, MEMPHIS, WHOLE LOTTA RED, MUSIC) or the tracks it ran on and
  LOOP / SONG; the seed, the tempo, the key and the scale;
- a thumbnail of the first two bars of each live track (4 or 6 rows), coloured by part: drums, bass, pad, lead, arp;
- the song's rows as blocks, each as wide as its bars (trap: **I**ntro, **H**ook, **V**erse, **P**re-hook; other
  genres A–D), with the playhead moving on the music; a LOOP piece shows **LOOP**.

**OLD GEN** marks a take another firmware's generator made (it loads; it may sound different); the cards peeking in at
the sides are the ones before and after. When a change lands, the reel shows NOW again.

| On the reel | |
| --- | --- |
| **PRESETS** | NOW, then the takes, round. Playing: the take under the cursor becomes the music **on the next bar** (NEXT on its card). |
| **OCT+** | On a take: it (stopped: at once; playing: on the next bar). On NOW: a new piece. |
| **PLAY** | Stopped, on a take's card: that take, playing. Else: play / stop. |
| **KNOB 1** | **TEMPO** of NOW, at once (a saved take keeps its own; SAVE keeps this one). MENU > BPM LOCK still wins. |
| **KNOB 2** | **KEY**, a semitone a detent: NOW made again once the knob rests (playing: on the next bar). |
| **KNOB 3** | **ENERGY**, 5 % a detent: NOW made again once the knob rests. |
| **KNOB 4** | The **section** the part keys make again: ALL, B, C, D (trap: the verse, the intro, the pre-hook). A loop has none. |
| **ALGORITHM** | New instruments for NOW. |
| **SAVE** | **NOW kept as a take** (the next number; refused when NOW is already a take, or the reel holds 32). **OCT+ held + SAVE**: type a take's **share code**. |
| **EDIT** | Delete the take under the cursor (OCT+ yes, OCT− no). **Held**: every take (asks first). NOW is never deleted. |
| **REC** | Playing: record a **SET** (again: stop recording). Stopped: it says to play first. |
| **SELECT** | The next page (the SET page); the ring goes on round to the settings. |
| **OCT−** | Back to the settings. **HOME** closes the GENERATOR. |

**Hold OCT+** — on every page of the GENERATOR screen but the SET page: the keys are silent and **shape NOW**, on the
next bar (stopped: at once), and **the whole screen is the keys' map**: the keys in two rows (the left half of the
keyboard, F3 … F4, over the right half, G4 … G5), each filled in its part's colour; under them the parts' names in the
same colours; then two lines that say **what the key down does** ("G3 BASS: NEW NOTES WHEN LET GO") and **what a key
just did**; then the other keys (S ALL, H HARM, M MIX, V VARY; < > ARR, O ORIG, J JAM, # CODE). A locked part or sound
is dotted, a key down lit. The key LEDs show it too: every key with a job breathes; a key down, a locked part or sound,
a dropped part and JAM on are lit. Held to read the map, let go without a key: nothing happens; let go after a key, a
knob or a button: nothing more happens (no piece, no load).

The map is **mirrored**: each part's drop key is under its part key, seven white keys to the right.

| Keys (OCT+ held) | |
| --- | --- |
| **F3 G3 A3 B3 C4 D4** | A part made again: DRUMS, BASS, CHORDS, ARP, LEAD, TEX (its notes; the others stay), when the key is let go. With KNOB 4 on a section: in that section only. |
| **F#3 G#3 A#3 C#4 D#4**, **F#4** | The sound of the part on the left (the kit, the bass, the pad, the lead, the texture); F#4 the arp's. The notes stay. |
| **E4** | Every sound (not the locked ones). |
| **F4** | The harmony: root, scale, chords, the bass's figure (the genre and its flavour kept). |
| **G4 A4 B4 C5 D5 E5** | **Drop** the DRUMS, BASS, CHORDS, ARP, LEAD, TEX **while held** (let go: back; NOW the same). |
| **F5** | The mix (levels, sends, the master's effects). |
| **G5** | VARY: the ornaments (echoes, octaves, bends, the kit's drift). |
| **G#4 / A#4** | The arrangement (four a form: trap's as made, hook first, two hooks, verse first). |
| **C#5** | **ORIG**: NOW back to the take it came from, all at once (GLO + G4 undoes it). |
| **D#5** | **JAM** on / off. **Held** half a second: JAM's bars, 4, 8, 16, 32 (round; again and again). |
| **F#5** | Show the card's share code (let go of OCT+ to read it). |

- A part key or a sound key **held half a second locks** it: ALGORITHM, E4 and JAM leave it alone, and **a new seed
  keeps it** (OCT+ on the settings: the new piece gets the locked part from the old one).
- With OCT+ held, **PRESETS only cues**: the cursor moves, nothing loads; let go of OCT+ and the cue plays on the next
  bar.
- On **a take's card** (PRESETS turned with OCT+ held), a part key or a sound key **borrows** that take's part or
  sound into NOW (its drums under this song, for instance). With the same genre a borrowed part keeps its choices; the
  harmony around it is NOW's.

**Hold OCT−** — **the takes' keys**: the white keys F3 … G5 play **takes 1 … 16** on the next bar (stopped: at once),
like launching clips. The screen shows them while OCT− is held: the take playing lit, the one next outlined, keys with
no take dark (they do nothing), and under them the take touched (number, ID, genre, tempo, key). Takes 17 … 32: with
PRESETS. Held to look and let go without a key: nothing (a tap of OCT− still goes back).

**JAM** (D#5): every few bars (D#5 held) **NOW changes by itself** — one thing made again: a part's notes mostly, now
and then a sound, the ornaments or the mix, never a locked one. It runs while the GENERATOR screen is open; SAVE keeps
a moment you like.

**The SET**: takes in a row, each for its bars. **REC** while playing records one: the take playing first, then every
take you play (OCT− + a white key, or PRESETS), each with the bars it played, rounded to whole loops of its take (so
each entry plays whole phrases); REC again ends it. **SELECT** on the reel
opens the **SET page**: the takes as blocks as long as their bars, the playhead moving; **KNOB 1** (or PRESETS) the
entry, **KNOB 2** its take, **KNOB 3** its bars, **OCT+** adds the take playing after the entry, **EDIT** deletes it,
**PLAY** plays the SET from the entry (each take queued a bar before its turn, round), PLAY again stops. While it
plays the cursor follows the entry playing, and **turning KNOB 1 to an entry plays it from the next bar**. **OCT−**
back to the reel, HOME closes. ALGORITHM and the take keys do nothing there (the SET leads), and JAM waits while a SET
plays.

**Share codes**: **F#5** (OCT+ held) shows the card's code, e.g. **8350 YG00 DWDW C0GJ 0GV**: its recipe (the seed, the
settings that are not as the screen opens, the take's own choices), the last character a check. **OCT+ held + SAVE**
opens the NAME screen for a code (up to 28 characters; 123 first, D# switches to letters; I L O U are read as 1 1 0 V);
**OCT+** makes it NOW (SAVE keeps it), a wrong character is caught ("CODE: A TYPO"). The same firmware makes the same
piece; a code made on the other number of tracks says so (GLO > SYSTEM > TRACKS). A take as made is 7 characters.

**Kept over power-off**: the reel (its takes), its SET and JAM's bars are saved in flash once they have stayed
unchanged for 10 s with the music stopped (at most once a minute) and are back at power-on. A take is the **recipe** —
not a recording: loading it makes the same piece again exactly, but changes you made to the piece afterwards are not on
it. To keep the music itself, SAVE it (SAVE > PROJECT, or a song slot); to keep the sound, record it over USB audio.

**The genres** (49): GAMMAWAVE, WRAITHWAVE, TRAP ATL, UK DRILL, DARK TRAP, RAGE TRAP, HOUSE, DEEP HOUSE, MICRO HOUSE,
TECH HOUSE, ACID HOUSE, TECHNO, DETROIT, DUB TECHNO, HARD TECHNO, HYPNOTIC, TRANCE, ELECTRO, EBM, ITALO, SYNTH POP,
DARKWAVE, POST PUNK, TRIBAL PUNK, MINIMAL SYNTH, MAN MACHINE, MOTORIK, INDIE, SHOEGAZE, POST ROCK, FUNK, DISCO,
BOOM BAP, TRAP, UK GARAGE, DUBSTEP, BREAKS, DRUM & BASS, DUB, STEPPERS, JAZZ, BOSSA, AFROBEAT, AMBIENT, NEO CLASSIC,
IMPRESSION, SOUNDTRACK, IDM, BROKEN. Each has its tempo range, scales, progressions, drum family (or none) and kit,
bass figures, how its chords move, how its lead and arp speak, the sounds it likes per role and how wet it is.

**ENERGY** thins or fills the piece: low energy leaves two or three things sounding with long tails, high energy brings
in every role, denser, with ratchets. **The drums may not play at all.** A role left out keeps its sound with an
empty sequence, ready to play or record.

**FORM**: **LOOP** writes one section; **PARTS** stores the whole piece in section A and variations with parts taken
out or changed in B, C (and D), chained in ARRANGE (A B A C); **SONG** makes a longer club or arc form (intro, build
with a filter opening and a roll, drop, break), each section 16 bars. A section you saved yourself is never
overwritten: then the piece stays a loop. Sections are 32 or 64 steps, depending on how fast the genre's chords move.

**Which parts get a track**: the drums, the bass, then the melodic parts by importance. With MENU > TRACKS on 4
(three melodic parts live) that is the bass, the genre's foreground — its lead, or its arp where the genre leans on the
arp — and the chords; with 6 the bass, the chords, the arp, the lead and a texture (or, in the trap family, the triplet
hats). DICE ALL always moves the tempo (one BPM off the last one when a throw lands on it).

What the generator knows, per genre (`gen_tables.c`):

- **Harmony**: the genre's scales and progressions, every chord with its own quality, voiced between C3 and G5 and
  led to the nearest voicing (or moved in parallel where the genre does).
- **Drums**: a map of priorities — a hit plays where its level clears the density, so ghost notes go first and the
  anchors last; the snare laid back where the genre does; hi-hat rolls in the trap family; the kit's MORPH drifting
  a little over the loop now and then.
- **Bass**: one of about fifty figures written against the kick (the root at every chord change, approaches by a
  semitone, the kick doubled, answered or interlocked).
- **The trap family** (TRAP, TRAP ATL, UK DRILL, DARK TRAP, RAGE TRAP) has its own generator, made for a voice on
  top: the drums and the 808 loud, the music sparse and behind them. Each piece has a flavour, picked by the seed:
  TRAP ATL plays Atlanta (dark bells and keys), Pi'erre (156–166 BPM, major or mixolydian, flute, organ and chip-arp
  leads, long tails, swing, filtered drums, a lighter 808) or Metro (a brass or string loop, choirs, a pummeling 808,
  116–124 or 140–150 BPM); DARK TRAP plays Memphis (the cowbell) or Metro; RAGE TRAP plays Whole Lotta Red (140–150
  BPM in major or lydian, square video-game leads, a crushed 808) or MUSIC (150–160 BPM, minor, wobbling synth stabs).
  The snare and the clap on beat 3, the kick
  around it, and the hats as the groove — eighths or sixteenths with 1/32 and 1/64 rolls before the snare and at the
  bar's end, triplet bursts and shuffled percussion (on a second DRUM track at 16T when TRACKS is 6), a crash on the
  hook's downbeat, drill's 3-3-2 gallop and late snare, dark trap's cowbell. The bass is an 808, a driven sine (dark
  trap and rage clip it), played as spaced patterns: two to six notes a bar, long and short ones with rests between
  them, a slide or two into the notes that matter (drill the most), the kick under each note. Its sound is one of
  ten recipes ported from GammaWave's and WRAITH's 808s (clean, deep, thumping, gliding, crushed, distorted, WRAITH's
  slide and dirty ones): they decay rather than hold, start a little sharp, click and glide. The
  melody is a two-bar motif of chord tones — a call, then a short answer whose end stays empty — about 10 dB under
  the drums (bells, keys, a flute; strings and choirs in drill; a saw hook in rage), over a low-passed pad; in the
  hook a counter melody replies in the gap (on TRACKS 4 the melody plays the reply itself). With FORM on SONG it makes the rap layout: intro (4 bars, the melody
  alone, its filter opening), hook (8), verse (16: another kick, plainer hats, a softer instrument playing only the
  calls, a darker pad; its last 4 bars a pre-hook where the drums and the 808 stop for a snare roll), hook, verse,
  hook, outro. PARTS stops after the second hook. The hook loop is four bars.
- **Arp and lead**: the arp as an order over the voiced chord (cycles of 3 or 5 against the bar); the lead as a
  one-bar motif in a phrase (A A A A', A R A R' …), varied by one change a bar.
- **Sounds**: a factory sound per role whose name fits, with its envelope, filter, a slow LFO and sends shaped for
  the role; HUMAN by how human the genre is, unless you set it.
- **Tempo**: inside the genre's range, higher with more ENERGY (the same seed, the same tempo), unless MENU >
  **BPM LOCK** is on. Swing, delay and reverb follow the genre (the wet genres — dub, dub techno, shoegaze, ambient —
  get long tails).

Making a piece replaces every track's steps, and with them their automation. **UNDO** (GLO held + G4, or OCT− held +
GLO) brings back the music from before.

### Sound dice (EDIT + SELECT)

Hold **EDIT** and turn SELECT (from any page): the selected track loads a random factory preset of its engine and each engine
parameter moves up to a quarter of its range. On DRUM, both sides of every kit sound are rolled around the factory kit.
You hear it at once under **KEEP DICED SOUND?** (DRUM: **KEEP DICED KIT?**): turn SELECT again for another roll,
**OCT+ OK** keeps it, **OCT− CANCEL** brings back the sound you had before the first roll (only that track's sound:
the music plays on). After OK, **GLO held + G4** still undoes it.

### Sequence, LFO and FX dice (SEQ / LFO / FX + SELECT)

The same idea for the other parts of the selected track, each heard at once under its **KEEP DICED …?** dialog
(SELECT again: another roll, **OCT+ OK** keeps it, **OCT− CANCEL** puts back what was there, the rest plays on):

- **SEQ + SELECT**: a new sequence as the GENERATOR writes a part, in the track's own scale: a beat on a DRUM track, else
  the role its steps sound like (low notes: a bass line, chords: a pad, else a lead or an arp). Its sound, mix and
  automation stay.
- **LFO + SELECT**: the track's LFO: a rate in the middle of the range, any wave and phase, a fade now and then, aimed
  mostly at the filter, a little at the shape, seldom at the pitch or the level.
- **FX + SELECT**: the track's drive (half the time none), its chorus, delay and reverb sends, an AUTO PAN one time in
  three, and the shared reverb's kind, size and damping and the delay's time and feedback (pitched one time in five).

### Macro dice

With the macros open (LFO held) or latched: **F3** rolls new macro routes on the tracks that are sounding;
G3 clears them. See [10](#10-modulation-and-macros).

### UNDO (GLO held + G4)

One step back over the **last big change**. Pressing it again goes forward (redo). The transport keeps playing.
G4 breathes on the GLO map when there is something to undo; with nothing, it says **NOTHING TO UNDO**.

**UNDO covers**: a piece made on the GENERATOR, a Hearth macro (OCT− + ARP), a sound diced, a track cleared (REC F3, TOOLS CLEAR SEQ), CLEAR ALL SEQUENCES (SEQ + REC),
NEW PROJECT, INIT and RECALL (EDIT layer or TOOLS), clearing the automation (SEQ > AUTOMATION),
and a section loaded with **SAVE > PROJECT > LOAD**. It restores the music as it was: sounds, steps, automation, the
kit, the punch-in lane, the macros, the tempo and global effects.

**UNDO does not cover**: section jumps, quick chains and RECALL from the SAVE layer, switching project with the SAVE
layer's KNOB 1, CLEAR SONG, deleting a row, SONG REC, anything saved (sections, user presets), user preset loads,
sounds picked with PRESETS, SEQ TOOLS actions, or ordinary knob turns. It is not available while a song plays
(**STOP TO EDIT**). It keeps only one step, in RAM: it's gone after power-off.

---

## 9. Effects and master

### Per track

- **FX**: DIST amount and the three sends (CHORUS, DLY, REV).
- **DIST**: amount, **TYPE** (SOFT, HARD, FOLD, CRUSH, RECT, and GW-1's **HEARTH** — warm tube-style drive with a
  little high-frequency edge and low weight; **BLAST** — a two-stage sine wavefolder over a soft-clipped body;
  **PLASMA** — saturation warped by the signal's own envelope; **FLUX** — a drifting bias and a bit-starved grain that
  grow with the amount), **TONE** (darker / brighter).
- **AUTO PAN**: the track's pan swung left and right around its PAN. **DEPTH** (0 = off), **RATE**, **SYNC** (BEAT:
  RATE in divisions 4BAR … 1/32, locked to the bar; FREE: RATE in Hz), **WAVE** (SIN, TRI, SAW, SQR, S&H).
- **FILTER**: see [4](#4-engines).

### SLICER (one, on a bus)

- **SLICER**: **MODE** OFF, GATE, STUT; **PAT** 1–16; **RATE** 1/8 … 32T; **DEPTH**.
- **SL BUS / SL BUS 2**: ON / OFF for what goes in: **T1–T6** and **FX** (the delay, reverb and chorus returns).
  Everything on chops the whole mix after the effects; with FX off the tails ring free.
- **SL PITCH**: the repeats follow a 16-step pitch sequence (±24 semitones). KNOB 1 the step, KNOB 2 its pitch, LEN,
  QUANT (CHR or SCL: the song's scale).
- Each section keeps its own SLICER settings. With **LOAD ECO** there is no SLICER (its tracks go straight to the mix).

### Delay

- **DLY**: **TIME** (synced divisions 4BAR … 1/32, or free times from 5 ms to 1.48 s), **FEEDBK**, **TONE** (below the
  middle darker, above thinner), **MIX**. FEEDBK behaves like an analog delay's (a tape, a PT2399; 0.6.86, 0.6.89):
  with **TONE in the middle (50 %)** each repeat is a copy of what went in; at **100 %** the echoes hold their level
  for a long time (they fade very slowly and are never cut off), and **past 100 %** they grow into themselves and
  hold, saturating softly instead of clipping (turn it back down to let them die). Up to 80 % it is as before.
  **TONE** is resonant: from the middle down a low-pass sweeping from ~8 kHz to ~250 Hz, from the middle up a low cut
  sweeping from ~70 Hz to ~3 kHz, both with a peak at the cutoff, so the repeats get darker or thinner and narrow to
  a band each pass (dub style), and at either end with FEEDBK high the loop self-oscillates at that band.
- **DLY 2**: **WIDE** (the right echo later), and the **GRAIN** delay: **PITCH** (±12 semitones per repeat) and
  **SPRAY**. With LOAD LITE or ECO the GRAIN delay plays as a plain delay (the card says LITE / ECO). **MOD**
  (0.6.86): the tape's drift, slow WOW and fast FLUTTER on the repeats (0, the default: none, exact copies; ~20 a
  tape's; up to a seasick chorus); the repeats stay bright while they drift (0.6.89); saved with the project.
- **DLY 3** (0.6.90): **TYPE**: **ANALOG** (the default: the soft saturation, the resonant TONE, FEEDBK past 100 %
  self-oscillating) or **DIGITAL** (clean repeats, TONE a plain filter, FEEDBK 100 % at most: past it the repeats
  just hold, never grow). **PING**: **ON** sends the repeats left, right, left … across the stereo field, each one
  FEEDBK softer (TIME up to 0.74 s; WIDE and the GRAIN delay out of it). Saved with the project.

### Reverb (five models)

**REVERB > TYPE** picks the model; **SIZE** is the decay on every model. KNOB 3 and 4 change meaning with the type:

| TYPE | What it is | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- |
| **ROOM** | Classic room | DAMP | PRE (pre-delay, ms) |
| **SPRING** | Amp spring tank | DAMP | PRE |
| **SHIMR** (shimmer) | Rises an octave on every pass | DAMP | **AIR** (±24 st: darker and lower / airier and higher) |
| **RESO** | Four strings tuned to the song's scale | **FOLLOW** (OFF / T1–T6: the chord follows the lowest note that track plays) | **PITCH** (the chord 0–14 steps up the scale) |
| **CLOUD** | Granular wash; SIZE at max freezes it (from silence it first takes ~0.7 s of sound) | DAMP | **AIR** |

**REVERB 2**: **MOD** (modulation depth), **RATE**, **FILT** (input tone: darker / brighter; 0.6.86 resonant, a band at the cutoff to carve the room's place), **WIDE**.

With **LOAD LITE**, SHIMMER and CLOUD play as ROOM and RESO as SPRING; with **LOAD ECO** every reverb plays as SPRING.
The settings stay; the TYPE card shows LITE or ECO under its value.

### Chorus

**CHORUS**: RATE, DEPTH, **STEREO** (0 as always, down to mono, up to about twice as wide). Hidden with LOAD ECO
(a track's chorus send card then says ECO).

### MASTER

- **CLIP**: saturation that adds character, not volume.
- **PUNCH**: fattens the DRUM tracks.
- **DUCK**: the kick pulls the rest down; **REL** is its release.

---

## 10. Modulation and macros

- **ENV DEST**: the envelope to FILTER, PITCH, SHAPE; **LOOP ON**: with the note held, the envelope goes back to the
  attack when it reaches sustain (an ADSR-shaped LFO).
- **LFO**: RATE, WAVE (SIN, TRI, SAW, SQR, S&H), PHASE, FADE. **LFO DEST**: PITCH, FILTER, SHAPE, AMP.
  - **RATE** is free in Hz; turn it left past the slowest and it goes **in time** with the tempo (4BAR, 2BAR, 1BAR, 1/2,
    1/4, 1/8D, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32), locked to the bar while the transport plays (PHASE its offset).
  - **PHASE** is where a new phrase restarts the LFO; turn it left past 0 to **FREE**: it never restarts, it just runs.
    In FREE, **FADE** comes in once (when the project loads), not again on every note.
- **MOD matrix**: 4 slots per track (KNOB 1 picks the slot).
  - Sources: OFF, LFO, ENV, VEL, KEY, RAND, MODW (MIDI CC1), AT (aftertouch), EXPR (CC11), **M1–M4**, **STEP** (MSEQ),
    **NOISE** (1.2.1-GW: a smooth random LFO, each track its own wander, a new value about every 46 ms and eased in
    between; RAND instead draws one value per note).
  - Destinations: PITCH, CUT, SHP, AMP, PAN, DIST, CHO, DLY, REV, RATE (LFO rate), VIB, **E1–E8** (the engine's own
    EDIT knobs, shown by name), **CLIP** and **PNCH** (the master).
- **MSEQ**: a 16-step modulation sequencer per track, like a CV sequencer. KNOB 1 picks the step, KNOB 2 sets its
  level, LEN and SLEW; DIV on MSEQ 2. Use it from MOD with SRC **STEP**.

### Macros M1–M4

- **Hold LFO**: KNOB 1–4 become M1–M4, and the map shows where each one goes ("NO ROUTE: MOD SRC" if nowhere).
- Let go: they stay **latched** — KNOB 1–4 keep moving the macros on any page — until you press LFO again.
- Every power-on starts with random routes (two per track) on tracks that have none. **F3** rolls new
  routes on the tracks that are sounding; **G3** clears them.
- Routes are ordinary MOD slots (SRC M1–M4): edit them by hand on LFO > MOD. Macro values are saved with each section.

---

## 11. Performance

### Punch-in FX (hold FX)

Effects that act while their key is held and go back exactly when you let go.

- **On the audio**: REPEAT 1/8, 1/16, 1/32 (F3 G3 A3), LPF (B3), HPF (C4). REPEAT starts on the next 1/16 while
  playing. Below about 81 BPM, REPEAT 1/8 is too long for its buffer and is shown dimmed.
- **On the notes**: OCT− / OCT+ (D4 E4), 1/2 TEMPO (F4), DEC− (short, plucky) / DEC+ (long) (G4 A4), STUTTER 1/8 / 1/16
  (B4 C5), ATK+ (slow attacks, drums too) (D5), ARP (E5; on a DRUM track a new fill every press), RANDOM (F5). They come
  in on the next 1/16. **A#4** chooses which tracks they touch: ALL, SYN (synths) or DRM (drums); the header shows it.
- **Knobs** while FX is held: **FILTER** (left low-pass, right high-pass), **CRUSH**, **THROW** (sends the dry mix into
  the delay and reverb), **DEPTH** (the REPEAT's level).
- **Black keys 1–6**: mute T1–T6 while held (not saved).
- **(QG-1) The other black keys**: **G#4** VIBRATO and **F#5** DIVE act on the notes like the effects above (A#4
  chooses the tracks; recorded in the punch-in lane; an older firmware ignores those steps). **C#5** GATE and **D#5**
  SWEEP act on the master like LPF / HPF (GATE ramps, no clicks; KNOB 4 DEPTH sets how far it shuts).

**The punch-in lane**: each section has a lane of 64 steps of 1/16 (4 bars). With **REC armed and playing** (not
during a song), the **note effects (D4–F5)** you hold are recorded into it, one per step (REPEAT, LPF, HPF and the mutes
are not recorded). Hold **FX + G5** while playing to erase where the play head passes; stopped, FX + G5 erases the whole
lane. **REC held + FX** also clears it (along with the track's FX automation).

**Lock**: hold FX until its map opens and let go without touching anything: the FX layer stays open (**[FX]
LOCKED**), as the macros do with LFO; the keys play the effects (momentary: held on, let go off), the knobs are FX's
macros and OCT− is ALL OFF, with both hands free. Press **FX** again to unlock (latched effects you left on stay on). Held with keys or knobs, FX
works as before and closes when let go.

**FX LATCH** (MENU > CONTROL > FX LATCH, **OFF by default**; ON by default from 0.6.44 to 0.6.63): ON, a key pressed
with FX toggles its effect on and off, the knob macros keep their values, **FX + OCT−** turns everything off (FX ALL
OFF). Latched effects are not recorded into the lane.

### Mutes and solos

- **GLO held**: black keys 1–6 mute T1–T6 (latched, saved with the project and in scenes); F3–D4 solo while held;
  black keys 7–10 mute the DRUM-X groups; E4 unmutes everything.
- **MIXER** KNOB 4: MUTE of the selected track.
- **EDIT held** on a DRUM track: black keys 1–8 mute each DRUM-X sound.
- **FX held**: black keys 1–6 mute while held.

### Live layers and the song

The SAVE layer (section jumps on the bar, quick chains, store, recall, SONG REC) is described in
[7](#7-arrange-songs-sections-and-scenes). GLO + PLAY restarts from the top without stopping.

---

## 12. Saving and loading

This is the chapter to read carefully. Pocket GammaWave keeps several kinds of data in different places, saved in different ways.

### The big picture

- **The music** is what's playing now, in RAM: all six tracks' sounds and steps, automation, effects, tempo.
- **Sections** (A–D of each of the 8 projects) are where you **save** music permanently, in flash.
- **The arrangement** (rows, END, scenes) belongs to the project and saves itself.
- **User presets** (32 slots) save single sounds.
- **Settings** (the MENU, TRACKS, LOAD) save themselves.
- **The autosave** is a safety copy of the music, so you find it again after power-off. **It is not a save into a
  section**, and it does not happen at power-off.

### What is saved where

| What | Where it lives | How it gets saved | Survives power-off? |
| --- | --- | --- | --- |
| The music now (sounds, steps, automation, tempo, effects, kit, macros, punch-in lane, SLICER) | RAM | Not saved by itself — store it into a section. The **autosave** keeps a copy. | Only via the autosave |
| A **section** (A–D of a project) | Flash | **SAVE > PROJECT > SAVE** (stopped), or **SAVE layer C4–F4 / OCT+** | Yes (layer stores: once you stop) |
| The **arrangement** (rows, bars, END, scenes) of each project | Flash (song index) | Automatically, ~2 s after a change while stopped | Yes |
| **User presets** U01–U32 (one sound + a 16-step pattern) | Flash | **SAVE > USER > SAVE** (stopped) | Yes |
| **Settings** (MENU, TRACKS, LOAD, favourites) | Flash | Automatically (MENU: when it closes; while playing: after you stop) | Yes |
| The **autosave** | Flash, its own place | 10 s after the last change, stopped and idle, at most once a minute | Yes (restored at power-on if RESTORE LAST is ON) |
| The **FM6 voice bank** B1–B32 | Flash, its own place | Sent from the web editor (until then: GW-1 KEYS, built in) | Yes (not in the backup: keep the .syx) |
| **UNDO** | RAM | — | No |

### How the autosave really works

- It writes a copy of the music **after 10 seconds without changes**, only when the transport is **stopped**, no key or
  button is held and nothing is sounding, and **at most once a minute**.
- It does **not** save when you switch off. If you change something and power off within those seconds, that last
  change is lost. To be safe: **stop, wait ~10 s**, or better, save into a section.
- At power-on the device starts at zero, as **NEW PROJECT** leaves it (every track its engine's default sound, no
  steps). With **MENU > SYSTEM > RESTORE LAST ON** the autosave becomes the music instead (the screen says
  **RESTORED**). RESTORE LAST is **OFF** by default since 0.6.40: no autosave at all.
- After a crash, a hang or a failed start, the autosave is not restored (the music that was playing could be what
  crashed).

### To save a project section

1. **Stop** the transport (saving needs it stopped: otherwise **STOP TO SAVE**).
2. Press **SAVE** until you reach **PROJECT** (SAVE opens USER first, then PROJECT).
3. **KNOB 1**: the project (1–8). Turning it does **not** change the music you're playing — it only chooses where to
   save. (It switches projects, so do this stopped.)
4. **KNOB 2** (or PRESETS): the section A–D. The list shows each section's name (or USED / --), **NOW** on the one
   the music came from, and the bars it plays in the arrangement.
5. **KNOB 4** to the right: SAVE is picked ([OCT+]). Press **OCT+**.
6. If the section already holds something: **OVERWRITE PROJECT A?** (with its name). OCT+ YES.
7. The **NAME** screen opens with a name ready. Edit it or not, then **OCT+** writes. **OCT−** cancels and writes
   nothing.
8. The screen says **SAVED**.

A section stores the music **including the tempo and global effects**. The **project's name** is its section A's name.

### To load a section

1. SAVE > PROJECT, **KNOB 1** the project, **KNOB 2** the section.
2. **KNOB 3** to the right (LOAD), press **OCT+**. The screen says **LOADED**.

LOAD **stops playback** and replaces the music: sounds, steps, automation, the kit, the punch-in lane, the macros,
and the **tempo, effects, MIDI routing and clock settings** stored with it. The arrangement stays. If you didn't mean
it, **GLO + G4** (UNDO) brings back what you had.

### Quick store while playing (SAVE layer)

Hold SAVE and press **C4–F4** to store the music into section A–D of the current project, or **OCT+** to store into
the section playing. This works **while playing**, without naming. The store is in **RAM at once and goes to flash
when you stop**. If you power off before stopping, it's lost — so stop before switching off.

### Copying

- **A section to another letter**: LOAD it, then SAVE it into the other letter.
- **A section to another project**: LOAD it, turn KNOB 1 to the other project (the music stays), SAVE into a letter.
- **"Save as" a new project**: with your music playing, stop, go to SAVE > PROJECT, turn KNOB 1 to an empty project
  and SAVE into A.

### Switching projects

- On **SAVE > PROJECT, KNOB 1** changes project but **keeps the music** (to store it there or load one of its sections).
- With the **SAVE layer's KNOB 1** (hold SAVE, turn, let go) the project changes **and its section A becomes the
  music** (if it has one). The previous music is replaced and **UNDO cannot bring it back** — save it first.
- Both need the transport stopped (**STOP TO CHANGE PROJECT** / **STOP TO CHANGE SONG**). Sections stored with the
  SAVE layer are written to flash before switching.

### Naming (the NAME screen)

Opens when you save a section or a user preset, and when you press **EDIT** on SAVE > USER or SAVE > PROJECT
(renames the selected slot; only the name is written). Up to 12 characters, upper case.

| Control | Does |
| --- | --- |
| White keys | Letters phone-style: AB, CD, EF, GH, IJK, LM, NO, PQ, RS, TU, VW, XYZ, then 123, 456, 789, 0-. — tap again within 0.8 s for the next character |
| Black keys (by name) | **F#** cursor left, **G#** space, **A#** cursor right, **C#** delete, **D#** switch ABC / 123 (digits and symbols, one per key) |
| KNOB 1 | move the cursor |
| KNOB 2 | change the character at the cursor |
| OCT+ / OCT− | write / cancel |

An empty name: a project section gets none, a user preset an automatic one.

### User presets (SAVE > USER)

32 slots for single sounds. **SAVE** opens straight on this page with SAVE already picked, so **SAVE, OCT+, OCT+**
saves the selected track's sound with the offered name.

- **KNOB 1**: the slot (U01–U32). **KNOB 2** LOAD, **KNOB 3** ERASE (asks first), **KNOB 4** SAVE (asks before
  overwriting, then NAME). Turn the knob right to pick, **OCT+** to do it. Saving and erasing need the transport stopped.
- A user preset stores the **engine, its sound parameters and a 16-step pattern**; an FM6 sound keeps its patch.
- It does **not** store the **DRUM-X kit** (that lives in the project sections).
- Loading one loads the sound only.
- Your user presets also appear when browsing with PRESETS, after the factory sounds.

### Saving FAQ

**I switched off and my last changes are gone.** The autosave only writes after 10 s of no changes while stopped, at
most once a minute, never at power-off. Stop and wait a moment, or save into a section.

**It says STOP TO SAVE.** Saving into a section or a user preset writes flash, which pauses the audio for a moment, so
it only happens stopped. Press PLAY to stop and try again (the NAME screen stays open; PLAY there only stops).

**It says SAVED (NO FLASH).** The flash chip wasn't detected, so the save only went to RAM. It survives a reset, not a
power-off. Restart the device; if it keeps saying it, see [16](#16-troubleshooting-and-faq).

**It says SECTION 90% FULL / TOO FULL TO SAVE / SECTION FULL.** A section has a fixed size. Very busy music (six tracks
of chords on all 64 steps plus lots of automation) can outgrow it. Saving a section that doesn't fit says SECTION FULL
and leaves the stored one untouched. Shorten patterns, clear unused steps or automation.

**I stored with the SAVE layer while playing and lost it.** Layer stores reach flash when you stop. Stop before
switching off.

**Is my arrangement saved?** Yes, automatically, a couple of seconds after each change while stopped.

**Where's my drum kit in a user preset?** User presets don't store the DRUM-X kit; save the project section instead.

**Can I back everything up?** Yes: the web editor's complete backup includes all 8 projects with their sections,
arrangements and scenes, the user presets and the settings. See [15](#15-web-tools-emulator-installer-editor-backup).

---

## 13. System: TRACKS, LOAD and the MENU

### TRACKS (GLO > SYSTEM, KNOB 1 — also MENU > AUDIO > TRACKS)

- **4** (the default): T1–T4 play; **T5 and T6 are asleep**. They keep their sound, steps and mix and cost nothing.
  If a note is sent to an asleep track (a key, MIDI, its steps) or PLAY starts with steps on them, the screen says
  **T5-T6 ASLEEP** and **GLO>SYSTEM>TRACKS 6**. Asleep tracks are dim on the GLO map, greyed on HOME and the MIXER.
- **6**: all six tracks play. Watch the load meter.

### LOAD (GLO > SYSTEM, KNOB 2 — also MENU > SYSTEM > LOAD)

How heavy the sound may get.

| LOAD | What changes while it sounds |
| --- | --- |
| **FULL** (default) | Everything as set. |
| **LITE** | SHIMMER and CLOUD reverbs play as ROOM, RESO as SPRING; the GRAIN delay plays plain; FM6 plays two voices. The screen draws lighter: the graphs animate at about 20 frames a second with simpler outlines, and values change at once (no rolling digits). |
| **ECO** | LITE, plus every reverb plays as a light SPRING, **no SLICER** (its tracks go straight to the mix), **no chorus** (their pages are hidden), at most **4 voices** per synth track and **6 voices** between all tracks (8 otherwise; the drums count too). When the load climbs, nearly silent tails are let go sooner. The screen at about 14 frames a second, the graphs simpler still. |

LOAD only changes how things are rendered: **projects keep their settings** and sound as saved on FULL (or on another
FM-1). The cards of swapped effects say LITE or ECO. LOAD frees **CPU**, not RAM: the firmware reserves all its memory
at start-up. The **CPU** card (KNOB 4 on SYSTEM) and the header's load meter show the audio load. TRACKS and LOAD are
device settings, not part of a project.

### ROUTE (GLO > SYSTEM, KNOB 3)

**CH1-6**: MIDI channels 1–6 play tracks 1–6, channels 7–16 are ignored. **SEL**: every channel plays the selected
track.

### GLOBAL (GLO > GLOBAL)

**BPM** (40–240), **SWING**, **CLOCK** (INT, USB, TRS: follow MIDI clock and start/stop from that input), **TUNE**
(±50).

### The MENU (hold HOME)

**ALGORITHM** switches tabs, **PRESETS** moves between rows, **any knob** or **OCT+ / OCT−** changes the value. HOME
closes it (settings are saved then).

| Tab | Row | Values |
| --- | --- | --- |
| DISPLAY | COLOR | GREY, GREEN, AMBER, ICE, VIOLET, ROSE, PAPER, HI-CON, NIGHT, MONO, THERMAL, **GAMMA** (default), WRAITH |
| DISPLAY | STYLE | FLAT, LINE |
| DISPLAY | LARGE | OFF, ON (big labels and values) |
| DISPLAY | ANIM | ON, OFF (values snap; the power-on signature appears whole) |
| DISPLAY | LEDS | OFF, DIM LO, **DIM HI** (default), INV |
| DISPLAY | SCREEN OFF | **NEVER** (default), 5 MIN, 15 MIN, 30 MIN, 60 MIN |
| CONTROL | HOLD | 0.3 s, **0.4 s** (default), 0.5 s, 0.6 s — how long until a held button opens its layer |
| CONTROL | KNOB ACCEL | **ON** (default), OFF |
| CONTROL | FX LATCH | **OFF** (default since 0.6.64), ON |
| CONTROL | BPM LOCK | OFF, ON — the GENERATOR keeps the tempo (the tempo is GLO + SELECT, GLO's F4 TAP or GLOBAL's BPM) |
| CONTROL | SCALE LEDS | OFF, ON — the keys of the scale glow, the root brighter |
| CONTROL | MIDI OUT | **KEYS**, CLOCK, ALL, OFF — what goes out over USB MIDI (see [14](#14-midi-and-usb)) |
| AUDIO | SPEAKER EQ | FLAT, LOWCUT, BASS+ (an EQ on the master for the small speaker; also reaches the outputs and USB) |
| AUDIO | USB LEVEL | MASTER (USB follows the MASTER knob), FIXED (USB at full level) |
| AUDIO | CLICK | OFF, REC (while a track is armed and playing), ON |
| AUDIO | CLICK LEVEL | LOW, MID, HIGH |
| AUDIO | COUNT-IN | OFF, 1 BAR, 2 BARS |
| AUDIO | TRACKS | **4** (default), 6 |
| SYSTEM | USB SERIAL | ON, OFF (OFF helps macOS 13–15 see the USB audio input; USB-MIDI and the editor still work) |
| SYSTEM | RESTORE LAST | **OFF** (default: no autosave, power-on at zero), ON (the last session back at power-on) |
| SYSTEM | LOAD | ECO, LITE, **FULL** (default) |
| SYSTEM | CALIBRATION | OCT+ opens HARDWARE CALIBRATION (teach each button and knob) |
| SYSTEM | INIT SYSTEM | OCT+ asks, then everything back to zero for a new song (as SAVE > TOOLS > NEW PROJECT; UNDO brings the music back) |
| SYSTEM | ABOUT | OCT+ opens: version, QR code, AUDIO LOAD (NOW, PEAK, LATE, SHED since power-on) and credits (scroll with PRESETS, OCT− back). OCT+ there opens the visualizer (see [17](#17-gw-1-additions)) |
| GEN | SELECT ON HOME | **GENERATOR** (default): SELECT alone on HOME opens the generator; TEMPO: it turns the tempo instead (the generator is still HOME held + SELECT, or OCT− + PLAY) |

If you hear clicks, open ABOUT and look at **LATE** (audio blocks that came too late) and **SHED** (voices the guard
faded): a photo of that screen helps when you report a problem.

---

## 14. MIDI and USB

- **USB** is class-compliant MIDI and audio: the master output reaches the computer as a USB audio input.
- **MIDI IN** over USB and the **TRS** jack. **ROUTE** decides which track each channel plays (see
  [13](#13-system-tracks-load-and-the-menu)). Incoming notes on an armed track are recorded like the keys.
- **MIDI OUT** (over USB; MENU > CONTROL > **MIDI OUT**, 0.6.72), each track on its channel (T1 = channel 1 … T6 =
  channel 6):
  - **KEYS** (the default): the keys send their notes, as before.
  - **CLOCK**: and the clock (24 a beat) with **START / STOP**, while the tempo is the FM-1's own (CLOCK INT): a drum
    machine or a DAW follows it. The clock runs while stopped too, so the follower knows the tempo before PLAY.
  - **ALL**: and every note the FM-1 plays: the sequencer, the arp, PLAYHEADS and the punch-in repeats, with the punch-ins'
    OCT− / OCT+ and RANDOM on them; each note gets its note-off, and STOP, a panic or a change of the setting end
    whatever is still on. Record the FM-1's music into a DAW as MIDI, or play other synths from it.
  - **OFF**: nothing.
- **PROGRAM CHANGE** (0.6.72): program *n* on the channel a track plays loads the PRESETS list's sound number *n* + 1
  (every engine's presets in their order, then the user presets) into that track; the selection stays where it was.
- **Sections and projects** (0.6.80): **channel 16** is the song's: its PROGRAM CHANGE never loads a sound (whatever
  ROUTE). Programs **0–3** jump to section **A–D** of the project (playing: on the next bar, as the song layer's keys;
  stopped: at once; an empty one is said so); programs **16–23** load project **1–8** with its section A (stopped only,
  as SAVE > PROJECT). A foot controller or a DAW can run a set.
- **Not possible**: MIDI OUT on the **TRS** jack (the FM-1's jack is an input only: no output line goes to it) and
  **Bluetooth MIDI** (the chip has a radio, but its Bluetooth software is JieLi's closed SDK, which Pocket GammaWave does not use).
- **Clock**: GLO > GLOBAL > CLOCK **USB** or **TRS** follows that input's clock and Start / Continue / Stop. With an
  external clock, TAP and GLO + PLAY restart are disabled.
- **Controllers** (on the track the channel plays): CC5 glide, CC7 level, CC10 pan, CC71 resonance and CC74 cutoff (the
  engine's own, where it has them), CC72 release, CC73 attack, CC75 decay, CC91 reverb send, CC93 chorus send,
  CC94 delay send. CC1 (mod wheel), CC11 (expression) and channel aftertouch are sources in the MOD matrix (MODW, EXPR,
  AT). Pitch bend, sustain (CC64), All Sound Off (CC120) and All Notes Off (CC123) work. A CC is recorded as automation
  like a knob move.
- USB MIDI name: the device still presents itself as "Felucca", so the web editor connects.

---

## 15. Web tools: emulator, installer, editor, backup

All three run in the browser and update themselves with every change to the firmware.

### Emulator — <https://fvalente.github.io/GammaWave-FM1/>

The same code as the FM-1, with sound, in the browser. Play it with the mouse or the computer keyboard: **Z–'** and
**Q–O** rows are the keys, **1** to **=** the buttons from FX to REC, **[ ]** OCT− / OCT+, **↑ ↓** SELECT, **← →**
ALGORITHM; **Shift-click** holds a button. Great for learning the layers without the device.

### Installer — <https://fvalente.github.io/GammaWave-FM1/webapp/installer/>

Chrome, Edge or Opera (it needs Web MIDI), the FM-1 connected straight to the computer.

- **Install** Pocket GammaWave (it checks the FM-1 first: official V15, JIANT or Pocket GammaWave only).
- **Return to official V15**: give it M-VAVE's unmodified **FM-1.fwsc** (checked locally in the browser). It first
  saves a complete backup (current music, saved music, sound banks and settings; not user samples). Use this before
  installing Pocket GammaWave if your FM-1 runs **another custom firmware** (Felucca, baudgirl…), and whenever you want to go back
  to stock. Official V15 cannot open Pocket GammaWave music; to restore the backup later, reinstall Pocket GammaWave and use the editor.

### Editor — <https://fvalente.github.io/GammaWave-FM1/webapp/editor/>

Every parameter from the computer, over USB. Tabs: Sound, 6-OP FM, Sequencer, Tracks, Library, Projects, Settings.

- **6-OP FM**: edit FM6 patches, import .syx, and **Send to the device's voice bank (B1–B32)**.
- **Projects**: the project slots, and the **Complete backup**: **Save backup** and **Restore backup**.
- **The GW-1 panel** (on the Sound tab): four demo songs, the editor's own 49-genre generator (the same genre list;
  its patterns are made in the browser, so they won't match the device's for the same seed), **4 tape lanes** (T1–T4:
  DUMP a track's pattern into a lane, layer, bounce, mute and clear them — note patterns, not recorded audio),
  **EXPORT .WAV** (the song and the tape lanes as a 16-bit 44.1 kHz stereo WAV), the Wraith bar and SEQ MUTATE.
  Without a device it plays on its own in the browser; with one connected, the macros, the Wraith bar and the
  generator's bass line are sent to the selected track.

### The complete backup

**Save backup** downloads one file with: the current music, **all 8 projects with their 4 sections each**, the
arrangements and scenes, the 32 user presets (with their FM6 patches) and the settings. **Restore backup** replaces
all of that on the device (keep USB connected). The **FM6 voice bank (B1–B32) is not included**: keep your .syx files.

Back up before updating, before returning to stock, and whenever you finish something you love.

---

## 16. Troubleshooting and FAQ

**T5 or T6 is silent.** TRACKS is set to 4 (the default): T5 and T6 are asleep. GLO > SYSTEM > TRACKS **6** (or
MENU > AUDIO > TRACKS). The screen tells you when you play into an asleep track.

**After clearing a sequence, notes still played on all tracks.** This was fixed in v0.6.28: a clear now ends the
punch-in lane's repeats (STUTTER, ARP), a DRUM track with no steps gets no ARP fill, and a section jump or quick chain
waiting for the bar is called off. If something still plays after a clear, it may be the **punch-in lane**: hold
**REC and press FX** to erase it (or **SEQ + REC** to clear everything).

**Two cores (0.6.71, test).** The second core renders some synth tracks; MENU > ABOUT says **CORES 2**, or
**CORES 1** and why (REC held at power-on, a bad read, no answer, a fault: then it stays on one core until switched
off). Hold **REC** while powering on to play on one core.

**Clicks, stutters, the load meter full.** Keep TRACKS on 4, try **LOAD LITE** or **ECO**, use fewer voices (MONO,
fewer FM6 tracks), lighter reverbs (CLOUD and SHIMMER are the heaviest). Check MENU > SYSTEM > ABOUT > AUDIO LOAD.

**SELECT doesn't change the tempo.** Since 0.6.67 SELECT alone scrolls the pages; the tempo is **GLO held + SELECT**
(or GLO's F4 TAP, or GLO > GLOBAL's BPM knob).

**The knobs control M1–M4 instead of the page.** The macros are latched (header [LFO] LATCH). Press LFO.

**A song won't start: "PATTERN A EMPTY" / "ADD A SONG ROW".** A row points to an empty section, or there are no rows.
Save music into that section, or change the row.

**It says STOP TO EDIT.** A song (arrangement) or quick chain is playing; stop it to edit steps, use tools or UNDO.

**The keys feel inverted / a button does the wrong thing.** Run HARDWARE CALIBRATION: hold OCT− and OCT+ while powering
on (or MENU > SYSTEM > CALIBRATION) and teach each button and knob. A piano key cancels.

**USB audio input doesn't appear on macOS.** MENU > SYSTEM > USB SERIAL **OFF**.

**SAVED (NO FLASH).** The flash chip wasn't detected; saves stay in RAM only. Power-cycle the device. If it persists,
back up what you can with the editor and reinstall.

**The FM-1 stays black after an interrupted update.** Check whether the computer sees it as **WL80UBOOT** (a USB device
4C4A:8057): that's the chip's boot mode and the installer can write it again (try another cable). To enter update mode
deliberately from a running device, hold **OCT− + OCT+** for 5 seconds. If that isn't enough,
[FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) reads and writes the flash with a Seeed XIAO RP2040.

**I want the factory firmware back.** Use the installer's **Return to official V15** section (it backs up first), or
M-VAVE's own updater.

**I came from baudgirl / Felucca. Can I install directly?** No: return to official V15 first with the installer, then
install Pocket GammaWave. Going straight from another custom firmware has bricked an FM-1.

**Where did my Felucca samples go?** Pocket GammaWave has no user samples; that flash now holds the songs.

---

## 17. GW-1 additions

What Pocket GammaWave adds to the firmware it is built on (Felucca, by way of JIANT FM). Everything else in this manual
applies as written.

### The `[OCT−]` shift deck

Hold **OCT−** on its own first, then press the second button. A plain OCT− tap still lowers the octave; when a
shortcut fires, the octave is put back. The deck works from HOME and the parameter pages — not inside the MENU or a
dialog, and not on pages where OCT− has its own job (on MUTATE it is FREEZE). Pressing a page button and OCT− at the
same moment is that page's own gesture, as before.

| Shortcut | What it does |
| --- | --- |
| **OCT− + ARP** | The next of the six **Hearth macros** on the selected track (below). |
| **OCT− + PLAY** (or **REC**) | Opens the **GENERATOR** (see [8](#8-dice-and-undo)). |
| **OCT− + ENV** (or **SCL**) | Jumps to **SEQ > MUTATE**. If SOUND and PATTERN were both OFF it sets SOUND every 2 bars, PATTERN every 4 bars and AMOUNT about 50 %; MODE stays as it was (ORBIT by default). |
| **OCT− + HOME** (or **SAVE**) | Opens the **visualizer** (below). |
| **OCT− + SEQ / EDIT / FX / LFO** | Dices the selected track's sequence / sound / effects / LFO, as SEQ / EDIT / FX / LFO + SELECT do: the **KEEP DICED …?** dialog, OCT+ keeps, OCT− cancels. |
| **OCT− + GLO** | **UNDO** (again: redo), as GLO + G4. |

The editing shortcuts say **STOP TO EDIT** while a song or quick chain plays, as their GLO-layer versions do.

### Hearth macros (OCT− + ARP)

Each press sets the track's DIST type and amount, its filter cutoff and resonance (on ANALOG, its own CUT and RES),
and the filter type; the screen says which one. UNDO takes a macro back.

| Macro | DIST TYPE | Filter TYPE | Also |
| --- | --- | --- | --- |
| 1/6 HEARTH GRAVITY | HEARTH | LP | low cutoff, medium resonance |
| 2/6 SINGULARITY | BLAST | LP | |
| 3/6 PHOTOSPHERE | PLASMA | LP | |
| 4/6 WRAITH SCREAM | FLUX | SCREAM | high cutoff and resonance |
| 5/6 FORMANT VOWEL | HEARTH | VOWEL | |
| 6/6 SOLAR ECLIPSE! | BLAST | SCREAM | raises the master DUCK and PUNCH |

### New filter and drive types

FILTER **TYPE** adds **VOWEL** and **SCREAM** (see [4](#4-engines)); DIST **TYPE** adds **HEARTH**, **BLAST**,
**PLASMA** and **FLUX** (see [9](#9-effects-and-master)). The GENERATOR adds six genres: GAMMAWAVE, WRAITHWAVE,
TRAP ATL, UK DRILL, DARK TRAP and RAGE TRAP (see [8](#8-dice-and-undo)).

### The GammaWave screens

- **HOME**: the filter curve over the live spectrum, the Hearth sun and moon, the T1–T6 strip (see [2](#2-the-screen)).
- **EDIT pages**: two live scope traces over the **Wraith ribbon**. Its six zones — **FLU · WHI · VOX · CHR · BEL ·
  SCR** — label the cutoff range, and the needle follows CUT (and the filter LFO). With the VOWEL or SCREAM filter, or
  on the VOICE engine, the ribbon shows the vowels **OO · OH · AH · EH · EE** instead.
- **SLICER**: drawn as four tape lanes. **DRUM**: the kit's eight pads with the A/B MORPH bar.
- **Palettes**: MENU > DISPLAY > COLOR adds **THERMAL**, **GAMMA** (the default) and **WRAITH**.
- **ABOUT** carries a QR code to gammawaveaudio.com.

### The visualizer

**OCT− + HOME** (or MENU > SYSTEM > ABOUT, then OCT+) opens **HYPERSPACE VIZ**: four animated scenes — 1/4
SUPERNOVA, 2/4 WRAITH 3D, 3/4 WARP TUNNEL, 4/4 MATRIX FFT — that pulse while the sequencer plays. **OCT+** (or
PRESETS / ALGORITHM) changes scene, **OCT−** leaves (to ABOUT; HOME goes back home).

---

If you resonate with my work, consider supporting me on Ko-fi (https://ko-fi.com/juanjiant) — in Argentina: Cafecito (https://cafecito.app/juanjiant).
