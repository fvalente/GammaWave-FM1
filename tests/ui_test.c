/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the UI (firmware/src/ui*.c with the sound sources of hostsim.c;
 * the display and the HAL are stubs, the buttons and knobs are driven through them):
 *   SOUNDS     a sound load (PRESETS knob / page, TOOLS INIT, user preset) changes the sound only: the
 *              steps, LEN DIV SWING GATE, ARP, SCL, the SLICER and the mix stay the track's. Power-on:
 *              every sequencer empty.
 *   PATTERNS   SEQ > PATTERNS: KNOB 1 PAT (factory patterns, then user presets that hold one), OCT+
 *              LOAD; over the user's steps the REPLACE? dialog (OCT- / OCT+); the PRESETS hint.
 *   UNDO       none (TONIC): SAVE held changes nothing; loads, tools and lock edits are final.
 *   REC        a tap on every screen arms the selected track and starts PLAY, without navigation.
 *              Long REC never clears. On STEP, armed and playing: keys do not write the
 *              cursor step. With the ARP on, the arp's notes are recorded, not the keys.
 *   MIDI IN    GLO > SYSTEM ROUT: CH1-4 (channels 5..16 ignored) / SEL, note-offs follow their note-ons.
 *   SAVE       refused while playing ("STOP TO SAVE"); a used slot asks OVERWRITE? (OCT+ / OCT-).
 *   ACTIONS    PATTERNS, USER, PROJECT, TOOLS: the knobs pick, OCT+ does it, OCT- cancels or goes
 *              HOME, on release, never both together; the OCT LEDs (OCT- lit, OCT+ blinks).
 *   TRACKS     the PRESETS knob does nothing there; KNOB 1 is MUTE.
 *   GRID       SEQ > STEP on a DRUM track: white keys toggle the selected lane's steps of the page, black
 *              keys 1..8 select the lane (and only they play), 9 held = ACC, 10 / 11 the page; KNOB 1..4 STEP
 *              LANE HIT ACC; the key LEDs (hits, the playhead inverted, the lane, ACC, the page keys); a step's
 *              GM notes shown on their lanes and made the lane's own by an edit; live recording into the grid
 *              (lane keys on the grid, GM keys elsewhere; other GM drums as notes; no TIE holds); 12 BEAT from
 *              PATTERNS fills the grid (undo); sound loads keep it, and other engines play it as GM notes.
 *   ROLL       the rolling digits of the card values and the header BPM: strips only, the last frame static,
 *              direction, retarget, the snaps (fast turn, names, shape, page, track, palette, force).
 *   SOAK       #63: 150000 random frames (turns, pages, transport, REC / motion, wrapping clocks): no roll
 *              outlives ROLL_FRAMES, the cards always equal a forced redraw once the rolls are over.
 *   USB SERIAL #67: MENU > USB SERIAL ON (default, the descriptors as before) / OFF (audio + MIDI only, class 0):
 *              the menu, the settings, the descriptor at boot (built with the console, as the firmware: FELUCCA_CDC).
 *   SCREEN OFF (1.1.5): MENU > DISPLAY > SCREEN OFF against a simulated clock and the panel's commands (lcd_power):
 *              dark after the time, nothing drawn, any input wakes it (SLPOUT, 120 ms, redraw, DISPON) and is
 *              swallowed; dialogs / count-in / UPDATE MODE keep it on; saved; the sound bit for bit while dark.
 * Built and run by tests/run_tests.sh. */
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                                    /* (as src/felucca.c: the console built in, MENU > USB SERIAL) */
#endif
#include <stddef.h>
#include <stdint.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ HAL stubs --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL];
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static uint32_t host_dim_lo;                      /* hal/fm1_input.h: the glow MENU > LEDS asked for (1: DIM LO) */
static void fm1_led_dim_level(uint32_t lo) { host_dim_lo = lo; }
static int host_anim;                             /* hal/fm1_input.h: the power-on sweep running (it has the LEDs) */
static int fm1_led_anim_on(void) { return host_anim; }
#define FM1_TICKS_PER_US 1u
static uint32_t host_ticks, host_pressed, host_notes;
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return host_ticks; }
static uint32_t fm1_input_edges(int x) { uint32_t p = host_pressed; (void)x; host_pressed = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
/* host_enc_late: detents the TIMER5 scan counts just after a read (#102: while ui_input runs); the next read has them */
static int32_t host_enc_late[7];
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = host_enc[e % 7u];
    host_enc[e % 7u] = host_enc_late[e % 7u];
    host_enc_late[e % 7u] = 0;
    return s;
}
static void (*host_wdt_hook)(void);                /* (a test's: time passing in a blocking loop, as panel_setup's) */
static void fm1_wdt_feed(void) { if (host_wdt_hook) host_wdt_hook(); }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static uint16_t host_screen[240 * 240];
/* (GEN fix) a flash: a pixel that changed during a frame and ended it as it began (a page drawn, then something over
 * it again; a body cleared and drawn the same): on the panel the eye sees it (the LCD shows every write). host_px
 * writes a pixel and counts; frame() adds a frame's flashes to host_flash */
static uint8_t host_chg[240 * 240];
static uint16_t host_px0[240 * 240];
static uint32_t host_dirty, host_flash, host_flash_frames, host_fx0 = 240, host_fy0 = 240, host_fx1, host_fy1;
static inline void host_px(uint32_t i, uint16_t v)
{
    if (host_screen[i] == v) return;
    if (!host_chg[i]) host_px0[i] = host_screen[i];
    if (host_chg[i] < 255u) host_chg[i]++;
    host_screen[i] = v;
    host_dirty = 1;
}
static void host_flash_end(void)
{
    uint32_t i, n = 0;
    if (!host_dirty) return;
    for (i = 0; i < 240u * 240u; i++) {
        if (host_chg[i] >= 2u && host_screen[i] == host_px0[i]) {
            uint32_t x = i % 240u, y = i / 240u;
            n++;
            if (x < host_fx0) host_fx0 = x;
            if (x > host_fx1) host_fx1 = x;
            if (y < host_fy0) host_fy0 = y;
            if (y > host_fy1) host_fy1 = y;
        }
        host_chg[i] = 0;
    }
    host_dirty = 0;
    host_flash += n;
    host_flash_frames += n != 0u;
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
#ifdef UI_FILL_HOOK
    UI_FILL_HOOK(x, y, w, h);                     /* (tests/ui_render.c: what a fill covers is gone) */
#endif
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_px((y + j) * 240u + x + i, (uint16_t)((c >> 8) | (c << 8)));
}
static void lcd_sync(void) {}
/* MENU > SCREEN OFF (lcd.c lcd_power / lcd_wake_now): what the panel was told, in order (0 off: DISPOFF, SLPIN;
 * 1 SLPOUT; 2 DISPON; 3 lcd_wake_now), with the time; host_bl the panel showing (the backlight itself stays on: PA2 is
 * the key matrix's /OE too, lcd.c lcd_power) */
static uint32_t host_pw_n, host_pw[256], host_pw_ms[256];
static int host_bl = 1;
static void host_pw_log(uint32_t s)
{
    if (host_pw_n < 256u) { host_pw[host_pw_n] = s; host_pw_ms[host_pw_n] = fm1_ms; }
    host_pw_n++;
    host_bl = s != 0u;
}
static void lcd_power(uint32_t s) { host_pw_log(s); }
static void lcd_wake_now(void) { host_pw_log(3u); }
#define SCOPE_N 512u                              /* audio.c: the HOME oscilloscope */
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
static uint32_t host_blit_rows;                    /* rows blitted (what a frame draws) */
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    host_blit_rows += h;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) host_px((y + j) * 240u + x + i, p[j * w + i]);
}
static struct { uint32_t stage; } felucca_dbg;
#define FELUCCA_FLASH 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "TEST"
#endif
#ifndef UI_NO_DRIFT
#define UI_NO_DRIFT 1                                    /* (0.6.61) no drifting granules: frames compared pixel by pixel */
#endif
#ifndef PF_MS                                            /* (0.6.58) no page name drawn: the tests read the screen */
#define PF_MS 0u                                         /* (ui_render.c sets the real one: it shows it) */
#endif
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_graph.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/ui_layer.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* main.c felucca_init */
static void ui_power_on(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    live.mode = 0;                                /* (0.6.81: PLAY the loop, as at power-on; a DICE ALL leaves SONG) */
    for (i = 0; i < 4u; i++)                      /* (and the sections a DICE ALL wrote emptied: power-on has none) */
        if ((dice_secs >> i) & 1u) memset(proj_slot[i].raw, 0, 4);
    dice_secs = 0;
    memset(&ui, 0, sizeof ui);
    gen_power_on();                               /* (QG-1 audit) no GEN settings from the test before */
    cin_left = cin_total = 0;                     /* (no count-in) */
    memset(&nm, 0, sizeof nm);                    /* NAME closed, no project name */
    proj_name[0] = 0;
    proj_cur = PROJ_NO_SLOT;
    memset(&favorites, 0, sizeof favorites);
    ui_prefs = PREF_ACCEL_OFF;                    /* (the tests turn a detent a frame: one step each; KNOB ACCEL, ON on the
                                                   * device by default, has its own test) */
    mac_latch = 0;
    fx_lock = 0;                                  /* (0.6.44) */
    memset(pat_last, 0, sizeof pat_last);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(up_bank, 0, sizeof up_bank);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    load_depth++;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[i][0]);
        apply_preset_to(t, TRK_DEF[i][1]);
        t->engine = t->eng_req;
        track_defaults_steps(t);
        if (TRK_DEF[i][2])
            load_pat16(t, PATTERNS[TRK_DEF[i][2] - 1u].note, PATTERNS[TRK_DEF[i][2] - 1u].flags);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = TRK_DEF[i][2];
    }
    load_depth--;
    song.sel = 0;
    ui.home = 1;
    ui.force = 1;
    panel = PANEL_DEFAULT;
    settings.palette = UI_GREY_INDEX;
    palette_set(UI_GREY_INDEX);
    transport_req = 0;
    panic_req = 0;
    mi_r = mi_w = midi_in_overflow = 0;
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_on, 0, sizeof kb_on);                     /* no key held */
    perf_held = perf_act = kb_layer = perf_mask = 0;      /* the FX layer: nothing held */
    perf_kill = 0;
    perf_mask = 1u << panel.btn[B_FX];                  /* (ui_input sets them every frame) */
    kb_mask = layer_bits();
    perf_solo = 0;
    memset(&lys, 0, sizeof lys);
    perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    settings_hold = HOLD_DEF;
    fm1_in.buttons = fm1_in.notes = 0;
    memset(&motion, 0, sizeof motion);                  /* (main.c: no motion at power-on) */
    memset(motion_active, 0, sizeof motion_active);
    memset(motion_locked, 0, sizeof motion_locked);
    motion_base_valid = motion_full = 0;
    memset(&scrn, 0, sizeof scrn);                        /* MENU > SCREEN OFF: on, idle from now (main.c) */
    scrn.idle = fm1_ms;
    kb_asleep = 0;
    panel_moved = 0;
}
/* card slot drawn with MOTION's mark (ui_draw.c card_mot: its icon, or a long label in the theme colour) */
static int card_motion(uint32_t slot) { return (card_mot >> slot) & 1u; }

/* one UI frame (input, then the drawing on the stub display) */
static void frame(void)
{
    host_ticks += 16000u;
    fm1_ms += 16u;
    ui_input();
    ui_draw();
    host_flash_end();
}
static void press(uint32_t label)                 /* a tap: down, a frame, up, a frame */
{
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) frame(); }
static void song_poll(void);
/* (TAKES v2) the music in real time: a frame's 16 ms of audio each frame, the song's sections staged (the main loop) */
static void play_ms(uint32_t ms)
{
    uint32_t i, j;
    for (i = 0; i < ms / 16u; i++) {
        frame();
        for (j = 0; j < 768u / CTL; j++) { events_block(CTL); song_poll(); }
    }
}
static void hold(uint32_t label)                  /* held 0.8 s, then let go */
{
    uint32_t k;
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    for (k = 0; k < 52u; k++)
        frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    frame();
    host_ticks += 200000u;                        /* (no knob acceleration between the turns) */
}
static void held_turn(uint32_t label, int32_t s)  /* (0.6.67) a button held + SELECT, let go after */
{
    fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label];
    turn(EN_SELECT, s);
    fm1_in.buttons &= ~(1u << panel.btn[label]);
}
static void tempo_turn(int32_t s) { held_turn(B_GLO, s); }   /* (0.6.67) the tempo: GLO + SELECT */
static void stop_transport(void) { transport_req = 0; song.playing = 0; }
static int16_t stored_param(uint32_t slot, uint32_t track, uint32_t id)
{
    project_t p;
    return proj_import(&p, &proj_slot[slot], sizeof proj_slot[slot]) ? p.t[track].p[id] : -32768;
}
static uint8_t stored_note(uint32_t slot, uint32_t track, uint32_t step)
{
    project_t p;
    return proj_import(&p, &proj_slot[slot], sizeof proj_slot[slot]) ? p.t[track].step[step].note[0] : 0;
}
static int msg_is(const char *s) { return ui.msg_t && str_eq(ui.msg, s); }
static void my_steps(track_t *t)                  /* "recorded" steps */
{
    uint32_t i;
    track_defaults_steps(t);
    for (i = 0; i < 32u; i += 3u) {
        t->step[i].note[0] = (uint8_t)(40 + i);
        t->step[i].n = 1;
        t->step[i].time = ST_NOTE;
        t->step[i].vel = 99;
    }
    t->p[P_SLEN] = 32;
}

static void go_page(uint32_t graph)               /* the page that draws graph */
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == graph)
            break;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static int steps_are(const track_t *t, uint32_t n)  /* steps 1..16 = PATTERNS[n], 17..64 empty, LEN 16 */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        uint8_t want = i < 16u ? PATTERNS[n].note[i] : 0u;
        if (t->step[i].note[0] != want || t->step[i].n != (want ? 1u : 0u))
            return 0;
    }
    return t->p[P_SLEN] == 16;
}
/* the sound of a track: engine, preset and the parameters that are not the track's own (param_kept) */
static int same_sound(const track_t *a, const track_t *b)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        if (!param_kept(i) && a->p[i] != b->p[i])
            return 0;
    return a->eng_req == b->eng_req && a->preset == b->preset;
}

/* the track's E values are DRUM's default kit (core.h DRUM_KIT_E: what a SAMPLE PERC sound loads as) */
static int perc_kit(const track_t *t)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    return !memcmp(&t->p[P_E0], KIT, sizeof KIT);
}
/* the GM drums a DRUM track plays: a kick, a snare and a closed hat each strike their lane and sound */
static int gm_hits_play(track_t *t)
{
    static const uint8_t N[3] = {36, 38, 42}, L[3] = {DV_KICK, DV_SNARE, DV_HATC};
    int32_t o[2u * CTL];
    uint32_t k, i, b, ok = 1, m = t->p[P_MUTE], mq = song.master_q12;
    t->p[P_MUTE] = 0;
    song.master_q12 = 4096;                       /* (the MASTER knob: main.c; 0 in this test) */
    for (b = 0; b < 256u && (t->xf_on || t->eng_req != t->engine); b++)   /* the engine switch done (its fade) */
        mix_block(o, CTL);
    for (k = 0; k < 3u; k++) {
        uint64_t e = 0;
        int lane = 0;
        trk_note_on(t, N[k], 100);
        for (b = 0; b < 16u; b++) {
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++)
                e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
            for (i = 0; i < NVOICE; i++)
                lane |= t->v[i].active && t->v[i].note == N[k] && (uint32_t)t->v[i].s[0] == L[k];
        }
        trk_note_off(t, N[k]);
        ok &= ENGINES[t->engine] == &ENG_DRUM && drum_lane(N[k]) == L[k] && lane && e > 0;
    }
    t->p[P_MUTE] = (int16_t)m;
    song.master_q12 = mq;
    return (int)ok;
}

/* sounds never touch the steps; the arp, the scale, the SLICER and the pattern parameters are the track's */
static int test_sound_loads(void)
{
    int bad = 0;
    uint32_t i, empty = 1;
    track_t *t = &trk[0], before;
    ui_power_on();
    for (i = 0; i < NTRK; i++)
        empty &= (uint32_t)seq_is_empty(&trk[i]);
    bad += check("power-on: the four sounds, every sequencer empty, no load open", empty && load_depth == 0 &&
                 trk[3].eng_req == ENGI_DRUM && trk[3].preset == 0u && trk[0].preset == TRK_DEF[0][1]);
    my_steps(t);
    t->p[P_E0 + 1] = 77;                          /* a sound edit */
    t->p[P_AMODE] = 2;                            /* and the track's own settings */
    t->p[P_AOCT] = 3;
    t->p[P_SCALE] = 2;
    t->p[P_TRANS] = 5;
    t->p[P_SLCR] = SL_GATE;
    t->p[P_SLPAT] = 4;
    t->p[P_SDIV] = 1;
    t->p[P_LEVEL] = 90;
    before = *t;
    turn(EN_PRESET, 1);                           /* HOME: the next preset */
    bad += check("PRESETS on HOME loads the sound: steps, LEN, DIV untouched", t->preset == (uint8_t)((before.preset + 1u) % ENGINES[0]->npresets) &&
                 !memcmp(t->step, before.step, sizeof t->step) && t->p[P_SLEN] == 32 && t->p[P_SDIV] == 1);
    bad += check("..ARP, SCL, the SLICER and the mix stay the track's", t->p[P_AMODE] == 2 && t->p[P_AOCT] == 3 && t->p[P_SCALE] == 2 &&
                 t->p[P_TRANS] == 5 && t->p[P_SLCR] == SL_GATE && t->p[P_SLPAT] == 4 && t->p[P_LEVEL] == 90);
    bad += check("..and no warning about the sequence", !msg_is("T1 SEQ REPLACED"));
    for (i = 0; i < ENGINES[0]->npresets; i++)
        if (str_eq(ENGINES[0]->presets[i].name, "RAVE"))
            break;
    t->p[P_AMODE] = 0;
    t->preset = (uint8_t)(i - 1u);
    turn(EN_PRESET, 1);
    bad += check("RAVE (an ARP preset) leaves the arp off; it suggests pattern 13 ARP", t->preset == i && t->p[P_AMODE] == 0 &&
                 preset_pat_hint() == 12 && ui.ppick == 12 && str_eq(PATTERNS[12].name, "ARP"));
    t->p[P_AMODE] = 2;
    before = *t;
    for (i = 0; i < 64u && t->eng_req == before.eng_req; i++)   /* several loads, into the next engine */
        turn(EN_PRESET, 1);
    bad += check("browsing on, into another engine: the steps still untouched", t->eng_req != before.eng_req &&
                 !memcmp(t->step, before.step, sizeof t->step) && t->p[P_AMODE] == 2);
    t->step[0].note[0] = 99;                      /* recorded after the loads */
    {
        track_t now = *t;
        hold(B_SAVE);
        bad += check("SAVE held (TONIC: no undo): the sound and the steps stay, no SAVE page", same_sound(t, &now) &&
                     t->step[0].note[0] == 99 && ui.home);
    }
    press(B_SAVE);
    bad += check("SAVE tap opens the SAVE pages (on release)", !ui.home && cur_page()->fam == FAM_SAVE);
    /* TOOLS INIT, user preset load: the sound only, undoable; one copy for all tracks */
    ui_power_on();
    my_steps(&trk[1]);
    trk[1].p[P_SLCR] = SL_STUT;
    before = trk[1];
    track_select(1);
    set_engine(trk[1].eng_req);                   /* TOOLS INIT */
    bad += check("TOOLS INIT: the sound only (steps and SLICER kept)",
                 !memcmp(trk[1].step, before.step, sizeof before.step) && trk[1].p[P_SLCR] == SL_STUT);
    track_select(0);
    my_steps(&trk[0]);
    trk[0].p[P_SDIV] = 2;
    up_store(3, "MINE");
    bad += check("a user preset keeps its pattern in the record (format unchanged)", up_has_pat(3) && up_rec(3)->note[0] == 40);
    trk[0].p[P_SDIV] = 0;
    track_defaults_steps(&trk[0]);
    trk[0].step[5].note[0] = 33;
    trk[0].step[5].n = 1;
    before = trk[0];
    up_load(3);
    bad += check("a user preset load: the sound only (steps, DIV kept)", trk[0].user == 4u &&
                 !memcmp(trk[0].step, before.step, sizeof before.step) && trk[0].p[P_SDIV] == 0);
    project_save(1);
    up_load(3);
    project_load(1);
    bad += check("a project load leaves no load open", load_depth == 0);
    ui_power_on();
    host_legacy_sample_perc(t);                /* SAMPLE PERC (SET 4, retired after 1.0.2): every load gives DRUM */
    t->preset = 4;                             /* a project written before the factory removal */
    my_steps(t);
    t->p[P_LEVEL] = 71;
    t->p[P_REV] = 33;
    before = *t;
    project_save(0);
    apply_preset_to(t, 0);
    track_defaults_steps(t);
    project_load(0);
    bad += check("saved SAMPLE PERC project: a DRUM track (its kit), the rest of the sound and every step kept",
                 t->eng_req == ENGI_DRUM && t->preset == 0u && perc_kit(t) &&
                 !memcmp(t->p, before.p, P_E0 * sizeof t->p[0]) && !memcmp(t->step, before.step, sizeof t->step));
    bad += check("  its GM hits play as drums (kick, snare, closed hat on their lanes)", gm_hits_play(t));
    {   /* a user preset of SAMPLE PERC (UP_PUT from an older editor, or a bank read): DRUM, its kit */
        up_rec_t r;
        int16_t keep[P_COUNT];
        uint32_t k;
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = ENGI_SAMPLE; r.np = P_COUNT;
        memcpy(r.name, "OLD PERC", 8);
        host_legacy_sample_perc(t);
        for (k = 0; k < P_COUNT; k++)
            up_set_value(&r, k, t->p[k]);
        up_migrate(&r);
        *up_rec(7) = r;
        set_engine_of(t, 0);
        memcpy(keep, t->p, sizeof keep);
        song.sel = (uint8_t)(t - trk);
        up_load(7);
        bad += check("SAMPLE PERC user preset: loads as a DRUM track (its kit)", t->eng_req == ENGI_DRUM && perc_kit(t) &&
                     t->p[P_LEVEL] == keep[P_LEVEL]);
        bad += check("  its GM hits play as drums", gm_hits_play(t));
        memset(up_rec(7), 0, sizeof r);
    }
    set_engine_of(t, 0);
    apply_preset_to(t, SMP_SET_PERC);             /* (engine 0: an ANALOG preset 4, not PERC) */
    bad += check("factory preset 4 of another engine stays that engine's", t->eng_req == 0u && t->preset == 4u);
    t->eng_req = t->engine = ENGI_SAMPLE;         /* (a SAMPLE sound arriving: SAMPLE is retired, set_engine_of gives ANALOG) */
    apply_preset_to(t, SMP_SET_PERC);             /* SAMPLE preset 4 (once PERC): the editor's PRESET, a favourite */
    bad += check("SAMPLE preset 4 (once PERC): DRUM's kit", t->eng_req == ENGI_DRUM && t->preset == 0u && perc_kit(t) &&
                 gm_hits_play(t));
    ui_power_on();
    host_legacy_sample_perc(&trk[3]);
    my_steps(&trk[3]);
    trk[3].p[P_SDIV] = 3;
    before = trk[3];
    project_save(1);
    project_v6_t legacy = {0};
    project_t decoded;
    proj_import(&decoded, &proj_slot[1], sizeof proj_slot[1]);
    legacy.magic = PROJ_MAGIC_V6; legacy.size = sizeof legacy;
    legacy.parts = 0; legacy.phys = PROJ_PHYS;
    memcpy(legacy.g, decoded.g, sizeof legacy.g);
    for (uint32_t ti = 0; ti < PROJ_LT; ti++) {
        memcpy(legacy.t[ti].p, decoded.t[ti].p, 61u * sizeof(int16_t));
        memcpy(legacy.t[ti].p + 61, decoded.t[ti].p + P_E0, 8u * sizeof(int16_t));
        legacy.t[ti].engine = decoded.t[ti].engine; legacy.t[ti].preset = decoded.t[ti].preset;
        for (uint32_t st = 0; st < NSTEP; st++) memcpy(&legacy.t[ti].step[st], &decoded.t[ti].step[st], sizeof(step10_t));
    }
    legacy.t[3].engine = 0;
    legacy.g[G_DRLVL] = 71; legacy.g[G_DRREV] = 43;
    legacy.g[G_RTYPE] = 10;                               /* (id 24 was the GM drum part's MIDI channel: 10) */
    legacy.sum = proj_hash(&legacy, offsetof(project_v6_t, sum));
    memcpy(&proj_slot[1], &legacy, sizeof legacy);
    project_load(1);
    bad += check("old GM projects (the drum part) load as DRUM's kit, their mix and steps kept",
                 trk[3].eng_req == ENGI_DRUM && trk[3].preset == 0u && perc_kit(&trk[3]) &&
                 trk[3].p[P_LEVEL] == 71 && trk[3].p[P_REV] == 43 && trk[3].p[P_SDIV] == 3 &&
                 !memcmp(trk[3].step, before.step, sizeof trk[3].step));
    bad += check("  their GM hits play as drums", gm_hits_play(&trk[3]));
    bad += check("  its old drum channel (10, in id 24) loads as REVERB TYPE ROOM", song.g[G_RTYPE] == 0);
    return bad;
}

/* SEQ > PATTERNS: K1 PAT, OCT+ LOAD; the dialog over the user's steps */
/* (0.6.41) smoother switches (voice.c engine_block): another engine keeps a held note sounding (faded out, then in on
 * the new one); a WAVE changed under a held note dips (out on the old wave, in on the new) instead of jumping */
static uint32_t tr_jump(track_t *t, uint32_t blocks, uint32_t at, int16_t wave, int dip_on)
{
    int32_t o[2u * CTL], prev = 0;
    uint32_t b, i, big = 0;
    for (b = 0; b < blocks; b++) {
        if (b == at) {
            t->p[P_E0] = wave;
            if (!dip_on) t->pe_old[0] = wave;            /* (as before 0.6.41: the new wave at once, no dip) */
        }
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t d = o[2u * i] - prev;
            prev = o[2u * i];
            if (b + 2u >= at && b < at + 8u && (uint32_t)(d < 0 ? -d : d) > big) big = (uint32_t)(d < 0 ? -d : d);
        }
    }
    return big;
}
static int test_transitions(void)
{
    int bad = 0, ok;
    uint32_t k, b, held, j0, j1;
    int32_t o[2u * CTL];
    track_t *t;
    ui_power_on();
    t = &trk[0];
    song.master_q12 = 4096;
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    set_engine_of(t, ENGI_ANALOG);
    t->engine = t->eng_req;
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 0;
    for (b = 0; b < 8u; b++) mix_block(o, CTL);         /* (the load's release done) */
    trk_note_on(t, 60, 100);
    for (b = 0; b < 40u; b++) mix_block(o, CTL);
    set_engine_of(t, 3u);                                /* LOFI, the note still held */
    for (b = 0; b < 4u; b++) events_block(CTL), mix_block(o, CTL);
    ok = t->xf_on && t->xf_len == 16u;
    for (b = 0; b < 40u; b++) events_block(CTL), mix_block(o, CTL);
    for (k = 0, held = 0; k < NVOICE; k++) held |= t->v[k].active && t->v[k].gate && t->v[k].note == 60u;
    ok &= t->engine == 3u && held;
    bad += check("another engine under a held note: faded out over 16 blocks, the note on in the new engine", ok);
    trk_note_off(t, 60);
    /* ANALOG SIN -> TRI under a held note: a dip, the voices on; the jump against the same change at once */
    ui_power_on();
    t = &trk[0];
    song.master_q12 = 4096;
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    set_engine_of(t, ENGI_ANALOG);
    t->engine = t->eng_req;
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 0;
    t->p[P_E0] = 3; t->p[P_E1] = 0; t->p[P_E3] = 0;    /* SIN, no detune, no noise */
    for (b = 0; b < 8u; b++) mix_block(o, CTL);
    trk_note_on(t, 57, 100);
    for (b = 0; b < 60u; b++) mix_block(o, CTL);
    j1 = tr_jump(t, 30u, 7u, 2, 1);
    for (k = 0, held = 0; k < NVOICE; k++) held |= t->v[k].active && t->v[k].gate && t->v[k].note == 57u;
    ok = held;
    t->p[P_E0] = 3; for (b = 0; b < 60u; b++) mix_block(o, CTL);
    j0 = tr_jump(t, 30u, 7u, 2, 0);
    bad += check("WAVE changed under a held note: a dip (the voice goes on), a smaller jump than at once", ok && j1 < j0);
    trk_note_off(t, 57);
    return bad;
}
/* (0.6.43) MUTATE (project.c mutate_poll): on the bar, AMOUNT 64 when turned on, never LEVEL / PAN, ORBIT near the
 * anchor, the kick kept, FREEZE stops it */
static void mut_bars(uint32_t bars)
{
    int32_t o[2u * CTL];
    uint32_t b, n = bars * 4u * beat_samples() / CTL + 4u;
    for (b = 0; b < n; b++) { mix_block(o, CTL); if (!(b & 7u)) mutate_poll(); }
}
static int test_mutate(void)
{
    int bad = 0, ok;
    uint32_t k, i, kick0 = 0, kick1 = 0;
    int16_t lvl, pan, e0[8];
    track_t *t, *d;
    ui_power_on();
    t = &trk[0];
    d = &trk[3];
    mut_freeze = 0;
    ok = mut_progress(0) == 0xFFFFu;
    t->p[P_MSND] = 1;                                     /* every bar */
    mutate_poll();
    ok &= t->p[P_MAMT] == 64 && mut.ok;
    bad += check("MUTATE: off, no ring; SOUND on: AMOUNT 64, the anchor taken", ok);
    for (i = 0; i < 8u; i++) e0[i] = t->p[P_E0 + i];
    lvl = t->p[P_LEVEL]; pan = t->p[P_PAN];
    press(B_PLAY);
    mutate_poll();
    mut_bars(1);
    ok = bar_n >= 1u;
    for (i = 0, k = 0; i < 8u; i++) k += t->p[P_E0 + i] != e0[i];
    ok &= k > 0u && t->p[P_LEVEL] == lvl && t->p[P_PAN] == pan;
    bad += check("MUTATE: on the bar the sound changed, LEVEL and PAN never", ok);
    mut_bars(6);                                          /* ORBIT: still near the anchor */
    for (i = 0, ok = 1; i < 8u; i++) {
        const param_desc_t *pd = track_desc(t, P_E0 + i);
        if (pd->fmt != F_ENUM)
            ok &= abs(t->p[P_E0 + i] - e0[i]) <= (pd->max - pd->min) * 64 / 256 + 1;
    }
    bad += check("MUTATE ORBIT: after 7 bars every value within AMOUNT of the anchor", ok);
    /* PATTERN on the DRUM track: hits change, the kick lane never */
    for (i = 0; i < 16u; i++) { d->step[i] = (step_t){{0}, 0, ST_NOTE, 0, 100}; d->step[i].hit = i % 4u ? 0x08u : 0x09u; }
    for (i = 0; i < 16u; i++) kick0 |= (uint32_t)(d->step[i].hit & 1u) << i;
    d->p[P_MPAT] = 1; d->p[P_MAMT] = 127;
    mutate_poll();
    mut_bars(3);
    for (i = 0, k = 0; i < 16u; i++) { kick1 |= (uint32_t)(d->step[i].hit & 1u) << i; k += d->step[i].hit != (i % 4u ? 0x08u : 0x09u); }
    bad += check("MUTATE PATTERN on DRUM: hits changed, the kick lane as it was", k > 0u && kick0 == kick1);
    /* FREEZE: nothing changes */
    for (i = 0; i < 8u; i++) e0[i] = t->p[P_E0 + i];
    mut_freeze = 1;
    mut_bars(2);
    for (i = 0, k = 0; i < 8u; i++) k += t->p[P_E0 + i] != e0[i];
    mut_freeze = 0;
    bad += check("MUTATE FREEZE: no change while frozen", k == 0u);
    /* (0.6.68) a sequence cleared while it mutates never comes back from the anchor; an empty kit stays empty */
    d->p[P_MPAT] = 0;
    for (i = 0; i < 16u; i++) { t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0}; if (!(i & 1u)) { t->step[i].n = 1; t->step[i].note[0] = (uint8_t)(60 + i); t->step[i].time = ST_NOTE; t->step[i].vel = 100; } }
    t->p[P_MSND] = 0; t->p[P_MPAT] = 1; t->p[P_MAMT] = 60;
    mutate_poll(); mut_bars(2);
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    mut_bars(4);
    for (i = 0, k = 0; i < NSTEP; i++) k += step_on(&t->step[i]);
    for (i = 0; i < NSTEP; i++) step_clear(&d->step[i]);
    d->p[P_MPAT] = 1; d->p[P_MAMT] = 127; d->p[P_MMODE] = 1;   /* (DRIFT, all the AMOUNT) */
    mut_bars(4);
    for (i = 0; i < NSTEP; i++) k += step_on(&d->step[i]);
    d->p[P_MMODE] = 0;
    bad += check("0.6.68: MUTATE PATTERN: a cleared sequence stays silent (no melody back from the anchor), an empty kit too", k == 0u);
    stop_transport();
    t->p[P_MSND] = 0; d->p[P_MPAT] = 0;
    mutate_poll();
    return bad;
}
/* (0.6.45) MORPH: ANALOG, PHASE, LOFI: 0 keeps the WAVE's own render (an_mpos -1), more slides it toward the next
 * waves, the three play with it; its page shows on them, not on FM6 */
static int32_t morph_peak(uint32_t eng, int16_t mo)
{
    int32_t o[2u * CTL], pk = 0;
    uint32_t b, i, k;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    song.master_q12 = 4096;
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    set_engine_of(t, eng);
    t->engine = t->eng_req;
    t->p[P_MORPH] = mo;
    for (b = 0; b < 8u; b++) mix_block(o, CTL);
    trk_note_on(t, 57, 100);
    for (b = 0; b < 40u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) pk = o[i] > pk ? o[i] : -o[i] > pk ? -o[i] : pk;
    }
    return pk;
}
static int test_morph(void)
{
    int bad = 0, ok = 1;
    uint32_t e, k, ph;
    int16_t p[P_COUNT] = {0};
    vmod_t m;
    static const uint32_t ENG[3] = {ENGI_ANALOG, 2u, 3u};   /* ANALOG, PHASE, LOFI */
    memset(&m, 0, sizeof m);
    m.shape = 64 << 8;
    ok &= an_mpos(p, 0, &m) == -1;                           /* 0: the WAVE's own render */
    ok &= an_mpos(p, 127 << 8, &m) == 768;                   /* SAW all the way to SIN */
    m.shape = 127 << 8;
    ok &= an_mpos(p, 0, &m) > 0;                                /* SHAPE modulation moves it */
    for (ph = 0; ph < 0xF0000000u; ph += 0x0F000000u)
        ok &= an_morph(256, ph, 1u << 24) == an_osc(1u, ph, 1u << 24) && an_morph(0, ph, 1u << 24) == an_osc(0u, ph, 1u << 24);
    bad += check("MORPH (ANALOG): 0 = the WAVE as it is, 127 = SIN, SHAPE moves it, whole steps are the next wave", ok);
    for (e = 0, ok = 1; e < 3u; e++) ok &= morph_peak(ENG[e], 90) > 1000;
    bad += check("MORPH 90 on ANALOG PHASE LOFI plays", ok);
    ui_power_on();
    set_engine_of(TSEL, ENGI_ANALOG);
    for (k = 0, ok = 0; k < NPAGES; k++) if (PAGES[k].graph == GR_MORPH) ok = page_visible(k);
    set_engine_of(TSEL, ENGI_FM6);
    for (k = 0; k < NPAGES; k++) if (PAGES[k].graph == GR_MORPH) ok &= !page_visible(k);
    bad += check("EDIT > MORPH on ANALOG, not on FM6", ok);
    return bad;
}
/* (0.6.46) INIT SOUND dry (no effect sends), FM6 the init voice (OP1 alone at 99, SLOT OWN); NOISE DUST at DENS 0
 * still crackles */
static int test_init_dry(void)
{
    int bad = 0, ok = 1;
    uint32_t k;
    const uint8_t *pt;
    ui_power_on();
    set_engine_of(TSEL, ENGI_FM6);
    TSEL->engine = TSEL->eng_req;
    for (k = 0; k < 4u; k++) TSEL->p[P_DIST + k] = 50;
    init_sound_of(TSEL, ENGI_FM6);
    for (k = 0; k < 4u; k++) ok &= TSEL->p[P_DIST + k] == 0;
    pt = fm6_patch[song.sel % NTRK];
    ok &= TSEL->p[P_E7] == FM6_OWN && pt[5u * FP_OP + FP_OL] == 99;
    for (k = 1; k < 6u; k++) ok &= pt[(5u - k) * FP_OP + FP_OL] == 0;
    fm6_poll();
    ok &= fm6_patch[song.sel % NTRK][5u * FP_OP + FP_OL] == 99 && fm6_patch[song.sel % NTRK][4u * FP_OP + FP_OL] == 0;
    bad += check("INIT SOUND: dry (sends 0), FM6 the init voice (OP1 alone, SLOT OWN)", ok);
    set_engine_of(TSEL, ENGI_ANALOG);
    init_sound_of(TSEL, ENGI_ANALOG);
    for (k = 0, ok = 1; k < 4u; k++) ok &= TSEL->p[P_DIST + k] == 0;
    bad += check("INIT SOUND on ANALOG: dry", ok);
    {
        int32_t o[2u * CTL], pk = 0;
        uint32_t b, i;
        track_t *t;
        ui_power_on();
        t = &trk[0];
        song.master_q12 = 4096;
        for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
        set_engine_of(t, 11u);                                  /* NOISE */
        t->engine = t->eng_req;
        t->p[P_E0] = 1;                                         /* DUST */
        t->p[P_E5] = 0;                                         /* DENS 0 */
        for (b = 0; b < 8u; b++) mix_block(o, CTL);
        trk_note_on(t, 60, 100);
        for (b = 0; b < 400u; b++) {
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++) pk = o[i] > pk ? o[i] : -o[i] > pk ? -o[i] : pk;
        }
        bad += check("NOISE DUST at DENS 0 still crackles", pk > 200);
    }
    return bad;
}
/* (0.6.48) DRUM-X IMAGE and the sounds' SENDs: IMAGE 0 and no SEND the part as mono as before; IMAGE puts each sound
 * on its side; a sound's SEND feeds its bus alone; the SENDs go with the project */
static void dximg_hit(uint32_t note, int16_t img, int64_t *el, int64_t *er, int64_t *sr)
{
    int32_t o[2u * CTL];
    uint32_t b, i, k;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    song.master_q12 = 4096;
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = t->p[P_DIST] = 0;
    t->p[P_DXIMG] = img;
    for (b = 0; b < 40u; b++) mix_block(o, CTL);       /* (past the engine change's crossfade: voice.c XF_ENG) */
    trk_note_on(t, note, 110);
    *el = *er = *sr = 0;
    for (b = 0; b < 60u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            *el += (int64_t)o[2u * i] * o[2u * i] >> 8;
            *er += (int64_t)o[2u * i + 1u] * o[2u * i + 1u] >> 8;
            *sr += send_r[i] < 0 ? -send_r[i] : send_r[i];
        }
    }
}
static int test_dximg(void)
{
    int bad = 0, ok = 1;
    int64_t el, er, sr;
    int16_t img;
    uint32_t l, side = 0;
    project_store_t st;
    memset(dx_snd, 0, sizeof dx_snd);
    dximg_hit(38, 0, &el, &er, &sr);                     /* the snare, IMAGE 0 */
    ok &= el > 0 && el - er < el / 32 && er - el < el / 32 && sr == 0 && !dx_xon;   /* (the master's own tiny width) */
    bad += check("DRUM IMAGE 0, no SEND: the part mono as before (L = R), nothing to the reverb", ok);
    for (img = 20, ok = 0; img < 128 && !ok; img++)     /* an IMAGE that puts the snare well to one side */
        if (dx_img_pan(DV_SNARE, img) > 40 || dx_img_pan(DV_SNARE, img) < -40) {
            int32_t p = dx_img_pan(DV_SNARE, img);
            dximg_hit(38, img, &el, &er, &sr);
            ok = p > 0 ? er > el * 4 : el > er * 4;
            side = p > 0;
        }
    bad += check(side ? "DRUM IMAGE: the snare to the right" : "DRUM IMAGE: the snare to the left", ok);
    for (img = 1, ok = 1; img < 128; img++)
        for (l = 0; l < DV_NLANE; l++)
            ok &= dx_img_pan(l, img) >= -64 && dx_img_pan(l, img) <= 64 && (l != DV_KICK || (dx_img_pan(l, img) >= -16 &&
                  dx_img_pan(l, img) <= 16));
    bad += check("DRUM IMAGE: pans in range, the kick near the middle", ok);
    memset(dx_snd, 0, sizeof dx_snd);
    dx_snd[DV_SNARE] = 3u << 6 | 63u;                   /* the snare alone to the REVERB */
    {
        int64_t a, b2, s1, s2;
        uint8_t keep[8];
        memcpy(keep, dx_snd, sizeof keep);
        dximg_hit(38, 0, &a, &b2, &s1);
        memcpy(dx_snd, keep, sizeof keep);
        dximg_hit(36, 0, &a, &b2, &s2);                 /* the kick: no SEND */
        bad += check("DRUM SEND: the snare's REVERB feeds the bus, the kick's none", s1 > 0 && s2 == 0);
    }
    ui_power_on();
    memset(dx_snd, 0, sizeof dx_snd);
    dx_snd[2] = 2u << 6 | 40u;
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    memset(dx_snd, 0, sizeof dx_snd);
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= dx_snd[2] == (2u << 6 | 40u) && !dx_snd[0];
    bad += check("DRUM SEND: kept with the project (FUNC 'D' 'S')", ok);
    memset(dx_snd, 0, sizeof dx_snd);
    {   /* (1.2.1-GW, JIANT 0.7.8) the two cores: the other core's part starting (voice.c track_render clears dx_xon)
         * leaves this core's DRUM part's image and sides as they were (they were one set: the image on and off block
         * by block, clicks) */
        uint8_t keep_cpu = mc_host_cpu;
        memset(dx_core, 0, sizeof dx_core);
        mc_host_cpu = 0;
        dx_xon = 1;
        dx_sl[3] = 1234;
        mc_host_cpu = 1;
        dx_xon = 0;                                     /* the other core: a part that is not DRUM */
        dx_sl[3] = 0;
        mc_host_cpu = 0;
        bad += check("DRUM IMAGE: each core its own image and sides (the other core's part leaves them)",
                     (dx_xon & 1u) && dx_sl[3] == 1234);
        mc_host_cpu = keep_cpu;
        memset(dx_core, 0, sizeof dx_core);
    }
    ui_power_on();
    return bad;
}
/* (0.6.49) DRUM-X LAYER and each sound's FEEDBK: 0 runs the sound as before; LAYER moves it; kept with the project;
 * a new project has the factory LAYERs, an older one none; NEW PROJECT: every track dry, the engine's own defaults */
static uint64_t dxlay_hash(uint32_t lane, uint8_t ly)
{
    dx_voice_t v;
    int32_t y[CTL];
    uint32_t b, i;
    uint64_t h = 1469598103934665603ull;
    uint8_t keep = dx_lay[lane], tone = dx_tone;
    memset(&v, 0, sizeof v);
    dx_lay[lane] = ly;
    dx_tone = 0;
    dx_trigger(&v);
    for (b = 0; b < 30u; b++) {
        dx_run(&dx_kit[lane], &v, 64, 0, 0, 0, 0, 0, 0, y, CTL);
        for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)y[i]) * 1099511628211ull;
    }
    dx_lay[lane] = keep;
    dx_tone = tone;
    return h;
}
static int test_dxlay(void)
{
    int bad = 0, ok = 1;
    uint32_t i;
    project_store_t st;
    ui_power_on();
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    ok &= dxlay_hash(DV_SNARE, 0) == dxlay_hash(DV_SNARE, 0);
    ok &= dxlay_hash(DV_SNARE, 12) != dxlay_hash(DV_SNARE, 0) && dxlay_hash(DV_BELL, 0x30) != dxlay_hash(DV_BELL, 0);
    ok &= dxlay_hash(DV_HATC, 15) != dxlay_hash(DV_HATC, 6);
    bad += check("DRUM LAYER / FEEDBK: each sound's, 0 as it was, more another sound", ok);
    memcpy(dx_lay, DX_LAY_DEF, sizeof dx_lay);
    dx_lay[DV_BELL] = 0x5Fu;
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    memset(dx_lay, 0, sizeof dx_lay);
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= dx_lay[DV_BELL] == 0x5Fu && dx_lay[DV_SNARE] == DX_LAY_DEF[DV_SNARE];
    bad += check("DRUM LAYER: kept with the project (FUNC 'D' 'L')", ok);
    memset(dx_mdl, 0, sizeof dx_mdl);                   /* (1.0-GW) SNARE / CLAP: kept with the project, MODE's steps */
    dx_mdl[DV_SNARE] = (uint8_t)(DXX_SNARE | 9u << 2);
    dx_mdl[DV_CLAP] = DXX_CLAP;
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    memset(dx_mdl, 0, sizeof dx_mdl);
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= dx_mdl[DV_SNARE] == (DXX_SNARE | 9u << 2) && dx_mdl[DV_CLAP] == DXX_CLAP && !dx_mdl[DV_KICK];
    memset(dx_mdl, 0, sizeof dx_mdl);                   /* (none set: no block, the project as before) */
    project_capture(&proj_scratch);
    ok &= proj_pack(&st, &proj_scratch) && proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    for (i = 0; i < 8u; i++) ok &= !dx_mdl[i];
    dx_kit[DV_RIM].mode = DX_MODE(DXW_BELL, DXF_HP, 1);  /* MODE: CLASSIC's last, then SNARE, SNARE+, CLAP, CLAP+; back */
    ok &= dx_ui_mode_ix(DV_RIM) == 23u;
    dx_ui_mode_set(DV_RIM, 24u); ok &= DXX_MODEL(dx_mdl[DV_RIM]) == DXX_SNARE && !((dx_kit[DV_RIM].mode >> 4) & 1u);
    dx_ui_mode_set(DV_RIM, 27u); ok &= DXX_MODEL(dx_mdl[DV_RIM]) == DXX_CLAP && ((dx_kit[DV_RIM].mode >> 4) & 1u);
    dx_ui_mode_set(DV_RIM, 23u); ok &= !DXX_MODEL(dx_mdl[DV_RIM]) && dx_kit[DV_RIM].mode == DX_MODE(DXW_BELL, DXF_HP, 1);
    memset(dx_mdl, 0, sizeof dx_mdl);
    bad += check("DRUM SNARE / CLAP: kept with the project (FUNC 'D' 'X'), none: no block; MODE's 28 steps", ok);
    fg_set[2][1] = (fg_set_t){8, 2, -5};                 /* (0.6.52) FUGUE: T3's head 3 x3 PING -5 */
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    memset(fg_set, 0, sizeof fg_set);
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= fg_set[2][1].rate == 8u && fg_set[2][1].dir == 2u && fg_set[2][1].ivl == -5 && !fg_set[0][0].rate;
    memset(fg_set, 0, sizeof fg_set);
    bad += check("FUGUE: kept with the project (FUNC 'F' 'G')", ok);
    system_init();
    ok = !memcmp(dx_lay, DX_LAY_DEF, sizeof dx_lay) && 1;
    for (i = 0; i < NTRK; i++) {
        const engine_t *e = ENGINES[trk[i].eng_req % NENGINES];
        ok &= !trk[i].p[P_DIST] && !trk[i].p[P_CHOR] && !trk[i].p[P_DLY] && !trk[i].p[P_REV];
        if (e != &ENG_FM6) ok &= trk[i].p[P_E1] == e->edit[1].def && trk[i].p[P_ATK] == TP[P_ATK].def;
    }
    bad += check("NEW PROJECT: every track dry, the engine's own defaults, the factory LAYERs", ok);
    ui_power_on();
    return bad;
}
/* (0.6.50) MORPH turned fast makes no click: its knob jumped 0 <-> 40 every few blocks (across the plain WAVE's render too)
 * the largest step between two samples no larger than with MORPH held (a smooth wave: a step shows) */
static int32_t morph_jump(uint32_t eng, int16_t e0, int mode)
{
    int32_t o[2u * CTL], last = 0, mx = 0;
    uint32_t b, i, k;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    song.master_q12 = 4096;
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    set_engine_of(t, eng);
    t->engine = t->eng_req;
    t->p[P_E0] = e0;
    if (eng == 3u) t->p[P_E1] = 1;                       /* (LOFI: TRI) */
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = t->p[P_DIST] = 0;
    t->p[P_APDEP] = 0;
    t->p[P_MORPH] = mode == 2 ? 40 : 0;
    for (b = 0; b < 40u; b++) mix_block(o, CTL);
    trk_note_on(t, 45, 100);
    for (b = 0; b < 40u; b++) mix_block(o, CTL);
    for (b = 0; b < 120u; b++) {
        if (mode == 1) t->p[P_MORPH] = (b / 6u) & 1u ? 40 : 0;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t d = o[2u * i] - last;
            last = o[2u * i];
            if (b || i) mx = d > mx ? d : -d > mx ? -d : mx;
        }
    }
    return mx;
}
static int test_morph_click(void)
{
    int bad = 0, ok = 1;
    static const struct { uint32_t eng; int16_t e0; } C[3] = {{ENGI_ANALOG, 2}, {2u, 0}, {3u, 0}};
    uint32_t c;
    for (c = 0; c < 3u; c++) {
        int32_t a = morph_jump(C[c].eng, C[c].e0, 0), b = morph_jump(C[c].eng, C[c].e0, 2), j = morph_jump(C[c].eng, C[c].e0, 1);
        int32_t ref = a > b ? a : b;
        if (j > ref + ref / 4) {
            ok = 0;
            fprintf(stderr, "  morph click: engine %u: held %d / %d, jumped %d\n", C[c].eng, a, b, j);
        }
    }
    bad += check("MORPH turned fast (0 <-> 40): no step larger than with it held (ANALOG TRI, PHASE, LOFI TRI)", ok);
    return bad;
}
/* (0.6.51) the LFO in time (RATE below 0) and FREE (PHASE -1): in time a cycle is that many beats on the transport's
 * grid; FREE a note does not restart it; RETRIG (PHASE 0..127) as before; the display */
static int test_lfo_sync(void)
{
    int bad = 0, ok = 1;
    int32_t o[2u * CTL];
    uint32_t b, ph0, len;
    char v[12];
    const char *u;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    t->p[P_LRATE] = -5;                                  /* 1/4: a cycle a beat */
    song.playing = 1;
    ms_clock = 0;
    track_lfo_tick(t);
    ph0 = t->lfo_ph;
    len = beat_samples();
    ms_clock = len / 2u;
    track_lfo_tick(t);
    ok &= ph0 < 0x01000000u && t->lfo_ph > 0x7E000000u && t->lfo_ph < 0x82000000u;   /* half a beat: half a turn */
    ms_clock = len;
    track_lfo_tick(t);
    ok &= t->lfo_ph < 0x01000000u || t->lfo_ph > 0xFF000000u;
    song.playing = 0;
    bad += check("LFO RATE 1/4 (in time): a cycle a beat on the transport's grid", ok);
    param_format(&TP[P_LRATE], -5, v, &u);
    ok = str_eq(v, "1/4");
    param_format(&TP[P_LRATE], -1, v, &u);
    ok &= str_eq(v, "4BAR");
    param_format(&TP[P_LPHASE], -1, v, &u);
    ok &= str_eq(v, "FREE");
    bad += check("LFO RATE below 0 shows the division, PHASE -1 FREE", ok);
    ui_power_on();
    t = &trk[0];
    t->p[P_LRATE] = 60;
    t->p[P_LPHASE] = -1;                                 /* FREE */
    for (b = 0; b < 10u; b++) mix_block(o, CTL);
    ph0 = t->lfo_ph;
    trk_note_on(t, 60, 100);
    ok = t->lfo_ph == ph0;
    trk_note_off(t, 60);
    for (b = 0; b < 400u; b++) mix_block(o, CTL);
    t->p[P_LPHASE] = 0;                                  /* RETRIG */
    trk_note_on(t, 62, 100);
    ok &= t->lfo_ph == 0u;
    trk_note_off(t, 62);
    bad += check("LFO PHASE FREE: a note does not restart it; 0: it does", ok);
    return bad;
}
static int test_patterns(void)
{
    int bad = 0;
    track_t *t = &trk[0], before;
    ui_power_on();
    go_page(GR_PATS);
    bad += check("SEQ > PHRASES is a SEQ page", cur_page()->fam == FAM_SEQ && str_eq(cur_page()->title, "PHRASES") &&
                 song.seq_mode);
    before = *t;
    turn(EN_K2, 1);
    bad += check("KNOB 2 loads nothing (the knobs only pick)", seq_is_empty(t) && ui.confirm == CF_NONE);
    press(B_OCTUP);                               /* LOAD 01 ACID into the empty sequencer */
    bad += check("LOAD into an empty sequencer: at once, LOADED 01 ACID", ui.confirm == CF_NONE && steps_are(t, 0) &&
                 msg_is("LOADED 01 ACID"));
    bad += check("..the sound and the track's settings untouched", same_sound(t, &before) && t->p[P_SDIV] == before.p[P_SDIV] &&
                 t->p[P_AMODE] == before.p[P_AMODE]);
    turn(EN_K1, 2);
    press(B_OCTUP);
    bad += check("a loaded pattern, untouched: the next one without asking (03 MELODY)", ui.confirm == CF_NONE && steps_are(t, 2) &&
                 msg_is("LOADED 03 MELODY"));
    t->step[3].note[0] = 70;                      /* an edit: the user's steps now */
    t->step[3].n = 1;
    t->step[3].time = ST_NOTE;
    press(B_OCTUP);
    bad += check("edited steps: the REPLACE T1 SEQUENCE? dialog", ui.confirm == CF_LOAD_PAT && ui.confirm_trk == 0u &&
                 t->step[3].note[0] == 70);
    press(B_OCTDN);
    bad += check("OCT- keeps them", ui.confirm == CF_NONE && t->step[3].note[0] == 70);
    my_steps(t);
    before = *t;
    turn(EN_K1, 9);                               /* 12 BEAT */
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("OCT+ loads (12 BEAT), no undo hint (TONIC)", ui.confirm == CF_NONE && steps_are(t, 11) &&
                 msg_is("LOADED 12 BEAT") && !ui.msg2[0]);
    /* a user preset's pattern: listed after the factory ones as U06, with its LEN DIV SWING GATE */
    my_steps(t);                                  /* (the user's steps again: no undo since TONIC) */
    t->p[P_SDIV] = 2;
    t->p[P_SSWING] = 40;
    up_store(5, "UPAT");
    t->p[P_SDIV] = 0;
    t->p[P_SSWING] = 0;
    up_store(6, "NOPAT");
    memset(up_rec(6)->note, 0, sizeof up_rec(6)->note);   /* .. a record with no pattern: not listed */
    bad += check("user presets with a pattern are listed after the factory ones", pat_count() == NPATTERNS + 1u);
    track_defaults_steps(t);
    turn(EN_K1, 20);                              /* (to the end of the list) */
    press(B_OCTUP);
    bad += check("LOAD U06: its 16 steps, LEN 16 (at most), DIV and SWING", msg_is("LOADED U06 UPAT") && t->step[0].note[0] == 40 &&
                 t->step[3].note[0] == 43 && !t->step[30].n && t->p[P_SLEN] == 16 && t->p[P_SDIV] == 2 && t->p[P_SSWING] == 40);
    /* a project's steps count as the user's */
    project_save(2);
    project_load(2);
    press(B_OCTUP);
    bad += check("over a project's steps: the dialog", ui.confirm == CF_LOAD_PAT);
    press(B_OCTDN);
    /* the hint: PRESETS page K3 PAT, and a load moves the pick there */
    go_page(GR_BROWSE);
    turn(EN_K1, 1);
    bad += check("PRESETS: a factory preset's suggested pattern becomes the pick", preset_pat_hint() >= 0 &&
                 ui.ppick == (uint8_t)preset_pat_hint());
    return bad;
}

static int test_rec(void)
{
    static const uint8_t FAMS[3] = {FAM_TRK, FAM_SEQ, FAM_ARP};
    int bad = 0;
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        ui_power_on();
        track_select(2);
        open_family(FAMS[k]);
        press(B_REC);
        bad += check(k == 0 ? "REC on TRACKS: arms the selected track, PLAY starts" :
                     k == 1 ? "#133 REC on SEQ > STEP: arms the selected track for step recording, PLAY does not start" :
                     "REC on ARP: arms the selected track, PLAY starts",
                     song.rec == 4u && (k == 1u ? transport_req == 0u && msg_is("KEYS WRITE STEPS") : transport_req == 1u));
        song.playing = 1;
        transport_req = 0;
        frames(400);                                   /* (not a double tap: that locks the REC layer) */
        press(B_REC);
        bad += check("REC again disarms (the transport runs on)", song.rec == 0u && transport_req == 0u);
        frames(400);
        hold(B_REC);                                   /* 1.1.5: held, the REC layer (ui_layer.c); the arming stays */
        bad += check("REC held: its layer, closed when let go; no dialog, the arming as it was",
                     ui.confirm == CF_NONE && song.rec == 0u && !ui.layer && !transport_req);
        ui.confirm = 0;
    }
    ui_power_on();
    press(B_REC);
    bad += check("REC on HOME records without changing the screen", ui.home && song.rec == 1u && transport_req == 1u);
    /* STEP page: armed + playing -> the keys record live only */
    ui_power_on();
    open_family(FAM_SEQ);
    bad += check("SEQ opens STEP", cur_page()->scope == SC_STEP);
    press(B_REC);
    song.playing = 1;
    transport_req = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("STEP, armed and playing: a key does not write the cursor step", !trk[0].step[0].n && ui.cursor == 0);
    song.rec = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("#133 STEP, not armed: the key only plays (nothing written, the cursor stays)",
                 !trk[0].step[0].n && ui.cursor == 0);
    song.rec = 1;
    song.playing = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("#133 STEP, armed and stopped: the key writes the cursor step (and moves on)",
                 trk[0].step[0].n == 1 && ui.cursor == 1);
    /* ARP: the arp's notes are recorded, not the key */
    ui_power_on();
    {
        track_t *t = &trk[0];
        uint32_t i, n = 0, notes = 0;
        t->engine = t->eng_req;
        t->p[P_AMODE] = 1;
        t->p[P_AOCT] = 2;
        t->p[P_APROB] = 127;
        song.rec = 1;
        song.playing = 1;
        input_on(t, 60, 100);
        for (i = 0; i < NSTEP; i++)
            n += t->step[i].n;
        bad += check("ARP on: the key held is not recorded", n == 0);
        arp_tick(t, 1);
        for (i = 0; i < NSTEP; i++)
            if (t->step[i].n)
                notes = t->step[i].note[0];
        bad += check("ARP on: the note the arp plays is recorded", notes == 60u);
        t->p[P_AMODE] = 0;
        input_off(t, 60);
    }
    return bad;
}

static uint32_t gated_notes(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].gate != 0;
    return n;
}

static int test_midi(void)
{
    int bad = 0;
    track_t *a, *b;
    ui_power_on();
    song.sel = 2;
    midi_hint = 0;
    midi_event(0x90, 0, 60, 100); midi_event(0x90, 9, 61, 100);
    bad += check("ROUT CH1-4 (default): channel 1 -> part 1, channel 10 ignored",
                 song.g[G_ROUTE] == 0 && mn_get(0, 60) == 1u && !mn_get(9, 61) && !gated_notes(&trk[2]));
    bad += check("a note into another track tells the UI", midi_hint == 1u);
    frame(); bad += check("MIDI IN -> T1 shown", msg_is("MIDI IN -> T1"));
    song.g[G_ROUTE] = 1;
    midi_event(0x80, 0, 60, 0); midi_event(0x90, 0, 62, 100);
    bad += check("ROUT changes: note-off releases original owner, new note uses selection",
                 !gated_notes(&trk[0]) && mn_get(0, 62) == 3u && midi_track(3) == &trk[2]);
    song.sel = 1; midi_event(0x80, 0, 62, 0);
    bad += check("note-off after selection follows the note-on's track", !mn_get(0, 62));
    midi_event(0x80, 9, 61, 0);
    ui_power_on();
    song.g[G_ROUTE] = 1;
    midi_event(0x90, 0, 60, 100);
    song.sel = 1;
    midi_event(0x90, 0, 60, 100);
    bad += check("same MIDI channel/pitch on a new selected track releases its old owner",
                 !gated_notes(&trk[0]) && gated_notes(&trk[1]) && mn_get(0, 60) == 2u);
    midi_event(0x80, 0, 60, 0);
    midi_event(0x80, 0, 60, 0);
    bad += check("both note-offs after a MIDI ownership change leave neither track held",
                 !gated_notes(&trk[0]) && !gated_notes(&trk[1]) && !mn_get(0, 60));
    ui_power_on();
    song.sel = 2; song.g[G_ROUTE] = 1;
    midi_event(0x90, 0, 61, 100);
    song.g[G_ROUTE] = 0;
    midi_event(0x90, 0, 61, 100);
    midi_event(0x80, 0, 61, 0);
    bad += check("same channel/pitch across a ROUT change cannot leave its previous part held",
                 !gated_notes(&trk[2]) && !gated_notes(&trk[0]) && !mn_get(0, 61));
    {
        uint32_t hold;
        for (hold = 0; hold < 2u; hold++) {
            ui_power_on();
            song.g[G_ROUTE] = 1;
            trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = (int16_t)hold;
            midi_event(0x90, 0, 60, 100);
            song.sel = 1;
            midi_event(0x90, 0, 60, 100);
            bad += check("MIDI ownership transfer releases old ARP keys and respects explicit HOLD",
                         !trk[0].arp_phys && trk[0].nheld == hold && mn_get(0, 60) == 2u);
        }
    }
    ui_power_on();
    trk[1].p[P_AMODE] = trk[1].p[P_AHOLD] = 1;
    midi_event(0x90, 0, 60, 100);
    midi_event(0x90, 1, 64, 100);
    song.playing = 1;
    {
        uint32_t i, held = 0;
        for (i = 0; i < MQ; i++) midi_in_event(0x643C9909u);
        midi_in_event(0x003C8908u);                      /* note-off lost to a full ring */
        events_block(CTL);
        for (i = 0; i < NTRK * NVOICE; i++) held += trk[i / NVOICE].v[i % NVOICE].gate;
        bad += check("MIDI overflow releases held notes and latched ARP, keeps transport running",
                     !held && !trk[1].nheld && !trk[1].arp_phys && !mn_get(0, 60) &&
                     !mn_get(1, 64) && !midi_in_overflow && mi_r == mi_w && song.playing);
        midi_in_event(0x643D9009u);
        events_block(CTL);
        bad += check("MIDI accepts a fresh note after overflow recovery", mn_get(0, 61) == 1u && trk[0].v[0].gate);
    }
    {
        const page_t *pg = &PAGES[page_first(FAM_GLO) + 1u];
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, 2, &vp);
        char v[8];
        const char *u;
        param_format(d, 1, v, &u);
        bad += check("GLO > SYSTEM K3: ROUT CH1-4 / SEL", str_eq(pg->title, "SYSTEM") && vp == &song.g[G_ROUTE] && str_eq(v, "SEL"));
    }
    return bad;
}

static int test_save(void)
{
    int bad = 0;
    uint32_t i;
    ui_power_on();
    trk[0].p[P_E0] = 5;
    press(B_SAVE);
    bad += check("SAVE opens USER with SAVE selected, without writing a slot",
                 !ui.home && cur_page()->graph == GR_USER && ui.act == 4u && !up_used(0));
    press(B_OCTUP);
    bad += check("SAVE then OCT+ opens NAME (the automatic name), writes nothing yet",
                 name_on() && nm.kind == NK_USER_SAVE && str_eq(nm.s, nm.ph) && nm.cur == nm.len && !up_used(0));
    press(B_OCTUP);
    {
        char nmb[16], au[16];
        up_name(0, nmb);
        up_auto_name(au, trk[0].eng_req, 0);
        bad += check("..OCT+ again stores the edited sound with that name (SAVE, OCT+, OCT+)",
                     !name_on() && up_used(0) && up_value(up_rec(0), P_E0) == 5 && ui.act == 0u && str_eq(nmb, au));
    }
    press(B_SAVE);
    bad += check("SAVE again still cycles to PROJECT", cur_page()->graph == GR_SLOTS);
    go_home();
    press(B_SAVE);
    bad += check("SAVE from HOME returns directly to USER, even after visiting PROJECT",
                 cur_page()->graph == GR_USER && ui.act == 4u);
    press(B_OCTUP);
    bad += check("direct SAVE still asks before overwriting an occupied slot",
                 ui.confirm == CF_OVR_USER && up_value(up_rec(0), P_E0) == 5);
    press(B_OCTDN);
    ui_power_on();
    song.playing = 1;
    project_save(0);
    bad += check("project save while playing: refused, STOP TO SAVE", !project_used(0) && msg_is("STOP TO SAVE"));
    bad += check("user preset save while playing: refused", up_store(0, "X") == 2 && !up_used(0));
    song.playing = 0;
    transport_req = 1;
    project_save(0);
    bad += check("a pending PLAY cannot save a project", !project_used(0) && msg_is("STOP TO SAVE"));
    bad += check("a pending PLAY cannot save a user preset", up_store(0, "X") == 2 && !up_used(0));
    transport_req = 0;
    chain.armed = 1;
    bad += check("a pending SONG cannot save a user preset", up_store(0, "X") == 2 && !up_used(0));
    chain.armed = 0;
    song.playing = 1;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_SLOTS)
            break;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    ui.msg_t = 0;
    turn(EN_K4, 1);                               /* SAVE picked */
    bad += check("PROJECT KNOB 4 picks SAVE, saves nothing", ui.act == 4u && !project_used(0) && !ui.msg_t);
    press(B_OCTUP);
    bad += check("OCT+ while playing: STOP TO SAVE, no NAME", msg_is("STOP TO SAVE") && !project_used(0) &&
                 song.g[G_SAVE] == 0 && !name_on());
    stop_transport();
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("stopped, an empty slot: OCT+ opens NAME (no name yet: PROJECT A)", name_on() && nm.kind == NK_PROJ_SAVE &&
                 nm.len == 0u && str_eq(nm.ph, "PROJECT A") && !project_used(0));
    press(B_OCTUP);
    bad += check("..OCT+ saves it unnamed, SAVE dropped", !name_on() && project_used(0) && msg_is("SAVED (NO FLASH)") &&
                 ui.act == 0u);
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("a used slot: the OVERWRITE? dialog", ui.confirm == CF_OVR_PROJ && ui.confirm_trk == 0);
    trk[0].p[P_E0] = 5;
    press(B_OCTDN);
    bad += check("OCT- keeps the slot", ui.confirm == CF_NONE && stored_param(0, 0, P_E0) != 5);
    turn(EN_K4, 1);
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("OCT+ (YES): NAME, the slot not written yet", name_on() && stored_param(0, 0, P_E0) != 5);
    press(B_OCTUP);
    bad += check("OCT+ overwrites it", ui.confirm == CF_NONE && !name_on() && stored_param(0, 0, P_E0) == 5);
    trk[0].p[P_LEVEL] = 11;
    turn(EN_K3, 1);
    press(B_OCTUP);
    bad += check("KNOB 3 LOAD, OCT+: the project back; LOAD stays picked", trk[0].p[P_LEVEL] != 11 &&
                 trk[0].p[P_LEVEL] == stored_param(0, 0, P_LEVEL) && msg_is("LOADED") && ui.act == 3u);
    /* user presets */
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_USER)
            break;
    ui.page = (uint8_t)i;
    page_entered();
    up_store(4, "OLD");
    ui.uslot = 4;
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("USER SAVE over a used slot: the OVERWRITE? dialog", ui.confirm == CF_OVR_USER && ui.confirm_trk == 4u);
    press(B_OCTUP);
    bad += check("YES: NAME with the sound's name (not the old slot's)", name_on() && !str_eq(nm.s, "OLD"));
    press(B_OCTUP);
    {
        char nm[16];
        up_name(4, nm);
        bad += check("OCT+ stores the sound there", ui.confirm == CF_NONE && !str_eq(nm, "OLD"));
    }
    song.playing = 1;
    ui.msg_t = 0;
    turn(EN_K3, 1);                               /* ERASE */
    press(B_OCTUP);
    bad += check("USER ERASE while playing: STOP TO SAVE", msg_is("STOP TO SAVE") && up_used(4));
    stop_transport();
    return bad;
}

static void go_title(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    if (i == NPAGES) {                          /* (0.6.80: a page renamed under a script; it read past PAGES) */
        fprintf(stderr, "go_title: no page \"%s\"\n", title);
        exit(2);
    }
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static void tools_pick(uint32_t n)                /* (0.6.25) TOOLS: the n-th action picked (KNOB 1 down the list) */
{
    uint32_t k;
    ui.act = 0;
    for (k = 0; k < n; k++) turn(EN_K1, 1);
}
static int test_init_system(void)
{
    int bad = 0;
    ui_power_on();
    {   /* (0.6.40) TOOLS 5 INIT SYSTEM: its dialog; everything as at power-on (sounds, steps, automation, mix, effects,
         * song, macros, the kit), the setup kept; UNDO brings the music back */
        uint32_t k, sig0, ok2;
        trk[0].step[2] = (step_t){{60}, 1, ST_NOTE, 0, 100};
        trk[2].p[P_LEVEL] = 30; trk[2].p[P_REV] = 90;
        song.g[G_BPM] = 97; song.g[G_RTYPE] = 3; song.g[G_TUNE] = 5; song.g[G_CLOCK] = 1;
        macro_v[1] = 77; dx_tone = 0x35u; dx_mot[XM_DPTH] = 50; pfx_lane[3] = 9;
        motion_capture(&trk[1], P_LEVEL, 40);
        sig0 = steps_sig(&trk[0]);
        go_title("TOOLS"); tools_pick(5); press(B_OCTUP);
        ok2 = ui.confirm == CF_INIT_SYS && trk[0].step[2].n == 1u;
        press(B_OCTUP);
        ok2 &= !ui.confirm && msg_is("NEW PROJECT") && chain_config.count == 0u;
        for (k = 0; k < NTRK; k++)
            ok2 &= seq_is_empty(&trk[k]) && !motion_count(&trk[k]) && trk[k].eng_req == trk_def_engine(k);
        ok2 &= trk[2].p[P_LEVEL] != 30 && trk[2].p[P_REV] != 90;   /* (the first sound's level and sends) */
        ok2 &= song.g[G_BPM] == GP[G_BPM].def && song.g[G_RTYPE] == GP[G_RTYPE].def && !macro_v[1] && !dx_tone &&
               !dx_mot[XM_DPTH] && !pfx_lane[3] && !memcmp(dx_kit, DX_KIT_DEF, sizeof dx_kit);
        ok2 &= song.g[G_TUNE] == 5 && song.g[G_CLOCK] == 1;       /* (the setup stays) */
        bad += check("TOOLS 5 NEW PROJECT: everything as at power-on, CLOCK and TUNE kept", ok2);
        ok2 = !undo_do() && steps_sig(&trk[0]) == sig0 && trk[2].p[P_LEVEL] == 30 && song.g[G_BPM] == 97;
        bad += check("  UNDO after NEW PROJECT: the music back", ok2);
        song.g[G_TUNE] = GP[G_TUNE].def; song.g[G_CLOCK] = GP[G_CLOCK].def;
        memset(macro_v, 0, sizeof macro_v); dx_tone = 0; memset(dx_mot, 0, sizeof dx_mot); pfx_lane[3] = 0;   /* (UNDO brought them) */
    }
    {   /* (0.6.83) TOOLS 6 CPU TEST: the heaviest song on every live track (the last one the kit), UNDO: back */
        uint32_t k, sig0, ok2, kit = 0, comps = 1;
        trk[0].step[2] = (step_t){{60}, 1, ST_NOTE, 0, 100};
        song.g[G_BPM] = 97;
        sig0 = steps_sig(&trk[0]);
        go_title("TOOLS"); tools_pick(6); press(B_OCTUP);
        ok2 = ui.confirm == CF_CPU_TEST;
        press(B_OCTUP);
        ok2 &= !ui.confirm && msg_is("CPU TEST: PRESS PLAY") && song.g[G_BPM] == 160 && song.g[G_RTYPE] == 4;
        for (k = 0; k < trk_live; k++) {
            kit += drum_track(&trk[k]);
            comps &= trk[k].step[0].comp != 0 && trk[k].p[P_SLEN] == 32;
        }
        ok2 &= kit == 1u && comps && trk[0].step[1].n == 4u && trk[0].step[1].comp2 != 0;
        project_capture(&proj_scratch);
        ok2 &= proj_pack(&proj_wire, &proj_scratch) != 0;   /* (storable) */
        bad += check("TOOLS 6 CPU TEST: four-note chords, two components a step, the kit; storable", ok2);
        ok2 = !undo_do() && steps_sig(&trk[0]) == sig0 && song.g[G_BPM] == 97;
        bad += check("  UNDO after CPU TEST: the music back", ok2);
        song.g[G_BPM] = GP[G_BPM].def;
    }
    {   /* (1.2.1-GW, issue #22) TOOLS 7 CLEAR PROJECT: the project's sections A-D free again and its song empty (they
         * stayed taken for good: the generator's SONG form keeps a section of yours), the music playing kept */
        uint32_t k, ok2, sig0, used = 0;
        for (k = 0; k < 4u; k++) memset(proj_slot[k].raw, 0, 4);
        chain_defaults(&chain_config);
        go_title("TOOLS"); tools_pick(7); press(B_OCTUP);
        ok2 = !ui.confirm && msg_is("NOTHING TO CLEAR");
        trk[0].step[2] = (step_t){{60}, 1, ST_NOTE, 0, 100};
        sig0 = steps_sig(&trk[0]);
        section_store(0);
        section_store(2);
        chain_config.count = 1; chain_config.row[0].slot = 2; chain_config.row[0].bars = 4;
        ok2 &= project_used(0) && project_used(2) && !project_used(1);
        go_title("TOOLS"); tools_pick(7); press(B_OCTUP);
        ok2 &= ui.confirm == CF_CLEAR_PROJ;
        press(B_OCTUP);
        for (k = 0; k < 4u; k++) used |= (uint32_t)project_used(k) << k;
        ok2 &= !ui.confirm && msg_is("PROJECT CLEARED") && !used && chain_config.count == 0u && live.cur < 0 &&
               steps_sig(&trk[0]) == sig0;
        bad += check("TOOLS 7 CLEAR PROJECT: its dialog; sections A-D free, the song empty, the music kept", ok2);
        confirm_open(CF_DICE_ALL, song.sel); gen_open(); gp.open_ms = 0;
        gp.genre = 1; gp.seed = 77; gp.form = 2; gp.energy = 60;
        gen_make();
        ok2 = project_used(0) && project_used(1);
        bad += check("  .. the generator's SONG form fills the freed sections again", ok2);
        ui.confirm = 0; ui.force = 1;                  /* (the GENERATOR closed) */
        project_clear();                                /* (the song mode, the sections: as before) */
        gp.form = 0;
        go_home();
    }
    return bad;
}
static uint32_t oct_leds_seen(int all)            /* over 1 s: the OCT LED bits lit at some point (all: throughout) */
{
    uint32_t k, any = 0, every = 3u | OCT_BREATH;
    for (k = 0; k < 64u; k++) {
        frame();
        any |= oct_leds();
        every &= oct_leds();
    }
    return all ? every : any;
}

/* the action pages: the knobs pick, OCT+ does it, OCT- cancels / goes HOME; the LEDs; chords */
static int test_actions(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    ui_power_on();
    press(B_OCTUP);
    bad += check("HOME: OCT+ shifts the octave (lit)", song.octave == 1 && oct_leds() == 2u);
    go_page(GR_PATS);
    bad += check("PATTERNS: OCT+ breathes (a pattern to load), OCT- lit", oct_leds_seen(0) == (1u | OCT_BREATH) &&
                 oct_leds_seen(1) == (1u | OCT_BREATH));
    press(B_OCTUP);
    bad += check("..OCT+ loads it, the octave stays", steps_are(t, 0) && song.octave == 1 && !ui.home);
    bad += check("..the same pattern again would change nothing: OCT+ dark", oct_leds_seen(0) == 1u);
    turn(EN_K1, 1);
    bad += check("..another one picked: OCT+ breathes again", oct_leds_seen(0) == (1u | OCT_BREATH));
    {
        char a[16], b[16];
        foot_hint(a, b);
        bad += check("..the footer: OCT+ LOAD / OCT- BACK", str_eq(a, "OCT+ LOAD") && str_eq(b, "OCT- BACK"));
    }
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* both together (the UPDATE MODE hold): nothing */
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    fm1_in.buttons |= 1u << panel.btn[B_OCTDN];
    host_pressed |= 1u << panel.btn[B_OCTDN];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTDN]);
    frame();
    bad += check("OCT- + OCT+ together: no load, no HOME, no octave change", steps_are(t, 0) && !ui.home && song.octave == 1);
    go_home();
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* pressed on HOME, let go on PATTERNS: no load */
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    go_page(GR_PATS);
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    bad += check("a press that began elsewhere does not load", steps_are(t, 0) && song.octave == 2);
    press(B_OCTDN);
    bad += check("PATTERNS: OCT- goes HOME (the octave stays)", ui.home && song.octave == 2);
    /* TOOLS: CLRSQ and INIT are picked, not done, by their knobs */
    my_steps(t);
    go_title("TOOLS");
    bad += check("TOOLS: nothing picked, OCT+ dark", act_cols() == 15u && act_col() == 0u && oct_leds_seen(0) == 1u);
    turn(EN_K1, 3);
    bad += check("KNOB 1 picks CLEAR, the steps stay; OCT+ breathes", ui.act == 1u && !seq_is_empty(t) &&
                 oct_leds_seen(0) == (1u | OCT_BREATH));
    {
        char a[16], b[16];
        foot_hint(a, b);
        bad += check("..the footer: OCT+ CLEAR / OCT- CANCEL", str_eq(a, "OCT+ CLEAR") && str_eq(b, "OCT- CANCEL"));
    }
    turn(EN_K1, -1);
    bad += check("KNOB 1 left drops it", ui.act == 0u);
    turn(EN_PRESET, 1); turn(EN_PRESET, 1);
    bad += check("(0.6.25) TOOLS is a list: PRESETS / KNOB 1 move the pick down (INIT SOUND), any knob too", ui.act == 2u);
    turn(EN_K3, -1);
    bad += check("  .. and up", ui.act == 1u);
    turn(EN_K1, -1);
    turn(EN_K1, 1);
    press(B_OCTDN);
    bad += check("OCT- cancels the pick (still on TOOLS)", ui.act == 0u && !ui.home && !seq_is_empty(t));
    turn(EN_K1, 1);
    press(B_OCTUP);
    bad += check("TOOLS CLEAR first asks confirmation", ui.confirm == CF_CLEAR_SEQ && !seq_is_empty(t));
    press(B_OCTUP);
    bad += check("OCT+ clears the pattern, the pick dropped", seq_is_empty(t) && msg_is("T1 CLEARED") && str_eq(ui.msg2, "SEQ+REC: ALL TRACKS") && ui.act == 0u);
    turn(EN_K1, 1);
    bad += check("nothing to clear: OCT+ dark", oct_leds_seen(0) == 1u);
    turn(EN_K2, 1);
    t->p[P_E0] = (int16_t)(t->p[P_E0] + 1);
    press(B_OCTUP);
    bad += check("TOOLS INIT first asks confirmation", ui.confirm == CF_INIT_SOUND);
    press(B_OCTUP);
    bad += check("KNOB 2 INIT, OCT+: SOUND INIT", msg_is("SOUND INIT") && ui.act == 0u);
    press(B_OCTDN);
    bad += check("OCT- with nothing picked: HOME", ui.home);
    /* the dialogs and the menu */
    open_family(FAM_SEQ);
    my_steps(t);
    confirm_open(CF_CLEAR_SEQ, song.sel);
    frame();
    bad += check("a dialog: OCT+ breathes, OCT- lit", ui.confirm == CF_CLEAR_SEQ && oct_leds_seen(0) == (1u | OCT_BREATH) &&
                 oct_leds_seen(1) == (1u | OCT_BREATH));
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    bad += check("..it answers on release, not on the press", ui.confirm == CF_CLEAR_SEQ && !seq_is_empty(t));
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    bad += check("..OCT+ let go: cleared", ui.confirm == CF_NONE && seq_is_empty(t));
    bad += check("back on SEQ: the octave LEDs again", oct_leds() == 2u);
    return bad;
}

static int test_tracks(void)
{
    int bad = 0;
    track_t before;
    ui_power_on();
    open_family(FAM_TRK);
    before = trk[0];
    turn(EN_PRESET, 1);
    bad += check("#94 MIXER: the PRESETS knob loads the next sound; the steps and the mix stay",
                 trk[0].preset != before.preset && !memcmp(trk[0].step, before.step, sizeof before.step) &&
                 trk[0].p[P_LEVEL] == before.p[P_LEVEL] && cur_page()->graph == GR_TRK);
    turn(EN_K4, 1);
    bad += check("MIXER KNOB 4 right: MUTE ON (the selected track stays)", trk[0].p[P_MUTE] == 1 && song.sel == 0);
    turn(EN_K1, -3);
    bad += check("KNOB 1 changes LEVEL only (MUTE stays)", trk[0].p[P_MUTE] == 1 && trk[0].p[P_LEVEL] == before.p[P_LEVEL] - 3);
    turn(EN_K2, 2);
    bad += check("KNOB 2 changes PAN", trk[0].p[P_PAN] == before.p[P_PAN] + 2);
    turn(EN_K4, -1);
    bad += check("MIXER KNOB 4 left: MUTE OFF", trk[0].p[P_MUTE] == 0);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM selects the track", song.sel == 1);
    {   /* (0.6.55) TRACKS 4: ALGORITHM stops at T4, T5 / T6 are not offered; TRACKS 6 reaches T6; back to 4 from T6: T4 */
        uint32_t k;
        trks_put(0);
        for (k = 0; k < 8u; k++) turn(EN_ALGO, 1);
        bad += check("TRACKS 4: ALGORITHM stops at T4", song.sel == 3);
        trks_put(1);
        for (k = 0; k < 8u; k++) turn(EN_ALGO, 1);
        bad += check("TRACKS 6: ALGORITHM reaches T6", song.sel == 5);
        trks_put(0);
        ui_input();
        bad += check("TRACKS 6 -> 4 on T6: T4 selected", song.sel == 3);
        for (k = 0; k < 8u; k++) turn(EN_ALGO, -1);
        bad += check("ALGORITHM back down to T1", song.sel == 0);
        turn(EN_ALGO, 1);
    }
    return bad;
}

/* key k of colour black (1) / white (0) at place p (seq.c key_place) */
static uint32_t key_at(int black, uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) == black && key_place(k) == p)
            return k;
    return 0;
}
static void tap_key(uint32_t k)                   /* a key down for a frame, then up */
{
    fm1_in.notes |= 1u << k;
    host_notes |= 1u << k;
    frame();
    fm1_in.notes &= ~(1u << k);
    frame();
}
static uint32_t lane_steps(const track_t *t, uint32_t l)   /* bit i: lane l strikes at step i (0..31) */
{
    uint32_t i, m = 0;
    for (i = 0; i < 32u; i++)
        m |= ((step_lanes(&t->step[i]) >> l) & 1u) << i;
    return m;
}

static int test_grid(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, k, leds, ok;
    ui_power_on();
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    open_family(FAM_SEQ);
    frame();
    bad += check("SEQ > STEP on a DRUM track is the grid (keys to the UI), GRID in the footer", grid_on() && song.grid == 1u &&
                 cur_page()->scope == SC_STEP);
    bad += check("the lanes: KICK SNARE CLAP HATCL HATOP TOM RIM BELL on GM 36 38 39 42 46 45 37 56",
                 str_eq(drum_lane_name(t, 0), "KICK") && str_eq(drum_lane_name(t, 7), "BELL") && DRUM_LANE_NOTE[3] == 42 &&
                 DRUM_LANE_NOTE[6] == 37 && drum_lane(41) == DV_TOM && drum_lane(49) == DV_BELL && drum_lane(35) == DV_KICK);
    tap_key(key_at(1, 1));
    bad += check("black key 2: lane 2 (SNARE) selected", ui.lane == 1u);
    tap_key(key_at(0, 4));
    bad += check("white key 5: SNARE on step 5, the cursor there", t->step[4].hit == 1u << DV_SNARE && t->step[4].time == ST_NOTE &&
                 !t->step[4].n && ui.cursor == 4u);
    leds = grid_leds();
    bad += check("LEDs: white key 5 lit, black key 2 lit, ACC and the page keys dark (LEN 16)",
                 (leds >> key_at(0, 4) & 1u) && (leds >> key_at(1, 1) & 1u) && !(leds >> key_at(0, 3) & 1u) &&
                 !(leds >> key_at(1, 0) & 1u) && !(leds >> key_at(1, GK_ACC) & 1u) && !(leds >> key_at(1, GK_PGUP) & 1u));
    song.playing = 1;
    t->seq_idx = 4;
    leds = grid_leds();
    ok = !(leds >> key_at(0, 4) & 1u);
    t->seq_idx = 6;
    leds = grid_leds();
    ok &= (leds >> key_at(0, 6) & 1u) && !(leds >> key_at(0, 4) & 1u) == 0u;
    song.playing = 0;
    bad += check("LEDs: the step playing inverted (a hit goes dark, an empty step lights)", ok);
    tap_key(key_at(0, 4));
    bad += check("white key 5 again: off, an empty step (REST)", !t->step[4].hit && t->step[4].time == ST_REST);
    /* ACC held: the white keys set accents, their LEDs show them */
    fm1_in.notes |= 1u << key_at(1, GK_ACC);
    frame();
    tap_key(key_at(0, 0));
    fm1_in.notes |= 1u << key_at(1, GK_ACC);
    leds = grid_leds();
    ok = t->step[0].hit == 1u << DV_SNARE && t->step[0].acc == 1u << DV_SNARE && (leds >> key_at(0, 0) & 1u) &&
         (leds >> key_at(1, GK_ACC) & 1u);
    tap_key(key_at(0, 0));
    fm1_in.notes |= 1u << key_at(1, GK_ACC);
    leds = grid_leds();
    ok &= t->step[0].hit == 1u << DV_SNARE && !t->step[0].acc && !(leds >> key_at(0, 0) & 1u);
    fm1_in.notes = 0;
    frame();
    leds = grid_leds();
    ok &= (leds >> key_at(0, 0) & 1u) != 0u;
    bad += check("ACC held: a tap adds the hit accented, again drops the accent only; the LEDs show accents", ok);
    /* the knobs: STEP LANE HIT ACC */
    turn(EN_K1, 2);
    turn(EN_K2, 1);
    turn(EN_K3, 1);
    ok = ui.cursor == 2u && ui.lane == 2u && t->step[2].hit == 1u << DV_CLAP;
    turn(EN_K4, 1);
    ok &= t->step[2].acc == 1u << DV_CLAP;
    turn(EN_K3, -1);
    ok &= !t->step[2].hit && !t->step[2].acc && t->step[2].time == ST_REST;
    turn(EN_K4, 1);
    ok &= t->step[2].hit == 1u << DV_CLAP && t->step[2].acc == 1u << DV_CLAP;
    bad += check("KNOB 1 STEP, 2 LANE, 3 HIT on / off, 4 ACC (an accent adds the hit)", ok);
    press(B_EDIT);
    bad += check("EDIT: the cursor step cleared, on to the next", t->step[2].time == ST_REST && !t->step[2].hit && ui.cursor == 3u &&
                 grid_on());
    /* pages: LEN 32, the page keys */
    t->p[P_SLEN] = 32;
    leds = grid_leds();
    tap_key(key_at(1, GK_PGUP));
    ok = ui.bank == 1u && ui.cursor == 19u && (leds >> key_at(1, GK_PGUP) & 1u) && (leds >> key_at(1, GK_PGDN) & 1u);
    tap_key(key_at(0, 15));
    ok &= t->step[31].hit == 1u << DV_CLAP;
    tap_key(key_at(1, GK_PGDN));
    ok &= ui.bank == 0u && ui.cursor == 15u;
    t->p[P_SLEN] = 12;
    frame();
    tap_key(key_at(0, 13));
    ok &= !t->step[13].hit && !t->step[13].n;
    bad += check("pages: black keys 11 / 10 up / down (the cursor along); past LEN a key does nothing", ok);
    t->p[P_SLEN] = 16;
    /* a step's GM notes on their lanes */
    t->step[8] = (step_t){{41, 49, 36, 0}, 3, ST_NOTE, SF_ACCENT, 96, 0, 0};
    ok = step_lanes(&t->step[8]) == ((1u << DV_TOM) | (1u << DV_BELL) | (1u << DV_KICK)) &&
         step_accents(&t->step[8]) == step_lanes(&t->step[8]);
    grid_hit(t, 8, DV_TOM, 0);
    ok &= t->step[8].n == 1 && t->step[8].note[0] == 49 && t->step[8].hit == 1u << DV_KICK && (t->step[8].flags & SF_ACCENT);
    grid_acc(t, 8, DV_KICK, 0);
    ok &= !t->step[8].acc && !(t->step[8].flags & SF_ACCENT) && t->step[8].n == 1 && (step_lanes(&t->step[8]) >> DV_BELL & 1u);
    grid_acc(t, 8, DV_BELL, 1);
    ok &= !t->step[8].n && t->step[8].hit == ((1u << DV_KICK) | (1u << DV_BELL)) && t->step[8].acc == 1u << DV_BELL;
    bad += check("GM notes on the grid: a low tom / crash on their lanes; an edit makes the lane its hit", ok);
    /* the keys in the audio ISR: on the grid only the lane keys play */
    kb_prev = 0;
    fm1_in.notes = (1u << key_at(0, 3)) | (1u << key_at(1, 3)) | (1u << key_at(1, GK_ACC));
    keyboard_block();
    ok = kb_note[key_at(0, 3)] == KB_SILENT && kb_note[key_at(1, 3)] == 42u && kb_note[key_at(1, GK_ACC)] == KB_SILENT;
    fm1_in.notes = 0;
    keyboard_block();
    bad += check("grid keys in the ISR: white keys and ACC silent, black key 4 plays HAT CL (42)", ok);
    /* live recording on the grid: armed and playing, a lane key records its hit at the play head */
    track_defaults_steps(t);
    song.rec = 1;
    song.playing = 1;
    t->seq_idx = 5;
    t->seq_pos = 0;
    fm1_in.notes = 1u << key_at(1, 4);
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    ok = t->step[5].hit == 1u << DV_HATO && !t->step[5].n && t->rskip_n == 0u;
    t->seq_pos = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), 5) - 10u;   /* late in step 6: into step 7 */
    fm1_in.notes = 1u << key_at(1, 0);
    keyboard_block();
    ok &= t->step[6].hit == 1u << DV_KICK && t->rskip_n == 1u && t->rskip[0] == 36u;
    rec_hold(t, 7, 16);
    fm1_in.notes = 0;
    keyboard_block();
    ok &= t->step[7].time == ST_REST && !t->rh_n;
    bad += check("REC on the grid: lane keys record hits, quantised (the next step late), no TIE holds", ok);
    /* live recording elsewhere (HOME): the GM keys; a lane's note a hit, another GM drum a note */
    go_home();
    frame();
    t->seq_idx = 9;
    t->seq_pos = 0;
    fm1_in.notes = (1u << 7) | (1u << 13) | (1u << 6);   /* C 36 KICK, F# 42 HAT CL, E 35 (a kick 2 st down) */
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    ok = !song.grid && t->step[9].hit == ((1u << DV_KICK) | (1u << DV_HATC)) && t->step[9].n == 1 && t->step[9].note[0] == 35 &&
         step_lanes(&t->step[9]) == ((1u << DV_KICK) | (1u << DV_HATC));
    song.rec = 0;
    song.playing = 0;
    bad += check("REC on HOME: GM keys quantised into the grid (35 a note on the KICK lane)", ok);
    /* SEQ > PATTERNS: 12 BEAT into the DRUM track fills the grid */
    track_defaults_steps(t);
    pat_sig[0] = steps_sig(t);
    go_page(GR_PATS);
    ui.ppick = 11;
    press(B_OCTUP);
    ok = msg_is("LOADED 12 BEAT") && t->p[P_SLEN] == 16 && lane_steps(t, DV_KICK) == ((1u << 0) | (1u << 6) | (1u << 8) | (1u << 11)) &&
         lane_steps(t, DV_SNARE) == ((1u << 4) | (1u << 12)) && lane_steps(t, DV_HATO) == 1u << 14 &&
         lane_steps(t, DV_HATC) == 0xA6AEu && t->step[0].acc == 1u << DV_KICK && t->step[4].acc == 1u << DV_SNARE &&
         t->step[1].acc == 0u && t->step[16].time == ST_REST;
    for (i = 0; i < 16u; i++)
        ok &= t->step[i].n == 0u && !(t->step[i].flags & SF_ACCENT);
    bad += check("PATTERNS 12 BEAT into a DRUM track: the grid (hits, the accents on 1 5 9 13)", ok);
    open_family(FAM_SEQ);
    ui.lane = DV_KICK;
    cursor_set(0);
    leds = grid_leds();
    ok = 1;
    for (i = 0; i < 16u; i++)
        ok &= (leds >> key_at(0, i) & 1u) == ((0x0941u >> i) & 1u);
    bad += check("..its KICK lane on the white key LEDs (1 7 9 12)", ok);
    {   /* the sound changes, the grid stays and plays as GM notes on another engine */
        step_t keep[NSTEP];
        memcpy(keep, t->step, sizeof keep);
        go_home();
        turn(EN_PRESET, 1);
        set_engine_of(t, 0);
        t->engine = t->eng_req;
        ok = !memcmp(keep, t->step, sizeof keep) && !grid_on();
        t->seq_hold = 0;
        t->seq_n = 0;
        seq_step(t, &t->step[0], div_samples((uint32_t)t->p[P_SDIV]), 0);
        ok &= t->seq_n == 1u && t->seq_notes[0] == 36u;
        seq_release(t);
        bad += check("a sound load keeps the grid; on ANALOG its hits play as their GM notes", ok);
        set_engine_of(t, ENGI_DRUM);
        t->engine = t->eng_req;
    }
    my_steps(t);
    go_page(GR_PATS);
    ui.ppick = 11;
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("BEAT over the user's steps: the dialog, then the beat (no undo)", t->step[0].hit != 0);
    /* the menu: the keys play again */
    go_page(GR_ROLL);
    frame();
    k = song.grid;
    hold(B_HOME);
    frame();
    bad += check("HOME held (the menu): the keys are no longer the grid's", k == 1u && ui.menu && !song.grid);
    menu_close();
    return bad;
}

static int test_screen(void)
{
    int bad = 0;
    ui_power_on();
    ui_say("STOP TO SAVE", "");
    ui.force = 1;
    ui_draw();
    bad += check("a message keeps the header's transport and BPM (drawn right of x 80)", ui.msg_t != 0);
    ui.uboot = 3;
    ui.menu = 1;
    ui.force = 1;
    ui_draw();
    bad += check("UPDATE MODE countdown drawn over the menu", ui.force == 0);
    ui.uboot = 0;
    ui.menu = 0;
    return bad;
}

static int test_favorites(void)
{
    int bad = 0;
    uint32_t total, pos;
    track_t before;
    ui_power_on();
    my_steps(TSEL);
    TSEL->p[P_AMODE] = 2;
    go_page(GR_BROWSE);
    before = *TSEL;
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    pos = preset_pos(&total);
    bad += check("FAV and LIST mark/filter the current sound without loading or touching its steps",
                 preset_favorite() && favorites.filter && total == 1 && pos == 0 &&
                 !memcmp(TSEL, &before, sizeof before));
    bad += check("one favorite occupies one display row, without repeated copies",
                 preset_visible(pos, total, 0) == 0 && preset_visible(pos, total, 1) == total);
    select_engine(ENGI_DRUM);
    before = *TSEL;
    turn(EN_K3, 1);
    bad += check("a DRUM sound can be a favorite on any track", preset_favorite() &&
                 favorite_has(ENGI_DRUM, 0) && !memcmp(TSEL, &before, sizeof before));
    turn(EN_PRESET, 1);
    bad += check("filtered browsing crosses DRUM and synth sounds while retaining the track's pattern and ARP",
                 TSEL->eng_req == 0 && preset_favorite() && !memcmp(TSEL->step, before.step, sizeof before.step) &&
                 TSEL->p[P_AMODE] == 2);
    turn(EN_K3, -1);
    pos = preset_pos(&total);
    bad += check("unmarking the current sound leaves it loaded even when outside the filtered list",
                 !preset_favorite() && total == 1 && pos == total && TSEL->eng_req == 0);
    turn(EN_PRESET, -1);
    bad += check("browsing from a nonfavorite selects the last favorite", TSEL->eng_req == ENGI_DRUM);
    turn(EN_K3, -1);
    before = *TSEL;
    turn(EN_PRESET, 1);
    bad += check("empty favorites explains LIST ALL and retains the current sound and steps",
                 msg_is("NO FAVORITES") && !memcmp(TSEL, &before, sizeof before));
    turn(EN_K4, -1);
    preset_pos(&total);
    bad += check("LIST ALL restores the full browser without an implicit load", !favorites.filter && total > 1 &&
                 !memcmp(TSEL, &before, sizeof before));
    up_store(31, "Favorite");
    up_load(31);
    turn(EN_K3, 1);
    bad += check("user slot 32 has its own favorite reference", favorite_has(NENGINES, 31) && preset_favorite());
    up_store(31, "Renamed");
    bad += check("overwriting or renaming a user slot retains its star", favorite_has(NENGINES, 31));
    up_put(31, 0);
    bad += check("successful erasure removes the user slot star and source label",
                 !favorite_has(NENGINES, 31) && !up_used(31) && !TSEL->user);
    up_store(31, "New sound");
    bad += check("reusing the erased slot does not restore its old star", !favorite_has(NENGINES, 31));
    return bad;
}

static int test_display_preferences(void)
{
    int bad = 0, k;
    track_t sounds[NTRK];
    uint16_t before[240 * 240];
    ui_power_on();
    memcpy(sounds, trk, sizeof sounds);
    hold(B_HOME);
    ui.menu_sel = MI_COLOR;
    ui.force = 1; ui_draw();
    memcpy(before, host_screen, sizeof before);
    settings.palette = 5; palette_set(5);
    press(B_OCTUP);
    bad += check("COLOR OCT+ previews the next palette (PAPER, light) without closing the menu",
                 settings.palette == 6 && T_BG == UI_PALETTES[6].bg && ux.light && ui.menu == 1 && !song.octave &&
                 memcmp(before, host_screen, sizeof before) && !memcmp(sounds, trk, sizeof sounds));
    turn(EN_K1, 1);
    bad += check("COLOR KNOB 1 steps on to HI-CON, NIGHT, MONO, THERMAL, GAMMA, WRAITH, QUANTUM, then wraps to GREY",
                 settings.palette == 7 && (turn(EN_K1, 1), settings.palette == 8u) && T_BG == 0u &&
                 str_eq(UI_PALETTES[8].name, "NIGHT") && (turn(EN_K1, 1), settings.palette == UI_BW_INDEX) &&
                 str_eq(UI_PALETTES[UI_BW_INDEX].name, "MONO") && T_BG == 0u && T_TEXT == 0xFFFFu &&
                 (turn(EN_K1, 1), settings.palette == UI_JIANT_INDEX) && str_eq(UI_PALETTES[UI_JIANT_INDEX].name, "THERMAL") &&
                 (turn(EN_K1, 1), settings.palette == UI_GAMMA_INDEX) && str_eq(UI_PALETTES[UI_GAMMA_INDEX].name, "GAMMA") &&
                 (turn(EN_K1, 1), settings.palette == UI_WRAITH_INDEX) && str_eq(UI_PALETTES[UI_WRAITH_INDEX].name, "WRAITH") &&
                 (turn(EN_K1, 1), settings.palette == UI_QUANTUM_INDEX) && str_eq(UI_PALETTES[UI_QUANTUM_INDEX].name, "QUANTUM") &&
                 (turn(EN_K1, 1), settings.palette == UI_GREY_INDEX) && str_eq(UI_PALETTES[UI_GREY_INDEX].name, "GREY"));
    settings.lowcut = 2;
    ui.menu_sel = MI_LOWCUT;
    press(B_OCTUP);
    k = settings.lowcut == 2u;                         /* (OCT+ stops at the end, as the knobs) */
    press(B_OCTDN);
    k &= settings.lowcut == 1u && fx_lowcut == 1u;
    press(B_OCTDN);
    bad += check("the expanded menu retains all three SPEAKER modes (OCT+ / OCT- step them, stopping at the ends)",
                 k && settings.lowcut == 0 && !fx_lowcut && ui.menu == 1);
    {   /* #42: SPEAKER EQ FLAT (the old OFF, stored 0) LOWCUT BASS+; KNOB 1 steps them, the audio follows at once */
        int ok = str_eq(MI_NAME[MI_LOWCUT], "SPEAKER EQ") && str_eq(SPK_EQ[0], "FLAT") && str_eq(SPK_EQ[1], "LOWCUT") &&
                 str_eq(SPK_EQ[2], "BASS+");
        turn(EN_K1, 1); ok &= settings.lowcut == 1u && fx_lowcut == 1u;
        turn(EN_K1, 1); ok &= settings.lowcut == 2u && fx_lowcut == 2u;
        turn(EN_K1, 1); ok &= settings.lowcut == 2u;                  /* (KNOB 1 stops at the ends) */
        turn(EN_K1, -1); turn(EN_K1, -1); ok &= settings.lowcut == 0u && !fx_lowcut;
        bad += check("#42 SPEAKER EQ: FLAT (old OFF) / LOWCUT / BASS+ on KNOB 1, fx_lowcut follows", ok);
    }
    press(B_OCTDN);
    k = ui.menu == 1 && settings.lowcut == 0u;         /* (OCT- at the first value: nothing, the menu stays) */
    press(B_HOME);
    bad += check("HOME leaves display preferences without changing musical state (OCT- no longer closes)",
                 k && !ui.menu && settings.palette == UI_GREY_INDEX && !memcmp(sounds, trk, sizeof sounds));
    return bad;
}

/* 1.0.5: the MENU in tabs (ui_menu.c, menu_items.c MI_TAB): every tab 1..MTAB_ROWS rows in a run, each fitting the
 * page in S and LARGE; ALGORITHM between the tabs (wraps, back at a tab's last row), PRESETS within one (wraps), any of
 * KNOB 1..4 and OCT+ / OCT- change a value, OCT+ opens CALIBRATION and ABOUT, HOME closes (OCT- not); reopened at the
 * same tab and row; the tabs slide with ANIM ON (only their strip drawn again), snap with ANIM OFF */
static void wdt_ms(void) { fm1_ms++; }
static void wdt_key(void) { fm1_ms++; if (fm1_ms % 50u == 0u) fm1_in.notes = 1u; }   /* a key pressed after a moment */
/* (1.1.1-GW) the issues: #3 MENU > GEN > SELECT ON HOME (GENERATOR as ever, or TEMPO); #2 / #10 a knob move recorded:
 * the header says it the first time (and which REC chord clears it), its card draws the "A" (card_mot) */
static int test_issues_111(void)
{
    int bad = 0, ok;
    int16_t bpm;
    ui_power_on();
    go_home(); frames(16);
    fm1_ms += 1200u;
    ui_sel_home = 1;
    bpm = song.g[G_BPM];
    turn(EN_SELECT, 3);
    ok = song.g[G_BPM] > bpm && ui.confirm != CF_DICE_ALL && ui.home;
    ui_sel_home = 0;
    fm1_ms += 1200u;
    turn(EN_SELECT, 1);
    ok &= ui.confirm == CF_DICE_ALL;
    bad += check("#3 SELECT ON HOME: TEMPO turns the BPM (no generator); GENERATOR (the default) opens it", ok);
    ui_power_on();
    go_title("ENV"); frames(16);
    song.rec = 1u << song.sel;
    song.playing = 1;
    ui.msg[0] = 0;
    turn(EN_K2, 2);                                     /* ENV's KNOB 2: DEC */
    ok = motion_has(TSEL, P_DEC) && str_eq(ui.msg, "AUTOMATED. REC+ENV CLR");
    ui.msg[0] = 0;
    turn(EN_K2, 2);                                     /* (once: not said again) */
    ok &= !ui.msg[0];
    song.rec = 0;
    song.playing = 0;
    ui.force = 1; frames(48);
    ok &= (card_mot >> 1) & 1u;                         /* (the DEC card drawn with its "A") */
    bad += check("#2 a knob move recorded: AUTOMATED. REC+ENV CLR once, its card marked", ok);
    return bad;
}
static int test_menu_tabs(void)
{
    int bad = 0, ok = 1;
    uint32_t t, i, n;
    for (t = 0; t < MTAB_COUNT; t++) {
        uint32_t f = mtab_first(t);
        n = mtab_rows(t);
        ok &= n >= 1u && n <= MTAB_ROWS && f < MI_COUNT;
        for (i = 0; i < MI_COUNT; i++)                  /* its rows in one run */
            ok &= (MI_TAB[i] == t) == (i >= f && i < f + n);
        ui.menu_sel = (uint8_t)f;
        ui_prefs = 0;
        ok &= mr_y(n) - mr_gap() + MP_PAD <= MK_Y - 6;
        ui_prefs = PREF_LARGE;
        ok &= mr_y(n) - mr_gap() + MP_PAD <= MK_Y - 6;
    }
    ok &= MP_Y + MP_PAD + (int32_t)MTAB_ROWS * 25 - 1 + MP_PAD <= MK_Y - 6;   /* (room for MTAB_ROWS: 24 px rows 1 px apart) */
    for (i = 0; i < MI_COUNT; i++) ok &= MI_TAB[i] < MTAB_COUNT;
    {   /* the tab bar: MT_GAP between every two tabs at every place of the slide, inside MT_X0..MT_X1 (S and LARGE) */
        int32_t a[MTAB_COUNT], x[MTAB_COUNT], w[MTAB_COUNT], p;
        for (n = 0; n < 2u; n++)
            for (p = 0; p <= (int32_t)(MTAB_COUNT - 1u) * MT_ONE; p++) {
                ui_prefs = n ? PREF_LARGE : 0u;
                mt.pos = (int16_t)p;
                mt_layout(a, x, w);
                ok &= x[0] >= MT_X0 && x[MTAB_COUNT - 1u] + w[MTAB_COUNT - 1u] <= MT_X1;
                for (t = 1; t < MTAB_COUNT; t++) ok &= x[t] - (x[t - 1u] + w[t - 1u]) == MT_GAP;
            }
    }
    ui_prefs = 0;
    bad += check("MENU tabs: DISPLAY CONTROL AUDIO SYSTEM GEN (6 6 6 6 1 rows), 1..6 each in a run, fit the page; tab gaps constant (S, LARGE)",
                 ok && MTAB_COUNT == 5u && str_eq(MTAB_NAME[0], "DISPLAY") && mtab_rows(MTAB_DISPLAY) == 6u &&
                 mtab_rows(MTAB_CONTROL) == 6u && mtab_rows(MTAB_AUDIO) == 6u && mtab_rows(MTAB_SYSTEM) == 6u &&
                 mtab_rows(MTAB_GEN) == 1u);   /* (0.6.58: INIT SYSTEM; 0.6.72: MIDI OUT in CONTROL; 1.1.1-GW: GEN) */

    ui_power_on();
    hold(B_HOME);
    ok = ui.menu == 1 && ui.menu_sel == MI_COLOR && menu_tab() == MTAB_DISPLAY;
    for (i = 0; i < 5u; i++) turn(EN_PRESET, 1);
    ok &= ui.menu_sel == MI_SCROFF;
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_COLOR;  /* (wraps within the tab) */
    turn(EN_PRESET, -1); ok &= ui.menu_sel == MI_SCROFF;
    turn(EN_PRESET, -1); ok &= ui.menu_sel == MI_LEDS;
    bad += check("MENU: PRESETS moves within the tab and wraps there (COLOR .. SCREEN OFF)", ok);
    turn(EN_ALGO, 1); ok = ui.menu_sel == MI_HOLD && menu_tab() == MTAB_CONTROL;
    turn(EN_PRESET, 1); turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_LATCH;
    turn(EN_ALGO, 1); ok &= ui.menu_sel == MI_LOWCUT && menu_tab() == MTAB_AUDIO;
    turn(EN_ALGO, -1); ok &= ui.menu_sel == MI_LATCH;   /* (back at the row left there) */
    turn(EN_ALGO, -1); ok &= ui.menu_sel == MI_LEDS;
    turn(EN_ALGO, -1); ok &= ui.menu_sel == MI_SELHOME && menu_tab() == MTAB_GEN;   /* (wraps; 1.1.1-GW: GEN last) */
    turn(EN_ALGO, -1); ok &= ui.menu_sel == MI_SERIAL && menu_tab() == MTAB_SYSTEM;
    turn(EN_ALGO, 1); turn(EN_ALGO, 1); ok &= ui.menu_sel == MI_LEDS;
    bad += check("MENU: ALGORITHM steps between the tabs and wraps, each tab back at its last row", ok && ui.menu == 1);
    turn(EN_ALGO, 1); turn(EN_PRESET, -1);            /* CONTROL: from FX LATCH up to KNOB ACCEL */
    ok = ui.menu_sel == MI_ACCEL;
    for (i = 0; i < 4u; i++) {                         /* any of KNOB 1..4: right ON, left OFF */
        turn(EN_K1 + i, 1); ok &= !(ui_prefs & PREF_ACCEL_OFF);
        turn(EN_K1 + i, -1); ok &= (ui_prefs & PREF_ACCEL_OFF) != 0u;
    }
    ui_prefs |= PREF_ACCEL_OFF;
    turn(EN_ALGO, 1); turn(EN_PRESET, 1);             /* AUDIO: USB LEVEL */
    ok &= ui.menu_sel == MI_USB;
    turn(EN_K4, 1); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed;
    turn(EN_K2, -1); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed;
    bad += check("MENU: any of KNOB 1..4 changes the selected row's value in any tab, the menu stays", ok && ui.menu == 1);

    /* OCT+ / OCT-: the next / previous value on every kind of row (as the knobs: stopping at the ends; COLOR wraps) */
    ok = 1;
    for (i = 0; i < MI_VALUES; i++) {
        uint32_t n = menu_n(i), v, j, seen, want;
        ui.menu_sel = (uint8_t)i;
        for (j = 0; j <= n; j++)                       /* down to the first value (COLOR: round) */
            press(B_OCTDN);
        seen = 1u << menu_get(i);
        for (j = 0; j <= n; j++) {                     /* up through every value */
            want = menu_step(i, 1);
            press(B_OCTUP);
            ok &= menu_get(i) == want && ui.menu == 1;
            seen |= 1u << menu_get(i);
        }
        ok &= seen == (1u << n) - 1u;
        for (j = 0; j <= n; j++) {                     /* and down again */
            want = menu_step(i, -1);
            press(B_OCTDN);
            ok &= menu_get(i) == want && ui.menu == 1;
        }
        v = menu_get(i);                               /* the ends: COLOR wraps, the rest stop */
        if (i == MI_COLOR) {
            menu_put(i, n - 1u); press(B_OCTUP); ok &= menu_get(i) == 0u;
            press(B_OCTDN); ok &= menu_get(i) == n - 1u;
        } else {
            press(B_OCTDN); ok &= menu_get(i) == v;
            for (j = 0; j < n; j++) press(B_OCTUP);
            v = menu_get(i);
            press(B_OCTUP); ok &= menu_get(i) == v && v != menu_step(i, -1);
        }
        want = menu_step(i, -1);                       /* a knob steps as OCT does */
        turn(EN_K3, -1); ok &= menu_get(i) == want && ui.menu == 1;
    }
    ui_prefs = 0; settings_hold = HOLD_DEF; settings_leds = LEDS_DIM; ui_style = ST_FLAT;
    settings.lowcut = 0; fx_lowcut = 0; menu_put(MI_COLOR, UI_GREY_INDEX);
    bad += check("MENU: OCT+ / OCT- step every value row up / down (COLOR wraps, the rest stop), the menu stays", ok);
    {   /* the ON / OFF rows: up (a knob right, OCT+) is ON, the switch's knob to the right, whichever bit it is */
        static const uint8_t SW[] = {MI_LARGE, MI_ANIM, MI_ACCEL, MI_LATCH, MI_BPMLOCK, MI_SERIAL, MI_RESTORE};
        ok = 1;
        for (i = 0; i < NELEM(SW); i++) {
            ui.menu_sel = SW[i];
            press(B_OCTDN); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "OFF");
            press(B_OCTUP); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "ON");
            turn(EN_K2, -1); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "OFF");
            turn(EN_K2, 1); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "ON");
        }
        ui.menu_sel = MI_STYLE;
        press(B_OCTUP); ok &= ui_style == ST_LINE;     /* (the others: up the second, LINE) */
        press(B_OCTDN); ok &= ui_style == ST_FLAT;
        ui_prefs = 0;
        bad += check("MENU: on an ON / OFF row OCT+ / a knob right is ON, OCT- / left OFF (ANIM, USB SERIAL, RESTORE LAST too)", ok);
    }
    ui.menu_sel = MI_USB;
    press(B_HOME);
    ok = !ui.menu;
    hold(B_HOME);
    ok &= ui.menu == 1 && ui.menu_sel == MI_USB && menu_tab() == MTAB_AUDIO;
    frames(500);
    ok &= ui.menu == 1;                                 /* (the hold's release: no tap, the menu stays) */
    hold(B_HOME);
    ok &= !ui.menu;
    frames(1000);
    ok &= !ui.menu;                                     /* (the hold that closed it: no reopen) */
    hold(B_HOME);
    bad += check("MENU: HOME pressed closes it, held too (no reopen); held opens it again at the same tab and row",
                 ok && ui.menu == 1 && ui.menu_sel == MI_USB);
    turn(EN_ALGO, 1); turn(EN_PRESET, -1);            /* SYSTEM: ABOUT */
    ok = ui.menu_sel == MI_ABOUT;
    frame();
    ok &= oct_leds() == OCT_BREATH;                     /* (OCT+ opens, OCT- does nothing: dark) */
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;   /* (no close, no back) */
    turn(EN_K1, 1); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;
    press(B_OCTUP); ok &= ui.menu == 2;
    ok &= oct_leds() == 1u;                             /* (ABOUT: OCT- back, lit) */
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;
    turn(EN_PRESET, -1); ok &= ui.menu_sel == MI_INIT;   /* (0.6.58) INIT SYSTEM: OCT+ its dialog, the menu closed */
    press(B_OCTUP); ok &= ui.menu == 0 && ui.confirm == CF_INIT_SYS;
    press(B_OCTDN); ok &= ui.confirm == CF_NONE;        /* (OCT- cancels: nothing changed) */
    hold(B_HOME); ok &= ui.menu == 1 && ui.menu_sel == MI_INIT;
    turn(EN_PRESET, -1); ok &= ui.menu_sel == MI_PANEL;
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_PANEL;
    {   /* CALIBRATION: OCT+ starts the setup (nothing pressed here: it times out and keeps the table) */
        panel_t p0 = panel;
        host_wdt_hook = wdt_ms;
        press(B_OCTUP);
        host_wdt_hook = 0;
        ok &= !memcmp(&p0, &panel, sizeof panel) && msg_is("SETUP CANCELLED") && ui.menu == 1;
    }
    {   /* CALIBRATION: a piano key cancels at once (no 30 s wait), the table kept */
        panel_t p0 = panel;
        uint32_t t = fm1_ms;
        host_wdt_hook = wdt_key;
        press(B_OCTUP);
        host_wdt_hook = 0;
        fm1_in.notes = 0;
        ok &= !memcmp(&p0, &panel, sizeof panel) && msg_is("SETUP CANCELLED") && ui.menu == 1 && fm1_ms - t < 1000u;
    }
    bad += check("MENU SYSTEM: OCT+ opens ABOUT (OCT- back to the tab) and CALIBRATION; OCT- does nothing on them", ok);
    turn(EN_PRESET, 1); turn(EN_PRESET, 1); press(B_OCTUP);   /* ABOUT (past INIT SYSTEM), HOME: closed, from the document */
    ok = ui.menu == 2;
    press(B_HOME);
    ok &= !ui.menu && ui.home;
    hold(B_HOME); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;   /* (opened at the tabs, ABOUT's row) */
    turn(EN_ALGO, -1); frame();
    {   /* a value row: OCT- and OCT+ both breathe, each while that way has a value to go to; none lit */
        uint32_t r = ui.menu_sel, want = (menu_step(r, -1) != menu_get(r) ? OCT_BREATH_DN : 0u) |
                                         (menu_step(r, 1) != menu_get(r) ? OCT_BREATH : 0u);
        ok &= r < MI_VALUES && oct_leds() == want && want != 0u;
        menu_put(MI_HOLD, 0u); ui.menu_sel = MI_HOLD;   /* HOLD at its first value: OCT- dark, OCT+ breathing */
        ok &= oct_leds() == OCT_BREATH;
        menu_put(MI_HOLD, 1u);                          /* in between: both breathe */
        ok &= oct_leds() == (OCT_BREATH | OCT_BREATH_DN);
    }
    menu_close();
    bad += check("MENU: HOME closes ABOUT too; reopened at the tabs; a value row breathes OCT- / OCT+ where they can go", ok);

    /* the slide: ANIM ON, a few frames from DISPLAY to CONTROL drawing only the tab strip; ANIM OFF, at once */
    ui_power_on();
    hold(B_HOME);
    frame();
    turn(EN_ALGO, 1);
    ok = mt.pos > 0 && mt.pos < MT_ONE;                 /* (a step on the way) */
    host_blit_rows = 0; frame();
    ok &= host_blit_rows == (uint32_t)(MP_Y - H_HEAD);  /* (only the strip) */
    for (i = 0; i < 8u && mt.pos != MT_ONE; i++) frame();
    ok &= mt.pos == MT_ONE && i <= 6u;
    host_blit_rows = 0; frame();
    ok &= host_blit_rows == 0u;                         /* (settled: nothing drawn) */
    ui_prefs |= PREF_ANIM_OFF;
    turn(EN_ALGO, 1);
    ok &= mt.pos == 2 * MT_ONE;
    ui_prefs &= ~PREF_ANIM_OFF;
    menu_close();
    bad += check("MENU tabs slide with ANIM ON (their strip only, settled within 6 frames), snap with ANIM OFF", ok);
    return bad;
}

static int test_information(void)
{
    uint16_t header[240 * H_HEAD], footer[240 * 19], battery[240 * H_HEAD];
    track_t sounds[NTRK];
    int bad = 0, quiet = 1;
    uint32_t x, y;
    ui_power_on();
    memcpy(sounds, trk, sizeof sounds);
    hold(B_HOME);
    ui.menu_sel = MI_ABOUT;
    press(B_OCTUP);
    ui_draw();
    bad += check("ABOUT opens one document with scrollable license text and credits", ui.menu == 2 && !ui.menu_scroll && menu_scroll_max() > MENU_DOC_H);
    for (y = 146; y < 220; y++)
        for (x = 158; x < 232; x++)
            if (x < 166 || x >= 224 || y < 154 || y >= 212)
                quiet &= host_screen[y * 240 + x] == swap16(UI_QR_LIGHT);
    bad += check("ABOUT QR keeps its complete white quiet zone above the fixed footer", quiet);
    memcpy(header, host_screen, sizeof header);
    memcpy(footer, host_screen + 221 * 240, sizeof footer);
    turn(EN_PRESET, 3);
    ui_draw();
    bad += check("PRESETS scrolls ABOUT with its header and controls fixed",
                 ui.menu == 2 && ui.menu_scroll > 0 && !memcmp(header, host_screen, sizeof header) &&
                 !memcmp(footer, host_screen + 221 * 240, sizeof footer));
    turn(EN_PRESET, 12);
    ui_draw();
    bad += check("PRESETS continues from ABOUT into CREDITS without changing pages",
                 ui.menu == 2 && ui.menu_scroll > 216 && !memcmp(header, host_screen, sizeof header) &&
                 !memcmp(footer, host_screen + 221 * 240, sizeof footer));
    uint16_t position = ui.menu_scroll;
    press(B_OCTUP);
    ui_draw();
    {
        int viz = ui.menu == 3 && song.octave == 0;
        press(B_OCTDN);
        bad += check("OCT+ opens the HYPERSPACE VIZ (no octave change), OCT- back to the document where it was",
                     viz && ui.menu == 2 && ui.menu_scroll == position && song.octave == 0);
    }
    turn(EN_PRESET, 10000);
    turn(EN_PRESET, 10000);
    ui_draw();
    bad += check("the continuous document stops at its final credit",
                 ui.menu == 2 && ui.menu_scroll == menu_scroll_max());
    turn(EN_PRESET, -10000);
    bad += check("scrolling back through CREDITS returns to ABOUT's top", ui.menu == 2 && !ui.menu_scroll);
    press(B_OCTDN);
    bad += check("OCT- returns to the menu without changing sounds or patterns",
                 ui.menu == 1 && !memcmp(sounds, trk, sizeof sounds));
    press(B_OCTUP);
    bad += check("reopening ABOUT starts the continuous document at the top", ui.menu == 2 && !ui.menu_scroll);
    menu_close();
    ui.msg_t = 0;
    usb.config = 1; usb.suspended = 0;
    song.batt_raw = 530;
    ui.force = 1; draw_head(); ui.force = 0;
    memcpy(battery, host_screen, sizeof battery);
    uint32_t signature = ui.head_sig;
    for (x = 0; x < 4; x++) { fm1_ms += 600; draw_head(); }
    bad += check("USB power shows a steady bolt inside the battery without animation",
                 batt_shown() == 4 && ui.head_sig == signature && !memcmp(battery, host_screen, sizeof battery) &&
                 host_screen[6 * 240 + 217] != 0 && host_screen[9 * 240 + 225] != 0);
    usb.suspended = 1;
    draw_head();
    bad += check("USB suspension restores the battery outline and measured level",
                 batt_shown() == 0 && ui.head_sig != signature && host_screen[6 * 240 + 217] != 0);
    usb.config = 0; usb.suspended = 0;
    song.batt_raw = 531; int levels = batt_shown() == 1;
    song.batt_raw = 561; levels &= batt_shown() == 2;
    song.batt_raw = 591; levels &= batt_shown() == 3;
    bad += check("battery operation retains all three ADC level thresholds", levels);
    return bad;
}

/* GREY is grayscale on every screen: every page of every engine, HOME, the menu, ABOUT, the dialogs (RGB565 gray:
 * R = B, G = 2 R). MONO (black and white, blended per channel) is neutral on every screen: R = B, G within 2 of 2 R
 * (the 5-6-5 rounding of its edges) */
static int screen_gray(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        if ((c >> 11) != (c & 31u) || ((c >> 5) & 63u) != (c >> 11) * 2u)
            return 0;
    }
    return 1;
}
static int screen_neutral(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        int32_t d = (int32_t)((c >> 5) & 63u) - (int32_t)(c >> 11) * 2;
        if ((c >> 11) != (c & 31u) || d < -2 || d > 2)
            return 0;
    }
    return 1;
}
static int mono_screens(uint32_t pal, int (*chk)(void), uint32_t *n)
{
    int ok = 1;
    uint32_t e, i;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;                               /* (DIGITAL without FELUCCA_FM4: never a track's) */
        for (i = 0; i < NPAGES; i++) {
            ui_power_on(); set_engine_of(TSEL, e); song.playing = 1; song.rec = 1;
            ui.home = 0; ui.page = (uint8_t)i; page_entered();
            if (!page_visible(i)) continue;
            settings.palette = pal; palette_set(pal);
            memset(host_screen, 0, sizeof host_screen);
            ui.force = 1; ui_draw();
            ok &= chk();
            (*n)++;
        }
        ui_power_on(); set_engine_of(TSEL, e); go_home();
        settings.palette = pal; palette_set(pal);
        ui.force = 1; ui_draw(); ok &= chk();
    }
    for (i = 0; i < 4u; i++) {
        ui_power_on(); song.rec = 3;
        settings.palette = pal; palette_set(pal);
        if (i < 2u) { ui.menu = (uint8_t)(i + 1u); ui.menu_scroll = 300; }
        else if (i == 2u) ui.confirm = CF_OVR_PROJ;
        else ui.uboot = 2;
        ui.force = 1; ui_draw(); ok &= chk();
    }
    return ok;
}
static int test_mono_screens(void)
{
    int bad = 0, ok;
    uint32_t e, n = 0, nb = 0;
    ok = mono_screens(UI_GREY_INDEX, screen_gray, &n);
    bad += check("GREY: every pixel of every page, HOME, menu, ABOUT, dialog and countdown is gray", ok && n > 200u);
    ok = mono_screens(UI_BW_INDEX, screen_neutral, &nb);
    bad += check("MONO (black and white): every pixel of every page, HOME, menu, ABOUT, dialog and countdown is neutral",
                 ok && nb == n);
    ui_power_on();
    for (e = 0; e < NPALETTES; e++) {               /* the other palettes are not gray: the check sees colour */
        ui_power_on(); palette_set(e); ui.force = 1; ui_draw();
        ok = screen_gray();
        if (e == UI_GREY_INDEX ? !ok : ok) bad += check("palette colour shows on HOME", 0);
    }
    palette_set(UI_GREY_INDEX);
    return bad;
}

/* ROLL: the rolling digits of the four cards and the header BPM (ui_draw.c roll_*). The GLOBAL page: BPM (also
 * in the header), SWG, CLK (a name), TUNE (signed). A roll frame differs from the static render only inside the
 * value strip, its last frame is the static render; unchanged digits stay; the direction follows the sign; a
 * change mid-roll retargets; a fast turn, another shape, a page / track / palette change and ui.force snap. */
static uint16_t roll_shot[240 * 240];
static void roll_settle(void)                     /* GLOBAL, nothing rolling, no knob hot */
{
    uint32_t k;
    go_title("GLOBAL");
    ui.hot_t = 0; ui.bpm_t = 0;
    for (k = 0; k < 12u; k++) frame();
}
/* host_screen against roll_shot: the pixels that differ inside the box (in = 1) or outside it */
static uint32_t roll_diff(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int in)
{
    uint32_t n = 0;
    int32_t x, y;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++) {
            int inside = x >= x0 && x < x1 && y >= y0 && y < y1;
            if (inside == in && host_screen[y * 240 + x] != roll_shot[y * 240 + x]) n++;
        }
    return n;
}
/* the mean row (x16) of the ink (not the card surface) in columns x0..x1-1, rows y0..y1-1 of a screen */
static int32_t roll_ink_row(const uint16_t *s, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    int32_t x, y, n = 0, sum = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (s[y * 240 + x] != swap16(T_SURF)) { n++; sum += y * 16; }
    return n ? sum / n : -1;
}
static int roll_static_now(void)                  /* the screen is the static render (a forced redraw changes nothing) */
{
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1;
    ui_draw();
    return roll_diff(0, 0, 0, 0, 0) == 0;
}
#define STRIP(c) CARD_X(c), Y_LABEL + ROLL_Y, CARD_X(c) + COL_W, Y_LABEL + ROLL_Y + ROLL_H
static int test_roll(void)
{
    int bad = 0, ok;
    uint32_t k, n, e, id;
    int32_t up, down, st;
    static const struct { const char *a, *b; int d; } DIRS[] = {
        {"120", "121", 1}, {"121", "120", -1}, {"129", "130", 1}, {"-12", "-13", -1}, {"-13", "-12", 1}, {"+5", "+6", 1},
        {"1.25", "1.30", 1}, {"0.9", "0.8", -1}, {"9", "10", 0}, {"0", "-1", 0}, {"+5", "-5", 0}, {"C3", "C4", 0},
        {"1/4", "1/8", 0}, {"OFF", "ON", 0}, {"2X", "4X", 0}, {"120", "120", 0}, {"123456", "123457", 1},
        {"1234567", "1234568", 0}, {"12%", "13%", 0}};

    ok = 1;
    for (k = 0; k < sizeof DIRS / sizeof DIRS[0]; k++)
        if (roll_dir(DIRS[k].a, DIRS[k].b) != DIRS[k].d) { printf("  roll_dir %s -> %s\n", DIRS[k].a, DIRS[k].b); ok = 0; }
    bad += check("roll: numbers of one shape roll by the sign of the change; names, length, sign changes snap", ok);
    ok = 1;                                         /* every page of every engine: names never roll, numbers do */
    n = 0;
    for (e = 0; e < NENGINES; e++)
        for (id = 0; id < NPAGES && eng_ok(e); id++) {
            uint32_t c;
            ui_power_on(); set_engine_of(TSEL, e);
            ui.home = 0; ui.page = (uint8_t)id; page_entered();
            if (!page_visible(id) || PAGES[id].graph == GR_MOD) continue;
            ui.force = 1; ui_draw();
            for (c = 0; c < 4u; c++) {
                int16_t *vp;
                const param_desc_t *d = PAGES[id].scope == SC_GLOBAL || PAGES[id].scope == SC_TRACK ||
                                        PAGES[id].scope == SC_ENGINE ? page_desc(cur_page(), c, &vp) : 0;
                int32_t v, v0;
                if (!d || !d->label || d->label[0] == '-' || ((act_cols() >> c) & 1u)) continue;
                v0 = *vp;
                for (v = d->min; v < d->max && v - d->min < 40; v++) {
                    *vp = (int16_t)v; ui.frame += ROLL_SNAP; draw_columns();
                    *vp = (int16_t)(v + 1); ui.frame += ROLL_SNAP; draw_columns();
                    if (ui.roll[c].from[0] && (d->fmt == F_ENUM || d->fmt == F_NOTE || d->fmt == F_ONOFF)) {
                        printf("  %s/%s %s: %s rolls\n", ENGINES[e]->name, PAGES[id].title, d->label, ui.roll[c].from);
                        ok = 0;
                    }
                    n += ui.roll[c].from[0] != 0;
                }
                *vp = (int16_t)v0;
            }
        }
    ok &= n > 500u;
    bad += check("roll: every page and engine: a name (enum, even ALG 1..8, note, ON/OFF) snaps, numbers roll", ok);

    /* TUNE 0 -> 1 (card 4): a roll of ROLL_FRAMES frames, the last one the static render */
    ui_power_on(); roll_settle();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 1 && str_eq(ui.roll[3].from, "0") && ui.roll[3].dir == 1;
    for (n = 1; ui.roll[3].from[0] && n < 20u; n++) frame();
    ok &= n == ROLL_FRAMES;
    bad += check("roll: a card's value rolls for 9 frames (TUNE 0 -> 1), the value itself at once", ok);
    bad += check("roll: the roll's last frame is pixel-identical to the static render", roll_static_now());

    /* mid-roll: only the value strip differs from the static render */
    ui_power_on(); roll_settle();
    turn(EN_K4, 1); frame(); frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();                       /* (snaps: the static render) */
    ok = roll_diff(STRIP(3), 0) == 0 && roll_diff(STRIP(3), 1) > 20u && !ui.roll[3].from[0];
    bad += check("roll: a mid-roll frame differs from the static render inside the value strip only", ok);

    /* BPM 120 -> 121 (SELECT): the header rolls, the digits 1 and 2 stay; the BPM card too (no unit:
     * the label says BPM: set in M, it rolls like any number) */
    ui_power_on(); roll_settle();
    song.g[G_BPM]++; frame();                     /* (0.6.67: the tempo is GLO + SELECT; GLO's press snaps: set here) */
    ok = song.g[G_BPM] == 121 && str_eq(ui.roll[ROLL_BPM].from, "120") && ui.roll[ROLL_BPM].dir == 1 &&
         str_eq(ui.roll[0].from, "120") && ui.roll[0].dir == 1;
    frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();
    {   /* the header's strip: from the metronome's ink to the lock's place (ui_draw.c draw_head) */
        head_geo_t g;
        int32_t x0, x1;
        head_geo(&g, "121");
        x0 = g.sx0; x1 = g.sx1;
        n = roll_diff(x0, 0, x1, H_HEAD, 1);
        ok &= n > 20u && roll_diff(x0, 0, x1, H_HEAD, 0) == roll_diff(STRIP(0), 1);   /* (+ the card's strip) */
        ok &= roll_diff(g.bx, 0, g.bx + text_w(&AF_M, "12"), H_HEAD, 1) == 0;
        ok &= x0 < g.bx && x1 <= 140 && x1 - x0 <= 40;
    }
    ok &= roll_diff(CARD_X(0), Y_LABEL + ROLL_Y, CARD_X(0) + 5 + text_w(&AF_M, "12"), Y_LABEL + ROLL_Y + ROLL_H, 1) == 0;
    /* TUNE 10 -> 11 on a card: the 1 stays */
    ui_power_on(); song.g[G_TUNE] = 10; roll_settle();
    turn(EN_K4, 1); frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();
    ok &= song.g[G_TUNE] == 11 && roll_diff(STRIP(3), 1) > 20u && roll_diff(STRIP(3), 0) == 0;
    ok &= roll_diff(CARD_X(3), Y_LABEL + ROLL_Y, CARD_X(3) + 5 + text_w(&AF_M, "1"), Y_LABEL + ROLL_Y + ROLL_H, 1) == 0;
    bad += check("roll: header BPM 120 -> 121 and a card 10 -> 11: only the strip changes, unchanged digits stay put", ok);

    /* the direction: 5 -> 6 and -5 -> -6 at frame 4: the new digit comes from below (up) / above (down) */
    ui_power_on(); song.g[G_TUNE] = 5; roll_settle();
    st = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    turn(EN_K4, 1); frame(); frame(); frame();
    up = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    ok = ui.roll[3].dir == 1;
    ui_power_on(); song.g[G_TUNE] = 7; roll_settle();
    turn(EN_K4, -1); frame(); frame(); frame();
    down = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    ok &= ui.roll[3].dir == -1 && st > 0 && up > st + 16 && down < st - 16;
    ui_power_on(); song.g[G_TUNE] = -5; roll_settle();
    turn(EN_K4, -1);
    ok &= song.g[G_TUNE] == -6 && str_eq(ui.roll[3].from, "-5") && ui.roll[3].dir == -1;
    bad += check("roll: an increase rolls up, a decrease down (-5 -> -6 is down)", ok);

    /* retarget: a change mid-roll restarts from the value it was going to; a fast turn snaps */
    ui_power_on(); song.g[G_TUNE] = 1; roll_settle();
    turn(EN_K4, 1); frame(); frame(); frame();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 3 && str_eq(ui.roll[3].from, "2") && (uint8_t)(ui.frame - ui.roll[3].t0) == 0;
    for (n = 1; ui.roll[3].from[0] && n < 20u; n++) frame();
    ok &= n == ROLL_FRAMES && roll_static_now();
    bad += check("roll: a change mid-roll retargets (from the last value, restarted) and ends static", ok);
    ui_power_on(); song.g[G_TUNE] = 1; roll_settle();
    turn(EN_K4, 1); turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 3 && !ui.roll[3].from[0] && roll_static_now();
    bad += check("roll: a change within 40 ms of the last one snaps (a fast turn)", ok);

    /* snaps: another shape, a name, the page, the track, the palette, ui.force */
    ui_power_on(); roll_settle();
    turn(EN_K4, -1);
    ok = song.g[G_TUNE] == -1 && !ui.roll[3].from[0] && roll_static_now();
    for (k = 0; k < 5u; k++) frame();
    turn(EN_K3, 1);
    ok &= song.g[G_CLOCK] != 0 && !ui.roll[2].from[0] && roll_static_now();
    bad += check("roll: a sign / length change (0 -> -1) and a name (CLK) snap", ok);
    ok = 1;
    for (k = 0; k < 5u; k++) {
        ui_power_on(); roll_settle();
        turn(EN_K4, 1);
        ok &= ui.roll[3].from[0] != 0;
        if (k == 0) go_title("SYSTEM");
        else if (k == 1) turn(EN_ALGO, 1);
        else if (k == 2) { settings.palette = 1; palette_set(1); }          /* (no ui.force) */
        else if (k == 3) set_engine_of(TSEL, eng_step(TSEL->eng_req, 1));   /* (no ui.force) */
        else ui.force = 1;
        frame();
        ok &= !ui.roll[3].from[0] && !ui.roll[ROLL_BPM].from[0];
    }
    ui_power_on(); roll_settle();
    song.g[G_BPM]++; frame();
    ok &= ui.roll[ROLL_BPM].from[0] != 0;
    ui.force = 1; frame();
    ok &= !ui.roll[ROLL_BPM].from[0];
    bad += check("roll: a page, track, palette or engine change and ui.force snap", ok);
    palette_set(UI_GREY_INDEX);

    /* MONO: every roll frame is gray */
    ui_power_on(); roll_settle();
    ok = 1;
    song.g[G_BPM] += 9; frame();
    turn(EN_K4, -1);
    for (k = 0; k < ROLL_FRAMES; k++) { ok &= screen_gray(); frame(); }
    bad += check("roll: MONO roll frames are gray", ok);
    ui_power_on();
    return bad;
}

/* #63 ("parameters moving on their own, fixed by a page change"): a soak of the cards and the rolling digits.
 * ~40 min of UI frames: random turns of KNOB 1..4 and SELECT (bursts back and forth), page / HOME / track
 * changes, the transport and REC (motion recorded and played back), LFO rates, uneven frame times, ui.frame
 * and fm1_ms wrapping. After every frame no roll is older than ROLL_FRAMES; at random quiet moments (no
 * roll, nothing hot) the cards are pixel-identical to a forced redraw (what a page change would show). */
static int test_soak_cards(void)
{
    uint32_t seed = 63u, f, quiet = 0, was_hot, old = 0, stale = 0, cmp = 0, c;
    static uint16_t shot[240 * CARD_H];
#define SOAK_RND(n) ((seed = seed * 1103515245u + 12345u) >> 8) % (n)
    ui_power_on();
    ui.frame = 0xFFFFFF00u;
    fm1_ms = 0xFFFFFFFFu - 60000u;
    for (f = 0; f < 150000u; f++) {
        uint32_t r = SOAK_RND(1000u);
        if (r < 25u) {                                  /* a knob, 1..3 detents either way */
            uint32_t role = SOAK_RND(5u) ? EN_K1 + SOAK_RND(4u) : EN_SELECT;
            int32_t s = (int32_t)SOAK_RND(3u) + 1;
            host_enc[panel.enc[role]] += (SOAK_RND(2u) ? s : -s) * panel.dir[role];
            quiet = 0;
        } else if (r < 29u) {                           /* a page */
            uint32_t i = SOAK_RND(NPAGES);
            if (page_visible(i)) { ui.home = 0; ui.page = (uint8_t)i; page_entered(); }
            quiet = 0;
        } else if (r < 30u) { ui.home = 1; page_entered(); quiet = 0; }
        else if (r < 31u) transport_req = song.playing ? 2u : 1u;
        else if (r == 31u && SOAK_RND(3u) == 0u) rec_tap();
        else if (r == 32u) { host_enc[panel.enc[EN_ALGO]] += SOAK_RND(2u) ? 1 : -1; quiet = 0; }
        else if (r == 33u) trk[SOAK_RND(NTRK)].p[P_LRATE] = (int16_t)SOAK_RND(128u);
        if (ui.confirm && SOAK_RND(6u) == 0u) {         /* (0.6.21: HOME + SELECT's dialog: now and then OK) */
            if (ui.confirm == CF_DICE_ALL && SOAK_RND(3u) == 0u) dice_all();
            ui.confirm = 0; ui.force = 1;
        }
        fm1_ms += SOAK_RND(50u) ? 16u : 1u + SOAK_RND(200u);
        host_ticks += 16000u + SOAK_RND(3u) * 4000u;
        ui_input();
        if (song.playing || transport_req) events_block(FS * 16u / 1000u);
        was_hot = ui.hot_t;                             /* (its last frame still drawn hot: the next one redraws) */
        ui_draw();
        quiet++;
        if (ui.menu || ui.layer || ui.confirm || name_on() || ui.uboot)
            continue;                                   /* (the cards are not drawn) */
        for (c = 0; c < 5u; c++)
            old += ui.roll[c].from[0] && (uint8_t)(ui.frame - ui.roll[c].t0) >= ROLL_FRAMES;
        if (quiet > 45u && !was_hot && !ui.bpm_t && !ui.msg_t && SOAK_RND(8u) == 0u) {
            for (c = 0; c < 5u && !ui.roll[c].from[0]; c++) {}
            if (c < 5u)
                continue;
            memcpy(shot, &host_screen[Y_LABEL * 240], sizeof shot);
            ui.force = 1;
            ui_draw();
            cmp++;
            stale += memcmp(shot, &host_screen[Y_LABEL * 240], sizeof shot) != 0;
        }
    }
#undef SOAK_RND
    if (old || stale || cmp < 500u) printf("  soak: %u old rolls, %u stale cards in %u checks\n", old, stale, cmp);
    stop_transport();
    memset(&motion, 0, sizeof motion);                  /* (what it recorded: the tests below start without) */
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = motion_full = 0;
    ui_power_on();
    return check("#63 soak: 150000 random frames, wrapping clocks, motion: no roll lasts, cards = a forced redraw",
                 !old && !stale && cmp >= 500u);
}

static void screen_save(const char *dir, const char *name)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    ui.msg_t = 0; ui.hot_t = 0; ui.force = 1;
    ui_draw();
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);           /* canvas stores panel-order RGB565 */
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}
static void chain_screens(const char *dir)
{
    uint32_t k;
    ui_power_on();
    go_page(GR_SONG);
    screen_save(dir, "song-empty");
    for (k = 0; k < 2u; k++) { my_steps(&trk[0]); project_save(k); }
    chain_config.count = 3;
    chain_config.row[0] = (chain_row_t){0, 2};
    chain_config.row[1] = (chain_row_t){1, 4};
    chain_config.row[2] = (chain_row_t){0, 1};
    ui.song_row = 1;
    screen_save(dir, "song-ready");
    chain_prepare(); events_block(32);
    screen_save(dir, "song-playing");
    seq_stop();
}

static int test_chain(void)
{
    uint32_t i, k, period, n = 32u;
    int bad = 0, ok;
    step_t before[NTRK][NSTEP];
    int16_t timing[NTRK][4];
    ui_power_on();
    bad += check("SONG empty: PLAY does not start", chain_prepare() == 1 && !transport_req);
    for (k = 0; k < 2u; k++) {
        for (i = 0; i < NTRK; i++) {
            track_defaults_steps(&trk[i]);
            trk[i].p[P_SLEN] = (int16_t)(2u + k);
            trk[i].step[0] = (step_t){{(uint8_t)(60u + 5u * k), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        }
        project_save(k);
    }
    for (i = 0; i < NTRK; i++) {
        my_steps(&trk[i]);
        trk[i].p[P_SLEN] = 9;
        memcpy(before[i], trk[i].step, sizeof before[i]);
        memcpy(timing[i], &trk[i].p[P_SLEN], sizeof timing[i]);
    }
    chain_config.count = 2;
    chain_config.row[0] = (chain_row_t){0, 2};
    chain_config.row[1] = (chain_row_t){3, 1};
    bad += check("SONG missing source: refuses before changing any track", chain_prepare() == 6 && !chain.armed &&
        !memcmp(trk[0].step, before[0], sizeof before[0]));
    chain_config.row[1].slot = 1;
    project_save(2);
    project_load(2);
    bad += check("SONG rows are the song's: a section's load keeps them", chain_config.count == 2 &&
        chain_config.row[0].bars == 2 && chain_config.row[1].slot == 1);
    song.rec = 3;
    bad += check("SONG prepares while stopped, no starts over a pending start", chain_prepare() == 0 && chain_prepare() == 2);
    events_block(n);
    bad += check("SONG starts all tracks at the section's step 0, recording paused", chain.running && song.playing &&
        !song.rec && trk[0].seq_idx == 0 && seq_steps(&trk[0])[0].note[0] == 60 && trk[0].seq_notes[0] == 60 &&
        trk[0].p[P_SLEN] == 2);
    song_poll();                                        /* (the main loop: the next row's section staged) */
    bad += check("SONG the next row's section staged ahead", sec_stage.ready && sec_stage.row == 1u);
    period = chain_bar();
    events_block(period - n);
    bad += check("SONG first row: its first bar played, one to go", chain.row == 0 && chain.remaining == 1);
    events_block(period);
    ok = chain.row == 1 && chain.remaining == 1;
    for (i = 0; i < NTRK; i++)
        ok &= trk[i].seq_idx == 0 && trk[i].p[P_SLEN] == 3 && seq_steps(&trk[i])[0].note[0] == 65;
    bad += check("SONG row change after its bars: four tracks together, the section's steps and LEN", ok);
    {
        step_t now[NSTEP];
        memcpy(now, trk[0].step, sizeof now);
        open_family(FAM_SEQ);
        for (k = 0; k < NPAGES && cur_page()->scope != SC_STEP; k++) open_family(FAM_SEQ);
        turn(EN_K2, 1); press(B_EDIT); hold(B_REC); hold(B_SAVE);
        bad += check("SONG playing: step edits, clears and recording are blocked", !memcmp(trk[0].step, now, sizeof now) &&
                     !ui.confirm);
    }
    events_block(period);
    ok = !song.playing && !chain.running && song.rec == 3;
    song_poll();                                        /* (the main loop: the music back) */
    for (i = 0; i < NTRK; i++) ok &= !trk[i].seq_n && !memcmp(trk[i].step, before[i], sizeof before[i]) &&
        !memcmp(&trk[i].p[P_SLEN], timing[i], sizeof timing[i]);
    bad += check("SONG end: stops, the music as before PLAY back (patterns, timing), record arms back", ok &&
                 msg_is("SONG END"));
    bad += check("SONG source projects stay unchanged", project_used(0) && project_used(1) && stored_note(0, 0, 0) == 60);
    {   /* (0.6.22) the song's END x2: twice through, then the end */
        uint32_t b2, rows_seen = 0;
        chain_config_t keep = chain_config;
        chain_config.count = 2;
        chain_config.row[0] = (chain_row_t){0, 1};
        chain_config.row[1] = (chain_row_t){1, 1};
        SONG_END(&chain_config) = 2;
        ok = chain_prepare() == 0;
        events_block(n);
        for (b2 = 0; b2 < 6u && chain.running; b2++) {
            song_poll();
            events_block(chain_bar());
            rows_seen++;
        }
        ok &= rows_seen == 4u && !chain.running && !song.playing && chain.pass == 1u;
        song_poll();
        bad += check("SONG END x2: the rows twice through, then it stops", ok);
        SONG_END(&chain_config) = SONG_FOREVER;
        ok = chain_prepare() == 0;
        events_block(n);
        for (b2 = 0; b2 < 9u; b2++) { song_poll(); events_block(chain_bar()); }
        ok &= chain.running && song.playing;
        bad += check("SONG END LOOP: still going after 9 bars", ok);
        transport_req = 2; events_block(n); song_poll();
        chain_config = keep;
        ok = song_end_step(0, 1) == 2u && song_end_step(2, -1) == 0u && song_end_step(8, 1) == SONG_FOREVER &&
             song_end_step(SONG_FOREVER, 1) == SONG_FOREVER && song_end_step(SONG_FOREVER, -1) == 8u;
        bad += check("SONG END steps: STOP x2 .. x8 LOOP", ok);
    }
    chain_config.row[0].bars = 1;
    chain_prepare(); events_block(n);
    song_poll();
    chain.bar_pos = chain_bar() - n + 19u;              /* the bar line 19 samples before the next block ends */
    events_block(n);
    ok = chain.row == 1;
    for (i = 0; i < NTRK; i++) ok &= trk[i].seq_pos == 19u && trk[i].seq_idx == 0;
    bad += check("SONG transition preserves fractional block time on all tracks", ok);
    transport_req = 2; events_block(n); song_poll();
    bad += check("SONG manual STOP brings the music back", !chain.running && !song.playing &&
                 !memcmp(trk[0].step, before[0], sizeof before[0]));
    chain_prepare(); events_block(n);
    project_load(0);
    song_poll();                                        /* (the load wins: nothing comes back over it) */
    bad += check("PROJECT load during SONG stops it before loading new timing", !chain.running && !song.playing &&
        trk[0].p[P_SLEN] == 2 && trk[0].step[0].note[0] == 60 && chain_config.count == 2);   /* (the rows: the song's) */
    ui_power_on();
    open_family(FAM_SEQ);
    go_page(GR_SONG); frame();                          /* (0.6.24: SAVE > ARRANGE, the song in one place) */
    press(B_PLAY);
    bad += check("SONG page empty PLAY explains how to start", msg_is("ADD A SONG ROW") && !transport_req);
    turn(EN_K2, 1);
    bad += check("SONG SLOT knob adds the first row with one repeat", chain_config.count == 1 && !chain_config.row[0].slot && chain_config.row[0].bars == 1);
    turn(EN_K1, 1); turn(EN_K2, 1); turn(EN_K2, 1); turn(EN_K3, 2);
    bad += check("SONG row, slot and repeat knobs build a chain", chain_config.count == 2 && ui.song_row == 1 &&
        chain_config.row[1].slot == 1 && chain_config.row[1].bars == 3);
    turn(EN_K4, 30);
    project_save(0);
    chain_config.count = 1;
    chain_config.row[0] = (chain_row_t){0, 1};
    fm1_in.buttons |= (1u << panel.btn[B_OCTUP]) | (1u << panel.btn[B_OCTDN]);
    host_pressed |= fm1_in.buttons; frame();
    fm1_in.buttons = 0; frame();
    bad += check("SONG OCT chord does not start playback or leave the page", !transport_req && !ui.home);
    press(B_OCTUP);
    bad += check("SONG OCT+ on release starts, pending start blocks SAVE", chain.armed && transport_req == 1);
    project_save(3);
    bad += check("SONG pending start cannot write a project", msg_is("STOP TO SAVE") && !project_used(3));
    press(B_PLAY); events_block(32);
    bad += check("SONG PLAY cancels a pending start", !chain.running && !chain.armed && !song.playing);
    uint32_t old_count = chain_config.count;
    turn(EN_K4, 30);
    bad += check("SONG unused K4 cannot accidentally add or clear rows", chain_config.count == old_count);
    ui.song_row = (uint8_t)chain_config.count;          /* (0.6.22: + ADD; past it the END row) */
    for (k = chain_config.count; k < CHAIN_ROWS; k++) { turn(EN_K2, 1); turn(EN_K1, 1); }
    bad += check("SONG: + ADD, then END the last row (full: END right after row 16)", ui.song_row == CHAIN_ROWS);
    bad += check("SONG append row stays bounded at 16", chain_config.count == CHAIN_ROWS && chain_valid(&chain_config));
    go_title("TOOLS"); tools_pick(4); press(B_OCTUP);
    bad += check("CLEAR SONG requires explicit confirmation", ui.confirm == CF_CLEAR_SONG && chain_config.count == CHAIN_ROWS);
    press(B_OCTDN);
    bad += check("cancel preserves the full song", chain_config.count == CHAIN_ROWS);
    { uint32_t n0 = chain_config.count, pg0 = ui.page;
    go_title("ARRANGE"); frame();
    ui.song_row = 0; chain_config.row[0].slot = 0;
    turn(EN_K2, -1);
    ok = ui.confirm == CF_DEL_ROW && ui.confirm_trk == 0u;
    press(B_OCTUP);
    bad += check("SONG: SECT left of A asks DELETE ROW?, OCT+ deletes it", ok && chain_config.count == n0 - 1u);
    ui.page = (uint8_t)pg0; ui.force = 1; frame();
    }
    tools_pick(4); press(B_OCTUP); press(B_OCTUP);
    bad += check("confirmed CLEAR SONG keeps musical steps", !chain_config.count && !ui.song_row);
    return bad;
}

/* Product controls are exercised through the actual button/encoder handlers
 * and canvas, including all pages, both fonts and every shipped palette. */
/* Discussions #92 / #94: the PRESETS knob on every page. SEQ's step pages (STEP, the DRUM grid, PATTERN, CHANCE,
 * AUTOMATION): the step cursor, the sound and the steps untouched; USER, PROJECT, PHRASES, SONG: the selection, as KNOB
 * 1; TOOLS: nothing; HOME, PRESETS and every other page: the next sound, the steps untouched, SAVE held undoes it */
static int test_presets_knob(void)
{
    int bad = 0, cur_ok = 1, sel_ok = 1, snd_ok = 1, tools_ok = 1, grid_ok;
    uint32_t i, n_snd = 0, n_cur = 0;
    for (i = 0; i < NPAGES; i++) {
        track_t before;
        uint32_t g = PAGES[i].graph, uslot, ppick;
        int16_t slot;
        ui_power_on();
        song.sel = 0;
        my_steps(TSEL);
        ui.home = 0; ui.page = (uint8_t)i; page_entered();
        if (!page_visible(i))
            continue;
        frame();
        before = *TSEL;
        uslot = ui.uslot; ppick = pat_pick(); slot = song.g[G_SLOT];
        turn(EN_PRESET, 1);
        {
            int same_snd = TSEL->preset == before.preset && TSEL->eng_req == before.eng_req &&
                           !memcmp(TSEL->p, before.p, sizeof before.p);
            int same_steps = !memcmp(TSEL->step, before.step, sizeof before.step);
            if (PAGES[i].fam == FAM_SEQ && (g == GR_ROLL || g == GR_STEPS || g == GR_CHANCE || g == GR_COMP || g == GR_MOTION)) {
                cur_ok &= ui.cursor == 1u && same_snd && same_steps && ui.page == i;
                n_cur++;
            } else if (g == GR_USER) {
                sel_ok &= ui.uslot == uslot + 1u && same_snd && same_steps;
            } else if (g == GR_SLOTS) {
                sel_ok &= song.g[G_SLOT] == slot + 1 && same_snd && same_steps;
            } else if (g == GR_PATS) {
                sel_ok &= pat_pick() == ppick + 1u && same_snd && same_steps;
            } else if (g == GR_SONG || g == GR_EVENTS) {   /* (AUTO LIST: its row) */
                sel_ok &= same_snd && same_steps && ui.page == i;
            } else if (g == GR_TOOLS) {
                tools_ok &= same_snd && same_steps && !ui.confirm;
            } else {
                snd_ok &= !same_snd && same_steps && ui.page == i && !ui.home;
                n_snd++;
            }
        }
    }
    bad += check("#92 PRESETS on STEP, COMP, PATTERN, SHIFT, CHANCE, AUTOMATION: the step cursor; the sound and the steps stay",
                 cur_ok && n_cur == 11u);   /* (0.6.81: and NOTE; 0.6.82: COMP 2, WHEN; 0.6.87: HUMAN; 0.6.88: TIMING) */
    bad += check("#94 PRESETS on USER, PROJECT, PHRASES, SONG: the selection (KNOB 1's); nothing loaded", sel_ok);
    bad += check("#94 PRESETS on TOOLS does nothing", tools_ok);
    bad += check("#94 PRESETS elsewhere (EDIT, ENV, LFO, FX, SCL, ARP, MIXER, GLOBAL, ...): the next sound, the steps stay",
                 snd_ok && n_snd >= 15u);
    ui_power_on();
    set_engine_of(TSEL, ENGI_DRUM);
    TSEL->engine = TSEL->eng_req;
    TSEL->p[P_SLEN] = 32;
    go_page(GR_ROLL);
    frame();
    turn(EN_PRESET, 17);
    grid_ok = grid_on() && ui.cursor == 17u && ui.bank == 1u && TSEL->eng_req == ENGI_DRUM;
    turn(EN_PRESET, -18);
    grid_ok &= ui.cursor == 31u;
    bad += check("#92 the DRUM grid: PRESETS moves the cursor (and the page), as KNOB 1; wraps", grid_ok);
    ui_power_on();
    hold(B_HOME);
    i = ui.menu_sel;
    turn(EN_PRESET, 1);
    bad += check("#94 the menu: PRESETS still moves the selection", ui.menu == 1 && ui.menu_sel == i + 1u);
    return bad;
}

static int test_product_ux(void)
{
    int bad = 0, ok = 1;
    uint32_t i, p, b;
    for (i = 0; i < NPAGES; i++) {
        ui_power_on(); set_engine_of(TSEL, 1);         /* (DIGITAL; without FELUCCA_FM4 FM6: no OP pages) */
        ui.home = 0; ui.page = (uint8_t)i; page_entered();
        if (!page_visible(i))
            continue;
        press(B_REC);
        if (PAGES[i].graph == GR_SONG)
            ok &= song.rec == 0 && !transport_req && msg_is("[SEQ] TO RECORD");
        else if (PAGES[i].graph == GR_ROLL)            /* #133: STEP's roll arms step recording, stopped */
            ok &= song.rec == 1 && !transport_req && ui.page == i && !ui.home;
        else ok &= song.rec == 1 && transport_req == 1 && ui.page == i && !ui.home;
    }
    bad += check("REC stays on every page, SONG stopped asks for pattern recording (STEP: step recording, stopped)", ok);
    ui_power_on(); hold(B_HOME); press(B_REC);
    bad += check("REC in the MENU does nothing (the menu stays, no arm, no transport start)",
                 ui.menu == 1 && song.rec == 0 && !transport_req);
    ui.menu_sel = MI_ABOUT; press(B_OCTUP); press(B_REC);
    bad += check("REC in ABOUT does nothing (the document and its scroll stay)",
                 ui.menu == 2 && !ui.menu_scroll && song.rec == 0 && !transport_req);
    ui_power_on(); press(B_GLO);
    bad += check("GLO directly opens the four-channel MIXER", cur_page()->graph == GR_TRK);
    int16_t len = TSEL->p[P_SLEN], send = TSEL->p[P_REV];
    turn(EN_K3, 1);
    bad += check("MIXER K3 edits reverb, never sequence length", TSEL->p[P_SLEN] == len && TSEL->p[P_REV] == send + 1);
    frames(320);                                       /* (taps further apart than a double tap: #83) */
    press(B_GLO); frames(320); ok = cur_page()->fam == FAM_GLO && str_eq(cur_page()->title, "GLOBAL");
    press(B_GLO); frames(320); ok &= str_eq(cur_page()->title, "SYSTEM");
    press(B_GLO); ok &= cur_page()->graph == GR_TRK;
    bad += check("GLO cycles MIXER > GLOBAL > SYSTEM > MIXER", ok);
    go_home(); hold(B_SEQ);
    bad += check("long SEQ off the SEQ pages is a tap (no SONG: SAVE held, G5)", cur_page()->graph != GR_SONG);
    go_page(GR_SONG);
    song.rec = 1; chain.armed = 1; press(B_REC);
    bad += check("REC cannot write borrowed patterns while SONG is armed", song.rec == 1 && msg_is("STOP TO RECORD"));
    ui_power_on(); open_family(FAM_EDIT);
    ok = 1;
    for (i = 0; i < 8u; i++) { open_family(FAM_EDIT); ok &= page_visible(ui.page) && (cur_page()->graph == GR_MORPH ||
                                                         !(cur_page()->id[0] >= P_FM1_ATK && cur_page()->id[0] <= P_FM4_LEVEL)); }
    bad += check("operator envelope pages are hidden on non-DIGITAL instruments", ok);
#if FELUCCA_FM4
    set_engine_of(TSEL, 1); open_family(FAM_EDIT); ok = 0;
    for (i = 0; i < 12u; i++) { if (cur_page()->id[0] == P_FM1_ATK) ok = 1; open_family(FAM_EDIT); }
    bad += check("DIGITAL exposes four envelopes and independent operator levels", ok);
    go_title("OP1 ENV"); turn(EN_K1, 4);
    bad += check("operator envelope K1 edits only OP1 attack", TSEL->p[P_FM1_ATK] == 4 && !TSEL->p[P_FM2_ATK]);
    track_select(2); frame();
    bad += check("changing to a non-FM track leaves the stale operator page", page_visible(ui.page) && cur_page()->id[0] == P_E0);
#else
    set_engine_of(TSEL, ENGI_DIGITAL); open_family(FAM_EDIT); ok = TSEL->eng_req == ENGI_FM6;
    for (i = 0; i < 12u; i++) { ok &= !(cur_page()->id[0] >= P_FM1_ATK && cur_page()->id[0] <= P_FM4_LEVEL); open_family(FAM_EDIT); }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] >= P_FM1_ATK && PAGES[i].id[0] <= P_FM4_LEVEL)
            ok &= !page_visible(i);
    bad += check("DIGITAL retired: engine 1 asked for loads FM6; the OP ENV / OP LEVEL pages never show", ok);
#endif
    ui_power_on(); go_page(GR_CHANCE); turn(EN_K2, -35);
    bad += check("CHANCE is per step and starts at backward-compatible 100 percent", step_chance(&TSEL->step[0]) == 65 && step_chance(&TSEL->step[1]) == 100);
    turn(EN_K2, -1000); ok = step_chance(&TSEL->step[0]) == 0;
    turn(EN_K2, 1000); ok &= step_chance(&TSEL->step[0]) == 100;
    bad += check("CHANCE controls clamp to 0..100 without changing adjacent steps", ok);
    ui_power_on(); song.rec = 1; seq_start(); go_home();
    int16_t baseline = TSEL->p[P_E0]; turn(EN_K1, 1);
    bad += check("REC plus a sound knob records motion through the UI", motion_count(TSEL) == 1 && motion_enabled(TSEL));
    press(B_PLAY); events_block(32);
    bad += check("stopping returns the original sound after a recorded knob gesture", TSEL->p[P_E0] == baseline);
    go_page(GR_MOTION); turn(EN_K1, -1);
    bad += check("#93 the page is AUTOMATION (GR_MOTION inside)", str_eq(cur_page()->title, "AUTOMATION"));
    bad += check("MOTION playback OFF preserves its stored events", !motion_enabled(TSEL) && motion_count(TSEL) == 1);
    turn(EN_K4, 1); press(B_OCTUP);
    bad += check("MOTION clear always asks confirmation", ui.confirm == CF_CLEAR_MOTION && motion_count(TSEL) == 1);
    {
        char qa[24], qb[24];
        confirm_text(qa, qb);
        bad += check("#93   the dialog asks CLEAR T1 AUTOMATION?", str_eq(qa, "CLEAR T1 AUTOMATION?"));
    }
    press(B_OCTDN); turn(EN_K4, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("confirmed MOTION clear removes the events", motion_count(TSEL) == 0 &&
                 msg_is("AUTOMATION CLEARED"));
    motion_full = 1; frame();
    bad += check("#93 the events used up: AUTOMATION FULL", msg_is("AUTOMATION FULL") && !motion_full);
    {   /* #63: a card shows MOTION's icon while the track's motion changes its parameter */
        uint32_t id = ENGINES[TSEL->eng_req]->knob[0];
        ui_power_on(); go_home(); frame();
        ok = !card_motion(0) && !card_motion(1);
        song.rec = 1; seq_start(); go_home(); turn(EN_K1, 1); frame();
        ok &= card_motion(0) && !card_motion(1) && !card_motion(2) && !card_motion(3);
        bad += check("MOTION: a recorded knob marks its HOME card (and no other)", ok && motion_count(TSEL) == 1);
        song.rec = 0; press(B_PLAY); events_block(32); frame();
        ok = card_motion(0);
        go_page(GR_MOTION); turn(EN_K1, -1); go_home(); frame();
        ok &= !card_motion(0) && motion_count(TSEL) == 1;
        bad += check("MOTION: PLAY OFF takes the icon off the card, the events stay", ok);
        go_page(GR_MOTION); turn(EN_K1, 1); go_home(); frame();
        ok = card_motion(0);
        track_select(1); frame();
        ok &= !card_motion(0);                           /* (another track: its own motion, none) */
        track_select(0); frame();
        ok &= card_motion(0);
        bad += check("MOTION: PLAY ON again shows it; only on the track that has the motion", ok);
        go_page(GR_TRK); frame();
        ok = !card_motion(0) && !card_motion(1) && !card_motion(2);
        motion_set_event(TSEL, 3, P_PAN, 20); frame();
        ok &= card_motion(1) && !card_motion(0) && !card_motion(2) && !card_motion(3);
        bad += check("MOTION: a mix parameter's event marks its MIXER card (PAN)", ok);
        go_page(GR_MOTION); turn(EN_K4, 1); press(B_OCTUP); press(B_OCTUP);
        ok = motion_count(TSEL) == 0;
        go_home(); frame();
        ok &= !card_motion(0) && !card_motion(1);
        go_page(GR_TRK); frame();
        ok &= !card_motion(1);
        bad += check("MOTION: CLEAR takes the icon off every card", ok && id < P_COUNT);
    }
    ui_power_on(); ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_PLAY]] = 1; led_pos[panel.btn[B_REC]] = 2;
    song.playing = song.rec = 1; ok = 1;
    for (i = 0; i < 120u; i++) { fm1_ms += 17; ui_leds(); ok &= (fm1_led[0] & 6u) == 4u && (fm1_led[8] & 2u); }
    bad += check("PLAY's green and REC LEDs stay lit across time and audio block phase", ok);
    {   /* the power-on sweep (hal/fm1_input.h fm1_led_anim_on) has the LEDs: the UI writes none of them while it runs,
         * all of its own picture on the next frame after */
        uint8_t l0[FM1_NCOL], d0[FM1_NCOL];
        memset(fm1_led, 0x5A, sizeof fm1_led);
        memset(fm1_led_dim, 0xA5, sizeof fm1_led_dim);
        memcpy(l0, fm1_led, sizeof l0);
        memcpy(d0, fm1_led_dim, sizeof d0);
        host_anim = 1;
        ui_leds();
        ok = !memcmp(l0, fm1_led, sizeof l0) && !memcmp(d0, fm1_led_dim, sizeof d0);
        host_anim = 0;
        ui_leds();
        ok &= (fm1_led[0] & 6u) == 4u && (fm1_led[8] & 2u) && fm1_led[1] != 0x5A;
        bad += check("the power-on LED sweep: the UI leaves the LEDs alone while it runs, takes them after", ok);
    }
    led_pos_init();
    ok = 1;
    for (p = 0; p < NPALETTES; p++) for (b = 0; b < 1u; b++) {
        uint16_t graph[240 * H_GRAPH], columns[240 * (Y_SEP_END - Y_LABEL)];
        ui_power_on(); palette_set(p); settings.zoom = 1;
        open_family(FAM_ENV); frame();
        memcpy(graph, host_screen + Y_GRAPH * 240, sizeof graph);
        memcpy(columns, host_screen + Y_LABEL * 240, sizeof columns);
        ui.hot_col = 1; ui.hot_t = 40; ui.force = 1; ui_draw();
        ok &= !memcmp(graph, host_screen + Y_GRAPH * 240, sizeof graph);
        int changed = 0;
        for (uint32_t y = 0; y < Y_SEP_END - Y_LABEL; y++) for (uint32_t x = 0; x < 240; x++) {
            uint16_t old = columns[y * 240 + x], now = host_screen[(Y_LABEL + y) * 240 + x];
            if (x >= 64 && x < 119) changed |= old != now;
            else ok &= ux.chamfer || old == now;        /* (the chamfered styles light the whole column frame) */
        }
        ok &= changed;
    }
    bad += check("all palettes: active column is subtle, stable-size, no zoom over graph", ok);
    ui_power_on(); return bad;
}

static int test_panel(void)
{
    int bad = 0;
    panel = PANEL_DEFAULT;
    panel.btn[B_PLAY] = panel.btn[B_REC];
    panel_init();
    bad += check("duplicate button mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.enc[EN_K1] = panel.enc[EN_K2];
    panel_init();
    bad += check("duplicate knob mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.btn[B_PLAY] = 255;
    panel_init();
    bad += check("out-of-range panel data recovers without an invalid shift", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.dir[EN_K1] = -1;
    panel_init();
    bad += check("a valid reversed knob mapping is retained", panel.dir[EN_K1] == -1);
    return bad;
}

/* ------------------------------------------------- the FX hold layer --- */
static void btn_down(uint32_t label) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
static void btn_up(uint32_t label) { fm1_in.buttons &= ~(1u << panel.btn[label]); }
static void key_down(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; keyboard_block(); }
static void key_up(uint32_t k) { fm1_in.notes &= ~(1u << k); keyboard_block(); }
static uint32_t gates(void)                        /* voices of every track with their key down */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].gate != 0;
    return n;
}
static uint32_t white(uint32_t w) { return key_at(0, w); }

/* a layer's key LEDs as the old 250 ms blink read them: lit, and the breathing ones (#119) "on" at even quarters,
 * so (a ^ b) over ms 0 and 250 = the ones that can be pressed, (a & b) = lit */
static uint32_t leds_at(uint32_t ms)
{
    uint32_t br, m;
    fm1_ms = ms;
    m = layer_leds(&br);
    return m | (((ms / 250u) & 1u) ? 0u : br);
}

static int test_layer(void)
{
    int bad = 0, ok, flash;
    uint32_t i, mo, k;
    /* tap: on release, the page; held briefly: no map ever */
    ui_power_on();
    btn_down(B_FX); frame();
    ok = ui.home;
    frames(64); btn_up(B_FX); frame();
    bad += check("FX tap: nothing on press, the FX page when let go", ok && !ui.home && cur_page()->fam == FAM_FX);
    frames(320);                                       /* (not a double tap: #83) */
    press(B_FX);
    bad += check("  a second tap: the next page of the family (DIST)", str_eq(cur_page()->title, "DIST"));
    ui_power_on();
    btn_down(B_FX);
    for (flash = 0, i = 0; i < 22u; i++) { frame(); flash |= ui.layer; }      /* 0.35 s */
    btn_up(B_FX); frame();
    bad += check("  held 0.35 s (HOLD 0.4): still a tap, the map never shown", !flash && !ui.home);
    /* peek: past HOLD alone, the map; letting go does nothing */
    ui_power_on();
    btn_down(B_FX); frames(368);
    ok = !ui.layer;
    frames(64);
    ok &= ui.layer == LAYER_FX;
    btn_up(B_FX); frame();
    frame();                                           /* (the keys follow the lock from the next frame) */
    ok &= ui.home && ui.layer == LAYER_FX && fx_lock;   /* (0.6.64) let go unused: locked open, FX LATCH OFF too */
    {   /* the effects momentary (FX LATCH OFF, the default): a key held plays it, let go stops it */
        uint32_t k = white(3);                         /* (the LPF key) */
        key_down(k); frame();
        ok &= (perf_held & PF_BIT(PF_LPF)) != 0u && !perf_latched;
        key_up(k); frame();
        ok &= !(perf_held & PF_BIT(PF_LPF)) && !perf_latched;
    }
    press(B_FX); frames(64);
    ok &= !fx_lock && !ui.layer;
    bad += check("FX held past 0.4 s: the map (not before); let go: no page, the map locked open (0.6.64), the effects "
                 "momentary; FX again: closed", ok && ui.home);
    /* the HOLD setting: 0.6 s */
    ui_power_on();
    hold(B_HOME); ui.menu_sel = MI_HOLD; turn(EN_K1, 1); turn(EN_K1, 1); ok = settings_hold == 3u; hold(B_HOME);
    btn_down(B_FX); frames(512); flash = ui.layer; btn_up(B_FX); frame();
    bad += check("menu HOLD 0.6 s (KNOB 1): FX held 0.5 s is a tap, no map", ok && !flash && !ui.home && !ui.menu);
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        settings_export(&p); settings_hold = HOLD_DEF;
        bad += check("  HOLD is saved with the settings and read back", settings_import(&p, sizeof p) && settings_hold == 3u);
    }
    /* the LEDS setting: KNOB 1 / OCT+ up to INV, left / OCT- down to OFF; saved in the retired zoom field */
    ui_power_on();
    hold(B_HOME); ui.menu_sel = MI_LEDS;
    ok = settings_leds == LEDS_DIM;
    turn(EN_K1, 1); ok &= settings_leds == LEDS_INV;
    turn(EN_K1, 1); ok &= settings_leds == LEDS_INV;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_DIM;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_DIM_LO;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_OFF;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_OFF;
    press(B_OCTUP); ok &= settings_leds == LEDS_DIM_LO && ui.menu == 1u;
    press(B_OCTUP); ok &= settings_leds == LEDS_DIM;
    press(B_OCTUP); ok &= settings_leds == LEDS_INV;
    press(B_OCTUP); ok &= settings_leds == LEDS_INV && ui.menu == 1u;   /* (stops at the end) */
    for (k = 0; k < 4u; k++) press(B_OCTDN);
    ok &= settings_leds == LEDS_OFF && ui.menu == 1u;
    ok &= !strcmp(LEDS_NAME[LEDS_OFF], "OFF") && !strcmp(LEDS_NAME[LEDS_DIM_LO], "DIM LO") &&
          !strcmp(LEDS_NAME[LEDS_DIM], "DIM HI") && !strcmp(LEDS_NAME[LEDS_INV], "INV");
    {   /* a setting changed from elsewhere (the editor's MENU_SET: menu_put) while the MENU is shown: redrawn */
        static uint16_t before[240 * 240];
        frame();
        memcpy(before, host_screen, sizeof before);
        menu_put(MI_LEDS, 3);                          /* (OFF -> INV, the row shown) */
        frame();
        ok &= settings_leds == LEDS_INV && memcmp(before, host_screen, sizeof before) && menu_get(MI_LEDS) == 3u;
        menu_put(MI_LEDS, 0);
        frame();
        ok &= !memcmp(before, host_screen, sizeof before);
    }
    hold(B_HOME);
    bad += check("menu LEDS: DIM HI by default; KNOB 1 OFF < DIM LO < DIM HI < INV (stops at the ends), OCT+ / OCT- the same; "
                 "a change from the editor redraws the shown MENU", ok && !ui.menu);
    {
        persist_t p = {0};
        uint32_t m;
        p.magic = PERSIST_MAGIC; p.panel = panel;
        ok = 1;
        settings.zoom = 0;                              /* (DIM HI keeps an older record's 0 / 1 as saved) */
        for (m = 0; m < LEDS_COUNT; m++) {
            settings_leds = (uint8_t)m;
            settings_export(&p); settings_leds = LEDS_DIM;
            ok &= settings_import(&p, sizeof p) && settings_leds == m && leds_stored_ok(p.zoom) &&
                  p.zoom == (m == LEDS_DIM ? 0u : LEDS_TAG | m);
            settings_export(&p);                    /* (idempotent: saved again, the same field) */
            ok &= p.zoom == (m == LEDS_DIM ? 0u : LEDS_TAG | m);
        }
        settings_leds = LEDS_DIM;
        settings_export(&p);
        bad += check("  LEDS is saved with the settings and read back (all four); DIM HI saves the field as before (0)",
                     ok && p.zoom == 0u && settings_import(&p, sizeof p) && settings_leds == LEDS_DIM);
    }
    /* combo: a key with FX: at once, silent, no MIDI, no recording, no step */
    ui_power_on();
    usb.config = 1; mo = mo_w;
    go_page(GR_ROLL); my_steps(TSEL); song.rec = 1; song.playing = 1;
    {
        step_t before[NSTEP];
        uint32_t cur = ui.cursor;
        memcpy(before, TSEL->step, sizeof before);
        btn_down(B_FX); key_down(white(2)); frame();
        ok = ui.layer == LAYER_FX && !gates() && mo_w == mo && (kb_layer >> white(2)) & 1u &&
             (perf_held & PF_BIT(PF_R32));                    /* white key 3 (A3): REPEAT 1/32 */
        for (i = 0; i < 40u; i++) events_block(CTL);
        ok &= !memcmp(before, TSEL->step, sizeof before) && ui.cursor == cur;
        bad += check("FX + a key: the map at once; no voice, no MIDI, no recording, no step written", ok);
        key_up(white(2)); frame();
        bad += check("  the key let go: its effect off, no note-off sent", !perf_held && !kb_layer && mo_w == mo);
        btn_up(B_FX); frame();
        bad += check("  FX let go after a combo: no tap, the map gone", !ui.layer && cur_page()->graph == GR_ROLL);
    }
    song.playing = 0; song.rec = 0;
    /* FX let go first: the effect stays with its key, the map too; a key pressed now is a note */
    ui_power_on();
    btn_down(B_FX); key_down(white(3)); frame();          /* white key 4 (B3): LPF */
    btn_up(B_FX); frame();
    ok = (perf_held & PF_BIT(PF_LPF)) && ui.layer == LAYER_FX;
    key_down(white(0)); frame();
    ok &= gates() > 0 && !((kb_layer >> white(0)) & 1u);
    key_up(white(0)); key_up(white(3)); frame();
    bad += check("FX let go first: the key holds its effect and the map; a new key is a note", ok && !ui.layer && !perf_held);
    /* G5 (the 16th white key) does nothing (F3 .. C4 the audio effects, D4 .. F5 the MIDI ones: pfx.c); KNOB 4 (DEPTH)
     * turns, back to 0 with FX */
    ui_power_on();
    usb.config = 1; mo = mo_w;
    btn_down(B_FX); key_down(white(15)); frame();
    ok = ui.layer == LAYER_FX && !gates() && mo_w == mo && !perf_held && (kb_layer >> white(15)) & 1u;
    turn(EN_K4, -40);
    ok &= perf_k[3] == 40;
    key_up(white(15)); btn_up(B_FX); frame();
    bad += check("FX + G5: nothing held, silent, no MIDI; KNOB 4 turns; FX let go: K4 back to 0",
                 ok && !perf_held && !ui.layer && !perf_k[3] && mo_w == mo);
    /* REVERB > TYPE (FX family, global): ROOM / SPRING on KNOB 1, kept by a project */
    ui_power_on();
    go_title("REVERB");
    ok = cur_page()->fam == FAM_FX && cur_page()->id[0] == G_RTYPE && cur_page()->id[1] == G_RSIZE &&
         cur_page()->id[2] == G_RDAMP && song.g[G_RTYPE] == 0;
    turn(EN_K1, 1);
    ok &= song.g[G_RTYPE] == 1 && str_eq(GP[G_RTYPE].names[song.g[G_RTYPE]], "SPRING");
    turn(EN_K1, 5);                                  /* (JIANT 0.5: SHIMR RESO CLOUD after it) */
    ok &= song.g[G_RTYPE] == 4 && str_eq(GP[G_RTYPE].names[4], "CLOUD");
    song.playing = 0;
    project_save(2);
    turn(EN_K1, -1);
    ok &= song.g[G_RTYPE] == 3;
    project_load(2);
    bad += check("REVERB page: TYPE ROOM -> SPRING .. CLOUD on KNOB 1 (SIZE, DAMP beside it); a project keeps CLOUD",
                 ok && song.g[G_RTYPE] == 4);
    {   /* (0.5.1) the DRUM-X kit's X-MOD kept by a project (the kit's reserved bytes) */
        static const uint8_t XM[4] = {6, 90, 4, 70};
        memcpy(dx_mot, XM, 4);
        song.playing = 0;
        project_save(2);
        memset(dx_mot, 0, 4);
        project_load(2);
        bad += check("X-MOD: a project keeps the kit's RATE DPTH SHPE RAND", !memcmp(dx_mot, XM, 4));
        memset(dx_mot, 0, 4);
    }
    go_title("CHORUS");
    bad += check("  CHORUS page: CRT CDP STER TYPE (0.6.9; QG-1 TYPE)", cur_page()->fam == FAM_FX && cur_page()->id[0] == G_CRATE &&
                 cur_page()->id[1] == G_CDEPTH && cur_page()->id[2] == G_CSTER && cur_page()->id[3] == G_CTYPE);
    {   /* (QG-1) CHORUS > TYPE PHASER: a project keeps it (bit 1 of 'DT'), the delay's TYPE beside it untouched */
        int16_t dt0 = song.g[G_DTYPE];
        song.playing = 0;
        song.g[G_CTYPE] = 1; song.g[G_DTYPE] = 0;
        project_save(2);
        song.g[G_CTYPE] = 0; song.g[G_DTYPE] = 1;
        project_load(2);
        ok = song.g[G_CTYPE] == 1 && song.g[G_DTYPE] == 0;
        song.g[G_CTYPE] = 0; song.g[G_DTYPE] = 1;
        project_save(2);
        song.g[G_CTYPE] = 1;
        project_load(2);
        ok &= song.g[G_CTYPE] == 0 && song.g[G_DTYPE] == 1;
        song.g[G_CTYPE] = 0; song.g[G_DTYPE] = dt0;
        bad += check("  CHORUS TYPE PHASER: a project keeps it, the delay's TYPE its own", ok);
    }
    /* chording: a key held before FX stays a note, its note-off arrives */
    ui_power_on();
    mo = mo_w;
    key_down(white(4)); frame(); btn_down(B_FX); frame(); key_up(white(4)); frame(); btn_up(B_FX); frame();
    for (i = 0; i < 4u; i++) events_block(CTL);
    bad += check("a key held before FX stays a note: note-on, note-off, nothing left held", mo_w == mo + 2u && !gates() &&
                 !kb_layer);
    /* knob macros: not the page's, not recorded, back to 0 when FX is let go */
    ui_power_on();
    go_title("ENV"); song.rec = 1; song.playing = 1;
    {
        int16_t atk = TSEL->p[P_ATK];
        uint32_t ev = motion.count;
        btn_down(B_FX); frame();
        turn(EN_K1, -12); turn(EN_K2, 30);
        ok = perf_k[0] == -12 && perf_k[1] == 30 && TSEL->p[P_ATK] == atk && motion.count == ev && ui.layer == LAYER_FX;
        turn(EN_PRESET, 3);
        ok &= user_of(TSEL) == UP_SLOTS && TSEL->preset == trk[0].preset;
        btn_up(B_FX); frame();
        bad += check("FX + KNOB 1 / 2: the macros, not ATK, not recorded; FX let go: back to 0, no tap",
                     ok && !perf_k[0] && !perf_k[1] && str_eq(cur_page()->title, "ENV"));
    }
    song.rec = 0; song.playing = 0;
    /* buttons in the layer: PLAY works, SAVE, HOME, a page button are swallowed */
    ui_power_on();
    btn_down(B_FX); frame();
    press(B_PLAY);
    ok = transport_req == 1u && ui.layer == LAYER_FX;
    transport_req = 0;
    press(B_SAVE); ok &= ui.home;
    hold(B_SAVE); ok &= !msg_is("UNDO/REDO T1") && ui.home;
    press(B_ENV); ok &= ui.home;
    hold(B_HOME); ok &= !ui.menu && ui.home;
    btn_up(B_FX); frame();
    bad += check("in the layer: PLAY plays; SAVE (tap and hold), ENV, HOME (hold) do nothing", ok && ui.home && !ui.menu);
    btn_down(B_FX); frame(); btn_down(B_ENV); frame(); btn_up(B_FX); frame(); btn_up(B_ENV); frame();
    bad += check("  a page button pressed with FX, let go after it: still nothing", ui.home);
    /* release-act: every page button acts when let go */
    {
        static const uint8_t PB[6] = {B_ENV, B_LFO, B_SCL, B_ARP, B_GLO, B_EDIT};
        static const uint8_t PF[6] = {FAM_ENV, FAM_LFO, FAM_SCL, FAM_ARP, FAM_GLO, FAM_EDIT};
        ok = 1;
        for (i = 0; i < 6u; i++) {
            ui_power_on();
            btn_down(PB[i]); frame();
            ok &= ui.home;
            btn_up(PB[i]); frame();
            ok &= !ui.home && (PB[i] == B_GLO ? cur_page()->graph == GR_TRK : cur_page()->fam == PF[i]);
            if (PB[i] == B_SCL || PB[i] == B_GLO || PB[i] == B_EDIT || PB[i] == B_LFO)
                continue;                             /* (a layer of their own: held long is a peek; LFO: MACRO) */
            ui_power_on();
            hold(PB[i]);                              /* (no layer of their own: a long press is a tap too) */
            ok &= !ui.home && cur_page()->fam == PF[i];
        }
        bad += check("ENV LFO SCL ARP GLO EDIT act when let go, not on press (ENV ARP held long: the same)", ok);
    }
    ui_power_on();
    go_page(GR_ROLL); my_steps(TSEL); ui.cursor = 0;
    btn_down(B_EDIT); frame();
    ok = step_on(&TSEL->step[0]) && ui.cursor == 0;
    btn_up(B_EDIT); frame();
    bad += check("EDIT on STEP clears the step when let go (not on press)", ok && !step_on(&TSEL->step[0]) && ui.cursor == 1);
    ui_power_on(); hold(B_HOME);
    ok = ui.menu == 1; hold(B_HOME); ok &= !ui.menu;
    bad += check("HOME (menu) hold of 0.7 s unchanged", ok);
    /* no layer in the menu or a dialog: FX + a key is a note */
    ui_power_on(); hold(B_HOME);
    btn_down(B_FX); frame(); key_down(white(3)); frame();
    ok = !ui.layer && !perf_mask && gates() > 0 && !kb_layer;
    key_up(white(3)); btn_up(B_FX); frame();
    bad += check("in the menu: no layer, FX + a key plays the key; FX let go: no page", ok && ui.menu == 1);
    ui_power_on(); ui.confirm = CF_CLEAR_SEQ;
    btn_down(B_FX); frames(512);
    ok = !ui.layer;
    btn_up(B_FX); frame();
    bad += check("in a dialog: FX held shows no map and taps nothing", ok && ui.confirm == CF_CLEAR_SEQ);
    /* UBOOT: the countdown closes the map and stops every effect; OCT- + OCT+ are still seen */
    ui_power_on();
    btn_down(B_FX); key_down(white(5)); frame();
    btn_down(B_OCTDN); btn_down(B_OCTUP); frame();
    ok = ui.layer == LAYER_FX && (fm1_in.buttons & 3u << panel.btn[B_OCTDN]) != 0u;
    ui.uboot = 3; frame();                          /* (main.c: OCT- + OCT+ held 2 s) */
    ok &= !ui.layer && perf_kill && !perf_mask && perf_begin(CTL) >= 0;
    ok &= (fm1_in.buttons & ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]))) ==
          ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]));
    ui.uboot = 0;
    btn_up(B_OCTDN); btn_up(B_OCTUP); key_up(white(5)); btn_up(B_FX); frame();
    bad += check("UBOOT countdown over the layer: the map closes, effects off; OCT- + OCT+ untouched", ok && !perf_kill && !kb_layer);
    /* the LEDs of the map */
    ui_power_on();
    btn_down(B_FX); key_down(white(1)); frame();
    {
        uint32_t a, b2;
        song.g[G_BPM] = 120;
        a = leds_at(0); b2 = leds_at(250);
        k = white(1);
        ok = ((a >> k) & 1u) && ((b2 >> k) & 1u);                    /* held: lit */
        for (i = 0; i < 15u; i++)                                     /* the other effects (F3 .. F5): breathe */
            ok &= i == 1u || ((a ^ b2) >> white(i)) & 1u;
        ok &= !((a | b2) >> white(15) & 1u);                          /* G5: no effect, dark */
        ok &= ((a ^ b2) >> key_at(1, 0)) & 1u && ((a ^ b2) >> key_at(1, 6)) & 1u &&   /* a mute, (QG-1) VIB breathe; */
              !((a | b2) >> key_at(1, 7) & 1u);                     /* A#4 (the tracks) dark */
        song.g[G_BPM] = 72;
        a = leds_at(0); b2 = leds_at(250);
        ok &= !((a | b2) >> white(0) & 1u);                          /* 1/8: too long at 72 */
        ok &= ((a ^ b2) >> white(2)) & 1u;                            /* 1/32 still blinks */
        song.g[G_BPM] = 120;
    }
    key_up(white(1)); btn_up(B_FX); frame();
    bad += check("map LEDs: held lit, the 19 effects breathe, G5 and a too-long REPEAT dark", ok);
    usb.config = 0;
    return bad;
}

/* ------------------------------------------------------------ NAME --- */
static void nm_tap(uint32_t k)                    /* a key tapped in NAME: down a frame, up a frame (32 ms) */
{
    fm1_in.notes |= 1u << k; host_notes |= 1u << k; frame();
    fm1_in.notes &= ~(1u << k); frame();
}
static uint32_t nm_black_key(uint32_t f, uint32_t octave)   /* the black key of function f (NB_*: F# G# A# C# D#), octave 0 / 1 */
{
    return key_at(1, f + 5u * octave);
}
static int test_name(void)
{
    int bad = 0, ok;
    uint32_t i, mo;
    char b[16];
    ui_power_on();
    go_page(GR_USER);
    ui.uslot = 2;
    press(B_OCTUP);                               /* (SAVE picked on entry) */
    up_auto_name(b, TSEL->eng_req, 2);
    bad += check("NAME: USER SAVE into an empty slot opens NAME, prefilled with the automatic name",
                 name_on() && nm.kind == NK_USER_SAVE && nm.slot == 2u && str_eq(nm.s, b) && nm.cur == nm.len);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    bad += check("  DELETE removes the character before the cursor, down to empty", nm.len == 0u && nm.cur == 0u && !nm.s[0]);
    /* multi-tap: the same key cycles, another key commits, 0.8 s commits */
    nm_tap(white(0));
    ok = nm.len == 1u && nm.s[0] == 'A' && nm.key == 1u && nm.cur == 0u;
    nm_tap(white(0));
    ok &= nm.s[0] == 'B' && nm.len == 1u;
    nm_tap(white(0));
    bad += check("  a white key types its first letter; tapped again within 0.8 s: the next one, cycling (A B A)",
                 ok && nm.s[0] == 'A' && nm.len == 1u && nm.cur == 0u);
    nm_tap(white(1));
    bad += check("  another key keeps the letter and types its own (A, C)", nm.len == 2u && str_eq(nm.s, "AC") && nm.cur == 1u &&
                 nm.key == 2u);
    frames(800);
    bad += check("  0.8 s without a tap keeps it: the cursor moves on", nm.key == 0u && nm.cur == 2u && str_eq(nm.s, "AC"));
    nm_tap(white(1));
    frames(400);
    nm_tap(white(1));
    bad += check("  a second tap within 0.8 s (0.43 s later) still cycles (C -> D)", str_eq(nm.s, "ACD") && nm.key == 2u);
    frames(800);
    nm_tap(white(4)); nm_tap(white(4)); nm_tap(white(4));   /* IJK: K */
    nm_tap(nm_black_key(NB_SPACE, 1));                       /* SPACE (the upper octave's G#) keeps K first */
    bad += check("  a three-letter key (IJK) cycles to K; SPACE keeps it and adds a space", str_eq(nm.s, "ACDK ") && nm.cur == 5u &&
                 !nm.key);
    nm_tap(nm_black_key(NB_DEL, 1));
    nm_tap(nm_black_key(NB_LEFT, 0));
    nm_tap(nm_black_key(NB_LEFT, 0));
    nm_tap(white(2));                             /* E inserted before D */
    frames(800);
    bad += check("  DELETE, cursor left twice, a letter inserted at the cursor", str_eq(nm.s, "ACEDK") && nm.cur == 3u);
    nm_tap(nm_black_key(NB_RIGHT, 1));
    nm_tap(nm_black_key(NB_RIGHT, 0));
    nm_tap(nm_black_key(NB_RIGHT, 0));
    bad += check("  cursor right stops at the end", nm.cur == 5u);
    /* 123 */
    nm_tap(nm_black_key(NB_MODE, 0));
    nm_tap(white(1)); nm_tap(white(9)); nm_tap(white(15));
    bad += check("  D# switches to 123: one tap, one character (2, 0, +), no cycling", nm.num && str_eq(nm.s, "ACEDK20+") &&
                 nm.cur == 8u && !nm.key);
    nm_tap(nm_black_key(NB_MODE, 1));
    nm_tap(white(15)); nm_tap(white(15)); nm_tap(white(15));
    frames(800);
    bad += check("  back to ABC: the last key (0-.) gives 0, then -, then .", !nm.num && str_eq(nm.s, "ACEDK20+.") && nm.cur == 9u);
    /* the knobs */
    turn(EN_K1, -2);
    turn(EN_K2, 1);
    bad += check("  KNOB 1 moves the cursor, KNOB 2 changes the character there (+ -> SPACE, wrapping)",
                 nm.cur == 7u && str_eq(nm.s, "ACEDK20 ."));
    turn(EN_K2, 5);
    bad += check("  KNOB 2 on: SPACE + 5 = E", nm.s[7] == 'E');
    turn(EN_K1, 9);
    turn(EN_K2, 1);
    bad += check("  KNOB 1 to the end, KNOB 2 adds a character there (A)", str_eq(nm.s, "ACEDK20E.A") && nm.cur == 9u && nm.len == 10u);
    turn(EN_K2, -2);
    bad += check("  .. and turns it back past SPACE to the last symbol (+)", nm.s[9] == '+');
    /* full */
    nm_tap(nm_black_key(NB_RIGHT, 0));
    nm_tap(white(7)); frames(800); nm_tap(white(7)); frames(800);
    ui.msg_t = 0;
    nm_tap(white(7));
    bad += check("  12 characters at most: NAME FULL, nothing typed", nm.len == 12u && msg_is("NAME FULL") && str_eq(nm.s, "ACEDK20E.+PP"));
    /* held DELETE repeats */
    fm1_in.notes |= 1u << nm_black_key(NB_DEL, 0); host_notes |= 1u << nm_black_key(NB_DEL, 0);
    frames(16 * 40);
    fm1_in.notes &= ~(1u << nm_black_key(NB_DEL, 0)); frame();
    bad += check("  DELETE held 0.64 s: one, then it repeats after 0.45 s, every 0.09 s (3 in all)", nm.len == 9u);
    /* the keys never sound, record or send MIDI */
    usb.config = 1; mo = mo_w; song.rec = 1;
    {
        step_t before[NSTEP];
        memcpy(before, TSEL->step, sizeof before);
        key_down(white(3)); key_down(nm_black_key(NB_SPACE, 0)); frame();
        for (i = 0; i < 8u; i++) events_block(CTL);
        ok = !gates() && mo_w == mo && !memcmp(before, TSEL->step, sizeof before) && song.grid == 2u;
        key_up(white(3)); key_up(nm_black_key(NB_SPACE, 0)); frame();
        bad += check("  keys in NAME: no voice, no MIDI, nothing recorded (seq.c: song.grid 2)", ok && mo_w == mo);
    }
    song.rec = 0; usb.config = 0;
    {   /* LEDs: all 27 keys do something; the cycling key blinks */
        uint32_t a, c, t0;
        nm_tap(nm_black_key(NB_DEL, 0)); nm_tap(nm_black_key(NB_DEL, 0));
        nm_tap(white(5));
        t0 = (fm1_ms / 500u + 1u) * 500u;
        fm1_ms = t0; a = name_leds(); fm1_ms = t0 + 250u; c = name_leds();
        nm.t = fm1_ms;                            /* (the letter still cycling) */
        bad += check("  LEDs: every key lit, the cycling key (LM) blinks", ((a | c) == 0x7FFFFFFu) && ((a ^ c) == (1u << white(5))));
    }
    /* cancel */
    {
        uint32_t page = ui.page;
        press(B_OCTDN);
        bad += check("  OCT- cancels: nothing written, back on the USER page", !name_on() && !up_used(2) && ui.page == page && !ui.home);
    }
    /* typing a name and saving it */
    ui.act = 4;
    press(B_OCTUP);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    nm_tap(white(1)); nm_tap(white(0)); nm_tap(white(0)); nm_tap(nm_black_key(NB_RIGHT, 0));    /* C B */
    nm_tap(nm_black_key(NB_SPACE, 0)); nm_tap(nm_black_key(NB_MODE, 0)); nm_tap(white(1));        /* " 2" */
    nm_tap(nm_black_key(NB_SPACE, 0));                                                           /* a trailing space */
    song.playing = 1;
    press(B_OCTUP);
    bad += check("  OCT+ while playing: STOP TO SAVE, NAME stays", name_on() && msg_is("STOP TO SAVE") && !up_used(2));
    stop_transport();
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  OCT+ stopped: saved as typed, the trailing space dropped (CB 2)", !name_on() && up_used(2) && str_eq(b, "CB 2") &&
                 msg_is("SAVED (NO FLASH)"));
    /* SAVE held does nothing in NAME; the FX layer neither */
    ui.act = 4; press(B_OCTUP); press(B_OCTUP);   /* (U03 used: OVERWRITE?, YES) */
    bad += check("  overwrite: the dialog, then NAME", name_on() && nm.kind == NK_USER_SAVE);
    ui.msg_t = 0;
    hold(B_SAVE);
    btn_down(B_FX); frames(600); ok = !ui.layer; btn_up(B_FX); frame();
    bad += check("  SAVE held, FX held: nothing in NAME (no undo, no map, no page)", name_on() && ok && cur_page()->graph == GR_USER &&
                 !ui.msg_t);
    press(B_OCTDN);
    /* rename: EDIT on USER */
    ui.uslot = 5;
    press(B_EDIT);
    bad += check("  EDIT on an empty slot: EMPTY SLOT", !name_on() && msg_is("EMPTY SLOT") && cur_page()->graph == GR_USER);
    ui.uslot = 2;
    up_load(2);                                   /* (the sound now comes from U03) */
    trk[0].p[P_E0] = 33;                          /* an edit: a rename must not store it */
    press(B_EDIT);
    bad += check("  EDIT on a used slot: NAME (rename) with its name", name_on() && nm.kind == NK_USER_RENAME && str_eq(nm.s, "CB 2"));
    nm_tap(nm_black_key(NB_DEL, 0));
    nm_tap(white(10)); nm_tap(white(10));          /* VW: W */
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  OCT+: renamed (CB W), the sound stored there unchanged", !name_on() && str_eq(b, "CB W") &&
                 up_value(up_rec(2), P_E0) != 33 && msg_is("RENAMED (NO FLASH)"));
    press(B_EDIT);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    press(B_OCTUP);
    {
        char au[16];
        up_name(2, b);
        up_auto_name(au, up_rec(2)->engine, 2);
        bad += check("  a name deleted to nothing: the automatic one", str_eq(b, au));
    }
    /* the sound's own name is the prefill */
    trk[0].user = 3;
    ui.uslot = 7; ui.act = 4;
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  SAVE of a sound that came from U03: prefilled with U03's name", name_on() && str_eq(nm.s, b));
    press(B_OCTDN);
    /* projects */
    go_page(GR_SLOTS);
    song.g[G_SLOT] = 2;
    turn(EN_K4, 1);
    press(B_OCTUP);
    for (i = 0; i < 4u; i++) { nm_tap(white(7)); nm_tap(white(6)); frames(800); }   /* P N P N .. */
    press(B_OCTUP);
    {
        char pn[16];
        int named = project_name(1, pn);
        bad += check("PROJECT SAVE: NAME, typed, saved: the slot has the name, the project too", named && str_eq(pn, "PNPNPNPN") &&
                     str_eq(proj_name, "PNPNPNPN"));
    }
    proj_name[0] = 0;
    project_load(1);
    bad += check("  loading it brings its name back (the next save's prefill)", str_eq(proj_name, "PNPNPNPN"));
    trk[0].p[P_LEVEL] = 7;
    press(B_EDIT);
    for (i = 0; i < 4u; i++) nm_tap(nm_black_key(NB_DEL, 1));
    press(B_OCTUP);
    {
        char pn[16];
        project_name(1, pn);
        bad += check("  EDIT renames a project (PNPN); its music unchanged", str_eq(pn, "PNPN") && stored_param(1, 0, P_LEVEL) != 7 &&
                     msg_is("RENAMED (NO FLASH)"));
    }
    song.g[G_SLOT] = 4;
    press(B_EDIT);
    bad += check("  EDIT on an empty project slot: EMPTY SLOT", !name_on() && msg_is("EMPTY SLOT"));
    song.g[G_SLOT] = 2;
    press(B_EDIT);
    hold(B_HOME);
    bad += check("  HOME held in NAME: the menu, NAME closed", ui.menu && !name_on());
    hold(B_HOME);
    return bad;
}

/* the EDIT cycle (no ENGINE page: engines are the EDIT layer's) and its memory */
static int engine_cycle(const char *const *want, uint32_t n)   /* EDIT tapped from HOME, then n - 1 times more */
{
    uint32_t i;
    int ok = 1;
    frames(320);
    go_home(); frame();
    for (i = 0; i < n; i++) {
        press(B_EDIT);
        frames(320);                                  /* (taps further apart than a double tap: #83 locks) */
        ok &= str_eq(cur_page()->title, want[i]);
    }
    return ok;
}
static int test_edit_cycle(void)
{
    static const char *const CYC_A[] = {"EDIT 1", "EDIT 2", "MORPH", "FILTER", "VOICE", "EDIT 1"};   /* (JIANT 0.5: ANALOG's
                                                         * FILTER; 0.6.45: MORPH) */
    static const char *const CYC_F[] = {"EDIT 1", "EDIT 2", "OPERATOR", "OP ENV", "OPERATOR 2", "FILTER", "VOICE",
                                        "EDIT 1"};   /* (the part's FILTER; 0.6.10: FM6's operator pages) */
    static const char *const CYC_D[] = {"EDIT 1", "EDIT 2", "FILTER", "OP1 ENV", "OP2 ENV", "OP3 ENV", "OP4 ENV",
                                        "OP LEVEL", "VOICE", "EDIT 1"};
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    btn_down(B_EDIT); frame();
    ok = ui.home;
    btn_up(B_EDIT); frame();
    bad += check("EDIT opens EDIT 1, when let go (not on press)", ok && str_eq(cur_page()->title, "EDIT 1"));
    for (i = 0; i < NPAGES; i++)
        ok &= !str_eq(PAGES[i].title, "ENGINE");
    bad += check("no ENGINE page (engines are the EDIT layer's)", ok);
    set_engine_of(TSEL, 0);
    bad += check("EDIT cycle (ANALOG): EDIT 1 EDIT 2 MORPH FILTER VOICE EDIT 1", engine_cycle(CYC_A, NELEM(CYC_A)));
#if FELUCCA_FM4
    set_engine_of(TSEL, 1);
    bad += check("EDIT cycle (DIGITAL): EDIT 1 EDIT 2 FILTER OP1..OP4 ENV OP LEVEL VOICE EDIT 1",
                 engine_cycle(CYC_D, NELEM(CYC_D)));
#else
    set_engine_of(TSEL, ENGI_DIGITAL);
    bad += check("EDIT cycle (engine 1 asked for: FM6, DIGITAL retired): EDIT 1 EDIT 2 OPERATOR OP ENV OPERATOR 2 FILTER VOICE EDIT 1",
                 TSEL->eng_req == ENGI_FM6 && engine_cycle(CYC_F, NELEM(CYC_F)));
    (void)CYC_D;
#endif
    {   /* #97: a DRUM track's lane levels on EDIT > LANES (KICK SNARE CLAP HATCL) and LANES 2 (HATOP TOM RIM BELL) */
        static const char *const CYC_K[] = {"EDIT 1", "EDIT 2", "KIT 3", "X-MOD", "FILTER", "SOUND", "SOUND 2", "SOUND 3",
                                            "SOUND 4", "LEVELS", "LEVELS 2", "STEREO", "VOICE", "EDIT 1"};
        set_engine_of(TSEL, ENGI_DRUM);
        bad += check("EDIT cycle (DRUM, 0.6.58): the kit's (KIT KIT 2 KIT 3 X-MOD FILTER), a sound's (SOUND 1..4), then the volumes and the stereo (LEVELS LEVELS 2 STEREO), VOICE",
                     engine_cycle(CYC_K, NELEM(CYC_K)));
        {   /* (0.6.58) KIT 3 turns BP and FEEDBK only; STEREO's KNOB 1 is IMAGE */
            uint8_t t0 = dx_tone;
            int16_t i0 = TSEL->p[P_DXIMG];
            go_title("KIT 3"); frame();
            turn(EN_K3, 3); frame();
            ok = TSEL->p[P_DXIMG] == i0 && dx_tone == t0;
            go_title("STEREO"); frame();
            turn(EN_K1, 5); frame();
            ok &= TSEL->p[P_DXIMG] > i0 && dx_tone == t0;
            TSEL->p[P_DXIMG] = i0;
            bad += check("KIT 3: KNOB 3 does nothing (IMAGE moved); STEREO: KNOB 1 turns IMAGE", ok);
        }
        go_title("LEVELS 2"); frame();
        ok = TSEL->p[P_LN5] == 127;
        turn(EN_K2, -3);
        ok &= TSEL->p[P_LN5] == 124 && TSEL->p[P_LN4] == 127 && TSEL->p[P_LN6] == 127;
        bad += check("LEVELS 2: KNOB 2 turns TOM's level (100 % by default), the other lanes stay", ok);
        apply_preset_to(TSEL, 0);
        bad += check("a factory preset load: every lane 100 % again", TSEL->p[P_LN5] == 127);
        ui.fam_last[FAM_EDIT] = ui.page;
        set_engine_of(TSEL, 0); frame();
        bad += check("LEVELS 2 of a track no longer DRUM falls back to EDIT 1", str_eq(cur_page()->title, "EDIT 1"));
    }
    go_title("OP2 ENV"); ui.fam_last[FAM_EDIT] = ui.page;
    set_engine_of(TSEL, 0); frame();
    bad += check("an OP page of a track no longer DIGITAL falls back to EDIT 1", str_eq(cur_page()->title, "EDIT 1"));
    go_title("EDIT 2"); ui.fam_last[FAM_EDIT] = ui.page;
    go_home(); press(B_EDIT);
    bad += check("EDIT remembers its last page (as every family)", str_eq(cur_page()->title, "EDIT 2"));
    return bad;
}


/* ------------------------------------------------ the GLO SCL EDIT layers --- */
static uint32_t black(uint32_t b) { return key_at(1, b); }
static void lay_combo(uint32_t btn, uint32_t k) { btn_down(btn); key_down(k); frame(); }
static void oct_back(void) { btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); }

static int test_quick_layers(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static const char *const HINT[3] = {"HOLD [GLO] QUICK", "HOLD [SCL] QUICK", "HOLD [EDIT] QUICK"};
    int bad = 0, ok, flash, hint;
    uint32_t i, k, a, b2, mo;
    track_t before;
    /* tap / peek / combo, the hint once */
    ok = 1; hint = 1;
    for (i = 0; i < 3u; i++) {
        ui_power_on();
        btn_down(LB[i]);
        for (flash = 0, k = 0; k < 20u; k++) { frame(); flash |= ui.layer; }      /* 0.32 s */
        btn_up(LB[i]); frame();
        ok &= !flash && !ui.home && (LB[i] == B_GLO ? cur_page()->graph == GR_TRK :
                                     LB[i] == B_SCL ? cur_page()->fam == FAM_SCL : str_eq(cur_page()->title, "EDIT 1"));
        hint &= msg_is(HINT[i]);
        go_home(); frame();
        btn_down(LB[i]); frames(368);
        ok &= !ui.layer;
        frames(64);
        ok &= ui.layer == LL[i] && ((layer_seen >> LL[i]) & 1u);
        btn_up(LB[i]); frame();
        ok &= ui.home && !ui.layer;
        ui.msg_t = 0;
        press(LB[i]);
        hint &= !msg_is(HINT[i]);
        go_home(); frame();
        btn_down(LB[i]); key_down(white(9)); frame();
        ok &= ui.layer == LL[i] && !gates();
        key_up(white(9)); btn_up(LB[i]); frame();
        ok &= ui.home && !ui.layer;
    }
    bad += check("GLO SCL EDIT: a tap opens the page, held past HOLD the map (no page), a key: the map at once", ok);
    bad += check("  after a tap \"HOLD [BTN] QUICK\" until the layer has been opened once", hint);
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        layer_seen = 0x1Cu;
        settings_export(&p); layer_seen = 0;
        bad += check("  the seen bits are kept with the settings (a spare favorites byte)",
                     settings_import(&p, sizeof p) && layer_seen == 0x1Cu);
    }

    {   /* (JIANT 0.6) how full the music is for a section: said once, 1.5 s after the last change */
        uint32_t k2, i2, p0;
        ui_power_on(); ui.msg_t = 0;
        proj_fill_poll(autosave_sig()); fm1_ms += 2000u; proj_fill_poll(autosave_sig());
        p0 = proj_fill_pct();
        ok = !ui.msg_t && p0 < 35u;
        for (k2 = 0; k2 < NTRK; k2++)                   /* every step a chord on every track: more than a section */
            for (i2 = 0; i2 < NSTEP; i2++) {
                trk[k2].step[i2].time = ST_NOTE; trk[k2].step[i2].n = 4; trk[k2].step[i2].vel = 100;
                trk[k2].step[i2].hit = 1; step_set_chance(&trk[k2].step[i2], 50u);   /* (chords alone fit: 6 bytes a step) */
            }
        proj_fill_poll(autosave_sig());
        ok &= !ui.msg_t;                                /* (not at once) */
        fm1_ms += 2000u; proj_fill_poll(autosave_sig());
        ok &= msg_is("TOO FULL TO SAVE") && proj_fill_pct() > 100u;
        ui.msg_t = 0; fm1_ms += 2000u; proj_fill_poll(autosave_sig());
        ok &= !ui.msg_t;                                /* (said once) */
        bad += check("JIANT 0.6: the music too full for a section: TOO FULL TO SAVE once; the power-on music ~1/4 full", ok);
        printf("ui:   (power-on music: %u %% of a section)\n", (unsigned)p0);
    }
    {   /* (JIANT 0.6.6) a page button held + PRESETS: its pages one by one, the title said; let go: no tap, no layer */
        const char *t1, *t3;
        ui_power_on();
        go_home(); frames(8);
        btn_down(B_FX); frame();
        turn(EN_PRESET, 1); frame();                    /* from HOME: FX's first page */
        t1 = cur_page()->title;
        turn(EN_PRESET, 2); frame();                    /* two on */
        t3 = cur_page()->title;
        ok = !ui.home && str_eq(t1, "FX") && str_eq(t3, "AUTO PAN") && msg_is("AUTO PAN");   /* (0.6.9: after DIST) */
        frames(600);                                    /* (held past HOLD: still no map) */
        ok &= !ui.layer;
        btn_up(B_FX); frames(4);
        ok &= str_eq(cur_page()->title, "AUTO PAN") && !ui.layer;   /* (no tap: no next page) */
        btn_down(B_FX); frame(); turn(EN_PRESET, -1); frame(); btn_up(B_FX); frames(4);
        ok &= str_eq(cur_page()->title, "DIST");
        btn_down(B_FX); frame(); turn(EN_PRESET, -9); frame(); btn_up(B_FX); frames(4);
        ok &= str_eq(cur_page()->title, "FX");          /* (no wrap: it stops at the first) */
        btn_down(B_ENV); frame(); turn(EN_PRESET, 1); frame(); turn(EN_PRESET, 1); frame(); btn_up(B_ENV); frames(4);
        ok &= str_eq(cur_page()->title, "ENV DEST");    /* (ENV: no layer; its tap swallowed) */
        press(B_ENV); frames(8);
        ok &= str_eq(cur_page()->title, "ENV");         /* (a plain tap: the next page, as always) */
        bad += check("JIANT 0.6.6: a page button held + PRESETS jumps through its pages (no wrap); let go: no tap, no layer; a tap as before", ok);
        {   /* (0.6.67) a button held + SELECT: ENV (no dice of its own) its pages, the turn's last detents after letting
             * go swallowed; SEQ, LFO, FX their dice under KEEP? (CANCEL: what was; OK: kept); GLO + SELECT the tempo */
            uint32_t j, sig, okk;
            int16_t bpm, lf[8], rs, rv;
            go_home(); frames(8);
            btn_down(B_ENV); frame(); turn(EN_SELECT, 1); frame();
            ok = str_eq(cur_page()->title, "ENV") && !ui.confirm;
            turn(EN_SELECT, 1); frame();
            ok &= str_eq(cur_page()->title, "ENV DEST");
            btn_up(B_ENV); frame(); turn(EN_SELECT, 1); frame();   /* (the same turn, the button let go first) */
            ok &= str_eq(cur_page()->title, "ENV DEST") && !ui.layer;
            frames(30);
            sig = steps_sig(TSEL);
            held_turn(B_SEQ, 1);
            ok &= ui.confirm == CF_DICE_SEQ && steps_sig(TSEL) != sig && str_eq(cur_page()->title, "ENV DEST");
            press(B_OCTDN); frame();
            ok &= !ui.confirm && steps_sig(TSEL) == sig && msg_is("SEQ BACK");
            for (j = 0; j < 8u; j++) lf[j] = TSEL->p[P_LRATE + j];
            fm1_ms += 1200u;
            held_turn(B_LFO, 1);
            okk = ui.confirm == CF_DICE_LFO;
            press(B_OCTUP); frame();
            for (j = 0, sig = 0; j < 8u; j++) sig += lf[j] != TSEL->p[P_LRATE + j];
            ok &= okk && !ui.confirm && sig > 0u && msg_is("LFO DICED");
            rs = song.g[G_RSIZE]; rv = TSEL->p[P_REV];
            fm1_ms += 1200u;
            for (j = 0, okk = 0; j < 4u && !okk; j++) {   /* (a dice may land where it was: again) */
                held_turn(B_FX, 1); fm1_ms += 1200u;
                okk = ui.confirm == CF_DICE_FX && (song.g[G_RSIZE] != rs || TSEL->p[P_REV] != rv);
            }
            press(B_OCTDN); frame();
            ok &= okk && !ui.confirm && song.g[G_RSIZE] == rs && TSEL->p[P_REV] == rv && msg_is("FX BACK") && !fx_lock;
            bpm = song.g[G_BPM];
            btn_down(B_GLO); frame(); turn(EN_SELECT, 3); frame(); btn_up(B_GLO); frames(4);
            ok &= song.g[G_BPM] != bpm;
            song.g[G_BPM] = bpm;
            bad += check("0.6.67: ENV + SELECT its pages; SEQ / LFO / FX + SELECT their dice (CANCEL back, OK kept); GLO + SELECT the tempo", ok);
            fm1_ms += 1200u;
        }
        {   /* (0.6.7) turned in the press's own frame, or before HOLD: the map never comes while it is held; held again
             * afterwards: the layer as always */
            static const uint8_t LB[5] = {B_FX, B_GLO, B_SCL, B_EDIT, B_LFO};
            uint32_t q, f2, mx;
            ok = 1;
            for (q = 0; q < 5u; q++) {
                ui_power_on(); go_home(); frames(64);
                btn_down(LB[q]); turn(EN_PRESET, 1); frame();
                for (f2 = 0, mx = 0; f2 < 120u; f2++) { frame(); mx |= ui.layer; }
                btn_up(LB[q]); frames(100);
                ok &= !mx && !ui.layer && !ui.home;
                btn_down(LB[q]); frames(800);
                ok &= ui.layer != 0;
                btn_up(LB[q]); frames(200);
            }
            bad += check("  0.6.7: PRESETS turned as the button goes down: no map at HOLD while held; held again: the layer", ok);
        }
    }
    /* GLO: mutes latch (lit = sounding), SOLO while held, UNMUTE ALL, TAP, levels, OCT- */
    ui_power_on();
    undo_ok = 0;                                        /* (G4 UNDO: dark with nothing to undo) */
    usb.config = 1; mo = mo_w;
    lay_combo(B_GLO, black(1));
    ok = trk[1].p[P_MUTE] == 1 && ui.layer == LAYER_GLO && !gates() && mo_w == mo;
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a & b2) >> black(0)) & 1u && !(((a | b2) >> black(1)) & 1u);           /* T1 sounding lit, T2 muted dark */
    ok &= ((a ^ b2) >> white(0)) & 1u && ((a ^ b2) >> white(7)) & 1u && !(((a | b2) >> white(8)) & 1u);
    key_up(black(1)); btn_up(B_GLO); frame();
    bad += check("GLO + black key 2: T2 MUTE latched (SET), silent, no MIDI; LEDs: sounding lit, muted dark", ok &&
                 trk[1].p[P_MUTE] == 1 && !ui.layer);
    lay_combo(B_GLO, black(6)); key_up(black(6)); frame();       /* black keys 7..10: the DRUM group mutes */
    lay_combo(B_GLO, black(8)); key_up(black(8));
    a = leds_at(0); b2 = leds_at(250);
    ok = dx_mute == (DXG_KICK | DXG_HAT) && ((a & b2) >> black(7)) & 1u && !(((a | b2) >> black(6)) & 1u) &&
         !(((a | b2) >> black(8)) & 1u) && trk[1].p[P_MUTE] == 1;
    btn_up(B_GLO); frame();
    lay_combo(B_GLO, black(8)); key_up(black(8)); btn_up(B_GLO); frame();
    ok &= dx_mute == DXG_KICK;
    lay_combo(B_GLO, white(6)); key_up(white(6)); btn_up(B_GLO); frame();
    ok &= dx_mute == 0;
    bad += check("GLO + black keys 7..10: KICK SNARE HAT PERC group mutes latched (lit = sounding), E4 clears them", ok);
    lay_combo(B_GLO, black(1)); key_up(black(1)); btn_up(B_GLO); frame();   /* (T2 muted again, as the test goes on) */
    lay_combo(B_GLO, black(1)); key_up(black(1)); frame();
    key_down(white(6)); key_up(white(6)); frame();
    lay_combo(B_GLO, black(2)); key_up(black(2)); frame();
    ok = trk[1].p[P_MUTE] == 0 && trk[2].p[P_MUTE] == 1;
    key_down(white(6)); frame(); key_up(white(6)); frame();
    btn_up(B_GLO); frame();
    bad += check("  again: unmuted; E4: UNMUTE ALL", ok && !trk[2].p[P_MUTE] && !trk[1].p[P_MUTE]);
    lay_combo(B_GLO, white(2));
    perf_begin(CTL);
    ok = perf_solo == 4u && ((perf_act >> PF_M1) & 15u) == 0xBu;   /* T3 solo: T1 T2 T4 muted */
    btn_up(B_GLO); frame();
    perf_begin(CTL);
    ok &= perf_solo == 4u && ui.layer == LAYER_GLO;                 /* GLO let go first: the solo stays with its key */
    key_up(white(2)); frame();
    perf_begin(CTL);
    bad += check("GLO + A3 held: SOLO T3 (the others muted), lasts while the key is held, not P_MUTE",
                 ok && !perf_solo && !((perf_act >> PF_M1) & 15u) && !trk[0].p[P_MUTE] && !ui.layer);
    song.g[G_BPM] = 100;
    btn_down(B_GLO); frame();
    for (i = 0; i < 3u; i++) { key_down(white(7)); frame(); key_up(white(7)); frames(480); }
    ok = song.g[G_BPM] == 120;
    btn_up(B_GLO); frame();
    song.g[G_CLOCK] = 1;
    lay_combo(B_GLO, white(7)); key_up(white(7)); frame();
    ok &= msg_is("TAP: CLK IS EXT") && song.g[G_BPM] == 120;
    btn_up(B_GLO); frame(); song.g[G_CLOCK] = 0;
    bad += check("GLO + F4 x3 at 0.5 s: TAP 120 BPM; with CLK EXT: dimmed, it says why", ok);
    {
        int16_t l2 = trk[2].p[P_LEVEL], atk = TSEL->p[P_ATK];
        go_title("ENV");
        btn_down(B_GLO); frame();
        turn(EN_K3, -10);
        ok = trk[2].p[P_LEVEL] == l2 - 10 && TSEL->p[P_ATK] == atk && ui.layer == LAYER_GLO;
        key_down(black(0)); frame(); key_up(black(0)); frame();
        ok &= trk[0].p[P_MUTE] == 1 && song.octave == 0;
        oct_back();
        ok &= trk[2].p[P_LEVEL] == l2 - 10 && trk[0].p[P_MUTE] == 1 && song.octave == 0 && ui.layer == LAYER_GLO;
        btn_up(B_GLO); frame();
        trk[0].p[P_MUTE] = 0;
        trk[2].p[P_LEVEL] = l2;
        bad += check("GLO KNOB 3: T3 LEVEL from any page; OCT-: nothing put back, no octave", ok &&
                     str_eq(cur_page()->title, "ENV"));
    }
    song.playing = 1; transport_req = 0;
    btn_down(B_GLO); frame(); press(B_PLAY);
    ok = transport_req == 3u;
    events_block(CTL);
    ok &= song.playing && transport_req == 0u;
    btn_up(B_GLO); frame();
    stop_transport();
    btn_down(B_GLO); frame(); press(B_PLAY);
    ok &= transport_req == 1u;
    btn_up(B_GLO); frame(); transport_req = 0;
    bad += check("GLO + PLAY: RESTART playing (from the top, not stopped); stopped: PLAY", ok);
    usb.config = 0;

    /* SCL: a key is ROOT, KNOB 2 the scale, the LEDs, OCT- */
    ui_power_on();
    TSEL->p[P_ROOT] = 0; TSEL->p[P_SCALE] = 1;                     /* C MAJ */
    lay_combo(B_SCL, black(4));                                    /* D#4 */
    ok = TSEL->p[P_ROOT] == 3 && ui.layer == LAYER_SCL && !gates();
    key_up(black(4)); frame();
    key_down(white(1)); key_up(white(1)); frame();                  /* G3 */
    ok &= TSEL->p[P_ROOT] == 7;
    turn(EN_K2, 1);
    ok &= TSEL->p[P_SCALE] == 2;
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a & b2) >> white(1)) & 1u && ((a & b2) >> white(8)) & 1u;   /* G3 G4: the root, lit */
    ok &= ((a ^ b2) >> white(2)) & 1u && ((a ^ b2) >> black(4)) & 1u;   /* A, A# (G minor): blink */
    ok &= !(((a | b2) >> black(0)) & 1u);                               /* F#: not in G minor, dark */
    btn_up(B_SCL); frame();
    bad += check("SCL + D#4, then G3: ROOT D#, G (latched); KNOB 2: SCL; LEDs: root lit, the scale breathes", ok &&
                 TSEL->p[P_ROOT] == 7 && TSEL->p[P_SCALE] == 2 && ui.home);
    btn_down(B_SCL); frame(); key_down(white(4)); key_up(white(4)); frame(); turn(EN_K2, 3);
    ok = TSEL->p[P_ROOT] == 0 && TSEL->p[P_SCALE] == 5;
    oct_back();
    btn_up(B_SCL); frame();
    bad += check("  OCT- in SCL: nothing put back (TONIC)", ok && TSEL->p[P_ROOT] == 0 && TSEL->p[P_SCALE] == 5);

    /* EDIT (JIANT): F3 INIT, G3 RECALL (the section's stored sound), each with its dialog; KNOB 1..4 EDIT's four */
    ui_power_on();
    set_engine_of(TSEL, ENGI_DRUM); go_home(); frame();
    my_steps(TSEL);
    before = *TSEL;
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    live.cur = -1;
    lay_combo(B_EDIT, white(1)); key_up(white(1)); btn_up(B_EDIT); frame();
    ok = !ui.confirm && msg_is("NOTHING SAVED");
    btn_down(B_EDIT); frame(); turn(EN_K1, 5);
    ok &= TSEL->p[P_E0] > before.p[P_E0];               /* KNOB 1: EDIT 1's first (DRUM: MRPH) */
    btn_up(B_EDIT); frame();
    bad += check("EDIT: G3 RECALL with no section stored: NOTHING SAVED; KNOB 1 the EDIT page's first (MRPH)", ok);
    project_save(0);                                    /* the safe state: the section, stored (the music's slot) */
    ok = 1;
    TSEL->p[P_E0] = 3; TSEL->p[P_E2] = 120;              /* the sound and the kit wander off */
    dx_kit[1].a[DXP_PITCH] = 99;
    lay_combo(B_EDIT, white(1));
    ok &= ui.confirm == CF_RECALL_SOUND;
    key_up(white(1)); btn_up(B_EDIT); frame();
    press(B_OCTUP);
    ok &= !ui.confirm && msg_is("SOUND RECALLED") && TSEL->p[P_E0] == before.p[P_E0] + 5 &&
          TSEL->p[P_E2] == before.p[P_E2] && dx_kit[1].a[DXP_PITCH] == DX_KIT_DEF[1].a[DXP_PITCH] &&
          !memcmp(TSEL->step, before.step, sizeof before.step) && TSEL->eng_req == ENGI_DRUM;
    bad += check("  G3 RECALL (dialog, OCT+): the sound and the DRUM-X kit as the section stored them; the steps stay", ok);
    TSEL->p[P_E0] = 3;
    dx_kit[1].a[DXP_PITCH] = 99;
    btn_down(B_EDIT); frame(); key_down(white(0)); frame();
    ok = ui.confirm == CF_INIT_SOUND;
    frame();
    ok &= !ui.layer;
    key_up(white(0)); btn_up(B_EDIT); frame();
    ok &= ui.home && ui.confirm == CF_INIT_SOUND;
    press(B_OCTUP);
    bad += check("  F3 INIT (dialog, closes the layer, no tap; OCT+): the engine's first preset, the factory kit", ok &&
                 !ui.confirm && msg_is("SOUND INIT") && TSEL->p[P_E0] == ENG_DRUM.presets[0].e[0] &&
                 dx_kit[1].a[DXP_PITCH] == DX_KIT_DEF[1].a[DXP_PITCH] && !memcmp(TSEL->step, before.step, sizeof before.step));
    set_engine_of(TSEL, 0);
    TSEL->p[P_E1] = (int16_t)(TSEL->p[P_E1] + 7);
    btn_down(B_EDIT); frame(); key_down(white(0)); frame(); key_up(white(0)); btn_up(B_EDIT); frame();
    press(B_OCTUP);
    bad += check("  INIT on a synth: the engine's own defaults (0.6.49), dry", msg_is("SOUND INIT") &&
                 TSEL->p[P_E1] == ENGINES[0]->edit[1].def && TSEL->p[P_E4] == ENGINES[0]->edit[4].def && !TSEL->p[P_REV]);
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    live.cur = -1;

    /* no layer in the menu or a dialog: GLO + a key plays */
    ui_power_on(); hold(B_HOME);
    btn_down(B_GLO); frame(); key_down(white(3)); frame();
    ok = !ui.layer && gates() > 0 && !kb_layer;
    key_up(white(3)); btn_up(B_GLO); frame();
    bad += check("in the menu: no GLO layer, GLO + a key plays it; GLO let go: no page", ok && ui.menu == 1);
    ui_power_on(); ui.confirm = CF_CLEAR_SEQ;
    btn_down(B_SCL); frames(512);
    ok = !ui.layer;
    btn_up(B_SCL); frame();
    bad += check("in a dialog: SCL held shows no map and taps nothing", ok && ui.confirm == CF_CLEAR_SEQ);
    /* one layer at a time: a second layer button is ignored, its keys stay notes after */
    ui_power_on();
    btn_down(B_GLO); frame(); btn_down(B_EDIT); frame(); key_down(white(1)); frame();
    ok = ui.layer == LAYER_GLO && TSEL->eng_req == trk[0].eng_req && perf_solo == 2u;
    key_up(white(1)); btn_up(B_GLO); frame();
    key_down(white(1)); frame();
    ok &= gates() > 0 && !ui.layer;
    key_up(white(1)); btn_up(B_EDIT); frame();
    bad += check("GLO then EDIT: GLO's layer; EDIT ignored (no layer, no page, its keys notes)", ok && ui.home);
    return bad;
}

/* bug fixes (UI): one regression each */
static int test_bughunt_ui(void)
{
    int bad = 0, ok;
    /* 1: REC in NAME, the menu or a dialog: nothing (PLAY cannot stop the transport there) */
    ui_power_on();
    go_page(GR_USER); ui.uslot = 3;
    press(B_OCTUP);                                     /* NAME opens */
    press(B_REC);
    bad += check("REC in NAME does nothing (no arm, no transport start); NAME stays",
                 name_on() && !song.rec && !transport_req && !song.playing);
    ui_power_on(); hold(B_HOME); press(B_REC); press(B_PLAY);
    bad += check("REC in the menu does nothing (then PLAY: nothing either)", ui.menu && !song.rec && !transport_req);
    ui_power_on();
    up_ui(2, 5);
    go_page(GR_USER); ui.uslot = 5; press(B_OCTUP);
    press(B_REC);
    ok = ui.confirm == CF_OVR_USER && !song.rec && !transport_req;
    press(B_OCTUP);
    bad += check("REC in the OVERWRITE? dialog does nothing; OCT+ then opens NAME", ok && name_on());
    {   /* the ARP button flashes on the beat while an ARP plays (the bar's first beat longer); dark otherwise */
        uint32_t q, b, r[5];
        ui_power_on(); frame();
        b = beat_samples();
        r[0] = arp_led();                               /* no ARP playing */
        trk[2].p[P_AMODE] = 1; trk[2].nheld = 1; trk[2].held[0] = 60;
        beat_n = 1; beat_pos = 0; r[1] = arp_led();
        beat_pos = b / 3u; r[2] = arp_led();
        beat_n = 0; r[3] = arp_led();
        beat_pos = b * 3u / 4u; r[4] = arp_led();
        bad += check("ARP LED: none without an ARP; flashes on the beat, the bar's first beat longer",
                     r[0] == 2u && r[1] == 1u && r[2] == 0u && r[3] == 1u && r[4] == 0u);
        for (q = 0; q < 4u * b / 128u + 4u; q++) events_block(128);
        bad += check("  the beat counter runs (a bar of blocks wraps the beat of the bar)", beat_n < 4u && beat_pos < b);
        trk[2].p[P_AMODE] = 0; trk[2].nheld = 0;
    }
    /* 3: USER ERASE asks first (ERASE U02?): OCT- keeps the preset, OCT+ erases it */
    ui_power_on();
    go_page(GR_USER); ui.uslot = 1;
    up_ui(2, 1);
    turn(EN_K1 + 2, 1);                                 /* KNOB 3: ERASE picked */
    press(B_OCTUP);
    ok = ui.confirm == CF_ERASE_USER && ui.confirm_trk == 1u && up_used(1);
    {
        char a[24], b[24];
        confirm_text(a, b);
        ok &= str_eq(a, "ERASE U02?") && b[0];
    }
    press(B_OCTDN);
    ok &= !ui.confirm && up_used(1);
    turn(EN_K1 + 2, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("USER ERASE: the ERASE U02? dialog; OCT- keeps the preset, OCT+ erases it",
                 ok && !ui.confirm && !up_used(1));
    turn(EN_K1 + 2, 1); press(B_OCTUP);
    bad += check("  an empty slot: no dialog, EMPTY SLOT", !ui.confirm && msg_is("EMPTY SLOT"));
    {   /* 4: CHANCE is not STEP: no grid, no key entry, no EDIT clear (its knobs only) */
        uint32_t before;
        step_t s0;
        ui_power_on();
        track_select(3); frame();                       /* T4 DRUM */
        go_page(GR_CHANCE); frame();
        before = lane_steps(TSEL, ui.lane);
        tap_key(white(2));
        bad += check("CHANCE on a DRUM track: no grid (title, LEDs), a white key toggles no hit",
                     drum_track(TSEL) && !grid_on() && !keys_mode() && lane_steps(TSEL, ui.lane) == before);
        ui_power_on(); go_page(GR_CHANCE); frame();
        my_steps(TSEL);
        s0 = TSEL->step[0];
        tap_key(white(5)); frame();
        ok = !memcmp(&s0, &TSEL->step[0], sizeof s0) && ui.cursor == 0u;
        press(B_EDIT);
        ok &= !memcmp(&s0, &TSEL->step[0], sizeof s0) && !msg_is("STEP CLEARED");
        ui_power_on(); go_page(GR_CHANCE); frame();
        s0 = TSEL->step[0];
        turn(EN_K2, -10);                               /* (100 % by default) */
        bad += check("CHANCE on a melodic track: a key writes no step, EDIT clears none; KNOB 2 the chance",
                     ok && step_chance(&TSEL->step[0]) != step_chance(&s0));
        ui_power_on(); go_page(GR_ROLL); frame(); song.rec = 1;   /* (#133: armed, stopped) */
        tap_key(white(5)); frame();
        bad += check("  STEP still: armed, a key writes the cursor step", step_on(&TSEL->step[0]) || TSEL->step[0].n);
        song.rec = 0;
    }
    {   /* 5: ALGORITHM does nothing while a layer's button is held (as PRESETS); after it, the track again */
        int16_t r0;
        ui_power_on();
        r0 = trk[0].p[P_ROOT];
        btn_down(B_SCL); frames(500);
        turn(EN_ALGO, 1);
        ok = ui.layer == LAYER_SCL && song.sel == 0u;
        key_down(white(1)); frame(); key_up(white(1)); frame();   /* ROOT on T1 */
        ok &= trk[0].p[P_ROOT] != r0;
        oct_back();
        ok &= trk[0].p[P_ROOT] != r0;                   /* (OCT-: nothing put back) */
        btn_up(B_SCL); frame();
        btn_down(B_EDIT); turn(EN_ALGO, 1);             /* armed, not open yet: ignored too */
        ok &= song.sel == 0u;
        btn_up(B_EDIT); frame(); go_home(); frame();
        turn(EN_ALGO, 1);
        bad += check("ALGORITHM ignored with a layer's button held; then T2",
                     ok && song.sel == 1u);
    }
    /* 6: TOOLS: each column its own ready check; nothing to do says so (not STOP TO EDIT) */
    ui_power_on();
    go_title("TOOLS"); frame();
    chain_defaults(&chain_config);                      /* no song rows */
    tools_pick(3);                                      /* DELETE ROW */
    press(B_OCTUP);
    ok = !ui.confirm && msg_is("NOTHING TO DELETE") && text_w(&AF_S, ui.msg) <= 236 - 106;   /* (fits the header) */
    tools_pick(4);                                      /* CLEAR SONG */
    press(B_OCTUP);
    ok &= !ui.confirm && msg_is("NOTHING TO CLEAR");
    track_defaults_steps(TSEL);
    chain_config.count = 1; chain_config.row[0].slot = 0; chain_config.row[0].bars = 1;
    tools_pick(1);                                      /* CLEAR PAT on an empty track, the song with a row */
    ok &= !act_ready();
    press(B_OCTUP);
    ok &= !ui.confirm && msg_is("NOTHING TO CLEAR");
    tools_pick(4);
    ok &= act_ready();
    press(B_OCTUP);
    bad += check("TOOLS: DELETE ROW / CLEAR SONG / CLEAR PAT with nothing there: NOTHING TO ..; a song: its dialog",
                 ok && ui.confirm == CF_CLEAR_SONG);
    /* 7: REC works in the layers, as PLAY: it arms the track (PLAY starts); the layer stays, no page */
    ok = 1;
    {
        static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
        uint32_t i;
        for (i = 0; i < 4u; i++) {
            ui_power_on();
            btn_down(LB[i]); frame();
            press(B_REC);
            ok &= song.rec == 1u && transport_req == 1u && ui.layer != 0u;
            btn_up(LB[i]); frame();
            ok &= ui.home;
        }
    }
    bad += check("REC in every layer: arms the track and starts PLAY; the layer stays, no page", ok);
    /* 8: the OCT LEDs dark in the SET layers (OCT± do nothing there since TONIC); FX (HOLD): the octave as usual */
    ok = 1;
    {
        static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
        uint32_t i;
        for (i = 0; i < 3u; i++) {
            ui_power_on();
            song.octave = 1;                            /* (outside: OCT+ lit) */
            btn_down(LB[i]); frames(500);
            ok &= layer_set_open() && oct_leds_seen(1) == 0u && oct_leds_seen(0) == 0u;
            btn_up(LB[i]); frame();
            ok &= oct_leds() == 2u;
        }
        ui_power_on();
        btn_down(B_FX); frames(500);
        ok &= ui.layer == LAYER_FX && oct_leds() == 0u;
        btn_up(B_FX); frame();
    }
    bad += check("SET layers: the OCT LEDs dark; after: the octave; FX: the octave", ok);
    /* 9: no "HOLD [EDIT] QUICK" over NAME (EDIT tapped on USER / PROJECT renames) */
    ui_power_on();
    layer_seen = 0;
    up_ui(2, 0);
    go_page(GR_USER); ui.uslot = 0; frame();
    ui.msg_t = 0;
    press(B_EDIT);
    ok = name_on() && !msg_is("HOLD [EDIT] QUICK");
    ui_power_on();
    layer_seen = 0;
    press(B_EDIT);
    bad += check("EDIT tap = RENAME on USER: no layer hint over NAME (elsewhere the hint as before)",
                 ok && msg_is("HOLD [EDIT] QUICK"));
    return bad;
}

/* (JIANT) REC held, then a page button: that part's automation goes: EDIT (the engine's), FX (the sends and the
 * punch-in lane), GLO (every track's LEVEL), ENV; the others stay, REC does not arm, no page opens */
static int test_rec_combos(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    t = TSEL;
    go_home(); frame();
    motion_set_event(t, 2, P_E0, 40);
    motion_set_lock(t, 5, P_E0 + 1, 20);
    motion_set_event(t, 3, P_ATK, 10);
    motion_set_event(t, 4, P_DIST, 50);
    motion_set_event(&trk[(song.sel + 1u) % NTRK], 1, P_LEVEL, 30);
    pfx_lane[0] = 0x11;
    btn_down(B_REC); frame();
    btn_down(B_EDIT); frame(); btn_up(B_EDIT); frame();
    btn_up(B_REC); frames(20);
    ok = motion_find(t, 2, P_E0) < 0 && motion_find(t, 5, P_E0 + 1) < 0 && motion_find(t, 3, P_ATK) >= 0 &&
         motion_find(t, 4, P_DIST) >= 0 && !song.rec && ui.home && msg_is("EDIT AUTOMATION CLEARED");
    bad += check("REC + EDIT: the engine's automation and locks go (ENV FX stay), REC not armed, no page", ok);
    btn_down(B_REC); frame();
    btn_down(B_FX); frame(); btn_up(B_FX); frame();
    btn_up(B_REC); frames(20);
    ok = motion_find(t, 4, P_DIST) < 0 && !pfx_lane[0] && motion_find(t, 3, P_ATK) >= 0 && !song.rec && ui.home;
    bad += check("  REC + FX (REC first): the FX automation and the punch-in lane go, ENV stays, REC not armed", ok);
    btn_down(B_REC); frame();
    btn_down(B_GLO); frame(); btn_up(B_GLO); frame();
    btn_up(B_REC); frames(20);
    ok = motion_find(&trk[(song.sel + 1u) % NTRK], 1, P_LEVEL) < 0 && motion_find(t, 3, P_ATK) >= 0 && ui.home;
    bad += check("  REC + GLO: every track's LEVEL automation goes", ok);
    btn_down(B_REC); frame();
    btn_down(B_ENV); frame(); btn_up(B_ENV); frame();
    btn_up(B_REC); frames(20);
    ok = motion_find(t, 3, P_ATK) < 0 && ui.home;
    btn_down(B_REC); frame();
    btn_down(B_ENV); frame(); btn_up(B_ENV); frame();
    btn_up(B_REC); frames(20);
    bad += check("  REC + ENV: the ENV automation goes (no page); again: NO ENV AUTOMATION", ok && msg_is("NO ENV AUTOMATION"));
    return bad;
}

/* MACRO (JIANT): LFO held is the macros' layer, KNOB 1..4 M1..M4; a MOD slot with SRC M1..M4 moves its destination
 * by the macro (an engine parameter, the master's CLIP); the project keeps them */
/* (JIANT 0.6.10, from Felucca 1.5) FM6's operator pages, EDIT > OPERATOR, OP ENV, OPERATOR 2 (ui_fm6op.c): KNOB 1 the
 * operator, KNOB 2..4 a byte of the track's patch each (OP ENV: KNOB 2 the stage, 3 / 4 its rate and level); a detent
 * writes that byte only, within 0..its highest, the patch the track's own (SLOT OWN); an edit keeps the output trim */
static uint32_t fop_diff(const uint8_t *a, const uint8_t *b, uint32_t *at)
{
    uint32_t i, n = 0;
    for (i = 0; i < FP_SIZE; i++)
        if (a[i] != b[i]) { n++; *at = i; }
    return n;
}
static void fop_pick(uint32_t opn) { turn(EN_K1, -9); turn(EN_K1, (int32_t)opn - 1); }
static int test_fm6_ops(void)
{
    static const struct { const char *page; uint32_t k, off; } F[] = {
        {"OPERATOR", EN_K2, FP_FC}, {"OPERATOR", EN_K3, FP_FF}, {"OPERATOR", EN_K4, FP_OL},
        {"OPERATOR 2", EN_K2, FP_MODE}, {"OPERATOR 2", EN_K3, FP_DET}, {"OPERATOR 2", EN_K4, FP_KVS},
        {"OP ENV", EN_K3, FP_R1}, {"OP ENV", EN_K4, FP_L1}};
    int bad = 0, ok = 1;
    uint32_t opn, f, st, at = 0, n = 0, tr;
    uint8_t v[FP_SIZE + 1u];
    ui_power_on();
    set_engine_of(TSEL, ENGI_FM6);
    TSEL->engine = TSEL->eng_req;
    tr = song.sel;
    go_title("OPERATOR"); frame();
    ok = str_eq(cur_page()->title, "OPERATOR");
    set_engine_of(TSEL, 0); TSEL->engine = TSEL->eng_req;
    ok &= !page_visible((uint32_t)(cur_page() - PAGES));
    set_engine_of(TSEL, ENGI_FM6); TSEL->engine = TSEL->eng_req;
    bad += check("FM6 ops: OPERATOR / OP ENV / OPERATOR 2 on an FM6 track only", ok);
    ok = 1;
    for (opn = 1; opn <= 6u; opn++)
        for (f = 0; f < NELEM(F); f++)
            for (st = 0; st < (F[f].off == FP_R1 || F[f].off == FP_L1 ? 4u : 1u); st++) {
                uint32_t b = (6u - opn) * FP_OP + F[f].off + st, max = fm6_max(b);
                int32_t d;
                go_title(F[f].page); frame();
                fop_pick(opn);
                if (F[f].off == FP_R1 || F[f].off == FP_L1) { turn(EN_K2, -4); turn(EN_K2, (int32_t)st); }
                memcpy(v, fm6_patch[tr], FP_SIZE);
                d = v[b] < max ? 1 : -1;
                frames(1600);
                turn(F[f].k, d);
                if (fop_diff(v, fm6_patch[tr], &at) != 1 || at != b || fm6_patch[tr][b] != (uint8_t)(v[b] + d) ||
                    fm6_slot[tr] != FM6_OWN || TSEL->p[P_E7] != FM6_OWN) {
                    printf("ui: FM6 ops: OP %u %s KNOB %u stage %u: byte %u, not %u\n", opn, F[f].page, F[f].k - EN_K1 + 1u,
                           st + 1u, at, b);
                    ok = 0;
                }
                turn(F[f].k, 200);                       /* the range: never past the byte's highest, nor below 0 */
                ok &= fm6_patch[tr][b] == max;
                turn(F[f].k, -200);
                ok &= fm6_patch[tr][b] == 0u;
                n++;
            }
    bad += check("FM6 ops: 6 operators x 14 bytes, each knob writes its byte only (+-1, 0..its highest), SLOT OWN", ok && n == 84u);
    {   /* (0.6.77) the graphs redraw on an edit: OP ENV's curve with KNOB 3 / 4, the stage, the operator (track
         * pages: the patch was in the redraw's signature on the engine pages only) */
        uint32_t s0;
        go_title("OP ENV"); frame(); fop_pick(2); turn(EN_K2, -4); frame();
        s0 = graph_signature(); turn(EN_K3, fm6_patch[tr][4u * FP_OP + FP_R1] ? -1 : 1);
        ok = graph_signature() != s0;
        frame(); s0 = graph_signature(); turn(EN_K4, fm6_patch[tr][4u * FP_OP + FP_L1] ? -1 : 1);
        ok &= graph_signature() != s0;
        frame(); s0 = graph_signature(); turn(EN_K2, 1);
        ok &= graph_signature() != s0;
        frame(); s0 = graph_signature(); turn(EN_K1, 1);
        ok &= graph_signature() != s0;
        go_title("OPERATOR"); frame(); s0 = graph_signature(); turn(EN_K4, fm6_patch[tr][3u * FP_OP + FP_OL] ? -1 : 1);
        ok &= graph_signature() != s0;
        bad += check("FM6 ops: OP ENV's curve and OPERATOR's chart redraw on every knob (rate, level, stage, operator)", ok);
    }
    fm6_trim[tr] = 40; fm6_tgen[tr] = fm6_pgen[tr];     /* (a trimmed patch, then an edit: the trim stays) */
    go_title("OPERATOR"); frame(); fop_pick(1); turn(EN_K3, 1);
    fm6_block(TSEL);
    ok = fm6_trim[tr] == 40;
    fm6_set_patch(tr, fm6_patch[tr]);                   /* (a patch put in from elsewhere: from none again) */
    fm6_block(TSEL);
    bad += check("FM6 ops: an operator edit keeps the patch's output trim; a new patch starts from none", ok && fm6_trim[tr] == 0);
    ui_power_on();
    return bad;
}
static int test_macro_layer(void)
{
    int bad = 0, ok;
    track_t *t;
    project_t a, b;
    project_store_t st;
    ui_power_on();
    t = TSEL;
    memset(macro_v, 0, sizeof macro_v);
    go_home(); frame();
    btn_down(B_LFO); frames(800);
    ok = ui.layer == LAYER_MACRO;
    turn(EN_K1, 10); frame();
    turn(EN_K4, 3); frame();
    ok &= macro_v[0] == 10 && macro_v[3] == 3 && macro_v[1] == 0;
    btn_up(B_LFO); frame();
    bad += check("MACRO: LFO held opens it, KNOB 1 / 4 turn M1 / M4", ok && !ui.layer);
    t->p[P_M1SRC] = MS_M1; t->p[P_M1DST] = MD_E1; t->p[P_M1AMT] = 64;
    macro_v[0] = 127;
    {
        const param_desc_t *pd = &ENGINES[t->engine]->edit[0];
        int16_t keep = t->p[P_E0] = (int16_t)pd->min;
        ok = mod_begin(t) && t->p[P_E0] >= pd->max - 1;
        mod_end(t);
        ok &= t->p[P_E0] == keep;
        macro_v[0] = 0;
        ok &= !mod_begin(t) || t->p[P_E0] == keep;
        mod_end(t);
    }
    bad += check("  M1 -> E1 AMT 64: M1 full takes the parameter to its top for the block, then it is back", ok);
    t->p[P_M1DST] = MD_CLIP; song.g[G_CLIP] = 10;
    macro_v[0] = 127; macro_master();
    ok = clip_eff >= 99 && song.g[G_CLIP] == 10;
    macro_v[0] = 0; macro_master();
    bad += check("  M1 -> CLIP: the master's clip rises with the macro, the stored CLIP untouched", ok && clip_eff == 10);
    t->p[P_M1SRC] = 0; song.g[G_CLIP] = 0;
    macro_v[0] = 5; macro_v[1] = 66; macro_v[2] = 127; macro_v[3] = 0;
    project_capture(&a);
    ok = proj_pack(&st, &a) && proj_import(&b, &st, sizeof st) && !memcmp(b.macro, macro_v, 4);
    memset(macro_v, 0, sizeof macro_v);
    (void)project_restore_runtime(&b);
    bad += check("  the project keeps M1..M4 (saved, loaded back)", ok && macro_v[1] == 66 && macro_v[2] == 127);
    ui_power_on();                                     /* SONG KNOB 4: SCENE, the state now into the row */
    chain_config.count = 2; chain_config.row[0] = (chain_row_t){0, 1}; chain_config.row[1] = (chain_row_t){1, 1};
    memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
    go_title("ARRANGE"); ui.song_row = 1; frame();
    trk[3].p[P_MUTE] = 1; macro_v[1] = 77;
    turn(EN_K4, 1); frame();
    ok = song_idx.scene[song_cur][1].on && ((song_idx.scene[song_cur][1].mute >> 3) & 1u) &&
         song_idx.scene[song_cur][1].mac[1] == 77 && !song_idx.scene[song_cur][0].on && msg_is("SCENE STORED");
    turn(EN_K4, -1); frame();
    ok &= !song_idx.scene[song_cur][1].on;
    trk[3].p[P_MUTE] = 0; chain_config.count = 0;
    bad += check("SONG KNOB 4 SCENE: right stores the mutes and macros into the row, left takes it away", ok);
    ui_power_on();                                     /* (JIANT) LFO > MSEQ: KNOB 1 the step, 2 its level, 3 LEN */
    ms_ui_step = 0;
    go_title("MSEQ"); frame();
    turn(EN_K1, 3); frame();
    turn(EN_K2, 10); frame();
    turn(EN_K3, -4); frame();
    bad += check("MSEQ: KNOB 1 picks step 4, KNOB 2 its level, KNOB 3 LEN", ms_ui_step == 3u && TSEL->p[P_MS0 + 3] == 10 &&
                 TSEL->p[P_MS0] == 0 && TSEL->p[P_MSLEN] == 12);
    return bad;
}

/* SHIFT (JIANT, in the chord keys' place): SEQ's SHIFT page (OFS PIT, the pattern graph); a sound load keeps them;
 * the SCL layer's KNOB 3 / 4 are OFS / PIT */
static int test_shift_page(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    t = TSEL;
    go_home(); frame();
    go_title("SHIFT"); frame();
    ok = str_eq(cur_page()->title, "SHIFT") && cur_page()->fam == FAM_SEQ && cur_page()->id[0] == P_SOFS &&
         cur_page()->id[1] == P_POFS;
    turn(EN_K1, 3); frame();
    turn(EN_K2, -2); frame();
    bad += check("SHIFT page: KNOB 1 OFS +3, KNOB 2 PIT -2", ok && t->p[P_SOFS] == 3 && t->p[P_POFS] == -2);
    apply_preset_to(t, 1);
    bad += check("  a sound load keeps OFS and PIT (sequence settings)", t->p[P_SOFS] == 3 && t->p[P_POFS] == -2);
    ui_power_on();
    t = TSEL;
    go_home(); frame();
    btn_down(B_SCL); frames(800);
    ok = ui.layer == LAYER_SCL;
    turn(EN_K3, 2); frame();
    turn(EN_K4, 5); frame();
    ok &= t->p[P_SOFS] == 2 && t->p[P_POFS] == 5 && t->p[P_QUANT] == 0 && t->p[P_TRANS] == 0;
    oct_back();
    btn_up(B_SCL); frame();
    bad += check("SCL layer: KNOB 3 OFS, KNOB 4 PIT (QNT TRN untouched)", ok && t->p[P_SOFS] == 2 && t->p[P_POFS] == 5);
    return bad;
}

/* the STEP page's piano roll (ui_graph.c graph_roll), read back from the screen: per column the rows whose centre
 * pixel (the bar's middle, x + 6) is a bar colour, against the notes the step sounds (its own; a TIE: the note
 * before's; a REST: none) */
static int pr_is_bar(uint32_t x, uint32_t y)
{
    uint16_t c = swap16(host_screen[y * 240u + x]);
    return c == T_THEME || c == T_ACCENT || c == T_TEXT;
}
static int pr_columns_match(const track_t *t, uint32_t *nbars)
{
    uint32_t i, r, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int ok = 1;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t want = 0, got = 0, s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&t->step[s], j), rr = (int32_t)proll.lo + PR_ROWS - 1 - n;
            if (n >= 0 && rr >= 0 && rr < PR_ROWS) want |= 1u << rr;
        }
        for (r = 0; r < PR_ROWS; r++)
            if (pr_is_bar((uint32_t)(PR_X0 + (int32_t)i * PR_CW + 6), Y_GRAPH + PR_Y0 + r * PR_RH + 2u)) got |= 1u << r;
        if (got != want) {
            printf("ui:   roll column %u: rows %05x drawn, %05x wanted\n", base + i, got, want);
            ok = 0;
        }
        for (r = 0; r < PR_ROWS; r++) *nbars += (got >> r) & 1u;
    }
    return ok;
}
/* 1.2, Discussion #133: step recording on SEQ > STEP. Not armed, the keys only play (being on SEQ writes nothing);
 * REC there arms step recording without starting the transport; armed and stopped a key (keys held together: a
 * chord) writes the cursor step and letting go moves the cursor on one step, wrapping inside LEN; PLAY records live
 * (at the play head, not the cursor). The DRUM grid keeps its own keys (white keys = steps, REC starts PLAY) */
static int test_step_rec(void)
{
    int bad = 0, ok;
    track_t *t;
    step_t before[NSTEP];
    ui_power_on();
    t = TSEL;
    t->p[P_VOICE] = V_POLY;
    track_defaults_steps(t);
    go_page(GR_ROLL);
    cursor_set(3);
    frame();
    memcpy(before, t->step, sizeof before);
    key_down(7); frame();
    ok = kb_on[7] != 0u;                                             /* it sounds */
    key_up(7); frame();
    ok &= !memcmp(before, t->step, sizeof before) && ui.cursor == 3u && !song.rec && !transport_req;
    bad += check("#133 STEP, not armed, stopped: a key sounds, writes nothing, the cursor stays", ok);
    song.playing = 1;
    key_down(7); frame(); key_up(7); frame();
    song.playing = 0;
    bad += check("#133 STEP, not armed, playing: nothing written either", !memcmp(before, t->step, sizeof before) &&
                 ui.cursor == 3u);
    song.rec = 2u;                                                    /* T2 armed, T1 selected */
    key_down(7); frame(); key_up(7); frame();
    bad += check("#133 another track armed: the selected track's STEP writes nothing",
                 !memcmp(before, t->step, sizeof before) && ui.cursor == 3u);
    song.rec = 0;
    press(B_REC);
    bad += check("#133 REC on STEP arms step recording: stopped, KEYS WRITE STEPS",
                 song.rec == 1u && !transport_req && !song.playing && !seq_counting() && msg_is("KEYS WRITE STEPS"));
    key_down(7); frame();
    ok = t->step[3].n == 1u && t->step[3].note[0] == kb_map(t, 7) && t->step[3].time == ST_NOTE && ui.cursor == 3u;
    key_up(7); frame();
    ok &= ui.cursor == 4u && !memcmp(before, t->step, 3u * sizeof before[0]) &&
          !memcmp(before + 4, t->step + 4, (NSTEP - 4u) * sizeof before[0]);
    bad += check("#133 armed, stopped: a key writes its note into the cursor step (only), letting go moves on one", ok);
    key_down(7); frame(); key_down(11); frame(); key_down(14); frame();
    ok = t->step[4].n == 3u && ui.cursor == 4u;
    key_up(7); frame();
    ok &= ui.cursor == 4u;                                            /* (two keys still down: the step stays open) */
    key_up(11); key_up(14); frame();
    ok &= ui.cursor == 5u && t->step[4].note[0] == kb_map(t, 7) && t->step[4].note[1] == kb_map(t, 11) &&
          t->step[4].note[2] == kb_map(t, 14);
    bad += check("#133 keys held together: one chord step (POLY), the cursor moves on once all are up", ok);
    t->p[P_VOICE] = V_MONO;
    key_down(7); frame(); key_down(9); frame(); key_up(7); key_up(9); frame();
    bad += check("#133 MONO: one note, the last key", t->step[5].n == 1u && t->step[5].note[0] == kb_map(t, 9) &&
                 ui.cursor == 6u);
    t->p[P_SLEN] = 8;
    cursor_set(7);
    frame();
    key_down(9); frame(); key_up(9); frame();
    bad += check("#133 the last step of LEN 8: written, the cursor wraps to step 1", t->step[7].n == 1u && ui.cursor == 0u);
    {   /* (0.6.37) a chord held: KNOB 1 its length in steps (TIEs after it), written when let go; ARP pressed while
         * held: arpeggiated, KNOB 1 the steps a note, 2 the order, 3 the octaves */
        uint32_t i;
        t->p[P_VOICE] = 0; t->p[P_SLEN] = 16;
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        cursor_set(0); frame();
        key_down(7); frame(); key_down(11); frame();
        turn(EN_K1, 3);
        ok = ui.ent_len == 4u && msg_is("LEN 4 STEPS") && ui.cursor == 0u;
        key_up(7); key_up(11); frame();
        ok &= t->step[0].n == 2u && t->step[0].time == ST_NOTE && t->step[1].time == ST_TIE && t->step[2].time == ST_TIE &&
              t->step[3].time == ST_TIE && t->step[4].time != ST_TIE && ui.cursor == 4u;
        bad += check("step recording: a chord held + KNOB 1: LEN 4, the chord then three TIEs, the cursor after them", ok);
        key_down(7); frame(); key_down(11); frame(); key_down(14); frame();
        press(B_ARP);
        turn(EN_K1, 1);                                               /* RES 1/8: a note every two steps */
        turn(EN_K3, 1);                                               /* two octaves */
        ok = ui.ent_arp && ui.ent_res == 2u && ui.ent_oct == 2u && !ui.home && cur_page()->graph == GR_ROLL;
        key_up(7); key_up(11); key_up(14); frame();
        ok &= t->step[4].n == 1u && t->step[4].note[0] == kb_map(t, 7) && t->step[5].time == ST_TIE &&
              t->step[6].note[0] == kb_map(t, 11) && t->step[8].note[0] == kb_map(t, 14) &&
              t->step[10].note[0] == kb_map(t, 7) + 12u && t->step[14].note[0] == kb_map(t, 14) + 12u &&
              ui.cursor == (4u + 12u) % 16u;
        bad += check("step recording: ARP while the chord is held: arpeggiated UP, 1/8, two octaves, into the roll", ok);
        cursor_set(0); frame();
        key_down(7); frame(); key_down(11); frame();
        press(B_ARP); turn(EN_K2, 1);                                 /* DOWN */
        key_up(7); key_up(11); frame();
        bad += check("step recording: ARP ORDER DOWN (KNOB 2): the high note first",
                     t->step[0].note[0] == kb_map(t, 11) && t->step[1].note[0] == kb_map(t, 7) && ui.cursor == 2u);
        t->p[P_SLEN] = 8;
    }
    cursor_set(4);                                                    /* (an empty step, away from the play head) */
    step_clear(&t->step[4]);
    frame();
    press(B_PLAY);
    events_block(CTL);
    ok = song.playing && live_rec_sel() && t->seq_idx < 2u;
    key_down(7); frame(); key_up(7); frame();
    ok &= ui.cursor == 4u && !t->step[4].n && !t->step[4].hit && (t->step[0].n || t->step[1].n);
    bad += check("#133 PLAY with the track armed: live recording at the play head (the cursor and its step stay)", ok);
    {   /* (0.6.83) SCL > CHORD TRIAD: live recording writes the key's whole chord, and the keys of its notes light */
        uint32_t i, best = 0, kr = 7, lit;
        t->p[P_VOICE] = V_POLY; t->p[P_CHRD] = CD_TRI;
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        key_down(kr); events_block(CTL); frame();
        for (i = 0; i < NSTEP; i++) if (t->step[i].n > best) best = t->step[i].n;
        lit = key_leds(0);
        ok = best == 3u && kb_sn[kr] == 3u;
        for (i = 0; i < 3u; i++) {                      /* (each chord note's key lit) */
            uint32_t k2, any = 0;
            for (k2 = 0; k2 < 27u; k2++) any |= kb_map(t, k2) == kb_snd[kr][i] && ((lit >> k2) & 1u);
            ok &= any != 0u;
        }
        key_up(kr); events_block(CTL); frame();
        bad += check("SCL > CHORD, live recording: the whole chord into the step; its notes' keys lit while it sounds", ok);
    }
    {   /* (0.6.84) STRUM: recorded whole into the one step where the key went down (its later notes no longer fall into
         * the next step), and the sequencer strums the chord back, low to high */
        uint32_t i, used = 0, kr = 7, b2, first = 0, later = 0;
        t->p[P_CSTR] = 127;                            /* (~63 ms apart) */
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        while (t->seq_pos < step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) * 2u / 5u)
            events_block(CTL);                         /* (just before the middle of a step: a strum would cross it) */
        key_down(kr);
        for (b2 = 0; b2 < 200u; b2++) events_block(CTL);
        key_up(kr); events_block(CTL); frame();
        for (i = 0; i < NSTEP; i++) if (t->step[i].n) used++;
        ok = used == 1u;
        for (i = 0; i < NSTEP; i++) if (t->step[i].n) ok &= t->step[i].n == 3u;
        bad += check("STRUM, live recording: the chord whole into one step, not spread over two", ok);
        song.rec = 0;                                  /* (playing it back: the step strummed) */
        while (t->seq_idx != 0u) events_block(CTL);
        while (t->seq_idx == 0u) events_block(CTL);
        for (i = 0; i < NSTEP && !t->step[i].n; i++) ;
        while (t->seq_idx != i) events_block(CTL);
        for (b2 = 0; b2 < NVOICE; b2++) first += t->v[b2].active && t->v[b2].gate;
        for (b2 = 0; b2 < 200u; b2++) events_block(CTL);
        for (b2 = 0; b2 < NVOICE; b2++) later += t->v[b2].active && t->v[b2].gate;
        ok = first == 1u && later == 3u;
        bad += check("STRUM, playing back: the step's chord comes in one note after another (1, then all 3)", ok);
        t->p[P_CSTR] = 0;
        song.rec = 1;
    }
    stop_transport();
    {   /* (0.6.83) and step recording (armed, stopped) */
        uint32_t i;
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        cursor_set(2); frame();
        key_down(7); events_block(CTL); frame(); key_up(7); events_block(CTL); frame();
        ok = t->step[2].n == 3u && t->step[2].note[0] == kb_map(t, 7) && ui.cursor == 3u;
        t->p[P_CHRD] = CD_OFF;
        bad += check("SCL > CHORD, step recording: the key's chord (3 notes) into the cursor step, not its note alone", ok);
    }
    cursor_set(4); frame();
    press(B_REC);                                                     /* disarm: the keys only play again */
    memcpy(before, t->step, sizeof before);
    key_down(7); frame(); key_up(7); frame();
    bad += check("#133 REC again disarms: the keys only play, nothing written",
                 !song.rec && !memcmp(before, t->step, sizeof before) && ui.cursor == 4u);
    /* the DRUM grid as before: white keys are the steps (armed or not), REC from stop starts PLAY */
    ui_power_on();
    t = TSEL;
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    go_page(GR_ROLL);
    frame();
    tap_key(key_at(0, 4));
    ok = grid_on() && t->step[4].hit != 0u;
    press(B_REC);
    ok &= song.rec == 1u && transport_req == 1u && msg_is("LANE KEYS RECORD");
    transport_req = 0;                                                /* (armed, still stopped) */
    tap_key(key_at(0, 6));
    tap_key(key_at(0, 4));
    ok &= t->step[6].hit != 0u && !t->step[4].hit && ui.cursor == 4u;
    bad += check("#133 the DRUM grid unchanged: white keys toggle steps armed or not, REC from stop starts PLAY", ok);
    song.rec = 0;
    return bad;
}

static int test_piano_roll(void)
{
    int bad = 0, ok;
    uint32_t i, nb = 0, lo0, steps = 0;
    track_t *t;
    ui_power_on();
    t = TSEL;
    load_pat16(t, PATTERNS[0].note, PATTERNS[0].flags);               /* ACID: A2..A3, ties, accents, slides */
    t->p[P_ROOT] = 9; t->p[P_SCALE] = 2;
    go_page(GR_ROLL); ui.cursor = 0; ui.bank = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok = pr_columns_match(t, &nb);
    bad += check("piano roll: ACID's bars drawn at their notes' rows, ties carried, rests empty", ok && nb >= 10u);
    bad += check("  the view holds the page's notes (A2..A3 in 21 rows), MONO gray", proll.lo <= 45 && proll.lo + PR_ROWS - 1 >= 57 &&
                 screen_gray());
    t->p[P_VOICE] = V_POLY;                                           /* chords: up to 4 bars in a column */
    for (i = 0; i < 16u; i += 4u) {
        step_t *s = &t->step[i];
        s->time = ST_NOTE; s->n = 4; s->note[0] = 57; s->note[1] = 60; s->note[2] = 64; s->note[3] = 67;
        t->step[i + 1u].time = ST_TIE;
    }
    nb = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  4-note chords stacked, their ties carry all four", pr_columns_match(t, &nb));
    for (i = 0; i < NSTEP; i++) {                                     /* LEN 32, page 2 */
        step_clear(&t->step[i]);
        if (i >= 16u && i % 2u == 0u) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = (uint8_t)(72 + i % 7u); }
    }
    t->p[P_SLEN] = 32; cursor_set(18);
    nb = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  LEN 32, the cursor on page 2: its 16 steps drawn", ui.bank == 1u && pr_columns_match(t, &nb) && nb == 8u);
    lo0 = proll.lo;                                                   /* two octaves up: the view follows in steps */
    for (i = 16; i < 32u; i++) if (t->step[i].n) t->step[i].note[0] += 24;
    for (i = 0; i < 20u && proll.lo != lo0 + 24u; i++) { frame(); steps++; }
    bad += check("  notes moved two octaves: the view follows over frames (not at once), then rests",
                 proll.lo == lo0 + 24u && steps >= 3u && steps < 20u);
    {
        uint32_t sig = ui.graph_sig;
        frame(); frame();
        bad += check("  nothing changed: the graph is not drawn again", ui.graph_sig == sig);
    }
    nb = 0;
    bad += check("  after the follow the bars match again", pr_columns_match(t, &nb));
    song.rec = 1;                                                     /* (#133: armed, the key writes the step) */
    key_down(7); frames(200);                                         /* a key held: its row lit on the strip */
    {
        int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - (int32_t)kb_snd[7][0];
        uint16_t c = r >= 0 && r < PR_ROWS ? swap16(host_screen[(Y_GRAPH + PR_Y0 + (uint32_t)r * PR_RH + 1u) * 240u + PR_KX + 7]) : 0;
        bad += check("  a key held: its row on the keyboard strip is the accent", kb_on[7] && c == T_ACCENT);
    }
    key_up(7); frames(100);
    song.rec = 0;
    return bad;
}

/* bug fixes, second round (UI) */
static uint32_t accent_in(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t n = 0;
    int32_t x, y;
    uint16_t a = (uint16_t)((T_ACCENT >> 8) | (T_ACCENT << 8));
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            n += host_screen[y * 240 + x] == a;
    return n;
}
static int test_bughunt_ui2(void)
{
    int bad = 0;
    uint32_t i;
    {   /* 1. a layer button pressed under a dialog and still held after it closes: dead, its keys stay notes */
        static const uint32_t BTN[2] = {B_FX, B_GLO};
        for (i = 0; i < 2u; i++) {
            uint32_t k = white(0), ok;
            ui_power_on(); TSEL->p[P_VOICE] = V_POLY;
            go_home(); frame();
            confirm_open(CF_CLEAR_SEQ, 0); frame();
            btn_down(BTN[i]); frame(); frames(100);
            btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();   /* the dialog closes, the button still held */
            frames(800);
            ok = !ui.confirm && (ui.ly_t0 & LY_DEAD) && !ui.layer && !kb_mask && !perf_mask;
            key_down(k); frame();
            ok = ok && kb_note[k] != KB_SILENT && gates() == 1u && !((kb_layer >> k) & 1u) && !perf_held && !ui.layer;
            key_up(k); frame();
            btn_up(BTN[i]); frame();
            bad += check(i ? "dead GLO held from a dialog: no map, the key plays a note"
                           : "dead FX held from a dialog: no map, the key plays a note, no effect", ok);
        }
    }
    {   /* 3. MIXER: the knob just turned is the one drawn in the accent (K1 LEVEL's dB, K2 PAN's middle, K3 REV, K4
         * MUTE: the sound's name); (0.6.80) in the six columns, with any COLOR (here not JIANT FM's) */
        uint32_t k, a[4][4], ok = 1;
        for (k = 0; k < 4u; k++) {
            int32_t X = T6_X(0), Y = Y_GRAPH;
            ui_power_on();
            settings.palette = 1; palette_set(1);
            go_page(GR_TRK); frame(); frame();
            turn(EN_K1 + k, 1);                                       /* (K4 right: MUTE on, its badge shown) */
            ui.force = 1; frame();
            a[k][0] = accent_in(X + 17, Y + 44, X + 37, Y + 57);      /* LEVEL's dB */
            a[k][1] = accent_in(X + 4, Y + 100, X + 37, Y + 110);     /* PAN */
            a[k][2] = accent_in(X + 4, Y + 112, X + 37, Y + 117);     /* REV */
            a[k][3] = accent_in(X + 4, Y + 22, X + 37, Y + 37);       /* MUTE: the name */
        }
        for (k = 0; k < 4u; k++) {
            uint32_t j;
            for (j = 0; j < 4u; j++)
                ok &= j == k ? a[k][j] > 0u : a[k][j] == 0u;
        }
        bad += check("MIXER (six columns, any COLOR): K1..K4 light LEVEL / PAN / REV / MUTE, nothing else", ok);
    }
    {   /* 4. renaming the slot the music came from renames the music: the next SAVE prefills the new name */
        char pn[16];
        uint32_t ok;
        ui_power_on();
        go_page(GR_SLOTS); song.g[G_SLOT] = 2;
        project_save_as(1, "OLD");
        project_save_as(2, "OTHER");
        project_load(1); frame();
        ok = str_eq(proj_name, "OLD");
        project_rename(2, "ELSE");
        ok &= str_eq(proj_name, "OLD");                               /* another slot: the music's name stays */
        press(B_EDIT);                                                /* rename B on the slot list */
        while (nm.len) { nm_do(NB_DEL); }
        nm_white(13); frames(900);
        str_cpy(nm.s, "NEW", sizeof nm.s); nm.len = nm.cur = 3; nm.key = 0;
        press(B_OCTUP);
        project_name(1, pn);
        ok &= str_eq(pn, "NEW") && str_eq(proj_name, "NEW");
        turn(EN_K4, 1); press(B_OCTUP);                               /* SAVE to B: OVERWRITE? */
        press(B_OCTUP);                                               /* YES: NAME */
        ok &= name_on() && str_eq(nm.s, "NEW");
        press(B_OCTDN);
        bad += check("rename of the loaded slot: the music's name follows, SAVE prefills it", ok);
    }
    {   /* 5. NAME: PLAY stops a transport started meanwhile (so the name can be saved), never starts it; REC ignored */
        uint32_t ok;
        ui_power_on(); go_page(GR_USER); ui.uslot = 3; press(B_OCTUP);
        ok = name_on();
        press(B_PLAY); frame();
        ok &= !transport_req && !song.playing && name_on();          /* stopped: PLAY starts nothing */
        press(B_REC); frame();
        ok &= !song.rec && !transport_req && name_on();
        song.playing = 1;                                             /* an external MIDI Start / the editor */
        press(B_OCTUP);
        ok &= name_on() && msg_is("STOP TO SAVE");
        press(B_REC); frame();
        ok &= !song.rec && !transport_req;
        press(B_PLAY); frame();
        ok &= transport_req == 2u && name_on();                       /* PLAY stops it, NAME stays */
        stop_transport();
        press(B_OCTUP);
        ok &= !name_on() && up_used(3);
        bad += check("NAME: PLAY stops a running transport (never starts), REC ignored, then OCT+ saves", ok);
    }
    {   /* 6. the piano roll's lowest C name ("C-1") stays inside the panel: nothing drawn left of x 3 */
        uint32_t p, x, y, out = 0;
        uint16_t bg;
        for (p = 0; p < NPALETTES; p++) {
            track_t *t;
            uint8_t lo = 0, hi = 127;
            ui_power_on(); t = TSEL; track_defaults_steps(t); t->p[P_SLEN] = 16;
            settings.palette = (uint8_t)p; palette_set(p);
            t->step[0].time = t->step[5].time = ST_NOTE; t->step[0].n = t->step[5].n = 1;
            t->step[0].note[0] = lo; t->step[5].note[0] = hi;
            go_page(GR_ROLL); cursor_set(0); ui.force = 1; frame(); frame();
            bg = (uint16_t)((T_BG >> 8) | (T_BG << 8));
            for (y = Y_GRAPH; y < Y_GRAPH + H_GRAPH; y++)
                for (x = 0; x < 3u; x++)
                    out += host_screen[y * 240u + x] != bg;
        }
        bad += check("piano roll: the C-1 label stays inside the panel (every palette)", !out);
    }
    {   /* 7. EDIT: only F3 (INIT) and G3 (RECALL) act; the other white keys do nothing */
        uint32_t p, ok = 1;
        for (p = 0; p < 16u; p++) {
            ui_power_on(); go_home(); frame();
            live.cur = -1;
            btn_down(B_EDIT); key_down(white(p)); frame(); frames(50);
            ok &= (ui.confirm == CF_INIT_SOUND) == (p == 0u) && ui.confirm != CF_RECALL_SOUND;
            key_up(white(p)); btn_up(B_EDIT); frame();
            ui.confirm = 0;
        }
        bad += check("EDIT: F3 opens INIT, G3 RECALL (none stored: no dialog), the other keys nothing", ok);
    }
    return bad;
}

#ifndef UI_TEST_NO_MAIN
/* L is a sparse face (tools/gen_aa_font.py L_CHARS): every string drawn in it has all its glyphs */
static int test_large_face(void)
{
    static const char *const FIXED[] = {"JIANT", "0123456789"};   /* ui_menu.c (ABOUT); ui_draw.c draw_uboot */
    uint32_t i, missing = 0;
    const char *s;
    for (i = 0; i < NB + NE + 2u; i++)                              /* ui_input.c setup_show: the control names */
        for (s = i < NB ? B_NAME[i] : i < NB + NE ? E_NAME[i - NB] : FIXED[i - NB - NE]; *s; s++)
            missing += glyph_at(&AF_L, (uint8_t)*s) < 0;
    return check("the L face holds every glyph of the strings drawn in it", !missing);
}

#if FELUCCA_FM4
/* The DIGITAL algorithm charts (ui_graph.c FM_CELL / FM_MOD) against the DSP: src/eng_digital.c's switch (alg)
 * in digital_render_legacy and digital_render_custom, written out here as "source>destination" routes and the
 * operators mixed to the output. Op 4's feedback is on every algorithm (drawn always). Also: the carriers sit
 * on the bottom row and only they, a modulator one row above what it modulates, no two operators in a cell,
 * every operator heard, and the eight charts distinct. */
static int test_fm_charts(void)
{
    static const char *const DSP[8][2] = {
        {"4>3 3>2 2>1", "1"},          /* o3 = op(o4), o2 = op(o3), s = op(o2) */
        {"3>2 4>2 2>1", "1"},          /* o2 = op((o3 + o4) / 2), s = op(o2) */
        {"3>2 2>1 4>1", "1"},          /* o2 = op(o3), s = op((o2 + o4) / 2) */
        {"4>3 2>1 3>1", "1"},          /* o3 = op(o4), s = op((o2 + o3) / 2) */
        {"4>3 2>1", "13"},             /* o3 = op(o4), o1 = op(o2), s = (o1 + o3) / 2 */
        {"4>1 4>2 4>3", "123"},        /* o1, o2, o3 = op(o4), s = (o1 + o2 + o3) / 3 */
        {"4>3", "123"},                /* o3 = op(o4), s = (o1 + o2 + o3) / 3 */
        {"", "1234"}};                 /* s = (o1 + o2 + o3 + o4) / 4 */
    uint32_t a, b, k, j, routes = 1, layout = 1, distinct = 1;
    for (a = 0; a < 8u; a++) {
        uint32_t want = 0, car = 0, ok = 1;
        const char *s;
        for (s = DSP[a][0]; *s; s++)
            if (*s == '>') want |= 1u << (4u * (uint32_t)(s[1] - '1') + (uint32_t)(s[-1] - '1'));
        for (s = DSP[a][1]; *s; s++) car |= 1u << (*s - '1');
        if (FM_MOD[a] != want) { printf("ui: algorithm %u: routes %04x, the DSP %04x\n", a + 1u, FM_MOD[a], want); routes = 0; }
        for (k = 0; k < 4u; k++) {
            uint32_t row = FM_CELL[a][k] >> 4, outs = 0;
            if ((row == 0u) != ((car >> k) & 1u)) ok = 0;
            for (j = 0; j < 4u; j++) {
                if (j != k && FM_CELL[a][j] == FM_CELL[a][k]) ok = 0;
                if (FM_MOD[a] >> (4u * j + k) & 1u) { outs++; if (row != (FM_CELL[a][j] >> 4) + 1u) ok = 0; }
            }
            if (!outs && !((car >> k) & 1u)) ok = 0;
        }
        if (!ok) { printf("ui: algorithm %u: the chart's layout is off\n", a + 1u); layout = 0; }
        for (b = 0; b < a; b++)
            if (FM_MOD[a] == FM_MOD[b] && !memcmp(FM_CELL[a], FM_CELL[b], 4)) distinct = 0;
    }
    return check("DIGITAL charts: the 8 algorithms' routes and carriers equal eng_digital.c's", routes) +
           check("DIGITAL charts: carriers on the bottom row, modulators a row up, one per cell, distinct", layout && distinct);
}
#endif

/* The FM6 algorithm charts (ui_graph.c graph_fm6: FM6_CELL, its routes fm6_routes and feedback operator
 * fm6_fb_op read from fm6_core.c FM6_ALG) against the 32 algorithms of 6-operator FM written out here as
 * "source>destination" routes, the carriers and the operator with feedback (fm6_core.c's: on algorithms 4 and
 * 6 the loop is the sixth operator's own, as msfa plays it). Also against the core itself: the carriers
 * fm6_carriers (fm6_car_ops), the feedback flags. The layout: the carriers on the bottom row and only they, a modulator one
 * row above everything it modulates, one operator per cell, columns 0..5, at most 4 rows. (The charts as drawn:
 * tests/ui_render.c's lint, fm6_lint.) */
static int test_fm6_charts(void)
{
    static const struct { const char *r, *car; char fb; } ALGS[32] = {
        {"2>1 4>3 5>4 6>5", "13", '6'}, {"2>1 4>3 5>4 6>5", "13", '2'}, {"2>1 3>2 5>4 6>5", "14", '6'},
        {"2>1 3>2 5>4 6>5", "14", '6'}, {"2>1 4>3 6>5", "135", '6'}, {"2>1 4>3 6>5", "135", '6'},
        {"2>1 4>3 5>3 6>5", "13", '6'}, {"2>1 4>3 5>3 6>5", "13", '4'}, {"2>1 4>3 5>3 6>5", "13", '2'},
        {"2>1 3>2 5>4 6>4", "14", '3'}, {"2>1 3>2 5>4 6>4", "14", '6'}, {"2>1 4>3 5>3 6>3", "13", '2'},
        {"2>1 4>3 5>3 6>3", "13", '6'}, {"2>1 4>3 5>4 6>4", "13", '6'}, {"2>1 4>3 5>4 6>4", "13", '2'},
        {"2>1 3>1 5>1 4>3 6>5", "1", '6'}, {"2>1 3>1 5>1 4>3 6>5", "1", '2'}, {"2>1 3>1 4>1 5>4 6>5", "1", '3'},
        {"2>1 3>2 6>4 6>5", "145", '6'}, {"3>1 3>2 5>4 6>4", "124", '3'}, {"3>1 3>2 6>4 6>5", "1245", '3'},
        {"2>1 6>3 6>4 6>5", "1345", '6'}, {"3>2 6>4 6>5", "1245", '6'}, {"6>3 6>4 6>5", "12345", '6'},
        {"6>4 6>5", "12345", '6'}, {"3>2 5>4 6>4", "124", '6'}, {"3>2 5>4 6>4", "124", '3'},
        {"2>1 4>3 5>4", "136", '5'}, {"4>3 6>5", "1235", '6'}, {"4>3 5>4", "1236", '5'},
        {"6>5", "12345", '6'}, {"", "123456", '6'}};
    uint32_t a, k, j, routes = 1, layout = 1, core = 1;
    for (a = 0; a < 32u; a++) {
        uint8_t m[6], want[6] = {0, 0, 0, 0, 0, 0};
        uint32_t car = 0, ok = 1, fbs = 0;
        const char *s;
        for (s = ALGS[a].r; *s; s++)
            if (*s == '>') want[s[1] - '1'] |= (uint8_t)(1u << (s[-1] - '1'));
        for (s = ALGS[a].car; *s; s++) car |= 1u << (*s - '1');
        fm6_routes(a, m);
        if (memcmp(m, want, 6) || fm6_fb_op(a) != (uint32_t)(ALGS[a].fb - '1')) {
            printf("ui: FM6 algorithm %u: the chart's routes or feedback differ from the written ones\n", a + 1u);
            routes = 0;
        }
        for (k = 0; k < 6u; k++)                                    /* the core: who writes the output, who has FB */
            fbs += (FM6_ALG[a][k] & 0xC0u) == 0xC0u;
        if (fm6_car_ops(a) != car || fbs != 1u) {
            printf("ui: FM6 algorithm %u: fm6_core.c's carriers / feedback differ\n", a + 1u);
            core = 0;
        }
        for (k = 0; k < 6u; k++) {
            uint32_t row = FM6_CELL[a][k] >> 4, outs = 0;
            if ((row == 0u) != ((car >> k) & 1u) || (FM6_CELL[a][k] & 0x0Fu) > 5u || row > 3u) ok = 0;
            for (j = 0; j < 6u; j++) {
                if (j != k && FM6_CELL[a][j] == FM6_CELL[a][k]) ok = 0;
                if (m[j] >> k & 1u) { outs++; if (FM6_CELL[a][k] >> 4 != (FM6_CELL[a][j] >> 4) + 1u) ok = 0; }
            }
            if (!outs && !((car >> k) & 1u)) ok = 0;               /* every operator heard */
        }
        if (!ok) { printf("ui: FM6 algorithm %u: the chart's layout is off\n", a + 1u); layout = 0; }
    }
    return check("FM6 charts: the 32 algorithms' routes, carriers and feedback operator equal fm6_core.c's", routes && core) +
           check("FM6 charts: carriers on the bottom row, modulators a row up, one per cell, at most 4 rows", layout);
}

#if !FELUCCA_FM4
/* DIGITAL retired (src/fm4_convert.c): engine 1 is on no track and in no list; its sounds arrive as FM6 with a patch
 * of their own: a user preset of it, its preset numbers (set_engine_of / apply_preset_to), a track that got engine 1
 * any other way (the main loop's net); the favourites of its presets move to FM6's; the power-on pad is FM6 PAD */
static int test_fm4_retired(void)
{
    int bad = 0, ok = 1;
    uint32_t i, k, e, total, seen = 0, all = ((1u << NENGINES) - 1u) & ~(1u << ENGI_DIGITAL) & ~(1u << ENGI_PHYS) &
        ~(1u << ENGI_SAMPLE) & ~(1u << ENGI_GRAIN) & ~(1u << ENGI_SLICE) & ~(1u << ENGI_TRIO);
    int16_t p[P_COUNT];
    uint8_t v[FP_SIZE + 1u];
    ui_power_on();
    bad += check("power-on: track 2 is FM6 PAD (TRK_DEF, was DIGITAL PAD)", trk[1].eng_req == ENGI_FM6 &&
                 str_eq(ENGINES[ENGI_FM6]->presets[trk[1].preset].name, "PAD"));
    preset_all_pos(&total);
    for (i = 0; i < total; i++) {
        e = preset_all_at(i, &k);
        if (e < NENGINES)
            seen |= 1u << e;
    }
    bad += check("PRESETS: the list holds every engine's presets but the retired ones' (DIGITAL PHYS SAMPLE GRAIN SLICE TRIO)",
                 seen == all && NENG_SHOWN == NENGINES - 6u);
    go_page(GR_BROWSE);
    set_engine_of(TSEL, 0);
    for (i = 0, seen = 0; i < NENG_SHOWN; i++) {
        turn(EN_K2, 1);
        seen |= 1u << TSEL->eng_req;
    }
    bad += check("PRESETS KNOB 2: the engines in order, DIGITAL skipped, back to the first",
                 seen == all && TSEL->eng_req == 0u && eng_step(0, 1) == ENGI_FM6 && eng_step(ENGI_FM6, 1) == 2u &&
                 eng_step(ENGI_FM6, -1) == 0u && eng_step(0, -1) == ENGI_DRUM);
    {   /* the display order (engines.c ENGINE_ORDER): every engine one can pick once; the PRESETS list follows it */
        static const char *const ORDER[] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "WHEEL", "NOISE", "DRUM"};
        uint32_t last = 0xFFu, r = 0, n = 0;
        ok = NENG_SHOWN == NELEM(ORDER);
        for (i = 0; ok && i < NENG_SHOWN; i++)
            ok &= str_eq(ENGINES[eng_vis(i)]->name, ORDER[i]) && eng_rank(eng_vis(i)) == i && eng_ok(eng_vis(i));
        preset_all_pos(&total);
        for (i = 0; i < total; i++) {                  /* the list's engines, each once, in that order */
            e = preset_all_at(i, &k);
            if (e >= NENGINES || e == last)
                continue;
            ok &= n < NENG_SHOWN && e == eng_vis(n);
            n++;
            last = e;
            r++;
        }
        bad += check("engines shown ANALOG FM6 PHASE ... NOISE DRUM (ENGINE_ORDER); PRESETS lists them so",
                     ok && r == NENG_SHOWN);
    }
    /* a user preset stored with engine 1: kept as it is, it loads as FM6 with the converted patch */
    for (i = 0; i < P_COUNT; i++)
        p[i] = param_desc_of(ENGI_DIGITAL, i)->def;
    fm4_preset_values(p, 5);                           /* DIGITAL PAD */
    {
        up_rec_t r;
        memset(&r, 0, sizeof r);
        r.used = UP_USED;
        r.ver = UP_VER;
        r.engine = ENGI_DIGITAL;
        r.np = P_COUNT;
        memcpy(r.name, "OLD PAD", 7);
        for (i = 0; i < P_COUNT; i++)
            up_set_value(&r, i, p[i]);
        up_put(7, &r);
    }
    fm4_convert(p, v);
    up_load(7);
    bad += check("a DIGITAL user preset loads as FM6: the converted patch its own (SLOT OWN), preset = FM6 PAD",
                 TSEL->eng_req == ENGI_FM6 && TSEL->preset == 4u && TSEL->p[P_E7] == FM6_OWN && !TSEL->p[P_E0] &&
                 !memcmp(fm6_patch[song.sel], v, FP_SIZE) && fm6_slot[song.sel] == FM6_OWN && TSEL->user == 8u);
    frame();
    bad += check("  .. and the main loop keeps that patch (not a factory one)", !memcmp(fm6_patch[song.sel], v, FP_SIZE));
    eng_list_pos(&total);
    bad += check("  the record stays DIGITAL in the bank, listed with FM6's sounds (EDIT KNOB 2)",
                 up_rec(7)->engine == ENGI_DIGITAL && up_engine(7) == ENGI_FM6 &&
                 total == ENGINES[ENGI_FM6]->npresets + 1u);
    up_store(8, "AGAIN");
    {
        uint8_t pk[FM6_PACKED], w[FP_SIZE + 1u];
        bad += check("  saved again: an FM6 user preset with SLOT OWN and the converted patch beside it",
                     up_rec(8)->engine == ENGI_FM6 && up_value(up_rec(8), P_E7) == FM6_OWN && !upf_get(8, pk) &&
                     (fm6_unpack(pk, w), !memcmp(w, v, FP_SIZE)));
        fm6_load_slot(song.sel, 2);                    /* another patch on the track, then the preset again */
        up_load(8);
        frame();
        bad += check("  .. which loads back: the same patch, SLOT OWN", !memcmp(fm6_patch[song.sel], v, FP_SIZE) &&
                     TSEL->p[P_E7] == FM6_OWN && fm6_slot[song.sel] == FM6_OWN);
    }
    /* preset numbers of engine 1 */
    set_engine_of(TSEL, ENGI_DIGITAL);
    bad += check("engine 1 asked for: DIGITAL E.PIANO converted (FM6, PTCH TINE EP, the patch named E.PIANO)",
                 TSEL->eng_req == ENGI_FM6 && TSEL->preset == 0u && !memcmp(fm6_patch[song.sel] + FP_NAME, "E.PIANO", 7));
    TSEL->eng_req = ENGI_DIGITAL;                      /* (the ISR's view: no other path does this) */
    apply_preset_to(TSEL, 1);
    bad += check("a DIGITAL preset number (apply_preset_to): its sound converted (BELL)",
                 TSEL->eng_req == ENGI_FM6 && TSEL->preset == 1u && !memcmp(fm6_patch[song.sel] + FP_NAME, "BELL", 4));
    for (i = 0; i < P_COUNT; i++)
        trk[2].p[i] = param_desc_of(ENGI_DIGITAL, i)->def;
    fm4_preset_values(trk[2].p, 2);
    trk[2].eng_req = ENGI_DIGITAL;
    frame();
    bad += check("a track left on engine 1 any other way: the main loop converts it (BASS -> FM BASS)",
                 trk[2].eng_req == ENGI_FM6 && trk[2].preset == 2u);
    {   /* favourites of DIGITAL presets (settings PER4) -> the FM6 presets that cover them */
        persist_t pe;
        memset(&pe, 0, sizeof pe);
        pe.magic = PERSIST_MAGIC;
        pe.panel = PANEL_DEFAULT;
        pe.favorites.factory[ENGI_DIGITAL][0] = 1u << 5 | 1u << 7;   /* PAD, FUNK KEY */
        settings_import(&pe, (int)sizeof pe);
        bad += check("favourites: DIGITAL PAD / FUNK KEY -> FM6 PAD / PLUCK, engine 1's cleared",
                     favorite_has(ENGI_FM6, 4) && favorite_has(ENGI_FM6, 7) && !favorites.factory[ENGI_DIGITAL][0]);
        memset(&favorites, 0, sizeof favorites);
    }
    return bad;
}
#endif

/* #38: the keys of the notes the selected track's sequencer and ARP sound light while it plays */
static int test_play_leds(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    uint32_t i;
    ui_power_on();
    song.sel = 0;
    t->step[0] = (step_t){.note = {60, 64}, .n = 2, .time = ST_NOTE};
    t->step[1] = (step_t){.note = {100}, .n = 1, .time = ST_NOTE};
    trk[1].step[0] = (step_t){.note = {67}, .n = 1, .time = ST_NOTE};
    t->seq_active = trk[1].seq_active = 1;
    bad += check("stopped, nothing held: no key lit", key_leds(0) == 0u && play_leds() == 0u);
    seq_start();
    events_block(CTL);
    bad += check("playing: C4 and E4 of step 1 light keys 8 and 12 (from F3), track 2's G4 does not",
                 t->seq_n == 2u && trk[1].seq_n == 1u && key_leds(0) == (1u << 7 | 1u << 11));
    song.sel = 1;
    bad += check("the selected track's notes: track 2's G4 on key 15", play_leds() == 1u << 14);
    song.sel = 0;
    fm1_in.notes = 1u << 0;
    bad += check("a key held still lights with them", key_leds(0) == (1u << 0 | 1u << 7 | 1u << 11));
    fm1_in.notes = 0;
    song.octave = -1;
    ok = play_leds() == (1u << 19 | 1u << 23);
    song.octave = 1;
    ok &= play_leds() == 0u;                           /* (C4 and E4 below the keys an octave up) */
    song.octave = 0;
    t->p[P_TRANS] = 2;                                 /* (0.6.81: TRN moves the sequence as the keys: the same keys) */
    ok &= play_leds() == (1u << 7 | 1u << 11);
    t->p[P_TRANS] = 0;
    bad += check("the keys follow the octave (TRN moves the keys and the sequence alike); notes off the keyboard are not shown", ok);
    t->p[P_QUANT] = 1;                                 /* SNAP in C major: C#4 rounds down to C4 too */
    t->p[P_SCALE] = 1;
    t->p[P_ROOT] = 0;
    ok = kb_map(t, 8) == 60u && play_leds() == (1u << 7 | 1u << 11);
    t->p[P_QUANT] = t->p[P_SCALE] = 0;
    bad += check("QNT SNAP: only the key of the note itself lights, not the keys rounding onto it", ok);
    t->arp_note = 65;
    bad += check("the ARP's note lights its key too", play_leds() == (1u << 7 | 1u << 11 | 1u << 12));
    t->arp_note = 0;
    events_block(div_samples((uint32_t)t->p[P_SDIV]) + CTL);
    bad += check("a note above the keyboard (G#7) lights nothing", t->seq_n == 1u && t->seq_notes[0] == 100u &&
                 play_leds() == 0u);
    transport_req = 2;
    events_block(CTL);
    bad += check("stopped: the notes end, their keys go dark", !song.playing && key_leds(0) == 0u);
    /* the layer, the grid and NAME keep their keys */
    seq_start();
    events_block(CTL);
    ui.layer = 1;
    ok = key_leds(0) == layer_leds(0);
    ui.layer = 0;
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    t->step[0] = (step_t){.hit = 1u << DV_KICK, .time = ST_NOTE};
    transport_req = 2; events_block(CTL);
    seq_start(); events_block(CTL);
    i = play_leds();
    ok &= t->seq_n == 1u && i != 0u && kb_map(t, (uint32_t)__builtin_ctz(i)) == DRUM_LANE_NOTE[DV_KICK];
    open_family(FAM_SEQ);
    frame();
    ok &= grid_on() && key_leds(0) == grid_leds();
    bad += check("the layer's map and the DRUM grid keep their keys; elsewhere a kit's hits light the keys that play them", ok);
    transport_req = 2; events_block(CTL);
    return bad;
}

/* Discussion #89: on SEQ > STEP (the piano roll) while stopped the keys of the cursor step's notes light, at the
 * octave now (the lowest key that gives each); with the keys held; playing, what plays instead (#38); not on
 * other pages, not for a REST step; the DRUM grid keeps its own map */
static int test_cursor_step_leds(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    song.sel = 0;
    t = TSEL;
    t->step[0] = (step_t){.note = {60, 64}, .n = 2, .time = ST_NOTE};
    t->step[1] = (step_t){.note = {100}, .n = 1, .time = ST_NOTE};
    t->step[2] = (step_t){.note = {62}, .n = 1, .time = ST_REST};
    t->step[3] = (step_t){.note = {67}, .n = 1, .time = ST_TIE};
    t->seq_active = 1;
    bad += check("#89 HOME, stopped: no step's keys lit", key_leds(0) == 0u);
    go_page(GR_ROLL);
    frame();
    bad += check("#89 STEP, stopped: the cursor step's C4 and E4 light keys 8 and 12",
                 ui.cursor == 0u && key_leds(0) == (1u << 7 | 1u << 11));
    song.octave = -1;
    ok = key_leds(0) == (1u << 19 | 1u << 23);
    song.octave = 1;
    ok &= key_leds(0) == 0u;                            /* (below the keys an octave up) */
    song.octave = 0;
    bad += check("#89 the step's keys follow the octave; notes off the keyboard are not shown", ok);
    fm1_in.notes = 1u << 0;
    bad += check("#89 a key held lights with them", key_leds(0) == (1u << 0 | 1u << 7 | 1u << 11));
    fm1_in.notes = 0;
    turn(EN_K1, 1);
    ok = ui.cursor == 1u && key_leds(0) == 0u;          /* G#7: off the keyboard */
    turn(EN_K1, 1);
    ok &= ui.cursor == 2u && key_leds(0) == 0u;         /* a REST */
    turn(EN_K1, 1);
    ok &= ui.cursor == 3u && key_leds(0) == 1u << 14;   /* a TIE carries its G4 */
    turn(EN_K1, 1);
    ok &= ui.cursor == 4u && key_leds(0) == 0u;         /* empty */
    bad += check("#89 KNOB 1 moves the cursor and the keys follow its step (REST, empty, off the keys: dark)", ok);
    turn(EN_K1, -4);
    seq_start();
    events_block(CTL);
    bad += check("#89 playing: the keys show what plays, not the cursor step", song.playing && ui.cursor == 0u &&
                 key_leds(0) == play_leds());
    turn(EN_K1, 3);
    bad += check("#89   (the cursor on G4's step changes nothing while playing)", key_leds(0) == play_leds() &&
                 !((key_leds(0) >> 14) & 1u));
    transport_req = 2;
    events_block(CTL);
    frame();
    bad += check("#89 stopped again: the cursor step's keys", !song.playing && key_leds(0) == 1u << 14);
    go_page(GR_CHANCE);
    frame();
    bad += check("#89 CHANCE (same cursor): no step keys", key_leds(0) == 0u);
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    t->step[0] = (step_t){.hit = 1u << DV_KICK, .time = ST_NOTE};
    go_page(GR_ROLL);
    frame();
    bad += check("#89 the DRUM grid keeps its map", grid_on() && key_leds(0) == grid_leds());
    return bad;
}

/* Discussion #81: the notes MIDI IN holds on the selected track light their keys as the sequencer's do: USB and TRS,
 * routed by ROUT; at the octave now, the lowest key; off the keyboard nothing; the pedal holds them; the layer's map
 * and the DRUM grid keep their keys */
static void usb_note(uint32_t ch, uint32_t note, uint32_t vel)   /* a USB-MIDI packet, as usb.c queues it */
{
    uint32_t st = (vel ? 0x90u : 0x80u) | ch;
    midi_in_event((st >> 4) | st << 8 | note << 16 | vel << 24);
    events_block(CTL);
}
static void trs_note(uint32_t a, uint32_t b, uint32_t c)        /* TRS MIDI IN: midi_uart.c's parser */
{
    um_byte(a); um_byte(b); um_byte(c);
    events_block(CTL);
}
static int test_midi_leds(void)
{
    int bad = 0, ok;
    ui_power_on();
    memset(&um, 0, sizeof um);
    song.sel = 0;
    usb_note(0, 60, 100);                              /* channel 1 -> T1 (ROUT CH1-4) */
    bad += check("#81 USB MIDI C4 into the selected track lights key 8 (C4)", mn_get(0, 60) == 1u &&
                 key_leds(0) == 1u << 7 && play_leds() == 1u << 7);
    usb_note(1, 67, 100);                              /* channel 2 -> T2: not the selected track */
    ok = key_leds(0) == 1u << 7;
    song.sel = 1;
    ok &= key_leds(0) == 1u << 14;
    song.sel = 0;
    bad += check("  another track's MIDI notes light nothing; selected, its G4 lights key 15", ok);
    usb_note(1, 67, 0);
    usb_note(0, 60, 0);
    bad += check("  note-off: dark", key_leds(0) == 0u && !midi_owners[0]);
    trs_note(0x90, 64, 90);                           /* TRS: E4 on channel 1 */
    ok = key_leds(0) == 1u << 11;
    trs_note(0x80, 64, 0);
    bad += check("  TRS MIDI IN: E4 lights key 12 while held", ok && key_leds(0) == 0u);
    song.g[G_ROUTE] = 1;                               /* ROUT SEL: every channel to the selected track */
    song.sel = 2;
    usb_note(9, 62, 100);
    ok = mn_get(9, 62) == 3u && key_leds(0) == 1u << 9;
    usb_note(9, 62, 0);
    song.g[G_ROUTE] = 0;
    song.sel = 0;
    bad += check("  ROUT SEL: channel 10 into the selected track lights its key", ok && key_leds(0) == 0u);
    usb_note(0, 60, 100);
    song.octave = -1;
    ok = key_leds(0) == 1u << 19;
    song.octave = 1;
    ok &= key_leds(0) == 0u;                            /* (C4 below the keys an octave up) */
    song.octave = 0;
    usb_note(0, 60, 0);
    usb_note(0, 100, 100);                             /* G#7: above the keyboard */
    usb_note(0, 40, 100);                              /* E2: below it */
    ok &= key_leds(0) == 0u && midi_owners[0] == 2u;
    usb_note(0, 100, 0);
    usb_note(0, 40, 0);
    bad += check("  the keys follow the octave; notes off the keyboard are not shown", ok);
    trk[0].p[P_QUANT] = 1;                             /* SNAP in C major: C#4 rounds down to C4 too */
    trk[0].p[P_SCALE] = 1;
    trk[0].p[P_ROOT] = 0;
    usb_note(0, 60, 100);
    ok = kb_map(&trk[0], 8) == 60u && key_leds(0) == 1u << 7;
    usb_note(0, 60, 0);
    trk[0].p[P_QUANT] = trk[0].p[P_SCALE] = 0;
    bad += check("  QNT SNAP: only the lowest key giving the note lights", ok && key_leds(0) == 0u);
    midi_event(0xB0, 0, 64, 127);                      /* the pedal down: a released note still sounds */
    usb_note(0, 65, 100);
    usb_note(0, 65, 0);
    ok = key_leds(0) == 1u << 12;
    midi_event(0xB0, 0, 64, 0);
    bad += check("  a note the pedal holds stays lit until the pedal is up", ok && key_leds(0) == 0u);
    fm1_in.notes = 1u << 0;                            /* with a key held and the sequencer */
    usb_note(0, 60, 100);
    ok = key_leds(0) == (1u << 0 | 1u << 7);
    fm1_in.notes = 0;
    trk[0].step[0] = (step_t){.note = {64}, .n = 1, .time = ST_NOTE};
    trk[0].seq_active = 1;
    seq_start();
    events_block(CTL);
    ok &= key_leds(0) == (1u << 7 | 1u << 11);
    bad += check("  with a key held and the sequencer's notes: all of them lit", ok);
    ui.layer = LAYER_SCL;
    {
        uint32_t b1, b2;
        ok = key_leds(&b1) == layer_leds(&b2) && b1 == b2 && b1;
    }
    ui.layer = 0;
    open_family(FAM_SEQ);                              /* STEP on a melodic track: the keys as elsewhere */
    frame();
    ok &= !grid_on() && (key_leds(0) & (1u << 7));
    set_engine_of(&trk[0], ENGI_DRUM);
    trk[0].engine = trk[0].eng_req;
    frame();
    ok &= grid_on() && key_leds(0) == grid_leds();
    bad += check("  the layer's map and the DRUM grid keep their keys over the MIDI notes", ok);
    transport_req = 2;
    events_block(CTL);
    usb_note(0, 60, 0);
    return bad;
}

/* #119: what can be pressed breathes (hal/fm1_input.h fm1_led_breath: dark .. ~60 % of lit), never a hard blink: every
 * layer, held and locked (double tap), in every MENU > LEDS mode: its keys that can be pressed and its button in the
 * breath set only (neither lit nor the glow), the ones in effect lit, the picture the same at any time (no 250 ms
 * toggle left); OCT+ in a dialog likewise; closing the layer leaves nothing breathing; the glow's level and
 * the breath's peak follow LEDS (fm1_led_dim_level: DIM LO the darker glow and ~30 %, else DIM HI's and ~60 %) */
static uint32_t breath_keys(void)                     /* the breathing keys, bit k = key k (the real positions) */
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++) {
        uint8_t q = led_pos[14u + k];
        m |= (uint32_t)(q != 0xFF && ((fm1_led_breath[q >> 3] >> (q & 7u)) & 1u)) << k;
    }
    return m;
}
static uint32_t lit_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++) {
        uint8_t q = led_pos[14u + k];
        m |= (uint32_t)(q != 0xFF && ((l[q >> 3] >> (q & 7u)) & 1u)) << k;
    }
    return m;
}
static int led_of(const uint8_t *l, uint32_t id)
{
    uint8_t q = led_pos[id];
    return q != 0xFF && ((l[q >> 3] >> (q & 7u)) & 1u);
}
static int test_breath(void)
{
    static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[4] = {LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static const uint8_t MODES[4] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    int bad = 0, ok = 1, okl = 1, okm = 1, oko = 1;
    uint32_t i, m, lk, t, held;
    ui_power_on();
    ui_leds();
    for (i = 0; i < 41u; i++)                           /* (the stub key map has none: 4 LEDs a column) */
        led_pos[i] = (uint8_t)((i / 4u) << 3 | (1u + i % 4u));
    for (i = 0; i < 4u; i++)
        for (lk = 0; lk < 1u; lk++)                         /* (1, locked: gone in TONIC) */
            for (m = 0; m < 4u; m++) {
                uint32_t br0 = 0, lit0 = 0, want_br, want_lit, hk = LL[i] == LAYER_GLO ? white(1) : white(2);
                ui_power_on();
                settings_leds = MODES[m];
                go_title("ENV"); frame();
                btn_down(LB[i]);
                key_down(hk); frame();
                okl &= ui.layer == LL[i];
                want_lit = layer_leds(&want_br);
                held = LL[i] == LAYER_FX || LL[i] == LAYER_GLO;  /* (HOLD keys: lit while held) */
                for (t = 0; t < 8u; t++) {                  /* 0 .. 1.75 s: the same picture */
                    fm1_ms += 250u;
                    ui_leds();
                    if (!t) {
                        br0 = breath_keys();
                        lit0 = lit_keys(fm1_led);
                    }
                    ok &= breath_keys() == br0 && lit_keys(fm1_led) == lit0 && !lit_keys(fm1_led_dim) &&
                          led_of(fm1_led_breath, panel.btn[LB[i]]) && !led_of(fm1_led, panel.btn[LB[i]]) &&
                          !led_of(fm1_led_dim, panel.btn[LB[i]]);
                }
                ok &= br0 == want_br && lit0 == want_lit && br0 && !(br0 & lit0);
                ok &= !held || (((lit0 >> hk) & 1u) && !((br0 >> hk) & 1u));   /* held: lit, not breathing */
                okm &= host_dim_lo == (MODES[m] == LEDS_DIM_LO);
                key_up(hk); frame();
                if (lk)
                    press(LB[i]);
                else
                    btn_up(LB[i]);
                frames(64);
                ui_leds();
                for (t = 0; t < FM1_NCOL; t++)
                    okl &= !ui.layer && !fm1_led_breath[t];
            }
    settings_leds = LEDS_DIM;
    bad += check("#119 FX GLO SCL EDIT, held and locked, every LEDS mode: the keys that can be pressed breathe", ok);
    bad += check("  ..held / in effect lit, never the glow, the layer's button breathes, steady over time", ok);
    bad += check("  ..closed: nothing breathes", okl);
    bad += check("  ..the breath's peak follows LEDS (DIM LO ~30 % of lit, else ~60 %)", okm);
    /* OCT+ in a dialog: breathing, OCT- lit, in every mode */
    for (m = 0; m < 4u; m++) {
        ui_power_on();
        settings_leds = MODES[m];
        go_title("ENV"); frame();
        ui.confirm = CF_CLEAR_SEQ;
        for (t = 0; t < 4u; t++) {
            fm1_ms += 250u;
            ui_leds();
            oko &= led_of(fm1_led_breath, panel.btn[B_OCTUP]) && !led_of(fm1_led, panel.btn[B_OCTUP]) &&
                   !led_of(fm1_led_dim, panel.btn[B_OCTUP]) && led_of(fm1_led, panel.btn[B_OCTDN]) ==
                   (MODES[m] != LEDS_INV) && !led_of(fm1_led_breath, panel.btn[B_OCTDN]);
        }
        ui.confirm = 0;
        ui_leds();
        oko &= !led_of(fm1_led_breath, panel.btn[B_OCTUP]);
    }
    settings_leds = LEDS_DIM;
    led_pos_init();
    bad += check("#119 a dialog: OCT+ breathes (not lit, not the glow), OCT- lit (INV: dark), every LEDS mode", oko);
    ui_power_on();
    return bad;
}

/* #35: every idle button and key glows dim, the active ones are lit; a map of the keys' own stays lit / dark */
static int test_idle_glow(void)
{
    int bad = 0, ok;
    ui_power_on();
    ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_HOME]] = 0u << 3 | 1u;
    led_pos[panel.btn[B_ENV]] = 0u << 3 | 2u;
    led_pos[panel.btn[B_PLAY]] = 0u << 3 | 3u;
    led_pos[14u + 0u] = 1u << 3 | 1u;              /* keys 1 and 2 */
    led_pos[14u + 1u] = 1u << 3 | 2u;
    go_home();
    ui_leds();
    bad += check("HOME: its button lit, ENV and PLAY (stopped) glow, the keys glow, none lit",
                 fm1_led[0] == 2u && fm1_led_dim[0] == 14u && fm1_led[1] == 0u && fm1_led_dim[1] == 6u);
    fm1_in.notes = 1u;
    song.playing = 1;
    ui_leds();
    ok = fm1_led[0] == 2u && (fm1_led_dim[0] & 8u) == 0u && fm1_led[8] == 2u && fm1_led[1] == 2u && fm1_led_dim[1] == 6u;
    fm1_in.notes = 0;
    song.playing = 0;
    bad += check("a key held lit over the glow; PLAY running: its green, its own LED dark", ok);
    ui.layer = 1;
    ui_leds();
    ok = fm1_led_dim[1] == 0u && fm1_led_dim[0] == 14u;
    ui.layer = 0;
    ui_leds();
    ok &= fm1_led_dim[1] == 6u;
    bad += check("a layer's map: the keys lit or dark (no glow), the buttons still glow", ok);
    /* MENU > LEDS INV: the active ones dark, the idle ones lit (stock), no glow; a breath as in DIM (#119); a map
     * of the keys' own as in DIM */
    settings_leds = LEDS_INV;
    ui_leds();
    ok = fm1_led[0] == 12u && fm1_led_dim[0] == 0u && fm1_led[1] == 6u && fm1_led_dim[1] == 0u;   /* HOME dark */
    fm1_in.notes = 1u;
    song.playing = 1;
    ui_leds();
    ok &= fm1_led[0] == 4u && fm1_led_dim[0] == 0u && fm1_led[1] == 4u && fm1_led_dim[1] == 0u && fm1_led[8] == 2u;   /* PLAY, key 1 dark; green */
    fm1_in.notes = 0;
    song.playing = 0;
    bad += check("LEDS INV: the page's button, PLAY running and a key held go dark, the rest lit, no glow", ok);
    ui.layer = 1;
    led_pos[panel.btn[layer_btn()]] = 0u << 3 | 4u;   /* the layer's button */
    {
        uint32_t seen = 0, t;
        for (t = 0; t < 4u; t++) {                  /* the layer's button breathes: never lit (inverted), never the glow */
            fm1_ms = t * 250u;
            ui_leds();
            seen |= ((fm1_led[0] >> 4) & 1u ? 1u : 2u) | ((fm1_led_dim[0] >> 4) & 1u ? 4u : 0u) |
                    ((fm1_led_breath[0] >> 4) & 1u ? 8u : 0u);
        }
        ok = seen == 10u && fm1_led_dim[1] == 0u && fm1_led[1] == (uint8_t)((layer_leds(0) & 3u) << 1);
    }
    ui.layer = 0;
    fm1_ms = 0;
    bad += check("LEDS INV: the layer's button breathes (not inverted); a layer's map stays lit / dark as in DIM", ok);
    settings_leds = LEDS_DIM;
    led_pos_init();
    return bad;
}

/* "On the step sequencer all the LEDs go out": SEQ > STEP, the piano roll and the DRUM grid, stopped and playing,
 * in every LEDS mode, with the LEDs where the hardware has them (hal/fm1_input.h FM1_KEYMAP). The buttons keep
 * their glow and their active states; on the piano roll the keys glow under the held and playing ones; on the
 * grid its map is lit and its keys that do something glow (DIM LO / HI). Before: keys_own() took the grid's keys
 * out of the glow, so in DIM 26 of 27 keys of an empty 16-step grid went fully dark */
/* the white keys of the grid's steps (bit p = step p of the page shown), bit k = key k */
static uint32_t step_keys(uint32_t steps)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) < 16u && ((steps >> key_place(k)) & 1u))
            m |= 1u << k;
    return m;
}
static int test_step_leds(void)
{
    static const int8_t MAP[6][FM1_NCOL] = {     /* (hal/fm1_input.h FM1_KEYMAP; the host stub is zero) */
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
        {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
        {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
        { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    };
    static const uint8_t MODES[LEDS_COUNT] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    int bad = 0;
    uint32_t id, p, r, mi, drum, run, k;
    ui_power_on();
    ui_leds();
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (MAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)(p << 3 | r);
    }
    for (drum = 0; drum < 2u; drum++)
        for (run = 0; run < 2u; run++)
            for (mi = 0; mi < LEDS_COUNT; mi++) {
                uint32_t m = MODES[mi], glow = m == LEDS_DIM || m == LEDS_DIM_LO, keys_lit = 0, keys_dim = 0, btn_bad = 0;
                uint32_t want_dim = 0, kc, keys;
                char what[128];
                ui_power_on();
                song.sel = 0;
                if (drum) {
                    set_engine_of(TSEL, ENGI_DRUM);
                    TSEL->engine = TSEL->eng_req;
                    TSEL->p[P_SLEN] = 16;
                    TSEL->step[2] = (step_t){.hit = 1u << DV_KICK, .time = ST_NOTE};   /* one hit, on step 3 */
                } else {
                    TSEL->step[0] = (step_t){.note = {60}, .n = 1, .time = ST_NOTE};   /* C4 on step 1 */
                }
                TSEL->seq_active = 1;
                settings_leds = (uint8_t)m;
                open_family(FAM_SEQ);
                frame();
                for (k = 0; k < NPAGES && cur_page()->graph != GR_ROLL; k++) {
                    open_family(FAM_SEQ);
                    frame();
                }
                if (run) {
                    seq_start();
                    events_block(CTL);
                }
                ui_leds();
                for (k = 0; k < NB; k++) {                  /* a button: lit if active, else glow / dark / lit */
                    uint8_t q = led_pos[panel.btn[k]];
                    uint32_t lit = (fm1_led[q >> 3] >> (q & 7u)) & 1u, dim = (fm1_led_dim[q >> 3] >> (q & 7u)) & 1u;
                    int active = k == B_SEQ || (k == B_PLAY && run);
                    btn_bad += m == LEDS_INV ? lit == (uint32_t)active || dim :
                               active ? (k == B_PLAY ? lit || dim : !lit) : lit || dim != glow;
                }
                keys = key_leds(0);
                for (k = 0; k < 27u; k++) {
                    uint8_t q = led_pos[14u + k];
                    keys_lit += (fm1_led[q >> 3] >> (q & 7u)) & 1u;
                    keys_dim += ((fm1_led_dim[q >> 3] & ~fm1_led[q >> 3]) >> (q & 7u)) & 1u;
                }
                kc = (uint32_t)__builtin_popcount(keys);
                if (glow)
                    want_dim = (uint32_t)__builtin_popcount((drum ? grid_glow() : 0x7FFFFFFu) & ~keys);
                else if (drum)                              /* (OFF, INV: the grid's beats alone glow) */
                    want_dim = (uint32_t)__builtin_popcount(grid_beats() & ~keys);
                snprintf(what, sizeof what, "STEP %s, %s, LEDS %s: buttons and keys as the mode says (%u lit, %u glow)",
                         drum ? "DRUM grid" : "piano roll", run ? "playing" : "stopped", LEDS_NAME[m],
                         (unsigned)keys_lit, (unsigned)keys_dim);
                bad += check(what, !btn_bad && host_dim_lo == (m == LEDS_DIM_LO) &&
                                       grid_on() == (int)drum && (!run || kc) &&
                                       keys_dim == want_dim && keys_lit == (m == LEDS_INV && !drum ? 27u - kc : kc) &&
                                       (!glow || keys_lit + keys_dim >= 25u) &&
                                       ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) == run);
                bad += check("  the mid level: the grid's beats in DIM HI / LO only, never a lit key",
                             lit_keys(fm1_led_mid) == (glow && drum ? grid_beats() & ~keys : 0u) &&
                             !(lit_keys(fm1_led_mid) & lit_keys(fm1_led)));
                if (run) {
                    transport_req = 2;
                    events_block(CTL);
                }
            }
    {   /* the DRUM grid's beats (ui_input.c grid_beats): "5111 5111 5111 5111", the first step of each four brighter
         * where no hit shows, the hits lit and the playhead as before; LEN cuts them, a page shows its own; groups of
         * four whatever LEN / DIV; DIM HI / LO: fm1_led_mid over the glow, OFF / INV: the glow alone */
        uint32_t want, beats, keys, kick = 1u << DV_KICK, ok = 1, mi;
        for (mi = 0; mi < LEDS_COUNT; mi++) {
            uint32_t m = MODES[mi], glow = m == LEDS_DIM || m == LEDS_DIM_LO;
            char what[128];
            ui_power_on();
            song.sel = 0;
            set_engine_of(TSEL, ENGI_DRUM);
            TSEL->engine = TSEL->eng_req;
            TSEL->p[P_SLEN] = 16;
            TSEL->step[4] = (step_t){.hit = kick, .time = ST_NOTE};   /* a kick on step 5 (a beat) and on step 3 */
            TSEL->step[2] = (step_t){.hit = kick, .time = ST_NOTE};
            TSEL->seq_active = 1;
            settings_leds = (uint8_t)m;
            open_family(FAM_SEQ);
            frame();
            for (k = 0; k < NPAGES && cur_page()->graph != GR_ROLL; k++) {
                open_family(FAM_SEQ);
                frame();
            }
            ui.lane = 0;
            ui_leds();
            keys = key_leds(0);
            beats = grid_beats();
            want = step_keys(1u << 0 | 1u << 8 | 1u << 12);   /* steps 1, 9, 13 (5 holds a hit: lit) */
            ok = beats == want && (keys & step_keys(1u << 4 | 1u << 2)) == step_keys(1u << 4 | 1u << 2) &&
                 lit_keys(fm1_led_mid) == (glow ? want : 0u) && (lit_keys(fm1_led_dim) & want) == want &&
                 (glow || !(lit_keys(fm1_led_dim) & ~want)) && host_dim_lo == (m == LEDS_DIM_LO);
            snprintf(what, sizeof what, "DRUM grid beats, LEDS %s: steps 1 9 13 %s, the hits 3 5 lit", LEDS_NAME[m],
                     glow ? "at the mid level over the glow" : "glow (nothing else does)");
            bad += check(what, ok);
            if (m != LEDS_DIM)
                continue;
            ui.lane = 1;                                /* another lane: none of its hits there, 1 5 9 13 */
            ok = grid_beats() == step_keys(0x1111u);
            ui.lane = 0;
            TSEL->p[P_SLEN] = 12;                       /* LEN 12 (3/4): 1 5 9; 13 is past it */
            ok &= grid_beats() == step_keys(1u << 0 | 1u << 8);
            TSEL->p[P_SLEN] = 16; TSEL->p[P_SDIV] = 4;  /* (DIV changes nothing: steps are counted) */
            ok &= grid_beats() == want;
            song.playing = 1; TSEL->seq_idx = 8;        /* the playhead on step 9: it lights it, no mid there */
            ui_leds();
            ok &= grid_beats() == step_keys(1u << 0 | 1u << 12) && (lit_keys(fm1_led) & step_keys(1u << 8)) &&
                  !(lit_keys(fm1_led_mid) & step_keys(1u << 8));
            song.playing = 0;
            TSEL->p[P_SLEN] = 32;                       /* page 2: its own steps 17 21 25 29 */
            TSEL->step[20] = (step_t){.hit = kick, .time = ST_NOTE};
            ui.bank = 1; ui.cursor = 16;
            ok &= grid_beats() == step_keys(1u << 0 | 1u << 8 | 1u << 12);
            bad += check("  another lane, LEN 12, DIV, the playhead, page 2: the beats as the steps show", ok);
        }
    }
    settings_leds = LEDS_DIM;
    led_pos_init();
    ui_power_on();
    return bad;
}

/* Discussion #127 (1.2): MENU > SCALE LEDS ON, outside the keys' own maps the keys of the selected track's scale glow
 * (the others dark), the root's at the mid level; the keys held / playing lit over it; LEDS OFF / DIM LO / DIM HI
 * alike, INV turned round (root lit, scale mid, the others glowing, the active dark); TRN; QNT WHITE: the white
 * keys; a DRUM track, a layer, the default OFF: as before */
/* MENU > DISPLAY > SCREEN OFF (1.1.5, ui.c scr_*): the screen and its backlight off after NEVER / 5 / 15 / 30 (default)
 * / 60 minutes without panel input; the press, key or turn that wakes it does nothing else; the dialogs, the count-in
 * and the UPDATE MODE countdown keep it on and wake it; the sound goes on as if it were on (forked sessions, bit for
 * bit); the setting saved (a spare favorites byte, 0 = NEVER since 1.1.5.1) and offered in the MENU */
static void scr_idle(uint32_t ms)                  /* nobody touches it for ms (a frame a minute, then frames) */
{
    while (ms > 60000u) { fm1_ms += 60000u - 16u; host_ticks += 60000u * 1000u; frame(); ms -= 60000u; }
    frames(ms);
}
static int scr_woke(uint32_t from)                 /* since host_pw_n was `from`: SLPOUT >= 120 ms after the SLPIN,
                                                    * DISPON + backlight >= 120 ms after the SLPOUT, nothing else */
{
    return host_pw_n == from + 2u && host_pw[from] == 1u && host_pw[from + 1u] == 2u && host_bl &&
           host_pw_ms[from] - host_pw_ms[from - 1u] >= 120u && host_pw_ms[from + 1u] - host_pw_ms[from] >= 120u &&
           scrn.st == SCR_ON;
}
static uint64_t scr_session(uint32_t setting, uint32_t *flags)
{
    int32_t o[2u * CTL];
    uint64_t h = 1469598103934665603ull, e = 0;
    uint32_t f, b, i, off = 0;
    ui_power_on();
    scr_put(setting);
    song.master_q12 = 4096;
    my_steps(&trk[0]);
    transport_req = 1;
    for (f = 0; f < 400u; f++) {
        if (f == 50u)
            fm1_ms += 6u * 60000u;                 /* six minutes untouched: 5 MIN goes dark */
        frame();
        off |= !host_bl;
        for (b = 0; b < 4u; b++) {
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++) {
                h = (h ^ (uint32_t)o[i]) * 1099511628211ull;
                e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
            }
        }
    }
    *flags = off | (song.playing ? 2u : 0u) | (e ? 4u : 0u);
    stop_transport();
    return h;
}
static uint64_t scr_session_fork(uint32_t setting, uint32_t *flags)   /* from the same state each time */
{
    int fd[2];
    uint64_t r[2] = {0, 0};
    pid_t p;
    if (pipe(fd))
        return 0;
    p = fork();
    if (!p) {
        uint32_t fl = 0;
        r[0] = scr_session(setting, &fl);
        r[1] = fl;
        if (write(fd[1], r, sizeof r) != sizeof r) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != sizeof r) r[0] = 0;
    close(fd[0]);
    waitpid(p, 0, 0);
    *flags = (uint32_t)r[1];
    return r[0];
}
static int test_screen_off(void)
{
    int bad = 0, ok;
    uint32_t n, rows, i;
    int16_t *vp, v0;

    ui_power_on();
    ok = ui_scr == 0u && scr_get() == 0u && SCR_MIN[scr_get()] == 0u && str_eq(MI_NAME[MI_SCROFF], "SCREEN OFF") &&
         MI_TAB[MI_SCROFF] == MTAB_DISPLAY && menu_n(MI_SCROFF) == 5u && str_eq(menu_vname(MI_SCROFF, 0), "NEVER") &&
         str_eq(menu_vname(MI_SCROFF, 1), "5 MIN") && str_eq(menu_vname(MI_SCROFF, 2), "15 MIN") &&
         str_eq(menu_vname(MI_SCROFF, 3), "30 MIN") && str_eq(menu_vname(MI_SCROFF, 4), "60 MIN") &&
         menu_get(MI_SCROFF) == 0u;
    bad += check("SCREEN OFF: MENU > DISPLAY row, NEVER 5 15 30 60 MIN, NEVER by default (stored 0, 1.1.5.1)", ok);
    menu_put(MI_SCROFF, 3u);                         /* 30 MIN for what follows */

    host_pw_n = 0;
    scr_idle(30u * 60000u - 2000u);
    ok = host_pw_n == 0u && host_bl && scrn.st == SCR_ON;
    scr_idle(2000u);
    ok &= host_pw_n == 1u && host_pw[0] == 0u && !host_bl && scrn.st == SCR_OFF && kb_asleep;
    rows = host_blit_rows;
    ui_message("SAVED");
    scr_idle(5000u);
    ok &= host_blit_rows == rows && host_pw_n == 1u && !ui.msg_t;
    bad += check("SCREEN OFF: 30 MIN untouched -> DISPOFF + SLPIN once; dark: nothing drawn", ok);

    n = host_pw_n;
    ok = ui.home && !ui.menu;
    btn_down(B_ENV); frame();                         /* a page button wakes it, does not open its page */
    ok &= scrn.st == SCR_SLPOUT && host_pw_n == n + 1u && host_pw[n] == 1u;   /* (SLPIN long ago: SLPOUT at once) */
    frames(100u);
    ok &= host_pw_n == n + 1u && host_blit_rows == rows;   /* (dark until 120 ms after it) */
    rows = host_blit_rows;
    frames(200u);
    ok &= scr_woke(n) && host_blit_rows - rows >= 200u;   /* (the frame before DISPON redrew it all) */
    btn_up(B_ENV); frame(); frames(100u);
    ok &= ui.home && !ui.menu && !ui.layer && !scrn.eat && !kb_asleep;
    press(B_ENV);                                     /* awake: the button works again */
    ok &= !ui.home && cur_page()->fam == FAM_ENV;
    bad += check("SCREEN OFF: a button wakes it (SLPOUT, 120 ms, the page redrawn, DISPON), swallowed", ok);

    go_home();
    for (i = 0; i < 3u; i++) {                        /* HOME, SAVE, REC held across the wake: no menu, undo, clear */
        static const uint32_t B[3] = {B_HOME, B_SAVE, B_REC};
        scr_idle(31u * 60000u);
        n = host_pw_n;
        ok = !host_bl;
        hold(B[i]); frames(300u);
        ok &= scr_woke(n) && ui.home && !ui.menu && !ui.confirm && !song.rec && !song.playing && !transport_req;
        bad += check(i == 0 ? "SCREEN OFF: HOME held to wake: no menu, no HOME tap after" :
                     i == 1 ? "SCREEN OFF: SAVE held to wake: no undo, no SAVE page" :
                              "SCREEN OFF: REC held to wake: nothing armed, no clear, no transport", ok);
    }

    scr_idle(31u * 60000u);
    n = host_pw_n;
    ok = !host_bl && !gates();
    key_down(white(0)); frame();                      /* a key: wakes it, no note (seq.c kb_asleep) */
    ok &= !gates();
    frames(300u);
    ok &= scr_woke(n) && !gates();
    key_up(white(0)); frame(); frames(50u);
    ok &= !gates() && !kb_asleep;
    key_down(white(0)); frame();
    ok &= gates() == 1u;                              /* awake: the key plays */
    key_up(white(0)); frame();
    bad += check("SCREEN OFF: a key wakes it silently (no note on, none off), the next one plays", ok);

    scr_idle(31u * 60000u);
    n = host_pw_n;
    home_param(0, &vp);
    v0 = *vp;
    for (i = 0; i < 20u; i++) turn(EN_K1, 1);        /* a knob turned to wake it, still turning (on past the wake) */
    ok = scr_woke(n) && *vp == v0;
    turn(EN_PRESET, 1); turn(EN_ALGO, 1); turn(EN_SELECT, 1); turn(EN_K4, -1);
    ok &= *vp == v0 && song.sel == 0u && song.g[G_BPM] == GP[G_BPM].def && scrn.eat;
    frames(400u);                                     /* at rest SCR_EAT_MS: the knobs are the page's again */
    turn(EN_K1, 1);
    ok &= *vp != v0;
    bad += check("SCREEN OFF: a knob wakes it; turns until it rests 0.3 s change nothing (K1..4, PRESETS, ALGO, SELECT)", ok);

    menu_put(MI_SCROFF, 0u);                         /* NEVER */
    n = host_pw_n;
    scr_idle(3u * 3600000u);
    ok = host_pw_n == n && host_bl && ui_scr == 0u;
    menu_put(MI_SCROFF, 1u);                         /* 5 MIN */
    scr_idle(4u * 60000u + 50000u);
    ok &= host_pw_n == n && host_bl;
    scr_idle(15000u);
    ok &= host_pw_n == n + 1u && !host_bl;
    press(B_PLAY); frames(300u);                      /* PLAY wakes it, does not start the transport */
    ok &= scr_woke(n + 1u) && !song.playing && !transport_req;
    bad += check("SCREEN OFF: NEVER stays on for hours; 5 MIN goes dark after 5; PLAY to wake starts nothing", ok);

    n = host_pw_n;
    ui.confirm = CF_CLEAR_SEQ; ui.force = 1;          /* a dialog: never dark */
    scr_idle(20u * 60000u);
    ok = host_pw_n == n && host_bl;
    ui.confirm = 0; ui.force = 1;
    cin_left = 1000u;                                 /* the count-in */
    scr_idle(20u * 60000u);
    ok &= host_pw_n == n && host_bl;
    cin_left = 0;
    scr_idle(6u * 60000u);
    ok &= host_pw_n == n + 1u && !host_bl;
    ui.confirm = CF_CLEAR_SEQ; ui.force = 1;          /* one opening while dark (the editor, MIDI): it wakes */
    frames(300u);
    ok &= scr_woke(n + 1u) && ui.confirm == CF_CLEAR_SEQ;
    ui.confirm = 0; ui.force = 1;
    scr_idle(6u * 60000u);
    n = host_pw_n;
    ui.uboot = 3; ui.force = 1;                       /* the UPDATE MODE countdown */
    frames(300u);
    ok &= scr_woke(n);
    ui.uboot = 0; ui.force = 1;
    scr_idle(6u * 60000u);
    n = host_pw_n;
    scr_wake_now();                                   /* main.c: the crash screen, UPDATE, UBOOT */
    ok &= host_pw_n == n + 1u && host_pw[n] == 3u && host_bl && scrn.st == SCR_ON;
    bad += check("SCREEN OFF: a dialog, the count-in, UPDATE MODE keep it on and wake it; the crash screen at once", ok);

    {   /* the setting with the settings: SCR_CODE in favorites.factory[15][27]; 1.1.5's codes: 0 (its 30 MIN, its
         * default) now NEVER, 3 (its NEVER) NEVER, 2 1 7 as they were; 30 MIN is 4 now; unknown NEVER */
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        menu_put(MI_SCROFF, 4u);
        settings_export(&p); ui_scr = 0;
        ok = p.favorites.factory[15][27] == 7u && settings_import(&p, sizeof p) && scr_get() == 4u;
        p.favorites.factory[15][27] = 0;
        ok &= settings_import(&p, sizeof p) && scr_get() == 0u;
        p.favorites.factory[15][27] = 3;              /* 1.1.5's NEVER */
        ok &= settings_import(&p, sizeof p) && scr_get() == 0u;
        p.favorites.factory[15][27] = 2;              /* 5 MIN, as on 1.1.5 */
        ok &= settings_import(&p, sizeof p) && scr_get() == 1u;
        menu_put(MI_SCROFF, 3u); ok &= ui_scr == 4u && scr_get() == 3u;
        p.favorites.factory[15][27] = 6;              /* unknown: NEVER */
        ok &= settings_import(&p, sizeof p) && scr_get() == 0u;
        ui_scr = 0x55; ok &= scr_get() == 0u;
        bad += check("SCREEN OFF: saved with the settings (0, 1.1.5's 30 MIN, and unknown = NEVER)", ok);
    }

    ui_power_on();                                    /* in the MENU: OCT+ / a knob step it */
    menu_put(MI_SCROFF, 3u);
    hold(B_HOME);
    for (i = 0; i < 5u; i++) turn(EN_PRESET, 1);
    ok = ui.menu == 1 && ui.menu_sel == MI_SCROFF;
    press(B_OCTUP); ok &= scr_get() == 4u;
    press(B_OCTUP); ok &= scr_get() == 4u;           /* (stops at the end) */
    for (i = 0; i < 4u; i++) turn(EN_K2, -1);
    ok &= scr_get() == 0u;
    press(B_OCTDN); ok &= scr_get() == 0u && ui.menu == 1;
    press(B_OCTUP); press(B_OCTUP); press(B_OCTUP); ok &= scr_get() == 3u;
    press(B_HOME); ok &= ui.menu == 0;
    bad += check("SCREEN OFF: the MENU row steps NEVER .. 60 MIN (OCT-/+, any knob), stops at the ends", ok);

    {
        uint32_t f0 = 0, f1 = 0;
        uint64_t a = scr_session_fork(0u, &f0), b = scr_session_fork(1u, &f1);
        bad += check("SCREEN OFF: the sound bit for bit as with NEVER while dark (sequencer playing)",
                     a && a == b && (f0 & 6u) == 6u && (f1 & 6u) == 6u && !(f0 & 1u) && (f1 & 1u));
    }
    ui_power_on();
    return bad;
}

static int test_scale_leds(void)
{
    static const int8_t MAP[6][FM1_NCOL] = {     /* (hal/fm1_input.h FM1_KEYMAP; the host stub is zero) */
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
        {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
        {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
        { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    };
    static const uint8_t MODES[LEDS_COUNT] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    const uint32_t ALL = 0x7FFFFFFu, C_KEYS = 1u << 7 | 1u << 19;
    int bad = 0, ok;
    uint32_t id, p, r, k, mi, root = 0, sc, white = 0;
    track_t *t;
    ui_power_on();
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (MAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)(p << 3 | r);
    }
    for (k = 0; k < 27u; k++)
        white |= (uint32_t)!key_black(k) << k;
    song.sel = 0;
    t = TSEL;
    t->p[P_SCALE] = 1;                                  /* C major */
    t->p[P_ROOT] = 0;
    settings_leds = LEDS_DIM;
    ui_leds();
    ok = !menu_get(MI_SCLLED) && str_eq(MI_NAME[MI_SCLLED], "SCALE LEDS") && MI_TAB[MI_SCLLED] == MTAB_CONTROL &&
         !scale_leds(&root) && lit_keys(fm1_led_dim) == ALL && !lit_keys(fm1_led_mid);
    bad += check("#127 SCALE LEDS OFF (the default): every key glows as before", ok);
    menu_put(MI_SCLLED, menu_step(MI_SCLLED, 1));
    ok = menu_get(MI_SCLLED) == 1u && (ui_rec_prefs & 0x40u) && !(ui_prefs & ~PREF_ACCEL_OFF) && str_eq(menu_vname(MI_SCLLED, 1), "ON");
    sc = scale_leds(&root);
    bad += check("#127 MENU > CONTROL > SCALE LEDS ON (a bit of its own); C major: the white keys, the root C4 C5",
                 ok && sc == white && root == C_KEYS);
    for (mi = 0; mi < LEDS_COUNT; mi++) {
        uint32_t m = MODES[mi], held = 1u << 1 | 1u << 7;   /* F#3 (out of the scale) and C4 (the root) held */
        char what[128];
        settings_leds = (uint8_t)m;
        fm1_in.notes = 0;
        ui_leds();
        if (m == LEDS_INV)
            ok = lit_keys(fm1_led) == C_KEYS && lit_keys(fm1_led_mid) == (white & ~C_KEYS) &&
                 lit_keys(fm1_led_dim) == (ALL & ~white);
        else
            ok = !lit_keys(fm1_led) && lit_keys(fm1_led_dim) == white && lit_keys(fm1_led_mid) == C_KEYS;
        fm1_in.notes = held;
        ui_leds();
        if (m == LEDS_INV)
            ok &= lit_keys(fm1_led) == (C_KEYS & ~held) && !(lit_keys(fm1_led_mid) & held) &&
                  !(lit_keys(fm1_led_dim) & held) && lit_keys(fm1_led_mid) == (white & ~C_KEYS);
        else
            ok &= lit_keys(fm1_led) == held && lit_keys(fm1_led_dim) == white && lit_keys(fm1_led_mid) == C_KEYS;
        snprintf(what, sizeof what, "#127 LEDS %s: %s; the keys held %s", LEDS_NAME[m],
                 m == LEDS_INV ? "root lit, the scale mid, the others glow" : "the scale glows, the root brighter",
                 m == LEDS_INV ? "dark" : "lit over it");
        bad += check(what, ok);
    }
    fm1_in.notes = 0;
    settings_leds = LEDS_DIM;
    t->p[P_ROOT] = 2;                                   /* D major: F# and C# in, F and C out, D the root */
    sc = scale_leds(&root);
    ok = (sc >> 1 & 1u) && (sc >> 8 & 1u) && !(sc & 1u) && !(sc >> 7 & 1u) && root == (1u << 9 | 1u << 21) &&
         __builtin_popcount(sc) == 16;
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 2;                                  /* C major, TRN +2: F3 plays G3 (in), A#3 plays C4 (the root) */
    sc = scale_leds(&root);
    ok &= (sc & 1u) && !(sc >> 1 & 1u) && root == (1u << 5 | 1u << 17);
    t->p[P_TRANS] = 0;
    bad += check("#127 the keys as they play: ROOT D, TRN +2", ok);
    t->p[P_QUANT] = QN_SNAP;                            /* SNAP: the keys' own notes (the others round down) */
    sc = scale_leds(&root);
    ok = sc == white && root == C_KEYS;
    t->p[P_QUANT] = QN_WHITE;                           /* WHITE, PEN: every white key; a root every 5 white keys */
    t->p[P_SCALE] = 5;
    sc = scale_leds(&root);
    ok &= sc == white && root == (1u << 7 | 1u << 16 | 1u << 24);
    ui_leds();
    ok &= lit_keys(fm1_led_dim) == white && lit_keys(fm1_led_mid) == root;
    t->p[P_QUANT] = QN_OFF;
    t->p[P_SCALE] = 0;                                  /* CHR: every key, the C keys brighter */
    sc = scale_leds(&root);
    ok &= sc == ALL && root == C_KEYS;
    bad += check("#127 QNT SNAP: the keys of the scale; QNT WHITE: all the white keys, its roots; CHR: every key", ok);
    t->p[P_SCALE] = 1;
    ui.layer = LAYER_SCL;                               /* a layer: its own map, no scale glow under it */
    ui_leds();
    ok = !lit_keys(fm1_led_mid) && !lit_keys(fm1_led_dim);
    ui.layer = 0;
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    sc = scale_leds(&root);
    ui_leds();
    ok &= !sc && !root && lit_keys(fm1_led_dim) == ALL && !lit_keys(fm1_led_mid);
    open_family(FAM_SEQ);
    frame();
    for (k = 0; k < NPAGES && cur_page()->graph != GR_ROLL; k++) {
        open_family(FAM_SEQ);
        frame();
    }
    ui_leds();
    ok &= grid_on() && lit_keys(fm1_led_dim) == grid_glow() &&
          lit_keys(fm1_led_mid) == (grid_beats() & ~key_leds(0));
    bad += check("#127 a layer, a DRUM track (its keys as before) and the DRUM grid keep their own LEDs", ok);
    menu_put(MI_SCLLED, 0);
    ok = !(ui_rec_prefs & 0x40u) && !scale_leds(&root);
    bad += check("#127 SCALE LEDS OFF again: the bit clears", ok);
    ui_rec_prefs = 0;
    settings_leds = LEDS_DIM;
    led_pos_init();
    ui_power_on();
    return bad;
}

/* #37: QNT SEQ on the SCL page: the keys as SNAP, the sequence snapped as it plays, saved with the project */
static int test_seq_quant(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    char val[16];
    const char *unit = 0;
    ui_power_on();
    song.sel = 0;
    open_family(FAM_SCL);
    frame();
    ok = cur_page()->id[2] == P_QUANT && t->p[P_QUANT] == 0;
    turn(EN_K3, 10);
    param_format(&TP[P_QUANT], t->p[P_QUANT], val, &unit);
    bad += check("SCL KNOB 3 QNT: OFF SNAP WHITE SEQ, SEQ last (default OFF)", ok && t->p[P_QUANT] == 3 &&
                 str_eq(val, "SEQ"));
    t->p[P_SCALE] = 1;
    t->p[P_ROOT] = 0;
    ok = kb_map(t, 8) == 60u && kb_map(t, 7) == 60u;   /* C#4 key -> C4, as SNAP */
    t->step[0] = (step_t){.note = {61, 66}, .n = 2, .time = ST_NOTE};
    t->seq_active = 1;
    seq_start();
    events_block(CTL);
    ok &= t->seq_n == 2u && t->seq_notes[0] == 60u && t->seq_notes[1] == 65u && t->step[0].note[0] == 61u &&
          t->step[0].note[1] == 66u;
    transport_req = 2;
    events_block(CTL);
    bad += check("QNT SEQ: the keys snap as SNAP; C#4 F#4 of a step play C4 F4 in C major, the step keeps C#4 F#4", ok);
    project_save(1);
    t->p[P_QUANT] = 0;
    project_load(1);
    bad += check("QNT SEQ saved and loaded with the project", trk[0].p[P_QUANT] == 3);
    return bad;
}

/* #39: KNOB 1..4 with a layer's button held are the layer's only, also as it is let go: a knob turned in the frame
 * the button is let go, while its map still shows (an effect key held after FX), or just after: the page under the
 * layer (ARP: ARP MODE on) or HOME kept its values. GLO / SCL / EDIT change only what their own knobs edit */
static int16_t lk_p[NTRK][P_COUNT], lk_g[G_COUNT];
static void lk_snap(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        memcpy(lk_p[i], trk[i].p, sizeof lk_p[i]);
    memcpy(lk_g, song.g, sizeof lk_g);
}
static int lk_same(uint32_t l)                      /* nothing changed but the layer's own (l: the layer) */
{
    uint32_t i, j;
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < P_COUNT; j++) {
            int own = (l == LAYER_GLO && j == P_LEVEL) ||
                      (l == LAYER_SCL && (j == P_ROOT || j == P_SCALE || (i == 0u && (j == P_SOFS || j == P_POFS)))) ||   /* (one
                                                         * scale: every melodic track's ROOT and SCALE) */
                      (l == LAYER_EDIT && i == 0u && (j < P_AMODE || j > P_AORDER));   /* (a load keeps the ARP) */
            if (lk_p[i][j] != trk[i].p[j] && !own)
                return 0;
        }
    for (j = 0; j < G_COUNT; j++)
        if (lk_g[j] != song.g[j] && l != LAYER_EDIT)
            return 0;
    return 1;
}
static void lk_turn(int32_t s)                       /* KNOB 1..4 together, no frame */
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        host_enc[panel.enc[EN_K1 + k]] += s * panel.dir[EN_K1 + k];
}
static int test_layer_knob_leak(void)
{
    static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[4] = {LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    uint32_t b, sc, pg, fails = 0, arp_fx = 0;
    int bad = 0, ok;
    for (b = 0; b < 4u; b++)
        for (sc = 0; sc < 4u; sc++)
            for (pg = 0; pg <= NPAGES; pg++) {          /* every page, HOME last */
                ui_power_on();
                if (pg < NPAGES) { ui.home = 0; ui.page = (uint8_t)pg; page_entered(); }
                frames(100);
                lk_snap();
                btn_down(LB[b]);
                if (sc == 3u) key_down(white(1));
                frame();
                lk_turn(5); frame();
                if (sc == 0u) { lk_turn(2); btn_up(LB[b]); frame(); }          /* turning as it is let go */
                if (sc == 1u) { btn_up(LB[b]); frame(); lk_turn(2); frame(); } /* .. the next frame */
                if (sc == 2u) { btn_up(LB[b]); frames(160); lk_turn(2); frame(); }   /* .. 160 ms after */
                if (sc == 3u) { btn_up(LB[b]); frame(); lk_turn(3); frame(); key_up(white(1)); frame(); }   /* the map */
                frames(400);
                ok = lk_same(LL[b]) && !perf_k[0] && !perf_k[1] && !perf_k[2] && !perf_k[3];
                if (!ok) {
                    fails++;
                    if (fails < 6u)
                        printf("ui:   leak: %s scenario %u page %s\n", B_NAME[LB[b]], sc,
                               pg < NPAGES ? PAGES[pg].title : "HOME");
                }
                if (b == 0u && pg < NPAGES && PAGES[pg].graph == GR_ARP && trk[0].p[P_AMODE] == lk_p[0][P_AMODE])
                    arp_fx++;
            }
    bad += check("#39 FX / GLO / SCL / EDIT + KNOB 1..4, let go while turning: no page under the layer changes",
                 !fails && arp_fx == 4u);
    /* the quiet time is short: KNOB 1 edits the page again 0.3 s after FX */
    ui_power_on();
    go_page(GR_ARP);
    {
        int16_t m = TSEL->p[P_AMODE];
        btn_down(B_FX); frame(); turn(EN_K1, 4); btn_up(B_FX); frame();
        frames(320);
        turn(EN_K1, 1);
        ok = TSEL->p[P_AMODE] == m + 1 && !perf_k[0] && str_eq(cur_page()->title, "ARP");
    }
    bad += check("  0.3 s after FX is let go KNOB 1 is ARP MODE again; a knob turned with FX opened no FX page", ok);
    /* a tap of FX with a knob turned in that frame: a combo, no FX page */
    ui_power_on();
    go_page(GR_ARP);
    btn_down(B_FX); frame(); lk_turn(1); btn_up(B_FX); frame(); frames(100);
    bad += check("  FX tapped while a knob turns: no FX page, no ARP change", str_eq(cur_page()->title, "ARP") &&
                 TSEL->p[P_AMODE] == 0);
    return bad;
}

/* #102: the TIMER5 scan counts the knobs' detents while ui_input runs; one that came after the layer's read of
 * KNOB 1..4 (host_enc_late) was read again by the page's: the layer's control and the page under it both moved
 * (HOME's KNOB 1 with FX's FILTER). Every layer (FX GLO SCL EDIT) x KNOB 1..4 x held past HOLD / a combo / locked
 * by a double tap, over every page and HOME, with late detents on the knob, PRESETS and ALGORITHM while it is open
 * and on the knob as it closes: the same state as with the detents read in time (the layer gets them all, one a
 * frame later), the layer's own control moved, and nothing under the layer changed (lk_same) */
typedef struct {
    int16_t p[NTRK][P_COUNT], g[G_COUNT];
    int8_t k[4];
    uint32_t sel, snd[NTRK], mot[NTRK];
    uint8_t home, page;
} rc_state_t;
static uint32_t rc_snd(const track_t *t) { return t->eng_req | (uint32_t)t->preset << 8 | (uint32_t)t->user << 16; }
static uint32_t rc_snd0;
static void rc_take(rc_state_t *st)
{
    uint32_t i;
    memset(st, 0, sizeof *st);
    for (i = 0; i < NTRK; i++) {
        memcpy(st->p[i], trk[i].p, sizeof st->p[i]);
        st->snd[i] = rc_snd(&trk[i]);
        st->mot[i] = motion_count(&trk[i]);
    }
    memcpy(st->g, song.g, sizeof st->g);
    memcpy(st->k, perf_k, sizeof st->k);
    st->sel = song.sel;
    st->home = ui.home;
    st->page = ui.page;
}
static void rc_enc(uint32_t role, int32_t s, int late)
{
    if (late)
        host_enc_late[panel.enc[role]] += s * panel.dir[role];
    else
        host_enc[panel.enc[role]] += s * panel.dir[role];
}
/* one run: layer button lb, mode (0 held past HOLD, 1 a combo, 2 locked), knob k, page pg (NPAGES: HOME); late:
 * the detents come after the reads. a: the state with the layer open, b: after it closed */
static void rc_run(uint32_t lb, uint32_t mode, uint32_t k, uint32_t pg, int late, rc_state_t *a, rc_state_t *b)
{
    int32_t d = (lb == B_GLO || (lb == B_FX && k == 3u)) ? -1 : 1;   /* (GLO LEVEL, FX DEPTH: room to move) */
    uint32_t i;
    ui_power_on();
    if (pg < NPAGES) { ui.home = 0; ui.page = (uint8_t)pg; page_entered(); }
    frames(64);
    lk_snap();
    rc_snd0 = rc_snd(TSEL);
    if (mode == 2u) {
        press(lb); frames(64); press(lb); frames(64);
    } else {
        btn_down(lb);
        frame();
        if (mode == 0u)
            frames(560);                                /* (past HOLD: the map, a peek) */
    }
    for (i = 0; i < 3u; i++) {
        rc_enc(EN_K1 + k, d, late && (mode != 1u || i));   /* (a combo: the first in time, it opens the map) */
        if (i == 1u) {
            rc_enc(EN_PRESET, 1, late);                 /* (in time: the layer's read drops them) */
            rc_enc(EN_ALGO, 1, late);
        }
        frame();
    }
    frame();                                            /* (the last late one) */
    rc_take(a);
    if (mode == 2u) {
        press(lb);
    } else {
        btn_up(lb);
        frame();
    }
    for (i = 0; i < 3u; i++) {                          /* turning on as it closes (#39: KNOB 1..4 quiet) */
        rc_enc(EN_K1 + k, d, late);
        frame();
    }
    frames(400);
    rc_take(b);
}
static int test_layer_knob_race(void)
{
    static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[4] = {LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static rc_state_t a0, b0, a1, b1;
    uint32_t l, mode, k, pg, fails = 0, runs = 0;
    int bad = 0, ok;
    for (l = 0; l < 4u; l++)
        for (mode = 0; mode < 2u; mode++)                  /* (2, locked by a double tap: gone in TONIC) */
            for (k = 0; k < 4u; k++)
                for (pg = 0; pg <= NPAGES; pg++) {
                    if (pg < NPAGES && !page_visible(pg))
                        continue;
                    if (mode == 2u && LB[l] == B_EDIT && pg < NPAGES &&   /* (EDIT's tap acts there: no lock) */
                        (PAGES[pg].graph == GR_USER || PAGES[pg].graph == GR_SLOTS || PAGES[pg].graph == GR_ROLL ||
                         PAGES[pg].graph == GR_EVENTS))
                        continue;
                    rc_run(LB[l], mode, k, pg, 0, &a0, &b0);
                    rc_run(LB[l], mode, k, pg, 1, &a1, &b1);
                    ok = !memcmp(&a0, &a1, sizeof a0) && !memcmp(&b0, &b1, sizeof b0) &&
                         lk_same(LL[l]) && song.sel == 0u &&
                         !perf_k[0] && !perf_k[1] && !perf_k[2] && !perf_k[3];
                    if (LL[l] == LAYER_FX)                  /* the layer's own control moved */
                        ok &= a1.k[k] == 3;
                    else if (LL[l] == LAYER_GLO)
                        ok &= a1.p[k][P_LEVEL] == lk_p[k][P_LEVEL] - 3;
                    else if (LL[l] == LAYER_SCL)
                        ok &= a1.p[0][LY_SCL.id[k]] != lk_p[0][LY_SCL.id[k]];
                    else                                    /* (EDIT: EDIT 1's four, the engine's first) */
                        ok &= a1.p[0][P_E0 + k] != lk_p[0][P_E0 + k];
                    runs++;
                    if (!ok) {
                        fails++;
                        if (fails < 6u)
                            printf("ui:   race: %s mode %u KNOB %u page %s\n", B_NAME[LB[l]], mode, k + 1u,
                                   pg < NPAGES ? PAGES[pg].title : "HOME");
                    }
                }
    bad += check("#102 FX GLO SCL EDIT (held, combo, locked) x KNOB 1..4, detents counted mid-pass: the layer's only",
                 !fails && runs > 400u);
    /* HOME, REC armed and playing: FX's macros and GLO's T2..T4 levels record nothing into T1's automation */
    ok = 1;
    for (l = 0; l < 2u; l++)
        for (k = l; k < 4u; k++) {
            int16_t *vp, c;
            uint32_t i;
            ui_power_on();
            song.rec = 1; seq_start(); go_home(); frames(64);
            home_param(k, &vp);
            c = *vp;
            btn_down(LB[l]); frames(560);
            for (i = 0; i < 4u; i++) { rc_enc(EN_K1 + k, l ? -1 : 1, 1); frame(); }
            frame();
            btn_up(LB[l]); frames(400);
            ok &= !motion_count(TSEL) && *vp == c;
            stop_transport();
            song.rec = 0;
        }
    bad += check("  HOME with REC: FX's KNOB 1..4, GLO's KNOB 2..4 record no automation, HOME's sound stays", ok);
    /* FX LATCH: the macro stays when FX is let go, HOME's KNOB 1 never moved; FX + OCT- turns it off */
    ui_power_on();
    fx_latch = PREF_LATCH | PREF_ACCEL_OFF;
    go_home(); frames(64);
    {
        int16_t *vp, c;
        uint32_t i;
        home_param(0, &vp);
        c = *vp;
        btn_down(B_FX); frames(560);
        for (i = 0; i < 5u; i++) { rc_enc(EN_K1, 1, 1); frame(); }
        frame();
        btn_up(B_FX); frames(400);
        ok = perf_k[0] == 5 && *vp == c;
        btn_down(B_FX); frames(560); press(B_OCTDN); btn_up(B_FX); frames(400);
        ok &= !perf_k[0] && *vp == c;
    }
    fx_latch = PREF_ACCEL_OFF;
    bad += check("  FX LATCH: FILTER stays after FX is let go, HOME's KNOB 1 never moves; FX + OCT- turns it off", ok);
    /* no layer: a late detent is HOME's KNOB 1's, once (the next pass) */
    ui_power_on();
    go_home(); frames(64);
    {
        int16_t *vp, c;
        home_param(0, &vp);
        c = *vp;
        rc_enc(EN_K1, 1, 1); frame(); frame();
        ok = *vp == c + 1;
    }
    bad += check("  no layer: a detent counted mid-pass is HOME's KNOB 1, once", ok);
    return bad;
}

/* ------------------------------------------------------ SEQ TOOLS (1.2) --- */
static void tl_note(step_t *s, uint32_t n) { memset(s, 0, sizeof *s); s->time = ST_NOTE; s->n = 1; s->note[0] = (uint8_t)n; s->vel = 96; }
static void tl_tie(step_t *s) { memset(s, 0, sizeof *s); s->time = ST_TIE; }
/* the selected track: LEN 8, C4 tied, E4, G4 (steps 1 2, 4, 6), a note past LEN (21), a lock on step 4 */
static void tl_melody(void)
{
    uint32_t i;
    track_defaults_steps(TSEL);
    for (i = 0; i < NSTEP; i++)
        step_clear(&TSEL->step[i]);
    tl_note(&TSEL->step[0], 60);
    tl_tie(&TSEL->step[1]);
    tl_note(&TSEL->step[3], 64);
    tl_note(&TSEL->step[5], 67);
    tl_note(&TSEL->step[20], 72);
    TSEL->p[P_SLEN] = 8;
    motion_set_lock(TSEL, 3, P_REV, 50);
}
static int tl_lock_at(uint32_t step) { int16_t v; return motion_lock_get(TSEL, step, P_REV, &v) && v == 50; }
static void tl_key(uint32_t w) { key_down(white(w)); frame(); key_up(white(w)); frame(); }
static uint32_t tl_count(const track_t *t, uint32_t l)
{
    uint32_t i, c = 0;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++)
        c += (step_lanes(&t->step[i]) >> l) & 1u;
    return c;
}
static uint32_t tl_lanes_of(const track_t *t, uint32_t l)   /* lane l over steps 1..16, a bit each */
{
    uint32_t i, m = 0;
    for (i = 0; i < 16u; i++)
        m |= ((step_lanes(&t->step[i]) >> l) & 1u) << i;
    return m;
}
static int tl_in_key(const track_t *t)                       /* every note of steps 1..LEN in the track's scale */
{
    uint32_t i, k;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++)
        for (k = 0; k < t->step[i].n; k++)
            if (t->step[i].time == ST_NOTE && !tl_in_scale(t, t->step[i].note[k]))
                return 0;
    return 1;
}
static int tl_times_are(const track_t *t, const step_t *ref, uint32_t len)   /* the rhythm: the same NOTE / TIE / REST */
{
    uint32_t i;
    for (i = 0; i < len; i++)
        if (t->step[i].time != ref[i].time || (t->step[i].n != 0) != (ref[i].n != 0))
            return 0;
    return 1;
}
static void tl_drum(void)
{
    set_engine_of(TSEL, ENGI_DRUM);
    TSEL->engine = TSEL->eng_req;
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = 16;
}

static int test_seq_tools(void)
{
    static const uint8_t PG[4] = {GR_ROLL, GR_STEPS, GR_CHANCE, GR_MOTION};
    static const uint8_t NOT[4] = {GR_PATS, GR_SONG, GR_TRK, 0};
    int bad = 0, ok, okc, flash;
    uint32_t i, k, a, b2, pg, seed;
    step_t ref[NSTEP], mid[NSTEP];

    /* where it opens: the SEQ pages that show the pattern; elsewhere SEQ held is a tap (SONG: SAVE held, G5) */
    ok = 1;
    for (i = 0; i < 4u; i++) {
        ui_power_on();
        go_page(PG[i]); frame();
        pg = ui.page;
        btn_down(B_SEQ); frames(500);
        ok &= ui.layer == LAYER_SEQ && ui.page == pg;
        btn_up(B_SEQ); frame();
        ok &= !ui.layer && ui.page == pg && !ui.home;
    }
    bad += check("SEQ TOOLS: SEQ held on STEP, PATTERN, CHANCE, AUTOMATION opens the map (no SONG, the page stays)", ok);
    ok = 1;
    for (i = 0; i < 4u; i++) {
        ui_power_on();
        if (i == 3u) go_home(); else go_page(NOT[i]);
        frame();
        btn_down(B_SEQ);
        for (flash = 0, k = 0; k < 60u; k++) { frame(); flash |= ui.layer; }
        btn_up(B_SEQ); frame();
        ok &= !flash && !ui.home;                     /* (a tap: the next SEQ page) */
    }
    ui_power_on();
    go_title("ENV"); frame(); hold(B_SEQ);
    ok &= cur_page()->graph != GR_SONG && !ui.layer;
    bad += check("  elsewhere (PHRASES, SONG, MIXER, HOME, ENV) SEQ held is a tap: no SONG, no map", ok);
    ui_power_on();
    go_page(GR_ROLL); frame();
    {   /* (0.6.90) the piano roll, its pages, then the track's */
        static const char *const ORD[] = {"NOTE", "TIMING", "STRETCH", "COMP", "COMP 2", "WHEN", "PATTERN", "SHIFT", "HUMAN",
                                          "PLAYHEADS"};
        uint32_t q;
        ok = 1;
        for (q = 0; q < sizeof ORD / sizeof ORD[0]; q++) {
            press(B_SEQ); frames(320);
            ok &= str_eq(cur_page()->title, ORD[q]);
            if (!q) ok &= msg_is("HOLD [SEQ] QUICK");
        }
    }
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_MOTION;
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_EVENTS;
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_MUTATE;
    bad += check("  a tap on STEP: the next SEQ page, as always (the hint until the layer was opened)", ok);
    {   /* (0.6.85) SEQ > STRETCH: x2 .. x8 / 2 .. / 8 (the notes spread, held ones held), REPEAT x2 .. x8 / TO 64, NUDGE */
        track_t *t;
        uint32_t i, ok2 = 1, ok3 = 1, ok4 = 1, ok5;
        ui_power_on(); stop_transport();
        song.sel = 0; t = &trk[0];
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        motion_clear(t);
        t->p[P_SLEN] = 4;                                /* C D E G, the D held a step (N T) */
        t->step[0].time = ST_NOTE; t->step[0].n = 1; t->step[0].note[0] = 60; t->step[0].vel = 100;
        t->step[1].time = ST_NOTE; t->step[1].n = 1; t->step[1].note[0] = 62; t->step[1].vel = 100;
        t->step[2].time = ST_TIE;
        t->step[3].time = ST_NOTE; t->step[3].n = 1; t->step[3].note[0] = 67; t->step[3].vel = 100;
        (void)motion_put(t, 3, P_FCUT, 20, 0);
        go_page(GR_SHAPE); frame();
        shp_i = 7u; shp_r = 0u;                          /* (x2, x2: whatever an earlier test turned) */
        ok = str_eq(cur_page()->title, "STRETCH");
        turn(EN_K1, 1);                                  /* x2 -> x3 */
        ok &= ui.act == 1u && shp_k() == 3 && shp_len() == 12u;
        press(B_OCTUP);
        ok &= t->p[P_SLEN] == 12 && t->step[0].note[0] == 60 && t->step[3].note[0] == 62 && t->step[9].note[0] == 67;
        for (i = 4; i < 9u; i++) ok2 &= t->step[i].time == ST_TIE;          /* the D held through its old TIE */
        ok2 &= t->step[1].time == ST_REST && t->step[10].time == ST_REST;   /* the short ones stay short */
        ok2 &= motion.count == 1u && MOTION_STEP(&motion.event[0]) == 9u;  /* the lock went with its step */
        if (!(ok && ok2)) { printf("  len %d mc %u ms %u |", t->p[P_SLEN], motion.count, motion.count ? MOTION_STEP(&motion.event[0]) : 99u);
            for (i = 0; i < 12u; i++) printf(" %u:%u", t->step[i].time, t->step[i].note[0]); printf("\n"); }
        bad += check("STRETCH x3: LEN 4 > 12, each step three apart, a held note held, its lock along", ok && ok2);
        ok = !undo_do() && t->p[P_SLEN] == 4 && t->step[3].note[0] == 67 && MOTION_STEP(&motion.event[0]) == 3u;
        bad += check("  UNDO: the sequence back as it was", ok);
        for (i = 0; i < 5u; i++) turn(EN_K1, -1);       /* x3 .. x2 /2 /3 /4 .. : back to /2 */
        while (shp_k() != -2) turn(EN_K1, shp_k() < -2 ? 1 : -1);
        t->p[P_SLEN] = 8;                               /* eight steps: C . D T E . G . */
        t->step[4] = t->step[3]; t->step[4].note[0] = 64; t->step[3].time = ST_REST; t->step[6] = t->step[4]; t->step[6].note[0] = 67;
        motion_clear(t);
        press(B_OCTUP);
        ok3 = t->p[P_SLEN] == 4 && t->step[0].note[0] == 60 && t->step[1].note[0] == 62 && t->step[2].note[0] == 64 &&
              t->step[3].note[0] == 67 && t->step[4].time == ST_REST;
        bad += check("STRETCH /2: LEN 8 > 4, the first note of each pair", ok3);
        for (i = 4; i < NSTEP; i++) step_clear(&t->step[i]);
        (void)motion_put(t, 1, P_FCUT, 30, 0);
        turn(EN_K2, 1);                                 /* x2 -> x3 */
        ok4 = ui.act == 2u && shp_len() == 12u;
        press(B_OCTUP);
        ok4 &= t->p[P_SLEN] == 12;
        for (i = 4; i < 12u; i++) ok4 &= t->step[i].time == t->step[i % 4u].time && t->step[i].note[0] == t->step[i % 4u].note[0];
        ok4 &= motion.count == 3u && t->step[12].time == ST_REST;
        if (!ok4) printf("  rep len %d mc %u\n", t->p[P_SLEN], motion.count);
        while (shp_r != 7u) turn(EN_K2, 1);             /* TO 64 */
        press(B_OCTUP);
        ok4 &= t->p[P_SLEN] == 60 && t->step[59].note[0] == t->step[11].note[0];   /* (whole copies of 12: 60) */
        press(B_OCTUP);
        ok4 &= t->p[P_SLEN] == 60;                      /* (no room for another 60) */
        bad += check("REPEAT x3: 4 steps > 12, their locks copied; TO 64: whole copies (12 > 60), no room after", ok4);
        t->p[P_SLEN] = 4;
        ok5 = t->step[0].note[0] == 60;
        turn(EN_K3, 1);
        ok5 &= t->step[1].note[0] == 60 && t->step[0].note[0] == 67 && t->p[P_SLEN] == 4;
        turn(EN_K3, -1);
        ok5 &= t->step[0].note[0] == 60;
        bad += check("NUDGE: KNOB 3 moves the sequence a step right / left at once", ok5);
    }
    {   /* (0.6.37) SHIFT: OFFSET PITCH CHANCE, the last SEQ page; CHANCE 0 skips every step, 100 % (stored 0) none */
        char v[12]; const char *u; step_t st; track_t *t3 = &trk[0];
        ui_power_on(); go_title("SHIFT"); frame();
        ok = str_eq(cur_page()->title, "SHIFT") && cur_page()->id[2] == P_TCHAN;
        param_format(track_desc(t3, P_TCHAN), t3->p[P_TCHAN], v, &u);
        ok &= str_eq(v, "100") && str_eq(track_desc(t3, P_TCHAN)->label, "CHANCE");
        turn(EN_K3, -127);
        param_format(track_desc(t3, P_TCHAN), t3->p[P_TCHAN], v, &u);
        memset(&st, 0, sizeof st);
        ok &= str_eq(v, "0") && trk_chance(t3, &st) == 0u;
        turn(EN_K3, 127);
        step_set_chance(&st, 40);
        ok &= t3->p[P_TCHAN] == 0 && trk_chance(t3, &st) == 40u;
        bad += check("SHIFT > CHANCE: the track's chance (100 % as before, 0 % silent), K3", ok);
    }

    /* the whole sequence: REVERSE (a note's ties stay after it), SHIFT, CLEAR, locks, LEN, silent keys, undo */
    ui_power_on();
    tl_melody();
    memcpy(ref, TSEL->step, sizeof ref);
    go_page(GR_ROLL); frame();
    usb.config = 1;
    {
        uint32_t mo = mo_w;
        btn_down(B_SEQ); frame();
        tl_key(1);                                      /* G3: REVERSE */
        ok = ui.layer == LAYER_SEQ && !gates() && mo_w == mo && msg_is("REVERSED");
    }
    usb.config = 0;
    ok &= TSEL->step[6].time == ST_NOTE && TSEL->step[6].note[0] == 60 && TSEL->step[7].time == ST_TIE &&
          TSEL->step[4].note[0] == 64 && TSEL->step[2].note[0] == 67 && TSEL->step[0].time == ST_REST &&
          TSEL->step[20].note[0] == 72 && tl_lock_at(4) && !tl_lock_at(3);
    bad += check("  G3 REVERSE: steps 1..LEN mirrored, C4 + its tie as C4 then the tie, the lock moves with E4", ok);
    btn_up(B_SEQ); frame();
    motion_reset(TSEL);                                 /* (no undo since TONIC: the melody again) */
    tl_melody();
    memcpy(ref, TSEL->step, sizeof ref);                /* (its rests keep stale note bytes: the new reference) */
    btn_down(B_SEQ); frame();
    tl_key(3);                                          /* B3: SHIFT > */
    ok = TSEL->step[1].note[0] == 60 && TSEL->step[2].time == ST_TIE && TSEL->step[4].note[0] == 64 && tl_lock_at(4) &&
         msg_is("SHIFTED RIGHT");
    tl_key(3);
    tl_key(2); tl_key(2);                               /* A3: SHIFT < twice */
    ok &= !memcmp(TSEL->step, ref, sizeof ref) && tl_lock_at(3);
    TSEL->p[P_SLEN] = 6;
    tl_key(2);
    ok &= TSEL->step[5].note[0] == 60 && TSEL->step[2].note[0] == 64 && TSEL->step[6].time == ST_REST &&
          TSEL->step[20].note[0] == 72;                 /* (inside LEN 6: step 7 and 21 stay) */
    tl_key(3);
    TSEL->p[P_SLEN] = 8;
    ok &= !memcmp(TSEL->step, ref, sizeof ref);
    bad += check("  B3 / A3 SHIFT > <: one step round inside LEN (the lock with it); back again: as it was", ok);
    tl_key(0);                                          /* F3: CLEAR */
    for (ok = msg_is("T1 CLEARED") || msg_is("T2 CLEARED") || msg_is("T3 CLEARED") || msg_is("T4 CLEARED"), i = 0; i < 8u; i++)
        ok &= TSEL->step[i].time == ST_REST && !TSEL->step[i].n;
    ok &= TSEL->step[20].note[0] == 72 && !motion_lock_count(TSEL);
    bad += check("  F3 CLEAR: steps 1..LEN empty (no dialog), past LEN kept, the locks of the steps cleared go", ok);
    tl_melody();                                        /* (JIANT 0.6.8) F4 CLR AUTO: the automation, the steps stay */
    memcpy(ref, TSEL->step, sizeof ref);
    (void)motion_set_event(TSEL, 1, P_DIST, 40); (void)motion_set_event(TSEL, 9, P_REV, 20);
    (void)motion_set_event(&trk[1], 2, P_DIST, 30);
    a = leds_at(0); b2 = leds_at(250);
    ok = ((a ^ b2) >> white(7)) & 1u;                   /* (F4 breathes: automation there) */
    tl_key(7);
    ok &= msg_is("AUTOMATION CLEARED") && !motion_count(TSEL) && motion_count(&trk[1]) == 1u &&
         !memcmp(TSEL->step, ref, sizeof ref) && ui.layer == LAYER_SEQ;
    tl_key(7);
    a = leds_at(0); b2 = leds_at(250);
    ok &= msg_is("NOTHING TO CLEAR") && !(((a | b2) >> white(7)) & 1u);
    motion_reset(&trk[1]);
    bad += check("  F4 CLR AUTO (breathes with automation): the track's automation and locks gone, its steps and the other tracks' stay", ok);
    tl_key(0);
    oct_back();
    ok = TSEL->step[0].time == ST_REST && !msg_is("SEQUENCE PUT BACK") && ui.layer == LAYER_SEQ && song.octave == 0;
    turn(EN_K1, -2);
    ok &= TSEL->p[P_SLEN] == 6;                         /* KNOB 1: LEN (the PATTERN page's four) */
    oct_back();
    ok &= TSEL->p[P_SLEN] == 6;
    btn_up(B_SEQ); frame();
    bad += check("  OCT-: nothing put back (TONIC: no undo), no octave; KNOB 1..4 LEN DIV SWING GATE", ok);

    /* RANDOM / COOK on a melodic track (fixed seeds): the rhythm stays, the notes in the scale, near the middle */
    ok = okc = 1;
    for (seed = 1; seed <= 40u; seed++) {
        ui_power_on();
        tl_melody();
        TSEL->p[P_ROOT] = 2; TSEL->p[P_SCALE] = 2;
        memcpy(ref, TSEL->step, sizeof ref);
        go_page(GR_ROLL); frame();
        rng_state = 0x9E3779B9u * seed;
        btn_down(B_SEQ); frame();
        tl_key(4);                                      /* C4: RANDOM */
        ok &= msg_is("RANDOM NOTES") && tl_times_are(TSEL, ref, NSTEP) && tl_in_key(TSEL) && TSEL->step[20].note[0] == 72;
        for (i = 0; i < 8u; i++)
            if (TSEL->step[i].n)
                ok &= TSEL->step[i].note[0] >= 54 && TSEL->step[i].note[0] <= 72;
        for (k = 0; k < 12u; k++) {                     /* D4: COOK, again and again */
            memcpy(mid, TSEL->step, sizeof mid);
            tl_key(5);
            okc &= tl_times_are(TSEL, ref, NSTEP) && tl_in_key(TSEL) && TSEL->step[20].note[0] == 72 &&
                   memcmp(mid, TSEL->step, sizeof mid) != 0;
        }
        okc &= msg_is("COOKED 12");
        btn_up(B_SEQ); frame();
    }
    bad += check("  C4 RANDOM (melodic): the rhythm kept, new notes in the scale near the middle (40 seeds)", ok);
    bad += check("  D4 COOK x12: each press changes it, the rhythm and the scale kept, \"COOKED 12\"", okc);
    ui_power_on();
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = 16;
    go_page(GR_ROLL); frame();
    rng_state = 12345u;
    btn_down(B_SEQ); frame();
    tl_key(5);
    ok = msg_is("NOTHING TO COOK");
    tl_key(4);
    for (k = 0, i = 0; i < 16u; i++) {
        ok &= i % 2u ? TSEL->step[i].time == ST_REST : 1;
        k += (uint32_t)tl_gate(&TSEL->step[i]);
    }
    ok &= tl_gate(&TSEL->step[0]) && k >= 3u && tl_in_key(TSEL);
    btn_up(B_SEQ); frame();
    bad += check("  an empty pattern: COOK says NOTHING TO COOK; RANDOM writes 8ths with rests, in the scale", ok);

    /* DRUM: RANDOM (kick 3..5 with one on step 1, snare 2 .. 3 on 2 and 4, closed hat 8..12), COOK keeps the 1 */
    ok = okc = 1;
    for (seed = 1; seed <= 60u; seed++) {
        ui_power_on();
        tl_drum();
        go_page(GR_ROLL); frame();
        rng_state = 0x2545F491u * seed;
        btn_down(B_SEQ); frame();
        tl_key(4);
        a = tl_count(TSEL, DV_KICK); b2 = tl_count(TSEL, DV_SNARE);
        ok &= msg_is("RANDOM BEAT") && a >= 3u && a <= 5u && (tl_lanes_of(TSEL, DV_KICK) & 1u) && b2 >= 2u && b2 <= 3u &&
              (tl_lanes_of(TSEL, DV_SNARE) & 0x1010u) == 0x1010u && tl_count(TSEL, DV_HATC) >= 8u &&
              tl_count(TSEL, DV_HATC) <= 12u;
        for (k = 0; k < 10u; k++) {
            memcpy(mid, TSEL->step, sizeof mid);
            tl_key(5);
            okc &= (tl_lanes_of(TSEL, DV_KICK) & 1u) && memcmp(mid, TSEL->step, sizeof mid) != 0 && tl_count(TSEL, DV_HATC);
        }
        btn_up(B_SEQ); frame();
    }
    bad += check("  DRUM RANDOM: kick 3..5 (one on 1), snare on 2 and 4 (+ a ghost), hat 8..12; 60 seeds", ok);
    bad += check("  DRUM COOK x10: each press changes it, the kick on 1 stays", okc);
    ui_power_on();
    tl_drum();
    go_page(GR_ROLL); frame();
    btn_down(B_SEQ); frame();
    tl_key(6);                                          /* E4: BEAT */
    ok = msg_is("BASIC BEAT") && tl_lanes_of(TSEL, DV_KICK) == 0x1111u && tl_lanes_of(TSEL, DV_SNARE) == 0x1010u &&
         tl_lanes_of(TSEL, DV_HATC) == 0x5555u;
    key_down(black(2)); frame(); key_up(black(2)); frame();   /* black key 3: the CLAP lane */
    ok &= ui.lane == DV_CLAP && !gates();
    tl_key(10);                                         /* B4: FILL, four times round */
    ok &= tl_lanes_of(TSEL, DV_CLAP) == 0xFFFFu && msg_is("CLAP EVERY STEP");
    tl_key(10);
    ok &= tl_lanes_of(TSEL, DV_CLAP) == 0x5555u;
    tl_key(10);
    ok &= tl_lanes_of(TSEL, DV_CLAP) == 0x1111u;
    tl_key(10);
    ok &= !tl_lanes_of(TSEL, DV_CLAP) && msg_is("CLAP CLEARED") && tl_lanes_of(TSEL, DV_KICK) == 0x1111u;
    bad += check("  E4 BEAT: four on the floor, 2 and 4, 8th hats; black key 3 the lane, B4 FILL 1 / 2 / 4 / none", ok);
    ui.lane = DV_SNARE;
    grid_acc(TSEL, 4, DV_SNARE, 1);
    tl_key(9);                                          /* A4: REVERSE the lane */
    ok = tl_lanes_of(TSEL, DV_SNARE) == 0x0808u && ((step_accents(&TSEL->step[11]) >> DV_SNARE) & 1u) &&
         !((step_accents(&TSEL->step[3]) >> DV_SNARE) & 1u) && tl_lanes_of(TSEL, DV_KICK) == 0x1111u;
    tl_key(8);                                          /* G4: CLEAR the lane */
    ok &= !tl_lanes_of(TSEL, DV_SNARE) && tl_lanes_of(TSEL, DV_HATC) == 0x5555u && msg_is("SNARE CLEARED");
    ui.lane = DV_TOM;
    rng_state = 77u;
    tl_key(11);                                         /* C5: RANDOM the lane: at least one hit */
    ok &= tl_count(TSEL, DV_TOM) >= 1u && tl_lanes_of(TSEL, DV_KICK) == 0x1111u;
    bad += check("  A4 / G4 / C5: the lane reversed (its accents with it), cleared, at random; the others stay", ok);
    a = leds_at(0); b2 = leds_at(250);
    ok = ((a ^ b2) >> white(0)) & 1u && ((a ^ b2) >> white(11)) & 1u && !(((a | b2) >> white(7)) & 1u) &&
         ((a & b2) >> black(DV_TOM)) & 1u && ((a ^ b2) >> black(0)) & 1u && !(((a | b2) >> black(8)) & 1u);
    btn_up(B_SEQ); frame();
    ui_power_on();
    tl_melody();
    go_page(GR_ROLL); frame();
    btn_down(B_SEQ); frames(500);                       /* (the map: a peek) */
    memcpy(ref, TSEL->step, sizeof ref);
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a ^ b2) >> white(5)) & 1u && !(((a | b2) >> white(6)) & 1u) && !(((a | b2) >> black(0)) & 1u);
    tl_key(6); tl_key(10);
    key_down(black(2)); frame(); key_up(black(2)); frame();
    ok &= !memcmp(TSEL->step, ref, sizeof ref) && ui.lane == 0u && !gates();
    btn_up(B_SEQ); frame();
    bad += check("  LEDs: the tools breathe, the lane lit (DRUM); a melodic track: no BEAT, no lane keys", ok);

    /* a song playing */
    ui_power_on();
    tl_melody();
    go_page(GR_ROLL); frame();
    chain.running = 1;
    memcpy(ref, TSEL->step, sizeof ref);
    btn_down(B_SEQ); frame();
    tl_key(4);
    ok = msg_is("STOP TO EDIT") && !memcmp(TSEL->step, ref, sizeof ref);
    btn_up(B_SEQ); frame();
    chain.running = 0;
    bad += check("  while a song plays: STOP TO EDIT, nothing changes", ok);
    return bad;
}


/* JIANT: the song layer (SAVE held, ui_layer.c song_key; song_chain.c): sections A..D on the next bar (stopped: at once),
 * STORE (RAM at once, flash once stopped), RECALL, a quick chain, SONG REC, LOOP / SONG; SAVE tapped: its pages */
static void sl_play(uint32_t samples)               /* the audio and the main loop, as on the device */
{
    uint32_t done;
    for (done = 0; done < samples; done += 128u) {
        events_block(128u);
        if (!(done & 1023u))
            song_poll();
    }
    song_poll();
}
static void sl_steps(uint32_t base)                 /* every track: step 1 holds note base + track */
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        track_defaults_steps(&trk[i]);
        trk[i].p[P_SLEN] = 16;
        trk[i].step[0] = (step_t){{(uint8_t)(base + i), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
    }
}
static void sl_key(uint32_t w)                      /* SAVE held, a white key tapped, SAVE let go */
{
    btn_down(B_SAVE); frame();
    key_down(white(w)); frame(); key_up(white(w)); frame();
    btn_up(B_SAVE); frame();
}
/* JIANT: EDIT held on a DRUM track: black keys 1..8 mute each DRUM-X sound (lit = sounding); saved with the section */
static int test_lane_mutes(void)
{
    int bad = 0, ok;
    uint32_t a, b2;
    project_t p;
    ui_power_on();
    set_engine_of(TSEL, ENGI_DRUM); go_home(); frame();
    dx_mute_set(0);
    btn_down(B_EDIT); frame(); key_down(black(2)); frame(); key_up(black(2));
    a = leds_at(0); b2 = leds_at(250);
    ok = (dx_mute & DXM_LANE(2)) && drum_muted(39) && !drum_muted(38) && !((a | b2) >> black(2) & 1u) &&
         ((a & b2) >> black(1)) & 1u;
    btn_up(B_EDIT); frame();
    project_capture(&p);
    ok &= (p.dx_mute & DXM_LANE(2)) != 0;
    lay_combo(B_EDIT, black(2)); key_up(black(2)); btn_up(B_EDIT); frame();
    ok &= !dx_mute;
    bad += check("EDIT + black keys 1..8 on DRUM: each sound muted (CP here), lit = sounding; in the section", ok);
    return bad;
}

/* JIANT: SEQ + REC held: CLEAR ALL SEQUENCES? (every track's steps and automation, the punch-in lane); OCT+ clears.
 * Neither SEQ's tap (SONG) nor REC's (arm) happens */
static int test_clear_all(void)
{
    int bad = 0, ok;
    uint32_t k, any = 0;
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        my_steps(&trk[k]);
    pfx_lane[3] = 0x22;
    go_home(); frame();
    btn_down(B_SEQ); frame();
    btn_down(B_REC); frame();
    ok = ui.confirm == CF_CLEAR_ALL;
    btn_up(B_REC); frame();
    btn_up(B_SEQ); frame();
    ok &= ui.confirm == CF_CLEAR_ALL && !song.rec && ui.home;
    press(B_OCTUP);
    for (k = 0; k < NTRK; k++)
        any |= !seq_is_empty(&trk[k]);
    bad += check("SEQ + REC: CLEAR ALL SEQUENCES? (no SONG page, REC not armed); OCT+: every track empty, the lane too",
                 ok && !ui.confirm && !any && !pfx_lane[3] && msg_is("ALL CLEARED"));
    return bad;
}

/* JIANT: EDIT > SOUND / SOUND 2 (ui_dx.c), the DRUM-X kit of the section */
static int test_dx_sound(void)
{
    int bad = 0, ok;
    uint32_t k, seen = 0;
    ui_power_on();
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);         /* (earlier tests may have turned SOUND's knobs) */
    set_engine_of(TSEL, ENGI_DRUM);
    open_family(FAM_EDIT);
    for (k = 0; k < 12u; k++) { seen |= cur_page()->graph == GR_DXSND; open_family(FAM_EDIT); }
    ok = seen;
    set_engine_of(TSEL, 0);
    open_family(FAM_EDIT);
    for (seen = 0, k = 0; k < 12u; k++) { seen |= cur_page()->graph == GR_DXSND; open_family(FAM_EDIT); }
    ok &= !seen;
    set_engine_of(TSEL, ENGI_DRUM);
    bad += check("EDIT > SOUND / SOUND 2: on a DRUM track only", ok);
    go_title("SOUND");
    dx_ui_lane = 0; dx_ui_side = 0; dx_ui_seen = last_note;   /* (no key since: the page keeps its lane) */
    turn(EN_K1, 1); turn(EN_K1, 1);                    /* LANE: CP */
    turn(EN_K2, 1);                                    /* SIDE: B */
    k = dx_kit[2].b[DXP_PITCH];
    turn(EN_K3, 3);
    ok = dx_ui_lane == 2u && dx_ui_side == 1u && dx_kit[2].b[DXP_PITCH] > k && dx_kit[2].a[DXP_PITCH] == DX_KIT_DEF[2].a[DXP_PITCH];
    bad += check("  SOUND: LANE, SIDE, PTCH edit that sound's patch (the other patch stays)", ok);
    go_title("SOUND 3");                               /* (0.6.54: MODE on SOUND 3's KNOB 2, KNOB 1 the sound) */
    turn(EN_K2, 1);
    ok = dx_kit[2].mode != DX_KIT_DEF[2].mode && ((dx_kit[2].mode >> 2) & 3u) <= DXF_HP && dx_ui_lane == 2u;
    go_title("SOUND 2");
    turn(EN_K2, -5);
    ok &= dx_kit[2].b[DXP_DECAY] < DX_KIT_DEF[2].b[DXP_DECAY];
    bad += check("  SOUND 3: MODE steps the wave / filter / SNAP; SOUND 2: DECAY the side's decay", ok);
    key_down(key_at(1, 3)); frame(); key_up(key_at(1, 3)); frame(); frame();   /* a key on the track: its lane */
    ok = dx_ui_lane == drum_lane(last_note);
    bad += check("  a key struck picks the sound it plays", ok);
    project_save(0);
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    project_load(0);
    bad += check("  the kit is the section's: saved and loaded with it", dx_kit[2].b[DXP_PITCH] > DX_KIT_DEF[2].b[DXP_PITCH]);
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    ui_power_on();
    return bad;
}

static int test_song_layer(void)
{
    int bad = 0, ok;
    uint32_t bar, i;
    ui_power_on();
    if (song_cur) (void)song_select(0);                 /* (song 1: the page sweeps above may have opened another) */
    sl_steps(60); project_save(0);                      /* A: notes 60.. */
    sl_steps(70); project_save(1);                      /* B: notes 70.. */
    sl_steps(80);                                       /* the music now: 80.. */
    btn_down(B_SAVE); frames(800);
    ok = ui.layer == LAYER_SAVE && str_eq(layer_head(), "[SAVE] SONG");
    btn_up(B_SAVE); frame();
    ok &= ui.home && !ui.layer;
    press(B_SAVE);
    ok &= !ui.home && cur_page()->fam == FAM_SAVE;      /* (a tap: the SAVE pages, as before) */
    bad += check("SONG LAYER: SAVE held opens it ([SAVE] SONG); SAVE tapped opens the SAVE pages", ok);
    go_home();
    sl_key(0);                                          /* F3: section A, stopped: at once */
    bad += check("  stopped: F3 loads section A at once (its steps, the song's rows kept)",
                 live.cur == 0 && trk[0].step[0].note[0] == 60 && trk[3].step[0].note[0] == 63 && !song.playing);
    sl_key(2);                                          /* A3: section C is empty */
    bad += check("  an empty section: it says so, nothing changes", msg_is("C IS EMPTY") && live.cur == 0);
    transport_req = 1; sl_play(256);
    bar = chain_bar();
    sl_key(1);                                          /* G3: section B on the next bar */
    ok = live.req == 1 && sec_stage.ready && sec_stage.row == SEC_LIVE && trk[0].step[0].note[0] == 60;
    sl_play(bar);
    ok &= live.cur == 1 && live.req < 0 && trk[0].step[0].note[0] == 70 && song.playing;
    for (i = 0; i < NTRK; i++)
        ok &= trk[i].seq_idx < 2u;                      /* (every track from its step 0, on the bar) */
    bad += check("  playing: G3 asks for B, it goes in on the next bar, every track from step 0", ok);
    trk[0].step[0].note[0] = 99;                        /* an edit of what plays */
    btn_down(B_SAVE); frame(); press(B_OCTUP); btn_up(B_SAVE); frame();
    ok = (live.dirty & 2u) && live.cur == 1 && msg_is("STORED B");
    project_load(1);                                    /* (stopped by it: the slot holds the stored edit) */
    ok &= trk[0].step[0].note[0] == 99 && !song.playing;
    song_poll();
    ok &= !FELUCCA_FLASH || !(live.dirty & 2u);         /* (stopped: in flash too; no flash: RAM only) */
    bad += check("  SAVE + OCT+ stores the music into the section playing (RAM at once, flash once stopped)", ok);
    transport_req = 1; sl_play(256);
    live.cur = 1;
    trk[0].step[0].note[0] = 55;                        /* played with, then RECALL */
    btn_down(B_SAVE); frame(); press(B_OCTDN); btn_up(B_SAVE); frame();
    ok = live.req == 1;
    sl_play(bar);
    bad += check("  SAVE + OCT- RECALLs the section as stored, on the next bar", ok && trk[0].step[0].note[0] == 99);
    btn_down(B_SAVE); frame();                          /* a quick chain: A then B in one hold */
    key_down(white(0)); frame(); key_up(white(0)); frame();
    key_down(white(1)); frame(); key_up(white(1)); frame();
    ok = msg_is("CHAIN A B");
    btn_up(B_SAVE); frame();
    ok &= chain.armed_bar && chain.loop && chain.config.count == 2 && chain.config.row[0].slot == 0 &&
          chain.config.row[0].bars == 1;                /* (a 16-step pattern of 1/16: one bar) */
    sl_play(bar);
    ok &= chain.running && chain.row == 0 && trk[0].step[0].note[0] == 60;
    sl_play(bar);
    ok &= chain.row == 1 && trk[0].step[0].note[0] == 99;
    sl_play(bar);
    ok &= chain.row == 0 && trk[0].step[0].note[0] == 60;
    bad += check("  two sections tapped in one hold: a quick chain, each its pattern's bars, round and round", ok);
    sl_key(1);                                          /* a single section ends the chain */
    sl_play(bar);
    bad += check("  a single section tapped ends the quick chain (that section on the next bar)",
                 !chain.running && live.cur == 1 && trk[0].step[0].note[0] == 99);
    sl_key(13);                                         /* E5: SONG REC, from the next bar */
    ok = live.srec == 1u;
    sl_play(bar);                                       /* B: recording from here */
    ok &= live.srec == 2u;
    sl_play(bar);                                       /* B, one bar */
    sl_key(0);                                          /* A on the next bar */
    sl_play(bar);
    sl_play(bar / 2u);                                  /* A, half a bar */
    transport_req = 2; sl_play(128);                    /* STOP: the begun bar counts */
    ok &= !live.srec && chain_config.count == 2 && chain_config.row[0].slot == 1 && chain_config.row[0].bars == 2 &&
          chain_config.row[1].slot == 0 && chain_config.row[1].bars == 1 && msg_is("SONG ROWS 2");
    bad += check("  E5 SONG REC: the sections played and their bars become the song's rows (STOP: the begun bar counts)",
                 ok);
    sl_key(12);                                         /* D5: SONG */
    ok = live.mode == 1u;
    sl_steps(80);
    press(B_PLAY); sl_play(256);
    ok &= chain.running && !chain.loop && chain.row == 0 && trk[0].step[0].note[0] == 99;
    transport_req = 2; sl_play(128);
    ok &= trk[0].step[0].note[0] == 80;                 /* (the music before the song back) */
    sl_key(12);
    ok &= !live.mode;
    bad += check("  D5 LOOP / SONG: in SONG, PLAY plays the song from any page; the music back after it", ok);
    chain_defaults(&song_idx.rows[1]);                  /* (song 2 empty: the page sweeps above may have opened it) */
    btn_down(B_SAVE); frame();                          /* KNOB 1: song 2, in when SAVE lets go */
    turn(EN_K1, 1); frame();
    ok = song_cur == 0 && lys.song == 2u;
    turn(EN_K1, 1); turn(EN_K1, -1); frame();
    ok &= lys.song == 2u;
    btn_up(B_SAVE); frame();
    ok &= song_cur == 1 && !chain_config.count && msg_is("SONG 2");
    btn_down(B_SAVE); frame(); turn(EN_K1, -1); frame(); btn_up(B_SAVE); frame();
    ok &= song_cur == 0;
    bad += check("  KNOB 1: the song, in when SAVE lets go (stopped)", ok);
    return bad;
}

/* (0.6.12) LOFI BYTE / FLOAT's RTRG: a note on a sounding voice restarts the formula's time (off: it runs on) */
static int test_lofi_rtrg(void)
{
    int bad = 0;
    uint32_t e;
    track_t *t = &trk[0];
    voice_t *v;
    ui_power_on();
    for (e = 0; e < NENGINES && ENGINES[e] != &ENG_LOFI; e++) {}
    set_engine_of(t, e); t->engine = t->eng_req;
    t->p[P_E1] = RW_FLOAT; t->p[P_VOICE] = V_MONO; t->p[P_SUS] = 127;   /* (held: the voice sounding) */
    trk_note_on(t, 60, 100);
    v = &t->v[0];
    v->ph[0] = v->ph[1] = 12345u;                       /* (the formula running: sounding, its time on) */
    trk_note_on(t, 64, 100);
    bad += check("RETRIG: OFF, a note again: FLOAT's time runs on", v->ph[0] == 12345u && v->ph[1] == 12345u);
    t->p[P_LRTRG] = 1;
    v->ph[0] = v->ph[1] = 12345u;
    trk_note_on(t, 62, 100);                            /* (MONO: another key retriggers; the same one held does not) */
    bad += check("  ON: the formula from its start", v->ph[0] == 0u && v->ph[1] == 0u);
    go_title("RETRIG");
    bad += check("  the RETRIG page: LOFI FLOAT's, RTRG on KNOB 1", str_eq(cur_page()->title, "RETRIG") &&
                 str_eq(track_desc(t, P_LRTRG)->label, "RTRG"));
    t->p[P_E1] = 0;
    bad += check("  not on LOFI's other waves", !page_visible(ui.page));
    return bad;
}

/* (0.6.14) LATCH: ARP OFF + HOLD ON, the keys' notes stay when let go; the next chord replaces them; HOLD off ends them */
static int gate_on(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static int test_key_latch(void)
{
    int bad = 0;
    track_t *t;
    uint32_t a, b;
    ui_power_on();
    song.sel = 0; t = &trk[0];
    t->p[P_AMODE] = 0; t->p[P_AHOLD] = 1; t->p[P_SUS] = 127; t->p[P_MUTE] = 0;
    fm1_in.notes = 1u; events_block(CTL); a = kb_note[0];
    fm1_in.notes = 0; events_block(CTL);
    bad += check("LATCH: ARP OFF, HOLD ON: the key let go, its note still sounds", gate_on(t, a));
    fm1_in.notes = 4u; events_block(CTL); b = kb_note[2];
    fm1_in.notes = 0; events_block(CTL);
    t->xf = 0; events_block(CTL);
    bad += check("  the next key: the latched note goes, the new one stays", !gate_on(t, a) && gate_on(t, b));
    t->p[P_AHOLD] = 0; events_block(CTL);
    bad += check("  HOLD off: the latched note goes", !gate_on(t, b));
    fm1_in.notes = 1u; events_block(CTL); fm1_in.notes = 0; events_block(CTL);
    bad += check("  HOLD OFF: a key let go ends its note as ever", !gate_on(t, a));
    t->p[P_AHOLD] = 1;                                  /* (0.6.19) HOLD off with a key still down */
    fm1_in.notes = 1u; events_block(CTL); fm1_in.notes = 0; events_block(CTL);
    fm1_in.notes = 4u; events_block(CTL);               /* (b down: arp_phys 1) */
    t->p[P_AHOLD] = 0; events_block(CTL);
    fm1_in.notes = 0; events_block(CTL);
    bad += check("  HOLD off with a key down: the latched notes still go (they droned)", !gate_on(t, a) && !gate_on(t, b));
    t->p[P_AHOLD] = 1;
    fm1_in.notes = 1u; events_block(CTL); fm1_in.notes = 0; events_block(CTL);
    t->p[P_AMODE] = 1; events_block(CTL);               /* ARP turned on */
    t->p[P_AMODE] = 0; t->p[P_AHOLD] = 1;
    fm1_in.notes = 1u; events_block(CTL); fm1_in.notes = 0; events_block(CTL);
    latch_drop_req |= 1u; events_block(CTL);            /* SEQ TOOLS CLEAR's request */
    bad += check("  a clear lets the latched chord go (it was orphaned)", !gate_on(t, a) && !t->nheld && !t->arp_phys);
    t->p[P_AHOLD] = 0; events_block(CTL);
    return bad;
}

/* (0.6.28) a user: "after clearing a sequence, it still randomly triggers notes on all 6 tracks". The punch-in lane's
 * STUTTER / ARP replayed the cleared steps' last notes (and ARP made random fills on a DRUM track with no steps); a clear
 * now ends them and forgets the notes; a pending section jump is called off */
static int test_clear_ghosts(void)
{
    int bad = 0;
    uint32_t k, i, on[NTRK];
    ui_power_on();
    trk_live = NTRK; ui_rec_prefs |= 0x80u;
    for (k = 0; k < NTRK; k++) my_steps(&trk[k]);
    memset(pfx_lane, 0, sizeof pfx_lane);
    pfx_ltgt = 0; pfx_tgt = 0; perf_latched = 0; scene_pfx = 0;
    for (i = 0; i < 64u; i++) pfx_lane_put(i, i < 32u ? PF_S16 - PF_OCTD + 1u : PF_ARP - PF_OCTD + 1u);   /* (STUTTER 1/16,
                                                         * then ARP, on every track) */
    transport_req = 1;
    for (i = 0; i < 2000u; i++) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    for (k = 0; k < NTRK; k++) {                        /* every track cleared, one by one (SAVE > TOOLS CLEAR) */
        song.sel = (uint8_t)k;
        load_begin(&trk[k], LOAD_PAT); track_defaults_steps(&trk[k]); load_end(&trk[k]);
    }
    for (i = 0; i < 4u; i++) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    for (k = 0; k < NTRK; k++) {
        uint32_t v;
        on[k] = 0;
        for (v = 0; v < NVOICE; v++) trk[k].v[v].active = 0;
    }
    for (i = 0; i < 6000u; i++) {                       /* (4 bars and more: the lane plays on) */
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (k = 0; k < NTRK; k++) {
            uint32_t v;
            for (v = 0; v < NVOICE; v++) on[k] += trk[k].v[v].gate;
        }
    }
    for (k = 0, i = 0; k < NTRK; k++) i += on[k];
    bad += check("clear: the punch-in lane repeats nothing of the cleared steps (no note on any of the 6 tracks)", i == 0u);
    transport_req = 2; events_block(CTL);
    memset(pfx_lane, 0, sizeof pfx_lane);
    ui_power_on();
    return bad;
}

/* (0.6.24) FX in sections: a tap the next page, two quick taps the next section; the footer counts within it */
static int test_fx_sections(void)
{
    int bad = 0, ok;
    char ti[20];
    ui_power_on();
    go_title("FX"); frame();
    press(B_FX); frames(600);
    ok = str_eq(cur_page()->title, "DIST");
    press(B_FX); press(B_FX); frames(600);             /* two quick taps from DIST: DLY (0.6.81: the SLICER after CHORUS) */
    ok &= str_eq(cur_page()->title, "DLY");
    press(B_FX); frames(600);
    page_title(ti);
    ok &= str_eq(cur_page()->title, "DLY 2") && str_eq(ti, "DLY 2/3");
    go_title("CHORUS"); frame();
    press(B_FX); press(B_FX); frames(600);
    ok &= str_eq(cur_page()->title, "SLICER");
    go_title("MASTER"); frame();
    press(B_FX); press(B_FX); frames(600);             /* (past MASTER: round to the first) */
    ok &= str_eq(cur_page()->title, "FX");
    page_title(ti);
    ok &= str_eq(ti, "FX 1/3");
    bad += check("FX sections: tap next page, two quick taps next section (TRACK DELAY REVERB CHORUS SLICER MASTER), 'DLY 2/3'", ok);
    ui_power_on();
    return bad;
}

/* (1.0-GW) GEN: SELECT turns its five pages round (SONG, FEEL the new ones); TEMPO from AUTO starts at the song's
 * tempo and the piece takes it; the settings back to AUTO after */
static int test_gen_pages(void)
{
    int bad = 0, ok;
    uint32_t k;
    ui_power_on();
    go_home(); frames(16);
    fm1_ms += 1200u;
    held_turn(B_HOME, 1);
    ok = ui.confirm == CF_DICE_ALL && gp.page == 0u && !gp.view;
    fm1_ms += 1200u;                                    /* (past the turn that opened it) */
    for (k = 1; k <= 4u; k++) { turn(EN_SELECT, 1); ok &= gp.page == k && !gp.view; }
    turn(EN_SELECT, 1); ok &= gp.view == 1u;           /* (TAKES v2: the ring goes on: the reel, the SET page, */
    turn(EN_SELECT, 1); ok &= gp.view == 2u;
    turn(EN_SELECT, 1); ok &= !gp.view && gp.page == 0u;   /* round) */
    turn(EN_SELECT, -1); ok &= gp.view == 2u;
    turn(EN_SELECT, -1); turn(EN_SELECT, -1); ok &= !gp.view && gp.page == 4u;
    turn(EN_K4, 1); ok &= gp.grit == GA_AUTO;          /* (GRIT: AUTO at the top) */
    turn(EN_K4, -1); ok &= gp.grit == 200u;
    turn(EN_K1, 1); ok &= gp.space == 10u;
    bad += check("GEN: SELECT turns the pages (1..5, the reel, the SET page, round), FEEL's knobs (GRIT from AUTO, SPACE)", ok);
    turn(EN_SELECT, -1); ok = gp.page == 3u;           /* SONG */
    turn(EN_K1, -1); ok &= !gp.tempo;                  /* (left of AUTO: AUTO) */
    turn(EN_K1, 1); ok &= gp.tempo == (uint8_t)clamp(song.g[G_BPM], 60, 200);
    turn(EN_K1, 1); ok &= gp.tempo == (uint8_t)clamp(song.g[G_BPM], 60, 200) + 1u;
    turn(EN_K2, 1); turn(EN_K2, 1); ok &= gp.key == 2u;   /* C# */
    press(B_OCTUP); frames(4);
    ok &= song.g[G_BPM] == gp.tempo && gs.root == 1;
    bad += check("GEN: SONG's TEMPO (AUTO, then the song's tempo up) and KEY make the piece", ok);
    gp.tempo = gp.key = gp.scale = gp.length = gp.space = 0; gp.fills = gp.wet = gp.grit = GA_AUTO; gp.page = 0;
    {   /* (1.0-GW) SELECT the next page (the genre kept); (TAKES v2) ALGORITHM new sounds (the seed kept), a take once it
         * rests: stopped, the notes the same and still stopped; playing: two takes one UNDO point (the music before) */
        uint32_t sig0[NTRK], sig1 = 0, same = 1, snd0[NTRK];
        uint16_t sd;
        uint8_t g0;
        gp.page = 2;
        fm1_ms += 1200u;
        g0 = gp.genre;
        turn(EN_SELECT, 1);
        ok = gp.page == 3u && gp.genre == g0;
        press(B_OCTUP); frames(4);                      /* (the piece of these settings: TEMPO, KEY back on AUTO) */
        sd = gp.seed;
        for (k = 0; k < NTRK; k++) { sig0[k] = steps_sig(&trk[k]); snd0[k] = gsnd_sig(&trk[k]); }   /* (the run's UNDO point) */
        turn(EN_ALGO, 1);
        ok &= gp.seed == sd && gp.svar[RL_BASS] == 1u && gp.svar[RL_DRUM] == 1u;
        frames(640);                                    /* (at rest: the take made, stopped) */
        for (k = 0; k < NTRK; k++) ok &= steps_sig(&trk[k]) == sig0[k];
        ok &= !song.playing && !chain_busy() && gp.sig == gen_sig();
        bad += check("GEN: SELECT the next page (genre kept), ALGORITHM new sounds (seed kept): made at rest, the notes the same, still stopped", ok);
        for (k = 0; k < NTRK; k++) sig0[k] = steps_sig(&trk[k]);
        press(B_PLAY);
        for (k = 0; k < 20u; k++) { frame(); events_block(CTL); }   /* (the transport runs with the audio) */
        ok = song.playing || chain_busy();
        turn(EN_ALGO, 1);                               /* (playing: a take once it rests, its UNDO point the music before) */
        play_ms(4000);                                  /* (TAKES v2: on the next bar) */
        ok &= gp.sig == gen_sig() && (song.playing || chain_busy());
        for (k = 0; k < NTRK; k++) sig1 |= gsnd_sig(&trk[k]) != snd0[k];   /* (new sounds) */
        turn(EN_ALGO, 1);                               /* (a second take: the same UNDO point) */
        play_ms(4000);                                  /* (TAKES v2: on the next bar) */
        ok &= gp.sig == gen_sig() && (song.playing || chain_busy());
        transport_req = 2;
        for (k = 0; k < 4u; k++) { frame(); events_block(CTL); }
        ok &= !undo_do();
        for (k = 0; k < NTRK; k++) same &= steps_sig(&trk[k]) == sig0[k] && gsnd_sig(&trk[k]) == snd0[k];
        ok &= same && sig1 != 0u;
        bad += check("GEN: playing, ALGORITHM makes a take once it rests, the music still playing; UNDO after two takes: the music before the first", ok);
    }
    ui_power_on();
    return bad;
}

/* (NOW) the GEN screen's NOW and its TAKES reel: a piece made is NOW (no take); SAVE keeps NOW as a take; PRESETS
 * goes NOW, then the takes (round); OCT+ on a take remakes it exactly (no new take); playing, a scrolled take becomes the
 * music once PRESETS rests; the reel never drops a take (full: SAVE says so); OCT- back, SELECT on, HOME closes */
static void gen_enter(void)                             /* HOME, then the GEN screen (HOME held + SELECT) */
{
    go_home(); frames(16);
    fm1_ms += 1200u;
    held_turn(B_HOME, 1);
    fm1_ms += 1200u;
}
static void sv(void) { press(B_SAVE); frames(16); }     /* SAVE: NOW a take */
static int test_gen_reel(void)
{
    int bad = 0, ok;
    uint32_t k, pr[3];
    uint16_t s0;
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1;
    gen_enter();
    ok = ui.confirm == CF_DICE_ALL && !gp.view;
    press(B_OCTUP); frames(4);
    ok &= reel_n == 0u && now_ok && now_dirty();       /* (a piece: NOW, no take) */
    sv();
    ok &= reel_n == 1u && reel_play == 0u && !now_dirty();
    sv();
    ok &= reel_n == 1u && gpop.on && str_eq(gpop.lbl, "ALREADY SAVED");   /* (the same NOW: not twice) */
    pr[0] = dice_print();
    for (k = 1; k < 3u; k++) { press(B_OCTUP); frames(4); sv(); pr[k] = dice_print(); }
    ok &= reel_n == 3u && reel_play == 2u && reel_on_now && pr[0] != pr[1] && pr[1] != pr[2];
    ok &= reel[2].set.seed == gp.seed && reel[2].bpm == song.g[G_BPM] && reel[2].nl == (uint8_t)trk_live;
    turn(EN_PRESET, -1); frames(32);                    /* (opens it on NOW, no step: nothing cued) */
    ok &= gp.view == 1u && reel_on_now && reel_want < 0;
    turn(EN_PRESET, -1); ok &= !reel_on_now && reel_cur == 2u;   /* (round: the newest before NOW) */
    turn(EN_PRESET, -1); ok &= reel_cur == 1u;
    turn(EN_PRESET, -1); ok &= reel_cur == 0u;
    turn(EN_PRESET, -1); ok &= reel_on_now;            /* (NOW, before the first) */
    turn(EN_PRESET, 1); frames(32); ok &= !reel_on_now && reel_cur == 0u;
    press(B_OCTUP); frames(4);
    ok &= dice_print() == pr[0] && reel_n == 3u && reel_play == 0u && gp.view == 1u;
    bad += check("GEN TAKES: a piece is NOW, SAVE a take (once); PRESETS NOW then the takes (round); OCT+ remakes a take exactly", ok);
    press(B_PLAY);
    for (k = 0; k < 20u; k++) { frame(); events_block(CTL); }
    ok = song.playing || chain_busy();
    turn(EN_PRESET, 1);                                 /* (playing: take 2 once PRESETS rests) */
    ok &= reel_want == 1 && reel_play == 0u;
    play_ms(4000);                                      /* (on the next bar) */
    ok &= dice_print() == pr[1] && reel_play == 1u && reel_n == 3u && (song.playing || chain_busy());
    transport_req = 2;
    for (k = 0; k < 4u; k++) { frame(); events_block(CTL); }
    bad += check("GEN TAKES: playing, a scrolled take becomes the music once PRESETS rests, the music still playing", ok);
    s0 = reel[0].set.seed;
    press(B_OCTDN); frames(32);                         /* (OCT- on the reel: back to the settings, still GEN) */
    ok = !gp.view && ui.confirm == CF_DICE_ALL;
    for (k = 0; k < 40u; k++) { press(B_OCTUP); frames(4); }
    ok &= reel_n == 3u && now_dirty();                  /* (40 pieces: NOW each time, no take) */
    for (k = 0; k < 40u; k++) { press(B_OCTUP); frames(4); sv(); }
    ok &= reel_n == REEL_N && reel[0].set.seed == s0 && gpop.on && str_eq(gpop.lbl, "32 TAKES: THE REEL IS FULL");
    turn(EN_PRESET, 1); ok &= gp.view == 1u;
    turn(EN_SELECT, 1); frames(32);                     /* (SELECT on the reel: the SET page) */
    ok &= gp.view == 2u && ui.confirm == CF_DICE_ALL;
    press(B_HOME); frames(32);                          /* (HOME: the GEN screen closed, from any page) */
    ok &= !ui.confirm;
    bad += check("GEN TAKES: 40 pieces make no take; SAVE fills the reel to 32 and says it is full (none dropped); OCT- back, SELECT on, HOME closes", ok);
    reel_n = 0; reel_cur = 0; reel_play = 0xFF;
    ui_power_on();
    return bad;
}

/* (TAKES v2) a take's own choices: a part made again changes that part only (its notes; every sound the same), a role's
 * sound made again changes that sound only (every note the same), turned back the piece as before; IDs never reused, a
 * saved take knows the take NOW came from, a new seed starts a family; EDIT deletes (a question first), EDIT held every
 * take; NOW is never deleted */
static uint32_t snd_sig(const track_t *t)
{
    uint32_t h = 2166136261u ^ t->eng_req, j;
    for (j = 0; j < P_COUNT; j++) h = (h ^ (uint16_t)t->p[j]) * 16777619u;
    return h;
}
static int test_gen_v2(void)
{
    int bad = 0, ok;
    uint32_t k, st0[NTRK], sn0[NTRK], pr0, dr, ld, ch_st = 0, ch_sn = 0;
    uint16_t id0, id1;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 0;          /* TRAP ATL, a loop */
    press(B_OCTUP); frames(4);
    sv();
    for (k = 0; k < NTRK; k++) { st0[k] = steps_sig(&trk[k]); sn0[k] = snd_sig(&trk[k]); }
    pr0 = dice_print();
    dr = gs.trk_of[RL_DRUM]; ld = gs.trk_of[RL_LEAD];
    id0 = reel[reel_play].id;
    ok = reel_n == 1u && reel[0].parent == 0u && id0 != 0u;
    gp.var[SL_DRUMS] = 1;                               /* new drums */
    gen_how = GH_DRUMS; gen_go(); gp.sig = gen_sig();
    for (k = 0; k < NTRK; k++) {
        ch_st |= (uint32_t)(steps_sig(&trk[k]) != st0[k]) << k;
        ch_sn |= (uint32_t)(snd_sig(&trk[k]) != sn0[k]) << k;
    }
    ok &= ((ch_st >> dr) & 1u) && !((ch_st >> ld) & 1u) && !ch_sn;
    ok &= reel_n == 1u && now_t.how == GH_DRUMS && now_dirty();
    now_save(); gpop.on = 0;
    ok &= reel_n == 2u && reel[1].parent == id0 && reel[1].how == GH_DRUMS && reel[1].id != id0;
    bad += check("GEN v2: new DRUMS: the drum part's notes change, the lead's don't, no sound changes; saved: a child of the take", ok);
    gp.var[SL_DRUMS] = 0;
    gp.svar[RL_LEAD] = 1;                               /* the lead's sound made again */
    gen_how = GH_SLEAD; gen_go(); gp.sig = gen_sig();
    ch_st = ch_sn = 0;
    for (k = 0; k < NTRK; k++) {
        ch_st |= (uint32_t)(steps_sig(&trk[k]) != st0[k]) << k;
        ch_sn |= (uint32_t)(snd_sig(&trk[k]) != sn0[k]) << k;
    }
    ok = !ch_st && ((ch_sn >> ld) & 1u) && !((ch_sn >> dr) & 1u);
    now_save(); gpop.on = 0;
    bad += check("GEN v2: the lead's sound made again: only its sound changes, every note the same", ok);
    gp.svar[RL_LEAD] = 0;
    gen_how = GH_SET; gen_go(); gp.sig = gen_sig();     /* (back: the first take, not a new one) */
    ok = dice_print() == pr0 && reel_n == 3u && reel_play == 0u && !now_dirty();
    bad += check("GEN v2: the variations turned back: the piece as first made, NOW its take (no new one)", ok);
    id1 = reel[2].id;
    reel_cur = 2; reel_on_now = 0; gp.view = 1;
    press(B_EDIT); frames(32);                          /* (a question) */
    ok = reel_ask == 1u && reel_n == 3u;
    press(B_OCTDN); frames(32);                         /* (no) */
    ok &= !reel_ask && reel_n == 3u && ui.confirm == CF_DICE_ALL;
    press(B_EDIT); press(B_OCTUP); frames(32);          /* (yes) */
    ok &= reel_n == 2u && reel_find(id1) < 0 && reel[0].id == id0 && reel_find(reel[1].id) == 1;
    bad += check("GEN v2: EDIT on the reel: a question (OCT- no, OCT+ yes), the take gone, the others' IDs as they were", ok);
    reel_on_now = 1;
    press(B_EDIT); frames(32);
    ok = !reel_ask && reel_n == 2u;                     /* (NOW: nothing to delete) */
    hold(B_EDIT); frames(32);
    ok &= reel_ask == 2u;
    press(B_OCTUP); frames(32);
    ok &= reel_n == 0u && reel_on_now && now_ok;
    bad += check("GEN v2: EDIT on NOW: nothing deleted; EDIT held: every take deleted (a question first), NOW stays", ok);
    gp.view = 0; gp.sig = gen_sig();
    press(B_OCTUP); frames(4);                          /* (the same settings: a new seed, a family of its own) */
    sv();
    ok = reel_n == 1u && reel[0].parent == 0u && reel[0].id > id1 && gp.seed != 202u;
    bad += check("GEN v2: IDs never reused, a new seed a family's first take (no parent)", ok);
    reel_n = 0; reel_cur = 0; reel_play = 0xFF;
    ui_rec_prefs &= (uint8_t)~(PREF_SIX >> 8); rp_apply();
    ui_power_on();
    return bad;
}

/* (TAKES v2, NOW) OCT+ held: the keys silent and the layer's: a part's key makes NOW again with that part new (stopped:
 * at once), held half a second it locks the part instead; a black key the part's sound; playing, a change waits for the
 * next bar and the loop starts again from its top; PRESETS with OCT+ held only cues (nothing loads), OCT+ let go loads
 * the cue on the next bar; on another take's card a part key borrows it (a pin); a locked part goes with a new seed */
static void to_reel(void)                               /* (TAKES v2) SELECT round the GEN screen's ring to the reel */
{
    uint32_t i;
    for (i = 0; i < 8u && gp.view != 1u; i++) turn(EN_SELECT, 1);
}
static void gl_key(uint32_t k, uint32_t ms)              /* OCT+ held: key k down for ms, let go (OCT+ stays) */
{
    key_down(k); frame();
    frames(ms);
    key_up(k); frame();
}
static int test_gen_layer(void)
{
    int bad = 0, ok;
    uint32_t k, st0[NTRK], sn0[NTRK], dr, ld;
    uint16_t id0;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 0;          /* TRAP ATL, a loop */
    press(B_OCTUP); frames(4);
    sv();
    to_reel(); frames(32);                              /* (the reel, on NOW) */
    for (k = 0; k < NTRK; k++) { st0[k] = steps_sig(&trk[k]); sn0[k] = gsnd_sig(&trk[k]); }
    dr = gs.trk_of[RL_DRUM]; ld = gs.trk_of[RL_LEAD];
    id0 = reel[reel_play].id;
    btn_down(B_OCTUP); frame();
    key_down(0); frame();
    ok = kb_note[0] == KB_SILENT;                       /* F3: silent while it is down, */
    frames(50); key_up(0); frame();                     /* new DRUMS when let go: NOW, no take */
    ok &= gl.held && reel_n == 1u && now_t.parent == id0 && now_t.how == GH_DRUMS && now_dirty();
    ok &= steps_sig(&trk[dr]) != st0[dr] && steps_sig(&trk[ld]) == st0[ld] && gsnd_sig(&trk[ld]) == sn0[ld];
    bad += check("GEN layer: OCT+ held on the reel: a key silent, F3 NOW with new DRUMS (stopped: at once), no take", ok);
    gl_key(8, 50);                                      /* C#4: the lead's sound */
    ok = reel_n == 1u && now_t.how == GH_SLEAD && gsnd_sig(&trk[ld]) != sn0[ld] && steps_sig(&trk[ld]) == st0[ld];
    gl_key(2, 700);                                     /* G3 held: BASS locked */
    ok &= ((gp.lock >> SL_BASS) & 1u) && reel_n == 1u;
    btn_up(B_OCTUP); frame(); frame();
    ok &= reel_n == 1u && !gl.held && reel_want == -1;  /* (its keys acted: OCT+ let go loads nothing) */
    bad += check("GEN layer: a black key the part's sound (the notes kept), a part key held locks it, OCT+ let go after keys: nothing more", ok);
    press(B_PLAY);
    play_ms(200);
    btn_down(B_OCTUP); frame();
    gl_key(11, 50);                                     /* E4: every sound, on the next bar */
    ok = gen_take_ms && reel_n == 1u && (song.playing || chain_busy());
    btn_up(B_OCTUP); frame();
    play_ms(4000);
    ok &= now_t.how == GH_SOUNDS && !gen_take_ms && song.playing && reel_n == 1u;
    bad += check("GEN layer: playing, E4 new sounds wait for the next bar, the music playing on", ok);
    sv();                                               /* (take 2: NOW with its new drums and sounds) */
    btn_down(B_OCTUP); frame();
    turn(EN_PRESET, -1); turn(EN_PRESET, -1); frames(600);   /* (NOW, take 2, take 1: a cue only) */
    ok = reel_n == 2u && !reel_on_now && reel_cur == 0u && reel_play == 1u && !gen_take_ms;
    btn_up(B_OCTUP); frame();
    ok &= gen_take_ms && reel_want == 0;
    play_ms(4000);
    ok &= reel_play == 0u && song.playing && reel_n == 2u;
    bad += check("GEN layer: PRESETS with OCT+ held cues (nothing loads), OCT+ let go: the cue on the next bar", ok);
    transport_req = 2; play_ms(100);
    gp.seed = 4242; gen_fresh_recipe(); gen_how = GH_NEW; now_from = 0; gen_go(); gp.sig = gen_sig();   /* (another family) */
    now_save(); gpop.on = 0;
    ok = reel_n == 3u && reel[2].parent == 0u;
    reel_cur = 1; reel_on_now = 0;                      /* (the cursor on take 2: seed 202, new drums) */
    btn_down(B_OCTUP); frame();
    gl_key(0, 50);                                      /* F3 on another take's card: its drums borrowed */
    btn_up(B_OCTUP); frame();
    ok &= reel_n == 3u && now_t.how == GH_BORROW && gpinned(1u + SL_DRUMS) && gp.seed == 4242u;
    for (k = 0; k < GP_PINS; k++)
        if (gp.pin[k].what == 1u + SL_DRUMS) ok &= gp.pin[k].seed == 202u && gp.pin[k].var == 1u;
    bad += check("GEN layer: on another take's card a part key borrows its part (its seed and variation pinned)", ok);
    gp.lock = (uint16_t)(1u << SL_BASS);
    gp.view = 0; gp.sig = gen_sig();
    press(B_OCTUP); frames(4);                          /* (the same settings: a new seed, BASS locked) */
    ok = gp.seed != 4242u && gpinned(1u + SL_BASS) && !gpinned(1u + SL_DRUMS);
    for (k = 0; k < GP_PINS; k++)
        if (gp.pin[k].what == 1u + SL_BASS) ok &= gp.pin[k].seed == 4242u;
    bad += check("GEN layer: a new seed keeps a locked part (pinned to the take's seed), drops the borrowed ones", ok);
    gp.lock = 0;
    reel_n = 0; reel_cur = 0; reel_play = 0xFF;
    ui_rec_prefs &= (uint8_t)~(PREF_SIX >> 8); rp_apply();
    ui_power_on();
    return bad;
}

/* (TAKES v2, NOW) a song's sections: KNOB 4 on the reel picks one, a part key then makes that part again there only
 * (the hook, A, as it was); A#4 another arrangement (the song's rows); JAM changes NOW by itself every few bars (no take);
 * the SET: REC while playing records the takes launched (OCT- + a white key) and their bars, the SET page plays them in a
 * row, KNOB 1 while it plays jumps to an entry on the next bar */
static uint32_t sec_print(uint32_t s, uint32_t role)     /* a section's part, hashed (section A back after) */
{
    uint32_t h = section_load(s) ? 0u : steps_sig(&trk[gs.trk_of[role]]);
    section_load(0);
    return h;
}
static void launch(uint32_t white_i)                    /* OCT- held + white key i: take i (OCT- let go after) */
{
    btn_down(B_OCTDN); frame();
    key_down(white(white_i)); frame(); key_up(white(white_i)); frame();
    btn_up(B_OCTDN); frame();
}
static int test_gen_song_takes(void)
{
    int bad = 0, ok;
    uint32_t a0, b0, c0, r0[CHAIN_ROWS], n0, k;
    uint16_t idA, idB;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 2;          /* TRAP ATL, a SONG */
    press(B_OCTUP); frames(4);
    to_reel(); frames(32);                              /* (the reel, on NOW) */
    a0 = sec_print(0, RL_PAD); b0 = sec_print(1, RL_PAD); c0 = sec_print(2, RL_PAD);
    turn(EN_K4, 1);                                     /* (KNOB 4, section B: the verse) */
    ok = gl.focus == 1u;
    btn_down(B_OCTUP); frame();
    gl_key(4, 50);                                      /* A3: the chords (the pad), in the verse only */
    btn_up(B_OCTUP); frame();
    ok &= now_t.how == GH_SECTION && gp.secvar[0][SL_CHORDS - SL_DRUMS] == 1u;
    ok &= sec_print(0, RL_PAD) == a0 && sec_print(1, RL_PAD) != b0 && sec_print(2, RL_PAD) == c0;
    bad += check("GEN song takes: KNOB 4 picks section B, a part key makes that part again there only (A and C as they were)", ok);
    turn(EN_K4, -1);
    n0 = chain_config.count;
    for (k = 0; k < n0; k++) r0[k] = chain_config.row[k].slot | (uint32_t)chain_config.row[k].bars << 8;
    btn_down(B_OCTUP); frame();
    gl_key(17, 50);                                     /* A#4: the next arrangement */
    btn_up(B_OCTUP); frame();
    ok = gp.arr == 1u && now_t.how == GH_FORM && live.mode && chain_config.row[0].slot == 0u;
    for (k = 0, c0 = 0; k < n0 && k < chain_config.count; k++) c0 |= (r0[k] != (chain_config.row[k].slot | (uint32_t)chain_config.row[k].bars << 8));
    ok &= c0 && sec_print(0, RL_PAD) == a0;
    bad += check("GEN song takes: A#4 another arrangement (hook first: the song's rows), the hook the same", ok);
    gp.form = 0; gp.arr = 0; memset(gp.secvar, 0, sizeof gp.secvar);
    press(B_OCTDN); frame();                            /* (the settings: a loop, for JAM and the SET) */
    press(B_OCTUP); frames(4);
    to_reel(); frames(32);
    sv(); idA = reel[reel_play].id;                     /* (take 1) */
    turn(EN_ALGO, 1); frames(640); sv(); idB = reel[reel_play].id;   /* (take 2: new sounds) */
    btn_down(B_OCTUP); frame();
    gl_key(22, 600); gl_key(22, 600); gl_key(22, 600);  /* (D#5 held: JAM's bars, 8 -> 16 -> 32 -> 4) */
    gl_key(22, 50);                                     /* D#5: JAM on */
    btn_up(B_OCTUP); frame();
    ok = jam.bars == 4u && jam.on && reel_n == 2u;
    press(B_REC); frame();                              /* (stopped: no SET recorded, said) */
    ok &= !set_rec && gp.view == 1u && gpop.on;
    press(B_PLAY);
    play_ms(12000);                                     /* (JAM: NOW changed by itself, no take) */
    ok &= reel_n == 2u && now_dirty() && now_t.parent == idB && now_t.how != GH_NONE;
    bad += check("GEN song takes: JAM changes NOW by itself every 4 bars (from the take it came from), no take", ok);
    btn_down(B_OCTUP); frame(); gl_key(22, 50); btn_up(B_OCTUP); frame();   /* (JAM off) */
    launch(1); play_ms(3000);                           /* (OCT- + G3: take 2, NOW again) */
    ok = !jam.on && reel_play == 1u;
    press(B_REC); frame();                              /* (playing: a SET recording, take 2 first) */
    ok &= set_rec && set_n == 1u && set_e[0].id == idB;
    launch(0); play_ms(3000);                           /* (OCT- + F3: take 1, its entry) */
    ok &= reel_play == 0u && set_n == 2u && set_e[1].id == idA && set_e[0].bars >= 1u;
    play_ms(3000);
    press(B_REC); frame();
    ok &= !set_rec && set_n == 2u;
    bad += check("GEN song takes: REC while playing records the takes launched with OCT- + a white key, each its bars", ok);
    transport_req = 2; play_ms(100);
    turn(EN_SELECT, 1); frame();                        /* (the SET page, by the ring) */
    set_cur = 0;
    ok = gp.view == 2u;
    set_e[0].bars = 1; set_e[1].bars = 2;               /* (short entries) */
    turn(EN_K3, 1);
    ok &= set_e[0].bars == 2u;
    set_e[0].bars = 1;
    press(B_PLAY);
    play_ms(800);
    ok &= set_play && reel[reel_play].id == set_e[0].id;
    play_ms(3000);
    ok &= set_play && set_i == 1u && reel[reel_play].id == set_e[1].id && set_cur == 1u;   /* (the cursor goes with it) */
    bad += check("GEN song takes: the SET page: KNOB 3 an entry's bars, PLAY plays the takes in a row, each its bars, the cursor with it", ok);
    set_e[0].bars = 4; set_e[1].bars = 8;
    play_ms(100);
    turn(EN_K1, -1);                                    /* (playing: entry 1 picked, on the next bar) */
    ok = set_cur == 0u && set_jump == 1u && gpop.on;
    play_ms(3000);
    ok &= set_i == 0u && reel[reel_play].id == set_e[0].id && set_play;
    press(B_PLAY); play_ms(100);
    ok &= !set_play;
    bad += check("GEN song takes: KNOB 1 while the SET plays: that entry on the next bar", ok);
    press(B_OCTDN); frame();
    set_n = 0; jam.on = 0; jam.bars = 8;
    reel_n = 0; reel_cur = 0; reel_play = 0xFF;
    ui_rec_prefs &= (uint8_t)~(PREF_SIX >> 8); rp_apply();
    ui_power_on();
    return bad;
}

/* (TAKES v2) the reel packed and read back (what flash keeps over power-off): every take, its card, the SET, the IDs
 * going on; too many for the sector: the oldest not kept left out (an older firmware's takes). Share codes: a take's code
 * makes the same piece (its fingerprint), a typo is caught by the check, a take as made is 7 characters */
static int test_gen_store(void)
{
    static uint8_t buf[3840];
    static take_t keep[REEL_N];
    int bad = 0, ok;
    uint32_t n, k, pr0, len, i, maxlen = 0;
    uint16_t next0;
    char code[CODE_MAX + 2u];
    gset_t g;
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1; set_n = 0;
    gen_fresh_recipe();                                 /* (what an earlier test's JAM and ALGORITHM left: not this take's) */
    gp.genre = 3; gp.seed = 202; gp.form = 2; gp.var[SL_DRUMS] = 2; gp.svar[RL_LEAD] = 1; gp.secvar[0][1] = 3; gp.arr = 1;
    gp.pin[0].what = 1u + SL_BASS; gp.pin[0].seed = 77; gp.pin[0].var = 4;
    gen_how = GH_NEW; gen_go(); now_save();
    pr0 = dice_print();
    gen_fresh_recipe();
    for (k = 0; k < 6u; k++) {
        gp.genre = (uint8_t)(7u + k); gp.seed = (uint16_t)(1000u + k); gp.form = (uint8_t)(k % 3u);
        gen_how = GH_NEW; now_from = 0; gen_go(); now_save();
    }
    gpop.on = 0;
    for (k = 0; k < reel_n; k++) reel[k].keep = 0;      /* (as an older firmware's takes: not kept, but one) */
    reel[2].keep = 1;
    set_e[0].id = reel[0].id; set_e[0].bars = 8; set_e[1].id = reel[3].id; set_e[1].bars = 4; set_n = 2;
    memcpy(keep, reel, sizeof keep);
    n = reel_n; next0 = reel_next;
    len = reel_pack(buf, sizeof buf);
    reel_n = 0; set_n = 0; reel_next = 1;
    ok = n == 7u && reel_unpack(buf, len) == 0 && reel_n == n && set_n == 2u && set_e[1].bars == 4u && reel_next == next0;
    for (k = 0; k < n; k++)
        ok &= !memcmp(&reel[k].set, &keep[k].set, sizeof keep[k].set) && reel[k].id == keep[k].id &&
              reel[k].keep == keep[k].keep && !memcmp(reel[k].thumb, keep[k].thumb, sizeof keep[k].thumb) &&
              reel[k].nrow == keep[k].nrow && !memcmp(reel[k].row, keep[k].row, sizeof keep[k].row) && reel[k].bpm == keep[k].bpm;
    gset_put(&reel[0].set); gen_how = GH_NONE; gen_go();
    ok &= dice_print() == pr0;
    bad += check("GEN store: the reel packed and read back (every recipe, card, the SET, the IDs); a take read back makes its piece", ok);
    buf[20] ^= 0x5Au;                                   /* (the pack not one: the reel left as it was) */
    ok = reel_unpack((const uint8_t *)"XX", 2) != 0 && reel_n == n;
    len = reel_pack(buf, 400);                          /* (a small sector: the oldest not kept out, the kept one in) */
    reel_n = 0; set_n = 0;
    ok &= len <= 400u && reel_unpack(buf, len) == 0 && reel_n < n && reel_find(keep[2].id) >= 0 && reel_find(keep[n - 1u].id) >= 0;
    bad += check("GEN store: a broken pack refused; too many for the room: the oldest not kept left out, the kept ones in", ok);
    memcpy(reel, keep, sizeof keep); reel_n = (uint8_t)n;
    ok = 1;
    for (k = 0; k < n; k++) {                           /* (every take: its code back to its recipe) */
        len = take_code(&reel[k].set, code);
        ok &= len >= 7u && code_take(code, &g) == 0 && g.seed == reel[k].set.seed && !memcmp(g.var, reel[k].set.var, sizeof g.var) &&
              !memcmp(g.svar, reel[k].set.svar, sizeof g.svar) && !memcmp(g.secvar, reel[k].set.secvar, sizeof g.secvar) &&
              g.arr == reel[k].set.arr && g.genre == reel[k].set.genre && g.form == reel[k].set.form && g.pin[0].what == reel[k].set.pin[0].what &&
              g.pin[0].seed == reel[k].set.pin[0].seed && g.energy == reel[k].set.energy && g.amt[2] == reel[k].set.amt[2];
        if (len > maxlen) maxlen = len;
    }
    memset(&g, 0, sizeof g);                            /* (a take as the screen makes it: 7 characters) */
    g.seed = 1234; g.genre = 5; g.nlive = 4; g.form = 1; g.energy = 50; g.complex = 50; g.swing = GA_AUTO; g.human = 30;
    g.fills = g.wet = g.grit = GA_AUTO;
    for (i = 0; i < GP_N; i++) g.amt[i] = GA_AUTO;
    len = take_code(&g, code);
    ok &= len == 7u;
    len = take_code(&reel[0].set, code);
    for (i = 0; code[i]; i++) code[i] = (char)(code[i] == 'X' ? 'x' : code[i]);
    ok &= code_take(code, &g) == 0;                     /* (lower case taken) */
    code[2] = (char)(code[2] == 'A' ? 'B' : 'A');
    ok &= code_take(code, &g) == -2 && code_take("HELLO!", &g) == -1;
    code_take(code, &g);
    len = take_code(&reel[0].set, code);
    code_take(code, &g);
    gset_put(&g); gen_how = GH_NONE; gen_go();
    ok &= dice_print() == pr0;
    printf("     (codes: a take as made 7 characters, the richest here %u)\n", maxlen);
    bad += check("GEN store: share codes: every take's code back to its recipe, the piece the same; a typo caught", ok);
    memset(gp.var, 0, sizeof gp.var); memset(gp.svar, 0, sizeof gp.svar); memset(gp.secvar, 0, sizeof gp.secvar);
    memset(gp.pin, 0, sizeof gp.pin); gp.arr = 0;
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; set_n = 0; reel_next = 1;
    ui_power_on();
    return bad;
}

/* (TAKES v2, NOW) the GEN screen's controls, no conflicts: OCT+ held makes the keys the layer's on every page (silent:
 * checked while the key is down), OCT+ let go after keys or another control does nothing more, the SET page's EDIT held,
 * PRESETS and ALGORITHM leave the reel alone, a delete question goes with the page, JAM waits for a SET playing, OCT+ on
 * the settings while the music plays makes NOW on the next bar, GLO held + G4 is NOW's UNDO there (silent) */
static int test_gen_conflicts(void)
{
    int bad = 0, ok;
    uint32_t k, n0, st0[NTRK];
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1; set_n = 0;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 0;
    press(B_OCTUP); frames(4);                          /* (a piece: NOW) */
    sv();
    ok = !gp.view && reel_n == 1u;
    key_down(0); frame();
    ok &= kb_note[0] != KB_SILENT;                      /* (a key alone: a note) */
    key_up(0); frame();
    btn_down(B_OCTUP); frame();
    key_down(0); frame();
    ok &= kb_note[0] == KB_SILENT;                      /* (OCT+ held: silent, while it is down) */
    frames(48);
    key_up(0); frame();
    ok &= reel_n == 1u && now_t.how == GH_DRUMS;        /* (F3: new drums, on the settings page too) */
    btn_up(B_OCTUP); frame(); frames(32);
    ok &= reel_n == 1u && !gl.used && now_t.how == GH_DRUMS;   /* (OCT+ let go after a key: no piece) */
    bad += check("GEN controls: settings page, OCT+ held: a key silent while down, F3 new drums (NOW); OCT+ let go: no piece too", ok);
    sv();                                               /* (take 2) */
    to_reel(); frames(32);                              /* (the reel, by the ring: on NOW) */
    btn_down(B_OCTUP); frame();
    key_down(9); frame();
    ok = gp.view == 1u && kb_note[9] == KB_SILENT;
    frames(48); key_up(9); frame();
    btn_up(B_OCTUP); frame(); frames(32);
    ok &= reel_n == 2u && now_t.how == GH_TEX && reel_want == -1;
    bad += check("GEN controls: the reel, OCT+ held: a key silent while down, NOW changed; OCT+ let go: no load", ok);
    sv();                                               /* (take 3) */
    press(B_PLAY); play_ms(300);
    reel_cur = 0; reel_on_now = 0;                      /* (the cursor on another take) */
    btn_down(B_OCTUP); frame();
    turn(EN_ALGO, 1);                                   /* (a control with OCT+ down) */
    btn_up(B_OCTUP); frame();
    ok = gl.used == 0u && reel_want == -1 && gen_take_how == GH_SOUNDS;   /* (no load: ALGORITHM's change waits) */
    play_ms(4000);
    ok &= now_t.how == GH_SOUNDS && reel_on_now;
    bad += check("GEN controls: ALGORITHM turned with OCT+ down: its change, OCT+ let go loads nothing over it", ok);
    transport_req = 2; play_ms(100);
    reel_cur = 1; reel_on_now = 0;
    press(B_EDIT); frame();                             /* (a question on the reel) */
    ok = reel_ask == 1u;
    turn(EN_SELECT, 1); frame();                        /* (the next page, the SET's: the question gone) */
    ok &= !reel_ask && gp.view == 2u;
    press(B_OCTDN); frame();                            /* (OCT-: back to the reel) */
    ok &= gp.view == 1u && !reel_ask;
    reel_cur = 1; reel_on_now = 0;
    n0 = reel_n;
    press(B_OCTUP); frames(4);                          /* (no delete answered by it: the take loaded) */
    ok &= reel_n == n0 && reel_play == reel_cur;
    bad += check("GEN controls: a delete question goes with the page (SELECT): a later OCT+ deletes nothing, OCT- back", ok);
    set_e[0].id = reel[0].id; set_e[0].bars = 2; set_e[1].id = reel[1].id; set_e[1].bars = 2; set_n = 2;
    turn(EN_SELECT, 1); frames(32);                     /* (the SET page, by the ring) */
    ok = gp.view == 2u;
    k = reel_cur;
    turn(EN_PRESET, 1); frame();
    ok &= set_cur == 1u && reel_cur == k && reel_want == -1;   /* (PRESETS: the entries, not the reel) */
    { uint8_t s2 = gp.svar[RL_BASS]; turn(EN_ALGO, 1); frame(); ok &= gp.svar[RL_BASS] == s2 && !gen_take_ms; }
    hold(B_EDIT); frame();
    ok &= !reel_ask && set_n == 2u;                     /* (EDIT held: nothing) */
    btn_down(B_OCTUP); frame(); key_down(0); frame();
    ok &= kb_note[0] == KB_SILENT;
    key_up(0); frame(); btn_up(B_OCTUP); frame();
    ok &= set_n == 2u;                                  /* (OCT+ with a key: no entry added) */
    bad += check("GEN controls: the SET page: PRESETS its entries, ALGORITHM and EDIT held nothing, OCT+ with a key no entry", ok);
    set_play = 1; jam.on = 1; jam.bars = 4; jam.n = 3;
    { uint32_t ms = gen_take_ms; jam_bar(); ok = gen_take_ms == ms; }
    set_play = 0; jam.on = 0; jam.bars = 8; set_n = 0;
    bad += check("GEN controls: JAM waits while a SET plays", ok);
    press(B_OCTDN); frame(); press(B_OCTDN); frame();    /* (OCT-, OCT-: the settings) */
    ok = !gp.view && ui.confirm == CF_DICE_ALL;
    press(B_PLAY); play_ms(300);
    n0 = reel_n;
    gp.energy = 70;                                     /* (a setting changed, the music playing) */
    press(B_OCTUP); frame();
    ok &= gen_take_ms != 0u && reel_n == n0;            /* (NOW, on the next bar) */
    play_ms(4000);
    ok &= reel_n == n0 && now_t.how == GH_SET && now_dirty() && song.playing;
    transport_req = 2; play_ms(100);
    bad += check("GEN controls: OCT+ on the settings while playing: NOW made on the next bar (no take)", ok);
    for (k = 0; k < NTRK; k++) st0[k] = steps_sig(&trk[k]);
    gp.var[SL_DRUMS] = 9;
    press(B_OCTUP); frames(4);                          /* (another piece: NOW's next state) */
    btn_down(B_GLO); frame(); key_down(14); frame();
    ok = kb_note[14] == KB_SILENT;
    key_up(14); frame(); btn_up(B_GLO); frame(); frames(16);
    for (k = 0; k < NTRK; k++) ok &= steps_sig(&trk[k]) == st0[k];
    bad += check("GEN controls: GLO held + G4 on the GEN screen: NOW's UNDO (silent), the piece before back", ok);
    memset(gp.var, 0, sizeof gp.var); memset(gp.svar, 0, sizeof gp.svar);
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; set_n = 0;
    ui_power_on();
    return bad;
}

/* (TAKES v2, the UX pass, NOW) the reel's knobs (TEMPO at once, KEY and ENERGY: NOW on the next change), the pop-up,
 * OCT- back and HOME closing, SAVE keeping NOW (with OCT+: a code), the drops held (G4 the drums), the layer's LEDs, REC
 * stopped; and a piece the same whatever came before it (its notes, its sounds, the song's settings) */
static uint32_t gen_full_sig(void)
{
    uint32_t h = 2166136261u, k, j;
    for (k = 0; k < NTRK; k++) {
        h = (h ^ steps_sig(&trk[k]) ^ trk[k].eng_req) * 16777619u;
        for (j = 0; j < P_COUNT; j++) h = (h ^ (uint16_t)trk[k].p[j]) * 16777619u;
    }
    for (j = 0; j < G_COUNT; j++) if (j != G_SLOT && j != G_NAME) h = (h ^ (uint16_t)song.g[j]) * 16777619u;
    return h;
}
static int test_gen_ux(void)
{
    int bad = 0, ok;
    uint32_t n0, dr, a;
    int16_t bpm0;
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1; set_n = 0;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 0;
    press(B_OCTUP); frames(4);
    sv();
    to_reel(); frames(32);
    n0 = reel_n; bpm0 = song.g[G_BPM];
    turn(EN_K1, 1); frame();
    ok = song.g[G_BPM] == bpm0 + 1 && reel_n == n0 && reel[0].bpm == bpm0 && now_t.bpm == song.g[G_BPM];
    ok &= gp.tempo == (uint8_t)(bpm0 + 1) && now_dirty() && gpop.on && str_eq(gpop.lbl, "TEMPO");
    bad += check("GEN UX: KNOB 1 on the reel, TEMPO at once (NOW's; the saved take keeps its own), said in the band", ok);
    turn(EN_K2, 1); frame();
    ok = gen_take_ms != 0u && gp.key != 0u;
    frames(640);                                        /* (at rest, stopped: NOW) */
    ok &= reel_n == n0 && now_t.how == GH_KEY;
    turn(EN_K3, 1); frames(640);
    ok &= reel_n == n0 && now_t.how == GH_ENERGY;
    bad += check("GEN UX: KNOB 2 the key, KNOB 3 the energy: NOW made again once the knob rests (no take)", ok);
    turn(EN_SELECT, 1); frame();                        /* (the SET page) */
    ok = gp.view == 2u;
    press(B_OCTDN); frame(); ok &= gp.view == 1u && ui.confirm == CF_DICE_ALL;
    press(B_OCTDN); frame(); ok &= !gp.view && ui.confirm == CF_DICE_ALL;
    press(B_OCTDN); frame(); ok &= !ui.confirm;         /* (the settings: OCT- closes) */
    held_turn(B_HOME, 1); fm1_ms += 1200u; frames(32);
    to_reel(); press(B_HOME); frame();
    ok &= !ui.confirm;                                  /* (HOME: closed from the reel at once) */
    bad += check("GEN UX: OCT- goes back (the SET page, the reel, the settings, closed), HOME closes from any page", ok);
    held_turn(B_HOME, 1); fm1_ms += 1200u; frames(32);
    to_reel();
    press(B_SAVE); frame();
    ok = reel_n == n0 + 1u && reel[n0].how == GH_ENERGY && reel[n0].parent == reel[0].id && gpop.on && !now_dirty();
    btn_down(B_OCTUP); frame(); press(B_SAVE); frame(); btn_up(B_OCTUP); frame();
    ok &= name_on();                                    /* (OCT+ held + SAVE: a code typed) */
    press(B_OCTDN); frame();
    ok &= !name_on() && ui.confirm == CF_DICE_ALL && gp.view == 1u && reel_n == n0 + 1u;   /* (cancelled: the reel again) */
    bad += check("GEN UX: SAVE keeps NOW (a child of its take, what changed), OCT+ held + SAVE types a code (OCT- back)", ok);
    dr = gs.trk_of[RL_DRUM];
    n0 = reel_n;
    btn_down(B_OCTUP); frame();
    key_down(14); frame(); frames(48);                  /* (G4 held: the drums dropped) */
    ok = trk[dr].p[P_MUTE] == 1 && kb_note[14] == KB_SILENT;
    { uint32_t on, br; on = gen_layer_leds(&br); ok &= ((on >> 14) & 1u) && ((br >> 0) & 1u); }   /* (G4 lit, F3 breathing) */
    key_up(14); frame();
    ok &= trk[dr].p[P_MUTE] == 0 && reel_n == n0 && !now_dirty();   /* (let go: back, nothing changed) */
    btn_up(B_OCTUP); frame(); frames(32);
    ok &= reel_n == n0 && !gl.used;
    bad += check("GEN UX: OCT+ held, G4 held drops the drums at once, let go they are back (NOW the same); the layer's LEDs", ok);
    press(B_REC); frame();
    ok = !set_rec && gpop.on && str_eq(gpop.val, "PLAY FIRST");
    bad += check("GEN UX: REC stopped: no SET page (SELECT has it), said to play first", ok);
    {   /* (a piece the same whatever came before: notes, sounds, the song's settings) */
        uint32_t g;
        ok = 1;
        for (g = 0; g <= NGENRE; g += 7u) {
            ui_power_on();
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(301u + g); gp.form = 1; gp.energy = 55;
            gen_go(); a = gen_full_sig();
            ui_power_on();
            gp.genre = (uint8_t)((g + 13u) % (NGENRE + 1u)); gp.seed = (uint16_t)(6070u + g); gp.form = 2; gen_go();
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(301u + g); gp.form = 1; gp.energy = 55;
            gen_go(); ok &= gen_full_sig() == a;
        }
        bad += check("GEN UX: a take made after another piece is the same as from power-on (its notes, sounds, settings)", ok);
    }
    memset(gp.var, 0, sizeof gp.var); memset(gp.svar, 0, sizeof gp.svar);
    gp.tempo = gp.key = 0; gp.energy = 50;
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; set_n = 0;
    ui_power_on();
    return bad;
}

/* (NOW) reported: every change made a take (ALGORITHM, KEY, ENERGY, OCT+, the keys), the pop-up covered the card. NOW:
 * nothing makes a take but SAVE; NOW's history (GLO + G4 back, GLO + A4 forward; at its start the music before GEN);
 * C#5 ORIG back to the take NOW came from; OCT- held: the white keys play the takes (a key with none: nothing; OCT- let
 * go after: no back); PLAY on a take's card plays that take; the keys' map and the takes' map the whole screen */
static int test_gen_now(void)
{
    int bad = 0, ok;
    uint32_t k, p1, p2, p3;
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1; set_n = 0;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 0;
    press(B_OCTUP); frames(4);
    to_reel(); frames(32);
    sv(); p1 = dice_print();                            /* (take 1) */
    turn(EN_ALGO, 1); frames(640); p2 = dice_print();   /* (NOW: new sounds) */
    turn(EN_K3, 1); frames(640); p3 = dice_print();     /* (NOW: more energy) */
    ok = reel_n == 1u && now_dirty() && now_hn == 3u && now_hi == 2u && p1 != p2 && p2 != p3;
    btn_down(B_GLO); frame(); key_down(14); frame(); key_up(14); frame(); btn_up(B_GLO); frames(16);
    ok &= dice_print() == p2 && now_hi == 1u;           /* (GLO + G4: a state back) */
    btn_down(B_GLO); frame(); key_down(14); frame(); key_up(14); frame(); btn_up(B_GLO); frames(16);
    ok &= dice_print() == p1 && now_hi == 0u && !now_dirty() && reel_play == 0u;   /* (back to take 1: NOW is it) */
    btn_down(B_GLO); frame(); key_down(16); frame(); key_up(16); frame(); btn_up(B_GLO); frames(16);
    ok &= dice_print() == p2 && now_hi == 1u;           /* (GLO + A4: forward) */
    bad += check("GEN NOW: ALGORITHM, ENERGY make no take; GLO + G4 steps NOW back, GLO + A4 forward (the piece each time)", ok);
    btn_down(B_OCTUP); frame(); gl_key(20, 50); btn_up(B_OCTUP); frame(); frames(16);   /* (C#5: ORIG) */
    ok = dice_print() == p1 && reel_play == 0u && now_hn == 3u && now_hi == 2u;   /* (back to take 1: a state of its
                                                         * own, after the one it came from: the redo gone) */
    btn_down(B_GLO); frame(); key_down(14); frame(); key_up(14); frame(); btn_up(B_GLO); frames(16);
    ok &= dice_print() == p2;                           /* (and undone) */
    bad += check("GEN NOW: OCT+ + C#5 ORIG: NOW back to the take it came from, all at once; GLO + G4 undoes it", ok);
    sv();                                               /* (take 2: NOW with new sounds) */
    press(B_PLAY); play_ms(300);
    launch(0);                                          /* (OCT- + F3: take 1, on the next bar) */
    ok = reel_want == 0 && gp.view == 1u && ui.confirm == CF_DICE_ALL;   /* (OCT- let go after a key: no back) */
    play_ms(3000);
    ok &= reel_play == 0u && dice_print() == p1;
    launch(5);                                          /* (D4: no take 6, nothing) */
    play_ms(3000);
    ok &= reel_play == 0u && song.playing;
    btn_down(B_OCTDN); frame(); key_down(white(1)); frame();
    ok &= kb_note[white(1)] == KB_SILENT && gl.held_dn;  /* (silent while OCT- is down) */
    { uint32_t on, br; on = gen_layer_leds(&br); ok &= ((on >> white(0)) & 1u) && ((br >> white(1)) & 0u) == 0u; }
    key_up(white(1)); frame(); btn_up(B_OCTDN); frame();
    play_ms(3000);
    ok &= reel_play == 1u && dice_print() == p2;
    transport_req = 2; play_ms(100);
    bad += check("GEN NOW: OCT- held + a white key plays that take on the next bar (silent; none there: nothing; no back)", ok);
    turn(EN_PRESET, 1);                                 /* (the cursor: take 1, stopped: nothing loads) */
    { int g = 0; while ((reel_on_now || reel_cur != 0u) && g++ < 4) turn(EN_PRESET, 1); }
    ok = !reel_on_now && reel_cur == 0u && reel_play == 1u;
    press(B_PLAY); play_ms(300);                        /* (PLAY on take 1's card: take 1, playing) */
    ok &= reel_play == 0u && dice_print() == p1 && (song.playing || chain_busy());
    transport_req = 2; play_ms(100);
    bad += check("GEN NOW: PLAY on a take's card plays that take", ok);
    reel_on_now = 1; ui.force = 1; frame();
    k = reel_play;
    btn_down(B_OCTUP); frames(600);                     /* (held 0.6 s, to read the keys' map) */
    ok = gl.held && host_screen[240u * 40u + 15u] != host_screen[240u * 40u + 239u];   /* (the map drawn) */
    btn_up(B_OCTUP); frames(16);
    ok &= reel_play == k && !gen_take_ms && !now_dirty();   /* (let go: no new piece, nothing loaded) */
    btn_down(B_OCTDN); frames(600);
    ok &= gl.held_dn;
    btn_up(B_OCTDN); frames(16);
    ok &= gp.view == 1u && ui.confirm == CF_DICE_ALL;   /* (held to read the takes: let go, no back) */
    press(B_OCTDN); frame();
    ok &= !gp.view;                                     /* (a tap: back) */
    bad += check("GEN NOW: OCT+ or OCT- held to read their maps (the whole screen): let go, nothing (a tap still acts)", ok);
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; set_n = 0;
    ui_power_on();
    return bad;
}

/* (GEN fix) reported: every change on the GEN screen (a BPM step, ALGORITHM, PRESETS, OCT+ and a key) showed the
 * takes page again before what changed: the page was cleared and drawn whole on every redraw, and drawn under the
 * pop-up each time. Now nothing on the panel flashes (a pixel written twice in a frame, back to what it was), stopped
 * or playing; the pop-up gone, the screen is what a fresh drawing of the page makes (nothing of it left) */
static void gen_scr_setup(int playing)
{
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1; set_n = 0;
    go_home(); frames(16);
    fm1_ms += 1200u;
    held_turn(B_HOME, 1);
    fm1_ms += 1200u;
    gp.genre = 3; gp.seed = 202; gp.form = 2;
    press(B_OCTUP); frames(800);
    gp.seed = 203; press(B_OCTUP); frames(800);
    to_reel(); frames(100);
    if (playing) { press(B_PLAY); play_ms(1000); }
    host_flash = host_flash_frames = 0;
}
static void gen_scr_run(uint32_t ms, int playing) { if (playing) play_ms(ms); else frames(ms); }
static int gen_scr_fresh_same(void)                     /* the screen as it is == the page drawn anew */
{
    static uint16_t a[240 * 240];
    uint32_t i, d = 0;
    ui.force = 1; frame();                              /* (the page as it is now: the playhead moves on redraws) */
    memcpy(a, host_screen, sizeof a);
    gen_fresh = 1; ui.force = 1; frame();
    for (i = 240u * H_HEAD; i < 240u * 240u; i++) d += a[i] != host_screen[i];
    return d == 0u;
}
static int test_gen_screen(void)
{
    int bad = 0, ok, pl;
    uint32_t i;
    for (pl = 0; pl < 2; pl++) {
        gen_scr_setup(pl);
        for (i = 0; i < 12u; i++) { turn(EN_K1, 1); gen_scr_run(48, pl); }   /* (BPM, a detent at a time) */
        ok = host_flash == 0u && gpop.on && str_eq(gpop.lbl, "TEMPO");
        gen_scr_run(2000, pl);
        ok &= !gpop.on && host_flash == 0u && gen_scr_fresh_same();
        bad += check(pl ? "GEN SCREEN: KNOB 1 TEMPO step by step, playing: the pop-up only, nothing flashes, nothing left"
                        : "GEN SCREEN: KNOB 1 TEMPO step by step, stopped: the pop-up only, nothing flashes, nothing left", ok);
        gen_scr_setup(pl);
        turn(EN_ALGO, 1); gen_scr_run(3000, pl);
        ok = host_flash == 0u && gen_scr_fresh_same();
        gen_scr_setup(pl);
        turn(EN_PRESET, -1); gen_scr_run(600, pl); press(B_OCTUP); gen_scr_run(3000, pl);
        ok &= host_flash == 0u && gen_scr_fresh_same();
        gen_scr_setup(pl);
        turn(EN_K2, 1); gen_scr_run(200, pl); turn(EN_K3, 1); gen_scr_run(3000, pl);
        ok &= host_flash == 0u && gen_scr_fresh_same();
        bad += check(pl ? "GEN SCREEN: ALGORITHM, PRESETS + OCT+, KEY, ENERGY, playing: nothing flashes (takes on the bar)"
                        : "GEN SCREEN: ALGORITHM, PRESETS + OCT+, KEY, ENERGY, stopped: nothing flashes", ok);
        gen_scr_setup(pl);
        btn_down(B_OCTUP); frame();
        key_down(2); frame(); key_up(2); gen_scr_run(200, pl);   /* (G3: the bass made again) */
        key_down(14); gen_scr_run(300, pl); key_up(14); frame();  /* (G4 held: the drums dropped) */
        btn_up(B_OCTUP); gen_scr_run(3000, pl);
        ok = host_flash == 0u && gen_scr_fresh_same();
        bad += check(pl ? "GEN SCREEN: OCT+ held, a part key, a drop, playing: nothing flashes, nothing left"
                        : "GEN SCREEN: OCT+ held, a part key, a drop, stopped: nothing flashes, nothing left", ok);
    }
    {   /* the settings pages and the SET page too */
        gen_scr_setup(1);
        press(B_OCTDN); frame();                        /* (the settings) */
        host_flash = 0;
        turn(EN_ALGO, 1); play_ms(2500); press(B_OCTUP); play_ms(3000);
        ok = gp.view == 0u && host_flash == 0u && gen_scr_fresh_same();
        to_reel(); turn(EN_SELECT, 1); frame();         /* (the SET page) */
        host_flash = 0;
        press(B_OCTUP); play_ms(3000);
        ok &= gp.view == 2u && host_flash == 0u && gen_scr_fresh_same();
        transport_req = 2; frames(64);
        bad += check("GEN SCREEN: the settings (ALGORITHM, OCT+) and the SET page (OCT+, bars going by): nothing flashes", ok);
    }
    memset(gp.var, 0, sizeof gp.var); memset(gp.svar, 0, sizeof gp.svar);
    gp.tempo = gp.key = 0; gp.energy = 50;
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; set_n = 0;
    ui_power_on();
    return bad;
}

/* (SET loops) reported: the SET's first take did not play completely. REC counted the take playing from the moment REC
 * was pressed (part of its loop), and played back from its top the entry cut its phrase short. An entry recorded is now
 * whole loops of its take (the nearest, one at least) */
static int test_gen_set_loops(void)
{
    int bad = 0, ok;
    uint32_t l, i;
    ui_power_on();
    reel_n = 0; reel_cur = 0; reel_play = 0xFF; reel_want = -1; set_n = 0;
    gen_enter();
    gp.genre = 3; gp.seed = 202; gp.form = 0;          /* (a loop) */
    press(B_OCTUP); frames(4);
    to_reel(); frames(32);
    sv();
    turn(EN_ALGO, 1); frames(640); sv();               /* (take 2) */
    l = now_loop_bars();
    press(B_PLAY); play_ms(chain_bar() * 1500u / FS);  /* (a bar and a half in: REC part-way through the loop) */
    press(B_REC); frame();
    play_ms(chain_bar() * 1000u / FS);
    launch(0);                                          /* (take 1, after about a bar) */
    play_ms(chain_bar() * 1000u / FS * (l + 1u));
    press(B_REC); frame();
    ok = l >= 1u && set_n == 2u;
    for (i = 0; i < set_n; i++) ok &= set_e[i].bars >= l && set_e[i].bars % l == 0u;
    printf("     (a loop of %u bars: entries %u, %u bars)\n", l, set_e[0].bars, set_n > 1u ? set_e[1].bars : 0u);
    bad += check("GEN SET: REC part-way through a loop, a take launched after a bar: each entry whole loops of its take", ok);
    transport_req = 2; play_ms(100);
    set_n = 0; reel_n = 0; reel_cur = 0; reel_play = 0xFF;
    ui_power_on();
    return bad;
}

/* (0.6.21) HOME + SELECT: the dialog, OCT+: everything diced (one musical scale, a sequence on every track, DRUM a
 * beat); GLO + G4: UNDO puts the music back, again: forward */
static int test_dice_all(void)
{
    int bad = 0, ok;
    uint32_t k, i, sig0[NTRK], eng0[NTRK], sig1[NTRK], on;
    ui_power_on();
    host_ticks = 0;                                      /* (QG-1 audit) the soak's clock must not seed this test */
    go_home(); frames(16);
    for (k = 0; k < NTRK; k++) { sig0[k] = steps_sig(&trk[k]); eng0[k] = trk[k].eng_req; }
    fm1_ms += 1200u;
    held_turn(B_HOME, 1);
    ok = ui.confirm == CF_DICE_ALL;
    press(B_OCTDN);
    ok &= !ui.confirm && steps_sig(&trk[0]) == sig0[0];
    bad += check("DICE ALL: HOME + SELECT asks first (OCT- NO: nothing changed)", ok);
    fm1_ms += 1200u;
    held_turn(B_HOME, 1); press(B_OCTUP); frames(4);
    ok = ui.confirm == CF_DICE_ALL && undo_ok;          /* (0.6.91) the GEN screen stays (OCT+ again: another piece) */
    press(B_OCTDN); frames(4);
    ok &= !ui.confirm;
    {   /* (0.6.34) the orchestration leaves some roles silent, the drums too: at least two melodic voices play */
        uint32_t mel = 0;
        for (k = 0; k < NTRK; k++) {
            for (i = 0, on = 0; i < (uint32_t)trk[k].p[P_SLEN]; i++) on += step_on(&trk[k].step[i]);
            mel += on > 0u && !drum_track(&trk[k]);
        }
        ok &= mel >= 2u;
    }
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        if (!drum_track(t))
            ok &= t->p[P_ROOT] == trk[0].p[P_ROOT] && t->p[P_SCALE] == trk[0].p[P_SCALE] && t->p[P_SCALE] != 0 &&
                  t->p[P_SCALE] != 13 && t->p[P_SCALE] < 14;
        sig1[k] = steps_sig(t);
    }
    for (i = 0; i < 64u; i++) events_block(CTL);       /* (it plays: no crash) */
    bad += check("DICE ALL: OK dices every track (two melodic voices at least, one musical scale for the melodic ones)", ok);
    btn_down(B_GLO); frames(40);
    key_down(white(8)); frame(); key_up(white(8)); frame();
    btn_up(B_GLO); frames(8);
    for (k = 0, ok = 1; k < NTRK; k++) ok &= steps_sig(&trk[k]) == sig0[k] && trk[k].eng_req == eng0[k];
    bad += check("UNDO (GLO + G4): the music before the dice back (steps, engines)", ok);
    btn_down(B_GLO); frames(40);
    key_down(white(8)); frame(); key_up(white(8)); frame();
    btn_up(B_GLO); frames(8);
    for (k = 0, ok = 1; k < NTRK; k++) ok &= steps_sig(&trk[k]) == sig1[k];
    bad += check("  UNDO again: forward (REDO)", ok);
    {   /* (0.6.26) a style, the engines really changed, FM6 on two tracks at most, the drums on one */
        uint32_t moved = 0, fm = 0, drums = 0, r;
        for (k = 0; k < NTRK; k++) eng0[k] = trk[k].eng_req;
        dice_all();
        for (k = 0; k < NTRK; k++) {
            moved += trk[k].eng_req != eng0[k];
            fm += trk[k].eng_req == ENGI_FM6;
            drums += drum_track(&trk[k]);
        }
        ok = !strncmp(ui.msg, "DICED: ", 7) || !strncmp(ui.msg, "GEN: ", 5);
        ok &= moved >= 2u && fm <= 2u && drums >= 1u && drums <= 2u;   /* (GEN: a second kit for some genres) */
        for (r = 0; r < 30u && ok; r++) {               /* thirty more throws: the same holds, every style seen */
            uint32_t m1 = 0;                            /* (QG-1) two engines at least every throw, five in six on average:
                                                         * three on one throw was luck (the FM4 build's seeds missed it) */
            for (k = 0; k < NTRK; k++) eng0[k] = trk[k].eng_req;
            dice_all();
            for (k = 0, fm = 0, drums = 0; k < NTRK; k++) {
                fm += trk[k].eng_req == ENGI_FM6; drums += drum_track(&trk[k]); m1 += trk[k].eng_req != eng0[k];
            }
            ok &= fm <= 2u && drums >= 1u && drums <= 2u && m1 >= 2u;
            moved += m1;
            for (k = 0; k < NTRK; k++) {                /* (0.6.34) the guard rails: chords of three or four, the lead under C7 */
                uint32_t i2;
                for (i2 = 0; i2 < (uint32_t)trk[k].p[P_SLEN]; i2++) {
                    const step_t *s2 = &trk[k].step[i2];
                    if (drum_track(&trk[k]) || s2->time != ST_NOTE) continue;
                    ok &= s2->n >= 1u && s2->n <= 4u;   /* (0.6.80: power chords, triads: three) */
                    ok &= s2->note[0] <= 96u;           /* (GEN: TRAP bells reach C7) */
                }
            }
            project_capture(&proj_scratch);              /* (0.6.81) every piece storable (a send past 127 was not) */
            ok &= proj_pack(&proj_wire, &proj_scratch) != 0;
            for (k = 0; k < 8u; k++) events_block(CTL);
        }
        ok &= moved * 2u >= 31u * 5u;
        bad += check("DICE ALL (0.6.26): a style named, engines changed, FM6 on two tracks at most, one DRUM track, every piece storable; 30 throws", ok);
        {   /* (0.6.27) 40 throws: never the same piece, the tempo moved every time (even from the same random state) */
            uint32_t h[40], n2, m, same = 0, bpm_same = 0;
            int16_t b0;
            for (n2 = 0; n2 < 40u; n2++) {
                b0 = song.g[G_BPM];
                if (n2 % 10u == 0u) rng_state = 0x1234567u;   /* (the same state again: the press time still differs) */
                host_ticks += 13777u;
                dice_all();
                bpm_same += song.g[G_BPM] == b0;
                h[n2] = dice_print();
                for (m = 0; m < n2; m++) same += h[m] == h[n2];
            }
            bad += check("DICE ALL (0.6.27): 40 throws, none the same piece, the tempo moved every time", !same && !bpm_same);
        }
    }
    {   /* (QG-1 audit) an explicit TEMPO is the user's: DICE ALL keeps it instead of forcing a move */
        int16_t bpm;
        gp.tempo = 137;
        dice_all();
        bpm = song.g[G_BPM];
        gp.tempo = 0;
        bad += check("DICE ALL: a TEMPO set on GEN is kept (AUTO still moves it)", bpm == 137);
    }
    ui_power_on();
    return bad;
}

/* (QG-1 stage 2) GEN at power-on: ui_power_on() must leave the defaults of the static initializer (a real boot),
 * whatever the test before it did. gen_power_on() clears the whole struct and then sets the same fields, so a
 * field added to gp shows up here instead of being inherited */
static int test_gen_power_on(void)
{
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    ok = gp.genre == 0 && gp.seed == 1 && gp.energy == 50 && gp.form == 1 && gp.complex == 50 && gp.human == 30 &&
         gp.swing == GA_AUTO && gp.fills == GA_AUTO && gp.wet == GA_AUTO && gp.grit == GA_AUTO &&
         gp.tempo == 0 && gp.key == 0 && gp.scale == 0 && gp.length == 0 && gp.space == 0;
    for (i = 0; i < GP_N; i++) ok &= gp.amt[i] == GA_AUTO;
    bad += check("GEN power-on: the defaults a fresh boot has (RANDOM, seed 1, ENERGY 50, AUTO everywhere)", ok);
    /* every field a take can set, dirtied the way a soak would leave them */
    gp.genre = 5; gp.seed = 4321; gp.energy = 7; gp.form = 2; gp.complex = 99; gp.swing = 88; gp.human = 3;
    gp.fills = 66; gp.wet = 55; gp.grit = 44; gp.tempo = 137; gp.key = 5; gp.scale = 3; gp.length = 2;
    gp.space = 64; gp.page = 3; gp.hot = 2; gp.view = 1; gp.arr = 2; gp.lock = 0xFFFF; gp.sig = 0xDEADBEEF;
    gp.open_ms = 999;
    for (i = 0; i < GP_N; i++) gp.amt[i] = 11;
    memset(gp.var, 0x5A, sizeof gp.var);
    memset(gp.svar, 0x5A, sizeof gp.svar);
    memset(&gp.secvar[0][0], 0x5A, sizeof gp.secvar);
    for (i = 0; i < GP_PINS; i++) { gp.pin[i].what = 3; gp.pin[i].var = 4; gp.pin[i].seed = 5; }
    str_cpy(gp.last, "HOUSE 0421", sizeof gp.last);
    ui_power_on();
    ok = gp.genre == 0 && gp.seed == 1 && gp.energy == 50 && gp.form == 1 && gp.complex == 50 && gp.human == 30 &&
         gp.swing == GA_AUTO && gp.fills == GA_AUTO && gp.wet == GA_AUTO && gp.grit == GA_AUTO &&
         gp.tempo == 0 && gp.key == 0 && gp.scale == 0 && gp.length == 0 && gp.space == 0 &&
         gp.page == 0 && gp.view == 0 && gp.arr == 0 && gp.lock == 0 && gp.sig == 0 && gp.open_ms == 0;
    for (i = 0; i < GP_N; i++) ok &= gp.amt[i] == GA_AUTO;
    for (i = 0; i < sizeof gp.var; i++) ok &= gp.var[i] == 0;
    for (i = 0; i < RL_N; i++) ok &= gp.svar[i] == 0;
    for (i = 0; i < sizeof gp.secvar / sizeof gp.secvar[0][0]; i++) ok &= (&gp.secvar[0][0])[i] == 0;
    for (i = 0; i < GP_PINS; i++) ok &= !gp.pin[i].what && !gp.pin[i].var && !gp.pin[i].seed;
    bad += check("GEN power-on: nothing of the take before survives (TEMPO, takes, pins, the reel's state)", ok);
    ui_power_on();
    return bad;
}

/* (QG-1 stage 2) the RNG is reproducible where it has to be: the same settings and seed make the same piece
 * (the seed drives the generator's own stream), while DICE ALL asks for a new seed every throw */
static int test_gen_reproducible(void)
{
    int bad = 0, ok;
    uint32_t a, b, c;
    ui_power_on();
    gp.genre = 2; gp.seed = 1234;
    gen_go();
    a = dice_print();
    ui_power_on();
    gp.genre = 2; gp.seed = 1234;
    gen_go();
    b = dice_print();
    ui_power_on();
    gp.genre = 2; gp.seed = 1235;
    gen_go();
    c = dice_print();
    ok = a == b && a != c;
    bad += check("GEN: the same settings and seed make the same piece, another seed another piece", ok);
    ui_power_on();
    return bad;
}

/* (0.6.16) the load budget: tight (audio.c), a new note takes a voice instead of adding one; calm, it adds */
static int test_load_budget(void)
{
    int bad = 0;
    uint32_t n0, e;
    track_t *t = &trk[0];
    ui_power_on();
    for (e = 0; e < NENGINES && ENGINES[e] != &ENG_ANALOG; e++) {}


    set_engine_of(t, e); t->engine = t->eng_req; t->p[P_VOICE] = V_POLY; t->p[P_SUS] = 127; t->p[P_MUTE] = 0;
    trk_note_on(t, 60, 100); trk_note_on(t, 64, 100); trk_note_on(t, 67, 100);
    n0 = voices_busy();
    load_tight = 1;
    trk_note_on(t, 71, 100);
    bad += check("LOAD BUDGET (0.6.18): tight, held notes and no dying tail: a new note still adds a voice",
                 voices_busy() == n0 + 1u && n0 == 3u);
    t->v[0].gate = 0; t->v[0].stage = 3; t->v[0].env_out = 20000;   /* a loud release tail */
    trk_note_on(t, 74, 100);
    bad += check("  tight: a loud release tail rings out (not taken; with a sequence they all were)",
                 voices_busy() == n0 + 2u && t->v[0].stage == 3u);
    t->v[0].env_out = 50;                               /* the same tail nearly gone (under -50 dB) */
    trk_note_on(t, 76, 100);
    bad += check("  tight: a dying tail makes the room (the count stays)", voices_busy() == n0 + 2u);
    load_tight = 0;
    { uint32_t i; for (i = 0; i < NVOICE; i++) if (t->v[i].note == 76) t->v[i].gate = 0, t->v[i].active = 0; }
    n0 = voices_busy();
    trk_note_on(t, 77, 100);
    bad += check("  calm: a new note adds a voice", voices_busy() == n0 + 1u);
    load_tight = 1;
    trk_all_off(t); { uint32_t i; for (i = 0; i < NVOICE; i++) t->v[i].active = 0; }
    trk_note_on(t, 60, 100);
    bad += check("  tight with nothing sounding: the first note still plays", voices_busy() == 1u);
    load_tight = 0;
    trk_hush = 1u;                                      /* (0.6.15) GLO solo / FX black key: a MIDI mute */
    trk_note_on(t, 72, 100);
    bad += check("MUTE / SOLO: a silenced track gets no new note (it costs nothing)", voices_busy() == 1u);
    trk_hush = 0;
    return bad;
}

/* (0.6.12) SAVE > PROJECT: KNOB 1 the project (its sections and song in, the music kept), KNOB 2 / PRESETS the
 * section, LOAD and SAVE on it; ARRANGE (the project's song) right after it */
static int test_project_page(void)
{
    int bad = 0, ok;
    uint32_t k;
    ui_power_on();
    if (song_cur) (void)song_select(0);
    sl_steps(60); project_save(0);                      /* project 1, A: notes 60.. */
    chain_config.count = 2;                             /* .. its song: A 4 > B 8 */
    chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
    sl_steps(80);                                       /* the music now: 80.. */
    go_title("PROJECT");
    ok = cur_page()->graph == GR_SLOTS && PAGES[ui.page + 1u].graph == GR_SONG && str_eq(PAGES[ui.page + 1u].title, "ARRANGE");
    bad += check("PROJECT: SAVE > PROJECT, then ARRANGE (the project's song)", ok);
    turn(EN_K1, 1); frame();
    ok = song_cur == 1 && trk[0].step[0].note[0] == 80 && live.cur < 0 && !chain_config.count && msg_is("PROJECT 2");
    bad += check("  KNOB 1: project 2 in (its sections, its song), the music kept", ok);
    song.g[G_SLOT] = 1;
    turn(EN_PRESET, 1); frame();
    ok = song.g[G_SLOT] == 2;
    turn(EN_K2, 1); frame();
    ok &= song.g[G_SLOT] == 3;
    bad += check("  KNOB 2 / PRESETS: the section (C)", ok);
    turn(EN_K4, 1); press(B_OCTUP); frame();            /* SAVE into project 2's C: the NAME screen, then OK */
    for (k = 0; k < 8u && name_on(); k++) { press(B_OCTUP); frame(); }
    ok = project_used(2) && live.cur == 2;
    bad += check("  SAVE: the music into project 2's section C (the music's section: NOW)", ok);
    turn(EN_K1, -1); frame();
    ok = song_cur == 0 && (!FELUCCA_FLASH || project_used(0)) && !project_used(2) && chain_config.count == 2 &&
         trk[0].step[0].note[0] == 80;                  /* (no flash here: the sections were RAM only) */
    bad += check("  KNOB 1 back: project 1, its song still there (its A: from flash), the music kept", ok);
    if (!FELUCCA_FLASH) { sl_steps(60); project_save(0); sl_steps(80); }
    song.g[G_SLOT] = 1;
    turn(EN_K3, 1); press(B_OCTUP); frame();            /* LOAD A */
    ok = trk[0].step[0].note[0] == 60 && live.cur == 0 && chain_config.count == 2;
    bad += check("  LOAD: section A is the music, the project's song kept", ok);
    transport_req = 1; frame();
    turn(EN_K1, 1); frame();
    ok = song_cur == 0 && msg_is("STOP TO CHANGE PROJECT");
    transport_req = 2; frame();
    bad += check("  playing: the project stays (STOP TO CHANGE PROJECT)", ok);
    return bad;
}

/* 1.1.5: the REC layer (ui_layer.c). REC held past HOLD on any page opens it (up to 1.1.4 REC held on SEQ asked "CLEAR Tn
 * SEQUENCE?", Discussion #91: gone, and its "HOLD: CLEAR" hint and the footer's REC + trash with it); a tap still
 * arms / disarms. F3 CLEAR (no dialog), G3 CLICK, A3 COUNT-IN, B3 CLICK LEVEL (each the next
 * value, the same store as MENU > AUDIO, saved when the layer closes), KNOB 1..3 the same; the LEDs;
 * REC in another layer still arms */
static void rec_down(void) { btn_down(B_REC); }
static void rec_up(void) { btn_up(B_REC); frame(); }
static uint32_t menu_rec(uint32_t r) { return menu_get(r == 0u ? MI_CLICK : r == 1u ? MI_COUNTIN : MI_CLKLVL); }
static int test_rec_layer(void)
{
    static const uint8_t G[8] = {GR_ROLL, GR_STEPS, GR_CHANCE, GR_PATS, GR_SONG, GR_MOTION, GR_ADSR, GR_TRK};
    int bad = 0, ok = 1;
    uint32_t k, i, n;
    step_t before[NSTEP];
    for (k = 0; k < 9u; k++) {                          /* every page (and HOME): held, the layer; let go, nothing */
        ui_power_on();
        song.sel = 1;
        my_steps(&trk[1]);
        if (k < 8u)
            go_page(G[k]);
        frame();
        memcpy(before, trk[1].step, sizeof before);
        n = ui.page;
        rec_down(); frames(512);
        ok &= ui.layer == LAYER_REC && ui.ly == LAYER_REC && !ui.confirm && song.rec == 0u && !transport_req &&
              !str_eq(ui.msg, "HOLD: CLEAR");
        rec_up();
        ok &= !ui.layer && !ui.confirm && song.rec == 0u && !transport_req && ui.page == n && (k < 8u ? !ui.home : ui.home) &&
              !memcmp(trk[1].step, before, sizeof before);
    }
    bad += check("REC layer: REC held on STEP PATTERN CHANCE PHRASES SONG AUTOMATION ENV MIXER HOME opens it", ok);
    bad += check("  no CLEAR dialog any more, nothing armed, cleared or started (#91's hold replaced)", ok);
    ok = 1;
    for (k = 0; k < 4u; k++) {                          /* the menu's HOLD time, as the other layers */
        ui_power_on();
        settings_hold = (uint8_t)k;
        rec_down();
        for (i = 0; i < 40u && !ui.layer; i++)
            frame();
        ok &= ui.layer == LAYER_REC && i * 16u >= HOLD_MS[k] && i * 16u <= HOLD_MS[k] + 32u;
        rec_up();
    }
    bad += check("  it opens after MENU > HOLD (0.3 .. 0.6 s), not the old 0.7 s", ok);
    ui_power_on();
    press(B_REC);
    ok = ui.home && song.rec == 1u && transport_req == 1u && !ui.layer;
    frames(400); transport_req = 0; song.playing = 1;
    press(B_REC);
    ok &= song.rec == 0u && !ui.layer && song.playing;
    song.playing = 0;
    go_page(GR_ROLL); frame(); frames(400);
    press(B_REC);
    ok &= song.rec == 1u && transport_req == 0u && msg_is("KEYS WRITE STEPS");
    bad += check("  a tap still arms / disarms when let go (HOME starts PLAY, STEP: step recording)", ok);
    ui_power_on();
    rec_down(); frame();
    press(B_PLAY);                                      /* REC + PLAY: PLAY plays, REC let go arms (no map) */
    ok = transport_req == 1u && !ui.layer;
    song.playing = 1; transport_req = 0;
    rec_up();
    ok &= song.rec == 1u && !ui.layer && song.playing;
    bad += check("  REC held + PLAY: plays, and REC let go before HOLD still arms (no combo)", ok);
    ui_power_on();
    song.sel = 2;
    chain.running = 1;
    press(B_REC);
    ok = song.rec == 0u && msg_is("STOP TO RECORD");
    chain.running = 0;
    frames(400);
    btn_down(B_GLO); frame();
    press(B_REC);                                       /* REC in GLO's layer: a tap, armed */
    ok &= song.rec == 4u && ui.ly == LAYER_GLO;
    btn_up(B_GLO); frame();
    frames(400);
    btn_down(B_GLO); frames(512); press(B_REC);
    ok &= song.rec == 0u && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    bad += check("  a song playing: STOP TO RECORD; REC in another layer still arms / disarms", ok);

    /* F3 CLEAR: no dialog (no undo); an empty track: NOTHING TO CLEAR; a song playing: STOP TO EDIT */
    ui_power_on();
    song.sel = 1;
    my_steps(&trk[1]);
    go_page(GR_ROLL); frame();
    memcpy(before, trk[1].step, sizeof before);
    rec_down(); frame();
    key_down(white(0)); frame();                        /* a combo: the map at once, cleared */
    ok = ui.layer == LAYER_REC && seq_is_empty(&trk[1]) && !ui.confirm && msg_is("T2 CLEARED") && !gates() &&
         str_eq(ui.msg2, "SEQ+REC: ALL TRACKS");
    key_up(white(0)); frame();
    rec_up();
    ok &= !ui.layer && song.rec == 0u && !transport_req && seq_is_empty(&trk[1]);
    bad += check("REC + F3: CLEAR T2's sequence at once (no dialog, the key silent), nothing armed", ok);
    frames(400);
    (void)before;
    track_defaults_steps(&trk[1]);
    rec_down(); frame(); key_down(white(0)); frame();
    ok = msg_is("NOTHING TO CLEAR");
    key_up(white(0)); frame(); rec_up();
    my_steps(&trk[1]);
    chain.running = 1;
    rec_down(); frame(); key_down(white(0)); frame();
    ok &= msg_is("STOP TO EDIT") && !seq_is_empty(&trk[1]);
    key_up(white(0)); frame(); rec_up();
    chain.running = 0;
    song.playing = 1; song.rec = 2u;                    /* recording live: a retake, the arming and PLAY stay */
    rec_down(); frame(); key_down(white(0)); frame();
    ok &= seq_is_empty(&trk[1]) && song.rec == 2u && song.playing;
    key_up(white(0)); frame(); rec_up();
    song.playing = 0;
    bad += check("  empty: NOTHING TO CLEAR; a song playing: STOP TO EDIT; armed and playing: clears, records on", ok);

    /* G3: CLICK, the next value each press, MENU > AUDIO's values; KNOB 1 (TONIC: COUNT-IN and LEVEL in MENU only) */
    ui_power_on();
    ui_rec_prefs = 0;
    rp_apply();
    ok = 1;
    rec_down(); frames(512);
    {
        static const char *const NM[3] = {"CLICK REC", "CLICK ON", "CLICK OFF"};
        static const uint32_t WANT[3] = {1, 2, 0};
        for (i = 0; i < 3u; i++) {
            key_down(white(1)); frame(); key_up(white(1)); frame();
            ok &= menu_rec(0) == WANT[i] && msg_is(NM[i]) && !gates() &&
                  str_eq(menu_vname(MI_CLICK, menu_rec(0)), rl_name(0, rp_get(RL_F[0])));
        }
        key_down(white(2)); frame(); key_up(white(2)); frame();   /* A3: nothing now */
        key_down(white(3)); frame(); key_up(white(3)); frame();   /* B3: nothing now */
    }
    ok &= ui_rec_prefs == 0u;                           /* (round: as it was; A3 B3 changed nothing) */
    bad += check("REC + G3 CLICK OFF>REC>ON: MENU's values; A3 B3 nothing (COUNT-IN, LEVEL: MENU)", ok);
    key_down(white(1)); frame(); key_up(white(1)); frame();        /* CLICK REC */
    ok = click_mode == 1u && lys.rp_dirty;
    rec_up();
    ok &= !ui.layer && !lys.rp_dirty;                  /* (closed: settings_save) */
    {
        persist_t p = {0};
        uint8_t was = ui_rec_prefs;
        p.magic = PERSIST_MAGIC; p.panel = panel;
        settings_export(&p); ui_rec_prefs = 0;
        ok &= settings_import(&p, sizeof p) && ui_rec_prefs == was && menu_get(MI_CLICK) == 1u;
    }
    bad += check("  applied at once (click.c), saved when the layer closes, kept with the settings", ok);
    rec_down(); frames(512);
    turn(EN_K1, 1);                                     /* CLICK REC -> ON */
    turn(EN_K1, 5);                                     /* (stops at the end) */
    turn(EN_K2, -1); turn(EN_K3, -5); turn(EN_K4, 3);  /* nothing */
    ok = menu_get(MI_CLICK) == 2u && menu_get(MI_COUNTIN) == 0u && ui.home;
    rec_up();
    bad += check("  KNOB 1 CLICK, stopping at the end; KNOB 2..4 nothing; the page untouched", ok);

    /* OCT- puts nothing back (TONIC: no undo) */
    ui_power_on();
    ui_rec_prefs = 0; rp_apply();
    song.sel = 1;
    my_steps(&trk[1]);
    memcpy(before, trk[1].step, sizeof before);
    rec_down(); frames(512);
    key_down(white(1)); frame(); key_up(white(1)); frame();
    key_down(white(0)); frame(); key_up(white(0)); frame();
    ok = seq_is_empty(&trk[1]) && click_mode == 1u;
    press(B_OCTDN);
    ok &= seq_is_empty(&trk[1]) && click_mode == 1u && !msg_is("REC PUT BACK") && song.octave == 0 &&
          ui.layer == LAYER_REC;
    rec_up();
    bad += check("  OCT- puts nothing back (CLICK and the clear stay); no octave shift", ok);

    /* the LEDs: CLEAR breathes when there is something; CLICK lit when on, else breathing */
    ui_power_on();
    ui_rec_prefs = 0; rp_apply();
    my_steps(TSEL);
    rec_down(); frames(512);
    {
        uint32_t a = leds_at(0), b = leds_at(250), w0 = 1u << white(0), w1 = 1u << white(1);
        ok = ((a ^ b) & (w0 | w1)) == (w0 | w1) && !(a & b) && !((a | b) & ~(w0 | w1));
        rp_put(RP_CLICK, 2);
        a = leds_at(0); b = leds_at(250);
        ok &= (a & b) == w1 && (a ^ b) == w0;
        track_defaults_steps(TSEL);
        a = leds_at(0); b = leds_at(250);
        ok &= !((a | b) & w0);
    }
    rec_up();
    bad += check("  LEDs: CLEAR breathes (dark when empty), CLICK lit when on", ok);

    return bad;
}

/* #40: KNOB 4's DEPTH rises to the right (it fell); MENU > FX LATCH ON: a key with FX turns its effect on, again off,
 * nothing held needed; the macros stay after FX; FX + OCT- all off; the menu turns them off; FX's LED shows them;
 * OFF (the default, and every older setting): as before */
static int test_fx_latch(void)
{
    int bad = 0, ok;
    uint32_t lpf = white(3), m1 = black(0);
    ui_power_on();
    btn_down(B_FX); frame();
    turn(EN_K4, 5);  ok = perf_k[3] == 0;                         /* DEPTH 100 %: right stays at 100 */
    turn(EN_K4, -30); ok &= perf_k[3] == 30;                      /* left: 70 % */
    turn(EN_K4, 10); ok &= perf_k[3] == 20;                       /* right: 80 % */
    btn_up(B_FX); frame();
    bad += check("#40 FX + KNOB 4: DEPTH rises to the right, falls to the left", ok && !perf_k[3]);
    /* OFF (the default; a byte older settings leave 0) */
    ui_power_on();
    ok = !(fx_latch & 1u);
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame(); btn_up(B_FX); frame();
    ok &= !perf_latched && !perf_held;
    bad += check("  FX LATCH OFF by default: a key's effect ends when it is let go", ok);
    /* ON */
    ui_power_on();
    fx_latch = PREF_LATCH | PREF_ACCEL_OFF;
    frame();                                                      /* (ui_input hands the setting to the ISR) */
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame();
    turn(EN_K2, 30);
    ok = perf_latched == PF_BIT(PF_LPF) && !perf_held && ui.layer == LAYER_FX && str_eq(layer_head(), "[FX] LATCH") &&
         (layer_leds(0) >> lpf) & 1u;
    btn_up(B_FX); frame();
    ok &= perf_latched == PF_BIT(PF_LPF) && perf_k[1] == 30 && !ui.layer;
    ok &= perf_begin(CTL) && (perf_act & PF_BIT(PF_LPF));                 /* it runs with nothing held */
    bad += check("  FX LATCH ON: FX + B3 latches LPF, CRUSH stays at 30 after FX is let go", ok);
    ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_FX]] = 0u << 3 | 1u;
    go_home(); ui_leds();
    ok = (fm1_led[0] & 2u) != 0u;
    btn_down(B_FX); key_down(m1); frame(); key_up(m1); frame();   /* a mute latches too */
    ok &= perf_latched == (PF_BIT(PF_LPF) | PF_BIT(PF_M1));
    key_down(lpf); frame(); key_up(lpf); frame();                 /* B3 again: LPF off */
    ok &= perf_latched == PF_BIT(PF_M1);
    btn_up(B_FX); frame();
    bad += check("  the FX LED lit while something is on; a mute key latches; B3 again: LPF off", ok);
    btn_down(B_FX); frame(); turn(EN_K1, -40);
    ok = song.octave == 0 && oct_leds() == 1u;
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();
    ok &= !perf_latched && !perf_k[0] && !perf_k[1] && song.octave == 0 && msg_is("FX ALL OFF");
    btn_up(B_FX); frame();
    bad += check("  FX + OCT-: every latched effect and macro off (no octave)", ok);
    /* the quick combo: FX down and OCT- before HOLD, nothing else touched (it was nobody's: no ALL OFF, no octave) */
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame(); turn(EN_K2, 20); btn_up(B_FX); frame();
    frames(400);
    ok = perf_latched == PF_BIT(PF_LPF) && perf_k[1] == 20 && !ui.layer;
    btn_down(B_FX); frame(); btn_down(B_OCTDN); frame();
    ok &= ui.layer == LAYER_FX && song.octave == 0;               /* (a combo: the map at once) */
    btn_up(B_OCTDN); frame();
    ok &= !perf_latched && !perf_k[1] && song.octave == 0 && msg_is("FX ALL OFF");
    {   uint32_t h = ui.home, pg = ui.page;
        btn_up(B_FX); frame();
        ok &= !ui.layer && ui.home == h && ui.page == pg;   /* (no tap: no page) */
    }
    bad += check("  FX LATCH: FX + OCT- quick (before HOLD): all off, no octave, no page", ok);
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame(); btn_up(B_FX); frame();
    frames(400);
    btn_down(B_FX); btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();   /* (both in one frame) */
    ok = !perf_latched && song.octave == 0 && msg_is("FX ALL OFF");
    btn_up(B_FX); frame();
    btn_down(B_FX); frame(); frames(800);                         /* held past HOLD, then OCT- */
    ok &= ui.layer == LAYER_FX;
    press(B_OCTDN);
    ok &= song.octave == 0;
    btn_up(B_FX); frame();
    press(B_OCTDN);                                               /* no FX: the octave as always */
    ok &= song.octave == -1 && !ui.layer;
    press(B_OCTUP);
    ok &= song.octave == 0;
    bad += check("  FX LATCH: FX + OCT- in one frame / held: no octave; OCT- alone: the octave", ok);
    fx_latch = PREF_ACCEL_OFF;
    frame();
    btn_down(B_FX); frame(); btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();
    ok = song.octave == -1 && ui.layer == LAYER_FX;               /* LATCH OFF: FX's HOLD layer, OCT- is the octave */
    {   uint32_t h = ui.home, pg = ui.page;
        btn_up(B_FX); frame();
        ok &= !ui.layer && ui.home == h && ui.page == pg;
    }
    btn_down(B_FX); frame(); frames(800); press(B_OCTUP); btn_up(B_FX); frame();
    ok &= song.octave == 0;
    btn_down(B_FX); frame(); btn_up(B_FX); frame();               /* FX tap: its page still */
    ok &= !ui.home && cur_page()->fam == FAM_FX;
    go_home();
    frames(400);
    btn_down(B_GLO); frame(); btn_down(B_OCTDN); frame();          /* a SET layer the same: GLO + OCT- quick */
    ok &= ui.layer == LAYER_GLO;
    btn_up(B_OCTDN); frame();
    ok &= song.octave == 0 && !msg_is("MIX PUT BACK");
    btn_up(B_GLO); frame();
    ok &= ui.home && !ui.layer;
    bad += check("  FX LATCH OFF: FX + OCT- quick / held shifts the octave (no page); FX tap: the FX page; "
                 "GLO + OCT- quick: no octave, nothing put back", ok);
    fx_latch = PREF_LATCH | PREF_ACCEL_OFF;
    frame();
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame(); btn_up(B_FX); frame();
    hold(B_HOME);
    ok = ui.menu && !perf_latched;
    ui.menu_sel = MI_LATCH;
    press(B_OCTDN); ok &= !(fx_latch & 1u) && ui.menu;
    press(B_OCTUP); ok &= (fx_latch & 1u) && ui.menu;
    turn(EN_K1, -1); ok &= !(fx_latch & 1u);
    turn(EN_K1, 1); ok &= (fx_latch & 1u) != 0u;
    turn(EN_K1, -1); ok &= !(fx_latch & 1u);
    press(B_HOME);
    bad += check("  the menu turns the latched effects off; MENU > FX LATCH: OCT+ / KNOB 1 right ON, OCT- / left OFF",
                 ok && !ui.menu);
    {   /* (0.6.44) FX held past HOLD and let go with nothing used: locked open, as the macros; the keys toggle the
         * effects and the knobs are FX's without FX held; OCT- all off; FX again: unlocked, the effects stay */
        fx_latch = PREF_LATCH | PREF_ACCEL_OFF;
        go_home(); frames(400);
        btn_down(B_FX); frames(800); btn_up(B_FX); frames(64);
        ok = fx_lock && ui.layer == LAYER_FX;
        key_down(lpf); frame(); key_up(lpf); frame();
        turn(EN_K2, 10);
        ok &= perf_latched == PF_BIT(PF_LPF) && perf_k[1] == 10 && ui.layer == LAYER_FX;
        press(B_OCTDN);
        ok &= !perf_latched && !perf_k[1] && song.octave == 0;
        key_down(lpf); frame(); key_up(lpf); frame();
        press(B_FX); frames(64);
        ok &= !fx_lock && !ui.layer && perf_latched == PF_BIT(PF_LPF) && ui.home;
        key_down(lpf); frame(); key_up(lpf); frame();                 /* (unlocked: the key plays its note) */
        ok &= perf_latched == PF_BIT(PF_LPF);
        perf_latched = 0;
        bad += check("  FX LATCH: FX held and let go unused locks the FX layer open (keys, knobs, OCT- ALL OFF); FX again "
                     "unlocks, the effects stay", ok);
    }
    {   /* kept with the settings, in the favorites record (an older one: OFF) */
        persist_t p;
        fx_latch = PREF_LATCH | PREF_ACCEL_OFF;
        settings_export(&p);
        fx_latch = PREF_ACCEL_OFF;
        ok = settings_import(&p, (int)sizeof p) == 1 && (fx_latch & 1u);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                                    /* PER3: no favorites */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(fx_latch & 1u);
        bad += check("  FX LATCH saved with the settings; settings from before it: OFF", ok);
    }
    fx_latch = PREF_ACCEL_OFF;
    return bad;
}

/* #48: the note divisions step by their length, longest first (triplets between their neighbours), on every list
 * that has them (SEQ DIV, ARP RATE, the delay's TIME, the SLICER's RATE); the stored values and their sound stay */
static int test_div_order(void)
{
    int bad = 0, ok = 1;
    uint32_t id, c, k, found = 0;
    static const param_desc_t *const LISTS[] = {&TP[P_SDIV], &TP[P_ARATE], &GP[G_DTIME], &GP[G_SLRATE]};   /* (0.6.9: the bus's) */
    ui_power_on();
    for (k = 0; k < NELEM(LISTS); k++) {           /* left end -> right end: each one shorter than the one before */
        const param_desc_t *d = LISTS[k];
        int32_t v = d->def, n, seen = 0;
        uint32_t prev = 0xFFFFFFFFu, len;
        for (n = 0; n < 40; n++) v = param_turn(d, v, -1);
        for (n = 0; n <= (d == &GP[G_DTIME] ? DT_FREE - 1 : d->max - d->min); n++) {   /* (TIME: its divisions; the
                                                         * free times past them: test_dtime_free) */
            len = d->names == N_SLDIV ? 1000000u / SL_DEN[v] : div_samples((uint32_t)v);
            if (!(len < prev && enum_rank(d, v) == d->min + n && !((seen >> v) & 1))) {
                printf("  %s: %s (value %d, place %d, length %u) after %u\n", d->label, d->names[v], (int)v,
                       (int)enum_rank(d, v), len, prev);
                ok = 0;
            }
            seen |= 1 << v;
            prev = len;
            v = param_turn(d, v, 1);
        }
        ok &= seen == (d == &GP[G_DTIME] ? (1 << DT_FREE) - 1 : (1 << (d->max - d->min + 1)) - 1);
    }
    {   /* (JIANT 0.4) TIME past the last division (1/32): the free times, 5 ms .. 1.48 s, each longer, the end holds */
        int32_t v = 3, n;
        uint32_t prev = 0;
        int okf = 1;
        for (n = 0; n < 48; n++) {
            v = param_turn(&GP[G_DTIME], v, 1);
            okf &= v == DT_FREE + n && DTIME_MS[n] > prev;
            prev = DTIME_MS[n];
        }
        okf &= param_turn(&GP[G_DTIME], v, 1) == v && DTIME_MS[0] == 5u && DTIME_MS[47] == 1480u &&
               str_eq(N_DTIME[DT_FREE], "5ms") && str_eq(N_DTIME[57], "1.48s");
        song.g[G_DTIME] = DT_FREE + 47;
        okf &= delay_samples() == 1480u * 441u / 10u;
        song.g[G_DTIME] = 1;
        bad += check("delay TIME: past 1/32 the free times 5 ms .. 1.48 s, each longer; the delay that long", okf);
    }
    ok &= str_eq(N_DIV[2], "1/16") && str_eq(N_DIV[4], "8T") && str_eq(N_DIV[9], "4BAR") && TP[P_SDIV].def == 2;
    ok &= param_turn(&TP[P_SDIV], 2, 1) == 5 && param_turn(&TP[P_SDIV], 2, -1) == 4 &&   /* 1/16: right 16T, left 8T */
          param_turn(&TP[P_SDIV], 3, 1) == 3 && param_turn(&TP[P_SDIV], 9, -1) == 9 &&   /* the ends hold */
          param_turn(&TP[P_SDIV], 2, -6) == 8 && param_turn(&TP[P_SDIV], 2, 40) == 3;    /* 6 left: 2BAR; far: clamped */
    bad += check("#48 DIV / RATE / TIME / SLICER RATE step longest -> shortest, every value once, the ends hold", ok);
    ok = 1;                                         /* the real knob on the pages that show them, the gauge follows */
    for (id = 0; id < NPAGES; id++)
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d;
            ui_power_on();
            ui.home = 0; ui.page = (uint8_t)id; page_entered();
            if (!page_visible(id) || !(d = page_desc(cur_page(), c, &vp)) || !vp || !enum_order(d))
                continue;
            found++;
            *vp = d->names != N_SLDIV ? 2 : 1;      /* 1/16 */
            turn(EN_K1 + c, 1);
            ok &= *vp == (d->names != N_SLDIV ? 5 : 4);  /* 16T */
            turn(EN_K1 + c, -2);
            ok &= *vp == (d->names != N_SLDIV ? 4 : 3);  /* 8T */
        }
    bad += check("#48 the pages' knobs step the divisions by length (SEQ, ARP, DLY, SLICER)", ok && found >= 4u);
    return bad;
}

/* #52 KNOB ACCEL (JIANT: ON by default; MENU > KNOB ACCEL OFF): a sustained fast turn of a wide value goes x2 .. x16
 * by its speed (capped by the range: x2 up to 32, x4 up to 64, x8 up to 100, x16 above); a slow turn, the start of a
 * turn, a lone quick detent and a reversal are one step per detent; the sign is always the detents' */
static uint32_t acc_ms(uint32_t role, const uint16_t *gaps, uint32_t n, int32_t dir, int32_t range, int32_t *out)
{
    uint32_t k, sum = 0;
    for (k = 0; k < n; k++) {
        fm1_ms += gaps[k];
        out[k] = accel(role, dir, range);
        sum += (uint32_t)(out[k] < 0 ? -out[k] : out[k]);
    }
    return sum;
}
static int test_knob_accel(void)
{
    int bad = 0, ok;
    uint32_t k, seed = 12345u;
    int32_t o[16];
    static const uint16_t SLOW[8] = {500, 80, 70, 65, 61, 90, 80, 70}, F40[8] = {500, 40, 40, 40, 40, 40, 40, 40},
                          F20[8] = {500, 20, 20, 20, 20, 20, 20, 20}, F10[8] = {500, 10, 10, 10, 10, 10, 10, 10},
                          BOUNCE[8] = {500, 2, 300, 1, 300, 3, 300, 2};
    ui_power_on();
    ui_prefs = PREF_ACCEL_OFF;
    ok = acc_ms(EN_K1, F10, 8, 1, 127, o) == 8u;                  /* OFF */
    ui_prefs = 0;                                                  /* ON (the device's default) */
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    ok &= acc_ms(EN_K1, SLOW, 8, 1, 127, o) == 8u && acc_ms(EN_K3, BOUNCE, 8, 1, 127, o) == 8u;
    bad += check("#52 KNOB ACCEL: ON by default; OFF 1 step a detent; ON, slow (>= 60 ms a detent) and lone quick ones: 1 each", ok);
    fm1_ms += 1000;
    acc_ms(EN_K2, F40, 8, -1, 127, o);
    ok = o[0] == -1 && o[1] == -1 && o[2] == -2 && o[7] == -2;      /* 40 ms a detent: x2 from the third */
    acc_ms(EN_K1, F20, 8, 1, 127, o);
    ok &= o[0] == 1 && o[1] == 1 && o[2] == 4 && o[7] == 4;         /* 20 ms: x4 */
    acc_ms(EN_K1, F10, 8, 1, 64, o);                               /* (a pause of 500 ms first: a new turn) */
    ok &= o[0] == 1 && o[1] == 1 && o[2] == 4 && o[7] == 4;         /* 10 ms: x8, capped x4 (a range <= 64) */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 64) == -1;                /* reversed: one step, the right sign */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 64) == -1;                /* (the turn starts again) */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 64) == -4;
    fm1_ms += 30; ok &= accel(EN_K1, -3, 64) == -12;               /* 3 detents in one read (10 ms each): x4 */
    bad += check("#52 KNOB ACCEL ON: 40 ms a detent x2, 20 ms x4, 10 ms x8 (capped by the range) from a turn's third detent; reversal x1", ok);
    {   /* a range above 100: 10 ms x8, 5 ms and less x16; a flick of 12 detents in ~120 ms sweeps most of 0..127 */
        static const uint16_t FLICK[12] = {500, 16, 12, 10, 8, 7, 7, 8, 10, 12, 16, 20}, F5[8] = {500, 5, 5, 5, 4, 5, 5, 5};
        uint32_t f;
        fm1_ms += 1000;
        f = acc_ms(EN_K2, FLICK, 12, 1, 127, o);
        ok = f >= 70u && f <= 110u && o[0] == 1 && o[1] == 1;
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F10, 8, -1, 200, o) == 2u + 6u * 8u && o[7] == -8;
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F5, 8, 1, 127, o) == 2u + 6u * 16u;
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F5, 8, 1, 100, o) == 2u + 6u * 8u;      /* (a range <= 100: x8 at most) */
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F5, 8, 1, 24, o) == 2u + 6u * 2u;       /* (a range <= 32: x2 at most) */
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F5, 8, 1, 0, o) == 8u;                  /* (a list) */
        printf("  a flick of 12 detents over %u ms on 0..127: %u steps\n", 126u, (unsigned)f);
        bad += check("KNOB ACCEL ON over a range above 100: 10 ms x8, 5 ms x16; a 12-detent flick sweeps 70..110 of 127", ok);
    }
    fm1_ms += 1000; acc_ms(EN_K4, F10, 8, 1, 16, o);
    ok = o[7] == 1;                                                /* a narrow range (<= 16): never */
    fm1_ms += 1000; acc_ms(EN_K4, F10, 8, 1, 0, o);
    ok &= o[7] == 1;                                               /* a list of names (range 0 at the callers) */
    for (k = 0; k < 20000u && ok; k++) {                           /* random turns: the sign is the detents', at */
        int32_t s, r, gap;                                         /* most x16, a slow detent exactly one step */
        seed = seed * 1103515245u + 12345u;
        s = (int32_t)((seed >> 16) % 7u) - 3;
        gap = (int32_t)((seed >> 8) % 120u);
        fm1_ms += (uint32_t)gap;
        r = accel(EN_K1 + (seed & 3u), s, 127);
        ok &= s ? (r > 0) == (s > 0) && r * s <= 16 * s * s && r * s >= s * s : r == 0;
        if (gap >= 60 * (s < 0 ? -s : s) && r != s) ok = 0;
    }
    bad += check("#52 KNOB ACCEL: never on narrow values or lists; 20000 random reads keep the sign, x1..x16, slow = x1", ok);
    {   /* the real knob: LEVEL on MIXER (0..127), a detent a frame (16 ms): 1 + 1 + 5 x 6 = 32 for 8 detents; OFF: 8 */
        int16_t v0;
        ui_power_on(); ui_prefs = 0;
        go_title("MIXER");
        TSEL->p[P_LEVEL] = 40; v0 = TSEL->p[P_LEVEL];
        fm1_ms += 1000;
        for (k = 0; k < 8u; k++) turn(EN_K1, 1);
        ok = TSEL->p[P_LEVEL] - v0 == 32;
        ui_prefs = PREF_ACCEL_OFF; TSEL->p[P_LEVEL] = 40; fm1_ms += 1000;
        for (k = 0; k < 8u; k++) turn(EN_K1, 1);
        ok &= TSEL->p[P_LEVEL] == 48;
        bad += check("#52 KNOB ACCEL on MIXER LEVEL: a fast turn of 8 detents moves 32 (OFF: 8)", ok);
    }
    {   /* #126 the FX layer's KNOB 1 (FILTER -100..100): a flick of 12 detents 10 ms apart; OFF: 12; slow (70 ms): 12;
         * 6 right then 6 left fast: back to 0; GLO's LEVEL too */
        static const uint32_t GAP[4] = {10u, 10u, 70u, 10u};
        static const uint32_t ON[4] = {1u, 0u, 1u, 1u};
        int32_t got[4], g;
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            ui_power_on();
            ui_prefs = ON[c] ? 0u : PREF_ACCEL_OFF;
            go_home(); frames(64);
            btn_down(B_FX); frames(560);
            for (k = 0; k < 12u; k++) {
                host_enc[panel.enc[EN_K1]] += (c == 3u && k >= 6u ? -1 : 1) * panel.dir[EN_K1];
                fm1_ms += GAP[c] - 16u;                         /* (frame() adds 16) */
                frame();
            }
            got[c] = perf_k[0];
            btn_up(B_FX); frames(400);
        }
        printf("  #126 FX FILTER, 12 detents: ON 10 ms %d, OFF %d, ON 70 ms %d, ON 6 right 6 left %d\n", (int)got[0],
               (int)got[1], (int)got[2], (int)got[3]);
        ok = got[0] >= 50 && got[1] == 12 && got[2] == 12 && got[3] == 0;
        ui_power_on(); ui_prefs = 0;
        go_home(); frames(64);
        trk[1].p[P_LEVEL] = 0;
        btn_down(B_GLO); frames(560);
        for (k = 0; k < 12u; k++) {
            host_enc[panel.enc[EN_K2]] += panel.dir[EN_K2];
            fm1_ms -= 6u;
            frame();
        }
        g = trk[1].p[P_LEVEL];
        btn_up(B_GLO); frames(400);
        ok &= g > 12 + 12;
        bad += check("#126 KNOB ACCEL on the FX layer (FILTER: a 12-detent flick >= 50, OFF 12, slow 12) and GLO LEVEL", ok);
    }
    {   /* saved with the settings; settings from before it: ON (the bit means OFF) */
        persist_t p;
        ui_prefs = PREF_ACCEL_OFF;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_ACCEL_OFF;
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & PREF_ACCEL_OFF);
        bad += check("#52 KNOB ACCEL OFF saved with the settings; settings from before it: ON", ok);
    }
    ui_prefs = PREF_ACCEL_OFF;
    return bad;
}

/* #51 ("a melody appears when I change presets"): browsing every sound in the list (every engine's presets, then user
 * presets, one of them holding a pattern), on HOME and the PRESETS page, stopped and playing, on an empty track: no
 * step is written, the ARP stays off, nothing is held, the transport is not started, no message about a pattern. (In
 * 0.9 a preset carried a 16-step pattern and replaced the steps; since 1.0 only SEQ > PHRASES loads one.) */
static int test_browse_no_pattern(void)
{
    int bad = 0, ok = 1;
    uint32_t pass, n, total, loads = 0, pat_hints = 0;
    track_t *t;
    for (pass = 0; pass < 4u; pass++) {
        ui_power_on();
        t = TSEL;
        my_steps(t); t->p[P_SDIV] = 2;
        up_store(3, "MINE");                          /* a user preset that holds a pattern */
        up_store(5, "PLAIN");
        track_defaults_steps(t);
        ok &= seq_is_empty(t) && up_has_pat(3);
        if (pass & 1u) song.playing = 1;              /* playing: the other tracks run, this one is empty */
        if (pass & 2u) go_page(GR_BROWSE);
        preset_pos(&total);
        for (n = 0; n < total + 2u; n++) {
            turn(EN_PRESET, 1);
            loads++;
            pat_hints += preset_pat_hint() >= 0;
            if (!seq_is_empty(t) || t->p[P_AMODE] || t->nheld || (!(pass & 1u) && (song.playing || transport_req))) {
                printf("  pass %u, sound %u (%s %u): steps %d ARP %d held %u playing %d\n", pass, n,
                       ENGINES[t->eng_req % NENGINES]->name, t->preset, !seq_is_empty(t), t->p[P_AMODE], t->nheld,
                       song.playing);
                ok = 0;
                break;
            }
        }
    }
    if (!ok || loads < 200u || pat_hints <= 10u) printf("  ok %d loads %u hints %u\n", ok, loads, pat_hints);
    bad += check("#51 every sound browsed (HOME, PRESETS; stopped, playing): no step, ARP off, nothing held, no start",
                 ok && loads >= 200u && pat_hints > 10u);
    return bad;
}

/* MENU > USB LEVEL: MASTER (the default, every older setting) / FIXED; the audio follows at once (audio_test: the levels) */
static int test_usb_level(void)
{
    int bad = 0, ok;
    ui_power_on();
    frame();
    ok = !fx_usb_fixed && str_eq(MI_NAME[MI_USB], "USB LEVEL") && str_eq(menu_flag(MI_USB)->name[0], "MASTER");
    hold(B_HOME);
    ui.menu_sel = MI_USB;
    ui.force = 1; ui_draw();
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed;      /* right: FIXED */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed;   /* left: MASTER */
    press(B_OCTUP); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed && ui.menu;
    press(B_OCTDN); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed && ui.menu;
    press(B_OCTUP);
    press(B_HOME);
    ok &= !ui.menu && fx_usb_fixed;
    {
        persist_t p;
        settings_export(&p);
        ui_prefs = 0;
        ok &= settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_USB_FIXED;
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & ~PREF_ACCEL_OFF);
        frame();
        ok &= !fx_usb_fixed;
    }
    bad += check("MENU > USB LEVEL: MASTER by default, KNOB 1 / OCT+ FIXED, left / OCT- MASTER, saved, the audio follows", ok);
    ui_prefs = 0;
    frame();
    return bad;
}

/* 1.1 (Discussion #131) MENU > AUDIO CLICK (OFF REC ON), CLICK LEVEL (LOW MID HIGH), COUNT-IN (OFF 1 BAR 2 BARS): OFF / MID /
 * OFF by default and in every older setting; a knob applies at once (click.c, seq.c); saved and read back, a stored 3
 * (no value) the default. The count-in from the UI: REC while stopped arms and counts in, the screen counts 4 3 2 1 (the
 * beat's cell moves), PLAY's green LED on each beat (its own dark), the STEP keys' entry paused, PLAY stops it (the page
 * back); at its end the sequencer plays and the page is back */
static uint16_t px_at(uint32_t x, uint32_t y) { return host_screen[y * 240u + x]; }
static void cin_beat(void)                       /* the audio until the count-in's next beat (or its end) */
{
    uint32_t left = cin_left, k;
    for (k = 0; k < 4000u && cin_left == left && seq_counting(); k++)
        events_block(CTL);
}
/* (0.6.80) channel 16's PROGRAM CHANGE: 0..3 a section (stopped: at once), 16 + n the project n + 1; never a track's
 * sound, whatever ROUTE */
/* (0.6.81) SCL > CHORD: a key plays the chord of its degree in the scale (TRIAD, 7TH), a shape on any key (MAJ7), the
 * root alone when OFF, on MONO and on a kit; LEAD the inversion nearest the last chord; STRUM the notes in one by one;
 * the key's release ends every note it started (those strummed in later too, none still to come) */
static uint32_t chord_sounding(const track_t *t, uint8_t *nn)
{
    uint32_t i, n = 0, j;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate) {
            for (j = 0; j < n && nn[j] != t->v[i].note; j++) {}
            if (j == n && n < 8u) nn[n++] = t->v[i].note;
        }
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && nn[j] < nn[j - 1]; j--) { uint8_t x = nn[j]; nn[j] = nn[j - 1]; nn[j - 1] = x; }
    return n;
}
static int test_chord(void)
{
    int bad = 0, ok;
    track_t *t;
    uint8_t nn[8];
    uint32_t n, k = white(4), i;                        /* C4 (white keys from F3) */
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    t->p[P_VOICE] = V_POLY;
    t->p[P_ROOT] = 0; t->p[P_SCALE] = 2;                /* C minor */
    t->p[P_QUANT] = QN_OFF; t->p[P_AMODE] = 0;
    events_block(CTL);
    key_down(k); events_block(CTL); n = chord_sounding(t, nn); key_up(k); events_block(CTL);
    ok = n == 1u && nn[0] == kb_note[k];
    bad += check("CHORD OFF: a key plays its note alone", ok);
    t->p[P_CHRD] = CD_TRI;
    key_down(k); events_block(CTL); n = chord_sounding(t, nn);
    ok = n == 3u && nn[1] - nn[0] == 3 && nn[2] - nn[0] == 7;   /* C Eb G */
    key_up(k); events_block(CTL);
    ok &= chord_sounding(t, nn) == 0u;
    bad += check("CHORD TRIAD in C minor: C4 plays C Eb G; the release ends all three", ok);
    t->p[P_CHRD] = CD_SEV;
    key_down(white(5)); events_block(CTL); n = chord_sounding(t, nn);   /* D: D F Ab C (half diminished) */
    ok = n == 4u && nn[1] - nn[0] == 3 && nn[2] - nn[0] == 6 && nn[3] - nn[0] == 10;
    key_up(white(5)); events_block(CTL);
    t->p[P_CHRD] = CD_MAJ7;
    key_down(white(5)); events_block(CTL); n = chord_sounding(t, nn);   /* D MAJ7 whatever the scale */
    ok &= n == 4u && nn[1] - nn[0] == 4 && nn[2] - nn[0] == 7 && nn[3] - nn[0] == 11;
    key_up(white(5)); events_block(CTL);
    bad += check("CHORD 7TH: the scale's seventh on D (D F Ab C); MAJ7: D F# A C# on any scale", ok);
    t->p[P_CHRD] = CD_TRI; t->p[P_CVOI] = CV_LEAD;
    {
        int32_t mid = 0, d;
        key_down(k); events_block(CTL); n = chord_sounding(t, nn);   /* C minor, as near the last chord as it can */
        for (i = 0; i < n; i++) mid += nn[i];
        mid /= (int32_t)(n ? n : 1u);
        key_up(k); events_block(CTL);
        key_down(white(7)); events_block(CTL); n = chord_sounding(t, nn);   /* F minor: every tone within a fifth */
        ok = n == 3u;
        for (i = 0; i < n; i++) { d = nn[i] - mid; ok &= d <= 6 && d >= -6; }
        key_up(white(7)); events_block(CTL);
    }
    bad += check("VOICING LEAD: F minor after C minor is the inversion nearest it (every tone within a fifth of its middle)", ok);
    t->p[P_CVOI] = CV_CLOSE; t->p[P_CSTR] = 40;         /* ~20 ms apart */
    key_down(k);
    for (i = 0; i < 2u; i++) events_block(CTL);
    ok = chord_sounding(t, nn) == 1u;
    for (i = 0; i < 40u; i++) events_block(CTL);       /* ~27 ms: the second */
    ok &= chord_sounding(t, nn) == 2u;
    key_up(k);
    for (i = 0; i < 80u; i++) events_block(CTL);
    ok &= chord_sounding(t, nn) == 0u;                  /* (the third never came) */
    bad += check("STRUM: the notes one after the other; let go before the last: everything ends, none comes later", ok);
    t->p[P_CSTR] = 0; t->p[P_VOICE] = V_MONO;
    key_down(k); events_block(CTL); n = chord_sounding(t, nn); key_up(k); events_block(CTL);
    bad += check("CHORD on a MONO track: the root alone", n == 1u);
    ui_power_on();
    return bad;
}

/* (0.6.81) SCL TRANS: the sequence's notes sound TRANS semitones away as the keys' do (it moved the keys only) */
static int test_trans_seq(void)
{
    int ok, bad = 0;
    uint32_t i, f = 0;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    t->p[P_VOICE] = V_POLY;
    t->p[P_TRANS] = 2;
    t->step[0].time = ST_NOTE; t->step[0].n = 1; t->step[0].note[0] = 60; t->step[0].vel = 100;
    transport_req = 1; ui_input();
    for (i = 0; i < 8u; i++) events_block(CTL);
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == 60u) f = (uint32_t)t->v[i].pitch16;
    ok = f == 62u * 16u;
    transport_req = 2; ui_input();
    bad += check("SCL TRANS +2: the sequence's C4 sounds D4 (as a key's would)", ok);
    t->p[P_TRANS] = 0;
    ui_power_on();
    return bad;
}
/* (0.6.81) SEQ > NOTE: FOCUS a note of the cursor's chord, PITCH it alone, MOVE it into the next step, LEN the chord's
 * TIEs, EDIT deletes the note in focus (the chord stays); MOVE with the whole chord in focus takes its length along */
static int test_note_page(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    t->p[P_SLEN] = 16;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0].time = ST_NOTE; t->step[0].n = 3; t->step[0].note[0] = 60; t->step[0].note[1] = 64; t->step[0].note[2] = 67;
    t->step[0].vel = 100;
    go_title("NOTE"); frame();
    cursor_set(0);
    turn(EN_K1, 1); turn(EN_K1, 1);                    /* FOCUS: note 2 (E4) */
    ok = ui.pr_focus == 2u;
    turn(EN_K2, 1);                                     /* PITCH: E4 -> F4, the others stay */
    ok &= t->step[0].note[1] == 65u && t->step[0].note[0] == 60u && t->step[0].note[2] == 67u;
    turn(EN_K3, 1);                                     /* MOVE: F4 into step 2 */
    ok &= t->step[0].n == 2u && t->step[1].time == ST_NOTE && t->step[1].n == 1u && t->step[1].note[0] == 65u &&
          ui.cursor == 1u;
    bad += check("SEQ > NOTE: FOCUS one note of a chord, PITCH it alone, MOVE it into the next step", ok);
    cursor_set(0);                                      /* the chord C4 G4: LEN 3, then MOVE it right (F4 changes places) */
    turn(EN_K4, 1); turn(EN_K4, 1);
    ok = t->step[0].time == ST_NOTE && t->step[1].time == ST_NOTE && t->step[2].time == ST_REST;   /* (blocked by F4) */
    t->step[1].time = ST_REST; t->step[1].n = 0;
    turn(EN_K4, 1); turn(EN_K4, 1);
    ok &= t->step[1].time == ST_TIE && t->step[2].time == ST_TIE && t->step[3].time != ST_TIE;
    turn(EN_K3, 1);                                     /* the chord and its TIEs a step right */
    ok &= t->step[0].time == ST_REST && t->step[1].time == ST_NOTE && t->step[1].n == 2u && t->step[3].time == ST_TIE &&
          ui.cursor == 1u;
    bad += check("SEQ > NOTE: LEN adds TIEs over empty steps only; MOVE the chord takes its length along", ok);
    turn(EN_K1, 1);                                     /* FOCUS note 1, EDIT: deleted, the chord's other note stays */
    press(B_EDIT); frame();
    ok = t->step[1].time == ST_NOTE && t->step[1].n == 1u && t->step[1].note[0] == 67u;
    bad += check("SEQ > NOTE: EDIT deletes the note in focus, the rest of the chord stays", ok);
    ui_power_on();
    return bad;
}
/* (0.6.81) a POLY chord with SLD glides into the next: its voices go on (not struck again), their pitch moving to the
 * new notes low to high; 3 -> 4 notes: the extra one starts from the nearest old note and fades in; 4 -> 3: the one
 * left glides on, released */
static int test_chord_glide(void)
{
    int bad = 0, ok;
    track_t *t;
    uint32_t i, k, ages[NVOICE], nact = 0, fresh = 0, fading = 0, rel = 0;
    ui_power_on();
    t = &trk[0];
    set_engine_of(t, ENGI_ANALOG); apply_preset_to(t, 1); t->engine = t->eng_req;
    t->p[P_VOICE] = V_POLY; t->p[P_SLEN] = 4; t->p[P_SGATE] = 120; t->p[P_SUS] = 120;
    for (i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; }
    t->step[0].time = ST_NOTE; t->step[0].n = 3; t->step[0].note[0] = 60; t->step[0].note[1] = 64; t->step[0].note[2] = 67;
    t->step[0].vel = 100; t->step[0].flags = SF_SLIDE;
    t->step[1].time = ST_NOTE; t->step[1].n = 4; t->step[1].note[0] = 62; t->step[1].note[1] = 65; t->step[1].note[2] = 69;
    t->step[1].note[3] = 72; t->step[1].vel = 100; t->step[1].flags = SF_SLIDE;
    t->step[2].time = ST_NOTE; t->step[2].n = 3; t->step[2].note[0] = 60; t->step[2].note[1] = 64; t->step[2].note[2] = 67;
    t->step[2].vel = 100;
    for (k = 0; k < NVOICE; k++) t->v[k].active = 0;    /* (0.6.87: no tail of an earlier test's notes still releasing, */
    memset(fg_set, 0, sizeof fg_set);                   /* nor the PLAYHEADS an earlier one left on: undo keeps them) */
    transport_req = 1; ui_input();
    for (i = 0; i < 8u; i++) events_block(CTL);
    for (k = 0; k < NVOICE; k++) ages[k] = t->v[k].active ? t->v[k].age : 0u;
    while (t->seq_idx == 0u) events_block(CTL);         /* into step 2 */
    for (i = 0; i < 2u; i++) events_block(CTL);
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &t->v[k];
        if (!v->active || !v->gate) continue;
        nact++;
        if (ages[k] && v->age != ages[k]) continue;
        if (!ages[k] || v->age != ages[k]) { fresh++; fading += v->fin_d > 0; }
    }
    ok = nact == 4u && fresh == 1u && fading == 1u;
    {   /* the old voices: same ages, gliding toward D F A (pitch_cur between) */
        uint32_t kept = 0;
        for (k = 0; k < NVOICE; k++)
            if (ages[k] && t->v[k].active && t->v[k].age == ages[k] && t->v[k].pitch16 != t->v[k].pitch_cur) kept++;
        ok &= kept == 3u;
    }
    bad += check("chord glide (SLD, POLY): 3 -> 4 notes, three voices glide on unstruck, the fourth starts and fades in", ok);
    while (t->seq_idx == 1u) events_block(CTL);         /* into step 3: four -> three */
    for (i = 0; i < 2u; i++) events_block(CTL);
    for (k = 0, nact = 0; k < NVOICE; k++) {
        if (t->v[k].active && t->v[k].gate) nact++;
        else if (t->v[k].active && t->v[k].stage == 3u) rel++;   /* (C5 left: the nearest new note is C5 itself) */
    }
    ok = nact == 3u && rel == 1u;
    bad += check("chord glide: 4 -> 3 notes, the one left goes to its nearest new note as it is released", ok);
    transport_req = 2; ui_input();
    for (i = 0; i < 40u; i++) events_block(CTL);
    for (i = 0; i < 3u; i++) t->step[i].flags = 0;    /* (0.6.81) no SLD: GLIDE on the POLY track glides by itself */
    t->p[P_GLIDE] = 64; t->p[P_SGATE] = 60;            /* (a short GATE: held to the next chord all the same) */
    transport_req = 1; ui_input();
    for (i = 0; i < 8u; i++) events_block(CTL);
    for (k = 0; k < NVOICE; k++) ages[k] = t->v[k].active && t->v[k].gate ? t->v[k].age : 0u;
    while (t->seq_idx == 0u) events_block(CTL);
    for (i = 0; i < 2u; i++) events_block(CTL);
    {
        uint32_t kept = 0;
        for (k = 0; k < NVOICE; k++)
            if (ages[k] && t->v[k].active && t->v[k].gate && t->v[k].age == ages[k]) kept++;
        ok = kept == 3u;
    }
    bad += check("chord glide by itself: GLIDE on a POLY track, no SLD, a short GATE: the chord's voices go on into the next", ok);
    transport_req = 2; ui_input();
    for (i = 0; i < 40u; i++) events_block(CTL);
    {   /* (0.6.82) a long slide: the next chord held over 14 TIEs, GLIDE 127: the voices glide over all of it, smooth */
        static int32_t gl_o[2 * CTL];
        int32_t p0 = -1, mid = -1, end = -1, tgt = 0;
        memset(fg_set, 0, sizeof fg_set);              /* (no PLAYHEADS: their notes would retrigger the voices) */
        t->p[P_SLEN] = 16; t->p[P_GLIDE] = 127; t->p[P_SGATE] = 120;
        for (i = 2; i < 16u; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_TIE; }
        t->step[1].n = 3;                              /* (D F A) */
        transport_req = 1; ui_input();
        for (i = 0; i < 8u; i++) mix_block(gl_o, CTL);
        while (t->seq_idx == 0u) mix_block(gl_o, CTL);
        mix_block(gl_o, CTL);
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active && t->v[k].gate && t->v[k].pitch16 != t->v[k].pitch_cur) { p0 = t->v[k].pitch_cur; tgt = t->v[k].pitch16; break; }
        while (t->seq_idx < 8u) mix_block(gl_o, CTL);
        if (k < NVOICE) mid = t->v[k].pitch_cur;
        while (t->seq_idx < 15u) mix_block(gl_o, CTL);
        for (i = 0; i < 400u && t->seq_idx == 15u; i++) mix_block(gl_o, CTL);
        if (k < NVOICE) end = t->v[k].pitch_cur;
        ok = k < NVOICE && mid != tgt && (mid - p0) * (tgt - mid) > 0 && end == tgt;
        bad += check("chord glide over the chord's TIEs (GLIDE 127): halfway at the 8th step, there by the last", ok);
        transport_req = 2; ui_input();
    }
    ui_power_on();
    return bad;
}
static int test_midi_song_pc(void)
{
    int bad = 0, ok;
    int32_t o[2 * CTL];
    uint32_t eng[NTRK], kk, s0;
    ui_power_on();
    s0 = song_cur;
    song.g[G_ROUTE] = 1;                                /* (SEL: every channel to the selected track, but not 16's PC) */
    sl_steps(70); section_store(1);                     /* section B: notes 70.. */
    sl_steps(80);                                       /* the music now: 80.. */
    for (kk = 0; kk < NTRK; kk++) eng[kk] = trk[kk].eng_req;
    midi_enqueue(0x0Cu | 0xCFu << 8 | 1u << 16, 1u);   /* PROGRAM CHANGE 1 on channel 16: section B */
    mix_block(o, CTL); frame();
    ok = trk[0].step[0].note[0] == 70 && live.cur == 1;
    for (kk = 0; kk < NTRK; kk++) ok &= trk[kk].eng_req == eng[kk];
    midi_enqueue(0x0Cu | 0xCFu << 8 | (16u + (s0 + 1u) % NSONG) << 16, 1u);   /* 16 + n: project n + 1 */
    mix_block(o, CTL); frame();
    ok &= song_cur == (s0 + 1u) % NSONG;
    midi_enqueue(0x0Cu | 0xCFu << 8 | (16u + s0) << 16, 1u);   /* and back */
    mix_block(o, CTL); frame();
    ok &= song_cur == s0;
    bad += check("PROGRAM CHANGE on channel 16: 0..3 a section (no track's sound), 16 + n project n + 1", ok);
    memset(proj_slot[1].raw, 0, 4);                     /* (section B emptied again) */
    live.cur = -1;
    song.g[G_ROUTE] = 0;
    ui_power_on();
    return bad;
}
static int test_click_menu(void)
{
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    frame();
    ok = !ui_rec_prefs && click_mode == CLICK_OFF && click_lvl == 1u && !cin_bars &&
         str_eq(MI_NAME[MI_CLICK], "CLICK") && str_eq(MI_NAME[MI_CLKLVL], "CLICK LEVEL") && str_eq(MI_NAME[MI_COUNTIN], "COUNT-IN") &&
         MI_TAB[MI_CLICK] == MTAB_AUDIO && MI_TAB[MI_CLKLVL] == MTAB_AUDIO && MI_TAB[MI_COUNTIN] == MTAB_AUDIO &&
         menu_get(MI_CLICK) == 0u && menu_get(MI_CLKLVL) == 1u && menu_get(MI_COUNTIN) == 0u;
    for (i = 0; i < 3u; i++)
        ok &= str_eq(menu_vname(MI_CLICK, i), (const char *[]){"OFF", "REC", "ON"}[i]) &&
              str_eq(menu_vname(MI_CLKLVL, i), (const char *[]){"LOW", "MID", "HIGH"}[i]) &&
              str_eq(menu_vname(MI_COUNTIN, i), (const char *[]){"OFF", "1 BAR", "2 BARS"}[i]);
    bad += check("MENU > AUDIO: CLICK OFF / REC / ON, CLICK LEVEL LOW / MID / HIGH, COUNT-IN OFF / 1 BAR / 2 BARS; OFF MID OFF by default", ok);
    hold(B_HOME);
    ui.menu_sel = MI_LOWCUT; ui.force = 1; ui_draw();
    turn(EN_PRESET, 1); turn(EN_PRESET, 1);
    ok = ui.menu_sel == MI_CLICK;
    turn(EN_K1, 1); ok &= click_mode == CLICK_REC;
    turn(EN_K2, 1); ok &= click_mode == CLICK_ON;
    turn(EN_K2, 1); ok &= click_mode == CLICK_ON;      /* (stops at the end) */
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_CLKLVL;
    press(B_OCTDN); ok &= click_lvl == 0u;
    press(B_OCTUP); press(B_OCTUP); ok &= click_lvl == 2u;
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_COUNTIN;
    turn(EN_K4, 1); ok &= cin_bars == 1u;
    turn(EN_K4, 1); ok &= cin_bars == 2u;
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_TRACKS;
    turn(EN_K1, -1); frame(); ok &= trk_live == 4u && !(ui_rec_prefs & 0x80u);   /* (0.6.18) TRACKS 4 */
    turn(EN_K1, 1); frame(); ok &= trk_live == NTRK && (ui_rec_prefs & 0x80u);   /* 6 */
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_LOWCUT;   /* (wraps within AUDIO) */
    press(B_HOME);
    ok &= !ui.menu;
    bad += check("MENU > AUDIO: PRESETS to CLICK, CLICK LEVEL, COUNT-IN, TRACKS; knobs / OCT step them, applied at once", ok);
    {   /* (0.6.18) TRACKS 4 LOW: T5 takes no note and sounds nothing (its voices off); TRACKS 6: it plays again */
        uint32_t k, on = 0, sel = song.sel;
        ui_rec_prefs &= (uint8_t)~0x80u; frame();
        trk_note_on(&trk[4], 60, 100);
        events_block(CTL);
        for (k = 0; k < NVOICE; k++) on |= trk[4].v[k].active | trk[4].v[k].gate;
        ok = trk_live == 4u && !on;
        trk_all_off(&trk[4]);
        ui_rec_prefs |= 0x80u; frame();
        trk_note_on(&trk[4], 60, 100);
        events_block(CTL);
        for (k = 0; k < NVOICE; k++) on |= trk[4].v[k].gate;
        ok &= trk_live == NTRK && on;
        trk_all_off(&trk[4]);
        events_block(CTL);
        song.sel = (uint8_t)sel; ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0; frame();
        bad += check("TRACKS 4 FULL: T5 asleep (no note, no voice); 6 wakes it", ok);
        ui_rec_prefs &= (uint8_t)~0x80u; frame();       /* (0.6.28) a note sent to it: the screen says why it is silent */
        ui.msg_t = 0; ui.msg[0] = 0; fm1_ms += 5000u;
        trk_note_on(&trk[4], 60, 100); frame();
        ok = msg_is("T5-T6 ASLEEP: ") && str_eq(ui.msg2, "GLO>SYSTEM>TRACKS 6");
        ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
        ui_rec_prefs |= 0x80u; frame();
        bad += check("TRACKS 4: a note for T5 says T5-T6 ASLEEP, GLO>SYSTEM>TRACKS 6", ok);
    }
    {   /* (0.6.19) GLO > SYSTEM's TRKS (KNOB 2): the same setting, both ways */
        go_title("SYSTEM");
        turn(EN_K1, -1); frame();
        ok = trk_live == 4u && !(ui_rec_prefs & 0x80u) && trks_ui == 0;
        turn(EN_K1, 1); frame();
        ok &= trk_live == NTRK && (ui_rec_prefs & 0x80u) && trks_ui == 1 && msg_is("6 TRACKS: MORE CPU");
        ui_rec_prefs &= (uint8_t)~0x80u; frame();
        ok &= trks_ui == 0;                             /* (MENU's change shown there) */
        ui_rec_prefs |= 0x80u; frame();
        ok &= load_ui == 2 && sys_load == LOAD_FULL;    /* (0.6.30) KNOB 2 LOAD: FULL, the default */
        turn(EN_K2, -1); frame();
        ok &= load_ui == 1 && sys_load == LOAD_LITE && ui_load == 1u && msg_is("LOAD LITE: LIGHTER FX");
        turn(EN_K2, -1); frame();
        ok &= load_ui == 0 && sys_load == LOAD_ECO && ui_load == 2u && trk_live == NTRK;   /* (TRACKS its own) */
        load_put(2u); frame();
        ok &= load_ui == 2 && sys_load == LOAD_FULL && !ui_load;
        ui.msg_t = 0; ui.msg[0] = 0; press(B_HOME); frame();
        bad += check("GLO > SYSTEM: KNOB 1 TRACKS 4 / 6, KNOB 2 LOAD ECO / LITE / FULL, the MENU's settings both ways", ok);
    }
    {   /* (0.6.57) LOAD LITE / ECO: the panel's animation at most every 50 / 70 ms; FULL as it changes; a new page
         * at once; the values snap (no rolling digits) */
        uint8_t home0 = ui.home;
        uint32_t s0;
        ui.home = 1;
        load_put(1u);
        ui.force = 1; draw_graph(); ui.force = 0; s0 = ui.graph_sig;
        ui.frame += 2u; fm1_ms += 20u; draw_graph();
        ok = ui.graph_sig == s0 && ANIM_SNAP();         /* (20 ms later: not yet) */
        ui.frame += 2u; fm1_ms += 40u; draw_graph();
        ok &= ui.graph_sig != s0;                       /* (60 ms: drawn) */
        load_put(0u);
        s0 = ui.graph_sig; ui.frame += 2u; fm1_ms += 60u; draw_graph();
        ok &= ui.graph_sig == s0;                       /* ECO: 60 ms not yet */
        ui.frame += 2u; fm1_ms += 20u; draw_graph();
        ok &= ui.graph_sig != s0;
        load_put(2u);
        s0 = ui.graph_sig; ui.frame += 2u; fm1_ms += 16u; draw_graph();
        ok &= ui.graph_sig != s0 && !ANIM_SNAP();       /* FULL: every change */
        ui.home = home0; ui.force = 1; frame();
        bad += check("LOAD LITE / ECO: the panel animates at most every 50 / 70 ms, values snap; FULL as before", ok);
    }
    {   /* (0.6.29) 6 LITE: the heavy parts swapped while they sound, the project's values kept */
        int16_t rt0 = song.g[G_RTYPE], dp0 = song.g[G_DPIT];
        int32_t o[2u * CTL];
        track_t ft = trk[0];
        load_put(1u);
        ok = sys_load == LOAD_LITE;
        song.g[G_RTYPE] = 2; mix_block(o, CTL); mix_block(o, CTL);
        ok &= fx.rtype == 0u && song.g[G_RTYPE] == 2;   /* SHIMMER sounds as ROOM */
        song.g[G_RTYPE] = 3; mix_block(o, CTL); mix_block(o, CTL);
        ok &= fx.rtype == 1u;                           /* RESO as SPRING */
        song.g[G_RTYPE] = 4; mix_block(o, CTL); mix_block(o, CTL);
        ok &= fx.rtype == 0u;                           /* CLOUD as ROOM */
        ft.engine = ENGI_FM6;
        ok &= trk_nvoice(&ft) == LITE_FM6_POLY;         /* FM6: two voices */
        song.g[G_RTYPE] = 2; go_title("REVERB"); frame();
        ok &= strstr(ui.col[0], "|SHIMR|LITE") != 0;   /* the card says it is swapped */
        load_put(2u);
        ok &= !sys_load && trk_nvoice(&ft) == FM6_POLY;
        song.g[G_RTYPE] = 2; mix_block(o, CTL); mix_block(o, CTL);
        ok &= fx.rtype == 2u;                           /* FULL: SHIMMER again */
        press(B_HOME); frame();
        bad += check("LOAD LITE: SHIMMER / CLOUD sound as ROOM, RESO as SPRING, FM6 two voices, TYPE says LITE; FULL as saved", ok);
        {   /* (0.6.30) LOAD ECO: every reverb a SPRING, no SLICER (its tracks to the mix, its pages gone), no chorus
             * page, ECO_POLY voices a track (DRUM as ever) */
            int16_t sm0 = song.g[G_SLMODE];
            uint32_t i, hidden = 1, shown = 0;
            ft.engine = ENGI_ANALOG;
            song.g[G_SLMODE] = 1;
            ok = slicer_on();
            load_put(0u);
            song.g[G_RTYPE] = 0; mix_block(o, CTL); mix_block(o, CTL);
            ok &= sys_load == LOAD_ECO && fx.rtype == 1u && !slicer_on() && song.g[G_SLMODE] == 1;
            ok &= trk_nvoice(&ft) == ECO_POLY;
            ft.engine = ENGI_FM6;
            ok &= trk_nvoice(&ft) == LITE_FM6_POLY;
            ft.engine = ENGI_DRUM;
            ok &= trk_nvoice(&ft) == (ENGINES[ENGI_DRUM]->poly ? ENGINES[ENGI_DRUM]->poly : NVOICE);
            for (i = 0; i < NPAGES; i++) {
                int sl = PAGES[i].graph == GR_SLCR || PAGES[i].graph == GR_SLPIT || str_eq(PAGES[i].title, "CHORUS");
                if (sl) hidden &= !page_visible(i);
                shown += str_eq(PAGES[i].title, "REVERB") && page_visible(i);
            }
            ok &= hidden && shown == 1u;
            ok &= voice_cap() == ECO_VOICES && fx.sp_ns == SP_N_ECO;   /* (0.6.70) 6 voices shared, a shorter SPRING */
            go_title("REVERB"); frame();
            ok &= strstr(ui.col[0], "|ROOM|ECO") != 0;
            load_put(2u);
            song.g[G_RTYPE] = 1; mix_block(o, CTL);
            ok &= voice_cap() == NVOICE && fx.sp_ns == SP_N;   /* FULL: all of them */
            for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "SLICER"); i++) {}
            ok &= slicer_on() && page_visible(i);
            song.g[G_SLMODE] = sm0;
            press(B_HOME); frame();
            bad += check("LOAD ECO: reverb SPRING (4 stages), no SLICER / chorus (pages hidden), 4 voices a synth, 6 shared, DRUM as ever; FULL back", ok);
        }
        song.g[G_RTYPE] = rt0; song.g[G_DPIT] = dp0; mix_block(o, CTL); mix_block(o, CTL);
    }
    {   /* (0.6.71) the second core: some synth parts on it (here run at once, as core 1: mc_host), each core with its
         * own buffers; the sound bit for bit the one core's. A diced song, six tracks, FM6 and ANALOG among them, the
         * matrix and a filter on, played for ~3 s in two children from the same state, one on one core, one on two */
        uint64_t h[2] = {0, 0}, used = 0;
        uint32_t k;
        int fds[2][2];
        uint8_t ld0 = ui_load;
        load_put(2u);                                   /* FULL */
        rng_state = 0x1234567u;                         /* (0.6.85) the same diced song whatever ran before (a drum in it) */
        dice_all();
        set_engine_of(&trk[1], ENGI_FM6);
        set_engine_of(&trk[2], 0);
        trk[2].p[P_FCUT] = 70; trk[2].p[P_FRES] = 60;  /* (voice.c tf_on) */
        trk[1].p[P_CHOR] = 60; trk[2].p[P_DLY] = 50; trk[0].p[P_REV] = 40;
        for (k = 0; k < NPART; k++)                     /* (0.6.73) the drums' IMAGE and their sounds' SENDs on */
            if (ENGINES[trk[k].eng_req] == &ENG_DRUM)
                trk[k].p[P_DXIMG] = 60;
        dx_snd[DV_SNARE] = (3u << 6) | 40u; dx_snd[DV_HATC] = (2u << 6) | 40u;
        if (!song.playing) transport_req = 1;
        for (k = 0; k < 2u; k++) {
            pid_t pid;
            if (pipe(fds[k])) break;
            pid = fork();
            if (pid == 0) {
                int32_t o[2u * CTL];
                uint64_t hh = 1469598103934665603ull, u = 0;
                uint32_t b, j;
                mc_host = (uint8_t)k;
                song.master_q12 = 4096;                 /* (heard: the test's power-on leaves MASTER at 0) */
                for (b = 0; b < 4000u; b++) {
                    mix_block(o, CTL);
                    for (j = 0; j < 2u * CTL; j++)
                        hh = (hh ^ (uint32_t)o[j]) * 1099511628211ull;
                    for (j = 0; j < 2u * CTL; j++)
                        u |= (uint64_t)(o[j] != 0) << 8;     /* (not silence) */
                    u |= mc_mask_last;
                    {   /* (0.6.73) the DRUM parts together, on one core or the other */
                        uint32_t dm = 0, q;
                        for (q = 0; q < NPART; q++)
                            dm |= (uint32_t)(ENGINES[trk[q].engine] == &ENG_DRUM && mc_cost(&trk[q])) << q;
                        if ((mc_mask_last & dm) && (mc_mask_last & dm) != dm) u |= 1ull << 16;   /* (split: wrong) */
                        if (mc_mask_last & dm) u |= 1ull << 17;                                  /* (on core 1) */
                    }
                }
                if (write(fds[k][1], &hh, 8) != 8 || write(fds[k][1], &u, 8) != 8) {}
                _exit(0);
            }
            close(fds[k][1]);
            if (read(fds[k][0], &h[k], 8) != 8 || read(fds[k][0], k ? &used : &h[k], k ? 8 : 0) < 0) {}
            waitpid(pid, 0, 0);
            close(fds[k][0]);
        }
        ok = h[0] && h[0] == h[1] && (used & 0xFFu) && ((used >> 8) & 1u) &&   /* parts on core 1, and sound */
             !((used >> 16) & 1u) && ((used >> 17) & 1u);   /* (0.6.73) the drums together, on core 1 too */
        if (!ok) for (k = 0; k < NPART; k++) printf("  T%u engine %u mute %d len %d\n", k + 1u, trk[k].eng_req, trk[k].p[P_MUTE], trk[k].p[P_SLEN]);
        if (!ok) printf("  dual core: one %016llx two %016llx, parts on core 1 %llx\n", (unsigned long long)h[0],
                        (unsigned long long)h[1], (unsigned long long)used);
        load_put(ld0 <= 2u ? 2u - ld0 : 2u);
        bad += check("DUAL CORE: parts on the second core (the DRUM ones together, there too), the sound bit for bit one core's", ok);
    }
    {   /* (0.6.72) MIDI OUT (USB): ALL sends the sequencer's notes, each with its note-off (all of them by STOP), the
         * clock 24 a beat with START / STOP; KEYS none of that; CLOCK the clock only. PROGRAM CHANGE n on a track's
         * channel loads the PRESETS list's n + 1-th sound into that track */
        int32_t o[2u * CTL];
        uint32_t lv, b, on[4], off[4], f8[4], fa[4], fc[4], bal[4], k, e, n0 = 0;
        uint8_t held[NTRK][128];
        usb.config = 1;
        load_put(2u);
        for (lv = 0; lv < 4u; lv++) {
            menu_put(MI_MIDIOUT, lv); frame();
            ok = mo_lvl == lv;
            rng_state = 0x1234567u + lv;                /* (0.6.85) the same diced songs whatever ran before */
            dice_all();
            if (lv == 2u)
                n0 = 2800u * CTL / (beat_samples() / 24u);   /* clocks in the 2800 blocks (also stopped) */
            if (song.playing) { transport_req = 2; mix_block(o, CTL); }
            mo_r = mo_w;
            memset(held, 0, sizeof held);
            on[lv] = off[lv] = f8[lv] = fa[lv] = fc[lv] = 0; bal[lv] = 1;
            transport_req = 1;
            for (b = 0; b < 2800u; b++) {               /* ~2 s, then STOP */
                if (b == 2700u) transport_req = 2;
                mix_block(o, CTL);
                while (mo_r != mo_w) {                  /* (as usb.c drains it) */
                    uint32_t p = midi_out_q[mo_r++ % MQ], st = (p >> 8) & 0xFFu, ch = st & 15u, nt = (p >> 16) & 127u;
                    if ((st & 0xF0u) == 0x90u) { on[lv]++; bal[lv] &= ch < NTRK && !held[ch][nt]; if (ch < NTRK) held[ch][nt] = 1; }
                    else if ((st & 0xF0u) == 0x80u) { off[lv]++; if (ch < NTRK) held[ch][nt] = 0; }
                    else if (st == 0xF8u) f8[lv]++;
                    else if (st == 0xFAu) fa[lv]++;
                    else if (st == 0xFCu) fc[lv]++;
                }
            }
            for (k = 0; k < NTRK; k++)
                for (b = 0; b < 128u; b++)
                    bal[lv] &= !held[k][b];             /* nothing left on */
        }
        ok = on[2] > 8u && bal[2] && fa[2] == 1u && fc[2] == 1u && f8[2] + 8u >= n0 && f8[2] <= n0 + 60u;   /* ALL */
        ok &= !on[0] && !f8[0] && !fa[0] && !fc[0];     /* KEYS: no sequencer notes, no clock */
        ok &= !on[1] && f8[1] > 8u && fa[1] == 1u && fc[1] == 1u;   /* CLOCK */
        ok &= !on[3] && !f8[3] && !fa[3];               /* OFF */
        if (!ok) printf("  midi out: on %u %u %u %u f8 %u %u %u %u (%u) fa %u %u fc %u %u bal %u\n", on[0], on[1], on[2], on[3],
                        f8[0], f8[1], f8[2], f8[3], n0, fa[1], fa[2], fc[1], fc[2], bal[2]);
        menu_put(MI_MIDIOUT, 0); frame();
        bad += check("MIDI OUT: ALL the sequencer's notes (each ended), clock + START / STOP; CLOCK the clock; KEYS / OFF none", ok);
        {
            uint32_t all, sel0 = song.sel;
            song.sel = 1;
            preset_all_pos(&all);
            e = preset_all_at(9u, &k);
            song.sel = (uint8_t)sel0;
            midi_enqueue(0x0Cu | 0xC1u << 8 | 9u << 16, 1u);   /* PROGRAM CHANGE 9 on channel 2 */
            mix_block(o, CTL); frame();
            ok = all > 9u && e < NENGINES && trk[1].eng_req == e && song.sel == sel0;
            {                                           /* 127: the list's 128th (QG-1: more than 128 sounds), past it nothing */
                uint32_t k2, e2 = all > 127u ? preset_all_at(127u, &k2) : e;
                midi_enqueue(0x0Cu | 0xC1u << 8 | 127u << 16, 1u);
                mix_block(o, CTL); frame();
                ok &= trk[1].eng_req == e2;
            }
            bad += check("PROGRAM CHANGE n: the PRESETS list's n + 1-th sound into the channel's track (127: past a short list, nothing), the selection kept", ok);
        }
    }
    {
        persist_t p;
        settings_export(&p);
        ui_rec_prefs = 0; frame();
        ok = settings_import(&p, (int)sizeof p) == 1 && (frame(), click_mode == CLICK_ON && click_lvl == 2u && cin_bars == 2u);
        p.favorites.factory[15][28] = 3u | 3u << 2 | 1u << 4;   /* CLICK and LEVEL no value, COUNT-IN 1 BAR */
        ok &= settings_import(&p, (int)sizeof p) == 1 && p.favorites.factory[15][28] == 1u << 4;
        frame();
        ok &= click_mode == CLICK_OFF && click_lvl == 1u && cin_bars == 1u;
        memset(&p.favorites, 0, sizeof p.favorites);   /* settings from before the favorites (PER3): the defaults */
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !ui_rec_prefs;
        frame();
        ok &= click_mode == CLICK_OFF && click_lvl == 1u && !cin_bars;
    }
    bad += check("CLICK / CLICK LEVEL / COUNT-IN saved and read back; a stored 3 the default; older settings OFF / MID / OFF", ok);

    /* the count-in from the UI */
    ui_power_on();
    menu_put(MI_COUNTIN, 1u);
    go_page(GR_ROLL);
    frame();
    press(B_REC);                                       /* (#133: on STEP REC arms step recording, stopped) */
    ok = (song.rec & 1u) && transport_req == 0u && !live_rec_sel();
    press(B_PLAY);
    ok &= transport_req == 1u;
    events_block(CTL);
    ok &= seq_counting() && !song.playing && cin_left == 4u && live_rec_sel();
    frame();
    {
        uint16_t now0 = px_at(16, CI_Y + 4), next0 = px_at(16 + 56, CI_Y + 4);
        ok &= now0 != next0;                            /* the first beat's cell lit, the next not */
        ui_leds();
        ok &= (fm1_led[8] & 2u) != 0u;                  /* PLAY's green: on the beat */
        for (i = 0; i < 400u; i++) events_block(CTL);    /* (~0.3 s: past the first quarter of the beat) */
        ui_leds();
        ok &= !(fm1_led[8] & 2u);
        cin_beat();
        frame();
        ok &= cin_left == 3u && px_at(16 + 56, CI_Y + 4) == now0 && px_at(16, CI_Y + 4) == next0;   /* the cell moved */
        ui_leds();
        ok &= (fm1_led[8] & 2u) != 0u;
    }
    bad += check("count-in from the UI: REC on STEP arms, PLAY counts in (STEP entry paused); 4 3 2 1 on screen; PLAY's green on the beats", ok);
    press(B_PLAY);
    ok = transport_req == 2u;
    events_block(CTL);
    frame();
    ok &= !seq_counting() && !song.playing && (song.rec & 1u) && px_at(16, CI_Y + 4) == px_at(16 + 56, CI_Y + 4);
    bad += check("PLAY while counting in stops it: nothing plays, still armed, the page back", ok);
    press(B_PLAY);
    events_block(CTL);
    ok = seq_counting() && cin_left == 4u;
    for (i = 0; i < 4u; i++) cin_beat();
    frame();
    ok &= !seq_counting() && song.playing && trk[0].seq_idx == 0u && px_at(16, CI_Y + 4) == px_at(16 + 56, CI_Y + 4);
    bad += check("PLAY armed and stopped: the count-in again; at its end the sequencer plays from step 1, the page back", ok);
    press(B_PLAY);
    events_block(CTL);
    menu_put(MI_COUNTIN, 0u);
    stop_transport();
    return bad;
}

/* #67 MENU > USB SERIAL: ON by default (and in every older setting): the console presented, the descriptors as
 * before. OFF: audio + MIDI only, device class 0, bcdUSB 1.10 (CFG_DESC_PLAIN: a FELUCCA_CDC=0 build's bytes), applied
 * when the menu closes (one re-enumeration); saved; at boot the saved choice before USB starts (never on then off) */
static int dev_class(void)                              /* the device class GET_DESCRIPTOR sends now (-1: none) */
{
    const uint8_t *d;
    uint16_t n;
    return get_desc(0x0100, &d, &n) && n == 18u ? d[4] : -1;
}
static int dev_is(const uint8_t *want)                  /* the device descriptor sent now = want */
{
    const uint8_t *d;
    uint16_t n;
    return get_desc(0x0100, &d, &n) && n == 18u && !memcmp(d, want, 18);
}
static int plain_desc(void)                             /* device + configuration = the console-less bytes */
{
    const uint8_t *d, *c;
    uint16_t n, m;
    return get_desc(0x0100, &d, &n) && get_desc(0x0200, &c, &m) && n == 18u && !memcmp(d, DEV_DESC_PLAIN, 18) &&
           m == sizeof CFG_DESC_PLAIN && !memcmp(c, CFG_DESC_PLAIN, m);
}
static int test_usb_serial(void)
{
    int bad = 0, ok;
    ui_power_on();
    usb.up = 0;                                          /* (no SIE here: usb_cdc_switch only flips the descriptors) */
    frame();
    ok = usb_cdc_on && dev_class() == 0xEF && !(ui_prefs & PREF_SERIAL_OFF) && str_eq(MI_NAME[MI_SERIAL], "USB SERIAL") &&
         str_eq(menu_flag(MI_SERIAL)->name[0], "ON") && str_eq(menu_flag(MI_SERIAL)->name[1], "OFF");
    ok &= dev_is(DEV_DESC);
    bad += check("#67 MENU > USB SERIAL: ON by default, the console presented (misc / IAD, as before)", ok);
    hold(B_HOME);
    ui.menu_sel = MI_SERIAL;
    ui.force = 1; ui_draw();
    turn(EN_K1, -1);
    ok = (ui_prefs & PREF_SERIAL_OFF) && usb_cdc_on;    /* left: OFF; applied when the menu closes */
    turn(EN_K1, 1); ok &= !(ui_prefs & PREF_SERIAL_OFF);   /* right: ON (the switch's knob right) */
    press(B_OCTDN); ok &= (ui_prefs & PREF_SERIAL_OFF) && ui.menu && usb_cdc_on;   /* OCT-: OFF */
    press(B_HOME);
    ok &= ui.menu == 0 && usb_cdc_on;                 /* (HOME acts as it is let go: closed this frame) */
    frame();
    ok &= !ui.menu && !usb_cdc_on && !usb.config && plain_desc() && dev_class() == 0;
    bad += check("  KNOB 1 left / OCT- OFF, right ON; the menu closed (HOME): audio + MIDI only, class 0", ok);
    {
        persist_t p;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_SERIAL_OFF;
        ok &= settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_SERIAL_OFF;     /* (again: the same) */
        /* boot (main.c): the settings, usb_serial_apply, then usb_start: the console-less descriptors from the start */
        usb_cdc_on = 1;
        usb.up = 0;
        usb_serial_apply();
        ok &= !usb_cdc_on && !usb.up && plain_desc();
        memset(&p.favorites, 0, sizeof p.favorites);   /* settings from before it (PER3): ON */
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & PREF_SERIAL_OFF);
        usb_serial_apply();
        ok &= usb_cdc_on && dev_is(DEV_DESC);
    }
    bad += check("  saved with the settings (older ones: ON); at boot the device enumerates with the saved choice", ok);
    {   /* OFF: the soft key and the update frames still come in (USB-MIDI SysEx, EP1) */
        static const uint8_t KEY[8] = {0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7D, 0xF7};
        ui_prefs = PREF_SERIAL_OFF;
        frame();
        usb.uboot_req = 0;
        ok = !usb_cdc_on && ep1_take(KEY, sizeof KEY) && usb.uboot_req;
        usb.uboot_req = 0;
        bad += check("  OFF: the soft key (SysEx on the MIDI endpoint) still asks for UPDATE MODE", ok);
    }
    ui_prefs = 0;
    frame();
    return bad;
}

/* #58 MENU > BPM LOCK: OFF by default (and in every older setting): SELECT sets the tempo on every page. ON: SELECT
 * leaves it on the pages ("BPM LOCKED" once per turn burst); GLO > GLOBAL's BPM knob, GLO + SELECT (a combo: the map
 * opens, no GLO page; OCT- puts it back) and GLO's F4 TAP still set it; the menu never takes SELECT as the tempo */
static int test_bpm_lock(void)
{
    int bad = 0, ok;
    int16_t b0;
    uint32_t i, page;
    ui_power_on();
    ui_prefs = 0;
    frame();
    go_title("GLOBAL"); frame();                       /* (0.6.67: SELECT alone scrolls the pages, never the tempo) */
    b0 = song.g[G_BPM];
    turn(EN_SELECT, 1);
    ok = song.g[G_BPM] == b0 && str_eq(cur_page()->title, "SYSTEM") && str_eq(menu_flag(MI_BPMLOCK)->name[0], "OFF");
    turn(EN_SELECT, -1);
    ok &= str_eq(cur_page()->title, "GLOBAL");
    go_home(); frame(); turn(EN_SELECT, 1);
    ok &= ui.home && ui.confirm == CF_DICE_ALL && song.g[G_BPM] == b0;   /* (0.6.69) HOME: DICE EVERYTHING? as before */
    press(B_OCTDN); frame(); ok &= !ui.confirm;
    go_title("EDIT 1"); frame(); turn(EN_SELECT, 1);
    ok &= str_eq(cur_page()->title, "EDIT 2");
    turn(EN_SELECT, -5);
    ok &= str_eq(cur_page()->title, "EDIT 1");                        /* (no wrap: the section's first) */
    go_title("ENV DEST"); frame(); turn(EN_SELECT, 3);
    ok &= str_eq(cur_page()->title, "ENV DEST") && !ui.home;          /* (the section's last: it stays, no LFO) */
    bad += check("0.6.69: SELECT alone scrolls the section's pages (no wrap, never out of it); HOME: DICE ALL; never the tempo", ok);
    go_home(); frame();

    hold(B_HOME);
    ui.menu_sel = MI_BPMLOCK;
    ui.force = 1; ui_draw();
    b0 = song.g[G_BPM];
    turn(EN_SELECT, 3);
    ok = str_eq(MI_NAME[MI_BPMLOCK], "BPM LOCK") && ui.menu == 1u && song.g[G_BPM] == b0;   /* (the menu: no tempo) */
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_BPM_LOCK) != 0u;          /* right: ON */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_BPM_LOCK);              /* left: OFF */
    press(B_OCTUP); ok &= (ui_prefs & PREF_BPM_LOCK) && ui.menu;      /* OCT+: ON */
    press(B_OCTDN); ok &= !(ui_prefs & PREF_BPM_LOCK) && ui.menu;     /* OCT-: OFF */
    press(B_OCTUP);
    ok &= (ui_prefs & ~PREF_ACCEL_OFF) == PREF_BPM_LOCK;                                  /* (no other setting moved) */
    press(B_HOME);
    bad += check("#58 MENU > BPM LOCK: KNOB 1 right / OCT+ ON, left / OCT- OFF; SELECT in the menu is no tempo", ok && !ui.menu);

    /* ON: no tempo from SELECT alone either (0.6.67: it scrolls) */
    ok = 1;
    for (i = 0; i < 2u; i++) {
        go_title(i ? "SYSTEM" : "GLOBAL");
        frame();
        b0 = song.g[G_BPM];
        turn(EN_SELECT, 1);
        ok &= song.g[G_BPM] == b0 && !ui.bpm_t;
    }
    bad += check("#58 BPM LOCK ON: SELECT alone leaves the tempo on GLOBAL and SYSTEM", ok);
    /* (JIANT 0.5) SELECT off HOME and GLOBAL: the track's sound diced (a synth: its engine's parameters move; DRUM-X:
     * the kit), never the tempo; once a turn burst */
    {
        int16_t before[8];
        uint32_t j, moved = 0;
        go_title("ENV"); frame();
        for (j = 0; j < 8u; j++) before[j] = TSEL->p[P_E0 + j];
        b0 = song.g[G_BPM]; fm1_ms += 1200u;
        turn(EN_SELECT, 1);                             /* (0.6.67) SELECT alone: a page on, no dice */
        for (j = 0; j < 8u; j++) moved += TSEL->p[P_E0 + j] != before[j];
        ok = !moved && song.g[G_BPM] == b0 && str_eq(cur_page()->title, "ENV DEST") && !ui.confirm;
        fm1_ms += 1200u;
        held_turn(B_EDIT, 1);                           /* EDIT held + SELECT: the sound's dice */
        ok &= str_eq(cur_page()->title, "ENV DEST");    /* (no page: EDIT's tap swallowed) */
        for (j = 0; j < 8u; j++) moved += TSEL->p[P_E0 + j] != before[j];
        ok &= song.g[G_BPM] == b0 && ui.confirm == CF_DICE_SOUND && moved > 0u && undo_ok;   /* (0.6.65) heard, asked */
        {
            int16_t orig[8], d1[8];
            uint32_t same = 1;
            for (j = 0; j < 8u; j++) orig[j] = before[j];
            for (j = 0; j < 8u; j++) d1[j] = TSEL->p[P_E0 + j];
            turn(EN_SELECT, 1);                                       /* (the same burst: no second dice) */
            for (j = 0; j < 8u; j++) ok &= TSEL->p[P_E0 + j] == d1[j];
            fm1_ms += 1200u;
            for (uint32_t n = 0; n < 4u && same; n++) {               /* SELECT again (a new burst): another dice */
                turn(EN_SELECT, 1); fm1_ms += 1200u;
                for (j = 0; j < 8u; j++) same &= TSEL->p[P_E0 + j] == d1[j];
            }
            ok &= !same && ui.confirm == CF_DICE_SOUND;
            press(B_OCTDN); frame();                                  /* CANCEL: the sound before the first dice */
            for (j = 0; j < 8u; j++) ok &= TSEL->p[P_E0 + j] == orig[j];
            ok &= !ui.confirm && msg_is("SOUND BACK");
            bad += check("0.6.65: EDIT (0.6.67: held) + SELECT dices at once under KEEP DICED SOUND?; SELECT again dices again; CANCEL "
                         "brings the sound back", ok);
            fm1_ms += 1200u;
            held_turn(B_EDIT, 1);
            for (j = 0; j < 8u; j++) d1[j] = TSEL->p[P_E0 + j];
            press(B_OCTUP); frame();                                  /* OK: the dice stays */
            ok = !ui.confirm && msg_is("SOUND DICED");
            for (j = 0; j < 8u; j++) ok &= TSEL->p[P_E0 + j] == d1[j];
        }
        bad += check("0.6.21: EDIT + SELECT dices the sound (not the tempo), once a turn burst (0.6.65: OK keeps it)", ok);
    }
    {   /* (0.6.67) the MIXER: SELECT the next of GLO's pages, the tempo kept */
        go_title("MIXER"); frame(); fm1_ms += 1200u;
        b0 = song.g[G_BPM];
        turn(EN_SELECT, -1);
        ok = song.g[G_BPM] == b0 && cur_page()->fam == FAM_GLO && !ui.confirm;
        bad += check("0.6.69: SELECT on the MIXER goes through GLO's pages", ok);
    }

    /* ON: GLO > GLOBAL's BPM knob */
    go_title("GLOBAL"); frame();
    b0 = song.g[G_BPM];
    turn(EN_K1, 1);
    ok = str_eq(cur_page()->title, "GLOBAL") && cur_page()->id[0] == G_BPM && song.g[G_BPM] == b0 + 1;
    bad += check("#58 BPM LOCK ON: GLO > GLOBAL KNOB 1 still sets the tempo", ok);

    /* ON: GLO + SELECT (a quick turn: a combo, no GLO page), OCT- puts it back, F4 TAP */
    go_title("ENV"); frame();
    page = ui.page;
    b0 = song.g[G_BPM];
    ui.msg_t = 0;
    btn_down(B_GLO); frame();
    turn(EN_SELECT, 3);
    ok = song.g[G_BPM] == b0 + 3 && ui.bpm_t && !msg_is("BPM LOCKED") && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    ok &= ui.page == page && !ui.home;                                /* (no tap: still ENV) */
    btn_down(B_GLO); frame();
    turn(EN_SELECT, -5);
    ok &= song.g[G_BPM] == b0 - 2;
    oct_back();
    ok &= song.g[G_BPM] == b0 - 2;                                    /* (OCT-: nothing put back) */
    btn_up(B_GLO); frame();
    bad += check("#58 BPM LOCK ON: GLO + SELECT sets the tempo (a combo: the map, no GLO page; OCT- nothing)",
                 ok && ui.page == page);
    song.g[G_BPM] = 100;
    btn_down(B_GLO); frame();
    for (i = 0; i < 3u; i++) { key_down(white(7)); frame(); key_up(white(7)); frames(480); }
    ok = song.g[G_BPM] == 120;
    btn_up(B_GLO); frame();
    bad += check("#58 BPM LOCK ON: GLO + F4 TAP still sets the tempo", ok);

    {   /* saved with the settings; settings from before it: OFF (and so again: idempotent) */
        persist_t p, q;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_BPM_LOCK;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_BPM_LOCK && !memcmp(&p, &q, sizeof p);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                                        /* (PER3: before the favorites record) */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & ~PREF_ACCEL_OFF);
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && !(ui_prefs & ~PREF_ACCEL_OFF);
        frame();
        b0 = song.g[G_BPM];
        go_title("GLOBAL"); frame();
        tempo_turn(1);
        ok &= song.g[G_BPM] == b0 + 1;
        go_home(); frame();
        bad += check("#58 BPM LOCK saved with the settings; settings from before it: OFF (idempotent)", ok);
    }
    ui_prefs = 0;
    ui_power_on(); frame();
    return bad;
}

/* #15 / Discussion #80 MENU > LARGE: OFF by default (and in every older setting); KNOB 1 right ON / left OFF, OCT+
 * toggles, nothing else moves. ON: HOME and the value pages draw tall cards (a K1..K4 pill over each knob's column,
 * the value in L) and the strip under them; the list / graph pages and the layers keep the layout (their labels in
 * M); the footer stays; a knob turned redraws its own card only; saved with the settings, older settings OFF
 * (idempotent) */
static uint16_t lg_px(int32_t x, int32_t y) { return swap16(host_screen[y * 240 + x]); }
static int lg_value_rows(uint32_t c)                    /* rows of card c's value band holding the value's colour */
{
    int32_t x, y, n = 0;
    for (y = Y_LABEL + 40; y < Y_LABEL + 76; y++)
        for (x = CARD_X(c); x < CARD_X(c) + CARD_W; x++)
            if (lg_px(x, y) == T_THEME) { n++; break; }
    return n;
}
static int test_large(void)
{
    int bad = 0, ok;
    uint32_t c;
    static uint16_t off[240 * 240];
    ui_power_on();
    palette_set(UI_GREY_INDEX);                                   /* (the flat panel: the chamfered ones draw their own gaps) */
    song.playing = 0;
    frame();
    ok = !(ui_prefs & PREF_LARGE) && large_kind() == LK_OFF && card_h() == CARD_H && graph_y() == Y_GRAPH;
    ok &= MI_LARGE == MI_STYLE + 1u && str_eq(MI_NAME[MI_LARGE], "LARGE") && menu_flag(MI_LARGE) &&
          str_eq(menu_flag(MI_LARGE)->name[0], "OFF") && str_eq(menu_flag(MI_LARGE)->name[1], "ON");
    ui.force = 1; frame();
    memcpy(off, host_screen, sizeof off);
    hold(B_HOME); ui.menu_sel = MI_LARGE;
    ok &= ui.menu == 1u;
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_LARGE) != 0u;          /* right: ON */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_LARGE);              /* left: OFF */
    press(B_OCTUP); ok &= (ui_prefs & PREF_LARGE) && ui.menu;     /* OCT+: ON */
    press(B_OCTDN); ok &= !(ui_prefs & PREF_LARGE) && ui.menu;    /* OCT-: OFF */
    press(B_OCTUP);
    ok &= (ui_prefs & ~PREF_ACCEL_OFF) == PREF_LARGE && ui_style == ST_FLAT;          /* (no other setting moved) */
    press(B_HOME);
    bad += check("#15 MENU > LARGE: OFF by default, under STYLE; KNOB 1 right / OCT+ ON, left / OCT- OFF", ok && !ui.menu);

    /* HOME: tall cards (rows 28..132) and the strip (136..198); the footer as it was */
    frame();
    ok = ui.home && large_kind() == LK_TALL && card_h() == LG_CARD_H && graph_y() == LG_Y_GRAPH && strip_kind() == SK_SCOPE;
    ok &= lg_px(30, Y_SEP_END + 1) == T_SURF && off[(Y_SEP_END + 1) * 240 + 30] == swap16(T_BG);     /* a card where the gap was */
    ok &= lg_px(30, LG_Y_GRAPH - 2) == T_BG && off[(LG_Y_GRAPH - 2) * 240 + 30] != swap16(T_BG);     /* a gap in the panel */
    ok &= lg_px(30, LG_Y_GRAPH + 1) == T_SURF;
    for (c = 0; c < 4u; c++)                                      /* each column's K pill, over its knob */
        ok &= lg_px(CARD_X(c) + 8, Y_LABEL + LG_MY + LG_MH / 2) == T_KEY;
    ok &= !memcmp(off + Y_FOOT * 240, host_screen + Y_FOOT * 240, 240u * H_FOOT * sizeof *off);
    bad += check("#15 LARGE ON: HOME's cards tall (a K1..K4 pill over each column), the scope a strip, the footer kept", ok);

    /* the value in L: GLOBAL's BPM 124 stands about twice as tall as in M */
    go_title("GLOBAL"); frame();
    ok = large_kind() == LK_TALL && strip_kind() == SK_TITLE && cur_page()->id[0] == G_BPM && lg_value_rows(0) >= AF_L_CAP_H - 3;
    ui_prefs = 0; ui.force = 1; frame();
    ok &= lg_value_rows(0) <= AF_M_CAP_H + 2;
    ui_prefs = PREF_LARGE; ui.force = 1; frame();
    bad += check("#15 LARGE: a card value in L (GLOBAL's BPM 120: about 21 rows of figures, M's 11)", ok);

    /* a knob turned: its own card redrawn, the other three not */
    go_title("ENV"); frame(); frames(600);
    memcpy(off, host_screen, sizeof off);
    turn(EN_K2, 3);
    {
        int32_t x, y, diff[4] = {0, 0, 0, 0};
        for (y = Y_LABEL; y < Y_LABEL + LG_CARD_H; y++)
            for (c = 0; c < 4u; c++)
                for (x = CARD_X(c); x < CARD_X(c) + CARD_W; x++)
                    diff[c] += off[y * 240 + x] != host_screen[y * 240 + x];
        ok = large_kind() == LK_TALL && strip_kind() == SK_ADSR && diff[1] && !diff[0] && !diff[2] && !diff[3];
    }
    bad += check("#15 LARGE: ENV tall with its ADSR in the strip; KNOB 2 turned redraws card 2 only", ok);

    /* per page type: lists and rolls keep the layout, the layers too; MIXER's tracks in the strip */
    go_page(GR_BROWSE); frame();
    ok = large_kind() == LK_LABEL && card_h() == CARD_H && graph_y() == Y_GRAPH && graph_h() == H_GRAPH;
    go_page(GR_ROLL); frame();
    ok &= large_kind() == LK_LABEL;
    go_page(GR_TRK); frame();
    ok &= large_kind() == LK_TALL && strip_kind() == SK_TRK && lg_px(CARD_X(0) + 8, LG_Y_GRAPH + 56) == T_SURF;
    go_title("ENV"); frame();
    btn_down(B_GLO); frames(800);
    ok &= ui.layer && large_kind() == LK_LABEL && card_h() == CARD_H;
    btn_up(B_GLO); frame(); frame();
    ok &= !ui.layer && large_kind() == LK_TALL && lg_px(30, LG_Y_GRAPH - 2) == T_BG;
    bad += check("#15 LARGE: PRESETS and the roll keep the layout (labels in M), the layers too; MIXER tall, its tracks in the strip", ok);

    {   /* saved with the settings; settings from before it: OFF (and so again: idempotent) */
        persist_t p, q;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_LARGE;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && (ui_prefs & ~PREF_ACCEL_OFF) == PREF_LARGE && !memcmp(&p, &q, sizeof p);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                                    /* (PER3: before the favorites record) */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & ~PREF_ACCEL_OFF);
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && !(ui_prefs & ~PREF_ACCEL_OFF);
        ui.force = 1; frame();
        ok &= large_kind() == LK_OFF && lg_px(30, Y_SEP_END + 1) == T_BG;
        bad += check("#15 LARGE saved with the settings; settings from before it: OFF (idempotent)", ok);
    }
    ui_prefs = 0;
    ui_power_on(); frame();
    return bad;
}

/* MENU (1.0.5: in tabs): every row reachable with ALGORITHM and PRESETS, each tab its own page; #46 MENU > ANIM: ON by
 * default (and in every older setting), OFF snaps the rolling digits, the piano roll and the menu's tabs */
static int test_menu_prefs(void)
{
    int bad = 0, ok = 1;
    uint32_t k, j, seen = 0;
    static uint16_t shot[MTAB_COUNT][240 * 240];
    ui_power_on();
    ui_prefs = 0;
    hold(B_HOME);
    ui.menu_sel = 0;
    for (k = 0; k < MTAB_COUNT; k++) {              /* ALGORITHM right through the tabs, PRESETS through each one's rows */
        ok &= menu_tab() == k && ui.menu_sel == mtab_first(k);
        for (j = 0; j < mtab_rows(k); j++) {
            seen |= 1u << ui.menu_sel;
            turn(EN_PRESET, 1);
        }
        ok &= ui.menu_sel == mtab_first(k);         /* (round the tab, back at its first row) */
        ui.force = 1; ui_draw();
        memcpy(shot[k], host_screen, sizeof shot[k]);
        for (j = 0; j < k; j++) ok &= memcmp(shot[j], shot[k], sizeof shot[k]) != 0;
        turn(EN_ALGO, 1);
    }
    ok &= seen == (1u << MI_COUNT) - 1u && ui.menu_sel == 0u;
    bad += check("MENU: ALGORITHM and PRESETS reach every row, each tab a page of its own", ok);
    /* ANIM */
    ui.menu_sel = MI_ANIM;
    ok = str_eq(MI_NAME[MI_ANIM], "ANIM") && !(ui_prefs & PREF_ANIM_OFF);
    turn(EN_K1, 1); ok &= !(ui_prefs & PREF_ANIM_OFF);            /* right: ON (stays: the default) */
    turn(EN_K1, -1); ok &= (ui_prefs & PREF_ANIM_OFF) != 0;       /* left: OFF */
    turn(EN_K1, 1); ok &= !(ui_prefs & PREF_ANIM_OFF);            /* right: ON (the switch's knob right) */
    press(B_OCTDN); ok &= (ui_prefs & PREF_ANIM_OFF) && ui.menu;  /* OCT-: OFF */
    ok &= !(ui_prefs & ~(uint32_t)PREF_ANIM_OFF);                 /* (no other setting moved) */
    press(B_HOME);
    bad += check("#46 MENU > ANIM: ON by default, KNOB 1 right / OCT+ ON, left / OCT- OFF", ok && !ui.menu);
    ui_prefs = 0;
    ui_power_on(); roll_settle();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 1 && ui.roll[3].from[0];              /* ON: TUNE 0 -> 1 rolls */
    ui_power_on(); ui_prefs = PREF_ANIM_OFF; roll_settle();      /* (ui_power_on clears the favorites record) */
    turn(EN_K4, 1);
    ok &= song.g[G_TUNE] == 1 && !ui.roll[3].from[0] && roll_static_now();   /* OFF: the value at once, static */
    turn(EN_SELECT, 1);
    ok &= !ui.roll[ROLL_BPM].from[0];
    bad += check("#46 ANIM OFF: the card values and the header BPM snap (no rolling digits), the frame is static", ok);
    {   /* the piano roll's view: ON glides toward the notes over frames, OFF jumps there at once */
        track_t *t = TSEL;
        uint32_t lo_on;
        ui_power_on();
        for (k = 0; k < NSTEP; k++) step_clear(&t->step[k]);
        t->step[0].n = 1; t->step[0].note[0] = 30; t->step[0].time = ST_NOTE;
        ui.force = 0;
        proll.init = 0; pr_follow(t); ui.frame++;
        t->step[0].note[0] = 90; pr_follow(t); lo_on = proll.lo;
        ui_prefs = PREF_ANIM_OFF;
        proll.init = 0; t->step[0].note[0] = 30; pr_follow(t); ui.frame++;
        t->step[0].note[0] = 90; pr_follow(t);
        ok = proll.moving == 0 && proll.lo > lo_on && lo_on != 0u;
        if (!ok) printf("  piano roll view: ON %u, OFF %u (moving %u)\n", lo_on, proll.lo, proll.moving);
        bad += check("#46 ANIM OFF: the piano roll's view jumps to the notes (ON: it glides)", ok);
    }
    {   /* saved with the settings; settings from before it: ON */
        persist_t p;
        ui_prefs = PREF_ANIM_OFF;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & PREF_ANIM_OFF);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & ~PREF_ACCEL_OFF);
        bad += check("#46 ANIM saved with the settings; settings from before it: ON", ok);
    }
    ui_prefs = 0;
    return bad;
}

/* MENU > STYLE (ui_menu.c, gfx.c ST_*): FLAT by default; KNOB 1 right LINE, left FLAT (stops at the ends), OCT+
 * toggles; drawn so from the next frame (ux.style, the tokens: LINE has no SURF, a rule under the header); saved in a
 * spare byte of the settings and read back; settings from before it, and an unknown value, read as FLAT; 2, the
 * retired PIXEL (1.0.1), as LINE (again on a second import: idempotent) */
static int test_style(void)
{
    int bad = 0, ok;
    const uint32_t rule = 26u * 240u + 120u;           /* (the rule under the header: ui_draw.c draw_rules) */
    ui_power_on();
    frame();
    ok = ui_style == ST_FLAT && ux.style == ST_FLAT && ux.surf != ux.bg;
    ui.force = 1; frame();
    ok &= host_screen[rule] == swap16(T_BG);
    hold(B_HOME); ui.menu_sel = MI_STYLE;
    ok &= ui.menu == 1u && str_eq(MI_NAME[MI_STYLE], "STYLE");
    turn(EN_K1, 1); ok &= ui_style == ST_LINE && ux.style == ST_LINE && ux.surf == ux.bg;
    turn(EN_K1, 1); ok &= ui_style == ST_LINE;
    turn(EN_K1, -1); ok &= ui_style == ST_FLAT;
    turn(EN_K1, -1); ok &= ui_style == ST_FLAT;
    press(B_OCTUP); ok &= ui_style == ST_LINE && ui.menu == 1u;
    press(B_OCTUP); ok &= ui_style == ST_LINE;
    press(B_OCTDN); ok &= ui_style == ST_FLAT && ux.style == ST_FLAT && ux.surf != ux.bg && ui.menu == 1u;
    press(B_OCTUP); ok &= ui_style == ST_LINE;
    ok &= !(ui_prefs & 0xFFu & ~PREF_ACCEL_OFF) && settings_leds == LEDS_DIM;   /* (no other setting moved) */
    hold(B_HOME);
    ok &= !ui.menu && ux.style == ST_LINE && host_screen[rule] == swap16(T_RULE);
    bad += check("MENU > STYLE: FLAT by default, KNOB 1 / OCT-+ FLAT / LINE, LINE drawn (rules, no SURF)", ok);
    {
        persist_t p, q;
        settings_export(&p);
        ui_style = ST_FLAT; frame();
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_style == ST_LINE;
        frame();
        ok &= ux.style == ST_LINE;
        p.favorites.factory[15][29] = 7;                /* unknown: FLAT, and so again */
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_style == ST_FLAT && !p.favorites.factory[15][29];
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && ui_style == ST_FLAT && !memcmp(&p, &q, sizeof p);
        p.favorites.factory[15][29] = 2;                /* the retired PIXEL: LINE, and so again */
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_style == ST_LINE && p.favorites.factory[15][29] == ST_LINE;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && ui_style == ST_LINE && !memcmp(&p, &q, sizeof p);
        frame();
        ok &= ux.style == ST_LINE;
        ui_style = 2; frame();                          /* (a live PIXEL byte, a restore: LINE) */
        ok &= ui_style == ST_LINE && ux.style == ST_LINE;
        ui_style = ST_LINE;
        settings_export(&p);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                          /* settings from before STYLE (PER3) */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && ui_style == ST_FLAT;
        frame();
        ok &= ux.style == ST_FLAT && ux.surf != ux.bg;
        bad += check("  STYLE saved with the settings and read back; older settings and unknown values: FLAT, PIXEL: LINE (idempotent)", ok);
    }
    ui_power_on(); frame();
    return bad;
}

/* the header (ui_draw.c draw_head, 1.2's three slots): its elements measured in the render (every pixel off BG; an
 * element is a run of inked columns, split at 2 empty columns and at the battery's cell), each one's ink centred on row
 * 12 (top + bottom + 1 within 1 px of H_HEAD); the track cushion's ink from x 4; the battery's in 214..236, nothing
 * between 208 and it; with the centre shown the BPM's ink within 2 px of x 120; a message / a label centred on x 120
 * (its ink between 40 and 205, within 3 px); GLO with the tempo turned: the label right of the BPM and its lock, no
 * battery. Every state of the header, FLAT and LINE, every palette */
#define HEAD_EL 32
static int32_t hel[HEAD_EL][4], nhel;                  /* x0 x1 (last inked column) top bot */
static int head_centres(const char *what, int print)
{
    uint16_t bg = swap16(T_BG);
    int32_t x = 0, ok = 1;
    nhel = 0;
    while (x < 240) {
        int32_t x0, x1, top = 99, bot = -1, gap = 0, y, ink;
        for (; x < 240; x++) {                        /* the next inked column */
            for (y = 0, ink = 0; y < H_HEAD; y++) ink |= host_screen[(Y_HEAD + y) * 240 + x] != bg;
            if (ink) break;
        }
        if (x >= 240) break;
        x0 = x1 = x;
        for (; x < 240 && gap < 2; x++) {
            if (x == HEAD_BAT_X && x != x0) break;     /* the battery's cell ends the element */
            for (y = 0, ink = 0; y < H_HEAD; y++)
                if (host_screen[(Y_HEAD + y) * 240 + x] != bg) {
                    ink = 1;
                    if (y < top) top = y;
                    if (y > bot) bot = y;
                }
            gap = ink ? 0 : gap + 1;
            if (ink) x1 = x;
        }
        x = x1 + 1;
        if (print) printf("    head %-10s x %3d..%3d ink rows %2d..%2d centre %4.1f (%+.1f)\n", what, x0, x1, top, bot,
                          (top + bot + 1) / 2.0, (top + bot + 1) / 2.0 - H_HEAD / 2);
        ok &= top + bot + 1 >= H_HEAD - 2 && top + bot + 1 <= H_HEAD + 2;
        if (nhel < HEAD_EL) { hel[nhel][0] = x0; hel[nhel][1] = x1; hel[nhel][2] = top; hel[nhel][3] = bot; nhel++; }
    }
    return ok;
}
/* the elements inside columns a .. b as one span (x0 .. x1): 2 x its centre, 0 none */
static int32_t head_span2(int32_t a, int32_t b, int32_t *x0, int32_t *x1)
{
    int32_t i;
    *x0 = 999; *x1 = -1;
    for (i = 0; i < nhel; i++)
        if (hel[i][0] >= a && hel[i][1] <= b) {
            if (hel[i][0] < *x0) *x0 = hel[i][0];
            if (hel[i][1] > *x1) *x1 = hel[i][1];
        }
    return *x1 < 0 ? 0 : *x0 + *x1 + 1;
}
enum { HS_STOPPED, HS_PLAY_REC, HS_SONG, HS_MSG, HS_OCT, HS_USB, HS_KEYCAP, HS_REC_OTHER, HS_BPM_TURN, HS_BPM_LOCK,
       HS_NOFILE, HS_REC_LAYER, HS_LAYER_LOCK, HS_GLO_TURN, HS_MSG2, HS_BEAT2, HS_COUNTIN, HS_BATT1, HS_BATT0, HS_BPM90,
       HS_N };
static const char *const HS_NAME[HS_N] = {"stopped", "play+REC", "song", "message", "OCT", "USB", "keycap", "REC other",
    "BPM turn", "BPM LOCK", "NO SAMPLE", "REC layer", "EDIT lock", "GLO turn", "msg2", "beat 2", "count-in", "battery 1",
    "battery 0", "BPM 90"};
static void head_state(uint32_t s)
{
    song.playing = s == HS_PLAY_REC || s == HS_SONG || s == HS_BEAT2 || s == HS_OCT;
    song.rec = s == HS_PLAY_REC || s == HS_COUNTIN ? 1u << song.sel : s == HS_REC_OTHER ? 1u << ((song.sel + 1u) % NTRK) : 0u;
    clk_pos = 0; clk_step = s == HS_BEAT2 ? 4u : 8u;    /* (the 2nd beat: the metronome mirrored; else the 3rd) */
    if (s == HS_SONG) { chain.running = 1; chain.row = 2; }
    if (s == HS_MSG) ui_message("SAVED 03 BASS");
    if (s == HS_MSG2) ui_say("LOADED ", "07 A VERY LONG PATTERN NAME");
    if (s == HS_OCT) song.octave = 2;
    usb.config = s == HS_USB; usb.suspended = 0;
    song.batt_raw = s == HS_BATT1 ? 540 : s == HS_BATT0 ? 500 : 600;
    if (s == HS_KEYCAP) ui_message("[OCT+] SAVE");
    if (s == HS_BPM_TURN || s == HS_GLO_TURN) ui.bpm_t = 40;
    if (s == HS_BPM_LOCK || s == HS_GLO_TURN) ui_prefs |= PREF_BPM_LOCK;
    if (s == HS_NOFILE) ui_message(MSG_NOFILE "NO FILE");   /* (an icon-led message: the header keeps it) */
    if (s == HS_REC_LAYER) { song.rec = 1u << song.sel; ui.layer = LAYER_REC; }
    if (s == HS_LAYER_LOCK) ui.layer = LAYER_EDIT;
    if (s == HS_GLO_TURN) ui.layer = LAYER_GLO;
    if (s == HS_COUNTIN) { cin_total = 4; cin_left = 3; }
    song.g[G_BPM] = s == HS_BPM90 ? 90 : 124;
    ui.roll_bpm = song.g[G_BPM];
}
static void head_clear(void)
{
    chain.running = 0; ui.msg_t = 0; song.octave = 0; usb.config = 0; song.playing = 0; song.rec = 0; ui.bpm_t = 0;
    ui_prefs &= ~PREF_BPM_LOCK; ui.layer = 0; cin_total = cin_left = 0; song.batt_raw = 600;
}
static int test_head_centres(void)
{
    int bad = 0, ok = 1, place = 1;
    uint32_t st, p, s;
    for (st = ST_FLAT; st <= ST_LINE; st++)
        for (p = 0; p < NPALETTES; p++)
            for (s = 0; s < HS_N; s++) {
                int32_t a, b, c2, msg = s == HS_MSG || s == HS_KEYCAP || s == HS_NOFILE || s == HS_REC_LAYER ||
                                        s == HS_LAYER_LOCK || s == HS_MSG2;
                int pr = !st && !p, okp;
                ui_power_on();
                ui_style = (uint8_t)st; style_apply();
                settings.palette = p; palette_set(p);
                head_state(s);
                memset(host_screen, 0, sizeof host_screen);
                ui.force = 1; draw_head(); ui.force = 0;
                ok &= head_centres(HS_NAME[s], pr);
                okp = nhel > 0 && hel[0][0] == 4;      /* the track cushion first, its ink from x 4 */
                if (s != HS_GLO_TURN) {                /* the battery last, in its cell; between 208 and it only the
                                                        * load meter (JIANT 0.5: 3 px at 209) */
                    okp &= head_span2(HEAD_BAT_X, 239, &a, &b) && a >= 214 && b <= 236;
                    okp &= !head_span2(HEAD_GRP_R, HEAD_BAT_X - 6, &a, &b) && !head_span2(HEAD_BAT_X - 2, HEAD_BAT_X - 1, &a, &b);
                } else {                               /* GLO, the tempo turned: the BPM, its lock, the label after */
                    head_geo_t g;
                    head_geo(&g, "124");
                    okp &= head_span2(g.bx + g.b[2] + 20, 239, &a, &b) && a >= 142;
                }
                if (!msg) {                            /* the BPM's ink on x 120 (between the metronome and the lock) */
                    head_geo_t g;
                    head_geo(&g, s == HS_BPM90 ? "90" : "124");
                    c2 = head_span2(g.mx1, g.bx + g.b[2] + HEAD_GAP - 1, &a, &b);
                    okp &= c2 >= 236 && c2 <= 244;
                } else {                               /* the message centred on x 120 */
                    c2 = head_span2(40, 205, &a, &b);
                    okp &= c2 >= 234 && c2 <= 246 && a >= 40 && b <= 205;
                }
                if (pr && !okp) printf("    head %s: misplaced\n", HS_NAME[s]);
                place &= okp;
                head_clear();
            }
    ui_style = ST_FLAT; style_apply();
    bad += check("header: every element's ink centred on row 12 (+-1 px), every state, FLAT and LINE, every palette", ok);
    bad += check("header: three slots: track at 4, BPM / message on x 120, octave to 208, battery 214..236 (GLO with "
                 "the tempo turned: the label right of the BPM and its lock)", place);
    ui_power_on();
    return bad;
}

/* the header's metronome (ui_draw.c head_metro): it swings on the beats the click is on (seq.c click_beat_now): the
 * bar's 2nd and 4th beats mirrored, the 1st the accent's, the 3rd as stopped; a beat redraws its ink only (the rest
 * of the screen untouched, the header's signature kept) */
static int test_head_metronome(void)
{
    static uint16_t shot[4][H_HEAD * 240], still[H_HEAD * 240];
    head_geo_t g;
    int bad = 0, ok = 1, only = 1, b;
    int32_t x, y, w;
    ui_power_on();
    settings.palette = 0; palette_set(0);
    song.g[G_BPM] = 124; ui.roll_bpm = 124;
    head_geo(&g, "124");
    w = g.mx1 - g.mx0;
    ui.force = 1; draw_head(); ui.force = 0;           /* stopped */
    memcpy(still, host_screen, sizeof still);
    song.playing = 1; clk_pos = 0;
    for (b = 0; b < 4; b++) {
        uint32_t sig;
        clk_step = (uint32_t)b * 4u + (b & 1u) * 2u;   /* (inside the beat: its 1st or 3rd 1/16) */
        ui.force = 1; draw_head(); ui.force = 0;
        memcpy(shot[b], host_screen, sizeof shot[b]);
        sig = ui.head_sig;
        clk_step = (uint32_t)((b + 1) % 4) * 4u;       /* the next beat: only the metronome's ink changes */
        draw_head();
        only &= ui.head_sig == sig;
        for (y = 0; y < H_HEAD; y++)
            for (x = 0; x < 240; x++)
                if (host_screen[y * 240 + x] != shot[b][y * 240 + x]) only &= x >= g.mx0 && x < g.mx1;
    }
    for (b = 0; b < 4; b++)                            /* 1.1.5: no swing: every beat as stopped */
        for (y = 0; y < H_HEAD; y++)
            for (x = 0; x < w; x++)
                ok &= shot[b][y * 240 + g.mx0 + x] == still[y * 240 + g.mx0 + x];
    bad += check("header metronome: still on every beat, as stopped", ok);
    bad += check("header metronome: a beat changes nothing in the header", only);
    song.playing = 0;
    ui_power_on();
    return bad;
}

/* (JIANT 0.4) one scale: ROOT / SCALE changed on the selected track become every melodic track's (not a DRUM
 * track's; selecting a track changes nothing); STRN moves the notes along the scale */
static int test_scale_link(void)
{
    int bad = 0, ok;
    uint32_t k;
    ui_power_on();
    go_home(); frames(8);
    for (k = 0; k < 3u; k++)
        set_engine_of(&trk[k], ENGI_PHYS_TO);      /* (ANALOG) */
    set_engine_of(&trk[3], ENGI_DRUM);
    for (k = 0; k < NTRK; k++) { trk[k].p[P_ROOT] = 0; trk[k].p[P_SCALE] = 0; }
    frames(48);
    song.sel = 1; frames(48);
    TSEL->p[P_ROOT] = 9; TSEL->p[P_SCALE] = 2;    /* (any path: a knob, the layer, the editor) */
    frames(48);
    ok = trk[0].p[P_ROOT] == 9 && trk[2].p[P_SCALE] == 2 && trk[1].p[P_ROOT] == 9 && trk[3].p[P_ROOT] == 0 && trk[3].p[P_SCALE] == 0;
    trk[2].p[P_ROOT] = 4; trk[2].p[P_QUANT] = 1;   /* (0.6.19) another track's own value (MIDI, a load): undone */
    frames(16);
    ok &= trk[2].p[P_ROOT] == 9 && trk[2].p[P_QUANT] == trk[1].p[P_QUANT];
    song.sel = 2; frames(48);
    ok &= trk[0].p[P_ROOT] == 9 && trk[2].p[P_ROOT] == 9;
    TSEL->p[P_QUANT] = 2; frames(16);              /* QNT is the song's too */
    ok &= trk[0].p[P_QUANT] == 2 && trk[1].p[P_QUANT] == 2;
    bad += check("SCALE (0.6.19): ROOT / SCALE / QNT one for every melodic track (not DRUM); no track keeps its own", ok);
    trk[0].p[P_ROOT] = 0;                          /* C major (SCALE 1: check its mask) */
    trk[0].p[P_SCALE] = 0;
    {
        int32_t a = scale_steps(&trk[0], 60, 2), b = scale_steps(&trk[0], 60, -1), c = scale_steps(&trk[0], 61, 0);
        uint32_t m = scale_mask(&trk[0]);
        ok = (m == 0xFFFu && a == 62 && b == 59 && c == 61) || (m == 0xAB5u && a == 64 && b == 59 && c == 60);
        trk[0].p[P_SCALE] = 1;
        m = scale_mask(&trk[0]);
        a = scale_steps(&trk[0], 60, 7);           /* a 7-note scale: 7 steps = an octave */
        ok &= __builtin_popcount(m) != 7 || a == 72;
        printf("  STRN: masks %03x / %03x, C4 +2 -> %d\n", (unsigned)SCALE_MASK[0], (unsigned)m, (int)scale_steps(&trk[0], 60, 2));
    }
    bad += check("STRN: the notes up / down the scale's steps (a 7-note scale: 7 steps an octave)", ok);
    return bad;
}

/* (JIANT 0.4) HOME tapped twice: M1..M4 latched on HOME (its knobs and screen) until HOME is tapped there again;
 * MACRO DICE: routes from the macros to what each sound has, two a track; the session's only where none were */
static int test_organic(void)
{
    int bad = 0, ok;
    uint32_t k, j, routes = 0, per[4] = {0, 0, 0, 0}, bad_dst = 0;
    int16_t cut;
    ui_power_on();
    go_home(); frames(400);
    cut = TSEL->p[ENGINES[TSEL->eng_req]->knob[0]];
    turn(EN_K1, 2);
    ok = !mac_latch && TSEL->p[ENGINES[TSEL->eng_req]->knob[0]] != cut;   /* HOME: the sound's four */
    cut = TSEL->p[ENGINES[TSEL->eng_req]->knob[0]];
    btn_down(B_LFO); frames(560); btn_up(B_LFO); frames(64);
    ok &= mac_latch && !ui.layer;                  /* LFO held, let go: latched (its cards drawn, no layer open) */
    macro_v[0] = 0;
    turn(EN_K1, 5);
    ok &= macro_v[0] == 5 && TSEL->p[ENGINES[TSEL->eng_req]->knob[0]] == cut;   /* (M1, not the sound's KNOB 1) */
    go_page(GR_ADSR); frames(64);
    turn(EN_K2, 1);
    ok &= mac_latch && macro_v[1] == 1;            /* on another page too */
    press(B_LFO); frames(400);
    ok &= !mac_latch && !str_eq(cur_page()->title, "LFO");   /* LFO again: off, no LFO page */
    go_home(); frames(64);
    turn(EN_K1, 1);
    ok &= macro_v[0] == 5;
    bad += check("MACRO: LFO held, let go: latched (M1..M4 on the knobs, any page); LFO again: off; HOME the sound's", ok);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        for (j = 0; j < 12u; j++)
            trk[k].p[P_M1SRC + j] = 0;
    trk[2].p[P_M1SRC] = MS_LFO; trk[2].p[P_M1DST] = MD_CUT; trk[2].p[P_M1AMT] = 30;   /* (a route of its own: kept) */
    session_dice(12345u);
    for (k = 0; k < NTRK; k++)
        for (j = 0; j < 4u; j++) {
            int16_t *m = &trk[k].p[P_M1SRC + 3u * j];
            if (m[0] >= MS_M1 && m[0] < MS_M1 + 4) {
                const param_desc_t *pd = m[1] >= MD_E1 && m[1] < MD_E1 + 8 ? track_desc(&trk[k], P_E0 + (uint32_t)(m[1] - MD_E1)) : 0;
                routes++;
                per[m[0] - MS_M1]++;
                bad_dst += !m[1] || !m[2] || (pd && (pd->max <= pd->min || pd->label[0] == '-'));
            }
        }
    ok = routes == 2u * NTRK && per[0] == NTRK / 2u && per[1] == NTRK / 2u && per[2] == NTRK / 2u && per[3] == NTRK / 2u && !bad_dst &&
         trk[2].p[P_M1SRC] == MS_LFO && trk[2].p[P_M1AMT] == 30;
    session_dice(999u);                            /* (a session restored: its routes stay, none added) */
    for (k = 0, j = 0; k < NTRK * 4u; k++)
        j += trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] >= MS_M1 && trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] < MS_M1 + 4;
    ok &= j == 2u * NTRK;
    bad += check("MACRO DICE at power-on: 2 routes a track to what its sound has, each macro 3; own routes kept; restored: none added", ok);
    go_home(); frames(64);
    song.playing = 1;                              /* (JIANT 0.5: a reroll routes the tracks that sound: playing, their steps) */
    for (k = 0; k < NTRK; k++) {
        trk[k].step[0].time = ST_NOTE; trk[k].step[0].n = 1; trk[k].step[0].note[0] = 60; trk[k].step[0].hit = 1;
    }
    btn_down(B_LFO); frames(560);
    key_down(white(1)); frame(); key_up(white(1)); frame();   /* G3: CLEAR */
    for (k = 0, j = 0; k < NTRK * 4u; k++)
        j += trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] >= MS_M1 && trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] < MS_M1 + 4;
    ok = j == 0u && trk[2].p[P_M1SRC] == MS_LFO;
    key_down(white(0)); frame(); key_up(white(0)); frame();   /* F3: DICE */
    for (k = 0, j = 0; k < NTRK * 4u; k++)
        j += trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] >= MS_M1 && trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] < MS_M1 + 4;
    ok &= j == 2u * NTRK;
    btn_up(B_LFO); frames(400);
    bad += check("MACRO layer: G3 clears the macros' routes (others kept), F3 rolls new ones", ok);
    {   /* (JIANT 0.5) a reroll: only the tracks that sound now (track 2 muted: none) */
        trk[1].p[P_MUTE] = 1;
        macro_dice(2); macro_dice(1);
        for (k = 0, j = 0; k < 4u; k++)
            j += trk[1].p[P_M1SRC + 3u * k] >= MS_M1 && trk[1].p[P_M1SRC + 3u * k] < MS_M1 + 4;
        ok = j == 0u && trk[0].p[P_M1SRC] >= MS_M1;
        song.playing = 0;
        macro_dice(2); macro_dice(1);              /* (stopped, silent: nothing sounds, no routes) */
        for (k = 0, j = 0; k < NTRK * 4u; k++)
            j += trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] >= MS_M1 && trk[k / 4u].p[P_M1SRC + 3u * (k % 4u)] < MS_M1 + 4;
        ok &= j == 0u;
        trk[1].p[P_MUTE] = 0;
        song.playing = 1;
        macro_dice(1);
        bad += check("JIANT 0.5: the macro dice routes only the tracks that sound now (a muted or silent one: none)", ok);
    }
    {   /* SELECT on HOME's macros: the routes rolled again (once a turn), the tempo untouched; on the sound's four: tempo */
        int16_t bpm = song.g[G_BPM], before[NTRK][12];
        uint32_t diff = 0;
        mac_latch = 1;
        go_home(); frames(64);
        for (k = 0; k < NTRK; k++)
            for (j = 0; j < 12u; j++) before[k][j] = trk[k].p[P_M1SRC + j];
        macro_dice(1);                                 /* (0.6.67: F3; SELECT alone scrolls the pages) */
        for (k = 0; k < NTRK; k++)
            for (j = 0; j < 12u; j++) diff += before[k][j] != trk[k].p[P_M1SRC + j];
        ok = diff > 0u && song.g[G_BPM] == bpm;
        mac_latch = 0;
        fm1_ms += 1200u;
        held_turn(B_HOME, 1);
        ok &= song.g[G_BPM] == bpm && ui.confirm == CF_DICE_ALL;   /* (0.6.67) HOME + SELECT: DICE EVERYTHING? */
        press(B_OCTDN);
        ok &= !ui.confirm && song.g[G_BPM] == bpm;
        song.playing = 0;
        bad += check("the latched macros' dice (the tempo kept); HOME + SELECT: the DICE ALL dialog", ok);
    }
    return bad;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int bad = 0;
    /* (QG-1 stage 2) one test on its own, and as many times as wanted, instead of the whole suite for each
     * run:  UI_ONLY=dice_all ./ui_test          DICE ALL first, in a fresh process
     *       UI_REPEAT_DICE=100 ./ui_test        DICE ALL 100 times in one process (the state it leaves
     *                                           behind, the generator's last-64-seeds memory and the
     *                                           clock included, must not change the outcome) */
    {
        const char *only = getenv("UI_ONLY");
        const char *rep = getenv("UI_REPEAT_DICE");
        if (only && !strcmp(only, "dice_all")) {
            int n = rep ? atoi(rep) : 1, i;
            if (n < 1) n = 1;
            for (i = 0; i < n; i++) bad += test_dice_all();
            if (bad) printf("ui: dice_all x%d: %d FAILED\n", n, bad);
            else printf("ui: dice_all x%d: all passed\n", n);
            return bad ? 1 : 0;
        }
    }
    bad += test_large_face();
    bad += test_sound_loads();
    bad += test_patterns();
    bad += test_init_system();
    bad += test_transitions();
    bad += test_mutate();
    bad += test_morph();
    bad += test_morph_click();
    bad += test_lfo_sync();
    bad += test_init_dry();
    bad += test_dximg();
    bad += test_dxlay();
    bad += test_rec();
    bad += test_step_rec();
    bad += test_step_rec();
    bad += test_presets_knob();
    bad += test_midi();
    bad += test_save();
    bad += test_actions();
    bad += test_tracks();
    bad += test_grid();
    bad += test_screen();
    bad += test_favorites();
    bad += test_display_preferences();
    bad += test_menu_tabs();
    bad += test_issues_111();
    bad += test_information();
    bad += test_chain();
    bad += test_product_ux();
    bad += test_mono_screens();
    bad += test_roll();
    bad += test_soak_cards();
    bad += test_panel();
    bad += test_layer();
    bad += test_layer_knob_leak();
    bad += test_layer_knob_race();
    bad += test_fx_latch();
    bad += test_rec_layer();
    bad += test_song_layer();
    bad += test_project_page();
    bad += test_lofi_rtrg();
    bad += test_key_latch();
    bad += test_load_budget();
    bad += test_dice_all();
    bad += test_gen_power_on();
    bad += test_gen_reproducible();
    bad += test_gen_pages();
    bad += test_gen_reel();
    bad += test_gen_v2();
    bad += test_gen_layer();
    bad += test_gen_song_takes();
    bad += test_gen_store();
    bad += test_gen_conflicts();
    bad += test_gen_ux();
    bad += test_gen_screen();
    bad += test_gen_now();
    bad += test_gen_set_loops();
    bad += test_midi_song_pc();
    bad += test_chord();
    bad += test_trans_seq();
    bad += test_note_page();
    bad += test_chord_glide();
    bad += test_fx_sections();
    bad += test_clear_ghosts();
    bad += test_dx_sound();
    bad += test_clear_all();
    bad += test_lane_mutes();
    bad += test_seq_tools();
    bad += test_menu_prefs();
    bad += test_style();
    bad += test_head_centres();
    bad += test_head_metronome();
    bad += test_div_order();
    bad += test_knob_accel();
    bad += test_scale_link();
    bad += test_organic();
    bad += test_usb_level();
    bad += test_click_menu();
    bad += test_usb_serial();
    bad += test_bpm_lock();
    bad += test_large();
    bad += test_browse_no_pattern();
    bad += test_name();
    bad += test_edit_cycle();
    bad += test_quick_layers();
    bad += test_shift_page();
    bad += test_macro_layer();
    bad += test_fm6_ops();
    bad += test_rec_combos();
    bad += test_bughunt_ui();
    bad += test_bughunt_ui2();
    bad += test_piano_roll();
    bad += test_play_leds();
    bad += test_cursor_step_leds();
    bad += test_midi_leds();
    bad += test_seq_quant();
    bad += test_idle_glow();
    bad += test_breath();
    bad += test_step_leds();
    bad += test_scale_leds();
    bad += test_screen_off();
    bad += test_fm6_charts();
#if FELUCCA_FM4
    bad += test_fm_charts();                        /* (DIGITAL's charts: built with FELUCCA_FM4=1 only) */
#else
    bad += test_fm4_retired();
#endif

    if (getenv("UI_SCREEN_DIR")) chain_screens(getenv("UI_SCREEN_DIR"));
    printf(bad ? "ui: %d FAILED\n" : "ui: all passed\n", bad);
    return bad ? 1 : 0;
}
#endif
