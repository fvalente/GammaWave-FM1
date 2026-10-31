/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout:
 * flat SURF cards and panels on BG,
 * rounded corners, no rules (MENU > STYLE LINE: no SURF, 1 px rules between the areas instead: draw_rules), colours from the theme tokens only (gfx.c T_*). Type: S (12 px) labels,
 * M (15 px) values and the header, L (28 px) big numerals. (JIANT 0.6.56: S Chakra Petch, M IBM Plex Mono, L Michroma 18 px;
 * the JIANT palette's boxes square with HUD corners, gfx.c cv_rrect). A card value too wide for M is set in S;
 * free text (names, messages) is ellipsised at its size. */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static uint64_t lock_held(void);                       /* parameter locks: the steps held on STEP (ui_input.c) */
static void name_draw(void);

/* --------------------------------------------------------- drawing --- */
#define COL_W CARD_W                                  /* a card: 57 x 44 at x 3 + 59 c, y 28 */
#define COL_H CARD_H

static int32_t batt_level(void)                         /* thresholds 531 / 561 / 591 on ADC ch3 */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* A USB host powers us; there is no separate charger status line.
 * 4 means external power (a steady bolt inside the battery), otherwise its level. */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 4;
    return batt_level();
}

/* REC: filled, the selected track armed; outlined, another track armed */
static void draw_rec_mark(int32_t x, uint16_t bg)
{
    if (song.rec)
        cv_icon_mid(x, H_HEAD / 2, 16, (song.rec >> song.sel) & 1u ? ICON_X_REC : ICON_X_REC_O, T_REC, bg);
}

/* the battery: a 16 px Fukiai icon. The stock thresholds give 0..3 bars of 3; the font has 0..4 bars of 4:
 * each level takes the nearest share, 0 -> battery_0 (empty), 1 (1/3) -> battery_1 (1/4), 2 (2/3) ->
 * battery_3 (3/4), 3 (full) -> battery_4 (battery_2 is not used); USB power: battery_charging.
 * Colours as the drawn battery had them: one bar left the accent, empty MID (its outline), else THEME */
static uint32_t batt_icon(int32_t lvl)
{
    static const uint8_t I[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
    return I[clamp(lvl, 0, 4)];
}
/* (JIANT 0.5) the load: how much of the audio's time the sound takes now (audio.c song.cpu_q8, averaged), as five
 * segments stacked left of the battery, lit from the bottom, cold to hot (JIANT; other palettes MID); 0..5 */
static uint32_t head_load(void)
{
    uint32_t l = (song.cpu_q8 * 5u + 128u) >> 8;
    return song.cpu_q8 && !l ? 1u : l > 5u ? 5u : l;
}
static void draw_load(int32_t x)
{
    uint32_t k, n = head_load();
    for (k = 0; k < 5u; k++) {
        uint16_t c = k < n ? (ux.chamfer ? heat_col(30 + (int32_t)k * 50) : T_MID) : T_RAISE;
        cv_rect(x, 18 - (int32_t)k * 3, 3, 2, c);
    }
}
static void draw_battery(int32_t bx)
{
    int32_t lvl = batt_shown();
    cv_icon_mid(bx, H_HEAD / 2, 24, batt_icon(lvl), lvl == 1 ? T_ACCENT : lvl == 0 ? T_MID : T_THEME, T_BG);   /* 24 px: the glyph is wide and short */
}

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames (ui.frame, ~135 ms) on an integer ease-out (ROLL_EASE: cubic, over
 * half-way in a quarter of the time, no overshoot); the ones digit first, each place one frame later, all ending
 * together on a frame equal to the static render. Clipped to the value strip, which alone is redrawn while it
 * rolls (a card: rows ROLL_Y.., the header: the BPM's columns). Characters other than digits never roll.
 * A change during a roll retargets it: it restarts from the value it was going to. Shown at once (no roll):
 * a change within ROLL_SNAP frames of the last one (a fast turn reads better as plain numbers), another
 * shape (length, sign, a non-digit, a name or an enum, a value set in S), another label or unit, a track /
 * engine / palette change (ui.roll[].sig), ui.force (a page change), a card's first draw; MENU > ANIM OFF (#46). */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y 18                                       /* a card's value strip: rows 18..34, its figures (21..31) in the middle, */
#define ROLL_H 17                                       /* the label (MONO's chip: to row 17) above it, the gauge (38) below */
#define UNIT_GAP 2                                      /* (QG-1 stage 3) the value and its unit in a card: 2 px between them
                                                         * (3 made "111" + "/133" one pixel too wide for the 57 px card, and the
                                                         * value was ellipsised: a lint finding, not a layout) */
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP || ANIM_SNAP())
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (M) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never kerned,
 * so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    const aafont_t *f = &AF_M;
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = k < ROLL_BPM ? ROLL_Y : 0, h = k < ROLL_BPM ? ROLL_H : H_HEAD;
    uint16_t bg = k < ROLL_BPM ? T_SURF : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
    cv_scroll = 1;                                      /* (the lint: rolling digits are cut on purpose) */
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* the header (y 0..24), three calm slots (1.2):
 *   left    the selected track's cushion (ink 4..20), the REC mark attached (filled: it is armed; outlined: another)
 *   centre  the play state (stop, play, the song's disc), the metronome and the BPM, its ink centred on x 120 (BPM
 *           LOCK ON: a lock right of it). The metronome swings on the click's beats (seq.c click_beat_now): mirrored
 *           on the bar's 2nd and 4th, the accent's on its 1st; stopped, still and MID
 *   right   the octave, or the song row while a song plays (ending at 208), and the battery (ink 216..236; its bolt
 *           says USB power: no separate USB icon)
 * A message or a layer's label replaces the centre and the octave, centred on x 120, up to
 * 158 px; while the BPM is being changed (a SELECT turn: bpm_t) or in the GLO layer the centre stays and the message
 * goes right of the BPM (over the right slot). Every element's ink centred on row 12 (H_HEAD / 2; ui_test.c
 * test_head_centres): icons by cv_icon_mid, text by its capitals' band (CAP_IN; the odd row left over above it, as
 * everywhere): M (BPM, the song row, the octave: capitals rows 6..16) from y 2, S (a message, OCT: rows 7..15) from y 4 */
#define HEAD_SY CAP_IN(S, H_HEAD)
#define HEAD_MY CAP_IN(M, H_HEAD)
#define HEAD_TRK_X 4                                   /* the track cushion's cell */
#define HEAD_REC_X 20                                  /* the REC mark's cell (ink 23..33) */
#define HEAD_BAT_X 214                                 /* the battery's cell */
#define HEAD_GRP_R 208                                 /* the octave / song row ends here */
#define HEAD_MSG_W 158                                 /* a message centred on x 120: 41..199 */
#define HEAD_GAP 4                                     /* ink to ink: the metronome -> the BPM -> the lock */
#define HEAD_PLAY_GAP 6                                /* the play state -> the metronome */
/* the header's centre: the pen of the BPM, its ink, the metronome's ink (x0 .. x1), the BPM's rolling strip (sx0 ..
 * sx1: between the metronome's cell, either way round, and the lock's). Placed by as many 0s as the BPM has digits
 * (tabular figures: the same advance), so nothing moves while the tempo changes, only with a digit more or less; the
 * BPM's ink then sits within a pixel or so of x 120 */
typedef struct { int32_t bx, b[4], mx0, mx1, sx0, sx1; } head_geo_t;
static void head_geo(head_geo_t *g, const char *bpm)
{
    int32_t ib[4], lb[4], c = 16;
    uint32_t sz = 16;
    char z[8] = "0000000";
    z[str_len(bpm) < 7u ? str_len(bpm) : 7u] = 0;
    g->bx = ink_in(&AF_M, z, 240);
    text_ink(&AF_M, z, g->b);
    icon_ink(16, ICON_TEMPO, ib);
    if (icon_cell(&sz, ICON_TEMPO) >= 0)
        c = (int32_t)sz;
    g->mx1 = g->bx + g->b[0] - HEAD_GAP;
    g->mx0 = g->mx1 - (ib[2] - ib[0]);
    g->sx0 = g->mx1 + (ib[0] > c - ib[2] ? ib[0] : c - ib[2]);
    icon_ink(16, ICON_X_LOCK, lb);
    g->sx1 = g->bx + g->b[2] + HEAD_GAP - lb[0];
}
/* the metronome with its ink at x0 (beat: click_beat_now; the 2nd and 4th mirrored, the same ink box; the 1st the
 * accent's). Mirrored by cv_flip: the cell's alpha drawn right to left, no second glyph */
static void head_metro(int32_t x0, int32_t beat)
{
    int32_t ib[4], c = 16;
    uint32_t sz = 16;
    icon_ink(16, ICON_TEMPO, ib);
    if (icon_cell(&sz, ICON_TEMPO) >= 0)
        c = (int32_t)sz;
    cv_flip = (uint8_t)(beat > 0 && (beat & 1));
    cv_icon_mid(x0 - (cv_flip ? c - ib[2] : ib[0]), H_HEAD / 2, 16, ICON_TEMPO, beat == 0 ? T_ACCENT : T_MID, T_BG);
    cv_flip = 0;
}
/* the width cv_free_hint(x, .., s, .., maxw) draws (its word, keycap and the ellipsised rest) */
static int32_t head_hint_w(const char *s, int32_t maxw)
{
    uint32_t n, i;
    int32_t id = kc_tag(s, &n), w = 0;
    char b[48];
    for (i = 0; id < 0 && i < 7u && s[i] && s[i] != ' '; i++)
        ;
    if (id < 0 && i && s[i] == ' ' && (id = kc_tag(s + i + 1u, &n)) >= 0) {
        char wd[8] = {0};
        uint32_t j;
        for (j = 0; j < i; j++)
            wd[j] = s[j];
        w = text_w(&AF_S, wd) + KH_GAP;
        s += i + 1u;
    }
    if (id >= 0 && kc_w((uint32_t)id) + KH_GAP < maxw - w) {
        w += kc_w((uint32_t)id) + KH_GAP;
        s += n;
        while (*s == ' ')
            s++;
        if (!*s)
            return w - KH_GAP;
    }
    text_fit(b, sizeof b, s, &AF_S, maxw - w);
    return w + text_w(&AF_S, b);
}
/* a message / a layer's label at x, or centred on x 120 (x < 0), at most maxw px with its
 * icon (ui.c MSG_NOFILE: the accent's) */
static void head_msg(int32_t x, int32_t maxw)
{
    const char *m = ui.msg_t ? ui.msg : layer_head();
    int icon = m[0] == MSG_NOFILE[0];
    int32_t lw = 0, iw = icon ? 16 + KH_GAP : 0;
    if (x < 0)
        x = 120 - HALF_UP(iw + head_hint_w(m + icon, maxw - lw - iw) + lw);
    if (icon)
        x += cv_icon_mid(x, H_HEAD / 2, 16, ICON_X_NOFILE, T_ACCENT, T_BG) + KH_GAP;
    (void)cv_free_hint(x, HEAD_SY, m + icon, T_TEXT, T_BG, maxw - lw - iw);   /* (may start with a keycap) */
}
/* the octave, or the song row while a song plays, its right end at HEAD_GRP_R */
static void head_group(void)
{
    char b[8];
    int32_t x;
    if (chain.running) {
        fmt_int(b, (int32_t)chain.row + 1);
        x = HEAD_GRP_R - (16 + 4 + text_w(&AF_M, b));
        x += cv_icon_mid(x, H_HEAD / 2, 16, ICON_X_SONG, T_MID, T_BG);   /* SONG: the disc */
    } else {
        str_cpy(b, song.octave > 0 ? "+" : "", sizeof b);
        fmt_int(b + str_len(b), song.octave);
        x = HEAD_GRP_R - (text_w(&AF_S, "OCT") + 4 + text_w(&AF_M, b));
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_SY, &AF_S, "OCT", T_MID);
    }
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    cv_text(x + 4, HEAD_MY, &AF_M, b, T_THEME);
}
static void draw_head(void)
{
    static int16_t metro_x = -1;                       /* the metronome's ink as drawn (-1: hidden), its beat */
    static int8_t metro_beat;
    char b[16];
    head_geo_t g;
    int32_t beat = -1;                                 /* the metronome stands still (its swing removed in 1.1.5) */
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;   /* 2 the selected track armed, 1 another */
    uint32_t sig = (uint32_t)song.playing * 3u + rec * 5u + (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u +
                   (ui.msg_t ? str_hash(7u, ui.msg) : ui.layer * 7919u) + (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u +
                   (uint32_t)seq_counting() * 7u + (ui_prefs & PREF_BPM_LOCK) * 4099u +
                   (uint32_t)batt_shown() * 7777u + (chain.running ? (chain.row + 1u) * 104729u : 0u) +
                   head_load() * 0x9E3779B1u;
    int msg = ui.msg_t || ui.layer;
    int centre = !msg || ui.bpm_t || ui.layer == LAYER_GLO;   /* the BPM being changed: it stays, the message right */
    if (song.g[G_BPM] != ui.roll_bpm) {
        char a[8];
        fmt_int(a, ui.roll_bpm);
        fmt_int(b, song.g[G_BPM]);
        roll_note(ROLL_BPM, a, b, ui.force);
        ui.roll_bpm = song.g[G_BPM];
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    if (!centre)
        ui.roll[ROLL_BPM].from[0] = 0;                  /* (0.6.29) a message over it: no roll left to finish later */
    fmt_int(b, song.g[G_BPM]);
    head_geo(&g, b);
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0] && centre) {     /* rolling: the BPM's strip only */
            cv_begin((uint32_t)(g.sx1 - g.sx0), H_HEAD, T_BG);
            roll_text(ROLL_BPM, g.bx - g.sx0, HEAD_MY, b, ui.bpm_t ? T_ACCENT : T_THEME);
            cv_blit((uint32_t)g.sx0, Y_HEAD);
        }
        if (metro_x >= 0 && beat != metro_beat) {      /* a beat: the metronome's cell only (either way round) */
            int32_t cx = g.mx0 - (g.sx0 - g.mx1);
            metro_beat = (int8_t)beat;
            cv_begin((uint32_t)(g.sx0 - cx), H_HEAD, T_BG);
            head_metro(g.mx0 - cx, beat);
            cv_blit((uint32_t)cx, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    metro_x = -1;
    cv_begin(240, H_HEAD, T_BG);
    cv_icon_mid(HEAD_TRK_X, H_HEAD / 2, 16, trk_icon(song.sel, 1), T_ACCENT, T_BG);
    draw_rec_mark(HEAD_REC_X, T_BG);
    if (centre) {
        int32_t ib[4], lx = g.bx + g.b[2];             /* lx: the right end of the BPM (and its lock) */
        uint32_t id = song.playing ? (chain.running ? ICON_X_SONG : ICON_X_PLAY) : seq_counting() ? ICON_X_PLAY : ICON_X_STOP;
        icon_ink(16, id, ib);                          /* the play state: the song's disc, play (counting in: the
                                                        * accent's, about to play), stop */
        cv_icon_mid(g.mx0 - HEAD_PLAY_GAP - ib[2], H_HEAD / 2, 16, id,
                    song.playing ? T_THEME : seq_counting() ? T_ACCENT : T_MID, T_BG);
        head_metro(g.mx0, beat);
        metro_x = (int16_t)g.mx0;
        metro_beat = (int8_t)beat;
        if (!ui.roll[ROLL_BPM].from[0])
            GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        roll_text(ROLL_BPM, g.bx, HEAD_MY, b, ui.bpm_t ? T_ACCENT : T_THEME);
        if (ui_prefs & PREF_BPM_LOCK) {                /* BPM LOCK ON (#58): the lock right of the BPM */
            icon_ink(16, ICON_X_LOCK, ib);
            cv_icon_mid(lx + HEAD_GAP - ib[0], H_HEAD / 2, 16, ICON_X_LOCK, T_MID, T_BG);
            lx += HEAD_GAP + ib[2] - ib[0];
        }
        if (msg) {                                     /* right of the BPM, over the right slot */
            int32_t x = lx + 9 > 142 ? lx + 9 : 142;
            head_msg(x, 236 - x);
        }
    } else {
        head_msg(-1, HEAD_MSG_W);
    }
    if (!msg && (chain.running || song.octave))
        head_group();
    if (!(msg && centre)) {
        draw_battery(HEAD_BAT_X);
        draw_load(HEAD_BAT_X - 5);
    }
    cv_blit(0, Y_HEAD);
}
/* LINE: 1 px dividers in the BG gaps between the strips, not around them: under the header, above and
 * below the panel, and between the four columns over rows y .. y + h (the cards; the mixer's strips too) */
#define RULE_X0 CARD_X(0)
#define RULE_W (CARD_X(3) + CARD_W - CARD_X(0))
static void lcd_rule(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    lcd_fill(x, y, w, h, T_RULE);
    GFX_HOOK_RULE(x, y, w, h);
}
static void draw_rules(uint32_t y, uint32_t h)
{
    uint32_t i;
    if (y == Y_LABEL) {                                 /* (LARGE: the geometry of the page, ui.c card_h) */
        lcd_rule(RULE_X0, (H_HEAD + Y_LABEL) / 2, RULE_W, 1);
        lcd_rule(RULE_X0, (Y_LABEL + card_h() + graph_y()) / 2, RULE_W, 1);
        lcd_rule(RULE_X0, (graph_y() + graph_h() + Y_FOOT) / 2, RULE_W, 1);
    }
    for (i = 0; i < 3u; i++)
        lcd_rule((uint32_t)(CARD_X(i) + CARD_W), y, 1, h);
}
/* full redraw: the strips (header, cards, panel, footer) cover the rest; only the BG between them is filled */
static void draw_frame(void)
{
    uint32_t i, ch = card_h(), gy = graph_y(), gh = graph_h();   /* (MENU > LARGE: tall cards, a strip) */
    lcd_fill(0, H_HEAD, 240, Y_LABEL - H_HEAD, T_BG);
    lcd_fill(0, Y_LABEL + ch, 240, gy - Y_LABEL - ch, T_BG);
    lcd_fill(0, gy + gh, 240, Y_FOOT - gy - gh, T_BG);
    lcd_fill(0, Y_LABEL, (uint32_t)CARD_X(0), ch, T_BG);
    for (i = 0; i < 4u; i++)                            /* right of each card */
        lcd_fill((uint32_t)(CARD_X(i) + CARD_W), Y_LABEL, i < 3u ? (uint32_t)(CARD_X(i + 1u) - CARD_X(i) - CARD_W) :
                 240u - (uint32_t)(CARD_X(i) + CARD_W), ch, T_BG);
    if (ux.style)                                       /* LINE: the dividers in the gaps */
        draw_rules(Y_LABEL, ch);
}

/* MOTION on the cards (#63: values that move on their own): the selected track's motion-driven parameters
 * (motion.c motion_mask: PLAY ON and an event for the id), once per draw_columns; card_mot_next is set just
 * before a draw_column of a track parameter (card_mot_of) and taken by it; card_mot: the cards drawn with it. */
static uint32_t card_mot_mask[(P_COUNT + 31u) / 32u];
static uint8_t card_mot_next, card_mot;
static void card_mot_of(const int16_t *vp)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp;   /* (not the track's: huge) */
    card_mot_next = (uint8_t)(id < P_COUNT && ((card_mot_mask[id / 32u] >> (id % 32u)) & 1u));
}

/* MENU > LARGE, a tall card (ui.c LK_TALL; 57 x LG_CARD_H): which knob it is (a "K1" pill in M, the keycap's colours,
 * the accent while it turns, DIM when the knob does nothing here) and the icon at the right of it; the label in M
 * centred (S when M is too wide); the value centred in L, its figures twice the height of M (M, then S, when L is too
 * wide or the sparse L face lacks a glyph: gen_aa_font.py L_CHARS); the unit (S) centred under it; the gauge. The
 * value snaps (no rolling digits). MONO, the knob just turned: the label inverted as on the small cards. */
#ifndef LARGE_HOOK
#define LARGE_HOOK(s, why) ((void)0)                    /* host: a tall card's value and its face (0 L, 1 M: a glyph not
                                                         * in L, 2 M: L too wide, 3 S) */
#endif
#define LG_MY 4                                         /* the K pill: rows 4..21 */
#define LG_MH 18
#define LG_LY 24                                        /* the label's line: M capitals rows 28..38 */
#define LG_VB 49                                        /* the value's band: L capitals rows 49..69 */
#define LG_UY 73                                        /* the unit's line (S capitals rows 76..84) */
#define LG_GY 92                                        /* the gauge, 3 px */
/* a card's gauge: GAMMA / WRAITH / JIANT (thermal) its value as heat; another palette its theme colour */
static uint16_t gauge_col(int32_t ratio)
{
    return ux.chamfer ? heat_col(clamp(ratio, 0, 1000) * 256 / 1000) : T_THEME;
}
/* a card's value colour: GAMMA / WRAITH / JIANT its heat (lifted a fifth toward the text, so the cold end reads on the card); else vc */
static uint16_t value_col(uint16_t vc, int32_t ratio)
{
    return ux.chamfer && vc == T_THEME && ratio >= 0 ? ux_mix(gauge_col(ratio), T_TEXT, 20) : vc;
}
static void draw_column_tall(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                             uint32_t icon, int hot, int mot, int32_t kid)
{
    uint16_t lc = hot ? T_ACCENT : mot ? T_THEME : T_MID;
    const aafont_t *lf = &AF_M, *vf = &AF_L;
    int32_t ly = LG_LY, vy, lw = 0, mw;
    char k[3] = {'K', (char)('1' + c), 0};
    uint32_t ic = mot ? ICON_X_MOTION : icon, isz = 16u;
    cv_begin(COL_W, LG_CARD_H, T_BG);
    cv_rrect(0, 0, COL_W, LG_CARD_H, 4, T_SURF, T_BG);
    {   /* the knob: "K1" .. "K4" on a pill */
        uint16_t fill = hot ? T_ACCENT : label[0] || val[0] ? T_KEY : T_DIM;
        mw = ink_w(&AF_M, k) + 10;
        cv_rrect(5, LG_MY, mw, LG_MH, 5, fill, T_SURF);
        GFX_HOOK_ALIGN(5, LG_MY, 5 + mw, LG_MY + LG_MH, AL_HV, "large card knob pill");
        cv_text_in(5, LG_MY + CAP_IN(M, LG_MH), mw, &AF_M, k, T_INK, fill);
    }
    if (!label[0] && !val[0]) {                         /* a knob that does nothing on this page */
        cv_blit((uint32_t)CARD_X(c), Y_LABEL);
        return;
    }
    if (FELUCCA_ICONS && ic != ICON_NONE && label[0] && (ic - ICON_TRK < NTRK || icon_cell(&isz, ic) >= 0)) {
        GFX_HOOK_ALIGN(0, LG_MY, 0, LG_MY + LG_MH, AL_V, "large card icon on its pill's line");
        cv_icon_in(COL_W - 5 - 16, LG_MY, 16, LG_MH, 16, ic, mot ? (hot ? T_ACCENT : T_THEME) : lc, T_SURF);
    }
    if (label[0]) {
        if ((lw = ink_w(lf, label)) > COL_W - 6) {      /* M too wide: S, on the same capitals' middle */
            lf = &AF_S;
            ly = LG_LY + AF_M_CAP_Y + HALF_UP(AF_M_CAP_H - AF_S_CAP_H) - AF_S_CAP_Y;
            lw = ink_w(lf, label);
        }
        if (hot && T_ACCENT == T_MID) {                 /* MONO: the label inverted, the chip 2 px round its ink */
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y), chh = lf == &AF_M ? AF_M_CAP_H : AF_S_CAP_H;
            int32_t x0 = HALF_UP(COL_W - lw) - 2;
            cv_rrect(x0, cy - 3, lw + 4, chh + 6, 3, T_ACCENT, T_SURF);
            GFX_HOOK_ALIGN(x0, cy - 3, x0 + lw + 4, cy + chh + 3, AL_HV, "large card label chip (MONO hot)");
            cv_text_in(0, ly, COL_W, lf, label, T_INK, T_ACCENT);
        } else {
            GFX_HOOK_ALIGN(0, 0, COL_W, 0, AL_H, "large card label centred");
            cv_text_in(0, ly, COL_W, lf, label, lc, T_SURF);
        }
    }
    if (kid >= 0) {                                     /* "[OCT+]": the keycap, centred */
        GFX_HOOK_ALIGN(0, LG_VB, COL_W, LG_VB + AF_L_CAP_H, AL_HV, "large card keycap centred");
        cv_keycap(HALF_UP(COL_W - kc_w((uint32_t)kid)), LG_VB + HALF_UP(AF_L_CAP_H - KC_H), (uint32_t)kid,
                  vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, T_SURF);
    } else if (val[0]) {
        int why = !large_face_has(val) ? 1 : ink_w(vf, val) > COL_W - 6 ? 2 : 0;
        if (why)
            vf = &AF_M;
        if (vf == &AF_M && ink_w(vf, val) > COL_W - 6)
            vf = &AF_S, why = 3;
        LARGE_HOOK(val, why);
        vy = vf == &AF_L ? LG_VB - AF_L_CAP_Y : vf == &AF_M ? LG_VB + HALF_UP(AF_L_CAP_H - AF_M_CAP_H) - AF_M_CAP_Y
                                                          : LG_VB + HALF_UP(AF_L_CAP_H - AF_S_CAP_H) - AF_S_CAP_Y;
        if (vf == &AF_S && ink_w(vf, val) > COL_W - 4) {   /* (a value always fits: the lint reports one cut) */
            cv_text_fit(2, vy, vf, val, vc, T_SURF, COL_W - 4);
        } else {
            GFX_HOOK_ALIGN(0, LG_VB, COL_W, LG_VB + AF_L_CAP_H, AL_HV, "large card value centred");
            cv_text_in(0, vy, COL_W, vf, val, vc, T_SURF);
        }
    }
    if (unit[0]) {
        GFX_HOOK_ALIGN(0, 0, COL_W, 0, AL_H, "large card unit centred");
        cv_text_in(0, LG_UY, COL_W, &AF_S, unit, T_MID, T_SURF);
    }
    if (ratio >= 0) {
        int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
        cv_rrect(5, LG_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
        cv_rrect(5, LG_GY, fx < 3 ? 3 : fx, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : gauge_col(ratio), T_BG);
    }
    cv_blit((uint32_t)CARD_X(c), Y_LABEL);
}

/* one card = one knob: [icon] LABEL / value unit / gauge on a SURF card, redrawn only when it changed.
 * The value is M, or S when M is too wide (engine names, long ENUMs); the unit S after it. The gauge is
 * a 3 px rounded bar: BG track, THEME fill. The knob just turned (hot): icon, label, value and gauge in
 * the accent. ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label;
 * a label too long to share the card with its icon goes without it. A parameter MOTION drives (card_mot_next):
 * the motion icon in place of its own, in the theme colour; a long label moves it left, up to the card's edge;
 * a label with no room for it at all (or FELUCCA_ICONS=0): the label in the theme colour.
 * vc: the value's colour (T_THEME, T_DIM inactive, T_ACCENT the knob just turned) */
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    char key[56];
    int hot = c == ui.hot_col && ui.hot_t, named = fmt_named, mot = card_mot_next && label[0], strip, snap;
    uint32_t kind = large_kind();                       /* MENU > LARGE: tall (draw_column_tall), or the label in M */
    uint8_t sig;
    uint16_t lc = hot ? T_ACCENT : T_MID;
    int32_t x, lx = 5, uw, room = COL_W - 8;
    const aafont_t *vf = &AF_M;
    uint32_t n, kn;
    int32_t kid = kc_tag(val, &kn);
    vc = value_col(vc, ratio);
    fmt_named = 0;                                      /* (params.c: a name, for this card only) */
    card_mot_next = 0;                                  /* (the same: MOTION, this card only) */
    card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)mot << c);
    if (str_eq(unit, label))
        unit = "";                                      /* "BPM 124 BPM", "USB OFF USB": the label says it */
    if (icon == ICON_AUTO)
        icon = icon_for_label(label);
    str_cpy(key, label, 12);                            /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, 14);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), unit, 8);
    n = str_len(key);
    /* every RGB565 bit: status colours can change with identical text */
    key[n] = (char)('A' + (vc & 15u));
    key[n + 1] = (char)('A' + ((vc >> 4) & 15u));
    key[n + 2] = (char)('A' + ((vc >> 8) & 15u));
    key[n + 3] = (char)('A' + (vc >> 12));
    key[n + 4] = (char)(' ' + (ratio < 0 ? 0 : 1 + clamp(ratio, 0, 1000) * (COL_W - 10) / 1000));   /* (the gauge's px) */
    key[n + 5] = (char)(icon == ICON_NONE ? '~' : '!' + icon % 90u);
    key[n + 6] = (char)('0' + hot + 2 * mot + 4 * (int)kind);
    {   /* (0.6.10) the gauge's colour too: JIANT's heat runs over 256 steps, the width over its pixels */
        uint16_t gc = ratio < 0 ? 0u : gauge_col(ratio);
        key[n + 7] = (char)('A' + (gc & 15u));
        key[n + 8] = (char)('A' + ((gc >> 4) & 15u));
        key[n + 9] = (char)('A' + ((gc >> 8) & 15u));
        key[n + 10] = (char)('A' + (gc >> 12));
    }
    key[n + 11] = 0;
    uw = unit[0] ? text_w(&AF_S, unit) + UNIT_GAP : 0;
    if (text_w(vf, val) + uw > room)
        vf = &AF_S;
    /* (QG-1 stage 3) the value matters more than its unit: when even the small font cannot hold both,
     * the unit goes and the value is drawn whole. Before, cv_text_fit cut the value instead ("1…"). */
    if (uw && text_w(vf, val) + uw > room) {
        uw = 0;
        unit = "";
    }
    sig = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, engine, palette */
    if (kind == LK_TALL)
        ui.roll[c].from[0] = 0;                         /* (a tall card: no rolling digits) */
    strip = !snap && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, T_SURF);
        cv_oy = -ROLL_Y;
    } else {
        char ov[16];                                    /* the value drawn before (in the cache key) */
        const char *k = ui.col[c];
        uint32_t i = 0;
        while (*k && *k != '|')
            k++;
        while (*k && k[1] && k[1] != '|' && i + 1u < sizeof ov)
            ov[i++] = *++k;
        ov[i] = 0;
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0 || kind == LK_TALL);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        if (kind == LK_TALL) {
            draw_column_tall(c, label, val, unit, vc, ratio, icon, hot, mot, kid);
            return;
        }
        cv_begin(COL_W, COL_H, T_BG);
        cv_rrect(0, 0, COL_W, COL_H, 4, T_SURF, T_BG);
        if (ux.chamfer && !ux.style && hot) {
            cv_rect(4, 0, COL_W - 8, 1, T_ACCENT);
            cv_rect(0, 4, 1, COL_H - 8, T_ACCENT);
        }
    }
    if (label[0] || val[0]) {
        int gw_rotary = (ux.chamfer && !ux.style && kind == LK_OFF && ratio >= 0 && !strip &&
                         5 + text_w(&AF_S, label) <= COL_W - 22);
        if (gw_rotary) {
            /* Authentic GammaWave Bakelite Amber Rotary Knob (20x20 at top-right of module, x=35..54, y=2..21)
             * with warm bakelite body + dual Cyan/Orange modulation rings */
            int32_t kr = KNOB_SMALL_R, kx = COL_W - 2 * kr - 2, ky = 2;
            int32_t kcx = kx + kr, kcy = ky + kr, r_cl = clamp(ratio, 0, 1000);
            uint32_t th = (uint32_t)((-384 + r_cl * 768 / 1000) * 64);
            int32_t px1 = kcx + (6 * bg_sin(th) >> 15), py1 = kcy - (6 * bg_sin(th + 0x4000u) >> 15);
            uint16_t kc_arc = hot ? T_ACCENT : vc == T_DIM ? T_DIM : gauge_col(r_cl);
            uint16_t bake_fill = ux.mono ? T_BG : ux_mix(T_BG, bg_col(128), 38);
            gw_disc(kcx, kcy, kr - 2, bake_fill);
            gw_orbit_ring(kcx, kcy, kr - 3, kr - 3, mot || hot ? T_ACCENT : T_LINE, 0);
            knob(kx, ky, kr, KNOB_SMALL_COV, KNOB_SMALL_ANG, r_cl, 0, 1000, kc_arc);
            if (mot) {                                  /* (1.1.1-GW) automated: an "A" on an accent disc in the knob */
                gw_disc(kcx, kcy, kr - 4, T_ACCENT);
                cv_text_on(kcx - (text_w(&AF_S, "A") >> 1), kcy - AF_S_CAP_Y - (AF_S_CAP_H >> 1), &AF_S, "A", T_INK,
                           T_ACCENT);
            } else {
                cv_line(kcx, kcy, px1, py1, hot ? T_ACCENT : T_TEXT);
                cv_pset(kcx, kcy, T_MID);
            }
        }
        if (!strip && label[0]) {
            /* MENU > LARGE on a list / graph page or a layer (LK_LABEL): the label in M where it fits the card (its
             * capitals rows 5..15, the icon on them when it fits too), else in S as without LARGE */
            const aafont_t *lf = kind == LK_LABEL && 5 + text_w(&AF_M, label) <= COL_W - 2 ? &AF_M : &AF_S;
            int32_t max_lx = gw_rotary ? (COL_W - 22) : (COL_W - 2);
            int32_t lw = text_w(lf, label), ix = 5, ly = lf == &AF_M ? 1 : 3;
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y), chh = lf == &AF_M ? AF_M_CAP_H : AF_S_CAP_H;
            uint32_t ic = mot ? ICON_X_MOTION : icon;
            if (mot && ix + ICON_CELL + ICON_GAP + lw > max_lx)
                ix = max_lx - lw - ICON_GAP - ICON_CELL;   /* a long label: MOTION's icon moves left */
            if (!gw_rotary && FELUCCA_ICONS && ic != ICON_NONE && ix >= 1 && ix + ICON_CELL + ICON_GAP + lw <= max_lx) {
                GFX_HOOK_ALIGN(ix, cy, ix, cy + chh, AL_V, "card icon on its label's line");
                lx = ix + cv_icon_in(ix, cy, 0, chh, ICON_CELL, ic,   /* its ink on the label's line */
                                     mot ? (hot ? T_ACCENT : T_THEME) : lc, T_SURF) + ICON_GAP;
            }
            if (mot && !hot)                            /* (1.1.1-GW, issue #2: values that moved by themselves went
                                                         * unnoticed) the label in the theme colour too */
                lc = T_THEME;
            if (hot && T_ACCENT == T_MID) {             /* MONO: no colour tells the hot knob: its label inverted, */
                int32_t b[4], w;                        /* the chip 2 px round its ink (M: 3 above, 2 below) */
                text_ink(lf, label, b);
                w = b[2] - b[0] + 4;
                if (w > max_lx + 3 - lx - b[0]) w = max_lx + 3 - lx - b[0];
                cv_rrect(lx + b[0] - 2, cy - 3, w, lf == &AF_M ? chh + 5 : 15, 3, T_ACCENT, T_SURF);
                GFX_HOOK_ALIGN(lx + b[0] - 2, cy - 3, lx + b[0] - 2 + w, lf == &AF_M ? cy + chh + 2 : 18, AL_HV,
                               "card label chip (MONO hot)");
                cv_text_fit(lx, ly, lf, label, T_INK, T_ACCENT, max_lx - lx);
            } else
                cv_text_fit(lx, ly, lf, label, lc, T_SURF, max_lx - lx);
            if (mot && !gw_rotary) {                    /* (1.1.1-GW) and an "A" chip after it, in the accent: this
                                                         * value is automated (it moves on its own while it plays;
                                                         * a card with its knob: the "A" in the knob) */
                int32_t ax = lx + lw + 3, aw = text_w(&AF_S, "A") + 5;
                if (ax + aw <= max_lx + 1) {
                    cv_rrect(ax, cy - 2, aw, chh + 4, 2, T_ACCENT, T_SURF);
                    GFX_HOOK_ALIGN(ax, cy - 2, ax + aw, cy + chh + 2, AL_HV, "card AUTOMATION chip");
                    cv_text_on(ax + (aw - text_w(&AF_S, "A")) / 2, ly, &AF_S, "A", T_INK, T_ACCENT);   /* (QG-1 stage 3)
                        * centred in its chip: 3 px from the left edge left the letter 1 px right of the chip's middle */
                }
            }
        }
        if (kid >= 0)                                   /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(5, 20, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, T_SURF);
        else if (vf == &AF_M) {
            if (!ui.roll[c].from[0] && val[0])
                GFX_HOOK_ALIGN(0, ROLL_Y, COL_W, ROLL_Y + ROLL_H, AL_V, "card value in its roll window");
            x = roll_text(c, 5, 17, val, vc);
        } else
            x = cv_text_fit(5, 20, vf, val, vc, T_SURF, room - uw);
        if (unit[0]) {
            GFX_HOOK_ALIGN(0, 0, 0, vf == &AF_M ? 17 + AF_M.asc : 20 + AF_S.asc, AL_B, "card unit on its value's baseline");
            cv_text_on(x + UNIT_GAP, 20, &AF_S, unit, T_MID, T_SURF);     /* on the value's baseline (S) */
        }
        if (!strip && ratio >= 0) {
            int32_t gw = COL_W - 10, fx = ratio * gw / 1000, fillw = fx < 3 ? 3 : fx;
            cv_rrect(5, 38, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);   /* (LINE: no card to cut it from) */
            cv_rrect(5, 38, fillw, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : gauge_col(ratio), T_BG);
            if (ux.chamfer && !ux.style) {
                int32_t s;
                for (s = 6; s < gw; s += 6)
                    cv_rect(5 + s, 38, 1, 3, T_SURF);
                cv_rect(5 + fillw - 2, 37, 2, 4, hot ? T_TEXT : T_ACCENT);
            }
        }
    }
    cv_oy = 0;
    cv_blit((uint32_t)CARD_X(c), Y_LABEL + (strip ? ROLL_Y : 0));
}

/* an action's column (act_cols): picked, the OCT+ keycap (accent while it would do something); else "--" */
static void draw_act_column(uint32_t c, const char *label, uint16_t vc, uint32_t icon)   /* icon: ICON_AUTO = by label */
{
    int picked = act_col() == c + 1u;
    draw_column(c, label, picked ? "[OCT+]" : "--", "", picked ? (act_ready() ? T_ACCENT : T_DIM) : vc, -1, icon);
}

/* action pages: the footer's first row says what OCT+ and OCT- do ("OCT+ LOAD   OCT- BACK") */
static void foot_hint(char *a, char *b)
{
    uint32_t c = act_col();
    str_cpy(a, "OCT+ ", 8);
    str_cpy(a + 5, c ? act_name(c - 1u) : "--", 8);
    str_cpy(b, ui.act && cur_page()->graph != GR_PATS ? "OCT- CANCEL" : "OCT- BACK", 16);
}

/* SAVE > USER / PROJECT: EDIT renames the selected slot (ui_name.c); SEQ > AUTO LIST: EDIT deletes the row's
 * record (ui_events.c); 0 = not such a page, 1 an empty slot (+ ADD LOCK), 2 used */
static uint32_t foot_rename(void)
{
    uint32_t g = ui.home ? GR_NONE : cur_page()->graph;
    if (g == GR_EVENTS) {
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx);
        return 1u + (ev_row(n) < n);
    }
    if (g == GR_USER)
        return 1u + (uint32_t)up_used(ui.uslot);
    if (g == GR_SLOTS)
        return 1u + (uint32_t)graph_project_used((uint32_t)song.g[G_SLOT] - 1u);
    return 0;
}

/* the sound's name on the track: a user preset or the engine's preset (b holds 16) */
static void sound_name(const track_t *t, char *b)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    b[0] = 0;
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 16);
}

static void draw_foot(void)
{
    char s[48], pn[16], ti[20];
    const track_t *t = TSEL;
    uint32_t sig;
    const page_t *pg = cur_page();
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    const char *ename = e->name;
    int32_t x;
    int glob = !ui.home && (pg->graph == GR_SLOTS || pg->graph == GR_SONG || pg->graph == GR_TOOLS || pg->fam == FAM_GLO);
    char pj[12];
    sound_name(t, pn);
    if (glob) {                                        /* (0.6.25) a page of the song / the instrument: the project and
                                                        * its name, not the track's engine and sound */
        str_cpy(pj, "PROJECT ", sizeof pj);
        fmt_int(pj + 8, (int32_t)song_cur + 1);
        ename = pj;
        project_cur_name(pn);
        if (!pn[0]) str_cpy(pn, "NO NAME", 16);
    }
    if (ui.home)
        str_cpy(ti, "HOME", sizeof ti);
    else                                               /* page title + number in its family: "ENV DEST 2/2" */
        page_title(ti);
    str_cpy(s, ename, sizeof s);
    s[str_len(s) + 1u] = 0;
    s[str_len(s)] = (char)('1' + song.sel);
    str_cpy(s + str_len(s), pn, 16);
    str_cpy(s + str_len(s), ti, sizeof ti);
    {   /* step markers: the playhead only when it is in the shown bank, the cursor only in SEQ */
        uint32_t ph = song.playing && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) : 0xFFu;
        sig = str_hash(0x9E3779B9u, s) + ph * 97u + (song.seq_mode ? ui.cursor : 0xFFu) * 3001u + steps_hash(t) +
              (ui.home ? 0u : page_icon(pg)) * 7121u +
              ui.bank * 7u + (uint32_t)t->p[P_SLEN] * 13u;
    }
    if (act_cols()) {                                  /* the hint, and whether OCT+ would act */
        char ha[16], hb[16];
        foot_hint(ha, hb);
        sig += str_hash(str_hash(act_ready() ? 7u : 3u, ha), hb) + foot_rename() * 7717u;
    }
    if (grid_on())
        sig += 0x51EDu + (uint32_t)black_held(GK_ACC) * 977u;
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    cv_begin(240, H_FOOT, T_BG);
    if (act_cols()) {                                 /* row 1: the OCT+ / OCT- hint in place of the steps */
        char ha[16], hb[16];
        khint_t kh[3];
        uint32_t rn = foot_rename();
        foot_hint(ha, hb);                            /* "OCT+ LOAD", "OCT- BACK": keycaps and their words */
        kh[0] = (khint_t){KC_OCTUP, ha + 5};
        kh[1] = (khint_t){KC_OCTDN, hb + 5};
        if (rn) {                                     /* USER / PROJECT: "EDIT NAME" between them */
            kh[2] = kh[1];
            kh[1] = (khint_t){KC_EDIT, cur_page()->graph == GR_EVENTS ? "DELETE" : "NAME"};
            cv_key_row(8, 232, 2, kh, 3, (act_ready() ? 1u : 0u) | (rn == 2u ? 2u : 0u) | 4u, T_BG);
        } else {
            cv_key_row(8, 232, 2, kh, 2, act_ready() ? 3u : 2u, T_BG);
        }
    } else if (grid_on()) {                           /* row 1: the page, and what the keys do */
        char b[16];
        uint32_t len = (uint32_t)t->p[P_SLEN];
        str_cpy(b, "PAGE ", sizeof b);
        fmt_int(b + 5, (int32_t)ui.bank + 1);
        str_cpy(b + str_len(b), "/", 4);
        fmt_int(b + str_len(b), (int32_t)((len + 15u) / 16u));
        cv_text(8, 2, &AF_S, b, T_THEME);
        if (black_held(GK_ACC))
            cv_text_r(232, 2, &AF_S, "ACCENT", T_ACCENT, T_BG);
        else
            cv_key_hint(232 - kh_w(KC_KEYS, "STEPS"), 2, KC_KEYS, "STEPS", 1, T_BG);   /* the keys are the steps */
    } else {
        uint32_t i;
        if ((ui.bank + 1u) * 16u <= (uint32_t)t->p[P_SLEN])
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(16), "footer step bars centred");
        for (i = 0; i < 16u; i++) {                   /* row 1: the cursor's bank, 16 bars in 4 groups */
            uint32_t si = ui.bank * 16u + i;
            int32_t sx = bar_x(i);                    /* (as the PATTERN page's, centred: x 12 .. 228) */
            const step_t *st = &seq_steps(t)[si];
            if (si >= (uint32_t)t->p[P_SLEN])
                continue;
            if (step_on(st)) {                        /* a note: a bar (accented: the accent) */
                cv_rrect(sx, 1, 9, 11, 2, (st->flags & SF_ACCENT) ? T_ACCENT : T_THEME, T_BG);
                if (ux.chamfer) {
                    cv_rect(sx + 1, 2, 7, 2, (st->flags & SF_ACCENT) ? T_TEXT : T_ACCENT);
                    cv_rect(sx + 1, 8, 7, 3, ux_mix(T_BG, (st->flags & SF_ACCENT) ? T_ACCENT : T_THEME, 55));
                }
            } else if (ux.chamfer && !ux.style) {     /* GAMMAWAVE: boxed step matrix well */
                cv_rrect(sx, 2, 9, 10, 1, st->time == ST_TIE ? T_MID : T_SURF, T_BG);
                cv_frame(sx, 2, 9, 10, (i & 3u) == 0u ? T_RAISE : T_LINE);
                cv_rect(sx + 4, 6, 1, 2, (i & 3u) == 0u ? T_MID : T_RAISE);
            } else                                    /* empty: a stub (a tie: brighter) */
                cv_rrect(sx, 9, 9, 3, 1, st->time == ST_TIE ? T_MID : T_RAISE, T_BG);
            if (song.seq_mode && si == ui.cursor)
                cv_rect(sx, 14, 9, 2, T_ACCENT);        /* the step edited */
            else if (song.playing && si == seq_src(t, t->seq_idx))
                cv_rect(sx, 14, 9, 2, T_TEXT);          /* the step sounding */
        }
    }
    {   /* row 2: engine icon + name, sound, page title at the right with its icon before it (MIXER, PHRASES, SONG,
         * CHANCE, AUTOMATION); icons: their ink on the names' capitals */
        uint32_t pi = ui.home ? ICON_NONE : page_icon(pg), ei = FELUCCA_ICONS ? (glob ? ICON_X_SONG : engine_icon(ename)) : ICON_NONE;
        int32_t r = 232, tx, ex, need = text_w(&AF_S, pn), ew = text_w(&AF_S, ename);
        ex = 8 + (FELUCCA_ICONS ? 12 + 5 : 0) + (ew < 80 ? ew : 80);   /* (where the engine's name ends) */
        tx = r - text_w(&AF_S, ti) - (pi != ICON_NONE ? 16 : 0);
        if (!ui.home && pg->graph == GR_MOTION && tx - 12 - (ex + 10) < need) {
            str_cpy(ti + 4, ti + 10, sizeof ti - 4);    /* #93: "AUTOMATION 6/6" -> "AUTO 6/6" where the sound's name */
            tx = r - text_w(&AF_S, ti) - (pi != ICON_NONE ? 16 : 0);   /* would be cut (MENU > LARGE) */
        }
        x = 8;
        if (FELUCCA_ICONS) {
            if (ei != ICON_NONE)
                GFX_HOOK_ALIGN(0, 19 + AF_S_CAP_Y, 0, 19 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                               "footer icons on the names' line");
            x += cv_icon_in(x, 19 + AF_S_CAP_Y, 0, AF_S_CAP_H, 12, ei, T_MID, T_BG) + 5;
        }
        x = cv_text_fit(x, 19, &AF_S, ename, T_THEME, T_BG, 80);
        cv_free_text(x + 10, 19, &AF_S, pn, T_TEXT, T_BG, tx - 12 - (x + 10));
        if (pi != ICON_NONE) {
            GFX_HOOK_ALIGN(0, 19 + AF_S_CAP_Y, 0, 19 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "footer icons on the names' line");
            cv_icon_in(tx, 19 + AF_S_CAP_Y, 0, AF_S_CAP_H, 12, pi, T_THEME, T_BG);
        }
        cv_text_r(r, 19, &AF_S, ti, T_TEXT, T_BG);      /* (0.6.12: the page's name lit, not dim: where you are) */
    }
    cv_blit(0, Y_FOOT);
}
/* the EDIT layer's cards (ui_layer.c): ENG (the engine), No. (its sounds: KNOB 2's list), FAV, an empty card */
static void engine_columns(void)
{
    uint32_t total, cur = eng_list_pos(&total), e = TSEL->eng_req % NENGINES;
    char val[8], u[8];
    fmt_int(val, (int32_t)cur + 1);
    str_cpy(u, "/", 8);
    fmt_int(u + 1, (int32_t)total);
    draw_column(0, "ENG", ENGINES[e]->name, "", VAL(0u), (int32_t)eng_rank(e) * 1000 / (NENG_SHOWN > 1 ? NENG_SHOWN - 1 : 1),
                engine_icon(ENGINES[e]->name));
    draw_column(1, "No.", val, u, VAL(1u), total > 1u ? (int32_t)(cur * 1000u / (total - 1u)) : 0, ICON_NONE);
    draw_column(2, "FAV", preset_favorite() ? "ON" : "OFF", "", VAL(2u), -1, ICON_X_STAR);
    draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
}
static void draw_columns(void)
{
    uint32_t c;
    char val[12];
    const char *unit;
    (void)motion_mask(song.sel, card_mot_mask);       /* (PLAY OFF: no scan) */
    if (ui.home) {
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d = home_param(c, &vp);
            param_format(d, *vp, val, &unit);
            card_mot_of(vp);
            draw_column(c, d->label, val, unit, VAL(c), RATIO(d, enum_rank(d, *vp)), param_icon(d, *vp));
        }
        return;
    }
    if (cur_page()->graph == GR_DXSND) {               /* EDIT > SOUND (ui_dx.c) */
        dx_cards(cur_page());
        return;
    }
    if (DX_KIT3(cur_page())) {                         /* (0.6.54) EDIT > KIT 3 */
        dx_kit3_cards();
        return;
    }
    if (cur_page()->graph == GR_SONG) {
        uint32_t row = ui.song_row < CHAIN_ROWS ? ui.song_row : CHAIN_ROWS - 1u;
        int used = row < chain_config.count;
        if (ui.song_row >= song_end_row()) {            /* (0.6.22) the song's END: STOP / x2 .. x8 / LOOP */
            uint32_t e = SONG_END(&chain_config);
            draw_column(0, "ROW", "END", "", VAL(0u), -1, ICON_X_SONG);
            if (e == SONG_FOREVER) str_cpy(val, "LOOP", sizeof val);
            else if (e >= 2u) { val[0] = 'x'; fmt_int(val + 1, (int32_t)e); }
            else str_cpy(val, "STOP", sizeof val);
            draw_column(1, "END", val, "", VAL(1u), -1, ICON_LOOP);
            draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
            draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
            return;
        }
        if (row == chain_config.count) str_cpy(val, "ADD", sizeof val);
        else fmt_int(val, (int32_t)row + 1);
        draw_column(0, "ROW", val, "", VAL(0u), -1, ICON_X_SONG);
        if (used) { val[0] = (char)('A' + chain_config.row[row].slot); val[1] = 0; }
        else str_cpy(val, "--", sizeof val);
        draw_column(1, "SECT", val, "", used ? VAL(1u) : T_DIM, -1, ICON_X_PATTERN);
        if (used) fmt_int(val, chain_config.row[row].bars);
        else str_cpy(val, "--", sizeof val);
        draw_column(2, "BARS", val, "", used ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "SCENE", used && song_idx.scene[song_cur][row].on ? "ON" : "--", "",
                    used && song_idx.scene[song_cur][row].on ? VAL(3u) : T_DIM, -1, ICON_MIX);   /* (JIANT) */
        return;
    }
    if (cur_page()->graph == GR_SHAPE) {               /* (0.6.85) STRETCH REPEAT NUDGE, the LEN after */
        int32_t k = shp_k();
        uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
        val[0] = k > 0 ? 'x' : '/';
        fmt_int(val + 1, k > 0 ? k : -k);
        draw_column(0, "STRETCH", val, "", ui.act == 1u ? VAL(0u) : T_DIM, (int32_t)shp_i * 1000 / 13, ICON_X_REPEAT);
        if (shp_r < 7u) { val[0] = 'x'; fmt_int(val + 1, (int32_t)shp_r + 2); }
        else str_cpy(val, "TO 64", sizeof val);
        draw_column(1, "REPEAT", val, "", ui.act == 2u ? VAL(1u) : T_DIM, (int32_t)shp_r * 1000 / 7, ICON_X_PATTERN);
        draw_column(2, "NUDGE", "< >", "", VAL(2u), -1, ICON_ORDER);
        fmt_int(val, (int32_t)len);
        if (ui.act == 1u || ui.act == 2u) { str_cpy(val + str_len(val), ">", 2); fmt_int(val + str_len(val), (int32_t)shp_len()); }
        draw_column(3, "LEN", val, "", VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_FUGUE) {               /* (0.6.52) HEAD RATE DIR INTERVAL */
        const fg_set_t *f = &fg_set[song.sel % NTRK][fg_ui % FG_HEADS];
        fmt_int(val, (int32_t)(fg_ui % FG_HEADS) + 2);
        draw_column(0, "HEAD", val, "", VAL(0u), (int32_t)(fg_ui % FG_HEADS) * 500, ICON_X_PATTERN);
        draw_column(1, "RATE", N_FGRATE[f->rate % FG_NRATE], "", f->rate ? VAL(1u) : T_DIM, (int32_t)f->rate * 1000 / 9,
                    ICON_RATE);
        draw_column(2, "DIR", N_FGDIR[f->dir & 3u], "", f->rate ? VAL(2u) : T_DIM, (int32_t)(f->dir & 3u) * 333, ICON_ORDER);
        val[0] = f->ivl > 0 ? '+' : 0;
        fmt_int(val + (f->ivl > 0), f->ivl);
        draw_column(3, "INTVL", val, "DEG", f->rate ? VAL(3u) : T_DIM, (int32_t)(f->ivl + 14) * 1000 / 28, ICON_PITCH);
        return;
    }
    if (cur_page()->graph == GR_COMP) {                /* (0.6.39) STEP TYPE and the TYPE's two values */
        const step_t *cs = &TSEL->step[ui.cursor];
        uint32_t v = comp_view(), ca = comp_arg(cs), ct = comp_type(cs);
        static const char *const ORD[4] = {"UP", "DOWN", "UP-DN", "RND"};
        static const char *const DIVN[CT_NDIV] = {"STEP", "1/2", "1/3", "1/4", "1/6", "1/8"};
        fmt_int(val, (int32_t)ui.cursor + 1);
        draw_column(0, "STEP", val, "", VAL(0u), -1, ICON_AUTO);
        if (comp_slot() == 2u) {                       /* (0.6.82) WHEN: each component's loops, the step's LEN */
            static const char *const WN[4] = {"ALWAYS", "1:2", "2:2", "SKIP"};
            uint32_t k;
            for (k = 0; k < 2u; k++) {
                uint32_t c = k ? cs->comp2 : cs->comp, w = CW_OF(cs, k);
                draw_column(1u + k, k ? "B" : "A", c ? WN[w] : "-", "", c && w ? VAL(1u + k) : T_DIM, -1, ICON_PROB);
            }
            if (CT_LEN(cs)) { fmt_int(val, (int32_t)CT_LEN(cs)); str_cpy(val + str_len(val), "/16", 4); }
            else str_cpy(val, "GATE", sizeof val);
            draw_column(3, "LEN", val, "", CT_LEN(cs) ? VAL(3u) : T_DIM, -1, ICON_X_REPEAT);
            return;
        }
        draw_column(1, v >= COMP_ARPN && v <= COMP_ARPS ? "ARP" : "TYPE", COMP_NAMES[v], "", v ? VAL(1u) : T_DIM, -1,
                    v == COMP_CHANCE ? ICON_PROB : v == COMP_RATCH ? ICON_X_REPEAT : ICON_ARP);
        if (v == COMP_CHANCE) {
            fmt_int(val, (int32_t)step_chance(cs));
            draw_column(2, "CHANCE", val, "%", VAL(2u), -1, ICON_PROB);
        } else if (v == COMP_RATCH) {
            val[0] = 'x'; val[1] = (char)('0' + step_ratchet(cs)); val[2] = 0;
            draw_column(2, "RATCH", val, "", VAL(2u), -1, ICON_X_REPEAT);
        } else if (v == COMP_TRIG) {
            const uint8_t *c = CP_TRIGS[ca % CP_NTRIG];
            if (!c[0]) str_cpy(val, c[1] == 1u ? "FIRST" : "!1ST", sizeof val);
            else { val[0] = (char)('0' + c[0]); val[1] = ':'; val[2] = (char)('0' + c[1]); val[3] = 0; }
            draw_column(2, "COND", val, "", VAL(2u), -1, ICON_PROB);
        } else if (v > COMP_TRIG) {                    /* (0.6.42) the others' one value */
            static const char *const OCTN[6] = {"+1", "-1", "+1/2", "-1/2", "ALT", "RND"};
            int32_t a = (int32_t)ca;
            const char *lb = "", *un = "";
            val[0] = 0;
            switch (v) {
            case COMP_RAND: lb = "RANGE"; fmt_int(val, a); un = "DEG"; break;
            case COMP_OCT: lb = "MODE"; str_cpy(val, OCTN[a % 6], sizeof val); un = "OCT"; break;
            case COMP_TONAL: lb = "SHIFT"; a -= 7; val[0] = a < 0 ? '-' : '+'; fmt_int(val + 1, a < 0 ? -a : a); un = "DEG"; break;
            case COMP_BEND: lb = "BEND"; a -= 12; val[0] = a < 0 ? '-' : '+'; fmt_int(val + 1, a < 0 ? -a : a); un = "ST"; break;
            case COMP_ECHO: lb = "REPEAT"; val[0] = 'x'; fmt_int(val + 1, 2 + a % 7); break;
            case COMP_PULSE: lb = "EVERY"; fmt_int(val, 1 + a % 4); un = a ? "STEPS" : "STEP"; break;
            case COMP_JUMP: lb = "TO STEP"; fmt_int(val, (int32_t)CP_JTO(a) + 1); break;
            default: break;
            }
            if (lb[0]) draw_column(2, lb, val, un, VAL(2u), -1, ICON_X_REPEAT);
            else draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
        } else if (v != COMP_OFF) {
            draw_column(2, "ORDER", ORD[CP_ORD(ca)], "", VAL(2u), -1, ICON_ORDER);
        } else
            draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
        if (v >= COMP_ARPN && v <= COMP_ARPS) {
            uint32_t g = CP_RNG(ca);
            char un[8];
            val[0] = 'x'; fmt_int(val + 1, CP_RATES[CP_RATE(ca) % CP_NRATE]);
            fmt_int(un, v == COMP_ARPS ? CP_DEGS[g] : (int32_t)g + 1);
            str_cpy(un + str_len(un), v == COMP_ARPS ? " DEG" : " OCT", 5);
            draw_column(3, "RATE", val, un, VAL(3u), -1, ICON_X_REPEAT);
        } else if (v == COMP_JUMP || v == COMP_REV) {  /* (0.6.62) how often it turns the track */
            static const char *const TIMES[4] = {"ALWAYS", "x1", "x2", "1:2"};
            draw_column(3, "TIMES", TIMES[CP_TIMES(ct, ca)], "", CP_TIMES(ct, ca) ? VAL(3u) : T_DIM, -1, ICON_X_REPEAT);
        } else if (v == COMP_RATCH || v == COMP_ECHO) {   /* (0.6.82) the parts' spacing */
            draw_column(3, "DIV", DIVN[CT_DIV(cs) % CT_NDIV], "", CT_DIV(cs) ? VAL(3u) : T_DIM, -1, ICON_RATE);
        } else
            draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_CHANCE) {              /* STEP CHANCE RATCH: the cursor step's, of all of it */
        const step_t *cs = &TSEL->step[ui.cursor];
        int rplays = cs->time == ST_NOTE && (cs->n || cs->hit);   /* RATCH does nothing on a REST, a TIE, empty: DIM */
        fmt_int(val, (int32_t)ui.cursor + 1);
        draw_column(0, "STEP", val, "", VAL(0u), -1, ICON_AUTO);
        fmt_int(val, (int32_t)step_chance(&TSEL->step[ui.cursor]));
        draw_column(1, "CHANCE", val, "%", VAL(1u), -1, ICON_PROB);   /* the die */
        val[0] = 'x';                                  /* x1 .. x4 */
        val[1] = (char)('0' + step_ratchet(&TSEL->step[ui.cursor]));
        val[2] = 0;
        draw_column(2, "RATCH", val, "", rplays ? VAL(2u) : T_DIM, -1, ICON_X_REPEAT);
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_EVENTS) {              /* AUTO LIST (ui_events.c): ROW STEP PARAM VALUE */
        const track_t *t = TSEL;
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx), r = ev_row(n), step, id;
        int16_t v;
        char nm[12];
        const param_desc_t *d;
        if (r < n) {
            const motion_event_t *e = &motion.event[idx[r]];
            fmt_int(val, (int32_t)r + 1);
            str_cpy(val + str_len(val), "/", 4);
            fmt_int(val + str_len(val), (int32_t)n);
            step = MOTION_STEP(e);
            id = MOTION_ID(e);
            v = MOTION_VAL(e);
        } else {
            ev_fix();
            str_cpy(val, "ADD", sizeof val);
            step = ui.ev_step;
            id = ui.ev_id;
            v = id < P_COUNT ? motion_base_value(t, id) : 0;
        }
        draw_column(0, "ROW", val, "", VAL(0u), -1, ICON_AUTO);
        fmt_int(val, (int32_t)step + 1);
        draw_column(1, "STEP", val, "", r < n || step < (uint32_t)t->p[P_SLEN] ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        if (id >= P_COUNT || !ev_id_ok(t, id)) {
            draw_column(2, "PARAM", "--", "", T_DIM, -1, ICON_AUTO);
            draw_column(3, "VALUE", "--", "", T_DIM, -1, ICON_AUTO);
            return;
        }
        ev_name(t, id, nm);
        draw_column(2, "PARAM", nm, "", VAL(2u), -1, ICON_AUTO);
        d = track_desc(t, id);
        param_format(d, v, val, &unit);
        draw_column(3, r == n ? "VALUE" : (motion.event[idx[r]].param & MOTION_LOCK) ? "LOCK" : "AUTO", val, unit,
                    r < n ? VAL(3u) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        return;
    }
    if (cur_page()->graph == GR_MOTION) {
        draw_column(0, "PLAY", motion_enabled(TSEL) ? "ON" : "OFF", "", VAL(0u), -1, motion_icon());
        fmt_int(val, (int32_t)(motion_count(TSEL) - motion_lock_count(TSEL)));
        draw_column(1, "EVENT", val, "", T_MID, -1, ICON_NONE);
        fmt_int(val, (int32_t)motion_lock_count(TSEL));   /* parameter locks (SEQ > STEP: a step held, a knob) */
        draw_column(2, "LOCK", val, "", T_MID, -1, ICON_NONE);
        draw_act_column(3, "CLEAR", T_MID, ICON_X_MOTION_DEL);
        return;
    }
    if (cur_page()->graph == GR_TOOLS) {
        static const char *const labels[] = {"PAT", "SOUND", "ROW", "SONG"};
        static const uint8_t icons[] = {ICON_AUTO, ICON_AUTO, ICON_X_SONG, ICON_X_SONG};   /* ROW, SONG: the song's */
        for (c = 0; c < 4u; c++) draw_act_column(c, labels[c], VAL(c), icons[c]);
        return;
    }
    if (cur_page()->scope == SC_TRK) {                 /* LEVEL PAN REV MUTE of the selected track */
        const track_t *t = TSEL;
        uint32_t lvl = trk_level(song.sel);
        param_format(&TP[P_LEVEL], (int32_t)lvl, val, &unit);   /* (0: OFF) */
        card_mot_of(&TSEL->p[P_LEVEL]);
        draw_column(0, "LEVEL", val, unit, lvl && !t->p[P_MUTE] ? VAL(0u) : T_DIM, (int32_t)lvl * 1000 / 127, ICON_AUTO);
        param_format(&TP[P_PAN], t->p[P_PAN], val, &unit);
        card_mot_of(&TSEL->p[P_PAN]);
        draw_column(1, "PAN", val, unit, VAL(1u), RATIO(&TP[P_PAN], t->p[P_PAN]), param_icon(&TP[P_PAN], t->p[P_PAN]));
        param_format(&TP[P_REV], t->p[P_REV], val, &unit);
        card_mot_of(&TSEL->p[P_REV]);
        draw_column(2, "REV", val, unit, VAL(2u), RATIO(&TP[P_REV], t->p[P_REV]), ICON_AUTO);
        draw_column(3, "MUTE", t->p[P_MUTE] ? "ON" : "OFF", "", t->p[P_MUTE] ? T_ACCENT : VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_BROWSE) {
        uint32_t total, cur = preset_pos(&total);
        char u[8];
        if (cur < total) fmt_int(val, (int32_t)cur + 1);
        else str_cpy(val, total ? "--" : "NONE", 8);   /* (0.6.25: an empty list said "--/0") */
        str_cpy(u, "/", 8);
        fmt_int(u + 1, (int32_t)total);
        if (!total) u[0] = 0;
        draw_column(0, "No.", val, u, VAL(0u), -1, ICON_NONE);
        draw_column(1, "ENG", ENGINES[TSEL->eng_req]->name, "", VAL(1u), -1, engine_icon(ENGINES[TSEL->eng_req]->name));
        draw_column(2, "FAV", preset_favorite() ? "ON" : "OFF", "", VAL(2u), -1, ICON_X_STAR);
        draw_column(3, "LIST", favorites.filter ? "FAV" : "ALL", "", VAL(3u), -1, ICON_X_FOLDER);
        return;
    }
    if (cur_page()->graph == GR_PATS) {                  /* PAT, then LOAD (a GO button) */
        uint32_t n = pat_count(), k = pat_pick();
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        draw_column(0, "PAT", tag, "", VAL(0u), (int32_t)k * 1000 / (int32_t)(n > 1u ? n - 1u : 1u), ICON_X_PATTERN);
        draw_act_column(1, "LOAD", T_THEME, ICON_AUTO);
        draw_column(2, "", "", "", T_THEME, -1, ICON_AUTO);
        draw_column(3, "", "", "", T_THEME, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLOTS) {                 /* (0.6.12) PROJECT, SECT A-D, then LOAD and SAVE */
        uint32_t k = (uint32_t)song.g[G_SLOT] - 1u;
        int used = graph_project_used(k);
        fmt_int(val, (int32_t)song_cur + 1);
        draw_column(0, "PROJ", val, "", VAL(0u), (int32_t)song_cur * 1000 / (int32_t)(NSONG - 1u), ICON_X_SONG);
        val[0] = (char)('A' + (k & 3u)); val[1] = 0;
        draw_column(1, "SECT", val, "", VAL(1u), (int32_t)(k & 3u) * 1000 / 3, ICON_AUTO);
        draw_act_column(2, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_USER) {                  /* SLOT, then three GO buttons */
        int used = up_used(ui.uslot);
        up_slot_label(val, ui.uslot);
        draw_column(0, "SLOT", val, "", VAL(0u), (int32_t)ui.uslot * 1000 / (int32_t)(UP_SLOTS - 1u), ICON_AUTO);
        draw_act_column(1, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(2, "ERASE", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLPIT) {                 /* (JIANT 0.6.3) STEP, its PIT, LEN, QNT */
        uint32_t id = G_SLP0 + (sl_ui_step & 15u);
        fmt_int(val, (int32_t)sl_ui_step + 1);
        draw_column(0, "STEP", val, "/16", sl_ui_step < (uint32_t)song.g[G_SLPLEN] ? VAL(0u) : T_DIM,
                    (int32_t)sl_ui_step * 1000 / 15, ICON_AUTO);
        param_format(&GP[id], song.g[id], val, &unit);
        draw_column(1, "PITCH", val, unit, song.g[id] ? VAL(1u) : T_DIM, RATIO(&GP[id], song.g[id]), ICON_AUTO);
        param_format(&GP[G_SLPLEN], song.g[G_SLPLEN], val, &unit);
        draw_column(2, "LEN", val, unit, VAL(2u), RATIO(&GP[G_SLPLEN], song.g[G_SLPLEN]), ICON_AUTO);
        param_format(&GP[G_SLPQ], song.g[G_SLPQ], val, &unit);
        draw_column(3, "QUANT", val, unit, VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_MSEQ) {                  /* (JIANT) STEP, its LVL, LEN, SLEW */
        const track_t *t = TSEL;
        uint32_t id = P_MS0 + (ms_ui_step & 15u);
        fmt_int(val, (int32_t)ms_ui_step + 1);
        draw_column(0, "STEP", val, "/16", ms_ui_step < (uint32_t)t->p[P_MSLEN] ? VAL(0u) : T_DIM,
                    (int32_t)ms_ui_step * 1000 / 15, ICON_AUTO);
        param_format(&TP[id], t->p[id], val, &unit);
        draw_column(1, "LEVEL", val, unit, VAL(1u), RATIO(&TP[id], t->p[id]), ICON_LEVEL);
        param_format(&TP[P_MSLEN], t->p[P_MSLEN], val, &unit);
        draw_column(2, "LEN", val, unit, VAL(2u), RATIO(&TP[P_MSLEN], t->p[P_MSLEN]), ICON_AUTO);
        param_format(&TP[P_MSSLW], t->p[P_MSSLW], val, &unit);
        draw_column(3, "SLEW", val, unit, t->p[P_MSSLW] ? VAL(3u) : T_DIM, RATIO(&TP[P_MSSLW], t->p[P_MSSLW]), ICON_GLIDE);
        return;
    }
    if (fop_page(cur_page()->graph)) {                   /* (0.6.10) FM6: OP, then three of its bytes (ui_fm6op.c) */
        for (c = 0; c < 4u; c++) {
            const char *lb, *u;
            int32_t r;
            uint32_t ic;
            int on;
            if (!fop_ok()) {                             /* (the engine changed before ui_draw left the page) */
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
                continue;
            }
            on = fop_card(c, &lb, val, &u, &r, &ic);
            draw_column(c, lb, val, u, on ? VAL(c) : T_DIM, r, ic);
        }
        return;
    }
    if (cur_page()->graph == GR_MOD) {                   /* SLOT, then that slot's SRC DST AMT */
        const track_t *t = TSEL;
        uint32_t id = P_M1SRC + 3u * mod_ui_slot;
        int32_t s = t->p[id], d = t->p[id + 1u], a = t->p[id + 2u];
        fmt_int(val, (int32_t)mod_ui_slot + 1);
        draw_column(0, "SLOT", val, "/4", VAL(0u), (int32_t)mod_ui_slot * 1000 / 3, mod_src_icon(MS_OFF));   /* (the mod icon) */
        draw_column(1, "SRC", N_MSRC[clamp(s, 0, MS_N - 1)], "", s ? VAL(1u) : T_DIM, -1, mod_src_icon(s));
        draw_column(2, "DEST", mod_dst_name(t, d), "", d ? VAL(2u) : T_DIM, -1, mod_dst_icon(t, d));
        param_format(&TP[id + 2u], a, val, &unit);
        draw_column(3, "AMT", val, unit, a ? VAL(3u) : T_DIM, RATIO(&TP[id + 2u], a), mod_src_icon(MS_OFF));
        return;
    }
    if (cur_page()->graph == GR_ROLL && lock_held()) {   /* a step held: what KNOB 1..4 lock (ui_input.c lock_turn) */
        uint64_t held = lock_held();
        uint32_t st = 0;
        while (!((held >> st) & 1u))
            st++;                                       /* (the first step held: its values) */
        for (c = 0; c < 4u; c++) {
            uint32_t id = lock_id(c);
            const param_desc_t *d;
            int16_t v;
            int on;
            if (id == 0xFFu) {
                draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
                continue;
            }
            d = track_desc(TSEL, id);
            on = motion_lock_get(TSEL, st, id, &v);
            if (!on)
                v = motion_base_value(TSEL, id);      /* not locked: the sound's own, DIM */
            param_format(d, v, val, &unit);
            draw_column(c, d->label, val, unit, on ? VAL(c) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        }
        return;
    }
    if (cur_page()->scope == SC_STEP && drum_track(TSEL)) {   /* the grid: STEP LANE HIT ACC */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t b = 1u << ui.lane, on = (step_lanes(st) & b) != 0u, ac = (step_accents(st) & b) != 0u;
        char sn[8], sl[8];
        fmt_int(sn, (int32_t)ui.cursor + 1);
        str_cpy(sl, "/", 8);
        fmt_int(sl + 1, TSEL->p[P_SLEN]);
        draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
        draw_column(1, "LANE", drum_lane_name(TSEL, ui.lane), "", VAL(1u), -1, ICON_AUTO);
        draw_column(2, "HIT", on ? "ON" : "--", "", on ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "ACC", ac ? "ON" : "--", "", ac ? VAL(3u) : T_DIM, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_STEP && cur_page()->id[0] == 8u) {   /* (0.6.88) SEQ > TIMING: STEP OFFSET REC Q */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        int32_t m = ST_MICRO(st), on = st->time == ST_NOTE && (st->n || st->hit), q = TSEL->p[P_HRQ];
        char sn[8], mo[10], rq[8];
        fmt_int(sn, (int32_t)ui.cursor + 1);
        if (!m) str_cpy(mo, on ? "GRID" : "--", sizeof mo);
        else { mo[0] = m < 0 ? '-' : '+'; fmt_int(mo + 1, m < 0 ? -m : m); str_cpy(mo + str_len(mo), "/16", 4); }
        fmt_int(rq, (q * 100 + 63) / 127);
        draw_column(0, "STEP", sn, "", VAL(0u), -1, ICON_AUTO);
        draw_column(1, "OFFSET", mo, "", on && m ? VAL(1u) : T_DIM, on ? (m + 8) * 1000 / 15 : -1, ICON_ORDER);
        draw_column(2, "REC Q", rq, "%", q < 127 ? VAL(2u) : T_DIM, q * 1000 / 127, ICON_X_PATTERN);
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
        return;
    }
    if (cur_page()->scope == SC_STEP && cur_page()->id[0] == 4u) {   /* (0.6.81) SEQ > NOTE: FOCUS PITCH MOVE LEN */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t on = st->time == ST_NOTE && st->n, f = on && ui.pr_focus <= st->n ? ui.pr_focus : 0u, l = 1, len;
        char fo[8], pn[8], sn[8], ln[8];
        len = TSEL->p[P_SLEN] > 0 ? (uint32_t)TSEL->p[P_SLEN] : 1u;
        while (on && l < len && seq_steps(TSEL)[(ui.cursor + l) % len].time == ST_TIE) l++;
        if (!f) str_cpy(fo, on && st->n > 1u ? "CHORD" : "ALL", 8);
        else { str_cpy(fo, "NOTE ", 8); fmt_int(fo + 5, (int32_t)f); }
        if (on) note_name(pn, st->note[f ? f - 1u : 0u]); else str_cpy(pn, "--", 8);
        fmt_int(sn, (int32_t)ui.cursor + 1);
        fmt_int(ln, (int32_t)l);
        draw_column(0, "FOCUS", fo, "", on ? VAL(0u) : T_DIM, -1, ICON_AUTO);
        draw_column(1, "PITCH", pn, "", on ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        draw_column(2, "MOVE", sn, "<>", on ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "LEN", on ? ln : "--", on ? "ST" : "", on ? VAL(3u) : T_DIM, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        char u[8];
        uint32_t cnt = st->n, first = st->note[0], l;
        for (l = NLANE; l-- > 0;)                         /* (lane hits count as their notes) */
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n)
                    first = DRUM_LANE_NOTE[l];
            }
        if (cnt) {
            note_name(val, first);
            u[0] = 0;
            if (cnt > 1) {
                str_cpy(u, "+", 8);
                fmt_int(u + 1, (int32_t)cnt - 1);
            }
        } else {
            str_cpy(val, "--", 12);
            u[0] = 0;
        }
        {
            static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
            char sn[8], sl[8];
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
            draw_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : T_DIM, -1, ICON_AUTO);
            draw_column(2, "TIME", TIME_N[st->time % 3u], "", VAL(2u), -1, ICON_AUTO);
            draw_column(3, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "",
                        VAL(3u), -1, ICON_AUTO);
        }
        return;
    }
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (!d || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_MIDI && cur_page()->scope == SC_GLOBAL) {
            str_cpy(val, !usb.up ? "OFF" : usb.config ? "MIDI" : usb.setups ? "ENUM" : usb.sof_seen ? "BUS" : "WAIT", 12);
            unit = "USB";
            draw_column(c, "USB", val, unit, T_THEME, -1, ICON_AUTO);
            continue;
        }
        if ((act_cols() >> c) & 1u) {
            draw_act_column(c, d->label, T_THEME, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_INFO && cur_page()->scope == SC_GLOBAL) {
            fmt_int(val, (int32_t)(song.cpu_q8 * 100u / 256u));
            unit = "%";
        } else {
            param_format(d, *vp, val, &unit);
            if (sys_load && cur_page()->scope == SC_GLOBAL &&   /* (0.6.29) LOAD LITE / ECO swaps it while it sounds */
                ((cur_page()->id[c] == G_RTYPE && REV_LITE[sys_load % 3u][*vp <= 4 ? *vp : 0] != *vp) ||
                 ((cur_page()->id[c] == G_DPIT || cur_page()->id[c] == G_DSPRY) && *vp)))
                unit = N_LOAD[LOAD_ECO - sys_load % 3u];
            if (sys_load == LOAD_ECO && cur_page()->scope == SC_TRACK && cur_page()->id[c] == P_CHOR && *vp)
                unit = "ECO";                           /* (0.6.30) no chorus: the send goes nowhere */
        }
        card_mot_of(vp);                                /* (a global: never) */
        draw_column(c, d->label, val, unit, VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, enum_rank(d, *vp)),
                    param_icon(d, *vp));
    }
}


/* The power-on splash (main.c fm1_main, web/emu felucca_web.c; JIANT 0.5): the maker's signature ("Jiant FM1",
 * tools/gen_signature.py: 24 strokes, 214 points) written in the thermal colours, stroke by stroke as the hand went:
 * the line cold (cyan) where the pen started, hotter along its way (violet, red, orange) to yellow white where it
 * ended, the pen's tip a white point while it writes; under it, small, the version and "based on Felucca". Written over
 * SPL_WRITE ms (splash_write, main.c's splash loop; MENU > ANIM OFF: whole at once). Lines only, the canvas
 * redrawn a step (~700 short lines). In the saved palette (settings_init before it) */
#include "ui_signature.h"
#define SPL_WRITE 800u                              /* ms: the signature written (was 900; 0.6.51: 600 -> 800) */
#define SPL_HOLD 900u                               /* .. then shown whole, before the UI (was 1100; 0.6.51: 500 -> 900) */
#define SPL_SY 40                                   /* the signature's canvas on the screen (240 x SIG_H + 12) */
#define SPL_TY 186                                  /* the two lines under it */
static const char *const SPLASH_LINES[] = {FELUCCA_VERSION, "QUANTUM GAMMAWAVE // QG-1"};
static uint8_t spl_anim;                            /* main.c: 1 = the signature to be written (splash_write) */
static int32_t sig_seg(uint32_t i)                  /* segment i -> i + 1's length, px (0: a stroke starts at i + 1) */
{
    int32_t dx = SIG_PTS[2u * i + 2u] - SIG_PTS[2u * i], dy = (SIG_PTS[2u * i + 3u] & 127) - (SIG_PTS[2u * i + 1u] & 127);
    if (SIG_PTS[2u * i + 3u] & 128u)                /* (y's bit 7: a stroke starts) */
        return 0;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return dx > dy ? dx + dy / 2 : dy + dx / 2;
}
static int32_t sig_len(void)
{
    static int32_t n;
    uint32_t i;
    if (!n)
        for (i = 0; i + 1u < SIG_N; i++)
            n += sig_seg(i);
    return n;
}
/* (0.5.2) the stroke point j of the signature, smoothed (1 4 6 4 1 over its neighbours inside its stroke [s, e],
 * the ends kept), Q4 */
static void sig_pt(uint32_t j, uint32_t s, uint32_t e, int32_t *x, int32_t *y)
{
    static const uint8_t W[5] = {1, 4, 6, 4, 1};
    int32_t k, sx = 0, sy = 0;
    if (j == s || j == e) {
        *x = SIG_PTS[2u * j] * 16;
        *y = (SIG_PTS[2u * j + 1u] & 127) * 16;
        return;
    }
    for (k = -2; k <= 2; k++) {
        int32_t i = (int32_t)j + k;
        uint32_t m = (uint32_t)(i < (int32_t)s ? (int32_t)s : i > (int32_t)e ? (int32_t)e : i);
        sx += W[k + 2] * SIG_PTS[2u * m];
        sy += W[k + 2] * (SIG_PTS[2u * m + 1u] & 127);
    }
    *x = sx;                                            /* (/16 x 16: Q4) */
    *y = sy;
}
/* Bold geometric block glyph renderer for "GAMMAWAVE" in StandaloneLogoComponent (no font hook side-effects) */
static void gw_logo_letter(char ch, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t t = 3, mw = w / 2;
    if (ch == 'G') {
        cv_rect(x, y, w, t, c);
        cv_rect(x, y, t, h, c);
        cv_rect(x, y + h - t, w, t, c);
        cv_rect(x + w - t, y + h / 2, t, h - h / 2, c);
        cv_rect(x + mw, y + h / 2, w - mw, t, c);
    } else if (ch == 'A') {
        cv_rect(x, y + t, t, h - t, c);
        cv_rect(x + w - t, y + t, t, h - t, c);
        cv_rect(x + t, y, w - 2 * t, t, c);
        cv_rect(x + t, y + h / 2, w - 2 * t, t, c);
    } else if (ch == 'M') {
        cv_rect(x, y, t, h, c);
        cv_rect(x + w - t, y, t, h, c);
        cv_line_t(x + t, y + 1, x + mw, y + h / 2, c, 2);
        cv_line_t(x + w - t - 1, y + 1, x + mw, y + h / 2, c, 2);
    } else if (ch == 'W') {
        cv_rect(x, y, t, h, c);
        cv_rect(x + w - t, y, t, h, c);
        cv_line_t(x + t, y + h - 2, x + mw, y + h / 2, c, 2);
        cv_line_t(x + w - t - 1, y + h - 2, x + mw, y + h / 2, c, 2);
    } else if (ch == 'V') {
        cv_line_t(x + 1, y, x + mw, y + h - 2, c, 3);
        cv_line_t(x + w - 2, y, x + mw, y + h - 2, c, 3);
    } else if (ch == 'E') {
        cv_rect(x, y, t, h, c);
        cv_rect(x, y, w, t, c);
        cv_rect(x, y + h / 2 - 1, w - 2, t, c);
        cv_rect(x, y + h - t, w, t, c);
    }
}

/* Authentic POCKET GAMMAWAVE // GW-1 Wave-Surfing Boot Splash:
 *   - Chamfered Octagonal GammaWave Frame with Top-Left #f27121 Orange Bracket
 *   - Upper Bay: THE HEARTH Blazing Yellow-White Solar Core + Radial Flare Corona + Spark Particles + Orbiting Moon
 *   - Lower Bay: Full 9-letter "GAMMAWAVE" typography physically surfing/undulating along the glowing 3-cycle sine wave */
static void splash_sig(int32_t px, uint32_t ms)
{
    static const char *const WORD = "GAMMAWAVE";
    int32_t tot = sig_len(), prog = px >= tot ? 256 : px * 256 / (tot ? tot : 1);
    int mono = ux.mono || settings.palette == UI_BW_INDEX;
    int32_t i, wx0 = 18, ww = 204, wy = 104, w_draw = ww * prog / 256;
    int32_t py = wy;
    (void)sig_pt;
    cv_begin(240, SIG_H + 12, T_BG);

    /* 1. Chamfered Octagonal GammaWave Logo Frame with Top-Left #f27121 Bracket */
    gw_oct_panel(8, 2, 224, SIG_H + 8, 6, mono ? T_BG : T_SURF, T_LINE, 1, mono ? T_TEXT : T_THEME);

    /* 2. Upper Bay: THE HEARTH Blazing Yellow-White Sun + Orbiting Satellite Moon */
    if (!mono) {
        gw_draw_hearth(120, 39, 15, 200 * prog / 256, 224, 196, ms * 24u);
    } else {
        gw_orbit_ring(120, 39, 30, 18, T_DIM, 1);
        gw_disc(120, 39, 12, T_TEXT);
    }
    /* 2b. (QG-1) the quantum shell: two electron orbits around the Hearth, written in with the signature */
    if (prog > 64) {
        static const int8_t ORB[2][2] = {{46, 11}, {22, 30}};
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            uint32_t ph = ms * (k ? 41u : 29u) + k * 21845u;
            int32_t ex = 120 + (bg_sin(ph) * ORB[k][0] >> 15), ey = 39 + (bg_sin(ph + 16384u) * ORB[k][1] >> 15);
            gw_orbit_ring(120, 39, ORB[k][0], ORB[k][1], mono ? T_DIM : T_LINE, 1);
            gw_disc(ex, ey, 2, mono ? T_TEXT : k ? T_ACCENT : T_THEME);
        }
    }
    cv_rect(18, 72, 204, 1, T_LINE);

    /* 3. Lower Bay: Glowing Triple-Stroked #f27121 Orange & Yellow 3-Cycle Sine Wave across full bay */
    for (i = 0; i <= w_draw; i++) {
        uint32_t ph = (uint32_t)(i * 3 * 65536 / ww) - ms * 22u;
        int32_t y = wy - (bg_sin(ph) * 8 >> 15);
        if (i) {
            if (!mono) {
                /* Outer warm amber glow */
                cv_line(wx0 + i - 1, py - 2, wx0 + i, y - 2, bg_col(84));
                cv_line(wx0 + i - 1, py + 2, wx0 + i, y + 2, bg_col(84));
                /* Mid #f27121 vibrant orange wave */
                cv_line(wx0 + i - 1, py - 1, wx0 + i, y - 1, bg_col(176));
                cv_line(wx0 + i - 1, py + 1, wx0 + i, y + 1, bg_col(176));
                /* Inner hot yellow-white core */
                cv_line(wx0 + i - 1, py, wx0 + i, y, bg_col(246));
            } else {
                cv_line_t(wx0 + i - 1, py - 1, wx0 + i, y - 1, T_TEXT, 2);
            }
        }
        py = y;
    }

    /* 4. Wave-Surfing "GAMMAWAVE" Typography: each letter rides the phase of the live sine wave */
    for (i = 0; i < 9; i++) {
        int32_t lx = 22 + i * 22;
        if (lx <= wx0 + w_draw) {
            uint32_t lph = (uint32_t)((lx - wx0) * 3 * 65536 / ww) - ms * 22u;
            int32_t surf_y = 85 - (bg_sin(lph) * 6 >> 15);
            uint16_t lc = mono ? T_TEXT : (i < 5 ? T_TEXT : bg_col(210 + i * 5));
            gw_logo_letter(WORD[i], lx, surf_y, 16, 18, lc);
        }
    }

    /* 5. Floating White/Gold Spark Particles around the Sine Wave */
    for (i = 0; i < 10; i++) {
        int32_t sx = wx0 + ((i * 21 + (int32_t)(ms / 24u)) % ww);
        int32_t sy = wy + (((i * 29) % 24) - 12);
        if (sx <= wx0 + w_draw)
            cv_pset(sx, sy, mono ? T_TEXT : (i & 1) ? T_TEXT : bg_col(224));
    }

    cv_blit(0, SPL_SY);
}
static void draw_splash(void)
{
    uint32_t k;
    lcd_fill(0, 0, 240, 240, T_BG);
    cv_begin(240, 36, T_BG);
    for (k = 0; k < 2u; k++)
        cv_text_in(0, (int32_t)k * 16, 240, &AF_S, SPLASH_LINES[k], k ? T_DIM : T_MID, T_BG);
    cv_blit(0, SPL_TY);
    splash_sig(spl_anim && !(ui_prefs & PREF_ANIM_OFF) ? 0 : 0x7FFFFFFF, SPL_WRITE);
    lcd_sync();
}
/* main.c's splash loop: the signature as written ms after the start */
static void splash_write(uint32_t ms)
{
    if (!spl_anim || (ui_prefs & PREF_ANIM_OFF))
        return;
    splash_sig(ms >= SPL_WRITE ? 0x7FFFFFFF : (int32_t)((int64_t)sig_len() * ms / SPL_WRITE), ms);
    lcd_sync();
}

/* UPDATE MODE countdown (main.c: OCT- + OCT+ held): over everything, the menu and the dialogs too */
static void draw_uboot(void)
{
    static uint8_t shown;
    char d[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
    if (!ui.force && shown == ui.uboot)
        return;
    shown = ui.uboot;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 76, 240, &AF_M, "UPDATE MODE IN", T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_L, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_ink(KC_OCTUP, "LET GO TO CANCEL"), x = HALF_UP(240 - w);
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(3), "update-mode keys centred");
        x = cv_keycap(x, 1, KC_OCTDN, T_KEY, T_INK, T_BG) + 3;
        cv_key_hint(x, 1, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 149);
        lcd_sync();
    }
    ui.force = 0;
}

/* the count-in (seq.c, MENU > COUNT-IN; 1.1, Discussion #131): over the page until the sequencer starts. Its bar's four
 * beats counted down, 4 3 2 1, in four cells (the beat now filled THEME, those gone dim), with 2 BARS which bar it is;
 * PLAY stops it. Drawn again on each beat (seq.c's state; the header stays live) */
#define CI_Y 88                                       /* the cells: 4 x 48 px, 8 px apart, 72 tall */
#define CI_H 72
static void draw_countin(void)
{
    static uint32_t shown;
    uint32_t total = cin_total, e = total - cin_left, j, sig = total * 64u + e + 1u;
    char sub[16];
    if (!ui.force && shown == sig)
        return;
    shown = sig;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 44, 240, &AF_M, "COUNT-IN", T_TEXT, 1);
    if (total > 4u) {
        str_cpy(sub, "BAR 1 OF 2", sizeof sub);
        sub[4] = (char)('1' + e / 4u % 9u);
        sub[9] = (char)('0' + total / 4u % 10u);
    } else
        str_cpy(sub, "1 BAR", sizeof sub);
    draw_text_box(0, 66, 240, &AF_S, sub, T_MID, 1);
    cv_begin(216, CI_H, T_BG);
    for (j = 0; j < 4u; j++) {
        int32_t x = (int32_t)j * 56;
        char d[2] = {(char)('4' - j), 0};
        int now = j == e % 4u, gone = j < e % 4u;
        uint16_t fill = now ? T_THEME : T_SURF;
        if (ux.style && !now) {                       /* LINE: no SURF card, a 1 px rule round it */
            cv_rrect(x, 0, 48, CI_H, 8, T_RULE, T_BG);
            cv_rrect(x + 1, 1, 46, CI_H - 2, 7, T_BG, T_RULE);
        } else
            cv_rrect(x, 0, 48, CI_H, 8, fill, T_BG);
        GFX_HOOK_ALIGN(x, 0, x + 48, CI_H, AL_HV, "count-in beat centred in its cell");
        cv_text_in(x, CAP_IN(L, CI_H), 48, &AF_L, d, now ? T_INK : gone ? T_DIM : T_TEXT, fill);
    }
    cv_blit(12, CI_Y);
    draw_text_box(0, CI_Y + CI_H + 14, 240, &AF_S, "RECORDING STARTS AFTER 1", T_MID, 1);
    {   /* PLAY stops it */
        int32_t x = HALF_UP(240 - kh_ink(KC_PLAY, "STOP"));
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "count-in key centred");
        cv_key_hint(x, 1, KC_PLAY, "STOP", 1, T_BG);
        cv_blit(0, 206);
        lcd_sync();
    }
    ui.force = 0;
}

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines, on a surface */
#define DLG_X 16
#define DLG_Y 62
#define DLG_W 208
#define DLG_H 116
static void confirm_text(char *a, char *b)
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_PROJ:
        str_cpy(a, "OVERWRITE PROJECT A?", 24);
        a[18] = (char)('A' + (k & 3u));
        if (!project_name(k & 3u, b) || !b[0])          /* the project's name, else what SONG plays from it */
            str_cpy(b, "SONG PATTERN CHANGES", 24);
        break;
    case CF_DEL_ROW:
        str_cpy(a, "DELETE SONG ROW?", 24);
        break;
    case CF_CLEAR_SONG:
        str_cpy(a, "CLEAR SONG ORDER?", 24);
        break;
    case CF_CLEAR_PROJ:                              /* (1.2.1-GW, issue #22) SAVE > TOOLS 7 */
        str_cpy(a, "CLEAR PROJECT 1?", 24);
        a[14] = (char)('1' + song_cur % 9u);
        str_cpy(b, "A-D + SONG. NO UNDO", 24);
        break;
    case CF_INIT_SOUND:
        str_cpy(a, drum_track(TSEL) ? "INIT SOUND + KIT?" : "INITIALIZE SOUND?", 24);
        break;
    case CF_CLEAR_ALL:
        str_cpy(a, "CLEAR ALL SEQUENCES?", 24);
        break;
    case CF_CPU_TEST:                                /* (0.6.83) SAVE > TOOLS 6 */
        str_cpy(a, "BUILD CPU TEST SONG?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_INIT_SYS:                                /* (0.6.40) SAVE > TOOLS 5 */
        str_cpy(a, "NEW PROJECT?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_DICE_SOUND:                              /* (0.6.65) EDIT + SELECT: heard already */
    case CF_DICE_SEQ:                                /* (0.6.67) SEQ / LFO / FX + SELECT */
    case CF_DICE_LFO:
    case CF_DICE_FX:
        str_cpy(a, ui.confirm == CF_DICE_SEQ ? "KEEP DICED SEQ?" : ui.confirm == CF_DICE_LFO ? "KEEP DICED LFO?" :
                   ui.confirm == CF_DICE_FX ? "KEEP DICED FX?" : drum_track(TSEL) ? "KEEP DICED KIT?" : "KEEP DICED SOUND?", 24);
        str_cpy(b, "SELECT: DICE AGAIN", 24);
        break;
    case CF_DICE_ALL:                                /* (0.6.21) HOME + SELECT */
        str_cpy(a, "DICE EVERYTHING?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_RECALL_SOUND:
        str_cpy(a, drum_track(TSEL) ? "RECALL SOUND + KIT?" : "RECALL SAVED SOUND?", 24);
        break;
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 AUTOMATION?", 24); a[7] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_USER:
        str_cpy(a, "OVERWRITE ", 24);
        up_slot_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        up_name(k, b);                               /* the sound stored there */
        break;
    case CF_ERASE_USER:
        str_cpy(a, "ERASE ", 24);
        up_slot_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        up_name(k, b);
        break;
    case CF_LOAD_PAT: {
        char tag[4];
        str_cpy(a, "REPLACE T1 SEQUENCE?", 24);
        a[9] = (char)('1' + k % NTRK);
        pat_label(pat_pick(), tag, b);               /* with the pattern picked */
        break;
    }
    default:
        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);
        break;
    }
}
static void draw_gen(void);                             /* (0.6.91) gen.c: the GEN screen */
static void draw_confirm(void)
{
    char a[24], b[24];
    const aafont_t *tf = &AF_M;
    int dk = ui.confirm >= CF_DICE_SOUND && ui.confirm <= CF_DICE_FX;
    const char *no = dk ? "CANCEL" : "NO", *yes = dk ? "OK" : "YES";
    if (ui.confirm == CF_DICE_ALL) { draw_gen(); return; }
    confirm_text(a, b);
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    cv_begin(DLG_W, DLG_H, T_BG);                     /* a SURF card: warning, the question, the detail, two buttons */
    if (ux.style)                                     /* LINE: no card, a 1 px rule round it */
        cv_rrect(0, 0, DLG_W, DLG_H, 8, T_RULE, T_BG);
    cv_rrect(!!ux.style, !!ux.style, DLG_W - 2 * !!ux.style, DLG_H - 2 * !!ux.style, 8 - !!ux.style, T_SURF,
             ux.style ? T_RULE : T_BG);
    GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog icon centred");
    cv_icon_in(0, 12, DLG_W, 0, 16, ui.confirm == CF_CLEAR_MOTION ? ICON_X_MOTION_DEL : ICON_X_WARN, T_ACCENT, T_SURF);
    if (text_w(tf, a) > DLG_W - 16)
        tf = &AF_S;
    GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog question centred");
    cv_text_in(0, 34, DLG_W, tf, a, T_TEXT, T_SURF);
    if (b[0]) {
        char f[32];
        text_fit(f, sizeof f, b, &AF_S, DLG_W - 16);    /* a name: free text */
        GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog detail centred");
        cv_text_flags(ink_in(&AF_S, f, DLG_W), 56, &AF_S, f, T_MID, T_SURF, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    cv_rrect(10, 80, 90, 26, 6, T_RAISE, T_SURF);     /* OCT-: NO */
    GFX_HOOK_ALIGN(10, 80, 100, 106, AL_HV | AL_N(2), "dialog button NO");
    cv_key_hint(10 + HALF_UP(90 - kh_ink(KC_OCTDN, no)), 80 + HALF_UP(26 - KC_H), KC_OCTDN, no, 1, T_RAISE);   /* (ink centred) */
    cv_rrect(108, 80, 90, 26, 6, T_THEME, T_SURF);    /* OCT+: YES (on the THEME fill: an INK keycap, THEME label) */
    {
        int32_t x = 108 + HALF_UP(90 - kh_ink(KC_OCTUP, yes));
        GFX_HOOK_ALIGN(108, 80, 198, 106, AL_HV | AL_N(2), "dialog button YES");
        x = cv_keycap(x, 80 + HALF_UP(26 - KC_H), KC_OCTUP, T_INK, T_THEME, T_THEME);
        cv_text_on(x + KH_GAP, 80 + HALF_UP(26 - KC_H) - 1, &AF_S, yes, T_INK, T_THEME);
    }
    cv_blit(DLG_X, DLG_Y);
}

/* the stored STYLE (ui_style: the menu, a settings import, a restore) into gfx.c's copy: the tokens again, all redrawn */
static void style_apply(void)
{
    if (ui_style > ST_LINE)                             /* 2, the retired PIXEL: LINE; unknown: FLAT */
        ui_style = ui_style == 2u ? ST_LINE : ST_FLAT;
    if (ux.style != ui_style) {
        ux.style = ui_style;
        palette_set(settings.palette);
        ui.force = 1;
    }
}

static void ui_draw_page(uint32_t counting);
static void ui_draw(void)
{
    static uint8_t counting;
    style_apply();
    ui.frame++;
    if (counting != (uint8_t)seq_counting()) {          /* a count-in began or ended: the page drawn again after it */
        counting = (uint8_t)seq_counting();
        ui.force = 1;
    }
    if (!scr_frame())                                   /* MENU > SCREEN OFF: dark (or waking), nothing drawn */
        return;
    ui_draw_page(counting);
    scr_shown();
}
static void ui_draw_page(uint32_t counting)
{
    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.menu) {
        draw_menu();
        ui.force = 0;
        return;
    }
    if (counting && !ui.confirm && !name_on()) {        /* the count-in: over the page (and a layer) */
        draw_countin();
        draw_head();
        return;
    }
    if (ui.confirm) {                                   /* the OCT- / OCT+ dialog */
        if (ui.force) {
            draw_confirm();
            ui.force = 0;
        }
        draw_head();                                    /* recording remains visible over confirmations */
        return;
    }
    if (name_on()) {                                    /* NAME: a user preset's or a project's (ui_name.c) */
        name_draw();
        return;
    }
    if (ui.layer || mac_latch) {                        /* a layer's map over the page (ui_layer.c); (JIANT 0.4) the MACRO
                                                         * layer latched: its cards (no layer open) */
        uint8_t hm = !ui.layer;
        static uint8_t hm_was;
        if (hm != hm_was)
            ui.force = 1, hm_was = hm;
        if (hm)
            ui.layer = LAYER_MACRO;
        if (ui.force)
            draw_frame();
        draw_head();
        draw_layer();
        if (hm)
            ui.layer = 0;
        if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
            str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
            ui.msg2[0] = 0;
            ui.msg_t = 60;
        }
        if (ui.hot_t)
            ui.hot_t--;
        if (ui.bpm_t)
            ui.bpm_t--;
        ui.force = 0;
        return;
    }
    if (!ui.home && !page_visible(ui.page)) {          /* an OP page of a track that is not DIGITAL (without
                                                         * FELUCCA_FM4: any track): EDIT 1 */
        ui.page = (uint8_t)page_first(FAM_EDIT);
        page_entered();
    }
    cursor_fix();
    if (ui.force)
        draw_frame();
    felucca_dbg.stage = 3;
    draw_head();
    felucca_dbg.stage = 4;
    draw_columns();
    felucca_dbg.stage = 5;
    draw_graph();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {     /* the second message (ui_notices) */
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    felucca_dbg.stage = 6;
    if (pf_on)                                          /* (0.6.61) the page's name for a moment; (0.6.64) over the
                                                         * footer, which says the same: the header, the cards and
                                                         * the graph stay in sight */
        page_flash_draw();
    if (!pf_on)                                         /* (its end: the footer back at once) */
        draw_foot();
    ui.force = 0;
}
