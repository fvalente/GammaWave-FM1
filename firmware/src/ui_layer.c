/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca quick layers: while a page button is held, the keys and KNOB 1..4
 * are its shortcuts and its map shows over the page. One table (LAYERS) drives the gesture, the keys, the knobs,
 * the LEDs and the overlay:
 *   FX   HOLD  the performance effects (perform.c) and the track mutes while held; KNOB 1..4 its macros
 *   GLO  SET   black keys 1..4 (F#3 G#3 A#3 C#4) T1..T4 MUTE (latched; lit = sounding), F3..B3 SOLO T1..T4 while
 *              held (HOLD cells, a corner triangle), C4 UNMUTE ALL, F4 TAP tempo; KNOB 1..4 T1..T4 LEVEL;
 *              black keys 5..8 (D#4 F#4 G#4 A#4) the DRUM group mutes KICK SNARE HAT PERC (eng_drum.c dx_mute; the
 *              row shows while a track plays DRUM; C4 unmutes them too);
 *              GLO + PLAY: from the top without stopping
 *   SCL  SET   any key: its note name is ROOT; KNOB 1..4 ROOT SCL CHRD VOIC (LY_SCL: the SCL page's first two,
 *              the CHORD page's two; QNT TRN stay on SCL); the LEDs show the root lit and the scale's notes breathing
 *   EDIT SET   (JIANT: Felucca's engine keys are gone, PRESETS picks engines and sounds) F3 INIT: the track's sound
 *              to its engine's first preset, a DRUM track's DRUM-X kit to the factory one; G3 RECALL: the track's
 *              sound (and a DRUM track's kit) as the section playing stored it, the safe state (project.c
 *              project_recall_sound). Each asks first (the dialog closes the layer); the steps stay. KNOB 1..4 the
 *              EDIT page's four (the engine's first); on a DRUM track black keys 1..8 mute each DRUM-X sound
 *              (eng_drum.c DXM_LANE, saved with the section; lit = sounding)
 *   SEQ  SET   (1.2, on the SEQ pages that show the pattern: STEP / the DRUM grid, PATTERN, CHANCE, AUTOMATION; elsewhere
 *              SEQ held opens SONG as before) TOOLS: the white keys from F3 the sequence tools (ui_tools.c: CLEAR,
 *              REVERSE, SHIFT < >, RANDOM, COOK, CLR AUTO (0.6.8: the track's automation); on a DRUM track BEAT and the lane's CLEAR REVERSE FILL RANDOM), black
 *              keys 1..8 the lane (DRUM); KNOB 1..4 the PATTERN page's LEN DIV SWING GATE. No undo (TONIC: the song
 *              layer's RECALL)
 *   REC  SET   (1.1.5, on every page; REC's tap still arms / disarms) the white keys from F3: CLEAR (the selected
 *              track's steps and automation, no dialog), CLICK (OFF REC ON, each press the next value); KNOB 1 the
 *              same (TONIC: COUNT-IN and CLICK LEVEL only in MENU > AUDIO). The settings
 *              are MENU > AUDIO's (ui.c rp_get / rp_put: one store), saved when the layer closes.
 *              Replaces the old REC held on SEQ (Discussion #91:
 *              the CLEAR Tn SEQUENCE? dialog and its "HOLD: CLEAR" hint)
 * The gesture: let go before HOLD (the menu: 0.3 .. 0.6 s) with nothing else touched: a tap, the button's page.
 * Held past HOLD alone: the map (a peek), letting go does nothing. A key, a knob or a button meanwhile: a combo,
 * the map at once, no tap. Keys pressed with the button down are the layer's (seq.c keyboard_block): silent, no
 * MIDI, never recorded; keys held before stay notes. Only one layer at a time: a second layer button is ignored.
 * PLAY and REC work in every layer (GLO + PLAY: RESTART); SAVE, HOME, SEQ and the other page buttons are swallowed. In SET
 * layers OCT± do not shift the octave (TONIC: OCT- no longer puts back what the layer changed: no undo). A HOLD key's effect lasts until the key is let go, the map with it. No layer in the menu, a dialog,
 * NAME or the UPDATE MODE countdown. After a tap, until the layer has been opened once: "HOLD [GLO] QUICK"
 * (the seen bits are kept with the settings, favorites.c spare byte).
 * TONIC: no lock (Felucca's double tap, Discussion #83, is gone): a layer is open while its button is held. */
#include "ui_tools.c"                                   /* SEQ TOOLS' actions */
/* the song layer's actions (project.c: song_main.c) */
static int section_jump(uint32_t s);
static void section_store(uint32_t s);
static void section_recall(void);
static void quick_chain(const uint8_t *s, uint32_t n);
static void song_rec_toggle(void);
static int song_select(uint32_t n);
static int project_used(uint32_t slot);
enum { LK_HOLD, LK_SET };
typedef struct {
    uint8_t btn, kind, fam;            /* the button, HOLD / SET, the family whose first page KNOB 1..4 edit */
    const char *head;                  /* (FAM_HOME: the layer's own knobs) the header over the map */
    khint_t foot[3];                   /* the footer's key hints (a third with no word: none) */
} layer_t;
static const layer_t LAYERS[LAYER_N] = {
    {0, 0, 0, "", {{0, 0}, {0, 0}, {0, 0}}},
    {B_FX, LK_HOLD, FAM_HOME, "[FX] HOLD", {{KC_KEYS, "EFFECTS"}, {KC_K14, "MACROS"}, {0, 0}}},   /* (LET GO: the header's HOLD) */
    {B_GLO, LK_SET, FAM_HOME, "[GLO] SET", {{KC_PLAY, "RESTART"}, {KC_GLO, "DONE"}, {0, 0}}},
    {B_SCL, LK_SET, FAM_SCL, "[SCL] SET", {{KC_KEYS, "ROOT"}, {KC_SCL, "DONE"}, {0, 0}}},
    {B_EDIT, LK_SET, FAM_EDIT, "[EDIT] SET", {{KC_KEYS, "INIT / RECALL"}, {KC_EDIT, "DONE"}, {0, 0}}},
    {B_SEQ, LK_SET, FAM_SEQ, "[SEQ] TOOLS", {{KC_REC, "CLEAR ALL"}, {KC_SEQ, "DONE"}, {0, 0}}},   /* (0.6.14: REC, all six
                                                                     * tracks cleared, shown: it was nowhere) */
    {B_REC, LK_SET, FAM_HOME, "[REC] SET", {{KC_KEYS, "RECORDING"}, {KC_REC, "DONE"}, {0, 0}}},
    {B_SAVE, LK_SET, FAM_HOME, "[SAVE] SONG", {{KC_KEYS, "PLAY"}, {KC_OCTUP, "STORE"}, {KC_OCTDN, "RECALL"}}},
    {B_LFO, LK_SET, FAM_HOME, "[LFO] MACRO", {{KC_K14, "M1-M4"}, {KC_SELECT, "DICE"}, {KC_KEYS, "DICE / CLEAR"}}},   /* JIANT (mod.c macro_v; SELECT, F3: macro_dice, G3 clears) */
};
static const uint8_t LY_KC[LAYER_N] = {0, KC_FX, KC_GLO, KC_SCL, KC_EDIT, KC_SEQ, KC_REC, KC_SAVE, KC_LFO};
/* SCL's knobs: the key and the sequence offset (cur_page() while the layer edits or draws them: page_over) */
static const page_t LY_SCL = {"SCL", FAM_SCL, SC_TRACK, GR_SCALE, {P_ROOT, P_SCALE, P_SOFS, P_POFS}};
/* SEQ TOOLS' knobs: the PATTERN page's (the length the tools work in) */
static const page_t LY_SEQ = {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}};
static const page_t *ly_page(uint32_t l) { return l == LAYER_SCL ? &LY_SCL : l == LAYER_SEQ ? &LY_SEQ : 0; }
#define LY_OPEN 2u                     /* ui.ly_t0: the map opened (no tap any more) */
#define LY_COMBO 4u                    /* .. by a combo */
#define LY_DEAD 8u                     /* .. pressed where there is no layer: does nothing */
enum { LY_INIT, LY_RECALL };          /* EDIT: the white keys F3 G3 */
#define layer_seen (favorites.factory[15][31])   /* bit l: layer l opened once (a byte no engine uses) */
static const khint_t FX_LATCH_FOOT[3] = {{KC_KEYS, "ON / OFF"}, {KC_K14, "MACROS"}, {KC_OCTDN, "ALL OFF"}};
static const khint_t MAC_LATCH_FOOT[3] = {{KC_K14, "M1-M4"}, {KC_SELECT, "DICE"}, {KC_LFO, "DONE"}};   /* (MACRO latched) */

static struct {
    uint8_t l, oct;                    /* the layer open; OCT- / OCT+ pressed in a SET layer (bits) */
    uint8_t cook;                      /* SEQ: COOK pressed since it opened */
    uint8_t qc[8], nqc;                /* SAVE: the sections tapped in this hold (two or more: a quick chain) */
    uint8_t song;                      /* SAVE: the song KNOB 1 picked (+ 1; 0 none): in when SAVE lets go */
    uint8_t rp_dirty;                  /* REC: a setting changed (settings_save when the layer lets go) */
    uint32_t solo;                     /* GLO: the keys held that solo */
    uint32_t tap[4];                   /* GLO TAP: the last taps (fm1_ms) */
    uint8_t ntap;
    uint8_t quiet;                     /* #39: a layer closed after it opened: KNOB 1..4 do nothing until quiet_t + */
    uint32_t quiet_t;                  /* LY_QUIET_MS (fm1_ms) */
    uint8_t used;                      /* (0.6.44) a key, a knob or OCT used in the layer since it was armed */
} lys;
#define LY_QUIET_MS 250u

/* (0.6.44) FX LATCH: FX held open and let go locks its layer open, as the macros latch: its keys, knobs and map stay
 * until FX is pressed again (or another layer is held over it, for as long as it is) */
static uint8_t fx_lock;
static int layer_allowed(void) { return !ui.menu && !ui.confirm && !ui.uboot && !name_on(); }
static uint32_t ly_bit(uint32_t l) { return 1u << panel.btn[LAYERS[l].btn]; }
/* SEQ's layer is there on the SEQ pages that show the pattern only (elsewhere SEQ held opens SONG: ui_input) */
static int tools_page(void)
{
    const page_t *pg = &PAGES[ui.page];
    return !ui.home && pg->fam == FAM_SEQ &&
           (pg->graph == GR_ROLL || pg->graph == GR_STEPS || pg->graph == GR_CHANCE || pg->graph == GR_COMP ||
            pg->graph == GR_MOTION);
}
static int ly_avail(uint32_t l) { return l != LAYER_SEQ || tools_page(); }   /* (REC: on every page) */
static uint32_t ly_down(uint32_t l) { return l && (fm1_in.buttons & ly_bit(l)) != 0u; }
static uint32_t layer_bits(void)
{
    uint32_t l, m = 0;
    for (l = LAYER_FX; l < LAYER_N; l++)
        m |= ly_avail(l) ? ly_bit(l) : 0u;
    return m;
}
static uint32_t layer_btn(void) { return LAYERS[ui.layer % LAYER_N].btn; }
static uint32_t layer_open(void);
static const char *layer_head(void)                     /* FX: LATCH, and the MIDI effects' tracks (A#4) */
{
    static const char *const H[3][3] = {{"[FX] HOLD", "[FX] HOLD SYN", "[FX] HOLD DRM"},
                                        {"[FX] LATCH", "[FX] LATCH SYN", "[FX] LATCH DRM"},
                                        {"[FX] LOCKED", "[FX] LOCK SYN", "[FX] LOCK DRM"}};
    if (ui.layer == LAYER_MACRO && !layer_open())       /* (JIANT 0.4) the MACRO layer latched */
        return "[LFO] LATCH";
    return ui.layer == LAYER_FX ? H[fx_lock && !ui.ly ? 2 : perf_latch_on ? 1 : 0][pfx_tgt % 3u] : LAYERS[ui.layer % LAYER_N].head;
}
static uint32_t layer_open(void)
{
    if (ui.ly && (ui.ly_t0 & LY_OPEN) && ly_down(ui.ly) && layer_allowed())
        return ui.ly;
    return fx_lock && !ui.ly && layer_allowed() ? LAYER_FX : 0u;
}
static int layer_set_open(void)                 /* (FX LATCH: FX is one too, OCT- turns all off) */
{
    return layer_open() && (LAYERS[layer_open()].kind == LK_SET || (layer_open() == LAYER_FX && perf_latch_on));
}
static uint32_t layer_held(void) { return ui.ly && ly_down(ui.ly) && layer_allowed(); }

/* each pass, before the keys: what the ISR (seq.c keyboard_block) gives to a layer. Armed: its button; none
 * armed: any layer button not already down (one pressed meanwhile is armed below; one held over from before
 * is ignored, its keys stay notes); armed but dead (pressed under a dialog, a menu or NAME that has closed
 * since): nothing, its keys stay notes and FX starts no effect with no map shown; FX's effects only with FX */
static void layer_masks(void)
{
    uint32_t m = !layer_allowed() || (ui.ly && (ui.ly_t0 & LY_DEAD)) ? 0u
               : ui.ly ? ly_bit(ui.ly) : layer_bits() & ~fm1_in.buttons;
    if (ui.confirm == CF_DICE_ALL && !ui.menu)          /* (TAKES v2) the GEN screen, every page: OCT+ held, the keys are
                                                         * the take layer's (gen_layer); GLO held, G4 its UNDO; silent */
        m |= 1u << panel.btn[B_OCTUP] | 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_GLO];   /* (NOW: OCT- held, the
                                                         * takes' keys) */
    kb_mask = m;
    perf_mask = m & ly_bit(LAYER_FX);
    fx_lock_on = (uint8_t)(fx_lock && !ui.ly && layer_allowed());
}

/* a layer button pressed (its edge) while none is armed: it is now, a dead one where no layer opens */
static void layer_arm(uint32_t pressed, uint32_t now)
{
    uint32_t l;
    if (ui.ly)
        return;
    if (fx_lock && (pressed & ly_bit(LAYER_FX))) {      /* (0.6.44) FX pressed again: the lock off, nothing else */
        fx_lock = 0;
        ui.force = 1;
        ui.ly = LAYER_FX;
        ui.ly_t0 = (now & ~15u) | 1u | LY_DEAD;
        ui_message("FX UNLOCKED");
        return;
    }
    if (mac_latch && (pressed & ly_bit(LAYER_MACRO))) { /* (JIANT 0.4) LFO pressed again: the latch off, nothing else */
        mac_latch = 0;
        ui.force = 1;
        ui.ly = LAYER_MACRO;
        ui.ly_t0 = (now & ~15u) | 1u | LY_DEAD;
        return;
    }
    for (l = LAYER_FX; l < LAYER_N; l++)
        if ((pressed & ly_bit(l)) && ly_avail(l)) {
            lys.used = 0;
            ui.ly = (uint8_t)l;
            ui.ly_t0 = (now & ~15u) | 1u | (layer_allowed() ? 0u : LY_DEAD);
            return;
        }
}

/* the armed layer lets go (its button up): FX's macros snap back (FX LATCH: they stay); one that
 * opened, or a combo: KNOB 1..4 quiet for a while (#39: the knob still turning is not the page's) */
static void layer_let_go(uint32_t quiet)
{
    if (ui.ly == LAYER_SAVE && lys.nqc >= 2u)           /* SAVE: two or more sections tapped: a quick chain */
        quick_chain(lys.qc, lys.nqc);
    if (ui.ly == LAYER_SAVE && lys.song && lys.song - 1u != song_cur)   /* SAVE: KNOB 1 picked another song */
        (void)song_select(lys.song - 1u);
    lys.nqc = 0;
    lys.song = 0;
    if (ui.ly == LAYER_FX && !perf_latch_on)
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    if (ui.ly == LAYER_FX && (ui.ly_t0 & LY_OPEN) && !(ui.ly_t0 & (LY_DEAD | LY_COMBO)) &&
        !lys.used && !kb_layer && layer_allowed()) {
        fx_lock = 1;                                    /* (0.6.44) held open, nothing used, let go: locked open;
                                                         * (0.6.64) whatever FX LATCH (the effects' own: momentary by
                                                         * default) */
        ui_message("FX LOCKED: [FX] TO UNLOCK");
        quiet = 0;
        ui.force = 1;
    }
    if (ui.ly == LAYER_MACRO && (ui.ly_t0 & LY_OPEN) && !(ui.ly_t0 & LY_DEAD)) {   /* (JIANT 0.4) MACRO opened, let go: */
        mac_latch = 1;                                  /* latched (its knobs at once: no quiet time) */
        quiet = 0;
        ui.force = 1;
    }
    if (lys.rp_dirty) {                                 /* REC: its settings kept, as MENU does when it closes */
        lys.rp_dirty = 0;
        settings_save();                                /* (deferred while playing) */
    }
    if (quiet) {
        lys.quiet = 1;
        lys.quiet_t = fm1_ms;
    }
    ui.ly_t0 = 0;
    ui.ly = 0;
}

/* the layer opened: once seen, no more hint */
static void layer_opened(uint32_t l)
{
    if (!((layer_seen >> l) & 1u)) {
        layer_seen |= (uint8_t)(1u << l);
        settings_save();                                /* (deferred while playing) */
    }
    lys.l = (uint8_t)l;
    lys.cook = 0;
    lys.nqc = 0;
    lys.song = 0;
}

/* the armed button: 0, or the layer whose button was tapped (let go) */
static uint32_t layer_gesture(uint32_t now, uint32_t combo)
{
    uint32_t *t0 = &ui.ly_t0, l = ui.ly;
    if (!l)
        return 0;
    if (!layer_allowed())
        *t0 |= LY_DEAD;
    if (ly_down(l)) {
        if (!(*t0 & (LY_OPEN | LY_DEAD)) &&
            (combo || now - (*t0 & ~15u) >= (uint32_t)HOLD_MS[settings_hold % 4u] * 1000u * FM1_TICKS_PER_US)) {
            *t0 |= LY_OPEN | (combo ? LY_COMBO : 0u);
            layer_opened(l);
        }
        return 0;
    }
    l = *t0 & (LY_OPEN | LY_DEAD) || combo ? 0u : l;   /* (a knob turned as it was let go: a combo, no tap) */
    layer_let_go(*t0 & LY_OPEN || combo);
    return l;
}

/* #39: KNOB 1..4 belong to no page while a layer lets go: the frame its button is let go (the turns read then
 * were made with it held), while its map still shows (its keys held after it), and LY_QUIET_MS after it closed.
 * Their turns are dropped there (ui_input); before, they edited the page under the layer (an ARP page: ARP on) */
static int layer_knobs_quiet(void)
{
    if (lys.quiet && fm1_ms - lys.quiet_t >= LY_QUIET_MS)
        lys.quiet = 0;
    return layer_allowed() && (ui.ly || ui.layer || lys.quiet);
}

/* the map shows while the button is held open, and after it while its keys are still held */
static void layer_show(void)
{
    uint32_t show = layer_allowed() ? (layer_open() ? layer_open() : kb_layer ? ui.layer : 0u) : 0u;
    if (show != ui.layer) {
        ui.layer = (uint8_t)show;
        ui.force = 1;
    }
}

/* a tap: the button's page; the hint until its layer has been opened once (unless the tap said something, or
 * opened NAME: EDIT on USER / PROJECT renames; never over a dialog or the menu) */
static void layer_tap(uint32_t l)
{
    uint8_t m = ui.msg_t;
    if (l == LAYER_REC)                                 /* REC: arms / disarms (no page) */
        (void)rec_tap();
    else if (l == LAYER_SAVE)                           /* SAVE: its pages (presets, user sounds, projects) */
        open_family(FAM_SAVE);
    else
        (void)page_tap(LAYERS[l].btn);
    if (!((layer_seen >> l) & 1u) && ui.msg_t == m && !name_on() && !ui.confirm && !ui.menu) {
        ui_say("HOLD [", KC[LY_KC[l]].label);
        str_cpy(ui.msg + str_len(ui.msg), "] QUICK", sizeof ui.msg - str_len(ui.msg));
        ui.msg_t = 90;                                  /* ~1.5 s */
    }
}

static uint32_t snd_id(void) { return TSEL->eng_req | (uint32_t)TSEL->preset << 8 | (uint32_t)TSEL->user << 16; }
/* EDIT RECALL: the section playing has a stored sound to come back to */
static int edit_recallable(void) { return project_recall_slot() >= 0; }

/* GLO TAP: from the third tap the tempo of the taps (the last 4); 2 s without one starts over. INT clock only */
static void glo_tap(void)
{
    uint32_t n = lys.ntap, i;
    if (song.g[G_CLOCK]) {
        ui_message("TAP: CLK IS EXT");
        return;
    }
    if (n && fm1_ms - lys.tap[n - 1u] > 2000u)
        n = 0;
    if (n == 4u) {
        for (i = 0; i < 3u; i++)
            lys.tap[i] = lys.tap[i + 1u];
        n = 3;
    }
    lys.tap[n++] = fm1_ms;
    lys.ntap = (uint8_t)n;
    if (n >= 3u && lys.tap[n - 1u] != lys.tap[0]) {
        song.g[G_BPM] = (int16_t)clamp((int32_t)(60000u * (n - 1u) / (lys.tap[n - 1u] - lys.tap[0])),
                                       GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
}

/* SEQ TOOLS: white key place p's action on the selected track (ui_tools.c), "STOP TO EDIT" while a song plays */
static void tools_key(uint32_t p)
{
    const char *m;
    char n[4];
    if (!tl_cell(TSEL, p))
        return;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (!(m = tl_do(TSEL, p, ui.lane)))
        return;
    if (p == TL_COOK && lys.cook < 99u) {               /* "COOKED 3": the third since the layer opened */
        lys.cook++;
        fmt_int(n, lys.cook);
        ui_say("COOKED ", n);
    } else {
        ui_message(m);
    }
    ui.force = 1;
}

/* REC: CLEAR, the selected track's steps and automation (and ARP's latched chord), no dialog, as SEQ TOOLS' CLEAR
 * (up to 1.1.4 REC held on SEQ asked first, CF_CLEAR_SEQ) */
static void rec_clear(void)
{
    track_t *t = TSEL;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (seq_is_empty(t) && !motion_count(t)) {
        ui_message("NOTHING TO CLEAR");
        return;
    }
    undo_mark();                                        /* (0.6.21) UNDO: GLO + G4 */
    load_begin(t, LOAD_PAT);
    track_defaults_steps(t);
    load_end(t);
    latch_drop_req |= (uint8_t)(1u << (t - trk));       /* (0.6.19) the latched chord too (events_block) */
    {   /* (0.6.28) which track: the others play on */
        char cb[16] = "T1 CLEARED";
        cb[1] = (char)('1' + (t - trk));
        ui_say(cb, "");
        str_cpy(ui.msg2, "SEQ+REC: ALL TRACKS", sizeof ui.msg2);
    }
    ui.force = 1;
}
/* REC's settings, row r (G3, KNOB 1): CLICK as MENU > AUDIO steps it (ui.c rp_*). TONIC: COUNT-IN and CLICK LEVEL
 * left the layer (MENU > AUDIO still has them) */
#define RL_N 1u
static const uint8_t RL_F[RL_N] = {RP_CLICK};
static const char *const RL_LABEL[RL_N] = {"CLICK"};
static const uint8_t RL_ICON[RL_N] = {ICON_TEMPO};
static const char *rl_name(uint32_t r, uint32_t v) { (void)r; return CLICK_N[v % 3u]; }
static void rec_set(uint32_t r, uint32_t v)
{
    if (v == rp_get(RL_F[r]))
        return;
    rp_put(RL_F[r], v);
    lys.rp_dirty = 1;
}
static void rec_key(uint32_t p)
{
    uint32_t r = p - 1u, v;
    if (p == 0u) {
        rec_clear();
        return;
    }
    v = (rp_get(RL_F[r]) + 1u) % 3u;                    /* the next value, round */
    rec_set(r, v);
    ui_say("CLICK ", rl_name(r, v));
}

/* SAVE, the song layer (song_chain.c): white key p. 1..4 (F3..B3) play section A..D (on the next bar; tapped in a row
 * in one hold: a quick chain, layer_let_go), 5..8 (C4..F4) store the music into A..D, 13 (D5) LOOP / SONG (what PLAY
 * plays), 14 (E5) SONG REC, 16 (G5) the SONG page */
#define SK_PLAY 0u                                       /* the song layer's keys (white key places) */
#define SK_STORE 4u
#define SK_MODE 12u
#define SK_REC 13u
#define SK_PAGE 15u
static void song_key(uint32_t p)
{
    uint32_t k;
    if (p < SK_STORE) {
        if (lys.nqc < sizeof lys.qc)
            lys.qc[lys.nqc++] = (uint8_t)p;
        if (lys.nqc == 1u)
            (void)section_jump(p);                       /* (the first: at once, as a single tap) */
        else {
            char b[24] = "CHAIN";
            for (k = 0; k < lys.nqc && k < 8u; k++) {
                char c[3] = {' ', (char)('A' + lys.qc[k]), 0};
                str_cpy(b + str_len(b), c, sizeof b - str_len(b));
            }
            ui_message(b);
        }
    } else if (p < SK_STORE + 4u) {
        section_store(p - SK_STORE);
    } else if (p == SK_MODE) {
        live.mode ^= 1u;
        ui_message(live.mode ? "PLAY: THE SONG" : "PLAY: THE LOOP");
    } else if (p == SK_REC) {
        song_rec_toggle();
    } else if (p == SK_PAGE) {
        for (k = 0; k < NPAGES && PAGES[k].graph != GR_SONG; k++)
            ;
        if (k < NPAGES) {
            ui.home = 0;
            ui.page = (uint8_t)k;
            page_entered();
        }
    }
}

/* a key pressed in layer l (k: 0 = F3 .. 26 = G5) */
/* (JIANT 0.4) MACRO DICE: random routes from M1..M4 to what each track's sound has (its engine's parameters, CUT,
 * SHP, the sends, a little PITCH), two a track into its free MOD slots, the macros spread so each moves two tracks:
 * the instrument invites a turn from the start, the routes there to tune (MOD) or keep (saved with the project).
 * mode 0 (each power-on, project.c autosave_boot then main.c): only a track with no macro route yet, so a session
 * restored keeps its own; 1 (MACRO layer F3): the macros' routes rolled again; 2 (G3): the macros' routes cleared */
/* (JIANT 0.5) a track that sounds now: not muted, and a voice of it ringing, or (playing) a step of its pattern on */
static int track_sounding(const track_t *t)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    if (t->p[P_MUTE])
        return 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].stage && t->v[i].stage < 4u)
            return 1;
    if (song.playing)
        for (i = 0; i < len && i < NSTEP; i++)
            if (step_on(&seq_steps(t)[i]))
                return 1;
    return 0;
}
static void macro_dice(uint32_t mode)
{
    static const uint8_t DST[] = {MD_CUT, MD_SHP, MD_E1, MD_E1 + 1, MD_E1 + 2, MD_E1 + 3, MD_E1 + 4, MD_E1 + 5, MD_E1 + 6,
                                  MD_E1 + 7, MD_E1, MD_E1 + 1, MD_E1 + 2, MD_E1 + 3, MD_DLY, MD_REV, MD_PITCH, MD_DIST};   /* (the
                                                         * engine's, its first four twice as likely) */
    uint32_t k, j, n;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t has = 0, used = 0;
        for (j = 0; j < 4u; j++) {
            int16_t *m = &t->p[P_M1SRC + 3u * j];
            if (m[0] >= MS_M1 && m[0] < MS_M1 + 4) {
                if (mode) m[0] = m[1] = m[2] = 0;
                else has = 1;
            }
            if (m[0]) used |= 1u << m[1];
        }
        if (has || mode == 2u)
            continue;
        if (mode == 1u && !track_sounding(t))           /* (JIANT 0.5) a reroll: only the tracks that sound now */
            continue;
        if (mode == 3u && t->p[P_MUTE])                 /* (0.6.21) DICE ALL: every track not muted */
            continue;
        for (j = 0, n = 0; j < 4u && n < 2u; j++) {
            int16_t *m = &t->p[P_M1SRC + 3u * j];
            uint32_t d, tries;
            if (m[0])
                continue;
            for (tries = 0; tries < 16u; tries++) {     /* a destination the sound has, not used on this track */
                const param_desc_t *pd;
                d = DST[rng() % sizeof DST];
                if ((used >> d) & 1u)
                    continue;
                if (d >= MD_E1 && d < MD_E1 + 8u &&
                    ((pd = track_desc(t, P_E0 + (d - MD_E1))) == 0 || pd->max <= pd->min || pd->label[0] == '-'))
                    continue;
                break;
            }
            if (tries == 16u)
                continue;
            used |= 1u << d;
            m[0] = (int16_t)(MS_M1 + (k + 2u * n) % 4u);
            m[1] = (int16_t)d;
            m[2] = (int16_t)((d == MD_PITCH ? 4 + (int32_t)(rng() % 9u) : 24 + (int32_t)(rng() % 40u)) * (rng() & 1u ? 1 : -1));
            n++;
        }
    }
}

/* (JIANT 0.4) the session's macros (main.c after autosave_boot; the emulator too): the random numbers seeded by
 * something of this power-on, then the routes the tracks have none of */
static void session_dice(uint32_t seed)
{
    rng_state ^= seed * 2654435761u;
    if (!rng_state)
        rng_state = 0x1234567u;
    macro_dice(0);
}

/* GLO's white keys after the SOLOs (F3 ..): unmute all, tap tempo; KNOB 1..4: the levels from track trk_base4() */
#define GLO_ALL ((uint32_t)NTRK)
#define GLO_TAP 7u
#define GLO_UNDO 8u                                     /* (0.6.21) G4: UNDO the last big change (project.c undo_do) */
#define GLO_FRZ 9u                                      /* (0.6.43) A4: MUTATE frozen / free (project.c mutate_poll) */
static void layer_key(uint32_t l, uint32_t k)
{
    uint32_t p = key_place(k), i;
    if (l == LAYER_GLO) {
        if (key_black(k)) {
            if (p < NTRK)
                trk[p].p[P_MUTE] = (int16_t)!trk[p].p[P_MUTE];
            else if (p < NTRK + 4u)
                dx_mute_set(dx_mute ^ (1u << (p - NTRK)));
        } else if (p < NTRK) {
            lys.solo |= 1u << k;                        /* (layer_masks: perf_solo while held) */
        } else if (p == GLO_ALL) {
            for (i = 0; i < NTRK; i++)
                trk[i].p[P_MUTE] = 0;
            dx_mute_set(0);
        } else if (p == GLO_TAP) {
            glo_tap();
        } else if (p == GLO_FRZ) {
            mut_freeze = (uint8_t)!mut_freeze;
            ui_message(mut_freeze ? "MUTATE FROZEN" : "MUTATE FREE");
        } else if (p == GLO_UNDO) {
            if (chain_busy())
                ui_message("STOP TO EDIT");
            else if (undo_do())
                ui_message("NOTHING TO UNDO");
            else
                ui_message("UNDONE (AGAIN: REDO)");
        }
    } else if (l == LAYER_SCL) {
        TSEL->p[P_ROOT] = (int16_t)((k + 5u) % 12u);    /* the key's note name (F3 = F) */
    } else if (l == LAYER_MACRO && !key_black(k) && p < 2u) {   /* (JIANT 0.4) F3 DICE, G3 CLEAR */
        if (chain_busy()) {
            ui_message("STOP TO EDIT");
        } else {
            macro_dice(p ? 2u : 1u);
            ui_message(p ? "MACROS CLEARED" : "MACROS DICED");
        }
    } else if (l == LAYER_EDIT && key_black(k) && p < NLANE && drum_track(TSEL)) {
        dx_mute_set(dx_mute ^ DXM_LANE(p));             /* DRUM: black keys 1..8 mute each sound (lit = sounding) */
    } else if (l == LAYER_EDIT && !key_black(k) && p <= LY_RECALL) {
        if (chain_busy())
            ui_message("STOP TO EDIT");
        else if (p == LY_RECALL && !edit_recallable())
            ui_message("NOTHING SAVED");
        else
            confirm_open(p == LY_INIT ? CF_INIT_SOUND : CF_RECALL_SOUND, song.sel);   /* (the dialog closes the layer) */
    } else if (l == LAYER_REC) {
        if (!key_black(k) && p < 1u + RL_N)
            rec_key(p);
    } else if (l == LAYER_SEQ) {
        if (!key_black(k))
            tools_key(p);
        else if (p < NLANE && drum_track(TSEL))
            ui.lane = (uint8_t)p;                       /* (black keys 1..8: the lane, as on the grid) */
    } else if (l == LAYER_SAVE && !key_black(k)) {
        song_key(p);
    }
}
/* each pass: the keys the layer got now; then GLO's solo: the tracks of its keys still held (perform.c) */
static void layer_keys(uint32_t keys)
{
    uint32_t k, l = layer_open() ? layer_open() : ui.layer, s = 0;
    if (keys) lys.used = 1;
    for (k = 0; keys && k < 27u; k++)
        if ((keys >> k) & 1u)
            layer_key(l, k);
    lys.solo &= kb_layer;
    for (k = 0; k < 27u; k++)
        if ((lys.solo >> k) & 1u)
            s |= 1u << key_place(k);
    perf_solo = (uint8_t)s;
}

/* KNOB 1..4 in the open layer: its own (FX macros, GLO levels, EDIT's sound), else the four of its page */
static void layer_knob(uint32_t k, int32_t s)
{
    uint32_t l = layer_open();
    lys.used = 1;
    if (l == LAYER_FX) {                                /* perform.c perf_k, not recorded */
        if (k == 3u)
            s = -s;                                     /* DEPTH (100 - perf_k[3]) rises to the right (#40: it fell) */
        perf_k[k] = (int8_t)clamp(perf_k[k] + accel(EN_K1 + k, s, k ? 100 : 200), k ? 0 : -100, 100);   /* (#126) */
    } else if (l == LAYER_GLO) {                        /* T1..T4 LEVEL (T5, T6 selected: T3..T6), recorded as on MIXER */
        track_t *t = &trk[k + trk_base4()];
        int16_t *vp = &t->p[P_LEVEL];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, TP[P_LEVEL].max - TP[P_LEVEL].min), TP[P_LEVEL].min,
                             TP[P_LEVEL].max);
        motion_capture(t, P_LEVEL, *vp);
    } else if (l == LAYER_SAVE) {                       /* KNOB 1: the song (in when SAVE lets go); 2..4: none */
        if (k == 0u)
            lys.song = (uint8_t)(clamp((int32_t)(lys.song ? lys.song - 1u : song_cur) + (s > 0 ? 1 : -1), 0,
                                       NSONG - 1) + 1);
    } else if (l == LAYER_MACRO) {                      /* M1..M4 (the project's; the MOD slots that name them) */
        macro_v[k] = (uint8_t)clamp(macro_v[k] + accel(EN_K1 + k, s, 127), 0, 127);
    } else if (l == LAYER_REC) {                        /* CLICK (KNOB 2..4: none) */
        if (k < RL_N)
            rec_set(k, (uint32_t)clamp((int32_t)rp_get(RL_F[k]) + s, 0, 2));
    } else if (l) {                                     /* the page's knobs, wherever the page is */
        uint8_t h = ui.home, pg = ui.page;
        ui.home = 0;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        edit_param(k, s);
        page_over = 0;
        ui.home = h;
        ui.page = pg;
    }
}

/* PLAY with GLO held open: RESTART (from the top, playing on); stopped: PLAY. 1 = done here */
static int layer_play(void)
{
    if (layer_open() != LAYER_GLO || chain_busy())
        return 0;
    if (song.g[G_CLOCK] && song.playing) {
        ui_message("RESTART: CLK IS EXT");
        return 1;
    }
    transport_req = song.playing ? 3u : 1u;
    return 1;
}

/* OCT- / OCT+ pressed with a SET layer's button down (FX LATCH: FX's) before it opened: a combo, so it opens now,
 * before oct_taps and layer_oct read it (else the press was nobody's: no put back / ALL OFF, no octave) */
static void layer_oct_open(uint32_t pressed)
{
    uint32_t l = ui.ly, oct = 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_OCTUP];
    if ((pressed & oct) && l && !(ui.ly_t0 & (LY_OPEN | LY_DEAD)) && layer_held() &&
        (LAYERS[l].kind == LK_SET || (l == LAYER_FX && perf_latch_on))) {
        ui.ly_t0 |= LY_OPEN | LY_COMBO;
        layer_opened(l);
    }
}

/* OCT- / OCT+ in a SET layer: no octave (TONIC: no put back; FX LATCH: OCT- all off). Returns the taps left */
static uint32_t layer_oct(uint32_t pressed, uint32_t oct)
{
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP], mine;
    if (layer_set_open())
        lys.oct |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (ui.ly && (pressed & (1u << dn | 1u << up)))
        lys.used = 1;                                   /* (0.6.44: an OCT with it: no FX lock) */
    mine = oct & lys.oct;
    if ((mine & 1u) && lys.l == LAYER_FX) {             /* OCT- (FX LATCH): every effect and macro off */
        perf_latched = 0;
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
        ui_message("FX ALL OFF");
    }
    if (lys.l == LAYER_SAVE && (mine & 2u)) {           /* SAVE + OCT+: STORE the music into the section playing */
        if (live.cur < 0)
            ui_message("PLAY A SECTION FIRST");
        else
            section_store((uint32_t)live.cur);
    }
    if (lys.l == LAYER_SAVE && (mine & 1u))             /* SAVE + OCT-: RECALL it as it was stored */
        section_recall();
    lys.oct &= (uint8_t)(((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1);
    return oct & ~mine;
}

/* --------------------------------------------------------- the LEDs --- */
/* lit = in effect now (held, the value, a track sounding), breathing = can be pressed (*br: dark .. ~60 % and
 * back, ~1.1 s, hal/fm1_input.h fm1_led_breath; was a hard 250 ms blink, #119), dark = nothing there */
static int glo_sounding(uint32_t t) { return !trk[t].p[P_MUTE] && (!perf_solo || ((perf_solo >> t) & 1u)); }
static int glo_drums(void)                              /* a track plays DRUM: GLO shows its group mutes */
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        if (drum_track(&trk[i]))
            return 1;
    return 0;
}
static int rec_clearable(void) { return !seq_is_empty(TSEL) || motion_count(TSEL) != 0u; }
static uint32_t layer_leds(uint32_t *br)
{
    uint32_t k, m = 0, n = 0, l = ui.layer, held = perf_held | perf_latched, ok = perf_avail();
    uint32_t mask = scale_mask(TSEL), root = (uint32_t)TSEL->p[P_ROOT] % 12u;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), b = (uint32_t)key_black(k), on = 0, can = 0, e;
        if (l == LAYER_FX) {                            /* effects breathe, held lit, a too-long REPEAT dark */
            e = perf_key(k);
            can = PF_IS(e) && ((ok >> e) & 1u);
            on = can && ((held >> e) & 1u);
        } else if (l == LAYER_GLO) {                    /* sounding lit; SOLO held lit, the others, C4, F4 breathe */
            on = b ? (p < NTRK && glo_sounding(p)) || (p >= NTRK && p < NTRK + 4u && !((dx_mute >> (p - NTRK)) & 1u))
                   : p < NTRK && ((lys.solo >> k) & 1u);      /* (a group sounding: lit) */
            can = !b && ((p < NTRK && p < trk_live) || p == GLO_ALL || (p == GLO_TAP && !song.g[G_CLOCK]) ||
                         (p == GLO_UNDO && undo_ok) || p == GLO_FRZ);   /* (the keys that do something breathe: not an
                                                         * asleep track's) */
            on |= !b && p == GLO_FRZ && mut_freeze;      /* (0.6.43: FREEZE on: lit) */
        } else if (l == LAYER_SCL) {                    /* the root lit, the scale's notes breathe */
            e = (k + 5u + 12u - root) % 12u;
            on = e == 0u;
            can = (mask >> e) & 1u;
        } else if (l == LAYER_EDIT && !b) {             /* INIT and RECALL breathe (RECALL: a stored section) */
            can = !chain_busy() && (p == LY_INIT || (p == LY_RECALL && edit_recallable()));
        } else if (l == LAYER_EDIT && p < NLANE && drum_track(TSEL)) {   /* DRUM: a sound sounding lit, muted dark */
            on = !dx_lane_muted(p);
        } else if (l == LAYER_SEQ) {                    /* the tools breathe (not while a song plays); DRUM: the lane */
            on = b && p < NLANE && drum_track(TSEL) && p == ui.lane;   /* lit, the other lanes breathe */
            can = b ? p < NLANE && drum_track(TSEL) : tl_cell(TSEL, p) && !chain_busy() &&
                  (p != TL_CAUTO || motion_count(TSEL));   /* (CLR AUTO: automation to clear) */
        } else if (l == LAYER_SAVE && !b) {             /* the section playing lit, the saved ones and the */
            on = (p < SK_STORE && live.cur == (int8_t)p) || (p == SK_REC && live.srec) ||   /* actions breathe */
                 (p == SK_MODE && live.mode);
            can = p < SK_STORE ? project_used(p) : p < SK_STORE + 4u || p == SK_MODE || p == SK_REC || p == SK_PAGE;
        } else if (l == LAYER_REC && !b && p < 1u + RL_N) {   /* the click on: lit, else it breathes; CLEAR: */
            on = p == 1u && rp_get(RP_CLICK) != 0u;    /* something to clear, not while a song plays */
            can = p ? 1u : !chain_busy() && rec_clearable();
        }
        m |= on << k;
        n |= (can & !on) << k;
    }
    if (br)
        *br = n;
    return m;
}

/* ------------------------------------------------------ the overlay --- */
/* One template: the header names the button and the kind ("[GLO] SET"); the cards are KNOB 1..4; the panel the
 * map of the keys as cells (the key's note name, a Fukiai icon, a name; FX's effects: the icon alone, 24 px, the
 * REPEATs' division in the other corner); the footer the keycaps. A cell: RAISE
 * (can be pressed), the selection's fill (the value now, SET), the accent (held, HOLD), DIM (cannot now: pressing
 * it says why), KEY (a muted track, as the MUTE badge); HOLD cells in a SET layer: a corner triangle */
static const char *const PF_NAME[PF_M1] = {"1/8", "1/16", "1/32", "LPF", "HPF",          /* the audio ones (perform.c) */
    "OCT-", "OCT+", "1/2", "DEC-", "DEC+", "ST8", "ST16", "ATK+", "ARP", "RND"};            /* the MIDI ones (pfx.c) */
static const char W_NOTE[16] = {'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};
static const char B_NOTE[11] = {'F', 'G', 'A', 'C', 'D', 'F', 'G', 'A', 'C', 'D', 'F'};   /* black keys 1..11: F# G# A# C# D# F# .. F# */
#define G6_X(c) (7 + 38 * (int32_t)(c))                 /* (JIANT 0.6) a cell per track: 36 px wide, 2 px apart */
#define G6_W 36
#define LC_X(c) (6 + 58 * (int32_t)(c))                 /* cell column c: 54 px wide, 4 px apart */
#define LC_W 54
#define LC_H 42                                          /* the big cells: rows at y 4 and 50 */
#define LF_X(c) (7 + 46 * (int32_t)(c))                 /* FX: the 10 effects, 5 a row (F3 .. C4, D4 .. A4), 42 px: x 7 .. 233 */
#define LF_W 42
#define LM_Y 96                                          /* the black keys' row (22 px) */
#define LFR_P 26                                         /* (QG-1) FX: the white keys' rows 26 px apart, 22 high; */
#define LFR_H 22
#define LFM_Y 82                                         /* .. the mutes' row, the black effects' row under it */
#define LFX_Y 103
#define LFB_H 17
#define LM_H 22
enum { LS_OFF, LS_SEL, LS_HELD, LS_WAIT, LS_DIM, LS_MUTE };

static int32_t lc_w = LC_W;                              /* the cells' width (FX's effects: LF_W) */
static uint16_t lc_fill(uint32_t st, uint16_t *ink)
{
    *ink = st == LS_DIM ? T_DIM : T_THEME;
    if (st == LS_MUTE) {
        *ink = T_INK;
        return T_KEY;
    }
    if (st == LS_HELD) {
        *ink = T_BG;
        return T_ACCENT;
    }
    if (st == LS_SEL || st == LS_WAIT) {
        *ink = T_INK;
        return T_THEME;
    }
    return T_RAISE;
}
/* a cell at x, y, h px high: big (h > 30) the icon over the name (FX's effects, LF_W wide: the icon alone, 24 px,
 * under the note; a name: in the top right corner); a name: compact, the icon at the right (none: no icon); else a
 * black key's: two icons at the right. tri: a HOLD cell in a SET layer */
/* a cell's box: its fill on the panel. LINE: a RAISE (idle) cell is not filled, and 1 px rules divide the
 * cells: one in the gap left of a cell (not the first column) and one in the gap above it (not the first row), each
 * across the gap's corner, so they meet in a grid. Cells are 4 px apart from x 6, y 4. Returns the fill */
static uint16_t lc_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t fill)
{
    if (ux.style) {
        if (fill == T_RAISE)
            fill = T_SURF;
        if (x > 7)                                       /* (not the first column: x 6, FX's 7) */
            cv_rule(x - 2, y > 4 ? y - 4 : y, 1, h + (y > 4 ? 4 : 0));
        if (y > 4)
            cv_rule(x > 7 ? x - 4 : x, y - 2, w + (x > 7 ? 4 : 0), 1);
    }
    if (ux.chamfer && !ux.style) {                       /* (JIANT 0.5) a pad: an idle cell its outline (violet, cut corners)
                                                         * on a faint tint; a held one hot (orange .. yellow) */
        if (fill == T_RAISE) {
            uint16_t ol = ux_mix(T_SURF, T_THEME, 48), in = ux_mix(T_SURF, T_THEME, 9);
            int32_t k;
            cv_rrect(x, y, w, h, 4, in, T_SURF);         /* (one box: the lint's cell) and its outline in lines */
            cv_rect(x + 3, y, w - 6, 1, ol);
            cv_rect(x + 3, y + h - 1, w - 6, 1, ol);
            cv_rect(x, y + 3, 1, h - 6, ol);
            cv_rect(x + w - 1, y + 3, 1, h - 6, ol);
            for (k = 0; k < 3; k++) {
                cv_pset(x + k, y + 3 - k, ol);
                cv_pset(x + w - 1 - k, y + 3 - k, ol);
                cv_pset(x + k, y + h - 4 + k, ol);
                cv_pset(x + w - 1 - k, y + h - 4 + k, ol);
            }
            return in;
        }
        if (fill == T_ACCENT)
            fill = heat_col(200);
    }
    cv_rrect(x, y, w, h, 4, fill, T_SURF);
    return fill;
}
static void lcell(int32_t x, int32_t y, int32_t h, const char *note, uint32_t icon, uint32_t icon2, const char *name,
                  uint32_t st, int tri)
{
    uint16_t ink, fill = lc_fill(st, &ink), idle = fill == T_RAISE;
    fill = lc_box(x, y, lc_w, h, fill);
    if (note)
        cv_text_on(x + 5, y + 3, &AF_S, note, idle ? T_MID : ink, fill);
    if (h > 30 && lc_w == LF_W) {
        GFX_HOOK_ALIGN(x, 0, x + lc_w, 0, AL_H, "FX cell icon centred across");
        cv_icon_on(x + (lc_w - 24) / 2, y + h - 25, 24, icon, ink, fill);   /* (its ink: rows 3 .. 20 of 24) */
        if (name)
            cv_text_r(x + lc_w - 5, y + 3, &AF_S, name, ink, fill);
    } else if (h > 30) {                                   /* the icon's cell over the name, 6 px between, centred */
        int32_t t = y + HALF_UP(h - (16 + 6 + AF_S_CAP_H));
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V | AL_N(2), "layer cell icon + name centred up/down");
        GFX_HOOK_ALIGN(x, 0, x + lc_w, 0, AL_H | AL_PASS, "layer cell icon / name centred across");
        cv_icon_in(x, t, lc_w, 0, 16, icon, ink, fill);
        GFX_HOOK_ALIGN(x, 0, x + lc_w, 0, AL_H | AL_PASS, "layer cell icon / name centred across");
        cv_text_in(x, t + 22 - AF_S_CAP_Y, lc_w, &AF_S, name, ink, fill);
    } else if (name && h > 24) {                           /* the same, 12 px, 2 px between (the icon at a side) */
        int32_t t = y + HALF_UP(h - (12 + 2 + AF_S_CAP_H));
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V | AL_N(2), "layer cell icon + name centred up/down");
        cv_icon_on(note ? x + lc_w - 17 : x + 5, t, 12, icon, ink, fill);
        GFX_HOOK_ALIGN(x, 0, x + lc_w, 0, AL_H | AL_PASS, "layer cell icon / name centred across");
        cv_text_in(x, t + 14 - AF_S_CAP_Y, lc_w, &AF_S, name, ink, fill);
    } else if (name) {                                     /* a key's row: the icon and the name on its middle */
        int32_t ib[4];                                     /* (QG-1 stage 3) by its ink, not by the capitals: a
                                                            * label with a "#" (G# C# .., the FX keys) reaches 2 px
                                                            * below them and sat low in its cell */
        text_ink(&AF_S, name, ib);
        int32_t ny = y + HALF_UP(h - (ib[3] - ib[1])) - ib[1];   /* (the ink box is [y0, y1): its height is the difference) */
        if (!note && icon < ICON_COUNT) {                  /* (the EDIT layer: the engine's icon instead of the key) */
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_icon_in(x + 3, y, 0, h, 12, icon, ink, fill);
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_text_r(x + lc_w - 3, ny, &AF_S, name, ink, fill);
        } else {
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_text_r(x + lc_w - 5, ny, &AF_S, name, ink, fill);
        }
    } else {
        /* (QG-1 stage 3) a hook declares the box for the item drawn next: one declared for an icon that is not
         * there was paired with whatever came after (a cell 23 px below), a false finding */
        if (icon != ICON_NONE) {                            /* (JIANT 0.6: a narrow cell, G6_W, the track's alone) */
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_icon_in(x + lc_w - 31, y, 0, h, 12, icon, ink, fill);
        }
        if (icon2 != ICON_NONE) {
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_icon_in(x + lc_w - (lc_w < LC_W ? 14 : 17), y, 0, h, 12, icon2, ink, fill);
        }
    }
    if (tri) {
        int32_t j;
        for (j = 0; j < 4; j++)
            cv_rect(x + lc_w - 7 + j, y + 2 + j, 4 - j, 1, ink);
    }
}
static void bnote(char *n, uint32_t t) { n[0] = B_NOTE[t]; n[1] = '#'; n[2] = 0; }

static void layer_fx(void)
{
    uint32_t held = perf_kill ? 0u : perf_held | perf_latched, act = perf_act, ok = perf_avail(), e;
    char n[3] = {0, 0, 0};
    lc_w = LF_W;
    for (e = 0; e < PF_M1; e++) {                       /* the effects of the white keys F3 .. F5, 5 a row */
        if (e % 5u == 0u && !ux.style)                  /* (LINE: its rules are cells too) */
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(5), "FX cells' row centred");
        uint32_t st = !((ok >> e) & 1u) ? LS_DIM : !((held >> e) & 1u) ? LS_OFF :
                      (act >> e) & 1u || ((PF_MIDI >> e) & 1u) ? LS_HELD : LS_WAIT;
        n[0] = W_NOTE[e];
        lcell(LF_X(e % 5u), 4 + LFR_P * (int32_t)(e / 5u), LFR_H, 0, ICON_COUNT, 0, PF_NAME[e], st, 0);   /* (its name alone: the
                                                         * cells stand where the keys do, 5 a row) */
    }
    lc_w = G6_W;
    for (e = 0; e < NTRK; e++) {                        /* the mutes of the black keys 1..6 */
        bnote(n, e);
        lcell(G6_X(e), LFM_Y, LFB_H, n, ICON_NONE, trk_icon(e, 0), 0, (held >> (PF_M1 + e)) & 1u ? LS_MUTE : LS_OFF, 0);
    }
    lc_w = LC_W;
    for (e = 0; e < 4u; e++) {                          /* (QG-1) G#4 VIB, C#5 GATE, D#5 SWEEP, F#5 DIVE */
        static const uint8_t K[4] = {PF_VIB, PF_GATE, PF_SWP, PF_DIVE}, B[4] = {NTRK, NTRK + 2, NTRK + 3, NTRK + 4};
        static const char *const N[4] = {"VIB", "GATE", "SWP", "DIVE"};
        uint32_t f = K[e];
        bnote(n, B[e]);
        lcell(LC_X(e), LFX_Y, LFB_H, n, ICON_NONE, 0, N[e], !((held >> f) & 1u) ? LS_OFF : LS_HELD, 0);
    }
}
/* a track playing DRUM: the rows shrink (28, 28, 24 px) and the group mutes take the third */
static const char *const GLO_GROUP[4] = {"KICK", "SNR", "HAT", "PERC"};
static void layer_glo(void)
{
    uint32_t e, dr = (uint32_t)glo_drums();
    int32_t h = dr ? 28 : LC_H, y2 = dr ? 36 : 50;
    char n[3] = {0, 0, 0};
    lc_w = G6_W;
    for (e = 0; e < NTRK; e++) {                        /* F3 .. D4: SOLO while held */
        n[0] = W_NOTE[e];
        lcell(G6_X(e), 4, h, n, trk_icon(e, 0), 0, "SOLO", (perf_solo >> e) & 1u ? LS_HELD : e >= trk_live ? LS_DIM : LS_OFF, 1);
    }
    lc_w = LC_W;
    n[0] = W_NOTE[GLO_ALL];
    lcell(LC_X(0), y2, h, n, ICON_MUTE, 0, "ALL", LS_OFF, 0);   /* (unmute all) */
    n[0] = W_NOTE[GLO_TAP];
    lcell(LC_X(3), y2, h, n, ICON_TEMPO, 0, "TAP", song.g[G_CLOCK] ? LS_DIM : LS_OFF, 0);
    n[0] = W_NOTE[GLO_UNDO];
    lcell(LC_X(2), y2, h, n, ICON_LOOP, 0, "UNDO", undo_ok ? LS_OFF : LS_DIM, 0);   /* (0.6.21) */
    n[0] = W_NOTE[GLO_FRZ];
    lcell(LC_X(1), y2, h, n, ICON_HOLD, 0, "FREEZE", mut_freeze ? LS_HELD : LS_OFF, 0);   /* (0.6.43) MUTATE */
    for (e = 0; dr && e < 4u; e++) {                    /* black keys 7..10: the DRUM groups, latched */
        bnote(n, NTRK + e);
        lcell(LC_X(e), 68, 24, n, 0, 0, GLO_GROUP[e], (dx_mute >> e) & 1u ? LS_MUTE : LS_OFF, 0);
    }
    lc_w = G6_W;
    for (e = 0; e < NTRK; e++) {                        /* the black keys 1..6: MUTE, latched */
        bnote(n, e);
        lcell(G6_X(e), LM_Y, LM_H, n, ICON_NONE, trk_icon(e, 0), 0, trk[e].p[P_MUTE] ? LS_MUTE : e >= trk_live ? LS_DIM : LS_OFF,
              0);                                       /* (0.6.25: a track asleep, TRACKS 4: dim) */
    }
    lc_w = LC_W;
}
static void layer_scl(void)                             /* KNOB 2's scales, 4 x 4, the one now selected */
{
    uint32_t i, sc = (uint32_t)TSEL->p[P_SCALE];
    for (i = 0; i < 16u && i <= (uint32_t)TP[P_SCALE].max; i++) {
        int32_t x = LC_X(i % 4u), y = 4 + 29 * (int32_t)(i / 4u);
        uint16_t ink, fill = lc_fill(i == sc ? LS_SEL : LS_OFF, &ink);
        fill = lc_box(x, y, LC_W, 25, fill);
        GFX_HOOK_ALIGN(x, y, x + LC_W, y + 25, AL_HV, "scale cell name centred");
        cv_text_in(x, y + CAP_IN(S, 25), LC_W, &AF_S, TP[P_SCALE].names[i], ink, fill);
    }
}
static void layer_edit(void)                            /* F3 INIT, G3 RECALL (a DRUM track: with the kit), the sound */
{
    uint32_t dr = (uint32_t)drum_track(TSEL), st = chain_busy() ? LS_DIM : LS_OFF;
    lcell(LC_X(0), 4, LC_H, "F", ICON_X_WARN, 0, dr ? "INIT+KIT" : "INIT", st, 0);
    lcell(LC_X(1), 4, LC_H, "G", ICON_LOOP, 0, dr ? "RCL+KIT" : "RECALL", edit_recallable() ? st : LS_DIM, 0);
    if (dr) {                                           /* DRUM: the 8 sounds of black keys 1..8, muted: the mute's fill */
        uint32_t l;
        lc_w = 26;
        for (l = 0; l < NLANE; l++)
            lcell(6 + 29 * (int32_t)l, 54, 24, 0, ICON_COUNT, 0, drum_lane_abbr(TSEL, l), dx_lane_muted(l) ? LS_MUTE : LS_OFF, 0);
        lc_w = LC_W;
    }
    engine_sound_row(104);
}

/* SEQ TOOLS: the white keys' tools, 4 a row from F3 (the whole sequence; RANDOM COOK BEAT; the lane's), on a DRUM
 * track the lanes of black keys 1..8 under them (the selected one: the selection's fill). A melodic track: the two
 * rows of the sequence's tools only */
static const uint8_t TL_ICON[TL_N] = {ICON_CLEAR, ICON_X_REVERSE, ICON_X_LEFT, ICON_X_RIGHT, ICON_PROB, ICON_W_SH,
    ICON_DRUM, ICON_X_MOTION_DEL, ICON_CLEAR, ICON_X_REVERSE, ICON_STEPS, ICON_PROB};
static const char *const TL_NAME[TL_N] = {"CLEAR", "REVERSE", "SHIFT", "SHIFT", "RANDOM", "COOK", "BEAT", "CLR AUTO",
    "CLR", "REV", "FILL", "RND"};
static void layer_seq(void)
{
    uint32_t p, st = chain_busy() ? LS_DIM : LS_OFF, drum = (uint32_t)drum_track(TSEL);
    char n[3] = {0, 0, 0}, nm[10];
    for (p = 0; p < TL_N; p++) {
        if (!tl_cell(TSEL, p))
            continue;
        n[0] = W_NOTE[p];                               /* (0.6.14) with its octave: F3 CLEAR and F4 CLR AUTO were */
        n[1] = (char)(p < 4u ? '3' : p < 11u ? '4' : '5');   /* both "F" (the white keys from F3: F G A B, C .. B, C) */
        str_cpy(nm, TL_NAME[p], sizeof nm);
        if (p >= TL_LCLR) {                             /* the lane's: "CLR BD" */
            str_cpy(nm + str_len(nm), " ", 2);
            str_cpy(nm + str_len(nm), drum_lane_abbr(TSEL, ui.lane), sizeof nm - str_len(nm));
        }
        lcell(LC_X(p % 4u), 4 + 32 * (int32_t)(p / 4u), 28, n, TL_ICON[p], 0, nm, st, 0);
    }
    for (p = 0; drum && p < NLANE; p++) {               /* the lanes (black keys 1..8): 8 cells of 25 px, 4 px apart */
        int32_t x = 6 + 29 * (int32_t)p;
        uint16_t ink, fill = lc_box(x, 100, 25, 18, lc_fill(p == ui.lane % NLANE ? LS_SEL : LS_OFF, &ink));
        GFX_HOOK_ALIGN(x, 100, x + 25, 118, AL_HV, "tools lane cell name centred");
        cv_text_in(x, 100 + CAP_IN(S, 18), 25, &AF_S, drum_lane_abbr(TSEL, p), ink, fill);
    }
}

/* REC: a row a key (26 px, 4 apart): the key's cell (its note, icon and name), then CLEAR's track and what it holds,
 * or the setting's three values, the one now with the selection's fill (as MENU > AUDIO steps them) */
#define LR_KW 78                                         /* the key's cell; the values 46 px each: x 6 .. 234 */
#define LR_VW 46
#define LR_H 26
/* MACRO: the MOD slots (any track's) whose SRC is macro m: their number; b (n bytes): "T1 CUT+40 T4 MRPH-20 .." */
#define LMC_TW 140                                       /* MACRO's routes: their width (x 92 .. 232) */
static uint32_t macro_routes(uint32_t m, char *b, uint32_t n)
{
    uint32_t i, k, c = 0;
    if (b)
        b[0] = 0;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < 4u; k++) {
            const int16_t *p = &trk[i].p[P_M1SRC + 3u * k];
            if (p[0] != (int16_t)(MS_M1 + m) || !p[1] || !p[2])
                continue;
            c++;
            if (b && str_len(b) + 12u < n) {           /* (as many as fit in LMC_TW; "+" when more) */
                char e[16] = {'T', (char)('1' + i), ' ', 0};
                uint32_t l0 = str_len(b);
                str_cpy(e + 3, mod_dst_name(&trk[i], p[1]), sizeof e - 3);
                str_cpy(e + str_len(e), p[2] > 0 ? "+" : "", sizeof e - str_len(e));
                fmt_int(e + str_len(e), p[2] * 100 / 64);
                if (l0)
                    str_cpy(b + l0, " ", n - l0);
                str_cpy(b + str_len(b), e, n - str_len(b));
                if (text_w(&AF_S, b) > LMC_TW) {
                    b[l0] = 0;
                    str_cpy(b + l0, " +", n - l0);
                    n = l0 + 3u;                        /* (nothing more goes in) */
                }
            }
        }
    return c;
}
static uint32_t macro_sig(void)
{
    uint32_t i, k, h = (uint32_t)macro_v[0] | (uint32_t)macro_v[1] << 8 | (uint32_t)macro_v[2] << 16 | (uint32_t)macro_v[3] << 24;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < 12u; k++)
            h = (h ^ (uint32_t)trk[i].p[P_M1SRC + k]) * 16777619u;
    return h + snd_id() * 31u;
}
/* MACRO's map: a row per macro, its value as a heat bar and where its MOD slots send it ("NO ROUTE": how to make one) */
#define LMC_H 26
static void layer_macro(void)
{
    uint32_t m;
    char n[4] = {'M', 0, 0, 0}, b[48];
    for (m = 0; m < 4u; m++) {
        int32_t y = 4 + (LMC_H + 3) * (int32_t)m, w = macro_v[m] * 44 / 127;
        uint32_t c = macro_routes(m, b, sizeof b);
        uint16_t ink, fill = lc_box(6, y, 30, LMC_H, lc_fill(c ? LS_OFF : LS_DIM, &ink));
        n[1] = (char)('1' + m);
        GFX_HOOK_ALIGN(6, y, 36, y + LMC_H, AL_HV, "macro row name centred");
        cv_text_in(6, y + CAP_IN(S, LMC_H), 30, &AF_S, n, ink, fill);
        cv_rrect(42, y + LMC_H / 2 - 2, 44, 5, 2, T_RAISE, T_SURF);
        if (w > 0)
            cv_rrect(42, y + LMC_H / 2 - 2, w < 4 ? 4 : w, 5, 2, heat_col(48 + macro_v[m] * 200 / 127), T_SURF);
        GFX_HOOK_ALIGN(0, y, 0, y + LMC_H, AL_V, "macro row routes on its middle");
        cv_text_on(92, y + CAP_IN(S, LMC_H), &AF_S, c ? b : "NO ROUTE: MOD SRC", c ? T_TEXT : T_DIM, T_SURF);
    }
}

static void layer_rec(void)
{
    uint32_t r, v;
    char n[3] = {0, 0, 0}, b[16];
    for (r = 0; r < 1u + RL_N; r++) {
        int32_t y = 4 + (LR_H + 4) * (int32_t)r, x = 6, vx = 6 + LR_KW + 4;
        uint32_t dim = r == 0u && (chain_busy() || !rec_clearable());
        uint16_t ink, fill = lc_box(x, y, LR_KW, LR_H, lc_fill(dim ? LS_DIM : LS_OFF, &ink));
        n[0] = W_NOTE[r];
        GFX_HOOK_ALIGN(0, y, 0, y + LR_H, AL_V, "rec layer key cell on its middle");
        cv_text_on(x + 5, y + CAP_IN(S, LR_H), &AF_S, n, fill == T_RAISE || fill == T_SURF ? T_MID : ink, fill);
        GFX_HOOK_ALIGN(0, y, 0, y + LR_H, AL_V, "rec layer key cell on its middle");
        cv_icon_in(x + 15, y, 0, LR_H, 12, r ? RL_ICON[r - 1u] : ICON_CLEAR, ink, fill);
        GFX_HOOK_ALIGN(0, y, 0, y + LR_H, AL_V, "rec layer key cell on its middle");
        cv_text_r(x + LR_KW - 5, y + CAP_IN(S, LR_H), &AF_S, r ? RL_LABEL[r - 1u] : "CLEAR", ink, fill);
        if (r == 0u) {                                   /* "T2 SEQUENCE" / "T2 EMPTY" (a song playing: why not) */
            int32_t w = 234 - vx;
            b[0] = 'T';
            b[1] = (char)('1' + song.sel);
            str_cpy(b + 2, rec_clearable() ? " SEQUENCE" : " EMPTY", sizeof b - 2);
            if (chain_busy())
                str_cpy(b, "STOP TO EDIT", sizeof b);
            fill = lc_box(vx, y, w, LR_H, lc_fill(dim ? LS_DIM : LS_OFF, &ink));
            GFX_HOOK_ALIGN(vx, y, vx + w, y + LR_H, AL_HV, "rec layer clear cell centred");
            cv_text_in(vx, y + CAP_IN(S, LR_H), w, &AF_S, b, ink, fill);
            continue;
        }
        for (v = 0; v < 3u; v++, vx += LR_VW + 4) {
            fill = lc_box(vx, y, LR_VW, LR_H, lc_fill(v == rp_get(RL_F[r - 1u]) ? LS_SEL : LS_OFF, &ink));
            GFX_HOOK_ALIGN(vx, y, vx + LR_VW, y + LR_H, AL_HV, "rec layer value cell centred");
            cv_text_in(vx, y + CAP_IN(S, LR_H), LR_VW, &AF_S, rl_name(r - 1u, v), ink, fill);
        }
    }
}

/* SAVE: the song layer. Row 1 the sections A..D (F3..B3: the one playing selected, the one asked for waiting, an
 * empty one dim), row 2 STORE A..D (C4..F4), row 3 D5 LOOP / SONG, E5 SONG REC, G5 SONG; at the bottom what goes on
 * (a quick chain being tapped, the song's row and its bars to go) */
static void layer_song(void)
{
    uint32_t s;
    char n[3] = {0, 0, 0}, nm[12], b[32];
    for (s = 0; s < 4u; s++) {
        uint32_t st = !project_used(s) ? LS_DIM : live.req == (int8_t)s ? LS_WAIT : live.cur == (int8_t)s ? LS_SEL
                    : LS_OFF;
        n[0] = W_NOTE[SK_PLAY + s];
        nm[0] = (char)('A' + s); nm[1] = 0;
        lcell(LC_X(s), 4, 28, n, ICON_X_PATTERN, 0, nm, st, 0);
        n[0] = W_NOTE[SK_STORE + s];
        str_cpy(nm, "STORE ", sizeof nm);
        nm[6] = (char)('A' + s); nm[7] = 0;
        lcell(LC_X(s), 36, 28, n, ICON_SAVE, 0, nm, (live.dirty >> s) & 1u ? LS_WAIT : LS_OFF, 0);
    }
    n[0] = W_NOTE[SK_MODE];
    lcell(LC_X(0), 68, 28, n, ICON_LOOP, 0, live.mode ? "SONG" : "LOOP", live.mode ? LS_SEL : LS_OFF, 0);
    n[0] = W_NOTE[SK_REC];
    lcell(LC_X(1), 68, 28, n, ICON_X_MOTION_REC, 0, "REC", live.srec == 2u ? LS_HELD : live.srec ? LS_WAIT : LS_OFF, 0);
    n[0] = W_NOTE[SK_PAGE];
    lcell(LC_X(3), 68, 28, n, ICON_X_SONG, 0, "SONG", LS_OFF, 0);
    b[0] = 0;
    if (lys.nqc >= 2u) {
        str_cpy(b, "CHAIN", sizeof b);
        for (s = 0; s < lys.nqc; s++) {
            char c[3] = {' ', (char)('A' + lys.qc[s]), 0};
            str_cpy(b + str_len(b), c, sizeof b - str_len(b));
        }
    } else if (chain.running) {
        str_cpy(b, chain.loop ? "QUICK CHAIN " : "SONG ROW ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)chain.row + 1);
        str_cpy(b + str_len(b), " - ", sizeof b - str_len(b));
        fmt_int(b + str_len(b), chain.remaining);
        str_cpy(b + str_len(b), " BARS", sizeof b - str_len(b));
    } else if (live.cur >= 0) {
        str_cpy(b, "SECTION A", sizeof b);
        b[8] = (char)('A' + live.cur);
    }
    if (b[0]) {
        GFX_HOOK_ALIGN(6, 100, 234, 118, AL_HV, "song layer status centred");
        cv_text_in(6, 100 + CAP_IN(S, 18), 228, &AF_S, b, T_THEME, T_SURF);
    }
}

static void layer_cards(uint32_t l)
{
    char val[12];
    const char *unit;
    uint32_t c;
    if (l == LAYER_FX) {                                /* FILTER CRUSH THROW DEPTH */
        int32_t m = perf_k[0];
        fmt_int(val, m < 0 ? -m : m);
        draw_column(0, "FILTER", m ? val : "OFF", m < 0 ? "LP" : m > 0 ? "HP" : "", m ? VAL(0u) : T_DIM, (m + 100) * 5,
                    ICON_CUTOFF);
        fmt_int(val, perf_k[1]);
        draw_column(1, "CRUSH", perf_k[1] ? val : "OFF", perf_k[1] ? "%" : "", perf_k[1] ? VAL(1u) : T_DIM,
                    perf_k[1] * 10, ICON_BITS);
        fmt_int(val, perf_k[2]);
        draw_column(2, "THROW", perf_k[2] ? val : "OFF", perf_k[2] ? "%" : "", perf_k[2] ? VAL(2u) : T_DIM,
                    perf_k[2] * 10, ICON_DELAY);
        fmt_int(val, 100 - perf_k[3]);
        draw_column(3, "DEPTH", val, "%", VAL(3u), (100 - perf_k[3]) * 10, ICON_MIX);
    } else if (l == LAYER_GLO) {                        /* T1..T4 LEVEL (T5, T6 selected: T3..T6; the icon; muted: dim) */
        for (c = 0; c < 4u; c++) {
            uint32_t t = c + trk_base4();
            param_format(&TP[P_LEVEL], trk[t].p[P_LEVEL], val, &unit);
            draw_column(c, "LEVEL", val, unit, glo_sounding(t) ? VAL(c) : T_DIM, RATIO(&TP[P_LEVEL], trk[t].p[P_LEVEL]),
                        trk_icon(t, 1));
        }
    } else if (l == LAYER_SAVE) {                       /* the section, LOOP / SONG, SONG REC, the song row */
        uint32_t n = lys.song ? lys.song - 1u : song_cur;
        fmt_int(val, (int32_t)n + 1);
        draw_column(0, "SONG", val, n != song_cur ? "NEXT" : "", VAL(0u), (int32_t)(n * 1000u / (NSONG - 1u)),
                    ICON_X_SONG);
        val[0] = live.cur >= 0 ? (char)('A' + live.cur) : '-'; val[1] = 0;
        draw_column(1, "SECT", val, "", live.cur >= 0 ? VAL(1u) : T_DIM, -1, ICON_X_PATTERN);
        draw_column(2, "PLAY", live.mode ? "SONG" : "LOOP", "", VAL(2u), -1, ICON_LOOP);
        draw_column(3, "REC", live.srec == 2u ? "ON" : live.srec ? "ARM" : "OFF", "", live.srec ? VAL(3u) : T_DIM, -1,
                    ICON_X_REC);
    } else if (l == LAYER_MACRO) {                      /* M1..M4, 0..100 % */
        static const char *const M[4] = {"M1", "M2", "M3", "M4"};
        for (c = 0; c < 4u; c++) {
            fmt_int(val, macro_v[c] * 100 / 127);
            draw_column(c, M[c], val, "%", macro_routes(c, 0, 0) ? VAL(c) : T_DIM, macro_v[c] * 1000 / 127, ICON_MIX);
        }
    } else if (l == LAYER_REC) {                        /* CLICK, then empty cards */
        for (c = 0; c < 4u; c++) {
            uint32_t v = c < RL_N ? rp_get(RL_F[c]) : 0u;
            if (c < RL_N)
                draw_column(c, RL_LABEL[c], rl_name(c, v), "", !v ? T_DIM : VAL(c), (int32_t)v * 500, RL_ICON[c]);
            else
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
        }
    } else {                                            /* the page's four (SCL: LY_SCL) */
        uint8_t h = ui.home, pg = ui.page;
        ui.home = 0;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        draw_columns();
        page_over = 0;
        ui.home = h;
        ui.page = pg;
    }
}

static void draw_layer(void)
{
    uint32_t l = ui.layer % LAYER_N, sig = l * 7919u + ux.gen * 977u;
    layer_cards(l);
    if (l == LAYER_FX)
        sig += (perf_kill ? 0u : perf_held | perf_latched) * 31u + perf_latch_on * 11u + perf_act * 131u + perf_avail() * 7u;
    else if (l == LAYER_GLO)
        sig += perf_solo * 31u + (uint32_t)song.g[G_CLOCK] * 5u +
               (uint32_t)(trk[0].p[P_MUTE] | trk[1].p[P_MUTE] << 1 | trk[2].p[P_MUTE] << 2 | trk[3].p[P_MUTE] << 3 |
                          trk[4].p[P_MUTE] << 4 | trk[5].p[P_MUTE] << 5) * 131u +
               ((uint32_t)dx_mute * 2u + (uint32_t)glo_drums()) * 4099u;
    else if (l == LAYER_SCL)
        sig += (uint32_t)TSEL->p[P_SCALE] * 31u;
    else if (l == LAYER_REC)
        sig += (uint32_t)ui_rec_prefs * 31u + (uint32_t)chain_busy() * 3u + (uint32_t)rec_clearable() * 5u + song.sel * 977u;
    else if (l == LAYER_SAVE)
        sig += (uint32_t)(live.cur + 1) * 31u + (uint32_t)(live.req + 1) * 131u + live.srec * 7u + live.mode * 11u +
               live.dirty * 977u + lys.nqc * 4099u + (chain.running ? chain.row * 13u + chain.remaining * 17u : 0u) +
               (lys.song * 8u + song_cur) * 65537u;
    else if (l == LAYER_MACRO)
        sig += macro_sig();
    else if (l == LAYER_SEQ)
        sig += (uint32_t)drum_track(TSEL) * 31u + ui.lane * 5u + (uint32_t)chain_busy() * 3u + (uint32_t)TSEL->p[P_E0] * 131u + song.sel * 977u;
    else
        sig += snd_id() * 31u + (uint32_t)chain_busy() * 3u + (uint32_t)(project_recall_slot() + 1) * 5u + dx_mute * 7919u +
               (uint32_t)drum_track(TSEL) * 11u + up_gen * 101u;
    if (ui.force || sig != ui.layer_sig) {
        ui.layer_sig = sig;
        cv_begin(240, H_GRAPH, T_BG);
        cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);
        cv_bg = T_SURF;
        if (l == LAYER_FX)
            layer_fx();
        else if (l == LAYER_GLO)
            layer_glo();
        else if (l == LAYER_SCL)
            layer_scl();
        else if (l == LAYER_SEQ)
            layer_seq();
        else if (l == LAYER_REC)
            layer_rec();
        else if (l == LAYER_SAVE)
            layer_song();
        else if (l == LAYER_MACRO)
            layer_macro();
        else
            layer_edit();
        cv_blit(0, Y_GRAPH);
    }
    if (ui.force) {                                     /* the footer: what the keys, knobs and buttons do */
        cv_begin(240, H_FOOT, T_BG);
        const khint_t *ft = l == LAYER_FX && perf_latch_on ? FX_LATCH_FOOT :
                            l == LAYER_MACRO && !layer_open() ? MAC_LATCH_FOOT : LAYERS[l].foot;
        cv_key_row(8, 232, 9, ft, ft[2].act ? 3u : 2u, 7u, T_BG);
        cv_blit(0, Y_FOOT);
        ui.foot_sig = 0;
    }
}
