/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;
#include "ui_organic.c"                              /* JIANT FM's organic line art (drawn by code) */
#include "ui_fm6op.c"                                /* (0.6.10, from Felucca 1.5) EDIT > OPERATOR, OP ENV, OPERATOR 2 */
static uint16_t heat_col(int32_t v);
/* GAMMA / WRAITH / JIANT (thermal): a curve's colour at canvas row y, hot at the top (y0) and cold at the bottom (y1); else c */
static uint16_t heat_y(int32_t y, int32_t y0, int32_t y1, uint16_t c)
{
    return ux.chamfer && y1 > y0 ? heat_col(24 + (y1 - y) * 232 / (y1 - y0)) : c;
}
/* GAMMA / WRAITH / JIANT: a hit or note as hot as it plays (velocity x chance; an accent white hot); else ACCENT or c */
static uint16_t step_heat(const step_t *st, int acc, uint16_t c)
{
    int32_t v = (st->vel ? st->vel : 100) * (int32_t)step_chance(st) / 100;
    if (!ux.chamfer)
        return acc ? T_ACCENT : c;
    return heat_col(acc ? 236 : 40 + v * 160 / 127);
}
#include "ui_being.c"                                /* (JIANT 0.5) the beings: the engine pages' creatures */

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t a = 4 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_line_t(x0, bot, x1, top, heat_y((bot + top) / 2, top, bot, c), 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), heat_y(py, top, bot, c), 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), heat_y(py, top, bot, c), 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), heat_y(py, top, bot, c), 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
    if (pg->id[0] == P_ATK) {                           /* (JIANT 0.5) the track's envelope now: the loudest voice's */
        const voice_t *bv = 0;                          /* level as a dot on the curve, a line down to the axis */
        int32_t lvl, x;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].stage && t->v[i].stage < 4u && (!bv || t->v[i].env > bv->env))
                bv = &t->v[i];
        if (bv) {
            lvl = (int32_t)(((int64_t)bv->env * 1000) >> 24);
            if (bv->stage == 1u)
                x = x0 + a * lvl / 1000;
            else if (bv->stage == 2u)
                x = x1 + (sus < 1000 ? clamp((1000 - lvl) * d / (1000 - sus), 0, d) : 0);
            else
                x = x3 + (sus > 0 ? clamp((sus - lvl) * r / sus, 0, r) : r);
            cv_rect(x, EGY(lvl), 1, bot + 2 - EGY(lvl), T_MID);
            cv_rrect(x - 3, EGY(lvl) - 3, 7, 7, 3, T_TEXT, T_SURF);
        }
    }
#undef EGY
}

/* LFO: the wave as it runs (JIANT 0.5): the phase now at the playhead (a quarter in), two cycles across, scrolling
 * at the LFO's own rate; behind the playhead dimmer (what it played), the level now a dot, its depth after FADE */
static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, xp = PANEL_W / 4;
    uint32_t span = 0xFFFFFFFFu / (PANEL_W / 2u), ph = t->lfo_ph - (uint32_t)xp * span;
    int32_t now = cy - mulq15(t->lfo_val, t->lfo_fade) * a / 32768;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {                  /* (lfo_wave only reads the track) */
        uint32_t px = ph + (uint32_t)x * span;
        int32_t y = cy - lfo_wave((track_t *)t, px) * a / 32768;
        if (t->p[P_LWAVE] == 4)                      /* S&H: a value per cycle part, held */
            y = cy - ((int32_t)((((px >> 29) + 1u) * 2654435761u) >> 16) - 32768) * a / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y,
                      x < xp ? ux_mix(T_SURF, heat_y(y, cy - a, cy + a, c), 45) : heat_y(y, cy - a, cy + a, c), 2);
        py = y;
    }
    cv_rect(PANEL_X0 + xp, cy - a - 4, 1, 2 * a + 8, T_MID);   /* the playhead */
    cv_rrect(PANEL_X0 + xp - 3, now - 3, 7, 7, 3, T_TEXT, T_SURF);
}

/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups),
 * the row centred: x 12 .. 228 */
static int32_t bar_x(uint32_t i) { return 12 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* PATTERN page: the 64 steps as 4 rows of 16 bars (an accent: the accent colour, a tie: a lower bar,
 * empty: a stub), the cursor and the playhead under them */
static void graph_steps(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    int32_t sm = graph_ht < 100;                     /* MENU > LARGE's strip: rows 14 px apart, bars 10 px */
    int32_t bh = sm ? 10 : 14, pitch = sm ? 14 : 24, y0 = sm ? 2 : 4;
    for (i = 0; i < NSTEP; i++) {
        int32_t x = bar_x(i), y = y0 + (int32_t)(i / 16u) * pitch;
        const step_t *st = &seq_steps(t)[i];
        if (i >= len)
            continue;
        if (i % 16u == 0u && i + 16u <= len)
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(16), "step bars' row centred");
        if (step_on(st))
            cv_rrect(x, y, 9, bh, 2, step_heat(st, st->flags & SF_ACCENT, c), T_SURF);
        else if (st->time == ST_TIE)
            cv_rrect(x, y + bh - 6, 9, 6, 1, T_MID, T_SURF);
        else
            cv_rrect(x, y + bh - 3, 9, 3, 1, T_RAISE, T_SURF);
        if (i == ui.cursor)
            cv_rect(x, y + bh + 2 - sm, 9, 2, T_ACCENT);
        else if (song.playing && i == seq_src(t, t->seq_idx))
            cv_rect(x, y + bh + 2 - sm, 9, 2, T_TEXT);
    }
}

/* (0.6.39) SEQ > COMP: the cursor's 16 steps as on PATTERN, each with its mark under it (an ARP component: heat
 * orange, TRIG: violet, a CHANCE under 100 %: MID, a RATCH: THEME); below, the cursor step's TYPE drawn: an ARP's
 * notes as a ladder in its ORDER (the one playing ACCENT), TRIG's loops as boxes (the ones it plays filled, the loop
 * now framed), CHANCE as a bar, RATCH as its parts */
static uint16_t comp_mark(const step_t *st)
{
    uint32_t c = st->comp ? st->comp : st->comp2;      /* (0.6.82: the second, when it is the only one) */
    if (cp_isarp(c)) return heat_col(200);
    if (c == CP_TRIG) return heat_col(90);
    if (c > CP_TRIG) return heat_col(20 + (c - CP_RAND) * 22u);   /* (0.6.42) the others */
    if (step_chance(st) < 100u) return T_MID;
    if (step_ratchet(st) > 1u) return T_THEME;
    return 0;
}
static void graph_comp(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], v = comp_view(), ct;
    const step_t *cs = &t->step[ui.cursor];
    int32_t y0 = 4, bh = 14, x, w;
    char b[32];
    uint32_t cl = (uint32_t)clamp(t->p[P_CLEN], 0, NSTEP), cnow = cl && song.playing ? (t->cpos + cl - 1u) % cl : 0xFFu;
    for (i = ui.bank * 16u; i < ui.bank * 16u + 16u && i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        uint16_t mk;
        x = bar_x(i);
        if (cl && i < cl && i == cnow) cv_rect(x, y0 + bh + 12, 9, 2, T_MID);   /* (0.6.47) C.LEN: the lane playing */
        if (i >= len) {                                 /* (past LEN: only C.LEN's components) */
            if (i < cl && (mk = comp_mark(st)) != 0) cv_rect(x, y0 + bh + 3, 9, 3, mk);
            continue;
        }
        if (step_on(st)) cv_rrect(x, y0, 9, bh, 2, step_heat(st, st->flags & SF_ACCENT, c), T_SURF);
        else if (st->time == ST_TIE) cv_rrect(x, y0 + bh - 6, 9, 6, 1, T_MID, T_SURF);
        else cv_rrect(x, y0 + bh - 3, 9, 3, 1, T_RAISE, T_SURF);
        if ((mk = comp_mark(st)) != 0 && (!cl || i < cl)) cv_rect(x, y0 + bh + 3, 9, 3, mk);
        if (st->comp == CP_JUMP && CP_JTO(st->carg) / 16u == ui.bank)   /* (0.6.42) JUMP: a line to the step it names */
            cv_line(x + 4, y0 + bh + 4, bar_x(CP_JTO(st->carg)) + 4, y0 + bh + 4, mk);
        if (st->comp && st->comp2)                      /* (0.6.82) two components: a second mark */
            cv_rect(x, y0 + bh + 6, 9, 1, comp_mark(&(step_t){.comp = st->comp2}));
        if (i == ui.cursor) cv_rect(x, y0 + bh + 8, 9, 2, T_ACCENT);
        else if (song.playing && i == seq_src(t, t->seq_idx)) cv_rect(x, y0 + bh + 8, 9, 2, T_TEXT);
    }
    if (comp_slot() == 2u) {                            /* (0.6.82) WHEN: the two, the loop now */
        static const char *const WN[4] = {"ALWAYS", "ON 1:2", "ON 2:2", "SKIPPED"};
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            uint32_t c = k ? cs->comp2 : cs->comp;
            str_cpy(b, k ? "B " : "A ", sizeof b);
            str_cpy(b + 2, c ? COMP_NAMES[COMP_ARPN + c - CP_ARPN] : "-", 12);
            if (c) { str_cpy(b + str_len(b), "  ", 3); str_cpy(b + str_len(b), WN[CW_OF(cs, k)], 9); }
            cv_text(PANEL_X0 + 2, 40 + (int32_t)k * 16, &AF_S, b, c ? T_MID : T_DIM);
        }
        cv_text(PANEL_X0 + 2, 80, &AF_S, "A SILENCING ONE (TRIG, FILL) WINS", T_DIM);
        if (song.playing) {
            str_cpy(b, "LOOP ", sizeof b);
            fmt_int(b + 5, (int32_t)t->loops + 1);
            cv_text_r(228, 40, &AF_S, b, t->loops & 1u ? T_THEME : T_ACCENT, T_SURF);
        }
        return;
    }
    ct = comp_type(cs);
    str_cpy(b, v >= COMP_ARPN && v <= COMP_ARPS ? "ARP " : "", sizeof b);
    str_cpy(b + str_len(b), v ? COMP_NAMES[v] : "NO COMPONENT  K2: TYPE", sizeof b - 8u);
    cv_text(PANEL_X0 + 2, 36, &AF_S, b, v ? T_MID : T_DIM);
    if (cl) {                                           /* (0.6.47) the lane's own length (SHIFT > C.LEN) */
        char lb[12] = {'C', '.', 'L', 'E', 'N', ' ', 0};
        fmt_int(lb + 6, (int32_t)cl);
        cv_text_r(228, 36, &AF_S, lb, T_ACCENT, T_SURF);
    }
    if (v >= COMP_ARPN && v <= COMP_ARPS) {             /* the ladder: one cycle (up to 16 notes), as it plays */
        step_t sh = *cs;
        uint32_t n, lo = 127u, hi = 0u, now = 0xFFu;
        sh.carg = (uint8_t)comp_arg(cs);
        uint8_t nt[16];
        if (drum_track(t)) {                            /* (0.6.82) a kit: its sounds in turn, or tuned up the scale */
            cv_text(PANEL_X0 + 2, 60, &AF_S, ct == CP_ARPS ? "THE HITS UP THE SCALE" : "THE SOUNDS IN TURN", T_DIM);
            return;
        }
        if (!cs->n) { cv_text(PANEL_X0 + 2, 60, &AF_S, "A STEP WITH NOTES PLAYS IT", T_DIM); return; }
        if (CP_ORD(sh.carg) == 3u) sh.carg = (uint8_t)(sh.carg & ~3u);   /* (RND: drawn UP, the order is the dice's) */
        n = CP_ORD(sh.carg) == 2u ? 2u * 16u : 16u;
        for (i = 0; i < 16u && i < n; i++) {
            nt[i] = (uint8_t)cp_arp_note(t, &sh, ct, sh.carg, i);
            if (nt[i] < lo) lo = nt[i];
            if (nt[i] > hi) hi = nt[i];
        }
        n = i;
        if (song.playing && t->arp_from == ui.cursor + 1u && t->arp_k)
            now = (uint32_t)(t->arp_k - 1u) % n;
        w = PANEL_W / (int32_t)n;
        for (i = 0; i < n; i++) {
            int32_t hgt = 8 + (hi > lo ? (int32_t)(nt[i] - lo) * 40 / (int32_t)(hi - lo) : 20);
            cv_rrect(PANEL_X0 + (int32_t)i * w + 1, 98 - hgt, w - 2, hgt, 2,
                     i == now ? T_ACCENT : heat_col(60 + (uint32_t)(nt[i] - lo) * 150u / (hi > lo ? hi - lo : 1u)), T_SURF);
        }
    } else if (v == COMP_TRIG) {                        /* the loops: a of b filled, the loop now framed */
        const uint8_t *cd = CP_TRIGS[comp_arg(cs) % CP_NTRIG];
        uint32_t nb = cd[0] ? cd[1] : 4u, cur = song.playing ? (cd[0] ? t->loops % nb : (t->loops < 3u ? t->loops : 3u)) : 0xFFu;
        w = 24;
        for (i = 0; i < nb; i++) {
            int on = cd[0] ? i == cd[0] - 1u : (cd[1] == 1u ? i == 0u : i > 0u);
            x = PANEL_X0 + (PANEL_W - (int32_t)nb * (w + 4)) / 2 + (int32_t)i * (w + 4);
            if (i == cur) cv_rrect(x - 2, 58, w + 4, w + 4, 3, T_ACCENT, T_SURF);
            cv_rrect(x, 60, w, w, 3, on ? heat_col(90) : T_RAISE, i == cur ? T_ACCENT : T_SURF);
        }
    } else if (v > COMP_TRIG) {                         /* (0.6.42) what it does, in a line, and its picture */
        static const char *const SAY[COMP_N - COMP_RAND] = {
            "THE NOTE AT RANDOM IN THE SCALE", "AN OCTAVE UP OR DOWN", "THE REST OF THE LOOP MOVED",
            "THE NOTE GLIDES OVER THE STEP", "REPEATS, EACH SOFTER", "AGAIN ON THE EMPTY STEPS AFTER",
            "THEN TO ANOTHER STEP", "THEN BACK TO STEP 1", "ONLY WHILE [FX] IS HELD", "CUTS WHAT THE TRACK SOUNDS"};
        int32_t a = (int32_t)comp_arg(cs), cx = PANEL_X0 + PANEL_W / 2;
        cv_text(PANEL_X0 + 2, 50, &AF_S, SAY[v - COMP_RAND], T_DIM);
        if (v == COMP_RAND || v == COMP_TONAL || v == COMP_OCT) {   /* the span it moves in, the note in the middle */
            int32_t r = v == COMP_RAND ? a * 4 : v == COMP_TONAL ? (a - 7) * 4 : (a % 6u == 1u || a % 6u == 3u ? -24 : 24);
            int32_t lo = v == COMP_RAND ? -r : r < 0 ? r : 0, hi = v == COMP_RAND ? r : r > 0 ? r : 0;
            cv_rrect(cx - 10, 82 - hi, 20, hi - lo + 4, 3, T_RAISE, T_SURF);
            cv_rrect(cx - 10, 82, 20, 4, 1, heat_col(200), T_SURF);
            if (v != COMP_RAND) cv_rrect(cx - 10, 82 - r, 20, 4, 1, T_ACCENT, T_SURF);
        } else if (v == COMP_BEND) {                    /* the glide */
            int32_t d = (a - 12) * 2;
            cv_line_t(PANEL_X0 + 20, 82, PANEL_X0 + PANEL_W - 20, 82 - d, heat_col(200), 3);
        } else if (v == COMP_ECHO) {                    /* the repeats fading */
            uint32_t h = 2u + (uint32_t)a % 7u, dv = CT_DIVS[CT_DIV(cs) % CT_NDIV];
            if (dv && h > dv) h = dv;                    /* (0.6.82) DIV: as many as fit, that far apart */
            w = (PANEL_W - 8) / (int32_t)(dv ? dv : h);
            for (i = 0; i < h; i++) {
                int32_t hh = 34 * (int32_t)(h - i) / (int32_t)h;
                cv_rrect(PANEL_X0 + 4 + (int32_t)i * w, 98 - hh, w - 6, hh, 3, heat_col(200 - i * 160u / h), T_SURF);
            }
        }
    } else if (v == COMP_CHANCE) {
        cv_rrect(PANEL_X0 + 2, 64, PANEL_W - 4, 12, 3, T_RAISE, T_SURF);
        cv_rrect(PANEL_X0 + 2, 64, (PANEL_W - 4) * (int32_t)step_chance(cs) / 100 + 1, 12, 3, heat_col(160), T_SURF);
    } else if (v == COMP_RATCH) {
        uint32_t r = step_ratchet(cs);
        w = (PANEL_W - 8) / (int32_t)r;
        for (i = 0; i < r; i++) cv_rrect(PANEL_X0 + 4 + (int32_t)i * w, 60, w - 6, 30, 3, heat_col(120 + i * 30u), T_SURF);
    }
}

/* (0.6.43) SEQ > MUTATE: a row a track, the next 32 bars: SOUND's changes an orange mark on top, PATTERN's a violet one
 * under it; the bar now ACCENT; the selected track's row lit; ORBIT / DRIFT at the right; FROZEN over it all */
static void graph_mutate(void)
{
    uint32_t k, b, now = song.playing ? bar_n : 0u;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        uint32_t s = MUT_BARS[(uint32_t)clamp(t->p[P_MSND], 0, 6)], q = MUT_BARS[(uint32_t)clamp(t->p[P_MPAT], 0, 6)];
        int32_t y = 4 + (int32_t)k * 16, on = (s || q) && k < trk_live;
        char nm[3] = {'T', (char)('1' + k), 0};
        cv_text(PANEL_X0, y + 1, &AF_S, nm, k == song.sel ? T_THEME : on ? T_MID : T_DIM);
        for (b = 0; b < 32u; b++) {
            uint32_t bar = now + 1u + b;
            int32_t x = PANEL_X0 + 22 + (int32_t)b * 5;
            cv_rect(x, y + 6, 4, 1, T_RAISE);
            if (on && s && bar % s == 0u) cv_rect(x, y + 1, 4, 4, mut_freeze ? T_DIM : heat_col(190));
            if (on && q && bar % q == 0u) cv_rect(x, y + 8, 4, 4, mut_freeze ? T_DIM : heat_col(90));
        }
        if (on) cv_text_r(PANEL_X0 + PANEL_W, y + 1, &AF_S, t->p[P_MMODE] ? "DRIFT" : "ORBIT", T_DIM, T_SURF);
    }
    cv_rect(PANEL_X0 + 22, 2, 4, 96, ux_mix(T_SURF, T_ACCENT, 25));   /* (the next bar's column) */
    if (mut_freeze) cv_text(PANEL_X0 + 70, 98, &AF_S, "FROZEN  [OCT-]", T_ACCENT);
}

/* (0.6.45) EDIT > MORPH: a cycle of the wave as it morphs (from WAVE towards the next ones), heat coloured by its
 * height; under it the two waves it is between */
/* (0.6.48) EDIT > IMAGE (a DRUM track): the eight sounds from left to right as IMAGE places them (as wide as DRV,
 * DIST and the FILTER let it play: dx_img_k), a row each, the kick on top, on a line from the middle */
static void graph_dximg(const track_t *t)
{
    static const char *const AB[DV_NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    int32_t img = clamp(t->p[P_DXIMG], 0, 127), k = img ? dx_img_k(t) : 0, cx = PANEL_X0 + PANEL_W / 2;
    uint32_t l;
    cv_rect(cx, 0, 1, 96, T_RAISE);
    for (l = 0; l < DV_NLANE; l++) {
        int32_t p = img ? dx_img_pan(l, img) * k >> 8 : 0, y = (int32_t)l * 12;
        int32_t x = cx + p * (PANEL_W / 2 - 14) / 64;
        uint16_t c = k < 256 && img ? T_MID : heat_col(60u + l * 22u);   /* (narrowed by DRV DIST FILTER: grey) */
        cv_rect(p < 0 ? x : cx, y + 6, p < 0 ? cx - x : x - cx, 1, c);
        cv_rrect(x - 13, y, 26, 12, 2, c, T_SURF);
        cv_text_in(x - 13, y + CAP_IN(S, 12), 26, &AF_S, AB[l], T_INK, c);
    }
}

static void graph_morph(const track_t *t)
{
    const int16_t *p = t->p;
    uint32_t e = t->eng_req % NENGINES, i, a0 = 0, a1 = 0;
    int32_t px = 0, py = 0, pos, f;
    const char *n0 = "", *n1 = "";
    char b[24];
    pd_t pa, pb;
    an_dirt_t dt = {0, 0, 0, 0, 0};
    int dirt = 0;
    if (ENGINES[e] == &ENG_PHASE) {
        uint32_t dcw = (uint32_t)(p[P_E2] << 8) * 65535u / (127u << 8);
        pos = (int32_t)(p[P_E0] & 7) * 256 + p[P_MORPH] * 768 / 127;
        dcw = (dcw * 56000u) >> 16;
        a0 = ((uint32_t)pos >> 8) & 7u; a1 = (a0 + 1u) & 7u;
        pd_setup(&pa, a0, dcw); pd_setup(&pb, a1, dcw);
        n0 = N_PD_WAVE[a0]; n1 = N_PD_WAVE[a1];
    } else if (ENGINES[e] == &ENG_LOFI) {
        pos = clamp(p[P_E1] * 256 + p[P_MORPH] * 768 / 127, 0, RW_WRAM * 256);
        a0 = (uint32_t)pos >> 8; a1 = a0 + 1u < RW_BYTE ? a0 + 1u : a0;
        n0 = N_RWAVE[a0]; n1 = N_RWAVE[a1];
    } else if (p[P_E0] >= AW_TUBE) {                  /* (1.0-GW) TUBE FOLD: the sine as MORPH makes it dirty */
        an_dirt_set(&dt, p[P_MORPH] << 8);
        dirt = 1;
        pos = 0;
        n0 = n1 = N_ANALOG_WAVE[(uint32_t)p[P_E0] % NELEM(N_ANALOG_WAVE)];
    } else {
        if (p[P_E0] >= 4) {                             /* (0.6.81) PWM SYNC RING SAW3: the WAVE, what MORPH moves */
            static const char *const WHAT[4] = {"PWM  WIDTH", "SYNC  RATIO", "RING  RATIO", "SAW3  SPREAD"};
            str_cpy(b, WHAT[(p[P_E0] - 4) & 3], sizeof b);
            cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
            for (i = 0; i <= 110u; i++) {
                int32_t x = PANEL_X0 + (int32_t)i * 2, y = 46 - (int32_t)((i * 34u * (uint32_t)p[P_MORPH]) / (110u * 127u)) + 17;
                if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)(46 - y + 34) * 2u), 2);
                px = x; py = y;
            }
            cv_text(PANEL_X0 + 2, 90, &AF_S, b, T_MID);
            return;
        }
        pos = clamp(p[P_E0] * 256 + p[P_MORPH] * 768 / 127, 0, 1536);
        a0 = ((uint32_t)pos >> 8) & 3u; a1 = (a0 + 1u) & 3u;
        n0 = N_ANALOG_WAVE[a0]; n1 = N_ANALOG_WAVE[a1];
    }
    f = pos & 255;
    cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
    for (i = 0; i <= 110u; i++) {
        uint32_t ph = (uint32_t)(i % 110u) * (0xFFFFFFFFu / 110u), inc = 0xFFFFFFFFu / 220u;
        int32_t s, x = PANEL_X0 + (int32_t)i * 2, y;
        if (ENGINES[e] == &ENG_PHASE) {
            s = pd_wave(&pa, ph >> 16);
            if (f) s += ((pd_wave(&pb, ph >> 16) - s) * f) >> 8;
        } else if (ENGINES[e] == &ENG_LOFI) {
            s = lofi_mto(a0, ph, 0x80000000u, WRAM[((uint32_t)p[P_E2] >> 3) & 15u], 0, 1u);
            if (f) s += ((lofi_mto(a1, ph, 0x80000000u, WRAM[((uint32_t)p[P_E2] >> 3) & 15u], 0, 1u) - s) * f) >> 8;
        } else if (dirt)
            s = an_dirt((uint32_t)p[P_E0], ph, &dt);
        else
            s = an_morph(pos, ph, inc);
        y = 46 - s * 34 / 32768;
        if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)(46 - y + 34) * 2u), 2);
        px = x; py = y;
    }
    str_cpy(b, n0, sizeof b);
    if (f && a1 != a0) { str_cpy(b + str_len(b), " > ", 4); str_cpy(b + str_len(b), n1, 8); }
    cv_text(PANEL_X0 + 2, 90, &AF_S, b, T_MID);
}

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 21                                  /* semitones shown (1.75 octaves) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_on[k] && kb_trk[k] == song.sel) {
            uint32_t i;
            for (i = 0; i < kb_on[k]; i++) {            /* (0.6.81: a chord's every note) */
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_snd[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
        }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || ANIM_SNAP();
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_ACCENT ? c : T_TEXT, hits);
        else
            pr_split(x, y + 1, w, PR_RH - 2, c, hits);
    }
}
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot = PR_Y0 + PR_ROWS * PR_RH, gw;
    char nb[8];
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    gw = (int32_t)ncol * PR_CW + 1;
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the lane, the key, the C names */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, T_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? T_DIM : T_RAISE);
        if (pc == 0u) {
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, T_RAISE);
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_V, "piano roll C label on its row");
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_R | AL_PASS, "piano roll C labels' right edge");
            {   /* its ink ends at x 23, a column before the keys (by the advance, C1's 1 ended 2 px short) */
                int32_t b[4];
                text_ink(&AF_S, nb, b);
                cv_text_on(23 - b[2], y - 5, &AF_S, nb, T_DIM, T_SURF);   /* (the widest, "C-1", inside the panel) */
            }
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_RAISE);
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (si == ui.cursor)
            cv_frame(x, PR_Y0 - 2, PR_CW + 1, ybot - PR_Y0 + 4, T_TEXT);
        if (song.playing && si == seq_src(t, t->seq_idx))
            cv_rect(x + 6, PR_Y0, 1, ybot - PR_Y0, T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark under the step */
            cv_rect(x + 3, ybot + 3, 7, 2, T_ACCENT);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        col = s == ui.cursor || si == ui.cursor ? T_ACCENT : step_heat(st, 0, c);   /* (JIANT: as hot as it plays) */
        if (s == ui.cursor && ui.pr_focus && ui.pr_focus <= st->n && cur_page()->id[0] == 4u) {   /* (0.6.81) SEQ >
                                                         * NOTE: the chord dimmed, the note in focus lit */
            step_t one = *st;
            int32_t yy = -10000;
            pr_bars(st, s == si ? x + 2 : x - 1, s == si ? 9 : 12, T_MID, 0, &y, s == si ? step_ratchet(st) : 1u);
            one.n = 1; one.note[0] = st->note[ui.pr_focus - 1u]; one.hit = 0;
            pr_bars(&one, s == si ? x + 2 : x - 1, s == si ? 9 : 12, T_ACCENT, 1, &yy, s == si ? step_ratchet(st) : 1u);
        } else if (s == si) {                         /* (0.6.88: its TIMING off the grid, shown off it) */
            int32_t mo = ST_MICRO(st) * PR_CW / 16;
            pr_bars(st, x + 2 + mo < PR_X0 ? PR_X0 : x + 2 + mo, 9, col, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        }
        else                                          /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + 10, y + 2, x + 14, ny + 2, T_TEXT);   /* the slide into the next note */
        }
    }
    if (ui.entry_open && !ui.ent_arp && ui.ent_nn && ui.ent_len > 1u) {   /* (0.6.81) a chord held, KNOB 1 its length:
                                                         * the length to be written, outlined from the cursor on */
        uint32_t k, j;
        for (k = 0; k < ui.ent_len && k < len; k++) {
            uint32_t si = (ui.cursor + k) % len;
            int32_t x;
            if (si < base || si >= base + ncol) continue;
            x = PR_X0 + (int32_t)(si - base) * PR_CW;
            for (j = 0; j < ui.ent_nn; j++) {
                int32_t y = pr_row_y(ui.ent_notes[j]);
                if (y < PR_Y0 || y >= ybot) continue;
                cv_frame(k ? x - 1 : x + 2, y, k ? 13 : 10, PR_RH, T_ACCENT);
            }
        }
    }
    if (song.g[G_STRN]) {                             /* (0.6.45) ALL TR (SCL 2): the notes sound moved, shown */
        char b[16];
        str_cpy(b, "ALL TR ", sizeof b);
        b[7] = song.g[G_STRN] < 0 ? '-' : '+'; b[8] = 0;
        fmt_int(b + 8, song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
        cv_text_r(PANEL_X0 + PANEL_W, 0, &AF_S, b, T_ACCENT, T_SURF);
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t y0 = 6;
    for (i = 0; i < 16u && base + i < len; i++)     /* a parameter lock: a mark over the step */
        if ((locks >> (base + i)) & 1u)
            cv_rect(41 + (int32_t)i * 12, 1, 8, 2, T_ACCENT);
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel = l == ui.lane;
        uint16_t row = sel ? T_RAISE : T_SURF;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rrect(6, y, 228, 14, 3, T_RAISE, T_SURF);   /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + 2, 0, y + 12, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y - 1, &AF_S, drum_lane_abbr(t, l), sel ? T_ACCENT : T_MID, row);   /* ink rows 2 .. 11, as the hits */
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            if ((step_lanes(st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + 2, 10, 10, step_heat(st, step_accents(st) & b, c), step_ratchet(st));
            else if (step_lanes(st) & b)
                cv_rrect(x, y + 2, 10, 10, 2, step_heat(st, step_accents(st) & b, c), row);
            else
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + 1, 12, 12, T_TEXT);
        }
    }
    if (song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank)
        cv_rect(40 + (int32_t)(seq_src(t, t->seq_idx) % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
}
/* SCL: the 12 keys as rounded bars (black keys high, white keys low): in the scale THEME, the root the
 * accent, out of the scale RAISE */
static void graph_scale(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t i, mask = scale_mask(t);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        int32_t x = 13 + (int32_t)i * 18;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? T_ACCENT : c) : T_RAISE;
        cv_rrect(x + 3, BLACK[i] ? 6 : 38, 10, 52, 3, col, T_SURF);
    }
}
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
/* the four sends as faders under their cards: a RAISE slot, the THEME fill from the bottom, a cap */
static void graph_fx(const track_t *t, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t h = t->p[P_DIST + i] * 76 / 127, x = CARD_X(i) + 26;
        uint16_t hc = ux.chamfer ? heat_col(t->p[P_DIST + i] * 256 / 127) : c;   /* (GAMMA / WRAITH / JIANT: heat) */
        cv_rrect(x, 6, 4, 82, 2, T_RAISE, T_SURF);
        if (h > 3)
            cv_rrect(x, 88 - h, 4, h, 2, hc, T_RAISE);
        cv_rrect(x - 4, 85 - h, 12, 6, 3, hc, T_SURF);
    }
}
/* SLICER and SL BUS pages (JIANT 0.6.3: one, on a bus): the pattern's 16 steps, a 'x' step a full bar; a '.' step: GATE
 * a bar as high as it stays open (DEPTH), STUT hatched (it repeats the last 'x'); the step playing underlined. Under
 * them what goes in: T1..T6 and FX, lit ON. Grey when OFF */
static void graph_slicer(uint16_t c)
{
    uint32_t i, pat = sl_pattern(), mode = (uint32_t)song.g[G_SLMODE], cur = sl.idx;
    int32_t open = 52 - song.g[G_SLDEP] * 52 / 127;     /* px a closed GATE step keeps */
    uint16_t col = mode == SL_OFF ? T_DIM : c;
    if (ux.chamfer && !ux.style) {
        /* POCKET GAMMAWAVE // GW-1: 4-Track Analog Audio Waveform Tape Deck (y = 4..69) */
        int32_t lane, x, head_x = 18 + (int32_t)(cur & 15u) * 13;
        gw_oct_panel(8, 4, 224, 65, 4, T_BG, T_LINE, 1, T_THEME);
        /* Top Yellow Tape Loop Bracket */
        cv_rect(52, 6, 136, 2, bg_col(228));
        cv_rect(52, 6, 2, 5, bg_col(228));
        cv_rect(186, 6, 2, 5, bg_col(228));
        /* 4 Continuous Scrolling Analog Audio Waveform Lanes (T1 DUB, T2 PLAY, T3 PLAY, T4 PLAY) */
        for (lane = 0; lane < 4; lane++) {
            int32_t cy = 17 + lane * 14, on = song.g[G_SLT1 + lane] != 0;
            uint16_t lc = !on ? T_DIM : lane == 0 ? bg_col(176) : lane == 1 ? T_ACCENT : lane == 2 ? bg_col(224) : bg_col(112);
            cv_rect(12, cy, 216, 1, T_GRID);
            /* Left Track Dub/Play LED */
            cv_rect(13, cy - 2, 4, 5, lane == 0 ? bg_col(184) : on ? T_ACCENT : T_RAISE);
            for (x = 20; x < 226; x += 2) {
                uint32_t step = (uint32_t)((x - 20) * 16 / 206);
                int32_t gate = ((pat >> step) & 1u) ? 5 : (open * 5 / 52);
                uint32_t sidx = (scope_w - 226u + (uint32_t)x + (uint32_t)lane * 37u) & (SCOPE_N - 1u);
                int32_t env = ((scope_buf[sidx] < 0 ? -scope_buf[sidx] : scope_buf[sidx]) * gate / 9000) +
                              (((bg_sin((uint32_t)(x * (1100 + lane * 420)) + ui.frame * 900u) < 0
                                    ? -bg_sin((uint32_t)(x * (1100 + lane * 420)) + ui.frame * 900u)
                                    : bg_sin((uint32_t)(x * (1100 + lane * 420)) + ui.frame * 900u)) * gate) >> 15);
                env = clamp(env, 1, 5);
                cv_rect(x, cy - env, 1, 2 * env + 1, lc);
            }
        }
        /* Vertical White-Hot Analog Tape Head */
        cv_rect(head_x, 6, 2, 61, T_TEXT);
        cv_rect(head_x - 2, 5, 6, 3, T_THEME);
        cv_rect(head_x - 2, 65, 6, 3, T_THEME);
    } else {
        for (i = 0; i < 16u; i++) {
            int32_t x = bar_x(i), y;
            if ((pat >> i) & 1u) {
                cv_rrect(x, 8, 9, 54, 2, mode == SL_OFF ? T_RAISE : col, T_SURF);
            } else if (mode == SL_STUT) {
                for (y = 8; y < 62; y += 4)
                    cv_rect(x, y, 9, 1, col);
            } else {
                cv_rrect(x, 59, 9, 3, 1, T_RAISE, T_SURF);
                if (open > 3)
                    cv_rrect(x, 62 - open, 9, open, 2, T_RAISE, T_SURF);
            }
            if (mode != SL_OFF && i == cur)
                cv_rect(x, 65, 9, 2, T_ACCENT);
        }
    }
    for (i = 0; i < NTRK + 1u; i++) {                   /* T1..T6, FX: in or not */
        int32_t x = 8 + 32 * (int32_t)i, on = song.g[G_SLT1 + i] != 0;
        uint16_t fill = on ? (mode == SL_OFF ? T_MID : c) : T_RAISE, ink = on ? T_INK : T_DIM;
        char b[3] = {(char)('1' + i), 0, 0};
        if (i == NTRK) { b[0] = 'F'; b[1] = 'X'; }
        cv_rrect(x, 74, 26, 16, 4, fill, T_SURF);
        GFX_HOOK_ALIGN(x, 74, x + 26, 90, AL_HV, "SLICER bus chip label centred");
        cv_text_in(x, 74 + CAP_IN(S, 16), 26, &AF_S, b, ink, fill);
    }
}
/* SL PITCH (JIANT 0.6.3): the repeats' 16 pitch steps as bars from the middle (+-24 semitones), past LEN dim, the one
 * KNOB 2 edits underlined, the one playing (STUT running) lit */
static void graph_slpitch(uint16_t c)
{
    uint32_t i, len = (uint32_t)clamp(song.g[G_SLPLEN], 1, 16), now = song.g[G_SLMODE] == SL_STUT ? sl.idx % len : 0xFFu;
    int32_t y0 = 46, hmax = 38;
    cv_rect(10, y0, 220, 1, T_RAISE);
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i), v = song.g[G_SLP0 + i], h = v * hmax / 24;
        uint16_t col = i >= len ? T_RAISE : i == now ? T_ACCENT : ux.chamfer ?
                       heat_col(128 + v * 5) : c;
        if (h > 0)
            cv_rrect(x, y0 - h, 9, h, 2, col, T_SURF);
        else if (h < 0)
            cv_rrect(x, y0 + 1, 9, -h, 2, col, T_SURF);
        else
            cv_rect(x, y0 - 1, 9, 3, col);
        if (i == sl_ui_step)
            cv_rect(x, y0 + hmax + 6, 9, 2, T_TEXT);
    }
}
/* MOD page: the four slots as rows "1 LFO > CUT +50%", the one KNOB 2..4 edit selected, slots that do
 * nothing (SRC, DST or AMT at 0) dim */
static void graph_mod(const track_t *t, uint16_t c)
{
    uint32_t k;
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 14 + (int32_t)k * 24, on = p[0] && p[1] && p[2], sel = k == mod_ui_slot;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : on ? c : T_DIM;
        char b[8];
        const char *unit;
        if (sel)
            cv_rrect(6, y, 228, 17, 4, T_THEME, T_SURF);
        b[0] = (char)('1' + k);
        b[1] = 0;
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(14, y + 1, &AF_S, b, sel ? T_INK : T_MID, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(34, y + 1, &AF_S, N_MSRC[clamp(p[0], 0, MS_N - 1)], col, bg);
        cv_line(78, y + 8, 92, y + 8, col);              /* an arrow */
        cv_line(88, y + 4, 92, y + 8, col);
        cv_line(88, y + 12, 92, y + 8, col);
        cv_text_on(102, y + 1, &AF_S, mod_dst_name(t, p[1]), col, bg);
        param_format(&TP[P_M1AMT], p[2], b, &unit);
        cv_text_on(cv_text_on(170, y + 1, &AF_S, b, col, bg) + 2, y + 1, &AF_S, unit, sel ? T_INK : T_DIM, bg);
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}



/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */
static void graph_wheel(const track_t *t, uint16_t c)
{
    uint32_t k;
    static const char *const names[9] = {"16", "5.3", "8", "4", "2.7", "2", "1.6", "1.3", "1"};
    for (k = 0; k < 9u; k++) {
        int32_t x = 11 + (int32_t)k * 25, level = drw_level(t->p, k);
        if (t->p[P_E4] && k == 8u) level = 0; /* DSP's percussion cancels the 1-foot bar. */
        int hot = ui.hot_t && cur_page()->id[0] == P_E0 &&
                  (ui.hot_col == 0u || ui.hot_col == (k < 2u ? 1u : k < 4u ? 2u : 3u));
        cv_rrect(x + 7, 2, 4, 73, 2, T_RAISE, T_SURF);
        if (level) cv_rrect(x + 3, 75 - level * 8, 12, level * 8, 3, hot ? T_ACCENT : c, T_SURF);
        else cv_rrect(x + 3, 73, 12, 2, 1, hot ? T_ACCENT : T_DIM, T_SURF);
        GFX_HOOK_ALIGN(x + 7, 0, x + 11, 0, AL_H, "wheel footage under its drawbar");
        cv_text_in(x + 3, 82, 12, &AF_S, names[k], T_MID, T_SURF);   /* under the bar, by its ink */
    }
}
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
/* FM6's EDIT pages: the algorithm, drawn as graph_fm draws DIGITAL's, "ALG 05" top left. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers. */
static void graph_fm6(const track_t *t, uint16_t c, uint32_t sel)   /* sel < 6: the operator picked (ui_fm6op.c) */
{
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    int32_t x[6], y[6], bus, cl = 240, cr = 0, fb = clamp(pt[FP_FB] + t->p[P_E1], 0, 7);
    uint8_t m[6], fan[6] = {0, 0, 0, 0, 0, 0};
    uint16_t rc = hot == P_E2 ? T_ACCENT : c;
    char b[11] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    if (!t->p[P_E0])                                 /* (0.6.46) ALG on PAT: the patch's number, and that it is the patch's */
        str_cpy(b + 6, " PAT", 5);
    if (hot == P_E3 || hot == P_E4 || hot == P_E5)
        hotset = 63u & ~car;
    else if (hot == P_E6)
        hotset = car;
    fm6_routes(alg, m);
    for (k = 0; k < 6u; k++) {
        uint32_t col = FM6_CELL[alg][k] & 7u, row = FM6_CELL[alg][k] >> 4;
        hi = col > hi ? col : hi;
        top = row > top ? row : top;
        for (j = 0; j < 6u; j++)
            fan[j] += (uint8_t)(m[k] >> j & 1u);
    }
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus);
    FM6_CHART_HOOK(FMH_FB, fbop, 0);
    fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    for (k = 0; k < 6u; k++) {
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = hotset >> k & 1u ? T_ACCENT : c, fill = k == sel ? T_ACCENT : fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "FM6 operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, n, fill_c || k == sel ? T_INK : on ? f : T_DIM, fill);
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    cv_text(10, 4 - GOY, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    if (sel < 6u) {                                  /* the operator picked: "OP3 x1.50" */
        char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
        fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
        cv_text_r(228, 4 - GOY, &AF_S, fq, T_TEXT, T_SURF);
    } else if (fm6_bank_slot(fm6_slot[(t - trk) % NTRK]) >= 0) {   /* (0.6.11) SLOT B1..B32: the bank voice's name */
        char nm[11];
        for (k = 0; k < 10u; k++)
            nm[k] = (char)pt[FP_NAME + k];
        for (nm[10] = 0; k && nm[k - 1] == ' '; k--)
            nm[k - 1] = 0;
        cv_text_r(228, 4 - GOY, &AF_S, nm, T_TEXT, T_SURF);
    }
    FM6_CHART_HOOK(FMH_END, alg, 0);
}

/* OP ENV (ui_fm6op.c): the picked operator's envelope as the patch has it: from L4 to L1 at R1, L2 at R2, L3 at R3, held
 * there while the key is down (a RAISE mark where it is let go), back to L4 at R4. A stage's width grows as its rate
 * falls (4..48 px: 99 is a step, 0 the longest), the levels 0..99 linearly (dB). The curve THEME, the stage KNOB 3 / 4
 * edit (STAGE) ACCENT with a dot at its target; "OP3" top left, the stage's rate and level top right */
static void graph_fm6env(const track_t *t, uint16_t c)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    int32_t top = 24, bot = 88, x[6], y[6], k;
    uint32_t sg = fop_stg & 3u;
    char b[24] = {'O', 'P', (char)('1' + fop_op % 6u), 0};
#define FEGY(l) (bot - (int32_t)(l) * (bot - top) / 99)
#define FEGW(r) (4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801)
    x[0] = 12;
    y[0] = FEGY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++) {
        x[k + 1] = x[k] + FEGW(op[FP_R1 + k]);
        y[k + 1] = FEGY(op[FP_L1 + k]);
    }
    x[5] = 228;
    x[4] = x[5] - FEGW(op[FP_R1 + 3]);
    y[4] = y[3];
    y[5] = y[0];
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_rect(x[4], top, 1, bot - top + 1, T_RAISE);   /* the key let go */
    for (k = 0; k < 5; k++)                          /* (3: the hold, no stage of its own) */
        cv_line_t(x[k], y[k], x[k + 1], y[k + 1], k != 3 && (uint32_t)(k < 3 ? k : 3) == sg ? T_ACCENT : c, 2);
    k = sg < 3u ? (int32_t)sg + 1 : 5;
    fm_dot(x[k], y[k], T_ACCENT);
#undef FEGY
#undef FEGW
    cv_text(10, 4 - GOY, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4 - GOY, &AF_S, b, T_TEXT, T_SURF);
}

#if FELUCCA_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++) mask |= (uint8_t)((project_name(i, graph_pname[i]) != 0) << i);
        graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave, PATTERN's steps,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_STEPS, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_STEPS ? SK_STEPS : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    if (pg->fam == FAM_FX && !page_over) {              /* (0.6.24) FX: the page within its section */
        uint32_t h = ui.page;
        while (h > 0u && !fx_head(h))
            h--;
        for (i = h; i < NPAGES && PAGES[i].fam == FAM_FX && (i == h || !fx_head(i)); i++)
            if (page_visible(i)) {
                n++;
                if (i == ui.page)
                    k = n;
            }
        pt = str_eq(PAGES[h].title, "FX") ? pg->title : PAGES[h].title;   /* (the section's name: "DLY 2/2", "SLICER
                                                         * 4/4"; 0.6.81: TRACK's its pages' own, "AUTO PAN 3/3" was
                                                         * "FX 3/3") */
    } else
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            n++;
            if (i == ui.page)
                k = n;
        }
    str_cpy(ti, pt ? pt : grid_on() ? "GRID" : pg->title, 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", 4);
        fmt_int(ti + str_len(ti), (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t)) {  /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
/* the thermal ramp (GAMMA / WRAITH / JIANT): a value 0..256 as heat:
 * - GAMMA: Cherenkov blue -> violet -> furnace crimson -> Bakelite orange -> amber-gold -> yellow -> core white
 * - WRAITH: Void indigo -> ultraviolet -> specimen crimson -> rose plasma -> peach -> spectral mint -> core white
 * - JIANT: cyan (cold) .. blue .. violet .. red .. orange .. yellow .. white */
static uint16_t heat_col(int32_t v)
{
    static const uint16_t H[7]  = {0x069Fu, 0x59FFu, 0xB19Cu, 0xF146u, 0xFBC0u, 0xFEA0u, 0xFFFFu};
    static const uint16_t HG[7] = {0x131Au, 0x4116u, 0xA8E4u, 0xEA20u, 0xFC80u, 0xFFE6u, 0xFFFFu};
    static const uint16_t HW[7] = {0x2092u, 0x611Au, 0xB8EEu, 0xF9CAu, 0xFD94u, 0x7FEFu, 0xFFFFu};
    static const uint16_t HQ[7] = {0x1047u, 0x2ADBu, 0x06FFu, 0xD959u, 0xFCA2u, 0xFF51u, 0xFFFFu};   /* QUANTUM: vacuum indigo, blue, cyan, magenta, amber, gold, white */
    const uint16_t *R = settings.palette == UI_GAMMA_INDEX ? HG :
                        settings.palette == UI_WRAITH_INDEX ? HW :
                        settings.palette == UI_QUANTUM_INDEX ? HQ : H;
    int32_t k;
    v = clamp(v, 0, 256);
    if (!ux.chamfer)
        return v < 128 ? ux_mix(T_SURF, T_ACCENT, v * 100 / 128) : ux_mix(T_ACCENT, T_TEXT, (v - 128) * 100 / 128);
    k = v * 6 / 257;
    return ux_mix(R[k], R[k + 1], (v * 6 - k * 256) * 100 / 256);
}

/* DRUM (DRUM-X): the eight sounds as columns, each as high and as hot as its hit now (a muted one crossed); the MORPH
 * A .. B above. sel: the sound edited (EDIT > SOUND), its name in the theme colour; -1 none */
static void graph_drum(const track_t *t, int32_t sel)
{
    const drum_lane_t *K = drum_kit_of(t);
    int32_t morph = clamp(t->p[P_E0] + dx_mot_v, 0, 127), mx = 30 + morph * 180 / 127;   /* (0.5.1: X-MOD live) */
    uint32_t l;
    cv_rect(30, 12, 180, 1, T_LINE);
    cv_rect(30, 11, mx - 30, 3, heat_col(morph * 2));
    cv_text(12, 8, &AF_S, "A", morph < 64 ? T_THEME : T_DIM);
    cv_text_r(226, 8, &AF_S, "B", morph >= 64 ? T_THEME : T_DIM, T_SURF);
    for (l = 0; l < 8u; l++) {
        int32_t x = 18 + (int32_t)l * 26, e = K && K[l].x.live ? K[l].x.ea >> 22 : 0, h = 4 + e * 66 / 256;
        int muted = dx_lane_muted(l);
        cv_rect(x, 94 - 70, 16, 70, T_SURF);
        cv_rect(x + 7, 24, 2, 70, T_LINE);
        if (!muted)
            cv_rect(x, 94 - h, 16, h, e ? heat_col(e) : T_DIM);
        else {
            cv_rect(x + 2, 58, 12, 1, T_DIM);
        }
        cv_text_in(x - 4, 100, 24, &AF_S, drum_lane_abbr(t, l), sel == (int32_t)l ? T_THEME : muted ? T_DIM : T_MID, T_SURF);
    }
}

static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}

/* (0.6.52) SEQ > FUGUE: the steps (their notes as heat) with the sequencer's playhead, then a lane each for heads 2..4:
 * where it is, its RATE DIR INTERVAL; the head edited lit */
static void graph_fugue(const track_t *t)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, k, ti = song.sel % NTRK;
    int32_t w = PANEL_W / (int32_t)len, x0 = PANEL_X0 + (PANEL_W - w * (int32_t)len) / 2;
    if (w < 2) w = 2;
    for (i = 0; i < len; i++) {
        const step_t *st = &seq_steps(t)[i];
        int on = step_on(st);
        cv_rect(x0 + (int32_t)i * w, on ? 4 : 14, w - (w > 3 ? 1 : 0), on ? 14 : 4, on ? step_heat(st, st->flags & SF_ACCENT, ACC) : T_RAISE);
        if (song.playing && i == seq_src(t, t->seq_idx)) cv_rect(x0 + (int32_t)i * w, 20, w, 2, T_TEXT);
    }
    for (k = 0; k < FG_HEADS; k++) {
        const fg_set_t *f = &fg_set[ti][k];
        const fg_head_t *h = &fg_head[ti][k];
        int32_t y = 28 + (int32_t)k * 23;
        uint16_t c = f->rate ? heat_col(70u + k * 60u) : T_DIM;
        char b[24] = {(char)('2' + k), ' ', 0};
        if (k == fg_ui % FG_HEADS) cv_rrect(PANEL_X0, y - 2, PANEL_W, 22, 3, T_RAISE, T_SURF);
        cv_rect(x0, y + 17, w * (int32_t)len, 1, T_MID);
        if (f->rate && song.playing && h->idx < len)
            cv_rect(x0 + (int32_t)h->idx * w, y + 14, w, 6, c);
        str_cpy(b + 2, N_FGRATE[f->rate % FG_NRATE], sizeof b - 2u);
        if (f->rate) {
            uint32_t n = str_len(b);
            b[n] = ' '; b[n + 1u] = 0;
            str_cpy(b + n + 1u, N_FGDIR[f->dir & 3u], sizeof b - n - 1u);
            n = str_len(b);
            b[n] = ' ';
            b[n + 1u] = f->ivl > 0 ? '+' : 0;
            fmt_int(b + n + 1u + (f->ivl > 0), f->ivl);
        }
        cv_text(PANEL_X0 + 4, y, &AF_S, b, f->rate ? T_TEXT : T_DIM);
    }
}

/* (0.6.51) the page's name big (page_flash). (0.6.61) In the header, not over the cards or the graph (covering them
 * lost the sense of what a turn did): for PF_MS the header shows the page's name (M) and, smaller at the right, the
 * track and its engine; on HOME (a track picked there) the track's engine big and "HOME" small. Then the header comes
 * back (ui.force). (0.6.64) Over the footer instead, which says the same: the header (load, battery) stays */
static void page_flash_draw(void)
{
    static uint32_t drawn;
    uint32_t e = fm1_ms - pf_ms, ph, dim;
    char nm[20], sub[20] = {'T', 0, 0};
    const page_t *pg = cur_page();
    const char *pt = pg->scope == SC_ENGINE ? ENGINES[TSEL->eng_req % NENGINES]->page_title[pg->id[0] != P_E0] : 0;
    const char *en = ENGINES[TSEL->eng_req % NENGINES]->name;
    if (e >= PF_MS) {
        pf_on = 0;
        ui.foot_sig ^= 1u;                           /* (0.6.64) the footer, whole again: that alone redrawn */
        return;
    }
    dim = e > PF_MS - 120u;
    ph = (pf_ms << 1 | dim) + 1u;                     /* (a new flash or its dimming; else every 4th frame: cheap) */
    if (!ui.force && ph == drawn && (ui.frame & 3u))
        return;
    drawn = ph;
    sub[1] = (char)('1' + song.sel % NTRK);
    sub[2] = ' ';
    if (ui.home) {                                   /* HOME: the engine big, "T2 HOME" small */
        str_cpy(nm, en, sizeof nm);
        str_cpy(sub + 3, "HOME", sizeof sub - 3u);
    } else {
        str_cpy(nm, pt && pt[0] != '-' ? pt : pg->title, sizeof nm);
        str_cpy(sub + 3, en, sizeof sub - 3u);
    }
    cv_begin(240, H_FOOT, T_BG);                     /* (0.6.64) at the bottom: the footer says the same */
    cv_bg = T_BG;
    cv_text(8, (H_FOOT - (int32_t)AF_M.h) / 2, &AF_M, nm, dim ? T_MID : T_TEXT);
    cv_text_r(232, (H_FOOT - (int32_t)AF_S.h) / 2, &AF_S, sub, dim ? T_DIM : T_ACCENT, T_BG);
    cv_blit(0, Y_FOOT);
}

/* (0.6.61) the panels' cytoplasm: a few granules drifting behind the graph, as HOME's, so the screen breathes. LOAD
 * FULL and ANIM ON only, not on HOME (its own), the lists and the dense grids (the roll, the steps); (0.6.64) no
 * redraw of their own: they move when the panel redraws anyway (a knob, the LFO, the envelope), so they cost nothing */
#ifndef UI_NO_DRIFT
#define UI_NO_DRIFT 0                                /* (tests/ui_test.c: 1, its frames compared pixel by pixel) */
#endif
static int drift_on(void)
{
    uint32_t g = cur_page()->graph;
    if (UI_NO_DRIFT)
        return 0;
    return !ui.home && !sys_load && !(ui_prefs & PREF_ANIM_OFF) && strip_kind() == SK_NONE && g != GR_ROLL && g != GR_SHAPE &&
           g != GR_STEPS && g != GR_CHANCE && g != GR_BROWSE && g != GR_USER && g != GR_SLOTS && g != GR_EVENTS &&
           g != GR_SONG && g != GR_TOOLS && g != GR_PATS;
}
static void graph_drift(void)
{
    uint32_t k, gh = graph_h();
    for (k = 0; k < 14u; k++) {
        uint32_t h = k * 2654435761u + 0x9E37u;
        int32_t gx = 10 + (int32_t)((h >> 8) % 220u + ui.frame * (1u + (h >> 28)) / 8u) % 220;
        int32_t gy = 4 + (int32_t)(((h >> 16) % (gh - 8u)) + (uint32_t)(sine_i((ui.frame * 200u + h) << 16) >> 13)) % (int32_t)(gh - 8u);
        cv_rect(gx, gy, 1 + (int32_t)(h >> 31), 1 + (int32_t)(h >> 31), ux_mix(T_SURF, T_THEME, 18 + (int32_t)(h >> 29) * 3));
    }
}
static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if (pg->graph == GR_NONE || pg->graph == GR_ARP || pg->graph == GR_MOTION) h ^= ui.frame / 2u;
    if (pg->graph == GR_LFO)                         /* (JIANT 0.5) the LFO runs: its phase, its level */
        h ^= (t->lfo_ph >> 23) * 2654435761u ^ (uint32_t)(t->lfo_val * t->lfo_fade >> 22);
    if (pg->graph == GR_ADSR)                        /* the envelope's dot: every voice's stage and level */
        for (i = 0; i < NVOICE; i++)
            h = (h ^ ((uint32_t)t->v[i].stage << 24 | (uint32_t)t->v[i].env >> 16)) * 16777619u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_MSEQ)                        /* (JIANT) the step picked, the one playing, the level now */
        h ^= (ms_ui_step + 1u) * 40503u + (song.playing ? ui.frame * 2654435761u : 0u);
    if (pg->graph == GR_SONG) {
        for (uint32_t r = 0; r < CHAIN_ROWS; r++)
            h = (h ^ (song_idx.scene[song_cur][r].on * (r + 1u))) * 16777619u;
        h ^= ui.song_row * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
        h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u + chain.pass * 15485863u : 0u;
        h ^= SONG_END(&chain_config) * 2246822519u;      /* (0.6.22: the END) */
        for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (24u + i);
        h += graph_pname_sig;
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if ((pg->graph == GR_SLCR || pg->graph == GR_SLPIT) && song.g[G_SLMODE])   /* the SLICER's step playing */
        h ^= (sl.idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLCR || pg->graph == GR_SLPIT)   /* (its globals: the page's knobs and the bus) */
        for (i = 0; i < G_SL_N; i++)
            h = (h ^ (uint32_t)song.g[G_SLMODE + i]) * 16777619u;
    if (pg->graph == GR_SLPIT)
        h ^= (sl_ui_step + 1u) * 40503u;
    if (pg->graph == GR_TOOLS)                       /* (0.6.12) the action picked, the song's row */
        h ^= ui.act * 7919u + ui.song_row * 104729u;
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot; the project, the music's section, the song) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig + song_cur * 2654435761u + (uint32_t)(live.cur + 1) * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
    }
    if (pg->graph == GR_MOTION) h ^= motion_count(t) * 131u + motion_enabled(t);
    if (pg->graph == GR_EVENTS) {                    /* AUTO LIST: the rows, the one selected, + ADD LOCK's place */
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx);
        for (i = 0; i < n; i++)
            h = (h ^ ((uint32_t)motion.event[idx[i]].place << 24 | (uint32_t)motion.event[idx[i]].param << 16 |
                      (uint16_t)motion.event[idx[i]].value)) * 16777619u;
        h = (h ^ (n + 1u) * 40503u ^ ev_row(n) * 2654435761u ^ ((uint32_t)ui.ev_step << 8 | ui.ev_id) * 104729u) * 16777619u;
    }
    if (pg->graph == GR_CHANCE) h ^= ui.cursor * 40503u + step_chance(&t->step[ui.cursor]);
    if (pg->graph == GR_FUGUE) {                     /* (0.6.52) the heads, their settings, where they are */
        uint32_t ti = song.sel % NTRK, k;
        h = (h ^ (steps_hash(t) + (song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu) * 31u + fg_ui * 7u)) * 16777619u;
        for (k = 0; k < FG_HEADS; k++)
            h = (h ^ ((uint32_t)fg_set[ti][k].rate | (uint32_t)fg_set[ti][k].dir << 4 | (uint32_t)(uint8_t)fg_set[ti][k].ivl << 8 |
                      (song.playing ? (uint32_t)fg_head[ti][k].idx << 16 : 0u))) * 16777619u;
    }
    if (pg->graph == GR_DXIMG)                       /* (0.6.48) IMAGE and how wide it plays */
        h = (h ^ ((uint32_t)t->p[P_DXIMG] << 16 | (uint32_t)dx_img_k(t))) * 16777619u;
    if (pg->graph == GR_MORPH)                       /* (0.6.45) the wave, MORPH */
        h = (h ^ ((uint32_t)t->p[P_MORPH] << 16 | (uint32_t)t->p[P_E0] << 8 | (uint32_t)t->p[P_E1] |
                  (uint32_t)t->p[P_E2] << 24)) * 16777619u;
    if (pg->graph == GR_MUTATE) {                    /* (0.6.43) the bar, the settings, FREEZE */
        h = (h ^ (song.playing ? bar_n : 0u) * 2654435761u ^ mut_freeze * 977u) * 16777619u;
        for (i = 0; i < NTRK; i++)
            h = (h ^ ((uint32_t)trk[i].p[P_MSND] | (uint32_t)trk[i].p[P_MPAT] << 4 | (uint32_t)trk[i].p[P_MMODE] << 8)) * 16777619u;
    }
    if (pg->graph == GR_COMP) {                      /* (0.6.39) the steps, their components, the arpeggio and the loop */
        const step_t *cs = &t->step[ui.cursor];
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu;
        h = (h ^ (steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.bank * 613u)) * 16777619u;
        h = (h ^ ((uint32_t)t->p[P_CLEN] << 8 | (song.playing ? t->cpos : 0xFFu))) * 16777619u;   /* (0.6.47) C.LEN */
        for (i = ui.bank * 16u; i < ui.bank * 16u + 16u && i < NSTEP; i++)
            h = (h ^ ((uint32_t)t->step[i].comp << 8 | t->step[i].carg | (uint32_t)t->step[i].probability << 16 |
                      (uint32_t)t->step[i].flags << 24 | (uint32_t)t->step[i].comp2 << 4)) * 16777619u;
        h = (h ^ (comp_view() * 977u + cs->carg + (song.playing ? t->arp_k * 131u + t->loops * 7u + t->arp_from : 0u))) *
            16777619u;
        h = (h ^ ((uint32_t)cs->carg2 | (uint32_t)cs->cwhen << 8 | (uint32_t)cs->ctime << 16 | comp_slot() << 24)) *
            16777619u;                                  /* (0.6.82) COMP 2, WHEN */
    }
    if (pg->scope == SC_ENGINE && (ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL || t->eng_req % NENGINES == ENGI_FM6 ||
                                   (FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_DRUM)
        h ^= ui.frame * 2654435761u;                 /* DRUM-X: the specimen moves every frame */
    if (pg->graph == GR_DXSND) h ^= ui.frame * 2654435761u ^ dx_sig();
    if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm, levels, FB) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u;
    if (fop_page(pg->graph))                         /* (0.6.77) OPERATOR, OP ENV (track pages): the patch, the operator */
        h = (h ^ ((fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u ^ (uint32_t)fop_op << 8 ^ fop_stg)) * 16777619u;
    if (pg->graph == GR_STEPS || pg->graph == GR_ROLL || pg->graph == GR_CHANCE || pg->graph == GR_SHAPE) {
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu;
        if (pg->graph != GR_STEPS && ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u;
        if (pg->graph != GR_STEPS) {                 /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (pg->graph != GR_STEPS && !drum_track(t)) {   /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
            h = (h ^ (ui.pr_focus | (uint32_t)pg->id[0] << 4 | (ui.entry_open ? (uint32_t)ui.ent_len << 8 : 0u) |
                      (uint32_t)ui.ent_nn << 16)) * 16777619u;   /* (0.6.81: NOTE's focus, the length being held) */
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
#if FELUCCA_FM4
                                       {"DIGITAL", "DGTL"},
#endif
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px): the selected one a THEME bar with INK text; tag at x 14, free text from x 54 to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + CAP_IN(S, 16), &AF_S, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(54, y + CAP_IN(S, 16), &AF_S, name, sel ? T_INK : nc, bg, x1 - 54);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 1, (uint32_t)id, T_KEY, T_INK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* preset browser: the global list (every engine), the current one selected; tag DIM, name TEXT,
 * favourites starred (the accent), the selected row's suggested pattern at its right */
static void graph_browse(void)
{
    uint32_t total, cur = preset_pos(&total), e, k;
    int32_t row;
    if (!total) {
        panel_note("NO FAVORITES", "LIST ALL TO ADD SOUNDS", 0);
        return;
    }
    for (row = -3; row <= 3; row++) {
        int32_t y = LIST_Y(row + 3), x1 = 212;
        char tag[6], nm[13], pt[4], pn[13];
        uint32_t index = preset_visible(cur, total, (uint32_t)(row + 3));
        int sel = index == cur;
        int32_t hint = sel ? preset_pat_hint() : -1;    /* the suggested pattern */
        if (index >= total) continue;
        e = preset_at(index, &k);
        if (e == NENGINES) {                             /* user preset: "U07" and its name */
            up_slot_label(tag, k);
            up_name(k, nm);
        } else {
            str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
        }
        if (hint >= 0) {
            pat_label((uint32_t)hint, pt, pn);
            x1 = 206 - text_w(&AF_S, pt) - 6;
        }
        list_row(y, sel, tag, T_DIM, nm, T_TEXT, x1);
        if (hint >= 0) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(206, y + CAP_IN(S, 16), &AF_S, pt, T_INK, T_THEME);
        }
        if (favorite_has(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
            cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_SURF);
        }
    }
}
/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: "03" (its place in KNOB 2's list,
 * the engine's sounds) or "U07", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t total, cur = eng_list_pos(&total), u = user_of(TSEL);
    char tag[6], nm[13];
    int fav = preset_favorite();
    if (u < UP_SLOTS) {
        up_slot_label(tag, u);
        up_name(u, nm);
    } else {
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        str_cpy(nm, e->npresets ? e->presets[TSEL->preset % e->npresets].name : "", sizeof nm);
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav) {
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
        cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, T_INK, T_THEME);
    }
    (void)total;
}
/* user preset slots around the selected one: "U07  NAME" / EMPTY */
static void graph_user(void)
{
    int32_t row, first = clamp((int32_t)ui.uslot - 3, 0, UP_SLOTS - 7);
    for (row = 0; row < 7; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        int used = up_used(k);
        up_slot_label(tag, k);
        if (used)
            up_name(k, nm);
        else
            str_cpy(nm, "--", sizeof nm);
        list_row(LIST_Y(row), k == ui.uslot, tag, T_MID, nm, used ? T_TEXT : T_DIM, 232);
    }
}
/* SEQ > PATTERNS: the pattern list around the one picked ("03  MELODY", "U07  MY BASS") */
static void graph_pats(void)
{
    uint32_t n = pat_count(), cur = pat_pick();
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        list_row(LIST_Y(row), k == cur, tag, T_MID, nm, T_TEXT, 232);
    }
}
/* SEQ > AUTO LIST (ui_events.c): the track's records around the one selected, a row each: the kind (LOCK in the
 * accent, AUTO), the step, the parameter's name, the value at the right; the last row + ADD LOCK */
static void graph_events(void)
{
    const track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), cur = ev_row(n), k;
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n + 1 > 7 ? (int32_t)n + 1 - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) <= n; row++) {
        int32_t y = LIST_Y(row);
        int sel;
        uint16_t bg;
        k = (uint32_t)(first + row);
        sel = k == cur;
        bg = sel ? T_THEME : T_SURF;
        if (sel)
            cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
        if (k == n) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, "+ ADD LOCK", sel ? T_INK : T_DIM, bg);
            break;
        }
        {
            const motion_event_t *e = &motion.event[idx[k]];
            uint32_t id = MOTION_ID(e), lock = (e->param & MOTION_LOCK) != 0u;
            char b[16], nm[12];
            const char *unit;
            const param_desc_t *d = track_desc(t, id);
            b[0] = (char)('0' + (MOTION_STEP(e) + 1u) / 10u);
            b[1] = (char)('0' + (MOTION_STEP(e) + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, lock ? "LOCK" : "AUTO", sel ? T_INK : lock ? T_ACCENT : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(54, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_MID, bg);
            param_format(d, MOTION_VAL(e), b, &unit);
            if (unit && unit[0] && str_len(b) + str_len(unit) < sizeof b)
                str_cpy(b + str_len(b), unit, sizeof b - str_len(b));
            ev_name(t, id, nm);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_free_text(80, y + CAP_IN(S, 16), &AF_S, nm, sel ? T_INK : T_TEXT, bg, 226 - text_w(&AF_S, b) - 8 - 80);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_TEXT, bg);
        }
    }
}
/* (0.6.12) SAVE > TOOLS: a list of what it can do (on what: the track, the song's row), the one picked (KNOB 1 or
 * PRESETS, 0.6.25: was a knob each) filled; OCT+ does it (a dialog first) */
static void graph_tools(void)
{
    static const char *const WHAT[7] = {"CLEAR SEQ", "INIT SOUND", "DELETE ROW", "CLEAR SONG", "NEW PROJECT", "CPU TEST",
                                        "CLEAR PROJ"};
    static const char *const ON[7] = {"STEPS + AUTO", "ENGINE DEFAULT", "", "ALL ROWS", "EVERYTHING", "HEAVY SONG",
                                      "SECTIONS A-D"};
    uint32_t c;
    char b[24];
    for (c = 0; c < 7u; c++) {                          /* (1.2.1-GW, issue #22: 7, CLEAR PROJECT; the list fills the
                                                         * panel, OCT+'s action in the footer as before) */
        int32_t y = LIST_Y(c);
        int sel = ui.act == c + 1u;
        uint16_t bg = sel ? T_THEME : T_SURF;
        b[0] = (char)('1' + c); b[1] = 0;               /* (0.6.25: a list, KNOB 1 / PRESETS: was a knob each) */
        list_row(y, sel, b, T_MID, WHAT[c], T_TEXT, 150);
        if (c == 2u) {                                   /* (the row SONG has picked) */
            str_cpy(b, "ROW ", sizeof b);
            fmt_int(b + 4, (int32_t)ui.song_row + 1);
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_DIM, bg);
        } else {
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, ON[c], sel ? T_INK : T_DIM, bg);
        }
    }
}
/* (0.6.12) SAVE > PROJECT: the project (one of NSONG: a song of storage.c) and its four sections. A title line
 * (PROJECT n, the section the music came from or went to), the sections A-D (name, USED or --; the one picked filled;
 * the music's: NOW; the bars each plays in the project's song), then the song (ARRANGE) in a line: A4 > B8 > .. */
static void graph_slots(void)
{
    uint32_t i, r, bars[4] = {0, 0, 0, 0};
    char b[40];
    for (r = 0; r < chain_config.count && r < CHAIN_ROWS; r++)
        bars[chain_config.row[r].slot & 3u] += chain_config.row[r].bars;
    str_cpy(b, "PROJECT ", sizeof b);
    fmt_int(b + 8, (int32_t)song_cur + 1);
    cv_text_on(14, LIST_Y(0) + CAP_IN(S, 16), &AF_S, b, T_ACCENT, T_SURF);
    str_cpy(b, "OF ", sizeof b);
    fmt_int(b + 3, (int32_t)NSONG);
    cv_text_r(226, LIST_Y(0) + CAP_IN(S, 16), &AF_S, b, T_DIM, T_SURF);
    for (i = 0; i < 4u; i++) {
        int32_t y = LIST_Y(i + 1u);
        int sel = (int32_t)i + 1 == song.g[G_SLOT], used = graph_project_used(i), now = live.cur == (int8_t)i;
        const char *n = graph_project_name(i);
        uint16_t bg = sel ? T_THEME : T_SURF;
        b[0] = (char)('A' + i);
        b[1] = 0;
        list_row(y, sel, b, now ? T_ACCENT : T_MID, !used ? "--" : n[0] ? n : "USED", used ? T_TEXT : T_DIM, 150);
        if (bars[i]) {                               /* (its bars in the song) */
            fmt_int(b, (int32_t)bars[i]);
            str_cpy(b + str_len(b), " BAR", 8);
            cv_text_r(now ? 190 : 226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_DIM, bg);
        }
        if (now) cv_text_r(226, y + CAP_IN(S, 16), &AF_S, "NOW", sel ? T_INK : T_ACCENT, bg);
    }
    if (!chain_config.count)
        str_cpy(b, "NO SONG YET: SAVE > ARRANGE", sizeof b);
    else {
        uint32_t n = 0;
        for (r = 0; r < chain_config.count && n + 6u < sizeof b; r++) {
            if (r) { b[n++] = '>'; }
            b[n++] = (char)('A' + (chain_config.row[r].slot & 3u));
            fmt_int(b + n, chain_config.row[r].bars);
            n = str_len(b);
        }
        if (r < chain_config.count) { b[n++] = '.'; b[n++] = '.'; }
        b[n] = 0;
    }
    cv_free_text(14, LIST_Y(5) + 2 + CAP_IN(S, 16), &AF_S, b, chain_config.count ? T_THEME : T_DIM, T_SURF, 226 - 14);
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];                              /* (the cards: JIANT's six columns, else the four from trk_base4) */
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_THEME;
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    cv_icon_on(4, 4, 16, trk_icon(c, sel), sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c - trk_base4()), LG_Y_GRAPH);
}
/* (JIANT 0.6) the JIANT style's mixer: the six tracks side by side, 38 px each. The track's cushion (armed: a dot,
 * red recording), the sound's name (muted: MUTE), LEVEL as the thermal column with the meter brighter over it, its dB
 * beside it, PAN (from the middle) and the REV send as the two bars under it */
#define T6_W 38
#define T6_X(c) (3 + 39 * (int32_t)(c))
static void track_col6(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t hot, uint32_t lvl, int32_t pan,
                       int32_t rv, const char *b)
{
    char v[8];
    int32_t k, lv = (int32_t)lvl * 14 / 127, mt = ts.meter[c] * 14 / TS_MH, pw = T6_W - 8;
    uint16_t lc = hot == 1u ? T_ACCENT : lvl ? (mute ? T_DIM : T_TEXT) : T_DIM;
    cv_begin(T6_W, H_GRAPH, T_BG);
    cv_rrect(0, 0, T6_W, H_GRAPH, 5, T_SURF, T_BG);
    if (ux.chamfer && !ux.style) {
        cv_rect(4, 0, T6_W - 8, 1, sel ? T_ACCENT : T_RAISE);
        cv_rect(4, H_GRAPH - 1, T6_W - 8, 1, sel ? T_ACCENT : T_RAISE);
    }
    cv_icon_on(4, 5, 16, trk_icon(c, sel), sel ? T_ACCENT : mute ? T_DIM : T_MID, T_SURF);
    if (st == 1u || st == 2u)                        /* armed: a dot (recording: red) */
        cv_rrect(T6_W - 11, 10, 6, 6, 3, st == 1u ? T_REC : T_ACCENT, T_SURF);
    if (mute == 2u)                                  /* asleep (TRACKS 4) */
        cv_text_in(2, 22, T6_W - 4, &AF_S, "OFF", T_DIM, T_SURF);
    else {                                           /* (0.6.24) the sound's name cut to the column (its first word, as
                                                      * many letters as fit, no "..."); muted: in MUTE's colour, still
                                                      * there (it said MUTE instead: which sound, gone) */
        char sn[13];
        uint32_t n;
        for (n = 0; b[n] && b[n] != ' ' && n < 12u; n++)
            sn[n] = b[n];
        sn[n] = 0;
        while (n > 1u && text_w(&AF_S, sn) > T6_W - 7)
            sn[--n] = 0;
        cv_text_on(4, 22, &AF_S, sn, mute ? T_ACCENT : sel ? T_TEXT : T_MID, T_SURF);
    }
    for (k = 0; k < 14; k++) {                       /* LEVEL: cold at the bottom, hot at the top */
        int32_t sy = 95 - k * 4;
        uint16_t sc = heat_col(20 + k * 220 / 13);
        sc = mute ? (k < lv ? T_DIM : T_RAISE) : k < mt ? sc : k < lv ? ux_mix(T_SURF, sc, 42) : T_RAISE;
        cv_rect(4, sy, 12, 3, sc);
    }
    if (lvl) {
        int32_t d = LEVEL_DB_X10[lvl];
        fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
    } else {
        str_cpy(v, "OFF", sizeof v);
    }
    cv_text_in(17, 44, T6_W - 18, &AF_S, v, lc, T_SURF);
    cv_text_in(17, 58, T6_W - 18, &AF_S, "dB", T_DIM, T_SURF);
    cv_rect(4, 104, pw, 3, T_RAISE);                 /* PAN, from the middle */
    cv_rect(4 + pw / 2, 102, 1, 7, hot == 2u ? T_ACCENT : T_MID);
    if (pan)
        cv_rect(pan > 0 ? 4 + pw / 2 + 1 : 4 + pw / 2 + pan * (pw / 2) / 64, 104, (pan > 0 ? pan : -pan) * (pw / 2) / 64 + 1,
                3, mute ? T_DIM : hot == 2u ? T_ACCENT : heat_col(70 + (pan > 0 ? pan : -pan) * 2));
    cv_rect(4, 113, pw, 3, T_RAISE);                 /* the REV send */
    if (rv)
        cv_rect(4, 113, rv * pw / 127 + 1, 3, mute ? T_DIM : hot == 3u ? T_ACCENT : heat_col(40 + rv * 3 / 2));
    cv_blit((uint32_t)T6_X(c), Y_GRAPH);
}
static void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK, six = !sk;   /* (0.6.80: the six columns with every COLOR and STYLE;
                                                         * MENU > LARGE: four, bigger) */
    uint32_t b4 = six ? 0u : trk_base4(), n = six ? NTRK : 4u;
    if (ui.force) {
        lcd_fill(0, graph_y(), 240, graph_h(), T_BG);
        if (ux.style && !six)                        /* LINE: the strips divided as the cards above (four) */
            draw_rules(graph_y(), graph_h());
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = b4; c < b4 + n; c++) {                  /* (JIANT 0.6: the six, or four of them) */
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        if (c >= trk_live)                           /* (0.6.22) TRACKS 4: asleep, grey as muted, "OFF" (mute 2) */
            mute = 2u;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : T_THEME;
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u + c * 0x51u +
              six * 0x3C6Fu;
        if (!ui.force && sig == ts.col[c - b4])
            continue;
        ts.col[c - b4] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b);
            continue;
        }
        if (six) {
            track_col6(c, sel, st, mute, hot, lvl, pan, rv, b);
            continue;
        }
        cv_begin(CARD_W, H_GRAPH, T_BG);
        cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);
        cv_icon_on(4, 5, 16, trk_icon(c, sel), sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted (K4 just turned: lit) */
            cv_keycap(53 - kc_w(KC_MUTE), 6, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* (the JIANT style: track_col6) */
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10[lvl];
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            if (ts.meter[c])
                cv_rrect(45, TS_MY + TS_MH - 1 - ts.meter[c], 4, ts.meter[c] + 1, ts.meter[c] >= 4 ? 2 : 0, T_MID, T_RAISE);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / 64);
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        }
        cv_blit((uint32_t)CARD_X(c - b4), Y_GRAPH);
    }
}
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N];
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = cv_h == H_GRAPH ? 46 : cy - 6;   /* (LARGE: the strip) */
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)                                                   /* GAMMA / WRAITH / JIANT: each stroke as hot as it is loud */
            cv_line_t(PANEL_X0 - 1 + x, py, PANEL_X0 + x, y, ux.chamfer ?
                      heat_col(40 + (y > cy ? y - cy : cy - y) * 216 / a) : c, 2);
        py = y;
    }
}

/* SONG: the rows, each a section (the saved project A..D, sounds and steps) and its bars (song_chain.c) */
/* (JIANT) MSEQ: the 16 levels as bars (past LEN dim), the step KNOB 1 picked framed, the one playing in the accent, and
 * the level the matrix reads now (STEP, slewed) as a line */
static void graph_mseq(const track_t *t, uint16_t c)
{
    uint32_t i, len = t->p[P_MSLEN] > 0 ? (uint32_t)t->p[P_MSLEN] : 1u;
    uint32_t per = div_samples((uint32_t)t->p[P_MSDIV]), now = song.playing && per ? ms_clock / per % len : 0xFFu;
    int32_t y0 = 92, hmax = 80;
    for (i = 0; i < 16u; i++) {
        int32_t x = 13 + (int32_t)i * 13, h = t->p[P_MS0 + i] * hmax / 127;
        uint16_t col = i >= len ? T_RAISE : i == now ? T_ACCENT : ux.chamfer ?
                       heat_col(48 + t->p[P_MS0 + i] * 200 / 127) : c;
        cv_rrect(x, y0 - hmax, 10, hmax, 2, T_RAISE, T_SURF);
        if (h > 0)
            cv_rrect(x, y0 - (h < 3 ? 3 : h), 10, h < 3 ? 3 : h, 2, col, T_RAISE);
        if (i == ms_ui_step)
            cv_rect(x, y0 + 3, 10, 2, T_TEXT);
    }
    if (song.playing)
        cv_rect(13, y0 - t->ms_v * hmax / 32767, 205, 1, T_TEXT);
}
/* (0.6.22) ARRANGE / SEQ > SONG, after SLOOP's arranger in JIANT's colours: the song as a column of rows, each a section
 * chip (A..D, each its own heat), its bars as blocks (one a bar, up to 16; past 16 each a 16th of it) filling as they
 * play, the bars' count, a dot for its scene; under them + ADD (the rows used of CHAIN_ROWS) and the song's END (STOP,
 * x2..x8, LOOP). On the right the whole song as a strip, each row's height its share of the bars, the play head on it,
 * the bars in all. KNOB 1 the row, 2 its section (on END: STOP / x2 .. / LOOP), 3 its bars, 4 its scene */
static uint16_t sec_col(uint32_t s)
{
    static const uint8_t HJ[4] = {60, 115, 175, 230}, HO[4] = {120, 160, 200, 240};
    return heat_col(ux.chamfer ? HJ[s & 3u] : HO[s & 3u]);
}
static void song_chip(int32_t x, int32_t y, uint32_t s, int saved, int sel, uint16_t bg)
{
    char b[2] = {(char)('A' + (s & 3u)), 0};
    uint16_t c = sel ? T_INK : saved ? sec_col(s) : T_RAISE;   /* (on the selected row: ink, its letter the theme's) */
    cv_rrect(x, y, 18, 14, 3, c, bg);
    GFX_HOOK_ALIGN(x, y, x + 18, y + 14, AL_HV, "song chip letter centred");
    cv_text_on(x + (18 - text_w(&AF_S, b)) / 2, y + CAP_IN(S, 14), &AF_S, b, sel ? T_THEME : saved ? T_BG : T_DIM, c);
}
static void graph_song(void)
{
    uint32_t end = song_end_row(), first, i, total = 0, played = 0;
    int32_t mh = (int32_t)graph_h() - 22, my = 4, acc = 0;
    if (!chain_config.count && ui.song_row == 0u) {
        panel_note("A 4 BARS > B 2 BARS", "[K2] ADD SECTION", "SECTIONS: SAVE > PROJECT");
        return;
    }
    first = ui.song_row > 3u ? ui.song_row - 3u : 0u;
    if (first + 7u > end + 1u)
        first = end + 1u > 7u ? end + 1u - 7u : 0u;
    for (i = first; i <= end && i < first + 7u; i++) {
        char b[16];
        int32_t y = LIST_Y(i - first), k, nb;
        int sel = i == ui.song_row, play = chain.running && i == chain.row;
        uint16_t bg = sel ? T_THEME : T_SURF, dim = sel ? T_INK : T_DIM;
        if (sel) cv_rrect(5, y, 182, 16, 4, T_THEME, T_SURF);
        if (i == end) {                                  /* the song's END */
            uint32_t e = SONG_END(&chain_config);
            cv_icon_on(9, y + 2, 12, ICON_LOOP, sel ? T_INK : T_MID, bg);
            cv_text_on(28, y + 1, &AF_S, "END", sel ? T_INK : T_MID, bg);
            if (e == SONG_FOREVER) str_cpy(b, "LOOP", sizeof b);
            else if (e >= 2u) { b[0] = 'x'; fmt_int(b + 1, (int32_t)e); }
            else str_cpy(b, "STOP", sizeof b);
            cv_text_on(60, y + 1, &AF_S, b, sel ? T_INK : T_TEXT, bg);
            if (e >= 2u && e != SONG_FOREVER && chain.running) {   /* (the time through) */
                fmt_int(b, chain.pass + 1); str_cpy(b + str_len(b), "/", 2); fmt_int(b + str_len(b), (int32_t)e);
                cv_text_on(110, y + 1, &AF_S, b, dim, bg);
            }
            break;
        }
        if (i == chain_config.count) {                   /* + ADD: the rows used of all */
            cv_text_on(28, y + 1, &AF_S, "+ ADD", sel ? T_INK : T_MID, bg);
            fmt_int(b, (int32_t)chain_config.count); str_cpy(b + str_len(b), "/", 2);
            fmt_int(b + str_len(b), (int32_t)CHAIN_ROWS);
            cv_text_on(110, y + 1, &AF_S, b, dim, bg);
            continue;
        }
        if (play) cv_icon_on(8, y + 2, 12, ICON_X_RIGHT, sel ? T_INK : T_ACCENT, bg);
        else { fmt_int(b, (int32_t)i + 1); cv_text_on(9, y + 1, &AF_S, b, sel ? T_INK : T_MID, bg); }
        {
            const chain_row_t *r = &chain_config.row[i];
            int saved = graph_project_used(r->slot);
            uint32_t bars = r->bars, done = play ? bars - chain.remaining : 0u;
            song_chip(28, y + 1, r->slot, saved, sel, bg);
            if (!saved && bars <= 9u)                    /* (a section with nothing stored: it plays silence) */
                cv_text_on(108, y + 1, &AF_S, "EMPTY", dim, bg);
            nb = bars > 16u ? 16 : (int32_t)bars;
            for (k = 0; k < nb; k++) {                   /* the bars as blocks, played ones full */
                uint32_t upto = bars > 16u ? (uint32_t)(k + 1) * bars / 16u : (uint32_t)k + 1u;
                uint16_t c = play && upto <= done ? (sel ? T_INK : T_TEXT) : saved ? sec_col(r->slot) : T_RAISE;
                cv_rect(52 + k * 6, y + 4, 5, 8, sel && !(play && upto <= done) ? (saved ? T_INK : T_DIM) : c);
            }
            if (song_idx.scene[song_cur][i].on)          /* the row has a scene: a dot */
                cv_rrect(150, y + 5, 6, 6, 3, sel ? T_INK : T_ACCENT, bg);
            fmt_int(b, (int32_t)bars);
            cv_text_r(182, y + 1, &AF_S, b, sel ? T_INK : saved ? T_TEXT : T_DIM, bg);
        }
    }
    for (i = 0; i < chain_config.count; i++) {          /* the song strip: the bars in all, played so far */
        total += chain_config.row[i].bars;
        if (chain.running && i < chain.row) played += chain_config.row[i].bars;
    }
    if (chain.running && chain.row < chain_config.count)
        played += chain_config.row[chain.row].bars - chain.remaining;
    if (!total)
        return;
    for (i = 0; i < chain_config.count; i++) {
        int32_t y0 = my + acc * mh / (int32_t)total, y1;
        acc += chain_config.row[i].bars;
        y1 = my + acc * mh / (int32_t)total;
        if (y1 - y0 < 2) y1 = y0 + 2;
        cv_rect(196, y0, 30, y1 - y0 - (y1 - y0 > 3 ? 1 : 0),
                graph_project_used(chain_config.row[i].slot) ? sec_col(chain_config.row[i].slot) : T_RAISE);
        if (i == ui.song_row)
            cv_rect(192, y0, 2, y1 - y0 - (y1 - y0 > 3 ? 1 : 0), T_THEME);
    }
    if (chain.running)
        cv_rect(194, my + (int32_t)(played * (uint32_t)mh / total), 34, 2, T_TEXT);   /* the play head */
    {
        char b[12];
        fmt_int(b, (int32_t)total);
        str_cpy(b + str_len(b), " BAR", 6);
        cv_text_r(230, my + mh + 4, &AF_S, b, T_MID, T_SURF);
    }
}
#include "ui_tech.c"                                /* (JIANT 0.5) the technical graphs (FX, DEST, VOICE, GLOBAL) */
static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    {   /* (0.6.57) LOAD LITE / ECO: the panel's animation (the beings, the scope, LFO, the envelope's dot, a turn
         * redrawing a curve) at most every 50 / 70 ms (~20 / 14 frames a second, FULL: as fast as it changes), so the
         * main loop keeps its time for the keys, the knobs and the LCD. A new page, track or engine: at once */
        static uint32_t gr_ms, gr_key;
        uint32_t key = ui.page * 7u + song.sel * 131u + ui.home * 3u + TSEL->eng_req * 1031u;
        if (sys_load && !ui.force && key == gr_key && fm1_ms - gr_ms < (sys_load == LOAD_ECO ? 70u : 50u))
            return;                                  /* (graph_sig stays: drawn on a later frame) */
        gr_key = key;
        gr_ms = fm1_ms;
    }
    ui.graph_sig = sig;
    cv_begin(240, graph_h(), T_BG);
    cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    if (drift_on())
        graph_drift();
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {                   /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) graph_scope(c);
        else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else if (k == SK_STEPS) graph_steps(t, c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        graph_ecosys();                              /* (JIANT 0.5: the system's beings, was the scope) */
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_STEPS:
            graph_steps(t, c);
            break;
        case GR_COMP:
            graph_comp(t, c);
            break;
        case GR_MUTATE:
            graph_mutate();
            break;
        case GR_MORPH:
            graph_morph(t);
            break;
        case GR_DXIMG:                               /* (0.6.58) KIT 3: the colony; STEREO: IMAGE */
            if (DX_KIT3(pg)) {
                cv_oy = 0;
                graph_colony(t, -1);
            } else
                graph_dximg(t);
            break;
        case GR_FUGUE:
            graph_fugue(t);
            break;
        case GR_ROLL:
        case GR_CHANCE:
        case GR_SHAPE:                               /* (0.6.85) STRETCH: the whole sequence, as the roll shows it */
            cv_oy = 0;
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_SCALE:
            graph_scale(t, c);
            break;
        case GR_FX:
            graph_fx(t, c);
            break;
        case GR_SLCR:
            graph_slicer(c);
            break;
        case GR_SLPIT:
            graph_slpitch(c);
            break;
        case GR_MOD:
            cv_oy = 0;
            graph_mod(t, c);
            break;
        case GR_BROWSE:
            cv_oy = 0;
            graph_browse();
            break;
        case GR_SLOTS:
            cv_oy = 0;
            graph_slots();
            break;
        case GR_USER:
            cv_oy = 0;
            graph_user();
            break;
        case GR_MSEQ:
            graph_mseq(t, c);
            break;
        case GR_SONG:
            cv_oy = 0;
            graph_song();
            break;
        case GR_PATS:
            cv_oy = 0;
            graph_pats();
            break;
        case GR_EVENTS:
            cv_oy = 0;
            graph_events();
            break;
        case GR_FMOP:                                /* (0.6.10) FM6's operators: the chart, the operator picked */
        case GR_FMOP2:
            if (fop_ok()) graph_fm6(t, c, fop_op % 6u);
            break;
        case GR_FMENV:                               /* .. its envelope */
            if (fop_ok()) graph_fm6env(t, c);
            break;
        case GR_DXSND:                               /* EDIT > SOUND: the kit's lanes, the sound edited singled out */
            cv_oy = 0;
            graph_colony(t, dx_ui_lane);
            break;
        case GR_TOOLS:                               /* (0.6.12) what each knob does, the one picked filled */
            cv_oy = 0;
            graph_tools();
            break;
        default:
            if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL) graph_wheel(t, c);
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c, 6u);   /* EDIT 1 and 2 */
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_DRUM) {
                cv_oy = 0;
                graph_colony(t, -1);
            }
#if FELUCCA_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && t->eng_req % NENGINES == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else if (pg->scope == SC_ENGINE) { cv_oy = 0; graph_being(t); }   /* (JIANT 0.5: the being, was the scope) */
            else {                                   /* the FX, DEST, VOICE, GLOBAL pages: their graphs (else the scope) */
                cv_oy = 0;
                if (!graph_tech(t, pg))
                    graph_scope(c);
            }
            break;
        }
    }
    cv_oy = 0;
    cv_blit(0, graph_y());
}
