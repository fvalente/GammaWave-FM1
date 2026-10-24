/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI input: LEDs, knobs and buttons, SEQ step entry, panel setup. */
#include "ui_name.c"                                    /* NAME: naming user presets and projects */
/* ----------------------------------------------------------- LEDs --- */
/* The LED picture is built off-line and copied one byte per column: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker. */
static uint8_t led_pos[41];                        /* (col << 3) | row bit, 0xFF = none */

static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}

static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}

/* PLAY's second, green LED: not in the key matrix (no key there), found on the hardware at column 8, row PA9 (bit 1) */
#define LED_PLAY_GREEN ((8u << 3) | 1u)
static void led_clear(uint8_t *nl, uint32_t id)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF)
        nl[q >> 3] &= (uint8_t)~(1u << (q & 7u));
}

static const uint8_t FAM_BTN[FAM_COUNT] = {B_HOME, B_ENV, B_LFO, B_FX, B_SCL, B_EDIT, B_GLO, B_SAVE,
                                           B_ARP, B_SEQ, B_GLO};   /* GLO: mixer + global settings; REC is transport */
static void motion_capture_ui(track_t *t, uint32_t id, int16_t v);   /* (1.1.1-GW: below, by rec_clear_auto) */

static uint32_t cur_fam(void) { return ui.home ? FAM_HOME : cur_page()->fam; }

/* (JIANT 0.6.6) a page button held + PRESETS: that family's pages one by one, forwards / back (GLO: the mixer and the
 * global pages), the page shown at once with its title; the button let go stays there (its tap, its layer: not now) */
static uint8_t ly_used;                                  /* the held layer's knobs or keys used: PRESETS stays its */
static int song_open(uint32_t n);                         /* (song_main.c: SAVE > PROJECT's KNOB 1) */
static uint8_t pj_btn = 0xFFu;                           /* the button a page jump was made with: no layer while held */
static uint32_t held_family(void)
{
    uint32_t f;
    for (f = FAM_ENV; f < FAM_COUNT; f++)
        if (f != FAM_TRK && (fm1_in.buttons >> panel.btn[FAM_BTN[f]]) & 1u)
            return f;
    return FAM_COUNT;
}
static int page_in(uint32_t i, uint32_t fam)
{
    return (PAGES[i].fam == fam || (fam == FAM_GLO && PAGES[i].fam == FAM_TRK)) && page_visible(i);
}
static void page_jump(uint32_t fam, int32_t s)
{
    uint32_t b = FAM_BTN[fam], i = ui.page, j;
    if (ui.home || !page_in(ui.page, fam)) {          /* (from elsewhere: the family's first page, then the turns) */
        for (i = 0; i < NPAGES && !page_in(i, fam); i++)
            ;
        if (i == NPAGES)
            return;
        s += s > 0 ? -1 : 1;
    }
    while (s) {                                         /* (no wrap: its first and last pages are where it stops) */
        for (j = i; ; ) {
            j = s > 0 ? j + 1u : j - 1u;
            if (j >= NPAGES || page_in(j, fam))
                break;
        }
        if (j >= NPAGES)
            break;
        i = j;
        s += s > 0 ? -1 : 1;
    }
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = ui.page;
    ui.home = 0;
    page_entered();
    page_flash();                                       /* (0.6.51) */
    ui_message(PAGES[i].title);
    ui.pg_down &= (uint16_t)~(1u << panel.btn[b]);     /* (the button let go: no next page) */
    if (b == B_SEQ) ui.seq_t0 |= 2u;
    if (b == B_SAVE) ui.save_t0 |= 2u;
    pj_btn = (uint8_t)b;                                /* (its layer off until it is let go: pj_hold) */
}
/* (0.6.67) a button whose SELECT combo was used: no next page when let go, no tap, its layer shut until let go */
static void btn_used(uint32_t b)
{
    ui.pg_down &= (uint16_t)~(1u << panel.btn[b]);
    if (b == B_SEQ) ui.seq_t0 |= 2u;
    if (b == B_SAVE) ui.save_t0 |= 2u;
    pj_btn = (uint8_t)b;
}
/* every pass: while the button of a page jump is held its layer stays shut (no map at HOLD, no tap when let go), its
 * page taps too; let go: free again */
static void pj_hold(void)
{
    if (pj_btn == 0xFFu)
        return;
    if (!((fm1_in.buttons >> panel.btn[pj_btn]) & 1u)) {
        pj_btn = 0xFFu;
        return;
    }
    ui.pg_down &= (uint16_t)~(1u << panel.btn[pj_btn]);
    if (pj_btn == B_SEQ) ui.seq_t0 |= 2u;
    if (pj_btn == B_SAVE) ui.save_t0 |= 2u;
    if (ui.ly)
        ui.ly_t0 = (ui.ly_t0 & ~2u) | 8u;               /* (ui_layer.c: LY_OPEN off, LY_DEAD) */
}

static int layer_set_open(void);                       /* (ui_layer.c) */
/* the OCT LEDs, bit 0 OCT- lit, bit 1 OCT+ lit, OCT_BREATH OCT+ breathing (can be pressed: dark .. ~60 %,
 * hal/fm1_input.h fm1_led_breath, #119). In the dialogs and on action pages OCT- (back) is lit and OCT+ breathes
 * while it would do something; the menu: on a value row OCT- and OCT+ both breathe while that way has a value to go to,
 * on CALIBRATION / ABOUT OCT+ breathing alone (opens), in ABOUT OCT- lit (back); elsewhere the octave shift */
#define OCT_BREATH 4u
#define OCT_BREATH_DN 8u                                /* OCT- breathing (the menu: the previous value) */
static uint32_t oct_leds(void)
{
    if (name_on() && !ui.confirm && !ui.menu)           /* NAME: OCT- cancels, OCT+ (breathing) writes */
        return 1u | OCT_BREATH;
    if (layer_set_open())                               /* a SET layer: OCT± do nothing (TONIC: no put back); */
        return ui.layer == LAYER_FX ? 1u : 0u;          /* FX LATCH: OCT- all off, lit */
    if (ui.menu && !ui.confirm)
        return ui.menu >= 2u ? 1u :                    /* (ABOUT: OCT- back, lit) */
               ui.menu_sel >= MI_VALUES ? OCT_BREATH :  /* CALIBRATION / ABOUT: OCT+ opens */
               (menu_step(ui.menu_sel, -1) != menu_get(ui.menu_sel) ? OCT_BREATH_DN : 0u) |   /* a value: each */
               (menu_step(ui.menu_sel, 1) != menu_get(ui.menu_sel) ? OCT_BREATH : 0u);         /* way it can go */
    if (ui.confirm || act_cols())
        return 1u | (ui.confirm || act_ready() ? OCT_BREATH : 0u);
    return (song.octave < 0 ? 1u : 0u) | (song.octave > 0 ? 2u : 0u);
}

/* the key LEDs on the grid, bit k = key k: the white keys show where the selected lane hits on the page
 * shown (its accents while ACC is held), the step playing inverted (a light walks over them); the black keys
 * the lane selected, ACC while held, the page keys while there is more than one page */
static uint32_t grid_leds(void)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN], b = 1u << ui.lane, acc = (uint32_t)black_held(GK_ACC);
    uint32_t ph = song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), on;
        if (!key_black(k)) {
            uint32_t i = ui.bank * 16u + p;
            on = i < len && ((acc ? step_accents(&seq_steps(t)[i]) : step_lanes(&seq_steps(t)[i])) & b) != 0u;
            on ^= (uint32_t)(p == ph);
        } else {
            on = p < NLANE ? p == ui.lane : p == GK_ACC ? acc : len > 16u;
        }
        m |= on << k;
    }
    return m;
}

/* the DRUM grid's beats, bit k = key k (ui_leds: a steady brighter glow than the other steps', hal/fm1_input.h
 * fm1_led_mid): the first step of each group of four on the page shown, steps 1 5 9 13 of 16 ("5111 5111 5111 5111"),
 * within LEN, where grid_leds shows no hit (the lane's, its accents while ACC is held) and not the playhead. Always
 * groups of four from the pattern's first step, whatever LEN or DIV (a 3/4 or a DIV 1/8 pattern too): the steps are
 * counted, not the time */
static uint32_t grid_beats(void)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN], b = 1u << ui.lane, acc = (uint32_t)black_held(GK_ACC);
    uint32_t ph = song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), i = ui.bank * 16u + p;
        if (key_black(k) || p % 4u || i >= len || p == ph)
            continue;
        m |= (uint32_t)(((acc ? step_accents(&seq_steps(t)[i]) : step_lanes(&seq_steps(t)[i])) & b) == 0u) << k;
    }
    return m;
}

/* the DRUM grid's keys that do something, bit k = key k (they glow with the idle LEDs, ui_leds): the page's steps
 * within LEN, the lane keys, ACC, the page keys while there is more than one page */
static uint32_t grid_glow(void)
{
    uint32_t k, m = 0, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        m |= (uint32_t)(!key_black(k) ? ui.bank * 16u + p < len : p < NLANE || p == GK_ACC || len > 16u) << k;
    }
    return m;
}

/* an ARP playing on any part flashes the ARP button on the beat, the bar's first beat longer: 1 lit, 0 dark,
 * 2 no ARP playing */
static uint32_t arp_led(void)
{
    uint32_t k, on = 0, b = beat_samples();
    for (k = 0; k < NPART; k++)
        on |= trk[k].p[P_AMODE] && trk[k].nheld;
    return !on ? 2u : beat_pos < (beat_n ? b / 6u : b / 2u);
}

/* Discussion #81: the keys of the notes MIDI IN (USB and TRS, routed by ROUT) holds on track t, bit k = key k: as
 * play_leds, where the keys play that note at the octave now, the lowest key that gives it; a note no key plays is
 * not shown. Read from midi_control.c's own state (midi_note_held: the notes each channel holds for the track, the
 * pedal's too, and the tones of MIDI chords): no state here; nothing to do while MIDI holds none of the track's */
static uint32_t midi_leds(const track_t *t)
{
    uint32_t k, note, m = 0, seen[4] = {0, 0, 0, 0};
    if (!midi_owners[trk_index(t)])
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= (uint32_t)midi_note_held(t, note) << k;
    }
    return m;
}

/* the keys of the notes the selected track's sequencer and ARP sound now (#38), bit k = key k: where the keys
 * play that note (kb_map: the octave, TRN, QNT, an engine's own map), the lowest key that gives it (QNT SNAP
 * rounds the keys above down onto it); a note no key plays is not shown. A snapshot of the ISR's seq_notes /
 * arp_note (arp_ch): no state of its own, nothing to do while nothing sounds */
static uint32_t play_leds(void)
{
    const track_t *t = TSEL;
    uint8_t s[4 + NLANE + 4];
    uint32_t n = t->seq_n < 4u + NLANE ? t->seq_n : 4u + NLANE, i, k, note, used = 0, m = 0, hit;
    for (i = 0; i < n; i++)
        s[i] = t->seq_notes[i];
    if (t->arp_note)
        s[n++] = t->arp_note;
    for (i = 0; i < 3u; i++)                            /* (the ARP's CHORD) */
        if (t->arp_ch[i])
            s[n++] = t->arp_ch[i];
    for (k = 0; n && k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0, hit = 0; i < n; i++)
            if (s[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                hit = 1;
            }
        m |= hit << k;
    }
    return m | midi_leds(t);
}

/* (0.6.83) the keys of every note the selected track's voices hold now, bit k = key k: a chord of SCL > CHORD, LATCH, a
 * chord gliding, MIDI, all of what sounds is shown (its released tails are not), as play_leds: the lowest key that
 * gives the note at the octave now; a kit's lanes not (its keys are its sounds). Read from the voices: no state */
static uint32_t voice_leds(void)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t i, k, m = 0, seen[4] = {0, 0, 0, 0}, have[4] = {0, 0, 0, 0}, any = 0, note;
    if (e->oneshot || (e->keys && e->keys(t, 0) >= 0))
        return 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note < 128u) {
            have[t->v[i].note >> 5] |= 1u << (t->v[i].note & 31u);
            any = 1;
        }
    for (k = 0; any && k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= ((have[note >> 5] >> (note & 31u)) & 1u) << k;
    }
    return m;
}

/* Discussion #89: on SEQ > STEP (the piano roll; the DRUM grid has its own map) while stopped, the keys of the
 * notes stored in the cursor step, bit k = key k: as play_leds, where the keys play that note now (kb_map: the
 * octave, TRN, QNT), the lowest key that gives it; a note no key plays is not shown, nor a REST step's. Playing,
 * the keys show what plays (play_leds) instead. Read from the step: no state of its own */
static uint32_t step_leds(void)
{
    const track_t *t = TSEL;
    const step_t *st;
    uint32_t i, k, note, used = 0, m = 0;
    if (ui.home || ui.menu || ui.confirm || !song.seq_mode || cur_page()->graph != GR_ROLL || song.playing ||
        chain_busy() || ui.cursor >= NSTEP)
        return 0;
    st = &t->step[ui.cursor];
    if (!st->n || st->time == ST_REST)
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0; i < st->n && i < 4u; i++)
            if (st->note[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                m |= 1u << k;
            }
    }
    return m;
}

/* 1: the keys show a map of their own (NAME, a layer's map: SCL's scale, FX; the DRUM grid), lit or
 * dark; 0: the keys held and the notes playing, over the idle glow */
static int gen_keys_own(void);                         /* (gen_reel.c: OCT+ held on the GEN screen, the take layer) */
static uint32_t gen_layer_leds(uint32_t *br);
static int keys_own(void)
{
    return (name_on() && !ui.menu) || ui.layer || grid_on() || gen_keys_own();
}

/* Discussion #127 (1.2): MENU > SCALE LEDS ON, the keys of the selected track's scale (ROOT, SCALE), bit k = key k, as
 * the keys play now: the note a key gives with TRN (QNT SNAP / SEQ: the key's own note; the others round down onto
 * the scale); QNT WHITE every white key (they walk the scale; the black ones are silent). *root: the keys of the
 * scale's root. None where the engine maps the keys itself (DRUM's GM map, the slices): 0 there, and OFF */
static uint32_t scale_leds(uint32_t *root)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t k, m = 0, r = 0, mask = scale_mask(t), note;
    int32_t pc;
    *root = 0;
    if (!(ui_rec_prefs & (PREF_SCALE_LEDS >> 8)))
        return 0;
    for (k = 0; k < 27u; k++) {
        if (e->keys && e->keys(t, k) >= 0)
            continue;
        if (t->p[P_QUANT] == QN_WHITE) {
            if ((note = kb_map(t, k)) > 127u)
                continue;                               /* (a black key: silent) */
            pc = (int32_t)note + t->p[P_TRANS];         /* (0.6.81: the note it sounds: TRANS added as it sounds,
                                                         * voice.c trk_trans; WHITE walks the scale before it) */
        } else {
            pc = 53 + (int32_t)k + t->p[P_TRANS];
        }
        pc = ((pc - t->p[P_ROOT]) % 12 + 12) % 12;
        m |= ((mask >> pc) & 1u) << k;
        r |= (uint32_t)(pc == 0) << k;
    }
    *root = r & m;
    return m;
}

/* the key LEDs, bit k = key k: NAME's keys, the layer's map, the DRUM grid, else the keys held and the notes
 * the selected track's sequencer, ARP and MIDI IN play, on STEP while stopped the cursor step's notes (#89), and
 * (0.6.83) every note its voices hold (a CHORD's whole chord) */
static uint32_t key_leds(uint32_t *br)                 /* (*br: a layer's keys that breathe, layer_leds) */
{
    uint32_t c;
    if (br)
        *br = 0;
    c = name_on() && !ui.menu ? name_leds() : ui.layer ? layer_leds(br) : gen_keys_own() ? gen_layer_leds(br) :
        grid_on() ? grid_leds() :
                 (fm1_in.notes & ~kb_layer) | play_leds() | step_leds() | voice_leds();
    return c;
}

/* The LEDs: lit = active (the page's family, PLAY / REC running, the keys held or playing, a map's keys), the ARP
 * beat flashes, the ones that can be pressed breathe (#119: the open layer's button, OCT+ when it would act, a
 * layer's keys; dark .. ~60 % of lit and back, hal/fm1_input.h fm1_led_breath, in every mode: never a hard blink in
 * a dark room; DIM LO up to ~30 %, DIM HI OFF INV ~60 %; with LEDS OFF it is the only glow; INV leaves it as it is,
 * between the dark active ones and the lit idle ones), every other button and key glows dim (#35: the
 * buttons of the black FM-1 can be found in the dark; hal/fm1_input.h fm1_led_dim, a short pulse each frame).
 * MENU > LEDS: DIM HI (default) that glow, DIM LO a darker one (fm1_led_dim_level), OFF no glow (as 1.0); INV
 * turns it around, as the stock firmware: the idle ones fully lit, the active ones dark, no glow (the ARP beat:
 * lit / dark). The keys' own maps (keys_own) stay lit or dark in every mode: their dark keys read as dark; only the
 * DRUM grid's keys that do something (grid_glow) glow under it, so STEP on a DRUM track is never a dark
 * keyboard (the steps of an empty pattern); on it the first step of each four (grid_beats: 1 5 9 13 of a page, where
 * no hit shows) glows steadier and brighter, so a bar's beats can be counted: DIM HI / DIM LO the mid level
 * (hal/fm1_input.h fm1_led_mid: lit 1 frame in 8, DIM LO in 12, over the glow), OFF and INV the glow alone (no
 * other key glows there). 1.2, MENU > SCALE LEDS ON (scale_leds; not over a map of the keys' own, nor on a DRUM or
 * SLICE track): the keys of the selected track's scale glow and the others are dark, the root's at the mid level, in
 * DIM HI, DIM LO and OFF alike (asked for: OFF only takes the other glow away); INV turns it around as it does the
 * rest: the root's keys lit, the scale's at the mid level, the others glowing; the keys held and playing as ever
 * (lit, INV dark). Each picture is built off-line and copied one byte per column, the
 * glow first: an LED going from lit to dim never has a dark frame */
static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, own[FM1_NCOL] = {0}, og[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    uint8_t nm[FM1_NCOL] = {0}, sg[FM1_NCOL] = {0}, sm[FM1_NCOL] = {0};
    uint32_t k, c, g, br, beat, sc, sr = 0;
    uint32_t fam = cur_fam(), mode = settings_leds;
    int keys_map = keys_own();
    static uint8_t ready;
    if (fm1_led_anim_on())                              /* the power-on sweep has them (main.c boot_leds) */
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    if (!ui.layer || FAM_BTN[fam] != layer_btn())
        led_put(nl, panel.btn[FAM_BTN[fam]], 1);
    if ((k = arp_led()) != 2u && (!ui.layer || layer_btn() != B_ARP))
        led_put(nl, panel.btn[B_ARP], FAM_BTN[fam] == B_ARP ? !k : (int)k);   /* (on ARP's page: dark flashes) */
    if (!ui.layer && (perf_latched || perf_k[0] || perf_k[1] || perf_k[2] || perf_k[3]))
        led_put(nl, panel.btn[B_FX], 1);                /* FX LATCH: lit while an effect or a macro is on */
    if (ui.layer)                                       /* the layer's button breathes while its map is up */
        led_put(nb, panel.btn[layer_btn()], 1);
    led_put(nl, panel.btn[B_REC], song.rec != 0u && ui.layer != LAYER_REC);   /* (its layer: it breathes) */
    k = oct_leds();
    led_put(nl, panel.btn[B_OCTDN], (int)(k & 1u));
    led_put(nl, panel.btn[B_OCTUP], (int)((k >> 1) & 1u));
    led_put(nb, panel.btn[B_OCTUP], (int)(k & OCT_BREATH));
    led_put(nb, panel.btn[B_OCTDN], (k & OCT_BREATH_DN) != 0u);
    c = key_leds(&br);
    g = !keys_map ? 0u : grid_on() && !ui.layer && !name_on() ? grid_glow() : 0u;   /* (key_leds: the grid's map) */
    beat = g ? grid_beats() & ~c : 0u;                  /* (the grid's empty beat starts: brighter than the glow) */
    sc = keys_map ? 0u : scale_leds(&sr);               /* SCALE LEDS: the scale's keys glow, the root's brighter */
    for (k = 0; k < 27u; k++) {
        uint32_t in = (sc >> k) & 1u, rt = (sr >> k) & 1u, idle = !((c >> k) & 1u);
        led_put(nm, 14u + k, (int)((beat >> k) & 1u));
        led_put(keys_map ? own : nl, 14u + k, (int)((c >> k) & 1u));
        led_put(keys_map ? og : nd, 14u + k, keys_map ? (int)((g >> k) & 1u) : !sc || (mode == LEDS_INV && rt));
        led_put(nb, 14u + k, (int)((br >> k) & 1u));
        if (!sc)
            continue;
        led_put(sg, 14u + k, mode == LEDS_INV ? !in && idle : (int)in);   /* (INV: the active ones dark) */
        led_put(sm, 14u + k, mode == LEDS_INV ? in && !rt && idle : (int)rt);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (song.playing || seq_counting())                 /* playing: PLAY's green, its own LED dark in every mode */
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++) {
        if (mode == LEDS_INV)                           /* INV: the active ones dark, the rest lit */
            nl[c] = (uint8_t)(nd[c] & ~nl[c]);
        nd[c] = mode == LEDS_DIM || mode == LEDS_DIM_LO ? (uint8_t)(nd[c] | og[c]) : nm[c];   /* OFF, INV: no glow but */
        if (mode != LEDS_DIM && mode != LEDS_DIM_LO)    /* the grid's beats (the one level above dark there); DIM HI */
            nm[c] = 0;                                  /* / LO: the beats at the mid level over the glow */
        nd[c] |= sg[c];                                 /* SCALE LEDS: in every mode (INV: inverted above) */
        nm[c] |= sm[c];
        nm[c] &= (uint8_t)~(nb[c] | own[c]);
        nl[c] |= own[c];
        nl[c] &= (uint8_t)~nb[c];                       /* breathing: neither lit (INV's idle) nor the glow */
        nd[c] &= (uint8_t)~nb[c];
    }
    if (song.playing || (seq_counting() && cin_pos < click_beat_len() / 4u))   /* counting in: green on each beat */
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(mode == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_mid[c] = nm[c];
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
}

/* ---------------------------------------------------------- input --- */
/* Predictable hardware response (#23): each decoded detent is one value step; a fast turn keeps its full signed
 * detent count. KNOB ACCEL (#52; JIANT: ON by default, MENU > KNOB ACCEL OFF turns it off) multiplies a fast turn of a
 * wide value (range > 16, not a list of names; the FX and GLO layers' knobs too, #126): the faster the turn the
 * wider the sweep, a slow turn is fine tuning, one step per detent. The main loop reads the knobs many times a frame
 * (main.c), so a read holds one detent as a rule: the speed is the time per detent, ACC_RATE / ms -> 40 ms x2,
 * 27 ms x3, 20 ms x4, 10 ms x8, 5 ms x16; capped by the range: x2 up to 32, x4 up to 64, x8 up to 100, x16 above (a
 * flick across 0..127 in a dozen detents). Only the longer of this read's and the previous read's time counts, and
 * only while the turn goes on (both under ACC_GAP ms) in one direction: a slow turn, the first two detents of a turn,
 * a single quick detent (a bounce) and a reversal are one step per detent, and the sign is always the detents'.
 * ui.enc_t[role]: bits 0..23 the ms of its last read, bit 24 its direction (+1), 25..31 its ms per detent (127 slow) */
#define ACC_GAP 60u
#define ACC_RATE 80u
static int32_t accel(uint32_t role, int32_t s, int32_t range)
{
    uint32_t now = fm1_ms & 0xFFFFFFu, st = ui.enc_t[role], up = s > 0, pi = st >> 25, a, i, m = 1;
    if ((ui_prefs & PREF_ACCEL_OFF) || range <= 16 || !s)
        return s;
    a = (uint32_t)(s < 0 ? -s : s);
    i = ((now - st) & 0xFFFFFFu) / a;                   /* ms per detent of this read */
    if (!st || ((st >> 24) & 1u) != up || i >= ACC_GAP)
        i = 127u;                                       /* a new turn, or reversed */
    else if (pi < ACC_GAP) {
        m = ACC_RATE / (i > pi ? i : pi ? pi : 1u);
        a = range > 100 ? 16u : range > 64 ? 8u : range > 32 ? 4u : 2u;   /* (the cap) */
        m = m < 1u ? 1u : m > a ? a : m;
    }
    ui.enc_t[role] = now | up << 24 | (i ? i : 1u) << 25;
    return s * (int32_t)m;
}

/* MIXER page: KNOB 1 LEVEL, 2 PAN, 3 REV send, 4 MUTE of the selected track (right = ON, left = OFF: the
 * track itself is ALGORITHM's, on every page). Pattern length stays on SEQ. */
static void tracks_edit(uint32_t slot, int32_t steps)
{
    track_t *t = TSEL;
    int16_t *vp;
    const param_desc_t *d;
    switch (slot) {
    case 3:
        t->p[P_MUTE] = (int16_t)(steps > 0);
        return;
    case 0:
        vp = &t->p[P_LEVEL];
        d = &TP[P_LEVEL];
        break;
    case 2:
        vp = &t->p[P_REV];
        d = &TP[P_REV];
        break;
    default:
        vp = &t->p[P_PAN];
        d = &TP[P_PAN];
        break;
    }
    *vp = (int16_t)clamp(*vp + accel(EN_K1 + slot, steps, d->max - d->min), d->min, d->max);
    motion_capture_ui(t, (uint32_t)(vp - t->p), *vp);
}

/* SEQ > STEP's piano roll (not the DRUM grid, whose white keys stay its steps): where step recording writes */
static int roll_on(void)
{
    return !ui.home && !ui.menu && !ui.confirm && song.seq_mode && cur_page()->graph == GR_ROLL && !drum_track(TSEL);
}

/* REC tap on every page: arm / disarm recording on the selected track without navigating; arming while stopped
 * starts the transport too (live recording), except on STEP's piano roll (1.2, Discussion #133): there it arms step
 * recording and the transport stays stopped: the keys write the cursor step (seq_entry), PLAY then records live.
 * Armed and playing, STEP's keys record live (at the play head) instead of writing the cursor step */
static int rec_tap(void)                                /* 1: the arming changed */
{
    uint8_t bit = (uint8_t)(1u << song.sel);
    if (chain_busy()) {
        ui_message("STOP TO RECORD");
        return 0;
    }
    if (!ui.home && cur_page()->graph == GR_SONG && !(song.rec & bit)) {
        ui_message("[SEQ] TO RECORD");
        return 0;
    }
    song.rec ^= bit;
    ui.force = 1;                                     /* also refresh the status on MENU / ABOUT */
    if ((song.rec & bit) && !song.playing && !seq_counting() && roll_on()) {
        ui_message("KEYS WRITE STEPS");               /* step recording: stopped, at the cursor */
        return 1;
    }
    if ((song.rec & bit) && !song.playing && !seq_counting())
        transport_req = 1;
    if ((song.rec & bit) && grid_on())
        ui_message("LANE KEYS RECORD");               /* (the white keys stay the steps) */
    else if ((song.rec & bit) && !ui.home && cur_page()->graph == GR_ROLL)
        ui_message("KEYS RECORD LIVE");               /* (seq_entry pauses while armed and playing) */
    return 1;
}

/* live recording into the selected track now: the STEP page's key entry pauses meanwhile */
static int live_rec_sel(void)
{
    return ((song.rec >> song.sel) & 1u) && (song.playing || transport_req == 1u || seq_counting());
}

/* the OCT- / OCT+ dialog (ui_draw.c draws it); trk: the track or the slot it is about */
static void confirm_open(uint32_t kind, uint32_t trk)
{
    ui.confirm = (uint8_t)kind;
    ui.confirm_trk = (uint8_t)trk;
    ui.act = 0;
    ui.force = 1;
}

/* the grid's page down (-1) / up (+1): the cursor to the same place on it (at most the last step) */
static void page_go(int32_t d)
{
    uint32_t len = (uint32_t)TSEL->p[P_SLEN], pages = (len + 15u) / 16u, b;
    if (pages < 2u)
        return;
    b = (ui.bank + pages + (uint32_t)d) % pages;
    cursor_set((int32_t)(b * 16u + ui.cursor % 16u < len ? b * 16u + ui.cursor % 16u : len - 1u));
}

/* the grid's knobs: 1 STEP (the cursor), 2 LANE, 3 HIT and 4 ACC of the lane at the cursor (right on, left off) */
static void grid_edit(uint32_t slot, int32_t steps)
{
    if (slot == 0u)
        cursor_set(ui.cursor + steps);
    else if (slot == 1u)
        ui.lane = (uint8_t)clamp((int32_t)ui.lane + (steps > 0 ? 1 : -1), 0, NLANE - 1);
    else if (slot == 2u)
        grid_hit(TSEL, ui.cursor, ui.lane, steps > 0);
    else
        grid_acc(TSEL, ui.cursor, ui.lane, steps > 0);
}

/* the keys on the grid (presses): a white key toggles the selected lane at its step of the page (its accent
 * while ACC is held) and puts the cursor there; a lane key selects the lane (seq.c plays it); the page keys */
static void grid_keys(uint32_t pressed)
{
    uint32_t k, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u))
            continue;
        if (!key_black(k)) {
            uint32_t i = ui.bank * 16u + p;
            if (chain_busy()) { ui_message("STOP TO EDIT"); continue; }
            if (i >= len)
                continue;                               /* past LEN: no step there */
            {   /* a hit (an accent) there goes when the key is let go, unless it was held for a lock (lock_keys) */
                const step_t *st = &TSEL->step[i];
                uint64_t b = (uint64_t)1 << i;
                uint32_t acc = (uint32_t)black_held(GK_ACC);
                if (((acc ? step_accents(st) : step_lanes(st)) >> ui.lane) & 1u) {
                    ui.plk_rm |= b;
                    ui.plk_acc = acc ? ui.plk_acc | b : ui.plk_acc & ~b;
                    ui.plk_lane = ui.lane;
                    ui.plk_trk = song.sel;
                } else if (acc) {
                    grid_acc(TSEL, i, ui.lane, 1);
                } else {
                    grid_hit(TSEL, i, ui.lane, 1);
                }
            }
            cursor_set((int32_t)i);
        } else if (p < NLANE) {
            ui.lane = (uint8_t)p;
        } else if (p != GK_ACC) {
            page_go(p == GK_PGUP ? 1 : -1);
        }
    }
}

/* ------------------------------------------------- parameter locks --- */
/* 1.1 (Discussions #75, #53): on SEQ > STEP, hold a step and turn KNOB 1..4: the step gets a lock of the parameter
 * that knob edits on the sound page shown last (ui.plk_src: HOME's four, ENV, LFO, FX, EDIT 1 / 2, OP ENV, ..; the
 * cards show those four while a step is held, a locked value bright, the sound's own DIM). The step held: on the
 * DRUM grid its white keys (several at once: each gets the lock; a press on a hit takes it away only when let go,
 * and not at all when it was held for a lock), on the roll the step being entered (its note keys held). [EDIT]
 * tapped meanwhile clears the held steps' locks (EDIT alone still clears the step, its locks with it). Not while a
 * song plays (STOP TO EDIT). A lock is a motion record (motion.c): AUTOMATION's PLAY plays it, CLEAR clears it */
static uint64_t lock_held(void)
{
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)t->p[P_SLEN], keys = fm1_in.notes & ~kb_layer;
    uint64_t m = 0;
    if (ui.home || ui.menu || ui.confirm || ui.layer || name_on() || !song.seq_mode || cur_page()->graph != GR_ROLL ||
        !keys)
        return 0;
    if (!grid_on())                                     /* the roll: the step being entered */
        return ui.entry_open && !live_rec_sel() && ui.cursor < len ? (uint64_t)1 << ui.cursor : 0u;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && !key_black(k) && ui.bank * 16u + key_place(k) < len)
            m |= (uint64_t)1 << (ui.bank * 16u + key_place(k));
    return m;
}

/* each pass, before the grid's keys: a hit pressed on goes once its key is let go (plk_rm, grid_keys) */
static void lock_keys(void)
{
    uint64_t gone = ui.plk_rm & ~lock_held();
    uint32_t i;
    for (i = 0; gone && i < NSTEP; i++)
        if ((gone >> i) & 1u) {
            if ((ui.plk_acc >> i) & 1u)
                grid_acc(&trk[ui.plk_trk % NTRK], i, ui.plk_lane, 0);
            else
                grid_hit(&trk[ui.plk_trk % NTRK], i, ui.plk_lane, 0);
        }
    ui.plk_rm &= ~gone;
}

/* KNOB k turned s with the steps `held` held: each gets (or changes) its lock of lock_id(k), from its lock or the
 * sound's own value */
static void lock_turn(uint32_t k, int32_t s, uint64_t held)
{
    track_t *t = TSEL;
    uint32_t id = lock_id(k), i;
    const param_desc_t *d;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (id == 0xFFu)
        return;                                         /* (an empty card: nothing to lock) */
    ui.plk_rm &= ~held;                                 /* (held for a lock: its hit stays) */
    d = track_desc(t, id);
    s = accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
    for (i = 0; i < NSTEP; i++) {
        int16_t v;
        if (!((held >> i) & 1u))
            continue;
        if (!motion_lock_get(t, i, id, &v))
            v = motion_base_value(t, id);
        if (motion_set_lock(t, i, id, (int16_t)param_turn(d, v, s)) == 2)
            break;                                      /* (full: ui_notices says so) */
    }
}

/* [EDIT] with the steps `held` held: their locks go */
static void lock_clear(uint64_t held)
{
    uint32_t i, n = 0;
    ui.plk_rm &= ~held;
    if (!(motion_lock_steps(song.sel) & held)) {
        ui_message("NO LOCKS");
        return;
    }
    for (i = 0; i < NSTEP; i++)
        if ((held >> i) & 1u)
            n += motion_clear_locks(TSEL, i);
    ui_message(n ? "LOCKS CLEARED" : "NO LOCKS");
}

/* (0.6.81) SEQ > NOTE, the piano roll's edits on the cursor's step: FOCUS the chord or one of its notes (lit), PITCH
 * moves what is in focus up or down, MOVE moves it a step left or right (the chord with its length: it changes places
 * with the step beside it; a note: into the step beside, a NOTE with room or an empty one), LEN the chord's length (its
 * TIEs, over empty steps only). EDIT deletes what is in focus (page_tap) */
static uint32_t note_len(const track_t *t, uint32_t c, uint32_t len)   /* the step and its TIEs after it */
{
    uint32_t l = 1;
    while (l < len && t->step[(c + l) % len].time == ST_TIE)
        l++;
    return l;
}
static void note_drop(step_t *st, uint32_t j)           /* note j out of the step (empty: a REST, its TIEs nothing) */
{
    uint32_t i;
    for (i = j; i + 1u < st->n; i++)
        st->note[i] = st->note[i + 1u];
    if (st->n) st->note[--st->n] = 0;
    if (!st->n && !st->hit) st->time = ST_REST;
}
static void note_edit(uint32_t slot, int32_t steps)
{
    track_t *t = TSEL;
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, c = ui.cursor, i;
    step_t *st = &t->step[c];
    uint32_t f = ui.pr_focus <= st->n ? ui.pr_focus : 0u;
    if (st->time != ST_NOTE)
        f = 0;
    switch (slot) {
    case 0:                                             /* FOCUS: ALL, 1 .. the chord's notes */
        ui.pr_focus = (uint8_t)clamp((int32_t)f + (steps > 0 ? 1 : -1), 0, st->time == ST_NOTE ? (int32_t)st->n : 0);
        break;
    case 1:                                             /* PITCH */
        if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); break; }
        for (i = 0; i < st->n; i++)
            if (!f || i == f - 1u)
                st->note[i] = (uint8_t)clamp(st->note[i] + steps, 1, 127);
        last_note = st->note[f ? f - 1u : 0u];
        break;
    case 2: {                                           /* MOVE, a step a detent */
        int32_t dir = steps > 0 ? 1 : -1;
        if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); break; }
        if (!f) {                                       /* the chord and its TIEs change places with the step beside */
            uint32_t l = note_len(t, c, len), k;
            step_t blk[NSTEP], other;
            if (l >= len) break;
            for (k = 0; k < l; k++) blk[k] = t->step[(c + k) % len];
            if (dir > 0) {
                uint32_t nx = (c + l) % len, nl = note_len(t, nx, len);
                if (t->step[nx].time == ST_NOTE && nl > 1u) { ui_message("BLOCKED: A LONG NOTE"); break; }
                other = t->step[nx];
                if (other.time == ST_TIE) { other.time = ST_REST; other.n = 0; }
                t->step[c] = other;
                for (k = 0; k < l; k++) t->step[(c + 1u + k) % len] = blk[k];
                ui.cursor = (uint8_t)((c + 1u) % len);
            } else {
                uint32_t pv = (c + len - 1u) % len;
                if (t->step[pv].time == ST_TIE) { ui_message("BLOCKED: A LONG NOTE"); break; }
                other = t->step[pv];
                for (k = 0; k < l; k++) t->step[(pv + k) % len] = blk[k];
                t->step[(pv + l) % len] = other;
                ui.cursor = (uint8_t)pv;
            }
        } else {                                        /* one note: into the step beside */
            uint32_t to = (c + len + (uint32_t)dir) % len;
            step_t *d = &t->step[to];
            uint8_t n = st->note[f - 1u];
            if (d->time == ST_TIE) { ui_message("BLOCKED: A LONG NOTE"); break; }
            if (d->time == ST_NOTE && d->n >= 4u) { ui_message("BLOCKED: 4 NOTES THERE"); break; }
            if (d->time != ST_NOTE) { memset(d, 0, sizeof *d); d->time = ST_NOTE; d->vel = st->vel; }
            note_drop(st, f - 1u);
            d->note[d->n++] = n;
            ui.cursor = (uint8_t)to;
            ui.pr_focus = d->n;
        }
        ui.bank = (uint8_t)(ui.cursor / 16u);
        ui.entry_open = 0;
        break;
    }
    default: {                                          /* LEN: the chord's TIEs */
        uint32_t l = note_len(t, c, len);
        if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); break; }
        if (steps > 0) {
            uint32_t nx = (c + l) % len;
            if (l >= len || l >= 16u) break;
            if (t->step[nx].time == ST_NOTE && (t->step[nx].n || t->step[nx].hit)) { ui_message("BLOCKED: A NOTE AFTER"); break; }
            memset(&t->step[nx], 0, sizeof t->step[nx]);
            t->step[nx].time = ST_TIE;
            t->step[nx].vel = st->vel;
        } else if (l > 1u) {
            step_t *e = &t->step[(c + l - 1u) % len];
            memset(e, 0, sizeof *e);
            e->time = ST_REST;
        }
        break;
    }
    }
}

/* (0.6.88) SEQ > TIMING: KNOB 1 the step, KNOB 2 its OFFSET off the grid (-8 .. +7 sixteenths: early / late; a step
 * with nothing to play keeps 0), KNOB 3 the track's REC Q (how much live recording snaps to the grid). EDIT: back on it */
static void timing_edit(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    if (slot == 0u) { cursor_set(ui.cursor + steps); return; }
    if (slot == 1u) {
        if (st->time != ST_NOTE || (!st->n && !st->hit)) { ui_message("NO NOTE HERE"); return; }
        ST_MICRO_SET(st, clamp(ST_MICRO(st) + (steps > 0 ? 1 : -1), -8, 7));
        return;
    }
    if (slot == 2u)
        TSEL->p[P_HRQ] = (int16_t)clamp(TSEL->p[P_HRQ] + steps, 0, 127);
}
static void step_edit(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    uint32_t i;
    if (cur_page()->id[0] == 8u) {                      /* (0.6.88) SEQ > TIMING (a DRUM track's too) */
        timing_edit(slot, steps);
        return;
    }
    if (drum_track(TSEL)) {
        grid_edit(slot, steps);
        return;
    }
    if (cur_page()->id[0] == 4u) {                      /* (0.6.81) SEQ > NOTE */
        note_edit(slot, steps);
        return;
    }
    switch (slot) {
    case 0:                                               /* STEP: the cursor */
        cursor_set(ui.cursor + steps);
        break;
    case 1:                                               /* NOTE: transpose the step */
        if (!st->n) {
            st->note[0] = last_note;
            st->n = 1;
            st->time = ST_NOTE;
            break;
        }
        for (i = 0; i < st->n; i++)
            st->note[i] = (uint8_t)clamp(st->note[i] + steps, 1, 127);
        st->time = ST_NOTE;
        last_note = st->note[0];
        break;
    case 2:
        st->time = (uint8_t)clamp((int32_t)st->time + (steps > 0 ? 1 : -1), ST_NOTE, ST_REST);
        break;
    default: {                                            /* FLAG: - / ACC / SLD / A+S */
        uint32_t f = (st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u);
        f = (uint32_t)clamp((int32_t)f + (steps > 0 ? 1 : -1), 0, 3);
        st->flags = (uint8_t)((st->flags & ~(SF_ACCENT | SF_SLIDE)) | (f & 1u ? SF_ACCENT : 0u) | (f & 2u ? SF_SLIDE : 0u));
        break;
    }
    }
}

/* (0.6.39) SEQ > COMP: KNOB 1 the step, KNOB 2 its TYPE (OFF, the step's CHANCE and RATCH, the components), KNOB 3 / 4
 * the TYPE's values (CHANCE %, RATCH x1..x4; ARP: ORDER, RATE x RANGE; TRIG: the condition). CHANCE and RATCH are the
 * step's own fields; a component is one at a time: turning to OFF, CHANCE or RATCH takes it away. (0.6.82) COMP 2: the
 * step's second component the same way (no CHANCE or RATCH there); RATCH and ECHO: KNOB 4 their DIV. WHEN: KNOB 2 / 3
 * the loops each component plays on (ALWAYS, 1:2, 2:2, SKIP), KNOB 4 the step's LEN */
static void comp_knob(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    uint32_t v = comp_view(), k = comp_slot();
    int32_t d = clamp(steps, -4, 4), e = steps > 0 ? 1 : -1;
    uint8_t *cp = k == 1u ? &st->comp2 : &st->comp, *ap = k == 1u ? &st->carg2 : &st->carg;
    if (slot == 0u) { cursor_set(ui.cursor + steps); return; }
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    if (k == 2u) {
        if (slot <= 2u) {
            uint32_t sh = (slot - 1u) * 2u, w = (uint32_t)clamp((int32_t)CW_OF(st, slot - 1u) + e, 0, 3);
            st->cwhen = (uint8_t)((st->cwhen & ~(3u << sh)) | w << sh);
        } else
            st->ctime = (uint8_t)((st->ctime & 7u) | (uint32_t)clamp((int32_t)CT_LEN(st) + e, 0, 15) << 3);
        return;
    }
    if (slot == 1u) {
        v = (uint32_t)clamp((int32_t)v + d, 0, COMP_N - 1);
        if (k && v > COMP_OFF && v < COMP_ARPN)         /* (COMP 2: no CHANCE or RATCH) */
            v = d > 0 ? COMP_ARPN : COMP_OFF;
        if (!k) ui.comp_sel = (uint8_t)v;
        if (v >= COMP_ARPN) {
            uint32_t c = CP_ARPN + v - COMP_ARPN;
            if (*cp != c) {
                int keep = cp_isarp(*cp) && cp_isarp(c);   /* (ARP to ARP: ORDER, RATE, RANGE kept) */
                *cp = (uint8_t)c;
                if (!keep) *ap = c <= CP_ARPS ? CP_ARG(0u, 2u, 0u) : CP_ADEF[c];   /* (UP x4 1 OCT; the others' own) */
            }
        } else
            *cp = *ap = 0;
        return;
    }
    if ((v == COMP_RATCH || v == COMP_ECHO) && slot == 3u)   /* (0.6.82) DIV */
        st->ctime = (uint8_t)((st->ctime & ~7u) | (uint32_t)clamp((int32_t)CT_DIV(st) + e, 0, CT_NDIV - 1));
    else if (v == COMP_CHANCE && slot == 2u)
        step_set_chance(st, (uint32_t)clamp((int32_t)step_chance(st) + steps, 0, 100));
    else if (v == COMP_RATCH && slot == 2u)
        step_set_ratchet(st, (uint32_t)clamp((int32_t)step_ratchet(st) + d, 1, 4));
    else if (*cp == CP_JUMP && slot == 2u)            /* (0.6.62) TO STEP (its TIMES kept) */
        *ap = (uint8_t)((*ap & 0xC0u) | (uint32_t)clamp((int32_t)CP_JTO(*ap) + steps, 0, 63));
    else if ((*cp == CP_JUMP || *cp == CP_REV) && slot == 3u) {   /* (0.6.62) TIMES */
        uint32_t m = (uint32_t)clamp((int32_t)CP_TIMES(*cp, *ap) + e, 0, 3);
        *ap = (uint8_t)(*cp == CP_JUMP ? (*ap & 63u) | m << 6 : m);
    } else if (v >= COMP_TRIG && slot == 2u && *cp > CP_ARPS && CP_AMAX[*cp])   /* TRIG .. PULSE: their value */
        *ap = (uint8_t)clamp((int32_t)*ap + d, CP_AMIN[*cp], CP_AMAX[*cp]);
    else if (v >= COMP_ARPN && v <= COMP_ARPS && slot == 2u)
        *ap = CP_ARG((uint32_t)clamp((int32_t)CP_ORD(*ap) + d, 0, 3), CP_RATE(*ap), CP_RNG(*ap));
    else if (v >= COMP_ARPN && v <= COMP_ARPS && slot == 3u) {   /* RATE x RANGE: one list, 1 range after another */
        int32_t i = clamp((int32_t)(CP_RATE(*ap) % CP_NRATE * 4u + CP_RNG(*ap)) + d, 0, CP_NRATE * 4 - 1);
        *ap = CP_ARG(CP_ORD(*ap), (uint32_t)i / 4u, (uint32_t)i % 4u);
    }
}
/* (0.6.85) SEQ > STRETCH: KNOB 1 STRETCH /8 .. /2, x2 .. x8; KNOB 2 REPEAT x2 .. x8, TO 64 (turning one picks it); OCT+
 * does the picked one to the selected track (UNDO: GLO + G4); KNOB 3 NUDGE: the whole sequence a step left / right now.
 * ui_tools.c tl_stretch, tl_repeat, tl_shift */
static uint32_t tl_len(const track_t *t);
static void tl_take(const track_t *t);
static void tl_shift(track_t *t, uint32_t len, uint32_t d);
static uint32_t tl_stretch(track_t *t, int32_t k);
static uint32_t tl_repeat(track_t *t, uint32_t k);
static void shape_knob(uint32_t slot, int32_t steps)
{
    int32_t d = steps > 0 ? 1 : -1;
    if (slot == 0u) { shp_i = (uint8_t)clamp((int32_t)shp_i + d, 0, 13); ui.act = 1; }
    else if (slot == 1u) { shp_r = (uint8_t)clamp((int32_t)shp_r + d, 0, 7); ui.act = 2; }
    else if (slot == 2u) {
        track_t *t = TSEL;
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        tl_take(t);
        tl_shift(t, tl_len(t), d > 0 ? 1u : tl_len(t) - 1u);
        ui_message(d > 0 ? "NUDGED RIGHT" : "NUDGED LEFT");
    }
}
static void edit_param(uint32_t slot, int32_t steps)
{
    int16_t *vp;
    const page_t *pg = cur_page();
    const param_desc_t *d;
    int32_t v;
    if (pg->graph == GR_COMP) {
        comp_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_SHAPE) {                        /* (0.6.85) STRETCH, REPEAT (picked, OCT+ does it), NUDGE (now) */
        shape_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_FUGUE) {                        /* (0.6.52) HEAD, RATE, DIR, INTERVAL */
        fg_set_t *f = &fg_set[song.sel % NTRK][fg_ui % FG_HEADS];
        int32_t d = steps > 0 ? 1 : -1;
        if (slot == 0u) fg_ui = (uint8_t)clamp((int32_t)fg_ui + d, 0, FG_HEADS - 1);
        else if (slot == 1u) f->rate = (uint8_t)clamp((int32_t)f->rate + d, 0, FG_NRATE - 1);
        else if (slot == 2u) f->dir = (uint8_t)clamp((int32_t)f->dir + d, 0, 3);
        else f->ivl = (int8_t)clamp((int32_t)f->ivl + d, -14, 14);
        return;
    }
    if (pg->graph == GR_CHANCE) {
        if (slot == 0u) cursor_set(ui.cursor + steps);
        else if (slot == 1u || slot == 2u) {
            if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
            step_t *st = &TSEL->step[ui.cursor];
            if (slot == 1u)
                step_set_chance(st, (uint32_t)clamp((int32_t)step_chance(st) + steps, 0, 100));
            else                                      /* RATCH x1..x4 */
                step_set_ratchet(st, (uint32_t)clamp((int32_t)step_ratchet(st) + steps, 1, 4));
        }
        return;
    }
    if (pg->graph == GR_DXSND) {                          /* EDIT > SOUND: the DRUM-X kit (ui_dx.c) */
        dx_knob(pg, slot, steps);
        return;
    }
    if (DX_KIT3(pg)) {                                    /* (0.6.54) KIT 3: BP, FEEDBK (0.6.58: IMAGE on STEREO) */
        dx_kit3_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_EVENTS) {                         /* AUTO LIST (ui_events.c) */
        ev_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_MOTION) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (slot == 0u) motion_set_enabled(TSEL, steps > 0);
        else if (slot == 3u) ui.act = steps > 0 ? 4u : 0u;
        return;
    }
    if (pg->graph == GR_SONG) {
        uint32_t end = song_end_row();                  /* (0.6.22) the rows, + ADD, then the song's END */
        if (slot == 0u) {
            ui.song_row = (uint8_t)clamp((int32_t)ui.song_row + steps, 0, (int32_t)end);
            return;
        }
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (ui.song_row >= end) {                       /* END: KNOB 2 STOP / x2 .. x8 / LOOP (the song's times) */
            if (slot == 1u) {
                SONG_END(&chain_config) = (uint8_t)song_end_step(SONG_END(&chain_config), steps > 0 ? 1 : -1);
                song_idx_touch();
            }
            return;
        }
        if (slot == 3u) {                     /* (JIANT) SCENE: right stores the state now (mutes, macros, punch-in), */
            scene_t *sc = &song_idx.scene[song_cur][ui.song_row % CHAIN_ROWS];   /* left takes it away */
            if (ui.song_row >= chain_config.count) { ui_message("NO ROW"); return; }
            if (steps > 0) { scene_capture(sc); ui_message("SCENE STORED"); }
            else if (sc->on) { memset(sc, 0, sizeof *sc); ui_message("SCENE CLEARED"); }
            song_idx_touch();
            return;
        }
        if (ui.song_row >= chain_config.count) {
            chain_row_t *r = &chain_config.row[ui.song_row];
            r->slot = ui.song_row ? chain_config.row[ui.song_row - 1u].slot : 0u;
            r->bars = 1;
            chain_config.count = ui.song_row + 1u;
            if (slot == 1u) return;
        }
        if (slot == 1u && steps < 0 && chain_config.row[ui.song_row].slot == 0u) {
            confirm_open(CF_DEL_ROW, ui.song_row);      /* (0.6.22) SECT left of A: DELETE ROW? (the dialog) */
            return;
        }
        if (slot == 1u)
            chain_config.row[ui.song_row].slot = (uint8_t)clamp((int32_t)chain_config.row[ui.song_row].slot + steps, 0, 3);
        if (slot == 2u)
            chain_config.row[ui.song_row].bars = (uint8_t)clamp((int32_t)chain_config.row[ui.song_row].bars + steps, 1,
                                                                (int32_t)CHAIN_BARS);
        return;
    }
    if (chain_busy() && (pg->scope == SC_STEP || pg->graph == GR_STEPS ||
        0)) {
        ui_message("STOP TO EDIT"); return;
    }
    if (pg->scope == SC_STEP) {
        step_edit(slot, steps);
        return;
    }
    if (pg->scope == SC_TRK) {
        tracks_edit(slot, steps);
        return;
    }
    if (pg->graph == GR_BROWSE) {                         /* KNOB 1: one preset, KNOB 2: the next / previous engine */
        if (slot == 0u) {
            preset_step(steps);
        } else if (slot == 1u) {
            select_engine(eng_step(TSEL->eng_req, steps));
        } else if (slot == 2u) {
            preset_mark(steps > 0);
        } else if (slot == 3u && favorites.filter != (uint32_t)(steps > 0)) {
            favorites.filter = steps > 0;
            ui.force = 1;
            settings_save();
        }
        return;
    }
    if (pg->graph == GR_TOOLS) {                          /* (0.6.25) TOOLS: a list, any knob (and PRESETS) moves the
                                                         * pick down / up; OCT+ does it (its dialog first) */
        ui.act = steps < 0 && ui.act <= 1u ? 0u : (uint8_t)clamp((int32_t)ui.act + (steps > 0 ? 1 : -1), 1, 6);   /* (left
                                                         * past the first: none picked) */
        return;
    }
    if ((act_cols() >> slot) & 1u) {                      /* an action's knob picks it (right) or drops it (left); */
        if (pg->graph != GR_PATS)                         /* OCT+ does it (act_do) */
            ui.act = steps > 0 ? (uint8_t)(slot + 1u) : ui.act == slot + 1u ? 0u : ui.act;
        return;
    }
    if (pg->graph == GR_SLOTS && slot == 0u) {            /* (0.6.12) KNOB 1 the project: its sections, the music kept */
        int32_t n = clamp((int32_t)song_cur + (steps > 0 ? 1 : -1), 0, (int32_t)NSONG - 1);
        if ((uint32_t)n != song_cur) (void)song_open((uint32_t)n);
        return;
    }
    if (pg->graph == GR_USER) {                           /* KNOB 1 the slot */
        if (slot == 0u)
            ui.uslot = (uint8_t)clamp((int32_t)ui.uslot + steps, 0, UP_SLOTS - 1);
        return;
    }
    if (pg->graph == GR_PATS) {                          /* KNOB 1 the pattern */
        if (slot == 0u)
            ui.ppick = (uint8_t)clamp((int32_t)pat_pick() + steps, 0, (int32_t)pat_count() - 1);
        return;
    }
    if (pg->graph == GR_SLPIT && slot == 0u) {           /* (JIANT 0.6.3) SL PITCH: KNOB 1 the step, 2 its pitch */
        sl_ui_step = (uint8_t)clamp((int32_t)sl_ui_step + steps, 0, 15);
        return;
    }
    if (pg->graph == GR_MSEQ && slot == 0u) {            /* (JIANT) MSEQ: KNOB 1 the step, 2 its level */
        ms_ui_step = (uint8_t)clamp((int32_t)ms_ui_step + steps, 0, 15);
        return;
    }
    if (fop_page(pg->graph)) {                           /* (0.6.10) FM6's operators: KNOB 1 OP, 2..4 its bytes (ui_fm6op.c) */
        fop_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_MOD && slot == 0u) {             /* MOD: KNOB 1 the slot, 2..4 its SRC DST AMT */
        mod_ui_slot = (uint8_t)clamp((int32_t)mod_ui_slot + (steps > 0 ? 1 : -1), 0, 3);
        return;
    }
    d = page_desc(pg, slot, &vp);
    if (!d || !vp || d->max == d->min)
        return;
    v = param_turn(d, *vp, accel(EN_K1 + slot, steps, d->fmt == F_ENUM ? 0 : d->max - d->min));
    *vp = (int16_t)v;
    if (vp == &trks_ui) {                               /* (0.6.18) GLO > SYSTEM's TRKS: the MENU's setting, saved */
        trks_put(v);
        ui_message(v ? "6 TRACKS: MORE CPU" : "4 TRACKS: T5-T6 ASLEEP");
        settings_save();
        return;
    }
    if (vp == &load_ui) {                               /* (0.6.30) GLO > SYSTEM's LOAD: the same */
        load_put(v);
        ui_message(v == 2 ? "LOAD FULL: ALL FX" : v ? "LOAD LITE: LIGHTER FX" : "LOAD ECO: NO SLICER/CHORUS");
        settings_save();
        return;
    }
    if (pg->scope != SC_GLOBAL) motion_capture_ui(TSEL, (uint32_t)(vp - TSEL->p), *vp);
}

/* OCT+ on an action page: the picked action. A load stays picked (browse and load again); the others
 * are dropped once done. Flash writes only while stopped; over the user's data: the dialog */
static void act_do(void)
{
    uint32_t c = act_col(), id, k = (uint32_t)song.g[G_SLOT] - 1u;
    if (!c--)
        return;
    if (cur_page()->graph == GR_MOTION) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        confirm_open(CF_CLEAR_MOTION, song.sel);
        return;
    }
    if (cur_page()->graph == GR_EVENTS) {
        ev_oct();
        return;
    }
    if (cur_page()->graph == GR_SHAPE) {                  /* (0.6.85) STRETCH / REPEAT, the music before kept (UNDO) */
        track_t *t = TSEL;
        uint32_t n, was = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        char m[24];
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (c == 1u && was >= NSTEP) { ui_message("LEN 64: NO ROOM"); return; }
        undo_mark();
        n = c == 0u ? tl_stretch(t, shp_k()) : tl_repeat(t, shp_r < 7u ? shp_r + 2u : 0u);
        if (!n) { ui_message(c == 1u ? "LEN 64: NO ROOM" : "NOTHING TO DO"); return; }
        str_cpy(m, "LEN ", sizeof m); fmt_int(m + 4, (int32_t)was); str_cpy(m + str_len(m), " > ", 4);
        fmt_int(m + str_len(m), (int32_t)n);
        if (c == 0u && shp_k() > 0 && was * (uint32_t)shp_k() > NSTEP) str_cpy(m + str_len(m), " CUT", 5);
        ui_message(m);
        return;
    }
    if (cur_page()->graph == GR_TOOLS) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (!act_ready()) {                               /* nothing there to clear or delete */
            ui_message(c == 2u ? "NOTHING TO DELETE" : "NOTHING TO CLEAR");
            return;
        }
        confirm_open(c == 0u ? CF_CLEAR_SEQ : c == 1u ? CF_INIT_SOUND : c == 2u ? CF_DEL_ROW : c == 4u ? CF_INIT_SYS :
                     c == 5u ? CF_CPU_TEST : CF_CLEAR_SONG, c == 2u ? ui.song_row : song.sel);
        return;
    }
    if (cur_page()->graph == GR_SONG) {
        if (song.playing || chain_busy() || seq_counting()) transport_req = 2;
        else chain_play_ui();
        return;
    }
    if (cur_page()->graph == GR_PATS) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (pat_needs_confirm(TSEL))                      /* the user's steps: the dialog */
            confirm_open(CF_LOAD_PAT, song.sel);
        else {                                            /* empty, or a pattern loaded and untouched */
            undo_mark();
            pat_load_ui(TSEL, pat_pick());
        }
        return;
    }
    if (cur_page()->graph == GR_USER) {                   /* 1 LOAD, 2 ERASE, 3 SAVE (the NAME screen first) */
        if (c > 1u)
            ui.act = 0;
        if (c == 2u && up_used(ui.uslot) && !transport_busy())
            confirm_open(CF_ERASE_USER, ui.uslot);        /* ERASE: the dialog first */
        else if (c != 3u)
            up_ui(c - 1u, ui.uslot);
        else if (transport_busy())
            ui_message("STOP TO SAVE");
        else if (up_used(ui.uslot))
            confirm_open(CF_OVR_USER, ui.uslot);
        else
            name_open(NK_USER_SAVE, ui.uslot);
        return;
    }
    id = cur_page()->id[c & 3u];
    if (id != G_LOAD)
        ui.act = 0;
    switch (id) {
    case G_LOAD:
        undo_mark();                                      /* (0.6.21) UNDO: the music before the load */
        project_load(k);
        break;
    case G_SAVE:                                          /* the NAME screen writes it */
        if (transport_busy())
            ui_message("STOP TO SAVE");
        else if (project_used(k))
            confirm_open(CF_OVR_PROJ, k);
        else
            name_open(NK_PROJ_SAVE, k);
        break;
    case G_CLRSEQ:
        if (chain_busy()) { ui_message("STOP TO EDIT"); break; }
        confirm_open(CF_CLEAR_SEQ, song.sel);
        break;
    default:                                              /* G_INITSND */
        set_engine(TSEL->eng_req);                        /* engine defaults + its first preset (the steps stay) */
        ui_message("SOUND INIT");
        ui.force = 1;
        break;
    }
}

/* OCT- / OCT+ where they answer (the dialogs, the menu, action pages): on release, and only a press
 * that began there; both down together (UPDATE MODE, main.c) is no tap. Bit 0 OCT-, bit 1 OCT+ */
static uint32_t oct_taps(uint32_t pressed, int here)
{
    static uint8_t down, chord;
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP];
    uint32_t now = ((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1, tap;
    if (here)
        down |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (now == 3u)
        chord = 1;
    tap = down & ~now;
    down &= (uint8_t)now;
    if (chord) {
        tap = 0;
        chord = now != 0u;
    }
    return tap;
}

/* (1.1.1-GW, issue #2: "the values keep changing by themselves") a knob turned with the track armed: as before, its
 * move recorded (motion_capture); the first time a parameter gets automation on the track the header says so, and
 * which REC chord clears it ("AUTOMATED. REC+ENV CLR"; the card shows an "A" from then on, ui_draw.c draw_column) */
static void motion_capture_ui(track_t *t, uint32_t id, int16_t v)
{
    int fresh = rec_on(t) && t == TSEL && motion_param(id) && !motion_has(t, id);
    motion_capture(t, id, v);
    if (fresh && motion_has(t, id)) {
        static const uint8_t BB[] = {B_FX, B_EDIT, B_ENV, B_LFO, B_SCL, B_ARP};
        uint32_t i, k, f, b = 6u;
        if (id == P_LEVEL || id == P_PAN)
            b = 7u;                                     /* (REC + GLO: the levels and pans) */
        for (i = 0; i < NPAGES && b == 6u; i++)        /* (the part whose REC chord clears it: rec_clear_auto's map) */
            for (k = 0; k < 4u; k++)
                if (PAGES[i].id[k] == id && (PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_ENGINE))
                    for (f = 0; f < 6u; f++)
                        if (FAM_BTN[PAGES[i].fam] == BB[f] && !(f == 1u && id == P_DIST)) { b = f; break; }
        if (b == 6u && id == P_DIST)
            b = 0u;
        ui_say("AUTOMATED. REC+", b < 6u ? (b == 0u ? "FX CLR" : b == 1u ? "EDIT CLR" : b == 2u ? "ENV CLR" :
                                           b == 3u ? "LFO CLR" : b == 4u ? "SCL CLR" : "ARP CLR")
                                         : b == 7u ? "GLO CLR" : "PAGE CLR");
    }
}
/* (JIANT) REC held, then a page button: that part's automation goes, both kinds (the knob moves and the
 * step locks), the stored values stay. FX: the selected track's FX and SLICER and the punch-in lane; EDIT ENV LFO SCL
 * ARP: the selected track's parameters of those pages; GLO: every track's LEVEL and PAN. No dialog (SEQ + REC, which
 * clears every sequence, asks). The other order is REC in a layer: it arms, as always (FX: to record the punch-in lane) */
static int rec_combo_btn(uint32_t b)
{
    return b == B_FX || b == B_EDIT || b == B_ENV || b == B_LFO || b == B_SCL || b == B_ARP || b == B_GLO;
}
/* REC held, then page button b pressed: REC's tap and layer dead (1 = rec_clear_auto's) */
static int rec_first(uint32_t b)
{
    if (!rec_combo_btn(b) || ui.menu || ui.confirm || !((fm1_in.buttons >> panel.btn[B_REC]) & 1u))
        return 0;
    ui.rec_t0 |= 2u;
    if (ui.ly == LAYER_REC)
        ui.ly_t0 |= 8u;                                 /* (LY_DEAD: no REC map, no arming tap) */
    return 1;
}
static void rec_clear_auto(uint32_t b)
{
    uint32_t m[(P_COUNT + 31u) / 32u], i, f, n = 0, k;
    char msg[28];
    const char *name = b == B_FX ? "FX" : b == B_EDIT ? "EDIT" : b == B_ENV ? "ENV" : b == B_LFO ? "LFO" :
                       b == B_SCL ? "SCL" : b == B_ARP ? "ARP" : "MIX";
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    memset(m, 0, sizeof m);
    if (b == B_GLO) {
        m[P_LEVEL / 32u] |= 1u << (P_LEVEL % 32u);
        m[P_PAN / 32u] |= 1u << (P_PAN % 32u);
    } else {
        for (f = FAM_HOME + 1u; f < FAM_COUNT && FAM_BTN[f] != b; f++)
            ;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == f && (PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_ENGINE))
                for (k = 0; k < 4u; k++)
                    if (PAGES[i].id[k] < P_COUNT && !(f == FAM_EDIT && PAGES[i].id[k] == P_DIST))   /* (EDIT > FILTER's
                                                         * DRV is the part's DIST: FX's) */
                        m[PAGES[i].id[k] / 32u] |= 1u << (PAGES[i].id[k] % 32u);
    }
    for (k = 0; k < NTRK; k++)
        if (b == B_GLO || k == song.sel)
            n += motion_clear_ids(&trk[k], m);
    if (b == B_FX)
        for (i = 0; i < sizeof pfx_lane; i++) {
            n += pfx_lane[i] != 0;
            pfx_lane[i] = 0;
        }
    str_cpy(msg, n ? "" : "NO ", sizeof msg);
    str_cpy(msg + str_len(msg), name, sizeof msg - str_len(msg));
    str_cpy(msg + str_len(msg), n ? " AUTOMATION CLEARED" : " AUTOMATION", sizeof msg - str_len(msg));
    ui_message(msg);
    ui.force = 1;
}

/* SEQ step entry = step recording (1.2, Discussion #133: only with the track armed and the transport stopped), acid
 * style: the keys pressed together (POLY: up to 4 notes, MONO: the last one) become the cursor step; releasing all
 * keys moves on one step (wrapping inside LEN) */
/* (0.6.37) the entry let go: a chord held for LEN steps (TIEs after it), or arpeggiated (ARP pressed while held): its
 * notes over the octaves in the ORDER, one every RES steps; the cursor after it */
static const char *const ENT_ORD[4] = {"UP", "DOWN", "UP-DN", "RND"};
static const char *const ENT_RES[4] = {"1/16", "1/8", "1/8.", "1/4"};
static void ent_message(void)
{
    char a[24], b[24];
    if (ui.ent_arp) {
        str_cpy(a, "ARP ", sizeof a); str_cpy(a + str_len(a), ENT_ORD[ui.ent_ord & 3u], 8);
        str_cpy(b, ENT_RES[(ui.ent_res - 1u) & 3u], 8); str_cpy(b + str_len(b), " x", 4);
        fmt_int(b + str_len(b), ui.ent_oct); str_cpy(b + str_len(b), " OCT", 6);
    } else {
        str_cpy(a, "LEN ", sizeof a); fmt_int(a + str_len(a), ui.ent_len);
        str_cpy(a + str_len(a), ui.ent_len > 1u ? " STEPS" : " STEP", 8);
        str_cpy(b, "ARP: ARPEGGIATE", sizeof b);
    }
    str_cpy(ui.msg, a, sizeof ui.msg); str_cpy(ui.msg2, b, sizeof ui.msg2); ui.msg_t = 40;
}
static void ent_knob(uint32_t k, int32_t s)
{
    int32_t d = clamp(s, -4, 4);                        /* (detents, not accelerated: small ranges) */
    if (k == 0u && !ui.ent_arp) ui.ent_len = (uint8_t)clamp(ui.ent_len + d, 1, TSEL->p[P_SLEN] > 16 ? 16 : TSEL->p[P_SLEN]);
    else if (k == 0u) ui.ent_res = (uint8_t)clamp(ui.ent_res + d, 1, 4);
    else if (k == 1u) ui.ent_ord = (uint8_t)clamp(ui.ent_ord + d, 0, 3);
    else ui.ent_oct = (uint8_t)clamp(ui.ent_oct + d, 1, 3);
    ent_message();
}
static void ent_commit(void)
{
    track_t *t = TSEL;
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, cur = ui.cursor, i, j, n = 0, span;
    uint8_t vel = t->step[cur].vel, list[24];
    if (!ui.ent_arp || !ui.ent_nn) {
        span = ui.ent_len < len ? ui.ent_len : len;
        for (i = 1; i < span; i++) {
            step_t *s = &t->step[(cur + i) % len];
            memset(s, 0, sizeof *s);
            s->time = ST_TIE; s->vel = vel;
        }
        cursor_set((int32_t)(cur + (span ? span : 1u)));
        return;
    }
    for (j = 0; j < ui.ent_oct; j++)                    /* the notes over the octaves, low to high */
        for (i = 0; i < ui.ent_nn; i++)
            if (ui.ent_notes[i] + 12u * j <= 127u) list[n++] = (uint8_t)(ui.ent_notes[i] + 12u * j);
    if (ui.ent_ord == 1u)                               /* DOWN */
        for (i = 0; i < n / 2u; i++) { uint8_t x = list[i]; list[i] = list[n - 1u - i]; list[n - 1u - i] = x; }
    else if (ui.ent_ord == 2u && n > 2u)                /* UP-DN: back down without the ends twice */
        for (i = n - 2u; i > 0u && n < sizeof list; i--) list[n++] = list[i];
    else if (ui.ent_ord == 3u)                          /* RND */
        for (i = n; i > 1u; i--) { uint32_t r = rng() % i; uint8_t x = list[i - 1u]; list[i - 1u] = list[r]; list[r] = x; }
    if (n * ui.ent_res > len) n = len / ui.ent_res ? len / ui.ent_res : 1u;
    span = n * ui.ent_res;
    for (i = 0; i < span; i++) {
        step_t *s = &t->step[(cur + i) % len];
        memset(s, 0, sizeof *s);
        s->vel = vel;
        if (i % ui.ent_res == 0u) { s->time = ST_NOTE; s->n = 1; s->note[0] = list[i / ui.ent_res]; }
        else s->time = ST_TIE;
    }
    cursor_set((int32_t)(cur + span));
}
static void ent_note(track_t *t, step_t *st, uint32_t note)   /* one note into the entry (the step, the arpeggio's) */
{
    uint32_t j;
    if (!ui.entry_open) {
        ui.entry_open = 1;
        st->n = 0;
        st->time = ST_NOTE;
        ui.ent_len = ui.ent_res = ui.ent_oct = 1; ui.ent_arp = ui.ent_ord = ui.ent_nn = 0;
    }
    for (j = 0; j < ui.ent_nn && ui.ent_notes[j] < note; j++)   /* (the arpeggio's notes, in order of pitch) */
        ;
    if (ui.ent_nn < sizeof ui.ent_notes && (j == ui.ent_nn || ui.ent_notes[j] != note)) {
        uint32_t m;
        for (m = ui.ent_nn; m > j; m--) ui.ent_notes[m] = ui.ent_notes[m - 1u];
        ui.ent_notes[j] = (uint8_t)note; ui.ent_nn++;
    }
    if (t->p[P_VOICE] && !ENGINES[t->engine]->oneshot) {   /* (drums: hits stack as a chord) */
        st->note[0] = (uint8_t)note;
        st->n = 1;
    } else if (st->n < 4u) {
        for (j = 0; j < st->n && st->note[j] != note; j++)
            ;
        if (j == st->n)
            st->note[st->n++] = (uint8_t)note;
    }
}
static void seq_entry(uint32_t pressed)
{
    track_t *t = TSEL;
    step_t *st = &t->step[ui.cursor];
    uint32_t k, ti = song.sel % NTRK;
    for (k = 0; k < 27u; k++) {
        uint32_t note, c, nc;
        uint8_t cn[KB_CH];
        if (!((pressed >> k) & 1u))
            continue;
        note = kb_map(t, k);
        if (note == KB_SILENT)
            continue;
        if (kb_sn[k] && kb_trk[k] == ti) {             /* (0.6.83) SCL > CHORD: the chord the key sounds (seq.c key_on), */
            nc = kb_sn[k];                              /* all of it into the step, as live recording does */
            memcpy(cn, kb_snd[k], KB_CH);
        } else {                                        /* (the key not in yet: built here, LEAD's memory as it was) */
            uint8_t last[KB_CH], nl = chord_nlast[ti];
            memcpy(last, chord_last[ti], KB_CH);
            nc = chord_build(t, note, cn);
            memcpy(chord_last[ti], last, KB_CH);
            chord_nlast[ti] = nl;
        }
        for (c = 0; c < nc && c < KB_CH; c++)
            ent_note(t, st, cn[c]);
        last_note = (uint8_t)note;
    }
    if (ui.entry_open && !(fm1_in.notes & ~kb_layer))
        ent_commit();                                   /* (let go: LEN / the arpeggio written, the cursor on) */
}

/* Discussions #92 / #94: the PRESETS knob by page (the menu, a dialog, NAME and the layers: ui_input before this).
 *   SEQ pages that show the steps (STEP and the DRUM grid, PATTERN, CHANCE, MOTION): the step cursor, as STEP's KNOB 1
 *     (a sound load there would drop the track's motion: never)
 *   the list / action pages USER, PROJECT, PHRASES, SONG: the selection, as their KNOB 1; TOOLS: nothing
 *   HOME, PRESETS and every other page (EDIT, ENV, LFO, FX, SCL, ARP, MIXER, GLOBAL, ...): the selected track's
 *     sound, one list over every engine, then the used user presets (preset_step). A sound load keeps the steps
 *     . Up to 1.0.3 it did that on HOME and PRESETS only */
static void presets_turn(int32_t s)
{
    const page_t *pg = cur_page();
    uint32_t g = pg->graph;
    if (ui.home || g == GR_BROWSE) {
        preset_step(s);
    } else if (g == GR_ROLL && (pg->id[0] == 4u || pg->id[0] == 8u)) {   /* (0.6.81) SEQ > NOTE (KNOB 1 FOCUS), TIMING:
                                                         * PRESETS the step */
        cursor_set(ui.cursor + s);
        ui.hot_col = 0;
        ui.hot_t = 40;
    } else if (g == GR_ROLL || g == GR_CHANCE || g == GR_COMP || g == GR_USER || g == GR_SLOTS || g == GR_PATS || g == GR_SONG ||
               g == GR_EVENTS || g == GR_TOOLS) {
        uint32_t c = g == GR_SLOTS ? 1u : 0u;             /* (0.6.12 PROJECT: the list is the sections, KNOB 2's) */
        edit_param(c, s);                                 /* KNOB 1's (STEP: STOP TO EDIT while a song plays) */
        ui.hot_col = (uint8_t)c;
        ui.hot_t = 40;
    } else if (pg->fam == FAM_SEQ && (g == GR_STEPS || g == GR_MOTION)) {
        cursor_set(ui.cursor + s);                        /* (PATTERN draws it; MOTION: STEP shows it) */
    } else if (g != GR_TOOLS) {
        preset_step(s);
    }
}

/* (JIANT 0.5) SELECT off HOME: the selected track's sound diced. DRUM-X: both patches (A, B) of every sound, each
 * value moved up to +-40 from the factory kit's (COLOR up 12 at most: no knife edges), the wave one time in five
 * SINE or FM; a synth: one of its engine's
 * factory presets, then each engine parameter moved up to a quarter of its range (an ENUM: another value, one time in
 * three). Musical by construction: always near a sound that works */
static int32_t dice_by(int32_t v, int32_t lo, int32_t hi, int32_t span)
{
    return clamp(v + (int32_t)(rng() % (uint32_t)(2 * span + 1)) - span, lo, hi);
}
static void sound_dice_of(track_t *t, int quiet)
{
    uint32_t e = t->eng_req % NENGINES, i, l;
    if (e == ENGI_DRUM) {
        for (l = 0; l < 8u; l++) {
            const dx_lane_t *D = &DX_KIT_DEF[l];
            dx_lane_t *L = &dx_kit[l];
            for (i = 0; i < DXP_N; i++) {               /* (COLOR: the brightness, down 40 but up 12 at most) */
                int32_t up = i == DXP_COLOR ? 12 : 127;
                L->a[i] = (uint8_t)dice_by(D->a[i], 0, D->a[i] + up < 127 ? D->a[i] + up : 127, 40);
                L->b[i] = (uint8_t)dice_by(D->b[i], 0, D->b[i] + up < 127 ? D->b[i] + up : 127, 40);
            }                                            /* (a new wave: SINE or FM, the gentle two) */
            L->mode = (uint8_t)((D->mode & ~3u) | (rng() % 5u ? (D->mode & 3u) : rng() % 2u));
        }
        for (l = 0; l < 8u; l++) {                      /* (0.6.49) LAYER strong where a note is in the way (snare,
                                                         * hats, rim, bell), light on the kick, clap and tom; FEEDBK
                                                         * one sound in three */
            static const uint8_t LO[8] = {0, 4, 0, 6, 6, 0, 3, 8}, HI[8] = {6, 15, 8, 15, 15, 7, 12, 15};
            uint32_t la = LO[l] + rng() % (HI[l] - LO[l] + 1u), fb = rng() % 3u ? 0u : 1u + rng() % 9u;
            dx_lay[l] = (uint8_t)(la | fb << 4);
        }
        if (!quiet) ui_message("KIT DICED");
        return;
    }
    if (ENGINES[e]->npresets)
        apply_preset_to(t, rng() % ENGINES[e]->npresets);
    for (i = 0; i < 8u; i++) {
        const param_desc_t *d = track_desc(t, P_E0 + i);
        int32_t span = (d->max - d->min) / 4;
        if (!d->label || d->label[0] == '-' || d->max <= d->min)
            continue;
        if (d->fmt == F_ENUM)
            t->p[P_E0 + i] = (int16_t)(rng() % 3u ? t->p[P_E0 + i] : d->min + (int32_t)(rng() % (uint32_t)(d->max - d->min + 1)));
        else
            t->p[P_E0 + i] = (int16_t)dice_by(t->p[P_E0 + i], d->min, d->max, span ? span : 1);
    }
    if (!quiet) ui_message("SOUND DICED");
}
/* (0.6.67) a button held + SELECT: that part of the selected track diced (EDIT the sound, SEQ the sequence, LFO the
 * LFO, FX its effects and the shared reverb and delay), at once so it is heard, under a dialog: OCT+ OK keeps it, OCT-
 * CANCEL brings back what it was before the first dice (only that part: the music plays on), SELECT again dices again.
 * (0.6.65: the sound first.) UNDO (GLO + G4) still brings back what was before */
static const uint8_t DK_LFO[8] = {P_LRATE, P_LWAVE, P_LPHASE, P_LFADE, P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP};
static const uint8_t DK_FXT[9] = {P_DIST, P_DTYPE, P_DTONE, P_CHOR, P_DLY, P_REV, P_APDEP, P_APRATE, P_APWAVE};
static const uint8_t DK_FXG[6] = {G_RTYPE, G_RSIZE, G_RDAMP, G_DTIME, G_DFDBK, G_DPIT};
static struct {                                         /* what CANCEL puts back (the sound: undo_sound) */
    step_t step[NSTEP];
    int16_t slen, v[9], g[6];
    const char *msg;
} dk;
static const char *dice_seq(track_t *t);                /* song_dice.c */
static int32_t dk_rr(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }
static int32_t dk_frac(const param_desc_t *d, int32_t a, int32_t b)   /* a value a% .. b% up the range */
{
    int32_t span = d->max - d->min;
    return dk_rr(d->min + span * a / 100, d->min + span * b / 100);
}
static void dk_keep(uint32_t kind)
{
    track_t *t = TSEL;
    uint32_t i;
    if (kind == CF_DICE_SEQ) {
        for (i = 0; i < NSTEP; i++) dk.step[i] = t->step[i];
        dk.slen = t->p[P_SLEN];
    } else if (kind == CF_DICE_LFO) {
        for (i = 0; i < 8u; i++) dk.v[i] = t->p[DK_LFO[i]];
    } else if (kind == CF_DICE_FX) {
        for (i = 0; i < 9u; i++) dk.v[i] = t->p[DK_FXT[i]];
        for (i = 0; i < 6u; i++) dk.g[i] = song.g[DK_FXG[i]];
    }
}
static int dk_back(uint32_t kind, uint32_t k)           /* CANCEL: 0 done */
{
    track_t *t = &trk[k % NTRK];
    uint32_t i;
    if (kind == CF_DICE_SOUND)
        return undo_sound(k);
    if (kind == CF_DICE_SEQ) {
        load_begin(t, LOAD_PAT);
        for (i = 0; i < NSTEP; i++) t->step[i] = dk.step[i];
        t->p[P_SLEN] = dk.slen;
        load_end(t);
        latch_drop_req |= (uint8_t)(1u << (k % NTRK));
    } else if (kind == CF_DICE_LFO) {
        for (i = 0; i < 8u; i++) t->p[DK_LFO[i]] = dk.v[i];
    } else {
        for (i = 0; i < 9u; i++) t->p[DK_FXT[i]] = dk.v[i];
        for (i = 0; i < 6u; i++) song.g[DK_FXG[i]] = dk.g[i];
    }
    return 0;
}
/* the LFO: its rate in the middle of the range, any wave and phase, a fade now and then; aimed mostly at the filter,
 * a little at the shape, seldom at the pitch or the level (a wobble, not a siren) */
static void lfo_dice(track_t *t)
{
    const param_desc_t *d;
    d = track_desc(t, P_LRATE); t->p[P_LRATE] = (int16_t)dk_frac(d, 25, 80);
    d = track_desc(t, P_LWAVE); t->p[P_LWAVE] = (int16_t)dk_frac(d, 0, 100);
    d = track_desc(t, P_LPHASE); t->p[P_LPHASE] = (int16_t)(rng() % 3u ? d->min : dk_frac(d, 0, 100));
    d = track_desc(t, P_LFADE); t->p[P_LFADE] = (int16_t)(rng() % 3u ? d->min : dk_frac(d, 0, 50));
    d = track_desc(t, P_LD_PIT); t->p[P_LD_PIT] = (int16_t)(rng() % 4u ? 0 : dk_frac(d, 44, 56));
    d = track_desc(t, P_LD_FLT); t->p[P_LD_FLT] = (int16_t)dk_frac(d, d->min < 0 ? 15 : 20, d->min < 0 ? 85 : 70);
    d = track_desc(t, P_LD_SHP); t->p[P_LD_SHP] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, d->min < 0 ? 30 : 0, d->min < 0 ? 70 : 40));
    d = track_desc(t, P_LD_AMP); t->p[P_LD_AMP] = (int16_t)(rng() % 3u ? 0 : dk_frac(d, 0, 35));
}
/* the effects: the track's drive (half the time none), its sends, an AUTOPAN one time in three; the shared reverb's
 * kind and size and the delay's time and feedback, a pitched delay one time in five */
static void fx_dice(track_t *t)
{
    const param_desc_t *d;
    d = track_desc(t, P_DIST); t->p[P_DIST] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, 8, 55));
    d = track_desc(t, P_DTYPE); t->p[P_DTYPE] = (int16_t)dk_frac(d, 0, 100);
    d = track_desc(t, P_DTONE); t->p[P_DTONE] = (int16_t)dk_frac(d, 30, 80);
    d = track_desc(t, P_CHOR); t->p[P_CHOR] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, 15, 70));
    d = track_desc(t, P_DLY); t->p[P_DLY] = (int16_t)dk_frac(d, 0, 65);
    d = track_desc(t, P_REV); t->p[P_REV] = (int16_t)dk_frac(d, 10, 85);
    d = track_desc(t, P_APDEP); t->p[P_APDEP] = (int16_t)(rng() % 3u ? 0 : dk_frac(d, 25, 80));
    d = track_desc(t, P_APRATE); t->p[P_APRATE] = (int16_t)dk_frac(d, 20, 80);
    d = track_desc(t, P_APWAVE); t->p[P_APWAVE] = (int16_t)dk_frac(d, 0, 100);
    song.g[G_RTYPE] = (int16_t)dk_rr(GP[G_RTYPE].min, GP[G_RTYPE].max);
    song.g[G_RSIZE] = (int16_t)dk_rr(GP[G_RSIZE].min + (GP[G_RSIZE].max - GP[G_RSIZE].min) / 3, GP[G_RSIZE].max);
    song.g[G_RDAMP] = (int16_t)dk_rr(GP[G_RDAMP].min, GP[G_RDAMP].max);
    song.g[G_DTIME] = (int16_t)dk_rr(GP[G_DTIME].min, GP[G_DTIME].max);
    song.g[G_DFDBK] = (int16_t)dk_rr(20, 70);
    {   static const int8_t PIT[4] = {-12, -5, 7, 12};
        song.g[G_DPIT] = (int16_t)(rng() % 5u ? 0 : clamp(PIT[rng() % 4u], GP[G_DPIT].min, GP[G_DPIT].max)); }
}
static void part_dice(uint32_t kind)
{
    static uint32_t last_ms;                            /* (once a turn burst, the dialog's turns too) */
    uint32_t again = last_ms && fm1_ms - last_ms <= 400u;
    last_ms = fm1_ms | 1u;
    if (again && ui.confirm == kind)
        return;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (ui.confirm != kind) {
        undo_mark();
        dk_keep(kind);
        confirm_open(kind, song.sel);
    }
    dk.msg = kind == CF_DICE_SEQ ? dice_seq(TSEL) : kind == CF_DICE_LFO ? "LFO DICED" : kind == CF_DICE_FX ? "FX DICED" :
             drum_track(TSEL) ? "KIT DICED" : "SOUND DICED";
    if (kind == CF_DICE_SOUND) sound_dice_of(TSEL, 1);
    else if (kind == CF_DICE_LFO) lfo_dice(TSEL);
    else if (kind == CF_DICE_FX) fx_dice(TSEL);
}
static int dice_kind(uint32_t c) { return c == CF_DICE_SOUND || c == CF_DICE_SEQ || c == CF_DICE_LFO || c == CF_DICE_FX; }
/* (0.6.67) SELECT alone: the pages, one a detent. (0.6.69) Only the section's (the button's) pages, as its taps go,
 * no wrap (its first and last pages are where it stops); on HOME (no pages) the caller dices everything */
static void page_scroll(int32_t s)
{
    uint32_t i = ui.page, j, f;
    int32_t n = s > 0 ? s : -s;
    if (ui.home)
        return;
    f = cur_page()->fam == FAM_TRK ? FAM_GLO : cur_page()->fam;
    while (n--) {
        for (j = i; ; ) {
            j = s > 0 ? j + 1u : j - 1u;
            if (j >= NPAGES || page_in(j, f)) break;
        }
        if (j >= NPAGES) break;
        i = j;
    }
    if (i == ui.page) return;
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = ui.page;
    page_entered();
    page_flash();
}

/* (0.6.21) HOME + SELECT (after the dialog): everything diced into a new piece. One scale for the song (ROOT, a
 * musical SCALE: never CHR / WHOLE / DIM; the RESO reverb and the SLICER follow it), each melodic track an engine, a
 * sound (dice of a factory preset) and a sequence in that scale by its role (T1 the bass; the others a lead, a pad or an
 * arpeggio), a DRUM track a diced kit and a beat; the macros' routes rolled for every track; the delay, reverb and
 * chorus their settings within musical ranges. Tempo, mixer levels, mutes and the master are left alone. UNDO brings
 * the music before it back */
static void macro_dice(uint32_t mode);
static int16_t sl_root = -1, sl_scale, sl_quant;      /* (scale_link: the song's scale) */
#include "song_dice.c"                             /* (0.6.26) DICE ALL: a song-idea generator */
#include "gen.c"                                   /* (0.6.91) GEN: the song generator and its screen (HOME + SELECT) */

/* HOME / REC / SAVE: tap on release, hold 0.7 s fires once. t0 = press time | 1,
 * bit 1 = fired (or swallowed: then the release is no tap either) */
enum { BT_NONE, BT_TAP, BT_HOLD };
static uint32_t btn_hold(uint32_t *t0, uint32_t label, uint32_t now, int hold_ok)
{
    uint32_t tap;
    if ((fm1_in.buttons >> panel.btn[label]) & 1u) {
        if (!*t0)
            *t0 = (now | 1u) & ~2u;
        else if (hold_ok && !(*t0 & 2u) && now - (*t0 & ~3u) > 700u * 1000u * FM1_TICKS_PER_US) {
            *t0 |= 2u;
            return BT_HOLD;
        }
        return BT_NONE;
    }
    tap = *t0 && !(*t0 & 2u);
    *t0 = 0;
    return tap ? BT_TAP : BT_NONE;
}

/* the quick layers: ui_layer.c (included after this file) */
static void layer_masks(void);
static void macro_dice(uint32_t mode);                  /* (JIANT 0.4) */
static void layer_arm(uint32_t pressed, uint32_t now);
static uint32_t layer_held(void);
static int layer_knobs_quiet(void);
static uint32_t layer_gesture(uint32_t now, uint32_t combo);
static void layer_show(void);
static void layer_tap(uint32_t l);
static void layer_keys(uint32_t keys);
static void layer_knob(uint32_t k, int32_t s);
static int layer_play(void);
static int layer_set_open(void);
static uint32_t layer_oct(uint32_t pressed, uint32_t oct);
static void layer_oct_open(uint32_t pressed);
static int layer_allowed(void);
static uint32_t ly_bit(uint32_t l);

/* a page button let go (they act on release; a layer's own button: layer_gesture). 1: it acted (EDIT on STEP, USER,
 * PROJECT) instead of opening a page */
static int page_tap(uint32_t b)
{
    uint32_t f;
    if (b == B_GLO) {
        open_global();                                  /* MIXER -> GLOBAL -> SYSTEM -> MIXER */
        return 0;
    }
    if (b == B_EDIT && song.seq_mode && !ui.home && cur_page()->graph == GR_ROLL) {   /* STEP: EDIT clears the step */
        uint64_t held = lock_held();
        if (chain_busy()) { ui_message("STOP TO EDIT"); return 1; }
        if (held) {                                     /* a step held: its locks (the step stays) */
            lock_clear(held);
            return 1;
        }
        if (cur_page()->id[0] == 8u) {                  /* (0.6.88) SEQ > TIMING: the step back on the grid */
            ST_MICRO_SET(&TSEL->step[ui.cursor], 0);
            ui_message("ON THE GRID");
            return 1;
        }
        if (cur_page()->id[0] == 4u && ui.pr_focus && TSEL->step[ui.cursor].time == ST_NOTE &&
            ui.pr_focus <= TSEL->step[ui.cursor].n) {   /* (0.6.81) SEQ > NOTE: the note in focus alone */
            note_drop(&TSEL->step[ui.cursor], ui.pr_focus - 1u);
            ui.pr_focus = 0;
            ui_message("NOTE DELETED");
            return 1;
        }
        step_clear(&TSEL->step[ui.cursor]);
        (void)motion_clear_locks(TSEL, ui.cursor);
        cursor_set(ui.cursor + 1);
        ui_message("STEP CLEARED");
        return 1;
    }
    if (b == B_EDIT && !ui.home && cur_page()->graph == GR_EVENTS) {   /* AUTO LIST: EDIT deletes the row's record */
        ev_delete();
        return 1;
    }
    if (b == B_EDIT && !ui.home && (cur_page()->graph == GR_USER || cur_page()->graph == GR_SLOTS)) {
        name_rename();                                  /* SAVE > USER / PROJECT: EDIT renames the slot */
        return 1;
    }
    for (f = FAM_HOME + 1u; f < FAM_COUNT; f++)
        if (FAM_BTN[f] == b) {
            open_family(f);
            return 0;
        }
    return 0;
}

/* a track whose sound's sample is missing (engines.c snd_missing: an empty user slot, a built-in set this build lacks;
 * it plays a sine): the no-file icon and NO SAMPLE (ui.c MSG_NO_SAMPLE). Once per track and source: at power-on, a sound
 * or project load, SRC / SET turned, a slot erased; again after another source or a project load. One a frame; after
 * a message already showing (LOADED) as the second message. Not on notes: nothing in the audio path */

/* messages of things that happened elsewhere (a load, the editor, MIDI in): after this frame's own */
static void ui_notices(void)
{
    static uint32_t midi_t, midi_last;
    if (motion_full) { motion_full = 0; ui_message("AUTOMATION FULL"); }
    if (midi_hint) {                                    /* MIDI notes into a track that is not selected */
        uint32_t h = midi_hint;
        midi_hint = 0;
        if (!ui.msg_t && (h != midi_last || fm1_ms - midi_t > 4000u)) {
            char b[4] = {'T', (char)('0' + h), 0, 0};
            ui_say("MIDI IN -> ", b);
            midi_last = h;
            midi_t = fm1_ms;
        }
    }
}

/* (JIANT 0.4) one scale for the whole instrument. (0.6.19) Strictly: ROOT, SCALE and QNT are the song's, held here;
 * the selected track's (a knob, the SCL layer, the editor, MIDI, automation, a project load) is the one that counts,
 * and every melodic track is kept on it each frame (any other way a track got its own scale undone: a sequence
 * never plays in another scale). A DRUM track has none to follow. Each track keeps its TRN */
static void scale_link(void)
{
    const track_t *t = TSEL;
    uint32_t k;
    if (!drum_track(t) && (t->p[P_ROOT] != sl_root || t->p[P_SCALE] != sl_scale || t->p[P_QUANT] != sl_quant)) {
        sl_root = t->p[P_ROOT];
        sl_scale = t->p[P_SCALE];
        sl_quant = t->p[P_QUANT];
    }
    if (sl_root < 0)
        return;                                         /* (a DRUM track selected from the start: nothing yet) */
    for (k = 0; k < NTRK; k++)
        if (!drum_track(&trk[k])) {
            trk[k].p[P_ROOT] = sl_root;
            trk[k].p[P_SCALE] = sl_scale;
            trk[k].p[P_QUANT] = sl_quant;
        }
}

static uint8_t gw_macro_idx;
static int8_t octdn_prev_oct;
static uint8_t octdn_shift;
static int gw_oct_combo(uint32_t b)
{
    track_t *t = TSEL;
    switch (b) {
    case B_ARP: {                                       /* [OCT-] + [ARP]: Cycle 6 GammaWave Hearth & Solar Eclipse Macros */
        static const char *const MN[6] = {
            "1/6 HEARTH GRAVITY", "2/6 SINGULARITY", "3/6 PHOTOSPHERE",
            "4/6 WRAITH SCREAM", "5/6 FORMANT VOWEL", "6/6 SOLAR ECLIPSE!"
        };
        static const uint8_t MD[6][4] = {
            {5, 78, 22, 64}, {6, 96, 46, 88}, {7, 92, 72, 76},
            {8, 84, 98, 108}, {5, 72, 56, 92}, {6, 110, 118, 112}
        };
        uint32_t m;
        if (chain_busy()) {
            ui_message("STOP TO EDIT");
            return 1;
        }
        undo_mark();                                    /* ([OCT-] + [GLO] takes the macro back) */
        m = gw_macro_idx++ % 6u;
        t->p[P_DTYPE] = MD[m][0];
        t->p[P_DIST]  = MD[m][1];
        if (t->eng_req % NENGINES == ENGI_ANALOG) {     /* ANALOG's own CUT / RES; on the other engines E4 / E5 are
                                                         * their own parameters (BUZZ, MEG, PERC...): left alone */
            t->p[P_E4] = MD[m][2];
            t->p[P_E5] = MD[m][3];
        }
        t->p[P_FCUT] = MD[m][2];
        t->p[P_FRES] = MD[m][3];
        t->p[P_FTYPE] = m == 4u ? 4 : m >= 3u ? 5 : 0;  /* VOWEL, SCREAM, else back to LP */
        if (m == 5u) { song.g[G_DUCK] = 96; song.g[G_PUNCH] = 90; }
        ui_message(MN[m]);
        ui.force = 1;
        return 1;
    }
    case B_ENV:
    case B_SCL: {                                       /* [OCT-] + [ENV] or [SCL]: 1-Click Jump to SEQ > MUTATE & Activate */
        uint32_t i;
        ui.home = ui.menu = ui.confirm = 0;
        name_close();
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].graph == GR_MUTATE) {
                ui.page = (uint8_t)i;
                ui.fam_last[PAGES[i].fam] = ui.page;
                page_entered();
                page_flash();
                break;
            }
        if (!t->p[P_MSND] && !t->p[P_MPAT]) {
            t->p[P_MSND] = 2;
            t->p[P_MPAT] = 3;
            if (!t->p[P_MAMT]) t->p[P_MAMT] = 64;
        }
        ui_message("SEQ > MUTATE");
        ui.force = 1;
        return 1;
    }
    case B_HOME:
    case B_SAVE:                                        /* [OCT-] + [HOME] or [SAVE]: 1-Click Secret Hyperspace Visualizer */
        if (b == B_HOME) ui.home_t0 |= 2u;              /* (its release is no tap: HOME's tap closes the menu) */
        else ui.save_t0 |= 2u;
        ui.menu = 3;
        ui.confirm = 0;
        name_close();
        song.seq_mode = 0;
        ui.force = 1;
        return 1;
    case B_PLAY:
    case B_REC:                                         /* [OCT-] + [PLAY] or [REC]: 1-Click 49-Genre Song Generator (GEN) */
        if (b == B_REC) ui.rec_t0 |= 2u;
        if (chain_busy()) ui_message("STOP TO EDIT");
        else { confirm_open(CF_DICE_ALL, song.sel); gen_open(); }
        return 1;
    case B_SEQ:  ui.seq_t0 |= 2u; part_dice(CF_DICE_SEQ); return 1;   /* [OCT-] + [SEQ]:  Dice Track Sequence */
    case B_EDIT: part_dice(CF_DICE_SOUND); return 1;    /* [OCT-] + [EDIT]: Dice Track Sound */
    case B_FX:   part_dice(CF_DICE_FX);    return 1;    /* [OCT-] + [FX]:   Dice Track FX */
    case B_LFO:  part_dice(CF_DICE_LFO);   return 1;    /* [OCT-] + [LFO]:  Dice Track LFO */
    case B_GLO:                                         /* [OCT-] + [GLO]: UNDO (again: REDO), as GLO + G4 */
        ui_message(chain_busy() ? "STOP TO EDIT" : undo_do() ? "NOTHING TO UNDO" : "UNDONE (AGAIN: REDO)");
        ui.force = 1;
        return 1;
    default: return 0;
    }
}

static void ui_input_frame(void);
static void ui_input(void)
{
    ui_input_frame();
    scale_link();                                       /* (JIANT 0.4) one scale for every melodic track (after the
                                                         * frame's edits) */
}
static void ui_input_frame(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), now = fm1_ticks(), id, b, k;
    uint32_t home, rec, seq, save;
    uint32_t oct;
    uint32_t lay, combo = 0, lytap, lkeys, glo, kq = 0;
    int32_t s, sel = 0, ks[4] = {0, 0, 0, 0};
    fm6_poll();                                         /* FM6: PTCH turned -> its patch */
    {   /* (JIANT 0.6.5) RESO's PITCH and FOLW live in PRE's and DAMP's slots: into RESO a ROOM's values become theirs
         * (DAMP past FOLW's T6: OFF; PRE past 14: 14), out of it the reverb's own defaults back */
        static uint8_t rt0 = 0xFFu;
        uint32_t rt = (uint32_t)song.g[G_RTYPE];
        if (rt == 3u) {
            if (song.g[G_RDAMP] > 6) song.g[G_RDAMP] = 0;
            if (song.g[G_RPRE] > 14) song.g[G_RPRE] = 14;
            if (song.g[G_RPRE] < 0) song.g[G_RPRE] = 0;
        } else if (rt0 == 3u) {
            song.g[G_RDAMP] = GP[G_RDAMP].def;
            song.g[G_RPRE] = GP[G_RPRE].def;
        }   /* (0.6.12) PRE (ROOM, SPRING) and AIR (SHIMMER, CLOUD) share the slot: from one to the other it starts at 0 */
        if (rt0 != 0xFFu && rt0 != rt && (rt == 2u || rt == 4u) != (rt0 == 2u || rt0 == 4u) && rt != 3u && rt0 != 3u)
            song.g[G_RPRE] = 0;
        if (rt != 2u && rt != 4u && song.g[G_RPRE] < 0) song.g[G_RPRE] = 0;
        if ((rt == 2u || rt == 4u) && song.g[G_RPRE] > 24) song.g[G_RPRE] = 24;
        rt0 = (uint8_t)rt;
    }
#if !FELUCCA_FM4
    for (k = 0; k < NTRK; k++)                          /* a DIGITAL sound any other way (the paths convert it */
        if (trk[k].eng_req == ENGI_DIGITAL)             /* already): FM6 (fm4_convert.c) */
            fm4_track(&trk[k]);
#endif
    for (k = 0; k < NTRK; k++)                          /* a retired engine's sound any other way (core.h): ANALOG */
        if (eng_gone(trk[k].eng_req))
            set_engine_of(&trk[k], ENGI_PHYS_TO);
    perf_latch_on = fx_latch & 1u;                      /* (MENU > FX LATCH; a settings load sets it too) */
    fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (MENU > USB LEVEL: fx.c, audio.c) */
    mo_lvl = (uint8_t)(midi_out_mode & 3u);             /* (0.6.72) MENU > MIDI OUT (seq.c) */
    midi_pc_poll();                                     /* (0.6.72) PROGRAM CHANGE in: the sounds it asked for */
    rp_apply();                                        /* (MENU > CLICK, CLICK LEVEL, COUNT-IN: click.c, seq.c) */
    {   /* (0.6.55) TRACKS 6 -> 4 (MENU or GLO > SYSTEM) on T5 / T6: T4 selected (ALGORITHM stops there) */
        static uint8_t live0 = NTRK;
        if (trk_live < live0 && song.sel >= trk_live)
            track_select(trk_live - 1u);
        live0 = trk_live;
    }
    {   /* (0.6.18) TRACKS 4 LOW: T5 / T6 picked, the screen says how to wake them (once a pick). (0.6.28) Also when a
         * note is sent to them (a key, MIDI, their steps) and when PLAY starts with steps on them: they were silent and
         * people did not know why (at most every 4 s) */
        static uint8_t sel0 = 0xFFu, play0;
        static uint32_t said_ms;
        uint32_t k, said = song.sel != sel0 && song.sel >= trk_live;
        int quiet = !said_ms || fm1_ms - said_ms > 4000u;
        if (song.playing && !play0 && song.g[G_STRN]) {   /* (0.6.45) PLAY with every sequence moved: say so (it
                                                         * looked like a bug: notes lower than the roll shows) */
            char b[24];
            str_cpy(b, "ALL TR ", sizeof b);
            if (song.g[G_STRN] < 0) str_cpy(b + 7, "-", 2);
            fmt_int(b + str_len(b), song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
            ui_say(b, " DEG");
            str_cpy(ui.msg2, "SCL 2 > ALL TR: 0 = OFF", sizeof ui.msg2);
        }
        if (song.playing && !play0 && quiet)
            for (k = trk_live; k < NTRK; k++)
                if (!seq_is_empty(&trk[k]) && !trk[k].p[P_MUTE]) said = 1;
        if (asleep_hit) { asleep_hit = 0; said |= quiet; }
        if (said) {
            ui_say("T5-T6 ASLEEP: ", "");
            str_cpy(ui.msg2, "GLO>SYSTEM>TRACKS 6", sizeof ui.msg2);
            said_ms = fm1_ms | 1u;
        }
        sel0 = song.sel;
        play0 = song.playing;
    }
    if (!ui.menu)
        usb_serial_apply();                             /* (MENU > USB SERIAL: when the menu has closed) */
    if (scr_input(pressed, notes))                      /* MENU > SCREEN OFF: dark, or the wake gesture: swallowed */
        return;                                         /* (before the buttons' timers: no tap or hold after it) */
    /* GammaWave [OCT-] Shift Deck Combos: hold [OCT-] first, then press ARP/HOME/SAVE/PLAY/SEQ/EDIT/FX/LFO/GLO */
    if (!((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u) || ((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u)) {
        octdn_shift = 0;
    } else if (((pressed >> panel.btn[B_OCTDN]) & 1u) && !(fm1_in.buttons & ~(1u << panel.btn[B_OCTDN])) &&
               !ui.ly && !ui.menu && !ui.confirm && !name_on() && !act_cols() && !layer_set_open()) {
        octdn_prev_oct = song.octave;
        octdn_shift = 1;
    }
    if (octdn_shift && !ui.menu && !ui.confirm && !name_on()) {
        uint32_t non_oct = pressed & ~((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]));
        for (id = 0; non_oct && id < 14u; id++) {
            if (((non_oct >> id) & 1u) && gw_oct_combo(panel_btn_of(id))) {
                if (octdn_shift == 1u) song.octave = octdn_prev_oct;
                octdn_shift = 2u;
                pressed &= ~(1u << id);
            }
        }
    }
    home = btn_hold(&ui.home_t0, B_HOME, now, 1);
    rec = btn_hold(&ui.rec_t0, B_REC, now, 0);          /* (a tap; held, the REC layer: ui_layer.c) */
    seq = btn_hold(&ui.seq_t0, B_SEQ, now, !ui.menu && !ui.confirm);
    save = btn_hold(&ui.save_t0, B_SAVE, now, 0);       /* (a tap; held, the song layer: ui_layer.c) */
    layer_arm(pressed, now);
    if (((pressed >> panel.btn[B_REC]) & 1u) && ui.ly == LAYER_REC)
        ui.rec_t0 |= 2u;                                /* (REC is its layer's button now: its tap is layer_tap's) */
    if (((pressed >> panel.btn[B_SAVE]) & 1u) && ui.ly == LAYER_SAVE)
        ui.save_t0 |= 2u;                               /* (SAVE too: layer_tap opens its pages) */
    layer_oct_open(pressed);                            /* (OCT± with a SET layer's button down: it opens now) */
    oct = oct_taps(pressed, ui.menu || ui.confirm || act_cols() || name_on() || layer_set_open());
    oct = layer_oct(pressed, oct);                      /* (a SET layer's OCT-: put back) */
    layer_masks();                                      /* seq.c: keys pressed with a layer's button are its own */
    {   /* (0.6.6) a page button held + PRESETS: its pages (page_jump); not in the menu, a dialog, NAME. (0.6.67) A
         * button held + SELECT: its dice (HOME everything, EDIT the sound, SEQ the sequence, LFO the LFO, FX the
         * effects; GLO + SELECT the tempo), the other buttons their pages; the button's tap and layer swallowed, and
         * the turn's last detents for a moment after it is let go (no page scrolled by them) */
        static uint32_t pj_sel_ms;
        uint32_t f = held_family(), hb = (fm1_in.buttons >> panel.btn[B_HOME]) & 1u;
        int32_t ps;
        if (!ui.menu && !ui.confirm && !name_on() && (f != FAM_COUNT || hb) && f != FAM_GLO &&
            (ps = panel_enc(EN_SELECT)) != 0) {
            uint32_t k = f == FAM_EDIT ? CF_DICE_SOUND : f == FAM_SEQ ? CF_DICE_SEQ : f == FAM_LFO ? CF_DICE_LFO :
                         f == FAM_FX ? CF_DICE_FX : 0u;
            if (f == FAM_COUNT) {                       /* HOME held: the whole piece (the dialog first) */
                ui.home_t0 |= 2u;
                if (!pj_sel_ms || fm1_ms - pj_sel_ms > 400u) {
                    if (chain_busy()) ui_message("STOP TO EDIT");
                    else { confirm_open(CF_DICE_ALL, song.sel); gen_open(); }
                }
            } else if (k) {
                part_dice(k);
                btn_used(FAM_BTN[f]);
            } else {
                page_jump(f, ps);
            }
            pj_sel_ms = fm1_ms | 1u;
        } else if (f != FAM_COUNT && !ui.menu && !ui.confirm && !name_on() && !ly_used &&
                   (ps = panel_enc(EN_PRESET)) != 0) {   /* (a layer already used: PRESETS stays its, dropped) */
            page_jump(f, ps);
        } else if (pj_sel_ms && fm1_ms - pj_sel_ms < 400u) {
            if (panel_enc(EN_SELECT))
                pj_sel_ms = fm1_ms | 1u;                /* (dropped: still the paging turn) */
        } else {
            pj_sel_ms = 0;
        }
        pj_hold();
    }
    lay = layer_held();
    glo = lay && ui.ly == LAYER_GLO;                    /* GLO held: SELECT is the tempo, BPM LOCK or not (#58) */
    if (!layer_allowed()) {
        perf_kill = 1;                                  /* (effects off until their keys are let go) */
        perf_latched = 0;                               /* (FX LATCH: the latched ones and the macros off) */
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    }
    else if (!kb_layer)
        perf_kill = 0;
    {   /* a key pressed with the button: a combo (its edge, or the ISR already took it); then not the grid's or a step's */
        static uint32_t kb_seen;
        lkeys = kb_layer & ~kb_seen;
        combo = lay && (notes || lkeys);
        kb_seen = kb_layer;
    }
    if (lay)
        lkeys |= notes & ~fm1_in.notes;                 /* (tapped and let go already) */
    notes &= ~kb_layer;
    if (lay) {                                          /* a layer's button held: keys, knobs and buttons are combos */
        combo |= (pressed & ~ly_bit(ui.ly) &            /* (REC + PLAY: PLAY plays and REC let go soon is still */
                  ~(ui.ly == LAYER_REC ? 1u << panel.btn[B_PLAY] : 0u)) != 0u;   /* a tap, it arms: as before) */
        notes = 0;                                      /* (the keys are the layer's, not the grid's or a step's) */
        if (pressed & (1u << panel.btn[B_SAVE]))       /* no UNDO, no page */
            ui.save_t0 |= 2u;
        if (pressed & (1u << panel.btn[B_HOME]))       /* no menu, no HOME */
            ui.home_t0 |= 2u;
        if (pressed & (1u << panel.btn[B_SEQ]))
            ui.seq_t0 |= 2u;
        for (k = 0; k < 4u; k++)                        /* KNOB 1..4: the layer's (ui_layer.c layer_knob) */
            if ((ks[k] = panel_enc(EN_K1 + k)) != 0)
                combo = 1;
        panel_enc(EN_PRESET);                           /* (a stray turn would load another sound) */
        panel_enc(EN_ALGO);                             /* (another track: OCT- puts back the layer's track only) */
        if (glo && (sel = panel_enc(EN_SELECT)) != 0)   /* GLO + SELECT: the tempo, a combo (OCT- puts it back) */
            combo = 1;
    } else if (fx_lock_on) {                            /* (0.6.44) the FX layer locked open: KNOB 1..4 its macros */
        for (k = 0; k < 4u; k++)
            ks[k] = panel_enc(EN_K1 + k);
        panel_enc(EN_PRESET);
        kq = 1;
    } else if ((kq = (uint32_t)layer_knobs_quiet()) != 0) {   /* a layer letting go: KNOB 1..4 are nobody's (#39) */
        for (k = 0; k < 4u; k++)
            if (panel_enc(EN_K1 + k) != 0)
                combo = 1;                              /* (with the button let go this frame: no tap) */
    }
    if (!lay)
        ly_used = 0;
    else if (combo)
        ly_used = 1;
    lytap = layer_gesture(now, combo);
    layer_show();
    layer_keys(lkeys);
    for (k = 0; k < 4u; k++)
        if (ks[k]) {
            layer_knob(k, ks[k]);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
        }
    song.grid = (uint8_t)keys_mode();                 /* (the menu, a dialog: the keys play again; NAME: silent) */
    if (home == BT_TAP && ui.menu) {                    /* HOME (pressed, or held) closes the menu, from ABOUT too; */
        menu_close();                                   /* the release of the hold that opened it is no tap */
        home = BT_NONE;                                 /* (menu_close went HOME already) */
    } else if (home == BT_HOLD) {                       /* HOME held: open the menu, or leave it */
        if (ui.menu) {
            menu_close();
        } else {
            ui.menu = 1;                                /* (at the tab and row it was left at) */
            ui.confirm = 0;                             /* (a clear dialog is cancelled, NAME too) */
            name_close();
            ui.force = 1;
            song.seq_mode = 0;
        }
    }
    if (rec == BT_TAP && !ui.menu && !ui.confirm && !name_on())   /* REC pressed in another layer (REC alone: */
        rec_tap();                                      /* its layer's tap); nothing in the menu, a dialog or NAME */
    if (ui.menu) {                                      /* SAVE / REC taps do nothing here (HOME: above) */
        if (ui.save_t0)
            ui.save_t0 |= 2u;
        ui.pg_down = 0;
        if (!ui.home_t0)
            menu_input(oct);
        return;
    }
    if (name_on() && !ui.confirm) {                     /* NAME: the keys type, KNOB 1 / 2, OCT+ / OCT- (ui_name.c); */
        if (ui.save_t0)                                 /* SAVE does nothing */
            ui.save_t0 |= 2u;
        if (((pressed >> panel.btn[B_PLAY]) & 1u) && (song.playing || chain_busy() || seq_counting()))
            transport_req = 2;                          /* PLAY stops a transport started meanwhile (MIDI Start, the
                                                         * editor) so the name can be saved; it never starts one */
        ui.pg_down = 0;
        name_input(notes, oct);
        return;
    }
    if (save == BT_TAP && !ui.confirm)                  /* SAVE let go (not its layer's: that tap is layer_tap's) */
        open_family(FAM_SAVE);
    if (ui.confirm) {                                   /* OCT- cancels, OCT+ does it; nothing else reacts */
        if (ui.confirm == CF_DICE_ALL)                  /* (TAKES v2) the reel's question first: OCT+ yes, OCT- no */
            oct = gen_oct(oct);
        if (oct & 2u) {
            uint32_t kind = ui.confirm;
            ui.confirm = 0;
            ui.force = 1;
            if (kind != CF_OVR_PROJ && kind != CF_OVR_USER && kind != CF_ERASE_USER && kind != CF_DICE_ALL &&
                !dice_kind(kind) && kind != CF_DEL_ROW && kind != CF_CLEAR_SONG && !chain_busy())
                undo_mark();                            /* (0.6.21) the music before a clear, INIT, RECALL, pattern */
            if (kind == CF_OVR_PROJ) {                  /* overwrite: the NAME screen writes it */
                name_open(NK_PROJ_SAVE, ui.confirm_trk & 3u);
            } else if (kind == CF_OVR_USER) {
                name_open(NK_USER_SAVE, ui.confirm_trk);
            } else if (kind == CF_ERASE_USER) {
                up_ui(1u, ui.confirm_trk);
            } else if (dice_kind(kind)) {               /* (0.6.65) OK: the diced part stays */
                ui_message(dk.msg);
            } else if (kind == CF_DICE_ALL) {           /* (0.6.21) HOME + SELECT; (0.6.91) GEN: the piece from the
                                                         * screen's settings, the screen stays (OCT+ again: another) */
                if (!chain_busy()) { gen_press(); ui.confirm = CF_DICE_ALL; }
            } else if (kind == CF_LOAD_PAT) {
                pat_load_ui(&trk[ui.confirm_trk % NTRK], pat_pick());
            } else if (kind == CF_CLEAR_MOTION) {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                if (!chain_busy()) { load_begin(t, LOAD_PAT); motion_clear(t); load_end(t); ui_message("AUTOMATION CLEARED"); }
            } else if (kind == CF_DEL_ROW) {
                uint32_t r = ui.confirm_trk;
                if (!chain_busy() && r < chain_config.count) {
                    for (; r + 1u < chain_config.count; r++) {
                        chain_config.row[r] = chain_config.row[r + 1u];
                        song_idx.scene[song_cur][r] = song_idx.scene[song_cur][r + 1u];   /* (its scene with it) */
                    }
                    memset(&song_idx.scene[song_cur][r], 0, sizeof(scene_t));
                    song_idx_touch();
                    chain_config.count--;
                    if (ui.song_row > chain_config.count) ui.song_row = chain_config.count;
                    ui_message("ROW DELETED");
                }
            } else if (kind == CF_CLEAR_SONG) {
                if (!chain_busy()) {
                    chain_defaults(&chain_config);
                    memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
                    song_idx_touch();
                    ui.song_row = 0;
                    ui_message("SONG CLEARED");
                }
            } else if (kind == CF_CPU_TEST) {           /* (0.6.83) SAVE > TOOLS 6: the heaviest song (UNDO: back) */
                if (!chain_busy()) {
                    cpu_test_song();
                    ui_message("CPU TEST: PRESS PLAY");
                }
            } else if (kind == CF_INIT_SYS) {           /* (0.6.40) SAVE > TOOLS 5: everything as at power-on */
                if (!chain_busy()) {
                    undo_mark();
                    if (song.playing) transport_req = 2;
                    system_init();
                    ui_message("NEW PROJECT");
                }
            } else if (kind == CF_CLEAR_ALL) {          /* SEQ + REC: every track's steps and automation, the lane */
                if (!chain_busy()) {
                    uint32_t k;
                    for (k = 0; k < NTRK; k++) {
                        load_begin(&trk[k], LOAD_PAT);
                        track_defaults_steps(&trk[k]);
                        motion_clear(&trk[k]);
                        load_end(&trk[k]);
                        latch_drop_req |= (uint8_t)(1u << k);   /* (the held / latched notes: events_block) */
                    }
                    memset(pfx_lane, 0, sizeof pfx_lane);
                    ui_message("ALL CLEARED");
                }
            } else if (kind == CF_INIT_SOUND) {         /* (DRUM: the factory DRUM-X kit too) */
                if (!chain_busy()) {
                    init_sound_of(TSEL, TSEL->eng_req);
                    if (drum_track(TSEL)) {
                        memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
                        memset(dx_mot, 0, sizeof dx_mot);   /* (0.5.1: X-MOD off) */
                        memset(dx_snd, 0, sizeof dx_snd);   /* (0.6.48: no SEND) */
                        memcpy(dx_lay, DX_LAY_DEF, sizeof dx_lay);   /* (0.6.49: the factory LAYERs) */
                        memset(dx_mdl, 0, sizeof dx_mdl);   /* (1.0-GW: every sound CLASSIC) */
                    }
                    ui_message("SOUND INIT");
                }
            } else if (kind == CF_RECALL_SOUND) {       /* the section's stored sound (ui_layer.c EDIT) */
                if (!chain_busy()) {
                    int sl = project_recall_slot();
                    if (sl < 0 || project_recall_sound((uint32_t)sl, song.sel)) ui_message("NOTHING SAVED");
                    else ui_message("SOUND RECALLED");
                }
            } else {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                load_begin(t, LOAD_PAT);
                track_defaults_steps(t);
                load_end(t);
                latch_drop_req |= (uint8_t)(1u << (t - trk));   /* and the latched chord (events_block) */
                if (kind == CF_CLEAR_TRK) {
                    char b[12] = "1 CLEARED";
                    b[0] = (char)('1' + ui.confirm_trk);
                    ui_say("TRACK ", b);
                } else {
                    {   /* (0.6.28) which track: the others play on (SEQ + REC clears them all) */
                        char cb[16] = "T1 CLEARED";
                        cb[1] = (char)('1' + ui.confirm_trk % NTRK);
                        ui_say(cb, "");
                        str_cpy(ui.msg2, "SEQ+REC: ALL TRACKS", sizeof ui.msg2);
                    }
                }
            }
        } else if (oct & 1u) {
            if (dice_kind(ui.confirm))                  /* (0.6.65) CANCEL: the part before the dice */
                ui_message(dk_back(ui.confirm, ui.confirm_trk) ? "CAN'T UNDO" :
                           ui.confirm == CF_DICE_SEQ ? "SEQ BACK" : ui.confirm == CF_DICE_LFO ? "LFO BACK" :
                           ui.confirm == CF_DICE_FX ? "FX BACK" : "SOUND BACK");
            ui.confirm = 0;
            ui.force = 1;
        } else if (dice_kind(ui.confirm) && panel_enc(EN_SELECT)) {   /* (0.6.65) SELECT again: another dice
                                                         * (once a turn burst) */
            part_dice(ui.confirm);
        } else if (ui.confirm == CF_DICE_ALL) {         /* (0.6.91) GEN: KNOB 1..4 its settings; (1.0-GW) PRESETS its
                                                         * pages, SELECT the genre, ALGORITHM the seed (a take while
                                                         * playing); (0.6.97) PLAY runs and stops the transport */
            for (k = 0; k < 4u; k++)
                if ((s = panel_enc(EN_K1 + k)) != 0) gen_knob(k, s);
            if ((s = panel_enc(EN_SELECT)) != 0) gen_select(s);
            if ((s = panel_enc(EN_PRESET)) != 0) gen_reel_turn(s);   /* (1.0-GW) the TAKES reel */
            if ((s = panel_enc(EN_ALGO)) != 0) gen_algo(s);
            gen_layer(lkeys);                           /* (TAKES v2) OCT+ held on the reel: the keys */
            gen_take_poll();
            {   /* (TAKES v2) EDIT on the reel: delete the take (held: the takes not kept), a question first; on the SET
                 * page, the entry (held: nothing) */
                static uint32_t ed_t0;
                uint32_t e = btn_hold(&ed_t0, B_EDIT, now, 1);
                if (gp.view == 2u) { if (e == BT_TAP) set_delete(); }
                else if (gp.view && (e == BT_TAP || e == BT_HOLD)) gen_reel_edit(e == BT_HOLD);
            }
            if (gl.down_oct && (pressed & ~((1u << panel.btn[B_OCTUP]) | (1u << panel.btn[B_OCTDN]))))
                gl.used = 1;                            /* (TAKES v2) a button with OCT+ down: no load when it is let go */
            {   /* (TAKES v2) REC: a SET recorded while playing (its own tap: in a dialog the REC layer is dead); (NOW)
                 * SAVE on the settings and the reel: NOW kept as a take, with OCT+ held: a take's code typed (NAME) */
                static uint32_t rc_t0, sv_t0;
                uint32_t sv;
                if (btn_hold(&rc_t0, B_REC, now, 0) == BT_TAP && gp.view != 2u) gen_reel_rec();
                sv = btn_hold(&sv_t0, B_SAVE, now, 0);
                if (sv == BT_TAP && gp.view != 2u) {
                    if (gl.down_oct) gen_code_enter();
                    else now_save();
                } else if (gl.down_oct && ((fm1_in.buttons >> panel.btn[B_SAVE]) & 1u) && gp.view != 2u) {
                    gl.used = 1;                        /* (SAVE down with OCT+: its tap comes after OCT+ is let go) */
                }
            }
            if (home == BT_TAP) {                       /* (0.7.0) HOME: done, from any GEN page (OCT-: back) */
                ui.confirm = 0;
                ui.force = 1;
                go_home();
            }
            if (((pressed >> panel.btn[B_PLAY]) & 1u) && gp.view == 2u) {
                set_toggle();                           /* (TAKES v2) the SET page: PLAY runs the SET */
            } else if ((pressed >> panel.btn[B_PLAY]) & 1u) {
                if (song.playing || chain_busy() || seq_counting()) transport_req = 2;
                else {
                    if (gp.view == 1u && !reel_on_now && reel_cur < reel_n && reel_cur != reel_play && gen_reel_load()) {
                        gp.sig = gen_sig();             /* (NOW) PLAY on a take's card: that take, then playing */
                        gen_take_ms = 0;
                    }
                    if (live.mode) chain_play_ui();
                    else transport_req = 1;
                }
            }
        }
        ui.pg_down = 0;
        enc_drop();
        return;
    }
    if (lytap)                                          /* a layer's button acts on release (held: the layer) */
        layer_tap(lytap);
    if (seq == BT_TAP || seq == BT_HOLD)                /* (SONG: SAVE held, G5) */
        open_family(FAM_SEQ);
    if (home == BT_TAP)                                 /* HOME acts on release: a hold opens the menu */
        go_home();
    cursor_fix();                                       /* LEN may have changed (knob, editor, load) */
    for (id = 0; id < 14u; id++) {
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        switch (b) {
        case B_PLAY:
            if (layer_play())                           /* (GLO held: RESTART) */
                break;
            if (song.playing || chain_busy() || seq_counting())   /* (a count-in: PLAY stops it) */
                transport_req = 2;
            else if ((!ui.home && cur_page()->graph == GR_SONG) || live.mode)   /* (the song layer's SONG mode) */
                chain_play_ui();
            else
                transport_req = 1;
            break;
        case B_REC:                                     /* (JIANT) with SEQ held: CLEAR ALL? (every track's steps) */
            if ((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u && !ui.menu && !ui.confirm) {
                ui.seq_t0 |= 2u;                        /* (SEQ: neither its tap nor its hold now) */
                if (ui.ly)
                    ui.ly_t0 |= 2u | 4u;                 /* (LY_OPEN LY_COMBO: the armed layer, no tap when let go) */
                if (chain_busy())
                    ui_message("STOP TO EDIT");
                else
                    confirm_open(CF_CLEAR_ALL, song.sel);
            }
            break;
        case B_SEQ:                                     /* tap: above; held, its layer (ui_layer.c) */
        case B_SAVE:
        case B_HOME:
            break;
        case B_FX:                                      /* the layers' buttons: ui_layer.c; REC held first: */
        case B_GLO:                                     /* their automation (rec_clear_auto) */
        case B_SCL:
        case B_EDIT:
            if (rec_first(b))
                rec_clear_auto(b);
            break;
        case B_OCTDN:
        case B_OCTUP: {
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if (act_cols() || layer_set_open())         /* action pages: enter / back (below); SET layers: OCT- */
                break;
            if (!ui.home && !ui.menu && !ui.confirm && cur_page()->graph == GR_MUTATE) {   /* (0.6.43) KEEP, FREEZE */
                if (b == B_OCTUP) { mut_keep(); ui_message("ANCHOR: THE MUSIC NOW"); }
                else { mut_freeze = (uint8_t)!mut_freeze; ui_message(mut_freeze ? "MUTATE FROZEN" : "MUTATE FREE"); }
                break;
            }
            if ((fm1_in.buttons & both) == both)
                song.octave = 0;
            else
                song.octave += b == B_OCTDN ? (song.octave > -3 ? -1 : 0) : (song.octave < 3 ? 1 : 0);
            break;
        }
        default:                                        /* page buttons (GLO SCL ENV LFO EDIT ARP): when let go */
            if (rec_first(b)) {                         /* (REC held first: their automation) */
                rec_clear_auto(b);
                break;
            }
            if (b == B_ARP && ui.entry_open && !ui.home && cur_page()->graph == GR_ROLL) {
                ui.ent_arp ^= 1u;                       /* (0.6.37) a chord held in step recording: arpeggiate it */
                ent_message();
                break;
            }
            if (!lay)                                   /* (with FX held: swallowed) */
                ui.pg_down |= (uint16_t)(1u << id);
            break;
        }
    }
    b = ui.pg_down & ~fm1_in.buttons;
    ui.pg_down &= (uint16_t)~b;
    for (id = 0; b; id++, b >>= 1)
        if (b & 1u)
            page_tap(panel_btn_of(id));
    if (act_cols() && (oct & 2u)) {                     /* action pages: OCT+ does the picked action, */
        act_do();
    } else if (act_cols() && (oct & 1u)) {              /* OCT- drops it, or (none picked) goes HOME */
        if (cur_page()->graph != GR_PATS && ui.act)
            ui.act = 0;
        else
            go_home();
    }
    song.grid = (uint8_t)keys_mode();                 /* (seq.c: the keys are the grid's) */
    seq_fill = (uint8_t)((fm1_in.buttons >> panel.btn[B_FX]) & 1u);   /* (0.6.42) FILL steps: FX held */
    lock_keys();                                        /* (a grid key on a hit let go: the hit goes) */
    if (song.grid) {
        grid_keys(notes);
    } else if (song.seq_mode && cur_page()->graph == GR_ROLL) {   /* STEP (not CHANCE: its knobs only) */
        if (live_rec_sel() || !((song.rec >> song.sel) & 1u))   /* armed and playing: the keys record live, not
                                                         * into the cursor step too; not armed (1.2, #133): they
                                                         * only play, being on STEP writes nothing */
            ui.entry_open = 0;
        else if (!chain_busy())
            seq_entry(notes);                           /* armed and stopped: step recording at the cursor */
        else if (notes)
            ui_message("STOP TO EDIT");
    }

    /* #102: the knobs a layer took above are not read again here. The TIMER5 scan (10 kHz) counts detents while this
     * pass runs, so one landing after the layer's read went to the page as well: KNOB 1..4 edited the layer's control
     * and the page under it (HOME's sound, motion recorded), PRESETS could load a sound. They stay for the next pass */
    kq |= lay;
    if (!lay && (s = panel_enc(EN_PRESET)) != 0)
        presets_turn(s);
    if (!lay && (s = panel_enc(EN_ALGO)) != 0)     /* ALGORITHM: the selected track, on every page. (0.6.55) Only
                                                    * the tracks that play: TRACKS 4 stops at T4 (T5 / T6 asleep) */
        track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
    if (glo) {                                          /* GLO + SELECT: the tempo (0.6.67: the only way, with TAP) */
        if (sel) {
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, sel, 200), GP[G_BPM].min, GP[G_BPM].max);
            ui.bpm_t = 40;                              /* the header's BPM lights up; no message over the header */
        }
    } else if ((s = panel_enc(EN_SELECT)) != 0 && !lay && !ui.menu) {   /* (0.6.67) SELECT alone: the section's pages;
                                                         * (0.6.69) on HOME, as before: DICE EVERYTHING? (once a turn) */
        static uint32_t hd_ms;
        if (!ui.home) page_scroll(s);
        else if (ui_sel_home & 1u) {                    /* (1.1.1-GW, issue #3) MENU > GEN > SELECT ON HOME TEMPO: as
                                                         * GLO + SELECT (the generator: HOME held + SELECT, OCT- + PLAY) */
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, s, 200), GP[G_BPM].min, GP[G_BPM].max);
            ui.bpm_t = 40;
        } else if (!ui.confirm && (!hd_ms || fm1_ms - hd_ms > 400u)) {
            if (chain_busy()) ui_message("STOP TO EDIT");
            else { confirm_open(CF_DICE_ALL, song.sel); gen_open(); }
        }
        if (ui.home) hd_ms = fm1_ms | 1u;
    }
    for (k = 0; k < 4u && !kq; k++) {
        const page_t *pg = cur_page();
        int16_t *hv;
        uint64_t held;
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (mac_latch && !ui.menu && !ui.confirm) {     /* (JIANT 0.4) the MACRO layer latched: M1..M4 */
            macro_v[k] = (uint8_t)clamp(macro_v[k] + accel(EN_K1 + k, s, 127), 0, 127);
            continue;
        }
        if (ui.entry_open && !ui.home && cur_page()->graph == GR_ROLL && !grid_on() && lock_held() &&
            (k == 0u || (ui.ent_arp && k < 3u))) {      /* (0.6.37) a chord being entered: KNOB 1 its LEN (ARP: RES), */
            ent_knob(k, s);                             /* 2 ORDER, 3 OCT; the others still lock */
            continue;
        }
        if ((held = lock_held()) != 0u) {               /* a step held on STEP: KNOB k locks (parameter locks) */
            lock_turn(k, s, held);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
            continue;
        }
        if (ui.home || pg->scope == SC_STEP || pg->scope == SC_TRK || page_desc(pg, k, &hv) ||
            ((pg->graph == GR_USER || pg->graph == GR_MOD || pg->graph == GR_MSEQ || pg->graph == GR_PATS) && k == 0u)
            || pg->graph == GR_SONG || pg->graph == GR_DXSND) {   /* (not an empty column) */
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
        }
        if (ui.home) {
            int16_t *vp;
            const param_desc_t *d = home_param(k, &vp);
            *vp = (int16_t)param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
            motion_capture_ui(TSEL, (uint32_t)(vp - TSEL->p), *vp);
        } else {
            edit_param(k, s);
        }
    }
    ui_notices();
}

/* ---------------------------------------------------- panel setup --- */
/* 30 s without input: give up and keep the old table (a stuck key cannot hang the boot). A piano key cancels at
 * any time: the keys are not part of what is taught, so one works whatever the buttons' table says */
#define SETUP_IDLE_MS 30000u
#define SETUP_GIVE_UP(t0) (fm1_ms - (t0) > SETUP_IDLE_MS || fm1_in.notes != 0u)
static void setup_title(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    {   /* the title with its icon (the menu row's), centred together; M from y 8 as before */
        const char *t = "HARDWARE CALIBRATION";
        int32_t x = (240 - (16 + 6 + text_w(&AF_M, t))) / 2;
        cv_begin(240, 24, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "calibration title centred");
        GFX_HOOK_ALIGN(0, HEAD_MY + AF_M_CAP_Y, 0, HEAD_MY + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS,
                       "header icon on its title's line");
        cv_icon_mid(x, 12, 16, ICON_X_DOCTOR, T_THEME, T_BG);
        cv_text(x + 22, HEAD_MY, &AF_M, t, T_TEXT);
        cv_blit(0, 5);
    }
    draw_text_box(0, 32, 240, &AF_S, "TEACH EACH BUTTON AND KNOB", T_MID, 1);
    lcd_fill(16, 56, 208, 1, T_LINE);
    draw_text_box(0, 206, 240, &AF_S, "ANY KEY: CANCEL", T_DIM, 1);
}
static void setup_show(const char *what, const char *name)     /* "PRESS" / "TURN RIGHT", the control */
{
    draw_text_box(0, 80, 240, &AF_S, what, T_MID, 1);
    draw_text_box(0, 100, 240, &AF_L, name, T_THEME, 1);
}
static void panel_setup(void)
{
    uint32_t i, used = 0, t0 = fm1_ms;
    const panel_t old = panel;
    setup_title();
    while (fm1_in.buttons) {                             /* wait for OCT-/OCT+ release */
        fm1_wdt_feed();
        if (SETUP_GIVE_UP(t0))
            goto timeout;
    }
    fm1_input_edges(0);
    for (i = 0; i < NB; i++) {
        uint32_t p = 0, id;
        setup_show("PRESS", B_NAME[i]);
        t0 = fm1_ms;
        while (!(p & ~used)) {
            fm1_wdt_feed();
            p |= fm1_input_edges(0);
            if (SETUP_GIVE_UP(t0))
                goto timeout;
        }
        for (id = 0; id < 14u; id++)
            if (((p & ~used) >> id) & 1u)
                break;
        panel.btn[i] = (uint8_t)id;
        used |= 1u << id;
    }
    used = 0;
    for (i = 0; i < NE; i++) {
        uint32_t e;
        int32_t st = 0;
        setup_show("TURN RIGHT", E_NAME[i]);
        for (e = 0; e < 7u; e++)
            fm1_enc_take(e);
        t0 = fm1_ms;
        for (;;) {
            fm1_wdt_feed();
            if (SETUP_GIVE_UP(t0))
                goto timeout;
            for (e = 0; e < 7u; e++)
                if (!((used >> e) & 1u) && (st = fm1_enc_take(e)) != 0)
                    break;
            if (e < 7u)
                break;
        }
        panel.enc[i] = (uint8_t)e;
        panel.dir[i] = (int8_t)(st > 0 ? 1 : -1);
        used |= 1u << e;
        fm1_delay_ms(300);
        fm1_enc_take(e);
    }
    panel.magic = PANEL_MAGIC;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    return;
timeout:
    panel = old;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    ui_message("SETUP CANCELLED");
}

#include "ui_dx.c"                                  /* EDIT > SOUND / SOUND 2: the DRUM-X kit */
