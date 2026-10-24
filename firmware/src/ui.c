/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA user interface.
 * Flat: SURF cards and panels on the palette's background, no rules, one type family (Inter Tight, three sizes), tracks
 * named by their numbers 1..4 on a cushion (icons.c trk_icon). Four columns <-> KNOB 1..4. Rendering is lazy:
 * every element remembers what it last drew and is redrawn only on change. */
static int project_save(uint32_t slot);
static void panel_setup(void);
static void project_load(uint32_t slot);
static int section_jump(uint32_t s);    /* (song_main.c: 0.6.80, channel 16's PROGRAM CHANGE) */
static int song_select(uint32_t n);
static void undo_mark(void);                    /* (0.6.21) project.c: the music before a change, */
static void system_init(void);                  /* (0.6.40) project.c: INIT SYSTEM */
static uint32_t mut_progress(uint32_t k);       /* (0.6.43) project.c: MUTATE's next, 0..256 (0xFFFF: off) */
static void mut_keep(void);                     /* .. the music now the anchor */
static uint8_t mut_flash[NTRK];                 /* .. a mutation just now: frames left of its flash (HOME) */
static uint8_t mut_freeze;                      /* .. GLO + A4: no mutation until again */
static const uint8_t MUT_BARS[7] = {0, 1, 2, 4, 8, 16, 32};   /* .. SOUND / PATTERN every (OFF, bars) */
static int undo_do(void);                       /* .. back (and again: forward) */
static uint8_t undo_ok;
static int project_used(uint32_t slot);
static int project_recall_sound(uint32_t slot, uint32_t k);
static int undo_sound(uint32_t k);                 /* (0.6.65) project.c: the sound dice's CANCEL */
static int project_recall_slot(void);
static uint32_t chain_prepare(void);
static int up_used(uint32_t k);              /* user presets: upreset.c */
static int up_load(uint32_t k);
static uint32_t up_count(void);
static uint32_t up_nth(uint32_t n);
static uint32_t up_rank(uint32_t slot);
static uint32_t up_engine(uint32_t k);
static void up_name(uint32_t k, char *b);
static void up_slot_label(char *b, uint32_t k);
static void up_ui(uint32_t op, uint32_t k);
static int up_has_pat(uint32_t k);               /* user presets that hold a pattern */
static uint32_t up_pat_count(void);
static uint32_t up_pat_nth(uint32_t n);
static uint32_t up_pat_rank(uint32_t slot);
static void up_pat_load(track_t *t, uint32_t k);
static void up_auto_name(char *b, uint32_t e, uint32_t k);   /* naming (ui_name.c) */
static void up_ui_named(uint32_t op, uint32_t k, const char *name);
static int project_save_as(uint32_t slot, const char *name);
static int project_name(uint32_t slot, char *b);
static int project_rename(uint32_t slot, const char *name);
static void project_cur_name(char *b);
static uint32_t user_of(const track_t *t)    /* user preset slot its sound came from, UP_SLOTS = none */
{
    return t->user && up_used(t->user - 1u) ? t->user - 1u : UP_SLOTS;
}
static uint32_t up_gen;                      /* bumped on every user bank change (redraws) */
#include "favorites.c"
/* MENU's two-valued settings (ui_menu.c MENU_FLAGS), a bit each in a byte no engine uses (as ui_layer.c layer_seen):
 * saved with the settings; 0 in older ones = every setting's default (append-only: a new setting takes a new bit,
 * its default is 0) */
#define ui_prefs (favorites.factory[15][30])
#define PREF_LATCH 1u                          /* MENU > FX LATCH ON (#40); JIANT 0.6.44: ON once for everyone (prefs_rev);
                                                * 0.6.64: OFF again (the FX screen locks open by itself) */
#define prefs_rev (favorites.factory[15][25])  /* (JIANT 0.6.44) the settings' revision (a byte no engine uses: engine 15):
                                                * 0 before; 1: FX LATCH turned ON once (main.c prefs_update); 2: OFF once (0.6.64) */
#define PREF_ANIM_OFF 2u                       /* MENU > ANIM OFF (#46): values snap (no rolling digits, no glide) */
/* (0.6.57) values snap: MENU > ANIM OFF, or LOAD LITE / ECO (the rolling digits, the roll's and the tabs' glides are
 * frames drawn again for a look: the time goes to the keys and knobs instead; the beings still move) */
#define ANIM_SNAP() ((ui_prefs & PREF_ANIM_OFF) || sys_load)
#define PREF_ACCEL_OFF 4u                      /* MENU > KNOB ACCEL OFF (#52; JIANT: ON by default, the bit now means OFF):
                                                * fast turns of wide values x2..x16 */
#define PREF_USB_FIXED 8u                      /* MENU > USB LEVEL FIXED: USB audio at the full level, MASTER after */
#define PREF_BPM_LOCK 16u                      /* MENU > BPM LOCK ON (#58): DICE ALL keeps the tempo; (0.6.67) SELECT
                                                * alone never sets it (GLO + SELECT does) */
#define PREF_LARGE 32u                         /* MENU > LARGE ON (#15, Discussion #80): big knob labels and values
                                                * (ui_draw.c large_kind); clear in every older setting = OFF */
#define PREF_SERIAL_OFF 64u                    /* MENU > USB SERIAL OFF (#67): no serial console, the device enumerates as
                                                * audio + MIDI only (usb.c usb_cdc_switch); clear in every older setting = ON */
#define PREF_RESTORE_ON 128u                   /* MENU > RESTORE LAST ON (1.2, Discussion #130: the autosave written and
                                                * restored at power-on, project.c). JIANT 0.6.40: OFF by default (clear =
                                                * OFF, power-on as INIT SYSTEM leaves it); the bit was RESTORE LAST OFF */
#define fx_latch ui_prefs
/* (JIANT 0.6.44) the settings brought up to date at power-on (main.c; 0.6.58 the browser emulator too, web/emu, where
 * the punch-in FX had no LATCH): FX LATCH ON by default, once; (0.6.64) OFF again, once, so who turns it ON after
 * keeps it so */
static void prefs_update(void)
{
    if (prefs_rev < 2u) {                              /* (0.6.64) FX LATCH OFF again (the punch-in effects momentary:
                                                        * held FX locks the FX screen open instead), once */
        ui_prefs &= (uint8_t)~PREF_LATCH;
        prefs_rev = 2u;
        settings_save();
    }
}
/* MENU > AUDIO CLICK, CLICK LEVEL, COUNT-IN (1.1, Discussion #131; click.c, seq.c): two bits each in another byte no
 * engine uses, saved with the settings: bits 0-1 CLICK (OFF REC ON), 2-3 CLICK LEVEL (stored 0 MID, 1 LOW, 2 HIGH),
 * 4-5 COUNT-IN (OFF, 1 BAR, 2 BARS); 0 in older settings = OFF / MID / OFF, so nothing clicks or counts in that did
 * not; 3 (unknown) reads as 0 (settings_persist.c puts it back). Bit 6 (1.2): MENU > SCALE LEDS ON (Discussion #127,
 * ui_input.c scale_leds; 0 in older settings = OFF). Bit 7 free (append-only) */
#define ui_rec_prefs (favorites.factory[15][28])
#define PREF_REC 0xFF00u                       /* (menu_items.c MENU_FLAGS: a bit of ui_rec_prefs, << 8) */
#define PREF_SCALE_LEDS (0x40u << 8)
#define ui_load (favorites.factory[15][26])    /* (JIANT 0.6.30) LOAD (voice.c sys_load): 0 FULL, 1 LITE, 2 ECO; a byte no
                                                * engine uses, 0 in older settings (0.6.29's 6 LITE: 1, LITE) */
#define ui_sel_home (favorites.factory[15][22])     /* (1.1.1-GW, issue #3) MENU > GEN > SELECT ON HOME: 0 GENERATOR (older
                                                    * settings, as ever), 1 TEMPO (SELECT alone on HOME sets the BPM; the
                                                    * generator is HOME held + SELECT, OCT- + PLAY); a byte no engine uses */
#define midi_out_mode (favorites.factory[15][24])   /* (JIANT 0.6.72) MENU > MIDI OUT (seq.c MO_*): 0 KEYS (older settings,
                                                * as ever), 1 CLOCK, 2 ALL, 3 OFF; a byte no engine uses */
#define PREF_SIX (0x80u << 8)                  /* (JIANT 0.6.18) MENU > TRACKS 6: all six tracks play; clear (the
                                                * default): 4, T5 and T6 asleep (voice.c trk_live) */
enum { RP_CLICK, RP_LEVEL, RP_COUNTIN };
static uint32_t rp_get(uint32_t f)                     /* a field as the MENU steps it: 0..2 */
{
    uint32_t v = (ui_rec_prefs >> (2u * f)) & 3u;
    v = v == 3u ? 0u : v;
    return f == RP_LEVEL ? (v == 0u ? 1u : v == 1u ? 0u : 2u) : v;   /* (LEVEL: LOW MID HIGH, MID stored as 0) */
}
static void rp_apply(void)                             /* the settings into click.c / seq.c (every frame: ui_input) */
{
    click_mode = (uint8_t)rp_get(RP_CLICK);
    click_lvl = (uint8_t)rp_get(RP_LEVEL);
    cin_bars = (uint8_t)rp_get(RP_COUNTIN);
    trk_live = (ui_rec_prefs & (PREF_SIX >> 8)) ? NTRK : 4u;
    sys_load = (uint8_t)(ui_load <= LOAD_ECO ? ui_load : LOAD_FULL);   /* (0.6.30) LOAD (voice.c) */
    trks_ui = trk_live == NTRK;                         /* (GLO > SYSTEM's TRACKS and LOAD show them) */
    load_ui = (int16_t)(LOAD_ECO - sys_load);
}
static void trks_put(uint32_t v)                       /* TRACKS: 0 four, 1 six */
{
    ui_rec_prefs = (uint8_t)(v ? ui_rec_prefs | PREF_SIX >> 8 : ui_rec_prefs & ~(PREF_SIX >> 8));
    rp_apply();
}
/* (0.6.30) LOAD as MENU and GLO > SYSTEM step it, the lightest first: 0 ECO, 1 LITE, 2 FULL */
static uint32_t load_get(void) { return LOAD_ECO - (ui_load <= LOAD_ECO ? ui_load : LOAD_FULL); }
static void load_put(uint32_t v)
{
    ui_load = (uint8_t)(LOAD_ECO - (v <= LOAD_ECO ? v : LOAD_ECO));
    rp_apply();
}
static void rp_put(uint32_t f, uint32_t v)
{
    uint32_t st = f == RP_LEVEL ? (v == 1u ? 0u : v == 0u ? 1u : 2u) : v % 3u;
    ui_rec_prefs = (uint8_t)((ui_rec_prefs & ~(3u << (2u * f))) | st << (2u * f));
    rp_apply();
}
/* MENU > STYLE (ui_menu.c): ST_FLAT ST_LINE (gfx.c) in another byte no engine uses, saved with the settings;
 * 0 in older ones = FLAT; 2, the retired PIXEL (1.0.1), reads as LINE; anything else unknown as FLAT (settings_persist.c). gfx.c draws from its copy, ux.style
 * (ui_draw.c style_apply) */
#define ui_style (favorites.factory[15][29])
static void draw_rules(uint32_t y, uint32_t h);       /* (ui_draw.c) */

static uint8_t sync_reload;                  /* engine / preset / project / user preset loaded: editor RELOAD push */

#define ACC T_THEME                /* values, curves */
#define VAL(c) ((c) == ui.hot_col && ui.hot_t ? T_ACCENT : T_THEME)   /* the knob just turned: accent */
#define RATIO(d, v) ((d)->max > (d)->min ? ((int32_t)(v) - (d)->min) * 1000 / ((d)->max - (d)->min) : -1)
/* layout: header 0..24, four cards 28..72 (57 px at x 3 + 59 c),
 * the panel 76..198 (a SURF area; the graphs live in it), footer 202..240; BG between them */
#define Y_HEAD 0
#define H_HEAD 24
#define Y_LABEL 28                    /* the cards */
#define Y_SEP_END 72
#define CARD_W 57
#define CARD_H 44
#define CARD_X(c) (3 + 59 * (int32_t)(c))
#define Y_GRAPH 76                    /* the panel */
#define H_GRAPH 122
#define Y_FOOT 202
#define H_FOOT 38
/* MENU > LARGE (large_kind below): on the value pages the cards are tall (28..132) and the panel a strip
 * (136..198); elsewhere (lists, rolls, layers) the layout above, its card labels in M */
#define LG_CARD_H 104
#define LG_Y_GRAPH 136
#define LG_H_GRAPH 62

static struct {
    uint8_t home;
    uint8_t page;                /* index into PAGES */
    uint8_t fam_last[FAM_COUNT]; /* last page used per family */
    uint8_t bank;                /* SEQ: 16-step bank (follows the cursor) */
    uint8_t cursor;              /* SEQ: step being edited (STEP page KNOB 1 moves it) */
    uint8_t entry_open;          /* SEQ: keys held since the first press of this entry */
    uint8_t pr_focus;            /* (0.6.81) SEQ > NOTE: the chord (0) or its note 1..4 (ui_input.c note_edit) */
    uint8_t ent_len, ent_arp, ent_res, ent_ord, ent_oct, ent_nn;   /* (0.6.37) that entry while held (ui_input.c
                                  * seq_entry): KNOB 1 its LEN in steps; ARP pressed: arpeggiated, KNOB 1 the steps a
                                  * note (RES), 2 the ORDER, 3 the octaves; the keys' notes in order of pitch */
    uint8_t ent_notes[8];
    uint8_t comp_sel;            /* (0.6.39) SEQ > COMP: the TYPE shown (COMP_*); comp_at: the step + 1 (+ the track x 64) */
    uint16_t comp_at;
    uint8_t lane;                /* SEQ > STEP on a DRUM track (the grid): the lane the keys and KNOB 3 / 4 edit */
    uint8_t plk_src;             /* parameter locks (ui_input.c lock_*): the sound page whose KNOB 1..4 a step held
                                  * locks, PAGES index + 1; 0 = HOME's four */
    uint8_t plk_lane, plk_trk;   /* .. the grid: the lane and the track of the hits plk_rm takes away */
    uint64_t plk_rm, plk_acc;    /* .. the grid's step keys pressed on a hit (bit = step): it goes when the key is let
                                  * go, unless the key was held for a lock (its accent: plk_acc) */
    uint8_t hot_col, hot_t;      /* column whose knob was just turned (drawn white) */
    uint8_t menu;                /* 0 off, 1 list, 2 about + credits (HOME held) */
    uint8_t menu_sel;            /* MENU: the row (menu_items.c MI_*; its tab MI_TAB), kept while the device runs */
    uint8_t menu_row[5];         /* MENU: the row last picked in each tab, from its first (ALGORITHM comes back to it) */
    uint16_t menu_scroll;        /* continuous ABOUT + CREDITS position, pixels */
    uint32_t menu_sig, home_t0;  /* HOME press time (btn_hold) */
    uint8_t force;               /* full redraw pending */
    uint8_t msg_t;               /* transient message frames */
    uint8_t bpm_t;               /* frames the BPM stays highlighted after a SELECT turn */
    uint8_t act;                 /* action pages: the column whose action OCT+ does, + 1; 0 = none (act_col) */
    uint32_t rec_t0;             /* REC press time: a tap arms (btn_hold, no hold: held, REC opens its layer) */
    uint32_t seq_t0;             /* SEQ held: direct SONG entry */
    uint32_t save_t0;            /* SAVE press time (btn_hold: held = UNDO) */
    uint8_t confirm;             /* the OCT- / OCT+ dialog: CF_*, 0 = none */
    uint8_t confirm_trk;         /* the track it clears, the slot it overwrites */
    uint8_t uslot;               /* SAVE > USER: the selected user preset slot */
    uint8_t ppick;               /* SEQ > PATTERNS: the pattern picked (pat_count list index) */
    uint8_t song_row;            /* SONG: row selected, count selects the next empty row */
    uint8_t ev_row;              /* SEQ > AUTO LIST (ui_input.c ev_*): the row selected (the track's count: + ADD LOCK) */
    uint8_t ev_step, ev_id;      /* .. + ADD LOCK: the step and the parameter a lock is added on */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
    uint32_t ly_t0;              /* the layer button's press time | 1, LY_* bits (ui_layer.c layer_gesture) */
    uint8_t ly;                  /* the layer whose button is down (LAYER_*), 0 = none */
    uint8_t layer;               /* the layer whose map is shown (LAYER_*), 0 = none */
    uint16_t pg_down;            /* page buttons down (panel ids) that act when let go */
    uint32_t layer_sig;          /* drawn-state cache of the map */
    char msg[24];
    char msg2[24];               /* a second message, shown when the first is over */
    uint32_t enc_t[NE];
    /* drawn-state cache */
    char col[4][56];   /* (0.6.10: was 32, a key up to 51 lost its colour / gauge / icon tail) */
    uint32_t graph_sig, head_sig, foot_sig, frame;
    /* rolling digits (ui_draw.c roll_*): the four card values and the header BPM */
    struct {
        char from[7];            /* the value rolling out; "" = idle */
        uint8_t t0;              /* (ui.frame) of the value's last change */
        int8_t dir;              /* +1: it went up (the old digit leaves upward), -1: down */
        uint8_t sig;             /* a card: what its value is of (label, unit, track, engine, palette) */
    } roll[5];
    int16_t roll_bpm;            /* the BPM last drawn */
} ui;

enum { CF_NONE, CF_CLEAR_SEQ, CF_CLEAR_TRK, CF_OVR_PROJ, CF_OVR_USER, CF_LOAD_PAT,
       CF_DEL_ROW, CF_CLEAR_SONG, CF_INIT_SOUND, CF_CLEAR_MOTION, CF_ERASE_USER,
       CF_RECALL_SOUND, CF_CLEAR_ALL, CF_DICE_ALL, CF_INIT_SYS, CF_DICE_SOUND, CF_DICE_SEQ, CF_DICE_LFO,
       CF_DICE_FX, CF_CPU_TEST };   /* ui.confirm: TOOLS' clears; (0.6.21) HOME + SELECT:
                                   * everything diced; (0.6.65) EDIT + SELECT: keep the diced sound?; (0.6.67) SEQ / LFO / FX + SELECT:
                                   * keep the diced sequence / LFO / effects?;
                                   * SAVE over a used slot; a pattern over the user's steps;
                                   * USER ERASE */

/* MENU > DISPLAY > SCREEN OFF (1.1.5): after a while with no input on the panel the screen goes dark (the backlight
 * stays on: lcd.c lcd_power)
 * (a picture held for hours can stay faintly on the panel); the sound, the sequencer, MIDI and USB go on, the LEDs
 * stay as they are. NEVER (the default since 1.1.5.1), 5, 15, 30, 60 minutes, kept in a byte no engine uses, saved
 * with the settings (SCR_CODE). 1.1.5 stored value ^ 3, 0 (its default and every older setting) being 30 MIN; 1.1.5.1
 * reads that 0 as NEVER (the new default: the panel went dark for good on 1.1.5, see lcd.c lcd_power) and keeps the
 * other codes, 30 MIN now 4. Unknown codes read as NEVER.
 * Input is the panel's only: a button or key held or pressed, a knob turned (not MIDI, the editor or MASTER, an
 * analogue pot). The press, key or turn that wakes it is swallowed: no note (seq.c kb_asleep), no button, no
 * value, until everything is let go and the knobs rest for SCR_EAT_MS. A dialog, the count-in and the UPDATE MODE
 * countdown keep it on (and wake it); the crash screen, UPDATE and UBOOT wake it at once (lcd_wake_now).
 * Off: lcd_power(0) (DISPOFF, SLPIN) and nothing is drawn. Waking, a frame at a time (never a wait in
 * the loop): 120 ms after the SLPIN, SLPOUT (lcd_power(1)); 120 ms after that one frame redraws everything
 * (ui.force), then DISPON (lcd_power(2)) */
#define ui_scr (favorites.factory[15][27])
#define SCR_DEF 0u                                     /* NEVER (1.1.5.1; 1.1.5: 30 MIN) */
static const uint8_t SCR_CODE[5] = {0, 2, 1, 4, 7};    /* the stored byte of NEVER 5 15 30 60 MIN (1.1.5: 3 2 1 0 7) */
static const uint8_t SCR_DEC[8] = {0, 2, 1, 0, 3, 0, 0, 4};   /* .. and back (3: 1.1.5's NEVER; 5, 6 unknown) */
#define SCR_EAT_MS 300u
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};  /* NEVER, minutes */
enum { SCR_ON, SCR_OFF, SCR_WAKE, SCR_SLPOUT, SCR_SHOW };
static struct {
    uint8_t st;                  /* SCR_*: SCR_WAKE asked, SCR_SLPOUT sent, SCR_SHOW the frame that redraws */
    uint8_t eat;                 /* the wake gesture: panel input swallowed (ui_input) */
    uint32_t idle;               /* fm1_ms of the last panel input (or blocker) */
    uint32_t t;                  /* fm1_ms of the last panel command (SLPIN, SLPOUT) */
    uint32_t eat_t;              /* fm1_ms of the last swallowed knob detent */
} scrn;
static uint32_t scr_get(void)                          /* the MENU's value: 0 NEVER .. 4 60 MIN */
{
    return ui_scr < NELEM(SCR_DEC) ? SCR_DEC[ui_scr] : SCR_DEF;
}
static void scr_put(uint32_t v)                        /* (the time counts from the change: the editor's too) */
{
    ui_scr = SCR_CODE[v < NELEM(SCR_MIN) ? v : SCR_DEF];
    scrn.idle = fm1_ms;
}
static void scr_wake(void)
{
    if (scrn.st == SCR_OFF)
        scrn.st = SCR_WAKE;
    scrn.eat = 1;
}
static void scr_wake_now(void)                         /* main.c: the crash screen, UPDATE, UBOOT */
{
    if (scrn.st != SCR_ON)
        lcd_wake_now();
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}
/* ui_input, every pass: 1 = the pass is swallowed (the screen dark or waking, or the wake gesture going on) */
static int scr_input(uint32_t pressed, uint32_t notes)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes, k;
    int32_t moved = 0;
    if (pressed | notes | held | panel_moved)
        scrn.idle = fm1_ms;
    panel_moved = 0;
    if (scrn.st == SCR_ON && !scrn.eat)
        return 0;
    for (k = 0; k < NE; k++)                           /* (every knob's turns go) */
        moved |= panel_enc(k);
    panel_moved = 0;
    if (moved)
        scrn.eat_t = scrn.idle = fm1_ms;
    if (pressed | notes | held | (uint32_t)moved)
        scr_wake();
    if (scrn.st != SCR_ON || held || (int32_t)(fm1_ms - scrn.eat_t) < (int32_t)SCR_EAT_MS)
        return 1;
    scrn.eat = 0;                                       /* all let go, the knobs at rest: input again */
    kb_asleep = 0;
    return 0;
}
/* ui_draw, every frame: 1 = draw it */
static int scr_frame(void)
{
    uint32_t lim = SCR_MIN[scr_get()] * 60000u;
    if (ui.confirm || ui.uboot || seq_counting()) {    /* (never dark under these) */
        scrn.idle = fm1_ms;
        if (scrn.st == SCR_OFF)
            scr_wake();
    }
    switch (scrn.st) {
    case SCR_ON:
        if (!lim || (int32_t)(fm1_ms - scrn.idle) < (int32_t)lim)   /* (signed: robust to a wrap) */
            return 1;
        lcd_power(0);
        scrn.st = SCR_OFF;
        scrn.t = fm1_ms;
        scrn.eat = 1;
        kb_asleep = 1;
        break;
    case SCR_WAKE:
        if (fm1_ms - scrn.t >= 120u) {
            lcd_power(1);
            scrn.st = SCR_SLPOUT;
            scrn.t = fm1_ms;
        }
        break;
    case SCR_SLPOUT:
        if (fm1_ms - scrn.t >= 120u) {
            scrn.st = SCR_SHOW;
            ui.force = 1;
            return 1;
        }
        break;
    case SCR_SHOW:
        return 1;
    }
    ui.msg_t = 0;                                      /* (dark: what would have shown is over; a clean page back) */
    ui.msg2[0] = 0;
    ui.hot_t = ui.bpm_t = 0;
    return 0;
}
static void scr_shown(void)                            /* after the frame: the redrawn page on */
{
    if (scrn.st != SCR_SHOW)
        return;
    lcd_power(2);
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}

static const page_t *page_over;   /* a quick layer's own four knobs (ui_layer.c), while it edits or draws them */
static const page_t *cur_page(void) { return page_over ? page_over : &PAGES[ui.page]; }

/* MENU > LARGE (#15, Discussion #80: what KNOB 1..4 do, in bigger type). Per page type:
 *   LK_TALL  HOME and the value pages (EDIT ENV LFO MOD FX SLICER SCL SHIFT ARP VOICE PATTERN STEP's knobs, MIXER,
 *            GLOBAL, SYSTEM, TOOLS, MOTION): tall cards (a K1..K4 keycap, the icon, the label in M, the value in L,
 *            in M or S when L is too wide or lacks a glyph, the unit under it, the gauge), the panel a strip: HOME's
 *            scope, ENV's ADSR, LFO's wave, PATTERN's 64 steps, MIXER's four tracks (name, state, meter) small; the
 *            page's title and number in L on the others (their charts cannot be read that small);
 *   LK_LABEL the list and graph pages (PRESETS, USER, PROJECT, PATTERNS, SONG, the piano roll and the drum grid,
 *            CHANCE) and the quick layers' maps: the layout as it is, the card labels in M;
 *   LK_OFF   LARGE off; the menu (its own LARGE: ui_menu.c), the dialogs and NAME keep their own layout in every case. */
enum { LK_OFF, LK_LABEL, LK_TALL };
static uint32_t large_kind(void)
{
    uint32_t g;
    if (!(ui_prefs & PREF_LARGE))
        return LK_OFF;
    if (ui.layer)
        return LK_LABEL;
    if (ui.home)
        return LK_TALL;
    g = cur_page()->graph;
    return g == GR_BROWSE || g == GR_SLOTS || g == GR_USER || g == GR_PATS || g == GR_SONG || g == GR_ROLL ||
           g == GR_CHANCE || g == GR_COMP || g == GR_EVENTS ? LK_LABEL : LK_TALL;
}
/* the geometry of the page shown: the cards' height, the panel's top and height */
static uint32_t card_h(void) { return large_kind() == LK_TALL ? LG_CARD_H : CARD_H; }
static uint32_t graph_y(void) { return large_kind() == LK_TALL ? LG_Y_GRAPH : Y_GRAPH; }
static uint32_t graph_h(void) { return large_kind() == LK_TALL ? LG_H_GRAPH : H_GRAPH; }
static int large_face_has(const char *s)                /* every glyph of s in the sparse L face */
{
    for (; *s; s++)
        if (glyph_at(&AF_L, fold(&AF_L, (uint8_t)*s)) < 0)
            return 0;
    return 1;
}
static int32_t ink_w(const aafont_t *f, const char *s)
{
    int32_t b[4];
    text_ink(f, s, b);
    return b[2] > b[0] ? b[2] - b[0] : 0;
}

/* the quick layers (ui_layer.c): a button held, the keys and KNOB 1..4 are its shortcuts, its map over the page */
enum { LAYER_NONE, LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT, LAYER_SEQ, LAYER_REC, LAYER_SAVE, LAYER_MACRO, LAYER_N };
static void draw_layer(void);
static const char *layer_head(void);
static uint32_t layer_leds(uint32_t *br);
static uint32_t layer_btn(void);

/* FM operator pages belong to DIGITAL; they never appear on other instruments (without FELUCCA_FM4: never). LANES /
 * LANES 2 a DRUM track's (its lane levels); SOUND / SOUND 2 a DRUM track's (DRUM-X: ui_dx.c) */
static int page_visible(uint32_t i)
{
    if (PAGES[i].graph == GR_SONG && PAGES[i].fam == FAM_SEQ)
        return 0;                                       /* (0.6.24) the song in one place: SAVE > ARRANGE (SEQ > SONG
                                                         * was the same page twice; kept in the table: its index) */
    if (sys_load == LOAD_ECO && ((PAGES[i].graph == GR_SLCR || PAGES[i].graph == GR_SLPIT) ||
                                 (PAGES[i].fam == FAM_FX && PAGES[i].id[0] == G_CRATE)))
        return 0;                                       /* (0.6.30) LOAD ECO: no SLICER, no chorus: their pages gone */
    if (PAGES[i].graph == GR_MORPH) {                   /* (0.6.45) MORPH: ANALOG's plain waves, PHASE, LOFI's (not BYTE FLOAT) */
        uint32_t e = TSEL->eng_req % NENGINES;
        return e == ENGI_ANALOG || ENGINES[e] == &ENG_PHASE ||   /* (0.6.81: every ANALOG WAVE) */
               (ENGINES[e] == &ENG_LOFI && TSEL->p[P_E1] < RW_BYTE);
    }
    if (PAGES[i].graph == GR_PATS || PAGES[i].graph == GR_CHANCE)   /* (0.6.37) PHRASES, a step's CHANCE: out of the */
        return !ui.home && ui.page == i;               /* SEQ pages' cycle (only where code put the screen) */
    if (PAGES[i].graph == GR_DXSND || PAGES[i].graph == GR_DXIMG)   /* (0.6.48: IMAGE) */
        return TSEL->eng_req % NENGINES == ENGI_DRUM;
    if (PAGES[i].scope == SC_TRACK && PAGES[i].id[0] >= P_LN0 && PAGES[i].id[0] <= P_LN7)
        return TSEL->eng_req % NENGINES == ENGI_DRUM;
    if (PAGES[i].graph >= GR_FMOP && PAGES[i].graph <= GR_FMENV)   /* (0.6.10) OPERATOR, OP ENV, OPERATOR 2: FM6's */
        return TSEL->eng_req % NENGINES == ENGI_FM6;
    if (PAGES[i].id[0] == P_LRTRG && PAGES[i].id[1] == 0xFFu)   /* (0.6.12) RETRIG: LOFI BYTE / FLOAT's */
        return ENGINES[TSEL->eng_req % NENGINES] == &ENG_LOFI && TSEL->p[P_E1] >= RW_BYTE;
    if (PAGES[i].scope == SC_TRACK && PAGES[i].id[0] == P_FTYPE)   /* (JIANT 0.5) FILTER: ANALOG's, or the part's */
        return (TSEL->eng_req % NENGINES == ENGI_ANALOG) == (PAGES[i].id[1] == P_E4);
    return !(PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] >= P_FM1_ATK &&
             PAGES[i].id[0] <= P_FM4_LEVEL) || (FELUCCA_FM4 && TSEL->eng_req % NENGINES == ENGI_DIGITAL);
}

static uint32_t page_first(uint32_t fam)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == fam)
            return i;
    return 0;
}

/* transient message in the top bar, right of the transport and the BPM: a + b */
static void ui_say(const char *a, const char *b)
{
    uint32_t n;
    str_cpy(ui.msg, a, sizeof ui.msg);
    n = str_len(ui.msg);
    str_cpy(ui.msg + n, b, sizeof ui.msg - n);
    ui.msg_t = 40;
    ui.msg2[0] = 0;
}

static void ui_message(const char *s) { ui_say(s, ""); }
/* a message led by an icon: its first byte (ui_draw.c draw_head draws the icon, then the words) */
#define MSG_NOFILE "\x01"                      /* the no-file icon (ICON_X_NOFILE) */

static int chain_busy(void) { return chain.running || chain.armed; }
/* (0.6.22) ARRANGE: the rows 0..count-1, + ADD (while there is room), then the song's END row */
static uint32_t song_end_row(void) { return chain_config.count < CHAIN_ROWS ? chain_config.count + 1u : CHAIN_ROWS; }
/* Main loop only, with interrupts enabled. PLAY may be consumed between reads;
 * take one coherent snapshot before a flash operation or song preparation. */
static int transport_busy(void)
{
    int busy;
    fm1_irq_off();
    busy = song.playing || chain_busy() || transport_req == 1u || seq_counting();   /* (a count-in: as playing) */
    fm1_irq_on();
    return busy;
}
static void chain_play_ui(void)
{
    uint32_t rc = chain_prepare();
    if (!rc) {
        ui.force = 1;
    } else if (rc >= 3u) {
        char b[8] = "A EMPTY";
        b[0] = (char)('A' + rc - 3u);
        ui_say("PATTERN ", b);
    } else {
        ui_message(rc == 1u ? "ADD A SONG ROW" : "STOP FIRST");
    }
}


/* parameter locks: a page whose KNOB 1..4 can lock (a track's sound: one of its parameters can be motion; not MOD's
 * slots), the parameter KNOB k locks (ui.plk_src's page, HOME's four), 0xFF = none */
static int lock_page_ok(uint32_t i)
{
    uint32_t k;
    if (i >= NPAGES || (PAGES[i].scope != SC_TRACK && PAGES[i].scope != SC_ENGINE) || PAGES[i].graph == GR_MOD ||
        PAGES[i].graph == GR_MSEQ)
        return 0;
    for (k = 0; k < 4u; k++)
        if (PAGES[i].id[k] != 0xFFu && motion_param(PAGES[i].id[k]))
            return 1;
    return 0;
}
static uint32_t lock_id(uint32_t k)
{
    uint32_t id = ui.plk_src && lock_page_ok(ui.plk_src - 1u) ? PAGES[ui.plk_src - 1u].id[k & 3u]
                                                               : ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u];
    return id < P_COUNT && motion_param(id) ? id : 0xFFu;
}

static void ev_enter(void);
static void page_entered(void)
{
    const page_t *pg = cur_page();
    if (!ui.home && pg->graph == GR_EVENTS)      /* AUTO LIST: + ADD LOCK on the SEQ cursor's step */
        ev_enter();
    if (!ui.home && lock_page_ok(ui.page))
        ui.plk_src = (uint8_t)(ui.page + 1u);
    song.seq_mode = !ui.home && pg->fam == FAM_SEQ;
    ui.entry_open = 0;
    ui.hot_t = 0;                                /* clear the previous page's emphasis */
    ui.act = pg->graph == GR_USER ? 4u : 0u;     /* the save screen is ready for OCT+ */
    ui.force = 1;
}

static int step_on(const step_t *st) { return st->time == ST_NOTE && (st->n || st->hit); }

enum { COMP_OFF, COMP_CHANCE, COMP_RATCH, COMP_ARPN, COMP_ARPQ, COMP_ARPS, COMP_TRIG, COMP_RAND, COMP_OCT, COMP_TONAL,
       COMP_BEND, COMP_ECHO, COMP_PULSE, COMP_JUMP, COMP_REV, COMP_FILL, COMP_KILL, COMP_N };   /* (CP_* in order) */
static const char *const COMP_NAMES[COMP_N] = {"OFF", "CHANCE", "RATCH", "NOTES", "SEQ", "SCALE", "TRIG", "RAND", "OCT",
                                               "TONAL", "BEND", "ECHO", "PULSE", "JUMP", "REV", "FILL", "KILL"};
/* (0.6.82) the COMP pages one after another: COMP (the step's first component), COMP 2 (its second), WHEN (theirs, and
 * the step's LEN): which this is (0..2) */
static uint32_t comp_slot(void)
{
    uint32_t i = ui.page, k = 0;
    while (i > 0u && k < 2u && PAGES[i - 1u].graph == GR_COMP)
        i--, k++;
    return k;
}
static uint32_t comp_type(const step_t *s) { return comp_slot() == 1u ? s->comp2 : s->comp; }   /* (the page's) */
static uint32_t comp_arg(const step_t *s) { return comp_slot() == 1u ? s->carg2 : s->carg; }
static uint32_t comp_view1(void);
static uint32_t comp_view(void)                         /* the TYPE shown for the cursor step: COMP 2's its second */
{
    const step_t *s = &TSEL->step[ui.cursor];
    if (comp_slot() == 1u)
        return s->comp2 ? COMP_ARPN + s->comp2 - CP_ARPN : COMP_OFF;
    return comp_view1();
}
static uint32_t comp_view1(void)                        /* the first's, CHANCE and RATCH (read once a step) */
{
    const step_t *s = &TSEL->step[ui.cursor];
    uint32_t at = (uint32_t)ui.cursor + 1u + (uint32_t)song.sel * 64u;
    if (ui.comp_at != at || ui.comp_sel >= COMP_N) {
        ui.comp_at = (uint16_t)at;
        ui.comp_sel = (uint8_t)(s->comp ? COMP_ARPN + s->comp - CP_ARPN : step_chance(s) < 100u ? COMP_CHANCE :
                                step_ratchet(s) > 1u ? COMP_RATCH : COMP_OFF);
    }
    if (s->comp && ui.comp_sel != COMP_ARPN + s->comp - CP_ARPN)   /* (changed elsewhere: the editor, a dice) */
        ui.comp_sel = (uint8_t)(COMP_ARPN + s->comp - CP_ARPN);
    return ui.comp_sel;
}
static void step_clear(step_t *st)
{
    st->n = 0;
    st->time = ST_REST;
    st->flags = 0;
    st->vel = 0;
    st->hit = st->acc = 0;
    st->comp = st->carg = 0;
    st->comp2 = st->carg2 = st->cwhen = st->ctime = 0;
    st->probability = 0;
}

/* ------------------------------------------------------- the DRUM grid --- */
/* SEQ > STEP on a DRUM track is the grid: 8 lanes x the 16 steps of a page. The white keys are the
 * steps of the page shown (a tap toggles the selected lane there), black keys 1..8 select the lane (and play
 * it), black key 9 held is ACC (white keys toggle accents, their LEDs show them), black keys 10 / 11 the page
 * down / up. KNOB 1 STEP, 2 LANE, 3 HIT, 4 ACC edit the cursor step. A sound load never converts the
 * steps: the grid shows a step's notes on their lanes (eng_drum.c step_lanes) and an edit makes the lane its
 * own (grid_own). Live recording on a DRUM track writes hits (seq.c rec_note) */
static int grid_on(void) { return !ui.home && !ui.menu && !ui.confirm && cur_page()->graph == GR_ROLL && drum_track(TSEL); }   /* (STEP only: CHANCE is SC_STEP too) */

/* black key place p (seq.c key_place) held, 0 = not */
static int black_held(uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) && key_place(k) == p)
            return (int)(((fm1_in.notes & ~kb_layer) >> k) & 1u);
    return 0;
}

/* lane l of step s from now on is its hit alone: the step's lane notes become hits (nothing sounds different),
 * a note of another pitch on the lane (a low tom 41) becomes the lane's own */
static void grid_own(step_t *s, uint32_t l)
{
    uint32_t k, j = 0;
    step_to_grid(s);
    for (k = 0; k < s->n; k++)
        if (drum_lane(s->note[k]) == l)
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[j++] = s->note[k];
    for (k = j; k < 4u; k++)
        s->note[k] = 0;
    s->n = (uint8_t)j;
}

/* lane l of step i: on 1, off 0, toggled 2 */
static void grid_hit(track_t *t, uint32_t i, uint32_t l, uint32_t on)
{
    step_t *s = &t->step[i % NSTEP];
    uint32_t b = 1u << (l % NLANE);
    if (on == 2u)
        on = !(step_lanes(s) & b);
    if (s->time != ST_NOTE) {                    /* a REST or a TIE: an empty step (nothing to turn off) */
        if (!on)
            return;
        step_clear(s);
        s->time = ST_NOTE;
    }
    grid_own(s, l % NLANE);
    if (on) {
        s->hit |= (uint8_t)b;
    } else {
        s->hit &= (uint8_t)~b;
        s->acc &= (uint8_t)~b;
        if (!s->n && !s->hit)
            step_clear(s);
    }
}

/* the accent of lane l at step i (on 1, off 0, toggled 2); an accent on an empty lane adds the hit */
static void grid_acc(track_t *t, uint32_t i, uint32_t l, uint32_t on)
{
    step_t *s = &t->step[i % NSTEP];
    uint32_t b = 1u << (l % NLANE);
    if (on == 2u)
        on = !(step_accents(s) & b);
    if (on)
        grid_hit(t, i, l, 1);
    if (!(step_lanes(s) & b))
        return;
    grid_own(s, l % NLANE);
    if (s->flags & SF_ACCENT) {                  /* a step accent: each hit's own from now on */
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
    s->acc = (uint8_t)(on ? s->acc | b : s->acc & ~b);
}

/* SEQ cursor: wraps inside the pattern length, the bank follows, a step entry ends */
static void cursor_set(int32_t c)
{
    int32_t len = TSEL->p[P_SLEN] > 0 ? TSEL->p[P_SLEN] : 1;
    ui.cursor = (uint8_t)((c % len + len) % len);
    ui.bank = (uint8_t)(ui.cursor / 16u);
    ui.entry_open = 0;
    ui.pr_focus = 0;                                    /* (0.6.81: another step, the whole chord) */
}

static void cursor_fix(void)                           /* LEN got shorter: onto the last step */
{
    if (ui.cursor >= (uint32_t)TSEL->p[P_SLEN])
        cursor_set(TSEL->p[P_SLEN] - 1);
}

static void note_name(char *b, uint32_t n)
{
    str_cpy(b, N_NOTE[n % 12u], 4);
    fmt_int(b + str_len(b), (int32_t)(n / 12u) - 1);
}

/* (0.6.24) FX's pages in sections: TRACK (FX, DIST, AUTOPAN), SLICER (SLICER, SL BUS, SL BUS 2, SL PITCH), DELAY (DLY,
 * DLY 2), REVERB (REVERB, REVERB 2), CHORUS, MASTER. A tap the next page, as before; two taps quickly the next section's
 * first page; the footer counts the page within its section ("DLY 2/2") */
static int fx_head(uint32_t i)
{
    const char *t = PAGES[i].title;
    return PAGES[i].fam == FAM_FX && (str_eq(t, "FX") || str_eq(t, "SLICER") || str_eq(t, "DLY") ||
                                      str_eq(t, "REVERB") || str_eq(t, "CHORUS") || str_eq(t, "MASTER"));
}
/* (JIANT 0.6.51) a page (or the track) changed by hand: its name in the header (0.6.61), (0.6.64) the footer, for PF_MS, so the
 * pages need no memorising (ui_graph.c page_flash_draw); a page set by code (a test, a dialog's way back) shows none */
#ifndef PF_MS                                        /* (tests/ui_test.c: 0, the cards always shown) */
#define PF_MS 900u                                   /* (0.6.61: in the header, so it can stay a little longer) */
#endif
static uint8_t fg_ui;                                /* (0.6.52) SEQ > FUGUE: the head edited (0..2: heads 2..4) */
static uint32_t pf_ms;
static uint8_t pf_on;
static void page_flash(void)
{
    pf_ms = fm1_ms;
    pf_on = 1;
}
static void open_family(uint32_t fam)
{
    static uint32_t fx_ms, fx_from = 0xFFu;
    if (fam == FAM_FX && !ui.home && cur_page()->fam == FAM_FX && fx_from < NPAGES && fm1_ms - fx_ms < 350u) {
        uint32_t n, i = fx_from;                        /* the second of two quick taps: the section after the one the
                                                         * first tap left */
        for (n = 0; n < NPAGES; n++) {
            i = (i + 1u) % NPAGES;
            if (fx_head(i) && page_visible(i)) break;
        }
        fx_from = 0xFFu;
        ui.page = (uint8_t)i;
        ui.fam_last[fam] = ui.page;
        page_entered();
        page_flash();
        return;
    }
    if (fam == FAM_FX) {
        fx_from = !ui.home && cur_page()->fam == FAM_FX ? ui.page : 0xFFu;
        fx_ms = fm1_ms;
    }
    if (!ui.home && cur_page()->fam == fam) {          /* same button again: next page */
        uint32_t n, i = ui.page;
        for (n = 0; n < NPAGES; n++) {
            i = (i + 1u) % NPAGES;
            if (PAGES[i].fam == fam && page_visible(i)) break;
        }
        ui.page = (uint8_t)i;
    } else if (fam == FAM_SAVE) {
        uint32_t i;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].graph == GR_USER) break;
        ui.page = (uint8_t)i;                         /* SAVE enters the sound save screen directly */
    } else {
        ui.page = ui.fam_last[fam] && PAGES[ui.fam_last[fam]].fam == fam && page_visible(ui.fam_last[fam]) ? ui.fam_last[fam]
                                                                          : (uint8_t)page_first(fam);
    }
    ui.fam_last[fam] = ui.page;
    ui.home = 0;
    page_entered();
    page_flash();
}

/* GLO always enters the mixer from another family. Subsequent taps visit the
 * global settings, then return to the mixer; recording has no navigation role. */
static void open_global(void)
{
    uint32_t i;
    if (!ui.home && cur_page()->fam == FAM_TRK) {
        ui.page = (uint8_t)page_first(FAM_GLO);
    } else if (!ui.home && cur_page()->fam == FAM_GLO) {
        for (i = ui.page + 1u; i < NPAGES && PAGES[i].fam != FAM_GLO; i++) {}
        ui.page = (uint8_t)(i < NPAGES ? i : page_first(FAM_TRK));
    } else {
        ui.page = (uint8_t)page_first(FAM_TRK);
    }
    ui.home = 0;
    page_entered();
    page_flash();
}

static void go_home(void)
{
    ui.home = 1;
    ui.plk_src = 0;                              /* (parameter locks: HOME's four) */
    ui.entry_open = 0;
    ui.hot_t = 0;
    song.seq_mode = 0;
    ui.force = 1;
}

/* ------------------------------------------------------- track setup --- */
static int seq_is_empty(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (t->step[i].n || t->step[i].hit)
            return 0;
    return 1;
}

/* Loads. A sound load (a factory or user preset, an engine jump, TOOLS INIT, the editor's PRESET / G_ENGSEL /
 * UP_LOAD) changes the sound only; a pattern load (SEQ > PATTERNS, a clear) changes the steps and the pattern
 * parameters only. A sound load keeps the track's motion on the parameters every engine has (load_end, motion.c
 * motion_sound_loaded); a pattern load drops it all (the steps it was on are gone).
 * TONIC: no undo. Felucca's one-step undo (SAVE held) and the layers' OCT- put back are gone; the song layer's
 * RECALL (docs/TONIC-SONG-PLAN.md) brings a section back as it was stored instead. */
enum { LOAD_SOUND = 1, LOAD_PAT = 2 };
static uint8_t load_depth;       /* loads nest (an engine jump loads its first preset): the outer one counts */
static uint32_t pat_sig[NTRK];   /* steps_sig of the pattern the last pattern load put into each track: such
                                  * steps, untouched, are replaced by the next pattern without asking */
static uint8_t pat_last[NTRK];   /* that pattern's list index + 1, 0 = none */
static uint8_t load_from;        /* a sound load (the outer one): the engine before it (load_end) */
static uint8_t load_mo;          /* .. the track's motion paused while the sound changes: bit 0 set, bit 1 it was ON */

static uint32_t fnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
static uint32_t steps_sig(const track_t *t) { return fnv(2166136261u, t->step, sizeof t->step); }

/* load_begin: a pattern load drops the track's motion (its steps go); a sound load pauses it (PLAY OFF while the
 * sound changes under it) and load_end keeps what still means the same (motion_sound_loaded) */
static void load_motion_hold(track_t *t, uint32_t what)
{
    if (what & LOAD_PAT) {
        motion_reset(t);
        load_mo = 0;
        return;
    }
    load_from = t->eng_req;
    load_mo = (uint8_t)(1u | (motion_enabled(t) ? 2u : 0u));
    motion_set_enabled(t, 0);
}

static void load_begin(track_t *t, uint32_t what)
{
    if (load_depth++)
        return;
    if (what & LOAD_SOUND) {                      /* (0.6.13) the sound as it is: the fade of what sounds renders
                                                   * with it (voice.c xf_snap), unless one is fading on it now */
        uint32_t k = trk_index(t);
        if (xf_snap_t == 0xFFu || !trk[xf_snap_t % NTRK].xf_on) {
            memcpy(xf_snap, t->p, sizeof xf_snap);
            xf_snap_t = (uint8_t)k;
        }
    }
    motion_restore(t);
    load_motion_hold(t, what);
}

static void load_end(track_t *t)
{
    if (--load_depth)
        return;
    if (load_mo & 1u) {                           /* a sound load: the motion on the common parameters stays */
        (void)motion_sound_loaded(t, load_from, t->eng_req);
        if ((load_mo & 2u) && motion_count(t))
            motion_set_enabled(t, 1);
        load_mo = 0;
    }
    motion_rebase(t);
}

static int param_kept(uint32_t i);

/* a 16-step pattern (PATTERNS[] format, user presets too) into steps 1..16, the rest empty, LEN 16 */
static void load_pat16(track_t *t, const uint8_t *note, const uint8_t *flags)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        uint8_t n = i < 16u ? note[i] : 0, fl = i < 16u ? flags[i] : 0;
        s->note[0] = n;
        s->n = n ? 1 : 0;
        s->time = (fl & 4u) ? ST_TIE : n ? ST_NOTE : ST_REST;
        s->flags = n ? (fl & (SF_ACCENT | SF_SLIDE | SF_RATCH)) : 0;
        s->vel = n ? 96 : 0;
        s->hit = s->acc = 0;
        s->probability = 0;
        if (drum_track(t))                           /* a DRUM track: the lanes' notes as its grid */
            step_to_grid(s);
    }
    t->p[P_SLEN] = 16;
}

/* a 16-step drum grid (user presets of version 3: lane hits, their accents) into steps 1..16, the rest empty,
 * LEN 16 */
static void load_grid16(track_t *t, const uint8_t *hit, const uint8_t *acc)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        if (i < 16u && hit[i]) {
            s->time = ST_NOTE;
            s->hit = hit[i];
            s->acc = acc[i] & hit[i];
        }
    }
    t->p[P_SLEN] = 16;
}

static void track_defaults_steps(track_t *t)
{
    uint32_t i;
    latch_drop_req |= (uint8_t)(1u << trk_index(t));    /* (0.6.28) its held / latched / repeated notes end too */
    if (!chain_busy() && (live.req >= 0 || chain.armed_bar)) {   /* (a section jump or quick chain waiting for the bar */
        live.req = -1;                                  /* would bring the steps back: called off) */
        chain.armed_bar = 0;
    }
    motion_reset(t);
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
}

/* SEQ > PATTERNS: the factory patterns (PATTERNS[], "01".."13"), then the used user presets that hold
 * one ("U07"): list index n. Loading one replaces the track's steps 1..16 (the rest cleared) and LEN;
 * a user preset's pattern brings its stored LEN (at most 16), DIV, SWING and GATE too. The notes are
 * loaded as they are: the patterns are written for the register of their kind of sound, DRUM and
 * SLICE patterns are drum and slice numbers, and SCL TRANS / OCT transpose what plays */
static uint32_t pat_count(void) { return NPATTERNS + up_pat_count(); }

static void pat_label(uint32_t n, char *tag, char *name)   /* tag: 4 bytes ("01", "U07"), name: 13 */
{
    if (n < NPATTERNS) {
        tag[0] = (char)('0' + (n + 1u) / 10u);
        tag[1] = (char)('0' + (n + 1u) % 10u);
        tag[2] = 0;
        str_cpy(name, PATTERNS[n].name, 13);
    } else {
        uint32_t k = up_pat_nth(n - NPATTERNS);
        up_slot_label(tag, k);
        up_name(k, name);
    }
}

static void pat_load(track_t *t, uint32_t n)
{
    load_begin(t, LOAD_PAT);
    if (n < NPATTERNS)
        load_pat16(t, PATTERNS[n].note, PATTERNS[n].flags);
    else
        up_pat_load(t, up_pat_nth(n - NPATTERNS));
    pat_sig[trk_index(t)] = steps_sig(t);
    pat_last[trk_index(t)] = (uint8_t)(n + 1u);
    load_end(t);
    ui.force = 1;
}

/* a pattern load would throw away steps of the user's (recorded, edited, from a project): ask first */
static int pat_needs_confirm(const track_t *t)
{
    return !seq_is_empty(t) && steps_sig(t) != pat_sig[trk_index(t)];
}

static uint32_t pat_pick(void)                   /* ui.ppick inside the list (user presets may be gone) */
{
    uint32_t n = pat_count();
    return ui.ppick < n ? ui.ppick : n - 1u;
}

/* SEQ > PATTERNS LOAD: pattern n into track t, "LOADED 03 MELODY" */
static void pat_load_ui(track_t *t, uint32_t n)
{
    char tag[4], nm[13], b[20];
    pat_label(n, tag, nm);
    pat_load(t, n);
    str_cpy(b, tag, sizeof b);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), nm, sizeof b - str_len(b));
    ui_say("LOADED ", b);
}

/* the track's settings, not the sound's: what a sound load (factory or user preset, an engine jump,
 * TOOLS INIT) leaves alone. The mix (LEVEL, PAN, MUTE: the TRACKS faders), the arpeggiator (ARP, ARP 2),
 * the scale and key map (SCL), the pattern parameters (LEN, DIV, SWING, GATE) and the SLICER insert,
 * which chops whatever the track plays in time with its sequencer */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_SOFS || i == P_POFS ||
           (!FELUCCA_FM4 && (i == P_HVEL || i == P_HTIM || i == P_HGAT || i == P_HRQ));   /* (0.6.87) HUMAN: the sequencer's, a sound
                                                         * keeps it (FELUCCA_FM4: DIGITAL's OP SUS / LEVEL there) */
}

/* a retired preset kept as an alias (SAMPLE's, gone with it in JIANT): none now. -> the preset k stands for */
static uint32_t preset_orig(const engine_t *e, uint32_t k) { (void)e; return k; }
/* the presets of engine e that browsing shows before preset k (k = npresets: all of them) */
/* (0.5.1) browsing order: LOFI's BYTE presets, then FLOAT's (appended in 0.5.1: the stored numbers stay), then the
 * rest; every other engine in its own order. preset_nth: the preset shown n-th */
static const uint8_t LOFI_ORDER[14] = {0, 1, 2, 8, 9, 10, 11, 12, 13, 3, 4, 5, 6, 7};
_Static_assert(sizeof LOFI_ORDER == NELEM(LOFI_PRESETS), "LOFI's browsing order: every preset once");
static uint32_t preset_rank(const engine_t *e, uint32_t k)
{
    uint32_t i;
    if (e == &ENG_LOFI)
        for (i = 0; i < sizeof LOFI_ORDER; i++)
            if (LOFI_ORDER[i] == k)
                return i;
    return k;
}
static uint32_t preset_nth(const engine_t *e, uint32_t n)
{
    return e == &ENG_LOFI && n < sizeof LOFI_ORDER ? LOFI_ORDER[n] : n;
}
#define preset_shown(e) (ENGINES[e]->npresets)

#if !FELUCCA_FM4
/* DIGITAL (engine 1, retired): t's sound = p, values as DIGITAL has them, converted to FM6 with a patch of its own
 * (fm4_convert.c). Every path that brings a DIGITAL sound into a track ends here: a track never keeps engine 1 */
static void fm4_apply(track_t *t, int16_t *p)
{
    uint8_t v[FP_SIZE + 1u];
    uint32_t pr = fm4_convert(p, v), tr = trk_index(t), f;
    fm6_set_patch(tr, v);
    f = motion_guard();                               /* the audio ISR sees the old sound or the new one */
    memcpy(t->p, p, sizeof t->p);
    t->eng_req = ENGI_FM6;
    t->preset = (uint8_t)pr;
    motion_unguard(f);
    fm6_adopt(tr);                                    /* SLOT OWN: the converted patch is the track's own */
}
static void fm4_track(track_t *t)                     /* t holds a DIGITAL sound (engine 1): convert it */
{
    int16_t p[P_COUNT];
    memcpy(p, t->p, sizeof p);
    fm4_apply(t, p);
}
/* DIGITAL preset k (a stored preset number of engine 1: the editor's PRESET, SET G_ENGSEL, an old project's
 * power-on sound): its sound as a preset load sets it, converted. The sound only (not the steps, not param_kept) */
static void fm4_load_preset(track_t *t, uint32_t k)
{
    int16_t p[P_COUNT];
    uint32_t i;
    load_begin(t, LOAD_SOUND);
    panic_req |= (uint8_t)(1u << trk_index(t));
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    memcpy(p, t->p, sizeof p);
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            p[i] = TP[i].def;
    fm4_preset_values(p, k);
    fm4_apply(t, p);
    load_end(t);
}
#endif

static void set_engine_of(track_t *t, uint32_t ei);
/* preset pi of the engine the track asked for: the sound only (not the steps, not param_kept) */
static void apply_preset_to(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i;
#if !FELUCCA_FM4
    if (t->eng_req % NENGINES == ENGI_DIGITAL) {
        fm4_load_preset(t, pi);
        return;
    }
#endif
    if (t->eng_req % NENGINES == ENGI_SAMPLE && pi == SMP_SET_PERC) {   /* SAMPLE preset 4 was PERC (retired, a stored */
        set_engine_of(t, ENGI_DRUM);                  /* number: the editor's PRESET, a favourite): DRUM's kit */
        return;                                       /* (core.h drum_from_perc) */
    }
    if (eng_gone(t->eng_req % NENGINES)) {            /* PHYS SAMPLE GRAIN SLICE (retired, core.h): ANALOG's first */
        set_engine_of(t, ENGI_PHYS_TO);
        return;
    }
    load_begin(t, LOAD_SOUND);
    panic_req |= (uint8_t)(1u << trk_index(t));       /* MONO/POLY may change: release what sounds */
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    if (!e->npresets) {
        load_end(t);
        return;
    }
    pi = preset_orig(e, pi % e->npresets);
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)                        /* the rest of the sound to its defaults: a preset */
        if (!param_kept(i))                           /* sounds the same after any edit */
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)e->presets[pi].e[i];
    t->p[P_ATK] = e->presets[pi].env[0];
    t->p[P_DEC] = e->presets[pi].env[1];
    t->p[P_SUS] = e->presets[pi].env[2];
    t->p[P_REL] = e->presets[pi].env[3];
    t->p[P_ED_FLT] = e->presets[pi].fenv;
    t->p[P_VOICE] = e->presets[pi].mono ? V_LEGATO : V_POLY;   /* mono presets keep the legato feel */
    {   /* the sends */
        static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
        const preset_t *pr = &e->presets[pi];
        for (i = 0; i < 4u; i++)
            t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
    }
    fm6_track_loaded(t);                              /* FM6: the preset's patch (its SLOT) */
    load_end(t);
}

/* the engine's defaults and its first preset. With the audio IRQ off: the ISR sees the old engine with
 * its values or the new one with its own (voice.c engine_block), never one with the other's */
static void set_engine_of(track_t *t, uint32_t ei)
{
    const engine_t *e;
    uint32_t i;
    if (eng_gone(ei % NENGINES))                     /* PHYS SAMPLE GRAIN SLICE (retired, core.h): ANALOG */
        ei = ENGI_PHYS_TO;
    e = ENGINES[ei % NENGINES];
#if !FELUCCA_FM4
    if (ei % NENGINES == ENGI_DIGITAL) {             /* DIGITAL (retired): its first preset, as FM6 */
        fm4_load_preset(t, 0);
        return;
    }
#endif
    load_begin(t, LOAD_SOUND);
    fm1_irq_off();
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    apply_preset_to(t, 0);
    fm1_irq_on();
    load_end(t);
}

static void apply_preset(uint32_t pi) { apply_preset_to(TSEL, pi); }
static void set_engine(uint32_t ei) { set_engine_of(TSEL, ei); }
/* (0.6.46) INIT SOUND, NEW PROJECT, (0.6.49) the power-on: the engine's own defaults (its EDIT values as the engine
 * declares them, not its first preset's colour; the envelope, VOICE and the filter envelope as a new track has them),
 * dry (no DIST CHOR DLY REV send: the effects are the player's to add), on FM6 the init voice */
static void init_sound_of(track_t *t, uint32_t ei)
{
    static const uint8_t ENVP[6] = {P_ATK, P_DEC, P_SUS, P_REL, P_ED_FLT, P_VOICE};
    const engine_t *e;
    uint32_t i;
    load_begin(t, LOAD_SOUND);
    set_engine_of(t, ei);
    e = ENGINES[t->eng_req % NENGINES];
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    for (i = 0; i < 6u; i++)
        t->p[ENVP[i]] = TP[ENVP[i]].def;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = 0;
    fm6_init_voice(t);
    load_end(t);
}

static void track_defaults(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    track_defaults_steps(t);
}

/* switch engine (its defaults + first preset) and say so */
static void select_engine(uint32_t e)
{
    set_engine(e);
    ui_say("ENGINE ", ENGINES[TSEL->eng_req]->name);
    ui.force = 1;
}

/* the presets of every engine (in ENGINE_ORDER), then the used user presets, as one list (the PRESETS knob and the
 * PRESETS page browse it) */
static uint32_t preset_all_pos(uint32_t *total)          /* list index of the selected track's preset */
{
    uint32_t n = 0, cur = 0, e, r;
    for (r = 0; r < NENG_SHOWN; r++) {                  /* (engines.c ENGINE_ORDER) */
        const engine_t *en = ENGINES[e = eng_vis(r)];
        if (e == TSEL->eng_req)
            cur = n + preset_rank(en, preset_orig(en, TSEL->preset % (en->npresets ? en->npresets : 1u)));
        n += preset_shown(e);
    }
    if (user_of(TSEL) < UP_SLOTS)
        cur = n + up_rank(user_of(TSEL));
    *total = n + up_count();
    return cur;
}

/* list index n (< total) -> engine, *k its preset; NENGINES = user preset, *k its slot */
static uint32_t preset_all_at(uint32_t n, uint32_t *k)
{
    uint32_t e = 0, i, r;
    for (r = 0; r < NENG_SHOWN && n >= preset_shown(eng_vis(r)); r++)
        n -= preset_shown(eng_vis(r));
    if (r == NENG_SHOWN) {
        *k = up_nth(n);
        return NENGINES;
    }
    e = eng_vis(r);
    (void)i;
    *k = preset_nth(ENGINES[e], n);                     /* the n-th shown preset */
    return e;
}

static uint32_t preset_pos(uint32_t *total)
{
    uint32_t all, current = preset_all_pos(&all), n = 0, pos = 0xFFFFFFFFu;
    if (!favorites.filter) { *total = all; return current; }
    for (uint32_t i = 0; i < all; i++) {
        uint32_t k, e = preset_all_at(i, &k);
        if (!favorite_has(e, k)) continue;
        if (i == current) pos = n;
        n++;
    }
    *total = n;
    return pos == 0xFFFFFFFFu ? n : pos; /* current sound need not be a favorite */
}
static uint32_t preset_at(uint32_t n, uint32_t *k)
{
    uint32_t all;
    if (!favorites.filter) return preset_all_at(n, k);
    preset_all_pos(&all);
    for (uint32_t i = 0; i < all; i++) {
        uint32_t e = preset_all_at(i, k);
        if (favorite_has(e, *k) && !n--) return e;
    }
    *k = UP_SLOTS;
    return NENGINES;
}
static int preset_favorite(void)
{
    uint32_t k = user_of(TSEL);
    return favorite_has(k < UP_SLOTS ? NENGINES : TSEL->eng_req,
                                        k < UP_SLOTS ? k : TSEL->preset);
}
static void preset_mark(int on)
{
    uint32_t k = user_of(TSEL);
    if (favorite_set(k < UP_SLOTS ? NENGINES : TSEL->eng_req, k < UP_SLOTS ? k : TSEL->preset, on)) {
        ui.force = 1;
        settings_save();
    }
}

/* the pattern the selected track's sound suggests: its index in the SEQ > PATTERNS list, or -1. A factory
 * preset's PAT(n); a user preset that holds a pattern: that one ("U07") */
static int32_t preset_pat_hint(void)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t u = user_of(TSEL);
    if (u < UP_SLOTS)
        return up_has_pat(u) ? (int32_t)(NPATTERNS + up_pat_rank(u)) : -1;
    if (!e->npresets)
        return -1;
    u = e->presets[TSEL->preset % e->npresets].pat;
    return u && u <= NPATTERNS ? (int32_t)u - 1 : -1;
}

static void preset_hinted(void)                     /* after a sound load: SEQ > PATTERNS starts at the suggested pattern */
{
    int32_t h;
    if ((h = preset_pat_hint()) >= 0)
        ui.ppick = (uint8_t)h;
    ui.force = 1;
}

static void preset_go(uint32_t n)                    /* load list index n into the selected track (the sound only) */
{
    uint32_t k, e = preset_at(n, &k);
    if (e == NENGINES) {
        up_load(k);
    } else {
        if (e != TSEL->eng_req)
            select_engine(e);
        apply_preset(k);
    }
    preset_hinted();
}

/* (JIANT 0.6.72) PROGRAM CHANGE (midi_control.c midi_pc_req): the list's n-th sound into the track whose channel it
 * came on, the selection kept; out of the list's range, nothing. Every frame (ui_input) */
static void midi_pc_poll(void)
{
    uint32_t i, sel = song.sel;
    if (midi_song_req) {                               /* (0.6.80) channel 16: 0..3 a section, 16..23 a project */
        uint32_t n = midi_song_req - 1u;
        midi_song_req = 0;
        if (n < 4u)
            (void)section_jump(n);
        else if (n >= 16u && n < 16u + NSONG)
            (void)song_select(n - 16u);
    }
    for (i = 0; i < NTRK; i++)
        if (midi_pc_req[i]) {
            uint32_t n = midi_pc_req[i] - 1u, all, k, e;
            midi_pc_req[i] = 0;
            song.sel = (uint8_t)i;
            preset_all_pos(&all);
            if (n < all) {
                e = preset_all_at(n, &k);
                if (e == NENGINES) {
                    up_load(k);
                } else {
                    if (e != TSEL->eng_req)
                        set_engine(e);
                    apply_preset(k);
                }
                ui.force = 1;
            }
            song.sel = (uint8_t)sel;
        }
}

/* the EDIT layer's KNOB 2 (ui_layer.c): the selected track's engine only: its factory presets, then the used user presets
 * made with it (slot order). List index of the current sound; *total the length */
static uint32_t eng_list_pos(uint32_t *total)
{
    const engine_t *en = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t e = TSEL->eng_req % NENGINES, np = preset_shown(e), u = user_of(TSEL), k, n = 0;
    uint32_t cur = np ? preset_rank(en, preset_orig(en, TSEL->preset % en->npresets)) : 0u;   /* (#124: the aliases
                                        * skipped, as on PRESETS: SAMPLE 1 loads as 0, so counting it stuck KNOB 2) */
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k) && up_engine(k) == e) {
            if (k == u)
                cur = np + n;
            n++;
        }
    *total = np + n;
    return cur;
}

static void eng_list_step(int32_t direction)         /* the next / previous sound of the engine (wraps) */
{
    uint32_t total, cur = eng_list_pos(&total), e = TSEL->eng_req % NENGINES, np = preset_shown(e), k, n;
    if (total < 2u)
        return;
    n = (cur + (direction > 0 ? 1u : total - 1u)) % total;
    if (n < np) {
        apply_preset(preset_nth(ENGINES[e], n));        /* the n-th shown preset */
    } else {
        n -= np;
        for (k = 0; k < UP_SLOTS; k++)
            if (up_used(k) && up_engine(k) == e && !n--) {
                up_load(k);
                break;
            }
    }
    preset_hinted();
}

static void preset_step(int32_t direction)
{
    uint32_t total, cur = preset_pos(&total);
    if (!total) { ui_message("NO FAVORITES"); return; }
    preset_go(cur >= total ? (direction > 0 ? 0 : total - 1) :
              (cur + (direction > 0 ? 1u : total - 1u)) % total);
}

/* Seven display rows. Favorites use a bounded window, not a repeating carousel.
 * Return total for an empty row; a non-favorite current sound shows the start. */
static uint32_t preset_visible(uint32_t cur, uint32_t total, uint32_t row)
{
    uint32_t first, last;
    if (!total || row >= 7u) return total;
    if (!favorites.filter)
        return (cur + total * 4u + row - 3u) % total;
    first = cur < total && cur > 3u ? cur - 3u : 0u;
    last = total > 7u ? total - 7u : 0u;
    if (first > last) first = last;
    return first + row < total ? first + row : total;
}

/* (JIANT 0.4) the MACRO layer latched: LFO held opens it, let go it stays (M1..M4 on KNOB 1..4, SELECT the dice, its
 * cards drawn by ui_draw over any page, no layer open: the keys stay notes) until LFO is pressed again (ui_layer.c) */
static uint8_t mac_latch;
/* HOME: what KNOB k edits: the engine's four main parameters */
static const param_desc_t *home_param(uint32_t k, int16_t **vp)
{
    uint32_t id = ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u];
    *vp = &TSEL->p[id];
    return track_desc(TSEL, id);
}

/* select track i (KNOB 1 on TRACKS, the editor): its sound, pages and pattern from now on */
static void track_select(uint32_t i)
{
    if (i >= NTRK || i == song.sel)
        return;
    song.sel = (uint8_t)i;
    ui.entry_open = 0;
    ui.hot_t = 0;
    ui.cursor = 0;
    ui.bank = 0;
    sync_reload = 1;
    ui.force = 1;
    page_flash();                                    /* (0.6.51) the track's number and engine shown big */
}

#include "ui_events.c"                            /* SEQ > AUTO LIST: the locks and events as a list (an action page) */
/* EDIT > SOUND / SOUND 2 (ui_dx.c, included by ui_input.c): the DRUM-X sound edited and its patch (0 A, 1 B) */
static uint8_t dx_ui_lane, dx_ui_side;
static uint32_t dx_pagen(const page_t *pg)            /* SOUND 1..3; (0.5.1) X-MOD 4; (0.6.54) SOUND 4: 8 */
{
    if (pg->title[0] == 'X')
        return 4u;
    return pg->title[5] == ' ' ? (pg->title[6] == '4' ? 8u : (uint32_t)(pg->title[6] - '0')) : 1u;
}
static void dx_knob(const page_t *pg, uint32_t k, int32_t s);
static void dx_cards(const page_t *pg);
static void dx_kit3_knob(uint32_t k, int32_t s);      /* (0.6.54) KIT 3: TONE's BP, FEEDBK */
#define DX_KIT3(pg) ((pg)->graph == GR_DXIMG && (pg)->title[0] == 'K')   /* (0.6.58) GR_DXIMG: KIT 3, or STEREO (IMAGE) */
static void dx_kit3_cards(void);
static uint32_t dx_sig(void);

/* ---------------------------------------------------- action pages --- */
/* Pages whose purpose is an action (SEQ > PATTERNS, SAVE > USER, PROJECT, TOOLS): the knobs pick,
 * OCT+ does it, OCT- cancels the picked action or goes HOME (ui_input.c). There OCT- / OCT+ do not
 * shift the octave */
static int go_id(uint32_t id) { return id == G_LOAD || id == G_SAVE || id == G_CLRSEQ || id == G_INITSND; }

static uint32_t act_cols(void)                   /* the columns that are actions, a bit each; 0 = not such a page */
{
    const page_t *pg = cur_page();
    uint32_t c, m = 0;
    if (ui.home)
        return 0;
    if (pg->graph == GR_MOTION) return 8u;
    if (pg->graph == GR_SHAPE) return 16u;      /* (0.6.85) STRETCH: KNOB 1 / 2 pick (ui.act), OCT+ does it (act_do) */
    if (pg->graph == GR_EVENTS) return 16u;     /* AUTO LIST: no knob is an action; OCT+ adds / toggles (ev_oct) */
    if (pg->graph == GR_TOOLS) return 15u;
    if (pg->graph == GR_SONG)
        return 1u;                               /* PLAY / STOP (also the PLAY button) */
    if (pg->graph == GR_PATS)
        return 2u;                               /* LOAD */
    if (pg->graph == GR_USER)
        return 14u;                              /* LOAD ERASE SAVE */
    if (pg->scope == SC_GLOBAL)
        for (c = 0; c < 4u; c++)
            if (go_id(pg->id[c]))
                m |= 1u << c;
    return m;
}

/* the action OCT+ does: its column + 1, 0 = none picked yet (PATTERNS has LOAD only) */
/* (0.6.85) SEQ > STRETCH's picks (ui_input.c shape_knob, act_do) */
static uint8_t shp_i = 7u, shp_r = 0u;                  /* STRETCH: /8 .. /2 (0..6), x2 .. x8 (7..13); REPEAT: x2 .. x8, TO 64 */
static int32_t shp_k(void) { return shp_i < 7u ? -(int32_t)(8u - shp_i) : (int32_t)shp_i - 5; }
static uint32_t shp_len(void)                           /* the LEN the picked action would give */
{
    uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP), k;
    if (ui.act == 1u) {
        int32_t m = shp_k();
        return m > 0 ? (len * (uint32_t)m > NSTEP ? NSTEP : len * (uint32_t)m) : (len + (uint32_t)-m - 1u) / (uint32_t)-m;
    }
    if (ui.act == 2u) {
        k = shp_r < 7u ? shp_r + 2u : 0u;
        len = k ? len * k : (NSTEP / len) * len;
        return len > NSTEP ? NSTEP : len;
    }
    return len;
}

static uint32_t act_col(void)
{
    if (!ui.home && cur_page()->graph == GR_SONG) return 1u;
    if (!ui.home && cur_page()->graph == GR_EVENTS) return 5u;
    return !ui.home && cur_page()->graph == GR_PATS ? 2u : ui.act;
}

static const char *act_name(uint32_t c)          /* column c's action (the footer hint) */
{
    static const char *const UP_GO[3] = {"LOAD", "ERASE", "SAVE"};
    uint32_t id = cur_page()->id[c & 3u];
    if (cur_page()->graph == GR_MOTION) return "CLEAR";
    if (cur_page()->graph == GR_EVENTS) return ev_act_name();
    if (cur_page()->graph == GR_SHAPE) return c ? "REPEAT" : "STRETCH";
    if (cur_page()->graph == GR_TOOLS) {
        static const char *const actions[] = {"CLEAR", "INIT", "DELETE", "CLEAR", "INIT"};
        return actions[c < 5u ? c : 4u];
    }
    if (cur_page()->graph == GR_SONG)
        return song.playing || chain_busy() ? "STOP" : "PLAY";
    if (cur_page()->graph == GR_PATS)
        return "LOAD";
    if (cur_page()->graph == GR_USER)
        return UP_GO[(c + 2u) % 3u];
    return id == G_CLRSEQ ? "CLEAR" : id == G_INITSND ? "INIT" : id == G_LOAD ? "LOAD" : "SAVE";
}

/* the picked action would do something now (OCT+ breathes): another pattern, a used slot, stopped for
 * a flash write, steps to clear */
static int act_ready(void)
{
    uint32_t c = act_col(), s = song.sel, id;
    if (!c--)
        return 0;
    if (cur_page()->graph == GR_MOTION) return !chain_busy() && motion_count(TSEL);
    if (cur_page()->graph == GR_EVENTS) return ev_act_ready();
    if (cur_page()->graph == GR_SHAPE) return !chain_busy() && c < 2u;
    if (cur_page()->graph == GR_TOOLS)                  /* one case per column: CLEAR PAT, INIT, DELETE ROW, CLEAR SONG */
        return !chain_busy() && (c == 0u ? !seq_is_empty(TSEL) || motion_count(TSEL) : c == 1u || c >= 4u ? 1 :
                                 c == 2u ? ui.song_row < chain_config.count : chain_config.count != 0u);
    if (cur_page()->graph == GR_SONG)
        return song.playing || chain_busy() || chain_config.count;
    if (cur_page()->graph == GR_PATS)
        return pat_last[s] != pat_pick() + 1u || steps_sig(TSEL) != pat_sig[s];
    if (cur_page()->graph == GR_USER)
        return c == 3u ? !song.playing : up_used(ui.uslot) && (c == 1u || !song.playing);
    id = cur_page()->id[c & 3u];
    if (id == G_LOAD)
        return project_used((uint32_t)song.g[G_SLOT] - 1u);
    if (id == G_SAVE)
        return !song.playing;
    if (id == G_CLRSEQ)
        return !seq_is_empty(TSEL);
    return 1;
}
