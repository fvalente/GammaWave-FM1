/* SPDX-License-Identifier: GPL-3.0-only
 * (1.0-GW) TAKES: the generator's reel. Every piece made (OCT+, a take, DICE ALL) goes on it as its recipe — the
 * settings that remake it exactly (the generator is deterministic) — with what its card shows: the genre and the trap
 * flavour, the seed, the tempo, the key, each live track's first two bars as a thumbnail and the song's sections.
 * PRESETS on the GEN screen opens the reel and scrolls it; while the music plays the take under the cursor becomes the
 * music once PRESETS rests (gen_take_poll), stopped OCT+ loads it; SAVE keeps a take (kept ones never drop off: the
 * reel holds the last 32). A recipe is not a recording: edits made after are not on it, SAVE keeps those.
 * (TAKES v2) A take has an ID of its own (#0001 .. #FFFF, never one still on the reel: a take's ID stays its ID when
 * others go) and its parent's: a take made from another (a part, a sound, a section made again, a setting changed) is
 * its child, the card says from which and what changed ("<0A3B DRUMS"); a new seed starts a family. EDIT deletes the
 * take under the cursor (OCT+ yes, OCT- no), EDIT held the takes not kept. Included by gen.c (after dice_print). */

static uint16_t heat_col(int32_t v);                    /* (ui_graph.c) */
/* (v2) the GEN screen's pop-up: what a control just did, big, in the middle of the screen for 1.6 s (a label, a
 * value); a take waiting for the next bar: the bar's progress under it */
static struct { char lbl[24], val[24]; uint32_t ms; uint8_t on; } gpop;
static void reel_hex(char *b, uint32_t id);
static void gen_pop(const char *lbl, const char *val)
{
    str_cpy(gpop.lbl, lbl, sizeof gpop.lbl);
    str_cpy(gpop.val, val, sizeof gpop.val);
    gpop.ms = (fm1_ms ? fm1_ms : 1u);
    gpop.on = 1;
    ui.force = 1;
}
#define REEL_N 32u
#define CODE_MAX 28u                                    /* (v2) a share code's characters, at most */
static uint8_t gen_fresh;                               /* (gen.c: the GEN screen drawn anew) */
#define GEN_VERSION 2u                                  /* (TAKES v2) the generator a recipe was made with: a recipe is
                                                         * remade exactly by the same generator only */
typedef struct {                                        /* the recipe: what gen_make reads (and the tracks it ran on) */
    uint16_t seed;
    uint8_t genre, energy, form, complex, swing, human, tempo, key, scale, length, space, fills, wet, grit, nlive;
    uint8_t amt[GP_N];
    uint8_t var[SL_N], svar[RL_N], secvar[3][SL_PARTS], arr;
    struct { uint16_t seed; uint8_t what, var; } pin[GP_PINS];
} gset_t;
typedef struct {
    gset_t set;
    uint16_t id, parent;                                /* (v2) the take's ID, its parent's (0: a family's first) */
    char name[14];                                      /* the genre as played (a RANDOM throw's, a hybrid's) */
    uint8_t flav, root, scl, nl, keep, nrow;            /* flav: the trap flavour + 1 (0: none) */
    uint8_t how, gv;                                    /* (v2) what changed from the parent (GH_*), the generator */
    int16_t bpm;
    uint32_t thumb[6];                                  /* a live track's first 32 steps: bit i, a note or hit */
    uint8_t role[6];
    uint8_t row[12][2];                                 /* the song's rows: section, bars */
} take_t;
static uint32_t take_code(const gset_t *g, char *out);
static take_t reel[REEL_N] __attribute__((section(".pool")));   /* (zeroed at boot) */
static uint8_t reel_n, reel_cur, reel_play = 0xFF;      /* takes, the cursor, the take playing now (0xFF: none) */
static int8_t reel_want = -1;                           /* a take to load once PRESETS rests (playing) */
static uint16_t reel_next = 1;                          /* (v2) the next ID */
static uint8_t reel_ask;                                /* (v2) a question on the card: 1 delete this take, 2 the unkept */
static const char *const REEL_FLAV[7] = {"ATLANTA", "PI'ERRE", "METRO", "DRILL", "MEMPHIS", "WHOLE LOTTA RED", "MUSIC"};
static const char *const REEL_KEY[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
/* what made a take from its parent (gen_how, set by whoever makes it: the card's "<0A3B DRUMS") */
static const char *const GH_NAME[GH_N] = {"", "", "SETTINGS", "SOUNDS", "DRUMS", "BASS", "CHORDS", "ARP", "LEAD", "TEX",
                                          "KIT", "BASS SND", "PAD SND", "LEAD SND", "ARP SND", "TEX SND", "HARMONY", "MIX",
                                          "FORM", "ENERGY", "KEY", "TEMPO", "SECTION", "BORROW", "VARY"};

static void gset_get(gset_t *s)
{
    uint32_t k;
    memset(s, 0, sizeof *s);
    s->seed = gp.seed; s->genre = gp.genre; s->energy = gp.energy; s->form = gp.form; s->complex = gp.complex;
    s->swing = gp.swing; s->human = gp.human; s->tempo = gp.tempo; s->key = gp.key; s->scale = gp.scale;
    s->length = gp.length; s->space = gp.space; s->fills = gp.fills; s->wet = gp.wet; s->grit = gp.grit;
    s->nlive = (uint8_t)(trk_live < NTRK ? trk_live : NTRK);
    for (k = 0; k < GP_N; k++) s->amt[k] = gp.amt[k];
    memcpy(s->var, gp.var, sizeof s->var);
    memcpy(s->svar, gp.svar, sizeof s->svar);
    memcpy(s->secvar, gp.secvar, sizeof s->secvar);
    s->arr = gp.arr;
    memcpy(s->pin, gp.pin, sizeof s->pin);
}
static void gset_put(const gset_t *s)
{
    uint32_t k;
    gp.seed = s->seed; gp.genre = s->genre; gp.energy = s->energy; gp.form = s->form; gp.complex = s->complex;
    gp.swing = s->swing; gp.human = s->human; gp.tempo = s->tempo; gp.key = s->key; gp.scale = s->scale;
    gp.length = s->length; gp.space = s->space; gp.fills = s->fills; gp.wet = s->wet; gp.grit = s->grit;
    for (k = 0; k < GP_N; k++) gp.amt[k] = s->amt[k];
    memcpy(gp.var, s->var, sizeof gp.var);
    memcpy(gp.svar, s->svar, sizeof gp.svar);
    memcpy(gp.secvar, s->secvar, sizeof gp.secvar);
    gp.arr = s->arr;
    memcpy(gp.pin, s->pin, sizeof gp.pin);
}
static int reel_find(uint16_t id)                       /* a take's place on the reel by its ID (-1: gone) */
{
    uint32_t i;
    for (i = 0; id && i < reel_n; i++)
        if (reel[i].id == id) return (int)i;
    return -1;
}
static uint16_t reel_new_id(void)                       /* the next ID: 0001 .. FFFF, round, never one on the reel */
{
    uint32_t n;
    for (n = 0; n < 65536u; n++) {
        uint16_t id = reel_next;
        reel_next = (uint16_t)(reel_next == 0xFFFFu ? 1u : reel_next + 1u);
        if (id && reel_find(id) < 0) return id;
    }
    return 1;
}
/* take i off the reel: the cursor and the take playing follow */
static void reel_drop(uint32_t i)
{
    uint32_t k = i;
    if (k >= reel_n) return;
    if (reel_play == k) reel_play = 0xFF;
    else if (reel_play != 0xFF && reel_play > k) reel_play--;
    if (reel_cur > k) reel_cur--;
    for (; k + 1u < reel_n; k++) reel[k] = reel[k + 1u];   /* (no memmove here: libc.c) */
    reel_n--;
    reel_want = -1;
    if (reel_cur >= reel_n) reel_cur = (uint8_t)(reel_n ? reel_n - 1u : 0u);
}
/* the piece just made, on the reel (the same recipe again: the cursor on it) */
static void reel_add(void)
{
    gset_t s;
    take_t *t;
    uint32_t i, k, nl = trk_live < NTRK ? trk_live : NTRK;
    uint16_t parent = 0;
    gset_get(&s);
    for (i = 0; i < reel_n; i++)
        if (!memcmp(&reel[i].set, &s, sizeof s)) { reel_cur = reel_play = (uint8_t)i; gen_how = GH_NONE; return; }
    if (gen_how > GH_NEW && reel_play < reel_n && reel[reel_play].set.seed == s.seed)
        parent = reel[reel_play].id;                    /* (a child: made from the take playing, its seed) */
    if (reel_n >= REEL_N) {                             /* full: the oldest not kept goes (all kept: the oldest) */
        for (i = 0; i < REEL_N && reel[i].keep; i++) {}
        reel_drop(i == REEL_N ? 0u : i);
    }
    t = &reel[reel_n];
    memset(t, 0, sizeof *t);
    t->set = s;
    t->id = reel_new_id();
    t->parent = parent;
    t->how = (uint8_t)(parent ? gen_how : GH_NONE);
    t->gv = GEN_VERSION;
    str_cpy(t->name, gs.g->name, sizeof t->name);
    t->flav = (uint8_t)(gs.trap ? tk_tf + 1u : 0u);
    t->root = (uint8_t)(gs.root % 12);
    t->scl = (uint8_t)gs.scale;
    t->bpm = song.g[G_BPM];
    t->nl = (uint8_t)(nl > 6u ? 6u : nl);
    for (k = 0; k < t->nl; k++) {
        const track_t *tr = &trk[k];
        uint32_t len = (uint32_t)clamp(tr->p[P_SLEN], 1, NSTEP);
        for (i = 0; i < 32u; i++)
            if (tr->step[i % len].time == ST_NOTE) t->thumb[k] |= 1u << i;
        t->role[k] = gs.role[k];
    }
    if (live.mode)
        for (i = 0; i < chain_config.count && i < 12u; i++) {
            t->row[i][0] = chain_config.row[i].slot;
            t->row[i][1] = chain_config.row[i].bars;
            t->nrow = (uint8_t)(i + 1u);
        }
    reel_cur = reel_play = reel_n++;
    gen_how = GH_NONE;
}
/* a DICE ALL throw moves the tempo after the piece (a BPM off the last one): the take keeps the throw's (set), a recall
 * plays at it (the tempo on AUTO, not locked) */
static void reel_tempo(int set)
{
    if (reel_play >= reel_n) return;
    if (set) reel[reel_play].bpm = song.g[G_BPM];
    else if (!gp.tempo && !(ui_prefs & PREF_BPM_LOCK)) song.g[G_BPM] = reel[reel_play].bpm;
}
/* OCT+ on the reel (gen_press): the take under the cursor made; 0: not on the reel (OCT+ makes a piece) */
static int gen_reel_load(void)
{
    if (!gp.view || !reel_n)
        return 0;
    gset_put(&reel[reel_cur].set);
    gen_go();
    reel_tempo(0);
    return 1;
}
/* PRESETS: the reel opened (the settings' view: its first turn), then scrolled; playing, the take under the cursor
 * loads once PRESETS rests */
static struct {                                         /* (v2) OCT+ held on the reel: the take layer */
    uint8_t held, used, focus;                          /* OCT+ held (the layer's); a key or a control acted (no load or
                                                         * piece when it is let go); KNOB 2's section */
    uint8_t down_oct;                                   /* OCT+ down (any page) */
    uint8_t drop, drop_mute[SL_PARTS];                  /* (v2) the parts dropped (held), their mute before */
    uint32_t down, t[27];                               /* the layer's keys down, since when */
} gl;
static struct { uint8_t on, bars, n; } jam = {0, 8, 0};   /* (v2) JAM: a take every `bars` bars, by itself */
/* (v2) the SET: takes in a row, each for its bars (REC while playing records one; the SET page edits and plays it) */
#define SET_N 32u
static struct { uint16_t id; uint8_t bars; } set_e[SET_N];
static uint8_t set_n, set_rec, set_play, set_i, set_left, set_cur, set_seen;
static uint32_t gl_code_ms;
static void set_knob(uint32_t k, int32_t s);
static void gen_reel_turn(int32_t s)
{
    reel_ask = 0;
    gl_code_ms = 0;
    if (gp.view == 2u) { set_knob(0, s); return; }      /* (the SET page: its entries, never the reel behind it) */
    if (!gp.view) {                                     /* (the settings: the reel, the turn a step too) */
        gp.view = 1;
        if (reel_play < reel_n) reel_cur = reel_play;
    }
    if (reel_n) {
        uint32_t c = (reel_cur + (s > 0 ? 1u : reel_n - 1u)) % reel_n;   /* (round: the newest, then the oldest) */
        if ((s > 0 && c < reel_cur) || (s < 0 && c > reel_cur)) {
            char b[8];
            fmt_int(b, c + 1u);
            str_cpy(b + str_len(b), "/", 2);
            fmt_int(b + str_len(b), reel_n);
            gen_pop(s > 0 ? "ROUND TO THE FIRST" : "ROUND TO THE NEWEST", b);
        }
        reel_cur = (uint8_t)c;
        if (gl.held) {                                  /* (v2) OCT+ held: a cue, the take when OCT+ is let go */
            reel_want = -1;
            gen_take_ms = 0;
        } else if ((song.playing || chain_busy()) && reel_cur != reel_play) {
            reel_want = (int8_t)reel_cur;
            gen_take_ms = (fm1_ms ? fm1_ms : 1u);
        }
    }
    ui.force = 1;
}
/* SAVE on the reel: keep or let go (0: toggled; right / left: kept / not) */
static void gen_reel_keep(int32_t s)
{
    char b[8] = "#";
    if (!reel_n) return;
    reel[reel_cur].keep = (uint8_t)(s > 0 ? 1 : s < 0 ? 0 : !reel[reel_cur].keep);   /* (0: toggled) */
    reel_hex(b + 1, reel[reel_cur].id);
    gen_pop(reel[reel_cur].keep ? "KEPT" : "NOT KEPT", b);
}
/* (v2) EDIT on the reel: ask to delete the take under the cursor (held: the takes not kept); a kept take stays */
static void gen_reel_edit(int held)
{
    uint32_t i, n = 0;
    if (!reel_n) return;
    if (held) {
        for (i = 0; i < reel_n; i++) n += !reel[i].keep;
        if (!n) { gen_pop("NOTHING TO DELETE", "ALL KEPT"); return; }
        reel_ask = 2;
    } else if (reel[reel_cur].keep) {
        gen_pop("KEPT", "SAVE LETS IT GO");
        return;
    } else {
        reel_ask = 1;
    }
    ui.force = 1;
}
/* the answer: OCT+ yes (1), OCT- no (0) */
static void gen_reel_answer(int yes)
{
    uint32_t i, n = 0;
    if (yes && reel_ask == 1u && reel_cur < reel_n) {
        reel_drop(reel_cur);
        gen_pop("TAKE", "DELETED");
    } else if (yes && reel_ask == 2u) {
        for (i = reel_n; i-- > 0;)
            if (!reel[i].keep) { reel_drop(i); n++; }
        gen_pop(n ? "NOT KEPT" : "NOTHING TO DELETE", n ? "DELETED" : "ALL KEPT");
    }
    reel_ask = 0;
    ui.force = 1;
}
/* a thumbnail's colour, a role's */
static uint16_t reel_col(uint32_t role)
{
    static const uint8_t H[RL_N] = {230, 175, 70, 125, 205, 255};   /* DRUM BASS PAD LEAD ARP TEX */
    return heat_col(H[role % RL_N]);
}
static void reel_hex(char *b, uint32_t id)              /* "0A3F" */
{
    static const char HX[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    b[0] = HX[(id >> 12) & 15u]; b[1] = HX[(id >> 8) & 15u]; b[2] = HX[(id >> 4) & 15u]; b[3] = HX[id & 15u]; b[4] = 0;
}
/* (v2) OCT+ held on the GEN screen: what the keys do, in the screen's last four lines */
static uint8_t gl_drawn;
static uint16_t reel_col(uint32_t role);
static const uint8_t GKEY[27];
static void draw_layer_keys(void)
{
    /* the 16 white keys F3 .. G5 (a letter; a part: filled in its colour; a drop: framed in it), the 11 black ones over
     * them (a sound: the part's colour), a key down lit, a locked part or sound dotted; two lines of help */
    static const char *const WT[16] = {"D", "B", "C", "A", "L", "T", "S", "H", "D", "B", "M", "C", "A", "L", "T", "V"};
    static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    static const uint8_t BLACK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25}, BAFTER[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    uint32_t i;
    if (!gl_drawn) lcd_fill(0, 170, 240, 70, T_BG);     /* (once: what was there before) */
    gl_drawn = 1;
    cv_begin(240, 44, T_BG);
    for (i = 0; i < 16u; i++) {                         /* (a part: filled in its colour; a drop: its colour as a frame) */
        uint32_t k = WHITE[i], c = GKEY[k], x = i * 15u;
        int part = c >= SL_DRUMS && c <= SL_TEX, drop = c >= 0x40u && c < 0x40u + SL_PARTS;
        uint32_t role = part ? SL_ROLE[c - SL_DRUMS] : drop ? SL_ROLE[c - 0x40u] : 0u;
        uint16_t col = reel_col(role), f = part ? col : drop ? col : T_RAISE, ink = part ? T_INK : drop ? col : T_TEXT;
        int down = ((fm1_in.notes >> k) & 1u) != 0u;
        int32_t w;
        cv_rrect((int32_t)x + 1, 16, 13, 26, 3, down ? T_TEXT : f, T_BG);
        if (drop && !down) cv_rrect((int32_t)x + 3, 18, 9, 22, 2, T_BG, f);
        w = text_w(&AF_S, WT[i]);
        cv_text((int32_t)x + 1 + (13 - w) / 2, 22, &AF_S, WT[i], down ? T_INK : ink);
        if (part && ((gp.lock >> c) & 1u)) cv_rect((int32_t)x + 6, 37, 3, 3, T_INK);   /* (locked) */
    }
    for (i = 0; i < 11u; i++) {
        uint32_t k = BLACK[i], c = GKEY[k], x = (BAFTER[i] + 1u) * 15u - 5u;
        int snd = c >= 0x10u && c < 0x10u + RL_N, down = ((fm1_in.notes >> k) & 1u) != 0u;
        cv_rrect((int32_t)x, 0, 10, 14, 2, down ? T_TEXT : snd ? reel_col(c - 0x10u) : T_MID, T_BG);
        if (snd && ((gp.lock >> (8u + c - 0x10u)) & 1u)) cv_rect((int32_t)x + 4, 10, 2, 2, T_INK);
    }
    cv_blit(0, 172);
    draw_text_box(0, 211, 240, &AF_S, "FILLED: NEW PART  BLACK: ITS SOUND", T_TEXT, 1);
    draw_text_box(0, 224, 240, &AF_S, "FRAMED: DROP WHILE HELD  HOLD: LOCK", T_MID, 1);
}
static void layer_keys_off(void)                        /* the keys' lines gone: the band cleared once */
{
    if (gl_drawn) lcd_fill(0, 170, 240, 70, T_BG);
    gl_drawn = 0;
}
static void draw_reel(void)
{
    char t[40];
    const take_t *k;
    uint32_t i, r, tot = 0, x;
    if (!gl.held) layer_keys_off();                     /* (the keys' lines gone first: the map, the footer over it) */
    str_cpy(t, "TAKES  ", sizeof t);
    if (reel_n) {                                       /* (v2) its place: 3/7 */
        fmt_int(t + str_len(t), reel_cur + 1u);
        str_cpy(t + str_len(t), "/", 2);
        fmt_int(t + str_len(t), reel_n);
    } else {
        str_cpy(t + str_len(t), "EMPTY", sizeof t - str_len(t));
    }
    if (gen_take_bar && gen_take_ms) str_cpy(t + str_len(t), "  NEXT BAR", sizeof t - str_len(t));   /* (v2) */
    else if (set_rec) str_cpy(t + str_len(t), "  REC SET", sizeof t - str_len(t));
    else if (jam.on) str_cpy(t + str_len(t), "  JAM", sizeof t - str_len(t));
    draw_text_box(0, 30, 240, &AF_M, t, T_THEME, 1);
    if (!reel_n) {
        lcd_fill(0, 50, 240, 148, T_BG);
        draw_text_box(0, 100, 240, &AF_S, "EVERY PIECE MADE GOES HERE", T_MID, 1);
        draw_text_box(0, 116, 240, &AF_S, "OCT+ MAKES THE FIRST", T_DIM, 1);
    } else {
        char b[40];
        uint32_t nl, y0 = 56u, rh;
        int32_t nw;
        k = &reel[reel_cur];
        nl = k->nl ? k->nl : 1u;
        rh = 60u / nl;
        cv_begin(240, 120, T_BG);
        if (reel_cur > 0u) cv_rrect(-12, 8, 22, 104, 6, T_SURF, T_BG);   /* (the takes either side) */
        if (reel_cur + 1u < reel_n) cv_rrect(230, 8, 22, 104, 6, T_SURF, T_BG);
        cv_rrect(16, 0, 208, 120, 8, reel_cur == reel_play ? T_RAISE : T_SURF, T_BG);
        b[0] = '#';
        reel_hex(b + 1, k->id);
        cv_text(26, 6, &AF_S, b, T_THEME);               /* (v2) its ID */
        nw = text_w(&AF_S, b);
        cv_text(26 + nw + 6, 3, &AF_M, k->name, T_TEXT);
        if (k->keep) {
            cv_rrect(176, 5, 40, 15, 4, T_THEME, T_SURF);
            cv_text(181, 6, &AF_S, "KEEP", T_INK);
        }
        if (k->gv != GEN_VERSION) cv_text(150, 108, &AF_S, "OLD GEN", T_DIM);   /* (another generator made it) */
        if (reel_cur == reel_play && (song.playing || chain_busy())) {   /* (the music now) */
            cv_rrect(k->keep ? 136 : 176, 5, 36, 15, 4, T_TEXT, T_RAISE);
            cv_text(k->keep ? 141 : 181, 6, &AF_S, "NOW", T_INK);
        } else if ((reel_want == (int8_t)reel_cur && gen_take_ms) || (gl.held && reel_cur != reel_play)) {
            cv_rrect(k->keep ? 136 : 176, 5, 36, 15, 4, T_ACCENT, T_SURF);   /* (v2) the take that comes next */
            cv_text(k->keep ? 141 : 181, 6, &AF_S, "CUE", T_INK);
        }
        if (k->flav) {
            str_cpy(b, REEL_FLAV[(k->flav - 1u) % 7u], sizeof b);
        } else {                                        /* (not trap: the tracks it ran on, its form) */
            b[0] = (char)('0' + k->nl); b[1] = 0;
            str_cpy(b + 1, k->nrow ? " TRACKS  SONG" : " TRACKS  LOOP", sizeof b - 1u);
        }
        cv_text(26, 23, &AF_S, b, T_THEME);
        if (k->parent) {                                /* (v2) its parent and what changed: "FROM 0A3B: DRUMS" */
            int32_t w0 = text_w(&AF_S, b), w;
            str_cpy(b, "FROM ", sizeof b);
            reel_hex(b + 5, k->parent);
            if (k->how && k->how < GH_N) { b[9] = ':'; b[10] = ' '; str_cpy(b + 11, GH_NAME[k->how], sizeof b - 11u); }
            if (26 + w0 + 8 + text_w(&AF_S, b) > 214) {   /* (no room: the short way, "<0A3B DRUMS") */
                b[0] = '<';
                reel_hex(b + 1, k->parent);
                if (k->how && k->how < GH_N) { b[5] = ' '; str_cpy(b + 6, GH_NAME[k->how], sizeof b - 6u); }
            }
            w = text_w(&AF_S, b);
            if (26 + w0 + 8 + w <= 214) cv_text(214 - w, 23, &AF_S, b, reel_find(k->parent) >= 0 ? T_MID : T_DIM);
        }
        b[0] = (char)('0' + k->set.seed / 1000u % 10u); b[1] = (char)('0' + k->set.seed / 100u % 10u);
        b[2] = (char)('0' + k->set.seed / 10u % 10u); b[3] = (char)('0' + k->set.seed % 10u);
        b[4] = ' '; b[5] = ' ';
        x = 6;
        if (k->bpm >= 100) b[x++] = (char)('0' + k->bpm / 100 % 10);
        b[x++] = (char)('0' + k->bpm / 10 % 10); b[x++] = (char)('0' + k->bpm % 10);
        b[x] = 0;
        str_cpy(b + x, " BPM  ", sizeof b - x);
        str_cpy(b + str_len(b), REEL_KEY[k->root % 12u], 4);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), N_SCALE[k->scl % NELEM(N_SCALE)], 6);
        cv_text(26, 37, &AF_S, b, T_MID);
        for (r = 0; r < nl; r++)                        /* the thumbnail: a row a live track, 32 steps */
            for (i = 0; i < 32u; i++)
                cv_rect((int32_t)(24u + i * 6u), (int32_t)(y0 + r * rh), 5, (int32_t)(rh > 2u ? rh - 2u : 1u),
                        (k->thumb[r] >> i) & 1u ? reel_col(k->role[r]) : reel_cur == reel_play ? T_SURF : T_RAISE);
        cv_blit(0, 50);
        /* the song: its rows as blocks (section colours), the playhead on the take playing */
        cv_begin(240, 24, T_BG);
        for (i = 0; i < k->nrow; i++) tot += k->row[i][1];
        if (!tot) {
            cv_rrect(24, 4, 192, 16, 4, T_SURF, T_BG);
            cv_text(108, 5, &AF_S, "LOOP", T_MID);
        } else {
            static const uint8_t SH[4] = {245, 120, 60, 200};   /* A B C D */
            static const char *const TRAP_SEC[4] = {"HOOK", "VERSE", "INTRO", "PRE"};   /* (narrow: H V I P) */
            uint32_t pos = 0, xs = 24;
            for (i = 0; i < k->nrow; i++) {
                uint32_t w = k->row[i][1] * 192u / tot, sl = k->row[i][0] & 3u;
                char sn[2] = {(char)(k->flav ? TRAP_SEC[sl][0] : 'A' + sl), 0};
                int lit = !gl.focus || sl == gl.focus;  /* (v2) KNOB 2's section lit, the others dim */
                cv_rect((int32_t)xs, 4, (int32_t)(w > 1u ? w - 1u : 1u), 16, lit ? heat_col(SH[sl]) : T_RAISE);
                if (k->flav && (int32_t)w >= text_w(&AF_S, TRAP_SEC[sl]) + 5) cv_text((int32_t)xs + 3, 5, &AF_S, TRAP_SEC[sl], T_INK);
                else if ((int32_t)w >= text_w(&AF_S, sn) + 4) cv_text((int32_t)xs + 2, 5, &AF_S, sn, T_INK);
                if (reel_cur == reel_play && chain.running && i < chain.row) pos += k->row[i][1];
                xs += w;
            }
            if (reel_cur == reel_play && chain.running && chain.row < k->nrow) {
                uint32_t b2 = k->row[chain.row][1];
                pos += b2 > chain.remaining ? b2 - chain.remaining : 0u;
                cv_rect((int32_t)(24u + pos * 192u / tot), 0, 2, 24, T_TEXT);   /* (the playhead) */
            }
        }
        if (!gl.held) cv_blit(0, 172);
    }
    if (gl_code_ms && fm1_ms - gl_code_ms < 10000u && reel_n) {   /* (v2) F#5: the take's code, 10 s */
        char c[CODE_MAX + 8u], g[CODE_MAX + 8u];
        uint32_t n = take_code(&reel[reel_cur].set, c), i, j = 0;
        if (!gl.held) {
            for (i = 0; i < n; i++) { if (i && !(i % 4u)) g[j++] = ' '; g[j++] = c[i]; }   /* (by four) */
            g[j] = 0;
            str_cpy(c, "CODE #", sizeof c);
            reel_hex(c + 6, reel[reel_cur].id);
            draw_text_box(0, 200, 240, &AF_S, c, T_MID, 1);
            draw_text_box(0, 214, 240, n > 12u ? &AF_S : &AF_M, n ? g : "TOO MANY CHOICES", T_THEME, 1);
            return;
        }
    }
    if (gl.held) {                                      /* (v2) OCT+ held: the keys */
        draw_layer_keys();
        return;
    }
    if (reel_ask) {                                     /* (v2) the question, in the footer */
        char q[32];
        if (reel_ask == 1u) {
            str_cpy(q, "DELETE #", sizeof q);
            reel_hex(q + 8, reel[reel_cur].id);
            str_cpy(q + 12, "?", 2);
        } else {
            str_cpy(q, "DELETE ALL NOT KEPT?", sizeof q);
        }
        draw_text_box(0, 200, 240, &AF_S, q, T_ACCENT, 1);
        draw_text_box(0, 216, 240, &AF_S, "OCT+ YES   OCT- NO", T_TEXT, 1);
        return;
    }
    if (gl.focus) {
        static const char *const SN[4] = {"", "SECTION B", "SECTION C", "SECTION D"};
        static const char *const TSN[4] = {"", "VERSE ONLY", "INTRO ONLY", "PRE-HOOK ONLY"};
        draw_text_box(0, 200, 240, &AF_S, reel_n && reel[reel_cur].flav ? TSN[gl.focus & 3u] : SN[gl.focus & 3u], T_ACCENT, 1);
        draw_text_box(0, 216, 240, &AF_S, "OCT+ AND A PART KEY: THAT PART", T_DIM, 1);
        return;
    }
    draw_text_box(0, 200, 240, &AF_S, "K1 TEMPO  K2 KEY  K3 ENERGY  K4 SECTION", T_TEXT, 1);
    draw_text_box(0, 216, 240, &AF_S, "SAVE KEEP  EDIT DEL  HOLD OCT+: KEYS", T_DIM, 1);
}

/* ------------------------------------------------------------------- (v2) OCT+ held on the reel: the take layer --- */
/* A key makes a take from the one playing (a child: the card says what), on the next bar (stopped: at once): the white
 * keys F3 .. D4 a part's notes made again (DRUMS BASS CHORDS ARP LEAD TEX), the black key next to each its sound, E4 every
 * sound, F4 the harmony, G4 / A4 ENERGY down / up, B4 the mix, C5 / D5 the key down / up, E5 / F5 the tempo, G5 VARY (the
 * ornaments: echoes, octaves, bends, the kit's drift), C#5 back to the parent take. A part or a sound key held half a
 * second locks it instead (ALGORITHM and the rerolls leave it; a new seed keeps it). On another take's card (PRESETS
 * turned with OCT+ held), a part or a sound key borrows that take's into the playing one. Let go of OCT+ without a key:
 * the take under the cursor, on the next bar. seq.c keeps the keys silent (layer_masks) */
enum { GK_SOUNDS = 0x20, GK_HARM, GK_MIX, GK_VARY, GK_PARENT, GK_ARR_DN, GK_ARR_UP, GK_JAM, GK_CODE };
#define GK_DROP 0x40u                                   /* + SL_* - SL_DRUMS: that part dropped while the key is held */
static const uint8_t GKEY[27] = {                       /* F3 .. G5: a part (SL_*), a role's sound (0x10 + RL_*), a drop
                                                         * (0x40 + part), an act */
    SL_DRUMS, 0x10 | RL_DRUM, SL_BASS, 0x10 | RL_BASS, SL_CHORDS, 0x10 | RL_PAD, SL_ARP, SL_LEAD, 0x10 | RL_LEAD, SL_TEX,
    0x10 | RL_TEX, GK_SOUNDS, GK_HARM, 0x10 | RL_ARP, GK_DROP + 0u, GK_ARR_DN, GK_DROP + 1u, GK_ARR_UP, GK_MIX,
    GK_DROP + 2u, GK_PARENT, GK_DROP + 3u, GK_JAM, GK_DROP + 4u, GK_DROP + 5u, GK_CODE, GK_VARY};
static const char *const GK_SAY[RL_N] = {"KIT", "BASS SOUND", "PAD SOUND", "LEAD SOUND", "ARP SOUND", "TEX SOUND"};
static const char *const SL_SAY[SL_PARTS] = {"DRUMS", "BASS", "CHORDS", "ARP", "LEAD", "TEX"};
/* a part or a sound borrowed: its pin (the same kind's replaced, else a free one, else the last) */
static void gl_pin(uint32_t what, uint32_t seed, uint32_t var)
{
    uint32_t i, at = GP_PINS - 1u;
    for (i = 0; i < GP_PINS; i++) if (gp.pin[i].what == what) { at = i; break; }
    if (i == GP_PINS)
        for (i = 0; i < GP_PINS; i++) if (!gp.pin[i].what) { at = i; break; }
    gp.pin[at].what = (uint8_t)what; gp.pin[at].seed = (uint16_t)seed; gp.pin[at].var = (uint8_t)var;
}
static void gl_unpin(uint32_t what)
{
    uint32_t i;
    for (i = 0; i < GP_PINS; i++) if (gp.pin[i].what == what) gp.pin[i].what = 0;
}
/* another take's own seed and variation for a stream or a sound (its pin, else its seed and variation) */
static uint32_t gl_of(const take_t *o, uint32_t what, uint32_t v, uint32_t *sd)
{
    uint32_t i;
    *sd = o->set.seed;
    for (i = 0; i < GP_PINS; i++) if (o->set.pin[i].what == what) { *sd = o->set.pin[i].seed; return o->set.pin[i].var; }
    return v;
}
static void gl_go(uint32_t how, const char *what, const char *say)
{
    char b[24];
    gen_take_arm(how);
    gen_take_ms = (fm1_ms - 460u) | 1u;                 /* (no rest: the next bar; stopped, now) */
    gl.used = 1;
    str_cpy(b, what, sizeof b);
    str_cpy(b + str_len(b), say, sizeof b - str_len(b));
    gen_pop(song.playing || chain_busy() ? "ON THE NEXT BAR" : "TAKE", b);
}
static void gl_act(uint32_t c)
{
    const take_t *o = gp.view == 1u && reel_cur < reel_n && reel_cur != reel_play && reel_play < reel_n ? &reel[reel_cur] : 0;
    uint32_t sd, v;
    if (c >= SL_DRUMS && c <= SL_TEX) {                 /* a part's notes */
        if (o) {                                        /* (borrowed from the take under the cursor) */
            v = gl_of(o, 1u + c, o->set.var[c], &sd);
            if (sd == gp.seed && !gpinned(1u + c)) gp.var[c] = (uint8_t)v;
            else gl_pin(1u + c, sd, v);
            gl_go(GH_BORROW, "BORROWED ", SL_SAY[c - SL_DRUMS]);
        } else if (gl.focus) {                          /* (a section's own: phase 3) */
            gp.secvar[(gl.focus - 1u) % 3u][c - SL_DRUMS]++;
            gl_go(GH_SECTION, "NEW SECTION ", SL_SAY[c - SL_DRUMS]);
        } else {
            gl_unpin(1u + c);
            gp.var[c]++;
            gl_go(GH_DRUMS + c - SL_DRUMS, "NEW ", SL_SAY[c - SL_DRUMS]);
        }
        return;
    }
    if (c >= 0x10u && c < 0x10u + RL_N) {               /* a role's sound */
        uint32_t r = c - 0x10u;
        if (o) {
            v = gl_of(o, 16u + r, o->set.svar[r], &sd);
            if (sd == gp.seed && !gpinned(16u + r)) gp.svar[r] = (uint8_t)v;
            else gl_pin(16u + r, sd, v);
            gl_go(GH_BORROW, "BORROWED ", GK_SAY[r]);
        } else {
            gl_unpin(16u + r);
            gp.svar[r]++;
            gl_go(GH_KIT + r, "NEW ", GK_SAY[r]);
        }
        return;
    }
    switch (c) {
    case GK_SOUNDS:
        for (v = 0; v < RL_N; v++) if (!((gp.lock >> (8u + v)) & 1u)) { gl_unpin(16u + v); gp.svar[v]++; }
        gl_go(GH_SOUNDS, "NEW SOUNDS", "");
        break;
    case GK_HARM: gp.var[SL_HARM]++; gl_go(GH_HARM, "NEW HARMONY", ""); break;
    case GK_MIX: gp.var[SL_MIX]++; gl_go(GH_MIX, "NEW MIX", ""); break;
    case GK_VARY: gp.var[SL_FORM]++; gl_go(GH_VARY, "VARIED", ""); break;
    case GK_ARR_DN: case GK_ARR_UP: {
        char b[4] = "1/4";
        if (!live.mode) { gen_pop("A LOOP", "NO SECTIONS"); break; }
        gp.arr = (uint8_t)((gp.arr + (c == GK_ARR_UP ? 1u : 3u)) % 4u);
        b[0] = (char)('1' + gp.arr);
        gl_go(GH_FORM, "ARRANGEMENT ", b);
        break;
    }
    case GK_CODE:                                       /* (v2) the take's code (let go of OCT+ to read it) */
        gl_code_ms = (fm1_ms ? fm1_ms : 1u);
        gl.used = 1;
        ui.force = 1;
        break;
    case GK_JAM: {
        char b[4] = {0};
        jam.on = (uint8_t)!jam.on;
        jam.n = 0;
        gl.used = 1;
        str_cpy(b, "", sizeof b);
        {
            char v[16] = "EVERY ";
            fmt_int(v + 6, jam.bars);
            str_cpy(v + str_len(v), " BARS", 6);
            gen_pop(jam.on ? "JAM ON  (D#5 HELD: BARS)" : "JAM", jam.on ? v : "OFF");
        }
        (void)b;
        break;
    }
    case GK_PARENT:
        if (reel_play < reel_n && reel[reel_play].parent) {
            int i = reel_find(reel[reel_play].parent);
            if (i < 0) { gen_pop("ITS PARENT", "DELETED"); break; }
            reel_cur = (uint8_t)i;
            reel_want = (int8_t)i;
            gl_go(GH_NONE, "BACK TO PARENT", "");
        } else {
            gen_pop("FIRST OF ITS FAMILY", "NO PARENT");
        }
        break;
    default: break;
    }
}
/* a part key or a sound key held: its lock */
static void gl_lock(uint32_t c)
{
    uint32_t bit = c >= 0x10u ? 8u + (c - 0x10u) : c, on;
    gp.lock ^= (uint16_t)(1u << bit);
    on = (gp.lock >> bit) & 1u;
    gen_pop(on ? "LOCKED" : "UNLOCKED", c >= 0x10u ? GK_SAY[(c - 0x10u) % RL_N] : SL_SAY[(c - SL_DRUMS) % SL_PARTS]);
    gl.used = 1;
}
/* (v2) a part dropped while its key is held (G4 A4 C5 D5 E5 F5: DRUMS BASS CHORDS ARP LEAD TEX): its track muted at
 * once, back as it was when the key is let go (no take: a performance) */
static void gen_reel_jam_bars(int32_t s);
static uint32_t gl_drop_trk(uint32_t p)
{
    uint32_t r = SL_ROLE[p % SL_PARTS];
    return gs.trk_of[r] < NTRK ? gs.trk_of[r] : NTRK;
}
static void gl_drop(uint32_t p, int on)
{
    uint32_t t = gl_drop_trk(p);
    if (on) {
        if (t >= NTRK || !gs.act[SL_ROLE[p]]) { gen_pop("DROP", "NOT PLAYING"); return; }
        if (!((gl.drop >> p) & 1u)) gl.drop_mute[p] = (uint8_t)trk[t].p[P_MUTE];
        gl.drop |= (uint8_t)(1u << p);
        trk[t].p[P_MUTE] = 1;
        gen_pop("DROPPED WHILE HELD", SL_SAY[p]);
    } else if ((gl.drop >> p) & 1u) {
        gl.drop &= (uint8_t)~(1u << p);
        if (t < NTRK) trk[t].p[P_MUTE] = gl.drop_mute[p];
        gpop.on = 0;
        gen_fresh = 1;
        ui.force = 1;
    }
}
static void gl_drop_hold(void)                          /* every frame: a section that came in keeps the drop */
{
    uint32_t p;
    for (p = 0; p < SL_PARTS; p++)
        if ((gl.drop >> p) & 1u) {
            uint32_t t = gl_drop_trk(p);
            if (t < NTRK) trk[t].p[P_MUTE] = 1;
        }
}
/* every frame on the GEN screen: the layer's keys (lkeys: pressed with OCT+ held, silent: seq.c) */
static void gen_layer(uint32_t lkeys)
{
    uint32_t down = (fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u, held = down && gp.view != 2u, k, up;
    if (down && !gl.down_oct) gl.used = 0;               /* (OCT+ pressed: nothing done with it yet) */
    gl.down_oct = (uint8_t)down;
    if (held != gl.held) ui.force = 1;
    gl.held = (uint8_t)held;
    if (((fm1_in.buttons >> panel.btn[B_GLO]) & 1u) && !down) {   /* GLO held: G4 UNDO (the keys silent: layer_masks) */
        if ((lkeys >> 14) & 1u) {
            reel_ask = 0;
            gen_pop("UNDO", chain_busy() ? "STOP TO EDIT" : undo_do() ? "NOTHING TO UNDO" : "UNDONE");
            ui.force = 1;
        }
        return;
    }
    if (down && !held) {                                 /* (the SET page: OCT+ adds; its keys do nothing) */
        if (lkeys) gl.used = 1;
        return;
    }
    up = gl.down & ~fm1_in.notes;
    for (k = 0; k < 27u; k++)
        if ((up >> k) & 1u) {                           /* a key let go: a part or a sound, held long its lock; D#5
                                                         * held long JAM's bars; a drop over */
            uint32_t c = GKEY[k];
            gl.down &= ~(1u << k);
            if (c >= GK_DROP && c < GK_DROP + SL_PARTS) gl_drop(c - GK_DROP, 0);
            else if (c == GK_JAM && fm1_ms - gl.t[k] >= 500u) gen_reel_jam_bars(1);
            else if (fm1_ms - gl.t[k] >= 500u) gl_lock(c);
            else gl_act(c);
        }
    gl_drop_hold();
    if (!held) return;
    for (k = 0; k < 27u; k++)
        if ((lkeys >> k) & 1u) {
            uint32_t c = GKEY[k];
            gl.used = 1;
            if (c >= GK_DROP && c < GK_DROP + SL_PARTS) { gl.down |= 1u << k; gl_drop(c - GK_DROP, 1); }   /* (held: out) */
            else if ((c && c < 0x20u) || c == GK_JAM) { gl.down |= 1u << k; gl.t[k] = fm1_ms; }   /* (let go: tap, hold) */
            else if (c) gl_act(c);
        }
}

/* ---------------------------------------------------------------------------------- (v2) KNOB 2, KNOB 3, JAM --- */
/* KNOB 4 on the reel: the section the part keys make again (ALL, B, C, D: a song's; a loop has none) */
static void gen_reel_focus(int32_t s)
{
    static const char *const SN[4] = {"ALL", "B", "C", "D"}, *const TN[4] = {"ALL", "VERSE", "INTRO", "PRE-HOOK"};
    if (!reel_n || !reel[reel_cur].nrow) { gl.focus = 0; gen_pop("A LOOP", "NO SECTIONS"); return; }
    gl.focus = (uint8_t)clamp((int32_t)gl.focus + (s > 0 ? 1 : -1), 0, 3);
    gen_pop("THE PART KEYS REMAKE", reel[reel_cur].flav ? TN[gl.focus] : SN[gl.focus]);
}
/* (v2) KNOB 1 on the reel: the tempo, at once (no new take: the take playing keeps it) */
static void gen_reel_tempo(int32_t s)
{
    int32_t x;
    char v[12];
    if (ui_prefs & PREF_BPM_LOCK) { gen_pop("TEMPO", "BPM LOCKED"); return; }
    x = clamp(song.g[G_BPM] + accel(EN_K1, s, 140), 60, 200);
    song.g[G_BPM] = (int16_t)x;
    gp.tempo = (uint8_t)x;
    if (reel_play < reel_n) { reel[reel_play].set.tempo = (uint8_t)x; reel[reel_play].bpm = (int16_t)x; }
    gp.sig = gen_sig();
    fmt_int(v, x);
    str_cpy(v + str_len(v), " BPM", 5);
    gen_pop("TEMPO", v);
}
/* (v2) KNOB 2: the key a semitone a detent, KNOB 3: the energy (5 a detent): a take once it rests, on the next bar */
static void gen_reel_key(int32_t s)
{
    int32_t x = gp.key ? (int32_t)gp.key - 1 : gs.root;   /* (AUTO: from the key playing) */
    char v[16];
    x = (x + (s > 0 ? 1 : 11)) % 12;
    gp.key = (uint8_t)(x + 1);
    gen_take_arm(GH_KEY);
    str_cpy(v, "KEY ", sizeof v);
    str_cpy(v + 4, REEL_KEY[x % 12], 4);
    gen_pop(song.playing || chain_busy() ? "ON THE NEXT BAR" : "TAKE", v);
}
static void gen_reel_energy(int32_t s)
{
    char v[16];
    gp.energy = (uint8_t)clamp((int32_t)gp.energy + (s > 0 ? 5 : -5), 0, 100);
    gen_take_arm(GH_ENERGY);
    str_cpy(v, "ENERGY ", sizeof v);
    fmt_int(v + 7, gp.energy);
    gen_pop(song.playing || chain_busy() ? "ON THE NEXT BAR" : "TAKE", v);
}
/* D#5 held on the take layer: JAM's bars (4, 8, 16, 32, round) */
static void gen_reel_jam_bars(int32_t s)
{
    static const uint8_t B[4] = {4, 8, 16, 32};
    uint32_t i;
    char v[16] = "EVERY ";
    for (i = 0; i < 3u && B[i] != jam.bars; i++) {}
    i = (i + (s > 0 ? 1u : 3u)) % 4u;                   /* (round: D#5 held again and again) */
    jam.bars = B[i];
    fmt_int(v + 6, jam.bars);
    str_cpy(v + str_len(v), " BARS", 6);
    gen_pop(jam.on ? "JAM" : "JAM (OFF)", v);
    gl.used = 1;
}
/* a bar went by (the music playing, the GEN screen open): JAM's count; its time: a take with one thing made again (a
 * part's notes mostly, a sound, the ornaments, the mix; never a locked one), on the next bar */
static void jam_bar(void)
{
    static const uint8_t W[4] = {60, 25, 10, 5};        /* a part, a sound, VARY, the mix */
    uint32_t c = 0, tries;
    if (!jam.on || set_play || gen_take_ms || ++jam.n < jam.bars) return;   /* (a SET playing: it leads) */
    jam.n = 0;
    reel_cur = reel_play < reel_n ? reel_play : reel_cur;   /* (from the take playing: never a borrow) */
    for (tries = 0; tries < 12u && !c; tries++) {
        uint32_t w = rng() % 100u, k = W[0], r;
        if (w < k) {
            r = SL_DRUMS + rng() % SL_PARTS;
            if (!((gp.lock >> r) & 1u) && gs.act[SL_ROLE[r - SL_DRUMS]]) c = r;
        } else if (w < (k += W[1])) {
            r = rng() % RL_N;
            if (!((gp.lock >> (8u + r)) & 1u) && gs.trk_of[r] < NTRK) c = 0x10u + r;
        } else if (w < k + W[2]) {
            c = GK_VARY;
        } else {
            c = GK_MIX;
        }
    }
    if (c) { gl_act(c); gl.used = 0; }
}

/* ---------------------------------------------------------------------------------------- (v2) the SET --- */
/* REC on the reel: playing, a SET recorded (a new one: the take playing first, then every take that becomes the music,
 * each with the bars it played; REC again ends it); stopped, the SET page */
static void set_note_take(void)                         /* a take became the music: the SET's next entry */
{
    if (!set_rec || reel_play >= reel_n) return;
    if (set_n && set_e[set_n - 1u].id == reel[reel_play].id) return;
    if (set_n >= SET_N) { set_rec = 0; gen_pop("SET FULL", "32 ENTRIES"); return; }
    set_e[set_n].id = reel[reel_play].id;
    set_e[set_n].bars = 0;
    set_n++;
}
static void gen_reel_rec(void)
{
    char b[16];
    if (song.playing || chain_busy()) {
        set_rec = (uint8_t)!set_rec;
        if (set_rec) { set_n = 0; set_note_take(); gen_pop("SET", "RECORDING"); }
        else {
            if (set_n && !set_e[set_n - 1u].bars) set_n--;   /* (a take that never played a bar) */
            fmt_int(b, set_n);
            str_cpy(b + str_len(b), set_n == 1u ? " TAKE" : " TAKES", 8);
            gen_pop("SET RECORDED (SELECT: ITS PAGE)", set_n ? b : "EMPTY");
        }
    } else {
        gen_pop("REC A SET", "PLAY FIRST");
    }
}
/* the take an entry plays (-1: deleted) */
static int set_take(uint32_t i) { return i < set_n ? reel_find(set_e[i].id) : -1; }
static void set_arm(uint32_t i)                         /* an entry's take, on the next bar */
{
    int t = set_take(i);
    if (t < 0) return;
    reel_want = (int8_t)t;
    reel_cur = (uint8_t)t;
    gen_take_arm(GH_NONE);
    gen_take_ms = (fm1_ms - 460u) | 1u;
}
/* a bar went by: the SET's count (recording: the take's bars; playing: the next entry armed a bar before its turn) */
static void set_bar(void)
{
    if (set_rec && set_n && set_e[set_n - 1u].bars < 255u) set_e[set_n - 1u].bars++;
    if (!set_play || !set_n) return;
    if (set_left > 1u) set_left--;
    else {
        set_i = (uint8_t)((set_i + 1u) % set_n);
        set_left = set_e[set_i].bars ? set_e[set_i].bars : 1u;
    }
    if (set_left == 1u) set_arm((set_i + 1u) % set_n);
}
/* PLAY on the SET page: from the entry under the cursor (stopped), or stopped */
static void set_toggle(void)
{
    int t;
    if (song.playing || chain_busy() || seq_counting()) { transport_req = 2; set_play = 0; return; }
    if (!set_n || (t = set_take(set_cur)) < 0) { gen_pop("THAT TAKE", "DELETED"); return; }
    gset_put(&reel[t].set);
    gen_how = GH_NONE;
    gen_go();
    reel_tempo(0);
    gp.sig = gen_sig();
    set_play = 1;
    set_seen = 0;
    set_i = set_cur;
    set_left = set_e[set_i].bars ? set_e[set_i].bars : 1u;
    if (set_left == 1u && set_n > 1u) set_arm((set_i + 1u) % set_n);
    if (live.mode) chain_play_ui();
    else transport_req = 1;
}
/* the SET page's knobs: KNOB 1 the entry, KNOB 2 its take (the reel's, in order), KNOB 3 its bars */
static void set_knob(uint32_t k, int32_t s)
{
    if (!set_n) return;
    if (k == 0u) set_cur = (uint8_t)clamp((int32_t)set_cur + (s > 0 ? 1 : -1), 0, (int32_t)set_n - 1);
    else if (k == 1u && reel_n) {
        int t = set_take(set_cur);
        t = clamp((t < 0 ? 0 : t) + (s > 0 ? 1 : -1), 0, (int32_t)reel_n - 1);
        set_e[set_cur].id = reel[t].id;
    } else if (k == 2u) {
        set_e[set_cur].bars = (uint8_t)clamp((int32_t)set_e[set_cur].bars + (s > 0 ? 1 : -1), 1, 64);
    }
    ui.force = 1;
}
/* OCT+ on the SET page: the take playing (else under the reel's cursor) after the entry; EDIT: the entry out */
static void set_insert(void)
{
    uint32_t i, t = reel_play < reel_n ? reel_play : reel_cur;
    if (set_n >= SET_N || !reel_n) return;
    for (i = set_n; i > (uint32_t)set_cur + 1u; i--) set_e[i] = set_e[i - 1u];
    i = set_n ? set_cur + 1u : 0u;
    set_e[i].id = reel[t].id;
    set_e[i].bars = 4;
    set_n++;
    set_cur = (uint8_t)i;
    ui.force = 1;
}
static void set_delete(void)
{
    uint32_t i;
    if (!set_n) return;
    for (i = set_cur; i + 1u < set_n; i++) set_e[i] = set_e[i + 1u];
    set_n--;
    if (set_cur >= set_n && set_cur) set_cur--;
    if (!set_n) { gp.view = 1; gen_pop("SET", "EMPTY"); }
    ui.force = 1;
}
static void draw_set(void)
{
    char t[32], b[40];
    uint32_t i, tot = 0, xs = 16, pos = 0;
    str_cpy(t, set_play ? "SET  PLAYING" : "SET", sizeof t);
    draw_text_box(0, 30, 240, &AF_M, t, T_THEME, 1);
    for (i = 0; i < set_n; i++) tot += set_e[i].bars ? set_e[i].bars : 1u;
    cv_begin(240, 40, T_BG);
    for (i = 0; i < set_n; i++) {                       /* the entries as blocks, as long as their bars */
        uint32_t bars = set_e[i].bars ? set_e[i].bars : 1u, w = bars * 208u / (tot ? tot : 1u);
        int tk = set_take(i);
        uint16_t c = tk < 0 ? T_RAISE : heat_col((int32_t)(60u + (set_e[i].id * 37u) % 190u));
        cv_rect((int32_t)xs, i == set_cur ? 2 : 8, (int32_t)(w > 1u ? w - 1u : 1u), i == set_cur ? 36 : 24, c);
        if (w >= 16u) {                                 /* (its bars on it) */
            char nb[6];
            fmt_int(nb, bars);
            if ((int32_t)w >= text_w(&AF_S, nb) + 4) cv_text((int32_t)xs + ((int32_t)w - text_w(&AF_S, nb)) / 2, 13, &AF_S, nb, T_INK);
        }
        if (set_play && i < set_i) pos += bars;
        xs += w;
    }
    if (set_play && set_i < set_n) {
        uint32_t bars = set_e[set_i].bars ? set_e[set_i].bars : 1u;
        pos += bars - (set_left < bars ? set_left : bars);
        cv_rect((int32_t)(16u + pos * 208u / (tot ? tot : 1u)), 0, 2, 40, T_TEXT);
    }
    cv_blit(0, 56);
    lcd_fill(0, 100, 240, 96, T_BG);
    if (set_n) {
        int tk = set_take(set_cur);
        uint32_t n;
        fmt_int(b, set_cur + 1u);
        str_cpy(b + str_len(b), " OF ", 5);
        fmt_int(b + str_len(b), set_n);
        draw_text_box(0, 104, 240, &AF_S, b, T_MID, 1);
        b[0] = '#';
        reel_hex(b + 1, set_e[set_cur].id);
        str_cpy(b + 5, "  ", 3);
        str_cpy(b + 7, tk < 0 ? "DELETED" : reel[tk].name, sizeof b - 7u);
        draw_text_box(0, 122, 240, &AF_M, b, tk < 0 ? T_DIM : T_TEXT, 1);
        n = set_e[set_cur].bars;
        b[0] = (char)('0' + n / 10u); b[1] = (char)('0' + n % 10u);
        str_cpy(b + 2, n == 1u ? " BAR" : " BARS", sizeof b - 2u);
        draw_text_box(0, 146, 240, &AF_M, b[0] == '0' ? b + 1 : b, T_THEME, 1);
    }
    else {
        draw_text_box(0, 120, 240, &AF_S, "NO SET YET", T_MID, 1);
        draw_text_box(0, 136, 240, &AF_S, "PLAYING: REC RECORDS THE TAKES", T_DIM, 1);
        draw_text_box(0, 152, 240, &AF_S, "OR OCT+ ADDS THE TAKE PLAYING", T_DIM, 1);
    }
    draw_text_box(0, 184, 240, &AF_S, "K1 ENTRY  K2 TAKE  K3 BARS", T_TEXT, 1);
    draw_text_box(0, 200, 240, &AF_S, "PLAY RUN  OCT+ ADD  EDIT DEL", T_TEXT, 1);
    draw_text_box(0, 216, 240, &AF_S, "OCT- BACK  SEL NEXT PAGE", T_DIM, 1);
}

/* ------------------------------------------------------------------ (v2) the reel packed (flash: OBJ_REEL) --- */
/* "TK", the format, the generator, the next ID, the takes, the SET's entries, JAM's bars; then the SET (3 bytes an
 * entry) and the takes oldest first, each its recipe (the variations only when it has some), its card. What does not
 * fit (3840 bytes) leaves out the oldest takes not kept first. The pack is read back by the same firmware; a take of
 * another generator (gv) is kept and loads, its card says OLD GEN */
#define REEL_FMT 1u
static uint32_t tk_pack(const take_t *t, uint8_t *out)  /* a take, packed into out (NULL: its length only) */
{
    uint8_t b[192];
    uint32_t n = 0, i, f = 0, np = 0;
    const gset_t *g = &t->set;
#define TP(x) (b[n++] = (uint8_t)(x))
    TP(g->seed); TP(g->seed >> 8);
    TP(g->genre); TP(g->energy); TP(g->form); TP(g->complex); TP(g->swing); TP(g->human); TP(g->tempo); TP(g->key);
    TP(g->scale); TP(g->length); TP(g->space); TP(g->fills); TP(g->wet); TP(g->grit); TP(g->nlive);
    for (i = 0; i < GP_N; i++) TP(g->amt[i]);
    for (i = 0; i < SL_N; i++) f |= g->var[i] ? 1u : 0u;
    for (i = 0; i < RL_N; i++) f |= g->svar[i] ? 2u : 0u;
    for (i = 0; i < 3u * SL_PARTS; i++) f |= (&g->secvar[0][0])[i] ? 4u : 0u;
    for (i = 0; i < GP_PINS; i++) np += g->pin[i].what != 0u;
    f |= np ? 8u : 0u;
    f |= g->arr ? 16u : 0u;
    TP(f);
    if (f & 1u) for (i = 0; i < SL_N; i++) TP(g->var[i]);
    if (f & 2u) for (i = 0; i < RL_N; i++) TP(g->svar[i]);
    if (f & 4u) for (i = 0; i < 3u * SL_PARTS; i++) TP((&g->secvar[0][0])[i]);
    if (f & 8u) {
        TP(np);
        for (i = 0; i < GP_PINS; i++)
            if (g->pin[i].what) { TP(g->pin[i].what); TP(g->pin[i].var); TP(g->pin[i].seed); TP(g->pin[i].seed >> 8); }
    }
    if (f & 16u) TP(g->arr);
    TP(t->id); TP(t->id >> 8); TP(t->parent); TP(t->parent >> 8);
    for (i = 0; i < 12u; i++) TP(t->name[i]);
    TP(t->flav); TP(t->root); TP(t->scl); TP(t->nl); TP(t->keep); TP(t->nrow); TP(t->how); TP(t->gv);
    TP(t->bpm); TP((uint16_t)t->bpm >> 8);
    for (i = 0; i < t->nl && i < 6u; i++) { TP(t->thumb[i]); TP(t->thumb[i] >> 8); TP(t->thumb[i] >> 16); TP(t->thumb[i] >> 24); }
    for (i = 0; i < t->nl && i < 6u; i++) TP(t->role[i]);
    for (i = 0; i < t->nrow && i < 12u; i++) { TP(t->row[i][0]); TP(t->row[i][1]); }
#undef TP
    if (out) memcpy(out, b, n);
    return n;
}
/* the reel into out (at most max bytes); its length */
static uint32_t reel_pack(uint8_t *out, uint32_t max)
{
    uint32_t i, tot = 9u + 3u * set_n, n, cnt = 0;
    uint32_t inc = 0;                                   /* (bit i: take i goes in) */
    for (i = 0; i < reel_n; i++) { inc |= 1u << i; tot += tk_pack(&reel[i], 0); }
    for (i = 0; tot > max && i < reel_n; i++)           /* (too much: the oldest not kept out, then the oldest) */
        if (!reel[i].keep) { inc &= ~(1u << i); tot -= tk_pack(&reel[i], 0); }
    for (i = 0; tot > max && i < reel_n; i++)
        if ((inc >> i) & 1u) { inc &= ~(1u << i); tot -= tk_pack(&reel[i], 0); }
    for (i = 0; i < reel_n; i++) cnt += (inc >> i) & 1u;
    out[0] = 'T'; out[1] = 'K'; out[2] = REEL_FMT; out[3] = GEN_VERSION;
    out[4] = (uint8_t)reel_next; out[5] = (uint8_t)(reel_next >> 8);
    out[6] = (uint8_t)cnt; out[7] = set_n; out[8] = jam.bars;
    n = 9u;
    for (i = 0; i < set_n; i++) { out[n++] = (uint8_t)set_e[i].id; out[n++] = (uint8_t)(set_e[i].id >> 8); out[n++] = set_e[i].bars; }
    for (i = 0; i < reel_n; i++)
        if ((inc >> i) & 1u) n += tk_pack(&reel[i], out + n);
    return n;
}
/* the reel from a pack (0: taken; the reel as it was when the pack is not one) */
static int reel_unpack(const uint8_t *b, uint32_t len)
{
    uint32_t n = 9u, i, k, cnt, sn;
    if (len < 9u || b[0] != 'T' || b[1] != 'K' || b[2] != REEL_FMT) return -1;
    cnt = b[6]; sn = b[7];
    if (cnt > REEL_N || sn > SET_N || n + 3u * sn > len) return -1;
    reel_n = 0;
    for (i = 0; i < sn; i++, n += 3u) { set_e[i].id = (uint16_t)(b[n] | b[n + 1u] << 8); set_e[i].bars = b[n + 2u]; }
    set_n = (uint8_t)sn;
    jam.bars = b[8] == 4u || b[8] == 8u || b[8] == 16u || b[8] == 32u ? b[8] : 8u;
    reel_next = (uint16_t)(b[4] | b[5] << 8);
    for (k = 0; k < cnt; k++) {
        take_t *t = &reel[reel_n];
        gset_t *g = &t->set;
        uint32_t f;
#define TG(x) do { if (n >= len) goto bad; (x) = b[n++]; } while (0)
        memset(t, 0, sizeof *t);
        { uint8_t lo, hi; TG(lo); TG(hi); g->seed = (uint16_t)(lo | hi << 8); }
        TG(g->genre); TG(g->energy); TG(g->form); TG(g->complex); TG(g->swing); TG(g->human); TG(g->tempo); TG(g->key);
        TG(g->scale); TG(g->length); TG(g->space); TG(g->fills); TG(g->wet); TG(g->grit); TG(g->nlive);
        for (i = 0; i < GP_N; i++) TG(g->amt[i]);
        TG(f);
        if (f & 1u) for (i = 0; i < SL_N; i++) TG(g->var[i]);
        if (f & 2u) for (i = 0; i < RL_N; i++) TG(g->svar[i]);
        if (f & 4u) for (i = 0; i < 3u * SL_PARTS; i++) TG((&g->secvar[0][0])[i]);
        if (f & 8u) {
            uint32_t np;
            TG(np);
            for (i = 0; i < np && i < GP_PINS; i++) {
                uint8_t lo, hi;
                TG(g->pin[i].what); TG(g->pin[i].var); TG(lo); TG(hi);
                g->pin[i].seed = (uint16_t)(lo | hi << 8);
            }
        }
        if (f & 16u) TG(g->arr);
        { uint8_t a, c, d, e; TG(a); TG(c); TG(d); TG(e); t->id = (uint16_t)(a | c << 8); t->parent = (uint16_t)(d | e << 8); }
        for (i = 0; i < 12u; i++) TG(t->name[i]);
        t->name[12] = 0;
        TG(t->flav); TG(t->root); TG(t->scl); TG(t->nl); TG(t->keep); TG(t->nrow); TG(t->how); TG(t->gv);
        { uint8_t lo, hi; TG(lo); TG(hi); t->bpm = (int16_t)(lo | hi << 8); }
        if (t->nl > 6u || t->nrow > 12u || t->how >= GH_N) goto bad;
        for (i = 0; i < t->nl; i++) { uint8_t a, c, d, e; TG(a); TG(c); TG(d); TG(e); t->thumb[i] = a | (uint32_t)c << 8 | (uint32_t)d << 16 | (uint32_t)e << 24; }
        for (i = 0; i < t->nl; i++) TG(t->role[i]);
        for (i = 0; i < t->nrow; i++) { TG(t->row[i][0]); TG(t->row[i][1]); }
#undef TG
        if (!t->id || reel_find(t->id) >= 0) goto bad;
        reel_n++;
    }
    reel_cur = (uint8_t)(reel_n ? reel_n - 1u : 0u);
    reel_play = 0xFF;
    if (!reel_next) reel_next = 1;
    return 0;
bad:
    reel_n = 0; set_n = 0; reel_next = 1;
    return -1;
}
/* what a save would hold, hashed: the reel changed since the last one */
static uint32_t reel_sig(void)
{
    uint32_t h = 2166136261u, i;
    const uint8_t *p = (const uint8_t *)reel;
    for (i = 0; i < reel_n * sizeof reel[0]; i++) h = (h ^ p[i]) * 16777619u;
    p = (const uint8_t *)set_e;
    for (i = 0; i < set_n * sizeof set_e[0]; i++) h = (h ^ p[i]) * 16777619u;
    return (h ^ reel_n ^ (uint32_t)set_n << 8 ^ (uint32_t)jam.bars << 16) * 16777619u ^ reel_next;
}

/* -------------------------------------------------------------------------------------- (v2) share codes --- */
/* A take's recipe as a code to read off one GW-1 and type into another (the same firmware makes the same piece): base
 * 32 without I L O U (typed as 1 1 0 V), the last character a check. Bits: the code's version (2), the seed (14), the
 * genre (6, RANDOM 0), 6 tracks (1), the form (2); the settings that are not as the screen opens (a mask of 17 and their
 * values); the take's own choices (a mask of 5: the streams', the sounds', the sections' variations, the pins, the
 * arrangement). A take as made: 7 characters, with a few choices 10 to 16, at most 28 */
#define CODE_VER 1u
static const char CK32[33] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
typedef struct { uint8_t b[20]; uint32_t n; } bits_t;   /* (28 characters: 140 bits) */
static void bw(bits_t *w, uint32_t v, uint32_t k)
{
    while (k--) {
        if (w->n < 160u && ((v >> k) & 1u)) w->b[w->n >> 3] |= (uint8_t)(0x80u >> (w->n & 7u));
        w->n++;
    }
}
static uint32_t brd(const bits_t *w, uint32_t *at, uint32_t k)
{
    uint32_t v = 0;
    while (k--) {
        v = v << 1 | (*at < w->n && *at < 160u ? (uint32_t)(w->b[*at >> 3] >> (7u - (*at & 7u))) & 1u : 0u);
        (*at)++;
    }
    return v;
}
static void bwv(bits_t *w, uint32_t v)                 /* a variation: 1..7 in 4 bits, else 9 */
{
    if (v < 8u) { bw(w, 0, 1); bw(w, v, 3); } else { bw(w, 1, 1); bw(w, v, 8); }
}
static uint32_t brv(const bits_t *w, uint32_t *at) { return brd(w, at, 1) ? brd(w, at, 8) : brd(w, at, 3); }
static uint32_t code7(uint32_t v) { return v == GA_AUTO ? 127u : v > 126u ? 126u : v; }   /* (AUTO: 127) */
static uint32_t uncode7(uint32_t v) { return v == 127u ? GA_AUTO : v; }
static uint32_t code_sum(const bits_t *w, uint32_t n)   /* the check: 5 bits of the payload's hash */
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < (n + 7u) / 8u && i < 20u; i++) h = (h ^ w->b[i]) * 16777619u;
    return (h ^ h >> 5 ^ h >> 10 ^ h >> 15) & 31u;
}
/* a take's code into out (CODE_MAX + 1); its length, 0 when it does not fit */
static uint32_t take_code(const gset_t *g, char *out)
{
    bits_t w;
    uint32_t m = 0, x = 0, i, k, n;
    static const uint8_t DEF[12] = {50, 50, GA_AUTO, 30, 0, 0, 0, 0, 0, GA_AUTO, GA_AUTO, GA_AUTO};
    const uint8_t v[12] = {g->energy, g->complex, g->swing, g->human, g->tempo, g->key, g->scale, g->length, g->space,
                           g->fills, g->wet, g->grit};
    memset(&w, 0, sizeof w);
    bw(&w, CODE_VER, 2); bw(&w, g->seed, 14); bw(&w, g->genre, 6); bw(&w, g->nlive > 4u, 1); bw(&w, g->form, 2);
    for (i = 0; i < 12u; i++) m |= (uint32_t)(v[i] != DEF[i]) << i;
    for (i = 0; i < GP_N; i++) m |= (uint32_t)(g->amt[i] != GA_AUTO) << (12u + i);
    bw(&w, m != 0u, 1);
    if (m) {
        static const uint8_t BITS[12] = {7, 7, 7, 7, 8, 4, 3, 2, 7, 7, 8, 8};
        bw(&w, m, 17);
        for (i = 0; i < 12u; i++)
            if ((m >> i) & 1u) bw(&w, BITS[i] == 7u ? code7(v[i]) : v[i], BITS[i]);
        for (i = 0; i < GP_N; i++) if ((m >> (12u + i)) & 1u) bw(&w, code7(g->amt[i]), 7);
    }
    for (i = 0; i < SL_N; i++) x |= g->var[i] ? 1u : 0u;
    for (i = 0; i < RL_N; i++) x |= g->svar[i] ? 2u : 0u;
    for (i = 0; i < 3u * SL_PARTS; i++) x |= (&g->secvar[0][0])[i] ? 4u : 0u;
    for (i = 0; i < GP_PINS; i++) x |= g->pin[i].what ? 8u : 0u;
    x |= g->arr ? 16u : 0u;
    bw(&w, x != 0u, 1);
    if (x) {
        bw(&w, x, 5);
        if (x & 1u) { for (i = 0, k = 0; i < SL_N; i++) k |= (uint32_t)(g->var[i] != 0u) << i; bw(&w, k, SL_N); for (i = 0; i < SL_N; i++) if (g->var[i]) bwv(&w, g->var[i]); }
        if (x & 2u) { for (i = 0, k = 0; i < RL_N; i++) k |= (uint32_t)(g->svar[i] != 0u) << i; bw(&w, k, RL_N); for (i = 0; i < RL_N; i++) if (g->svar[i]) bwv(&w, g->svar[i]); }
        if (x & 4u) {
            uint32_t sc, p;                             /* (the sections with any (3 bits), then each one's parts (6)) */
            for (sc = 0, k = 0; sc < 3u; sc++) for (p = 0; p < SL_PARTS; p++) k |= (uint32_t)(g->secvar[sc][p] != 0u) << sc;
            bw(&w, k, 3);
            for (sc = 0; sc < 3u; sc++) {
                uint32_t pm = 0;
                if (!((k >> sc) & 1u)) continue;
                for (p = 0; p < SL_PARTS; p++) pm |= (uint32_t)(g->secvar[sc][p] != 0u) << p;
                bw(&w, pm, SL_PARTS);
                for (p = 0; p < SL_PARTS; p++) if (g->secvar[sc][p]) bwv(&w, g->secvar[sc][p]);
            }
        }
        if (x & 8u) {
            for (i = 0, k = 0; i < GP_PINS; i++) k += g->pin[i].what != 0u;
            bw(&w, k - 1u, 2);
            for (i = 0; i < GP_PINS; i++)
                if (g->pin[i].what) { bw(&w, g->pin[i].what, 5); bwv(&w, g->pin[i].var); bw(&w, g->pin[i].seed, 14); }
        }
        if (x & 16u) bw(&w, g->arr, 2);
    }
    n = (w.n + 4u) / 5u;                                /* (the payload in characters, then the check) */
    if (n + 1u > CODE_MAX || w.n > 135u) return 0;
    {
        uint32_t at = 0, sum = code_sum(&w, n * 5u);
        for (i = 0; i < n; i++) out[i] = CK32[brd(&w, &at, 5)];
        out[n] = CK32[sum];
        out[n + 1u] = 0;
    }
    return n + 1u;
}
/* a code typed (spaces and dashes skipped, I L O U as 1 1 0 V) into a recipe; 0 ok, -1 not a code, -2 the check wrong */
static int code_take(const char *s, gset_t *g)
{
    bits_t w;
    uint32_t n = 0, at = 0, i, k, m, x, sum;
    int32_t vals[40];
    memset(&w, 0, sizeof w);
    for (; *s; s++) {
        char c = *s;
        int32_t d = -1;
        if (c == ' ' || c == '-' || c == '.' || c == '_') continue;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c == 'I' || c == 'L') c = '1';
        if (c == 'O') c = '0';
        if (c == 'U') c = 'V';
        for (i = 0; i < 32u; i++) if (CK32[i] == c) d = (int32_t)i;
        if (d < 0 || n >= 40u) return -1;
        vals[n++] = d;
    }
    if (n < 2u) return -1;
    for (i = 0; i + 1u < n; i++) bw(&w, (uint32_t)vals[i], 5);
    sum = code_sum(&w, (n - 1u) * 5u);
    if ((uint32_t)vals[n - 1u] != sum) return -2;
    memset(g, 0, sizeof *g);
    if (brd(&w, &at, 2) != CODE_VER) return -1;
    g->seed = (uint16_t)brd(&w, &at, 14);
    g->genre = (uint8_t)brd(&w, &at, 6);
    g->nlive = (uint8_t)(brd(&w, &at, 1) ? 6u : 4u);
    g->form = (uint8_t)brd(&w, &at, 2);
    g->energy = 50; g->complex = 50; g->swing = GA_AUTO; g->human = 30; g->fills = g->wet = g->grit = GA_AUTO;
    for (i = 0; i < GP_N; i++) g->amt[i] = GA_AUTO;
    if (brd(&w, &at, 1)) {
        static const uint8_t BITS[12] = {7, 7, 7, 7, 8, 4, 3, 2, 7, 7, 8, 8};
        uint8_t *v[12] = {&g->energy, &g->complex, &g->swing, &g->human, &g->tempo, &g->key, &g->scale, &g->length,
                          &g->space, &g->fills, &g->wet, &g->grit};
        m = brd(&w, &at, 17);
        for (i = 0; i < 12u; i++)
            if ((m >> i) & 1u) { k = brd(&w, &at, BITS[i]); *v[i] = (uint8_t)(BITS[i] == 7u ? uncode7(k) : k); }
        for (i = 0; i < GP_N; i++) if ((m >> (12u + i)) & 1u) g->amt[i] = (uint8_t)uncode7(brd(&w, &at, 7));
    }
    if (brd(&w, &at, 1)) {
        x = brd(&w, &at, 5);
        if (x & 1u) { k = brd(&w, &at, SL_N); for (i = 0; i < SL_N; i++) if ((k >> i) & 1u) g->var[i] = (uint8_t)brv(&w, &at); }
        if (x & 2u) { k = brd(&w, &at, RL_N); for (i = 0; i < RL_N; i++) if ((k >> i) & 1u) g->svar[i] = (uint8_t)brv(&w, &at); }
        if (x & 4u) {
            uint32_t sc, p, pm;
            k = brd(&w, &at, 3);
            for (sc = 0; sc < 3u; sc++) {
                if (!((k >> sc) & 1u)) continue;
                pm = brd(&w, &at, SL_PARTS);
                for (p = 0; p < SL_PARTS; p++) if ((pm >> p) & 1u) g->secvar[sc][p] = (uint8_t)brv(&w, &at);
            }
        }
        if (x & 8u) {
            k = brd(&w, &at, 2) + 1u;
            for (i = 0; i < k; i++) {
                g->pin[i].what = (uint8_t)brd(&w, &at, 5);
                g->pin[i].var = (uint8_t)brv(&w, &at);
                g->pin[i].seed = (uint16_t)brd(&w, &at, 14);
            }
        }
        if (x & 16u) g->arr = (uint8_t)brd(&w, &at, 2);
    }
    if (at > (n - 1u) * 5u || g->seed > 9999u || g->genre > NGENRE || g->form > 2u || g->energy > 100u || g->complex > 100u ||
        g->key > 12u || g->scale > 7u || g->arr > 3u)
        return -1;
    for (i = 0; i < GP_PINS; i++)
        if (g->pin[i].what && !((g->pin[i].what >= 1u && g->pin[i].what <= SL_N) || (g->pin[i].what >= 16u && g->pin[i].what < 16u + RL_N)) ) return -1;
    return 0;
}
/* (v2) SAVE on the reel: the NAME screen for a code (gen_code_ok makes its take); F#5 (OCT+ held): the take's code shown */
static void gen_code_enter(void)
{
    ui.confirm = 0;                                     /* (the NAME screen is a page of its own: the GEN screen back after) */
    name_open(NK_CODE, 0);
}
static void gen_code_back(void)                         /* cancelled: the reel again */
{
    confirm_open(CF_DICE_ALL, song.sel);
    gen_fresh = 1;
    gp.open_ms = 0;
    gp.view = 1;
    ui.force = 1;
}
static int gen_code_ok(const char *s)                   /* a code typed: its take made (1), or said why not (0) */
{
    gset_t g;
    int rc = code_take(s, &g);
    if (rc) { ui_message(rc == -2 ? "CODE: A TYPO (CHECK FAILED)" : "NOT A TAKE CODE"); return 0; }
    gen_code_back();
    gset_put(&g);
    gen_how = GH_NONE;
    gen_go();
    gp.sig = gen_sig();
    if (g.nlive != (trk_live < NTRK ? trk_live : NTRK)) {   /* (made on the other number of tracks: another piece) */
        ui_say(g.nlive > 4u ? "MADE ON 6 TRACKS" : "MADE ON 4 TRACKS", "");
        str_cpy(ui.msg2, "GLO>SYSTEM>TRACKS", sizeof ui.msg2);
    } else {
        ui_message("TAKE FROM ITS CODE");
    }
    return 1;
}

/* ------------------------------------------------------------------------------- (v2) the take layer's LEDs --- */
/* OCT+ held on the GEN screen: the keys that make takes breathe, a key down, a part dropped and a locked part (its notes'
 * key, its sound's) lit (ui_input.c key_leds) */
static int gen_keys_own(void) { return ui.confirm == CF_DICE_ALL && !ui.menu && gl.held; }
static uint32_t gen_layer_leds(uint32_t *br)
{
    uint32_t k, on = 0, can = 0;
    for (k = 0; k < 27u; k++) {
        uint32_t c = GKEY[k], lit = ((fm1_in.notes >> k) & 1u) != 0u;
        if (!c) continue;
        can |= 1u << k;
        if (c >= SL_DRUMS && c <= SL_TEX) lit |= (gp.lock >> c) & 1u;
        else if (c >= 0x10u && c < 0x10u + RL_N) lit |= (gp.lock >> (8u + c - 0x10u)) & 1u;
        else if (c >= GK_DROP && c < GK_DROP + SL_PARTS) lit |= (gl.drop >> (c - GK_DROP)) & 1u;
        else if (c == GK_JAM) lit |= jam.on;
        on |= lit << k;
    }
    if (br) *br = can & ~on;
    return on;
}
