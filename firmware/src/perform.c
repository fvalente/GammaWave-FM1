/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PERFORM: the effects of the FX hold layer, on the master. A key pressed while FX is held belongs to the
 * layer (seq.c keyboard_block): it plays no note, sends no MIDI, records nothing, and holds its effect until
 * it is let go (ui_input.c opens the layer and shows the map). The 5 white keys from the left (F3 .. C4):
 *   REPEAT 1/8 1/16 1/32, LPF | HPF; then (JIANT) the 10 punch-in MIDI effects D4 .. F5 (pfx.c); black keys 1..4
 *   (F#3 G#3 A#3 C#4): tracks 1..4 muted while held (not P_MUTE, never saved); A#4 the MIDI effects' tracks.
 * TONIC keeps only the audio stutters (REPEAT): REVERSE, TAPE STOP, FREEZE and OCT UP / DN (the harmonizer) were
 * removed; the punch-in effects of TONIC act on the notes instead (FELUCCA-TONIC-SPEC.md).
 * REPEAT starts on the next 1/16 (at once while stopped); the filters at once; all end when let go, with a 2.9 ms
 * ramp. Effects of different kinds stack; of the REPEATs the last pressed plays, and letting it go returns to the
 * one held before.
 * The buffer: the SLICER's recordings (sl_buf, 32 KB) borrowed as one stereo loop of 8192 frames at 22.05 kHz
 * (371 ms). While borrowed, STUT tracks play live; their recordings are dropped afterwards. A REPEAT 1/8 longer
 * than the loop at the tempo (below 81 BPM) does nothing (the map shows it dimmed).
 * KNOB 1..4 with FX: the macros FILTER (the LPF / HPF), CRUSH, THROW (the dry mix into the delay and reverb
 * sends), DEPTH (the REPEAT's level).
 * MENU > FX LATCH ON (#40, a hand that cannot hold FX, a key and a knob together): a key pressed with FX turns its
 * effect on, the next press turns it off (perf_latched; letting the key or FX go does nothing), the macros keep
 * their values when FX is let go, FX + OCT- turns everything off (ui_layer.c), so does the menu or a dialog.
 * Chain: REPEAT -> LPF -> HPF -> CRUSH, after the master level and before
 * master_out (the limiter); THROW and the mutes act before the buses (fx.c mix_block / mix_part).
 * Idle (no key, no knob, no ramp left) every stage is skipped: the output is bit-identical. */
enum { PF_R8, PF_R16, PF_R32, PF_LPF, PF_HPF,
       PF_OCTD, PF_OCTU, PF_HALF, PF_DSHT, PF_DLNG, PF_S8, PF_S16, PF_ATK, PF_ARP, PF_RND,   /* the MIDI ones: pfx.c */
       PF_M1, PF_N = PF_M1 + NTRK, PF_TGT = PF_N + 1,     /* PF_TGT: A#4, the MIDI effects' tracks (pfx_tgt) */
       PF_CLR = PF_N + 2,                                  /* PF_CLR: G5, erases the punch-in lane (pfx.c) */
       /* (QG-1) the black keys left over: G#4 VIBRATO and F#5 DIVE on the notes (pfx.c), C#5 GATE and D#5 SWEEP on
        * the master (here) */
       PF_VIB = PF_N + 3, PF_GATE, PF_SWP, PF_DIVE, PF_END };
#define PF_IS(e) ((e) < PF_N || ((e) >= PF_VIB && (e) < PF_END))   /* a key with an effect */
_Static_assert(PF_END <= 32, "the PF bits in a word");
#define PF_BIT(e) (1u << (e))
#define PF_REPEAT 0x7u                                         /* the three REPEAT rates */
#define PF_Q PF_REPEAT                                         /* start on the 1/16 */
#define PF_BUF PF_REPEAT                                       /* the buffer effects */
#define PF_MUTE (((1u << NTRK) - 1u) << PF_M1)
#define PF_MIDI ((((1u << (PF_RND + 1)) - 1u) & ~((1u << PF_OCTD) - 1u)) | PF_BIT(PF_VIB) | PF_BIT(PF_DIVE))   /* PF_OCTD .. PF_RND,
                                                                 * VIB, DIVE: on the notes (pfx.c) */
#define PB_FRAMES SL_FR                       /* stereo frames in sl_buf (a power of 2: the mask), 371 ms at 22.05 kHz */
_Static_assert(PB_FRAMES <= SL_FR && !(PB_FRAMES & (PB_FRAMES - 1u)), "the REPEAT loop in sl_buf");
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
#define PF_TOP (63 << 8)                      /* filter cutoff index, Q8 (PF_SVF): the LPF's open end */
#define PF_LOW (14 << 8)                      /* .. the LPF sweep's end, the HPF sweep's (and K1's) top */
enum { BM_NONE, BM_LOOP };

static volatile uint32_t perf_mask;   /* main: the FX button's bit while its layer may own keys, 0 = none */
static volatile uint32_t kb_mask;     /* main: the button bits whose hold makes keys a layer's (ui_layer.c), 0 = none */
static volatile uint8_t perf_solo;    /* main: tracks soloed in the GLO layer (the others muted as by a black key) */
static volatile uint32_t scene_pfx;  /* (JIANT) the punch-in MIDI effects a song row's scene holds (song_play.c), PF bits */
static volatile uint8_t perf_kill;    /* main: every effect off (the menu, a dialog, UBOOT) */
static volatile int8_t perf_k[4];     /* main: the knob macros, 0 = untouched: FILTER -100..100 (- LPF, + HPF),
                                       * CRUSH 0..100, THROW 0..100, DEPTH cut 0..100 (the buffer effects' level) */
static volatile uint32_t kb_layer;    /* ISR: the keys held that are the layer's, a bit per key */
static volatile uint32_t perf_held;   /* ISR: the effects held (PF_*) */
static volatile uint8_t perf_latch_on;   /* main: MENU > FX LATCH ON (#40): a key with FX toggles its effect */
static volatile uint32_t perf_latched;   /* ISR: the effects latched (FX LATCH; main clears it: OCT-, the menu) */
static volatile uint32_t perf_act;    /* ISR: .. running (a 1/16 one from its start on) */
static uint32_t perf_ord[PF_END], perf_seq;   /* press order: the last pressed buffer effect plays */

/* the ZDF state-variable filter at cutoff index i (30 Hz * 2^(i / 7), 0..63: up to 15.4 kHz), k = 1:
 * a1 a2 a3 in Q14 (tools: g = tan(pi fc / fs), a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2) */
static const int16_t PF_SVF[64][3] = {
    {16349, 35, 0}, {16345, 39, 0}, {16341, 43, 0}, {16337, 47, 0}, {16332, 52, 0}, {16327, 57, 0}, {16321, 63, 0},
    {16314, 70, 0}, {16307, 77, 0}, {16299, 85, 0}, {16290, 94, 1}, {16280, 103, 1}, {16269, 114, 1},
    {16257, 126, 1}, {16244, 139, 1}, {16229, 153, 1}, {16213, 169, 2}, {16196, 186, 2}, {16176, 205, 3},
    {16154, 227, 3}, {16130, 250, 4}, {16104, 275, 5}, {16075, 303, 6}, {16043, 334, 7}, {16007, 368, 8},
    {15968, 406, 10}, {15925, 447, 13}, {15877, 492, 15}, {15824, 541, 19}, {15766, 596, 22}, {15702, 655, 27},
    {15631, 720, 33}, {15553, 791, 40}, {15467, 869, 49}, {15372, 953, 59}, {15267, 1046, 72}, {15151, 1146, 87},
    {15024, 1255, 105}, {14883, 1374, 127}, {14729, 1502, 153}, {14559, 1640, 185}, {14372, 1790, 223},
    {14166, 1950, 268}, {13940, 2121, 323}, {13692, 2304, 388}, {13420, 2499, 465}, {13122, 2704, 557},
    {12797, 2921, 667}, {12441, 3147, 796}, {12053, 3382, 949}, {11631, 3624, 1129}, {11173, 3870, 1341},
    {10677, 4119, 1589}, {10140, 4365, 1879}, {9562, 4605, 2218}, {8940, 4832, 2612}, {8274, 5040, 3070},
    {7563, 5219, 3602}, {6807, 5359, 4219}, {6006, 5444, 4934}, {5163, 5456, 5766}, {4282, 5369, 6733},
    {3372, 5149, 7863}, {2449, 4745, 9190},
};

static struct {
    uint32_t ph, P, split;             /* samples since play; the 1/16; this block: where a 1/16 starts */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync, mute;          /* sync: a 1/16 starts with the next block; mute: tracks to ramp */
    /* the buffer */
    uint8_t mode, src, next;           /* BM_*, the effect it plays (PF_N none), one waiting for the fade */
    uint8_t state, odd;                /* LOOP: 0 recording, 1 playing; the sample of a pair */
    uint32_t len, wr, rp;              /* loop length (samples), frames written, read (samples into the loop) */
    uint32_t xrp, xlen, xn;            /* the loop it switched from, faded over xn samples */
    int32_t w, tw, hl, hr;             /* the buffer's share and its target, Q15; the first of a pair */
    /* the filters, CRUSH, THROW, mutes */
    int32_t lc, hc, la, ha;            /* cutoffs (Q8 index) and shares (Q15) of LPF and HPF */
    int32_t lz[4], hz[4];              /* their states: L ic1 ic2, R ic1 ic2 */
    int16_t lk[3], hk[3];              /* their coefficients this block */
    uint32_t lt, ht;                   /* samples the LPF / HPF key has been held */
    int32_t cw, cc, chl, chr;          /* CRUSH share, amount, held sample */
    uint32_t cn;
    int32_t td;                        /* THROW: the share of the dry mix sent, Q15 */
    int32_t mg[NTRK];                  /* mute gains, Q15 (32768 = open) */
    /* (QG-1) GATE: the cut now (Q15, 0 = open), the place in the 1/16 at the block's start; SWEEP: samples held */
    int32_t gc;
    uint32_t gph, swt;
} pf = {.src = PF_N, .next = PF_N, .lc = PF_TOP, .mg = {[0 ... NTRK - 1] = 32768}};   /* (every track open: 0.6, six) */

/* the loop length of a REPEAT at the tempo, 44.1 kHz samples; 0 = not one */
static const uint8_t PF_DEN[PF_R32 + 1] = {2, 4, 8};
static uint32_t perf_len(uint32_t e) { return e <= PF_R32 ? beat_samples() / PF_DEN[e] : 0u; }
/* the effects that can run at the tempo: a REPEAT longer than the loop cannot */
static uint32_t perf_avail(void)
{
    uint32_t e, m = ~0u, b = beat_samples();
    for (e = PF_R8; e <= PF_R32; e++)
        if (b > PB_MAX * PF_DEN[e])
            m &= ~PF_BIT(e);
    return m;
}

/* seq.c keyboard_block: a layer key down / up (e: PF_*, PF_N = no effect) */
static volatile uint8_t pfx_tgt;      /* main / keys: the MIDI effects' tracks, 0 ALL, 1 SYN, 2 DRM (pfx.c) */
static volatile uint8_t pfx_clr;      /* keys: G5 held (pfx.c: the lane erased where it passes, or all of it) */
static uint32_t pfx_lph;              /* samples into the punch-in lane (pfx.c): from the transport's start, a section's */
static uint8_t pfx_lane[32], pfx_ltgt;   /* the section's punch-in lane (pfx.c), its tracks (as pfx_tgt) */
static void perf_press(uint32_t e, int down)
{
    if (e == PF_TGT && down)
        pfx_tgt = (uint8_t)((pfx_tgt + 1u) % 3u);
    if (e == PF_CLR)
        pfx_clr = (uint8_t)(down ? 1u : 0u);
    if (!PF_IS(e))
        return;
    if (perf_latch_on) {                  /* FX LATCH: a press turns it on or off, letting go does nothing */
        if (down) {
            perf_latched ^= PF_BIT(e);
            perf_ord[e] = ++perf_seq;
        }
        return;
    }
    if (down) {
        perf_held |= PF_BIT(e);
        perf_ord[e] = ++perf_seq;
    } else {
        perf_held &= ~PF_BIT(e);
    }
}

static void perf_start(void) { pf.ph = 0; pf.sync = 1; pfx_lph = 0; }   /* seq_start: a 1/16 starts with the transport */

/* the SLICER's recordings dropped (when the buffer is taken and given back) */
static void perf_drop_slicer(void) { slicer_drop(); }

/* the buffer effect to play: the last pressed of those running */
static uint32_t perf_pick(uint32_t a)
{
    uint32_t e, best = PF_N;
    a &= PF_BUF;
    for (e = 0; e < PF_M1; e++)
        if (((a >> e) & 1u) && (best == PF_N || perf_ord[e] - perf_ord[best] < 0x80000000u))
            best = e;
    return best;
}

static void perf_buf_start(uint32_t e)
{
    if (!sl_lent) {
        sl_lent = 1;
        perf_drop_slicer();
    }
    pf.src = (uint8_t)e;
    pf.next = PF_N;
    pf.wr = pf.rp = 0;
    pf.odd = 0;
    pf.xn = 0;
    pf.mode = BM_LOOP;
    pf.state = 0;
    pf.len = perf_len(e);
}

/* the buffer effect to play becomes e (at a 1/16): a shorter REPEAT of what is
 * recorded switches with a cross-fade, anything else fades the old one out first */
static void perf_buf_select(uint32_t e)
{
    if (e == pf.src && pf.next == PF_N)
        return;
    if (e != PF_N && pf.mode == BM_LOOP && pf.state && e <= PF_R32 && pf.src != PF_N &&
        perf_len(e) <= 2u * pf.wr) {
        pf.xrp = pf.rp;
        pf.xlen = pf.len;
        pf.xn = SL_RAMP;
        pf.src = (uint8_t)e;
        pf.next = PF_N;
        pf.len = perf_len(e);
        pf.rp = 0;
        return;
    }
    if (pf.mode == BM_NONE || !pf.w) {
        if (e == PF_N) {
            pf.src = PF_N;
            pf.next = PF_N;
        } else {
            perf_buf_start(e);
        }
        return;
    }
    pf.src = PF_N;                                  /* fade out, then the next one (perf_seg) */
    pf.next = (uint8_t)e;
}

/* each block, after the keys (seq.c) and before the parts: what runs, the 1/16, the filters, the mutes.
 * Returns 1 when a stage has something to do */
/* (0.6.19) a track let go from its mute, silent (its voices ended at 0, perf_hush_end): nothing to ramp back, open at
 * once; else mix_part never reached perf_mute again (no signal) and the FX layer stayed busy every block */
static __attribute__((noinline)) void perf_unhush(void)   /* (out of perf_begin: target_budget.py) */
{
    uint32_t k, i, on;
    for (k = 0; k < NTRK; k++)
        if (!pf.mg[k] && !((pf.act >> (PF_M1 + k)) & 1u)) {
            for (i = on = 0; i < NVOICE; i++)
                on |= trk[k].v[i].active;
            if (!on)
                pf.mg[k] = 32768;
        }
}
static __attribute__((noinline)) int perf_begin(uint32_t n)
{
    uint32_t held = perf_kill ? 0u : (perf_held | perf_latched | (perf_solo ? (~(uint32_t)perf_solo & ((1u << NTRK) - 1u)) << PF_M1 : 0u)) & perf_avail() & ~PF_MIDI;
    uint32_t q, k, ph0, bnd;
    int32_t m;
    if (!held && !pf.busy && !(perf_k[0] | perf_k[1] | perf_k[2])) {   /* idle: the clock only */
        pf.ph += n;
        pf.sync = 0;
        perf_act = 0;
        trk_hush = 0;
        return 0;
    }
    pf.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    if (song.playing && clk_pos != CLK_START)       /* (JIANT) playing: the transport's 1/16 (seq.c click_tick, through
                                                     * tempo changes and an external clock), so a REPEAT starts on the grid */
        pf.P = div_samples(2);
    ph0 = (song.playing && clk_pos != CLK_START ? clk_pos : pf.ph) % pf.P;   /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.P - ph0 < n ? pf.P - ph0 : n) : 0u;
    pf.ph += n;
    pf.act &= held;                                 /* let go: at once */
    pf.act |= held & ~PF_Q;
    pf.act0 = pf.act;
    q = held & PF_Q & ~pf.act;
    pf.split = n;
    if (q && (!song.playing || bnd < n)) {
        pf.split = song.playing ? bnd : 0u;
        pf.act |= q;
    }
    perf_act = pf.act;
    /* the filters: LPF from open down over a bar while its key is held, KNOB 1 left; HPF up, KNOB 1 right */
    m = perf_k[0];
    {
        uint32_t bar = 16u * pf.P, t;
        int32_t lt = PF_TOP, ht = 0;
        pf.lt = (pf.act & PF_BIT(PF_LPF)) ? pf.lt + n : 0u;
        pf.ht = (pf.act & PF_BIT(PF_HPF)) ? pf.ht + n : 0u;
        if (pf.lt) {
            t = pf.lt < bar ? pf.lt : bar;
            lt = PF_TOP - (int32_t)((uint32_t)(PF_TOP - PF_LOW) / 64u * (t * 64u / bar));
        }
        if (m < 0 && PF_TOP + m * (PF_TOP - PF_LOW) / 100 < lt)
            lt = PF_TOP + m * (PF_TOP - PF_LOW) / 100;
        pf.swt = (pf.act & PF_BIT(PF_SWP)) ? pf.swt + n : 0u;
        if (pf.swt) {                               /* (QG-1) SWEEP: the LPF down and back up over a bar, again */
            uint32_t q = (pf.swt % bar) * 128u / bar;   /* (0..127: a triangle, open at both ends) */
            int32_t sw = PF_TOP - (int32_t)((uint32_t)(PF_TOP - PF_LOW) / 64u * (q < 64u ? q : 127u - q));
            if (sw < lt)
                lt = sw;
        }
        if (pf.ht) {
            t = pf.ht < bar ? pf.ht : bar;
            ht = (int32_t)((uint32_t)(PF_TOP - PF_LOW) / 64u * (t * 64u / bar));
        }
        if (m > 0 && m * (PF_TOP - PF_LOW) / 100 > ht)
            ht = m * (PF_TOP - PF_LOW) / 100;
        pf.lc += clamp(lt - pf.lc, -192, 192);      /* ~45 ms over the range */
        pf.hc += clamp(ht - pf.hc, -192, 192);
        for (k = 0; k < 3u; k++) {                  /* the coefficients, between two table rows */
            int32_t i = pf.lc >> 8, f = pf.lc & 255, j = pf.hc >> 8, g = pf.hc & 255;
            pf.lk[k] = (int16_t)(PF_SVF[i][k] + (((PF_SVF[i < 63 ? i + 1 : 63][k] - PF_SVF[i][k]) * f) >> 8));
            pf.hk[k] = (int16_t)(PF_SVF[j][k] + (((PF_SVF[j < 63 ? j + 1 : 63][k] - PF_SVF[j][k]) * g) >> 8));
        }
    }
    pf.gph = ph0;                                   /* (QG-1) GATE: the place in the 1/16 */
    pf.mute = 0;
    perf_unhush();
    for (k = 0; k < NTRK; k++)
        if (((held >> (PF_M1 + k)) & 1u) || pf.mg[k] != 32768)
            pf.mute |= (uint8_t)(1u << k);
    trk_hush = (uint8_t)((pf.act >> PF_M1) & ((1u << NTRK) - 1u));   /* (0.6.15) muted: no new notes (voice.c) */
    pf.busy = 1;
    return 1;
}

/* a track muted by its black key: its signal ramps to 0 and back (fx.c mix_part, after the SLICER) */
static __attribute__((noinline)) void perf_hush_end(uint32_t k)   /* (out of perf_mute's loop: target_budget.py) */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[k].v[i];
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = v->env_out = 0;
    }
}
static __attribute__((noinline)) void perf_mute(uint32_t k, int32_t *b, uint32_t n)
{
    uint32_t i;
    int32_t g = pf.mg[k], t = (pf.act >> (PF_M1 + k)) & 1u ? 0 : 32768;
    for (i = 0; i < n; i++) {
        g += clamp(t - g, -SL_SLOPE, SL_SLOPE);
        b[i] = mulq16(b[i], (uint32_t)g << 1);
    }
    pf.mg[k] = g;
    if (!g && !t)                                       /* (0.6.15) silent: its voices end, the track costs nothing
                                                         * while muted (a MIDI mute: no new notes, trk_hush) */
        perf_hush_end(k);
}

/* before the buses: THROW (KNOB 3) adds the dry mix (and the SLICER's bus, bl / br) to the delay and reverb sends */
static __attribute__((noinline)) void perf_pre(const int32_t *ml, const int32_t *mr, const int32_t *bl, const int32_t *br,
                                               int32_t *sd, int32_t *sr, uint32_t n)
{
    uint32_t i;
    int32_t m = perf_k[2] * 327;
    for (i = 0; i < n; i++) {
        pf.td += clamp(m - pf.td, -SL_SLOPE, SL_SLOPE);
        if (pf.td) {
            int32_t x = mulq16((ml[i] + mr[i] + bl[i] + br[i]) >> 1, (uint32_t)pf.td << 1);
            sd[i] += x;
            sr[i] += x;
        }
    }
}

/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}

/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* sample s of the frames recorded from 0 (n of them): between two frames on an odd one */
static inline void pb_at(uint32_t s, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 1 < n ? s >> 1 : n - 1u;    /* (an odd loop's last sample: the frame before) */
    const int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    const int16_t *q;                               /* a frame is the mean of samples 2f, 2f + 1: sample s */
    if (s & 1u)                                     /* lies a quarter frame from it, towards f + 1 or f - 1 */
        q = f + 1u < n ? p + 2 : p;
    else
        q = f ? p - 2 : p;
    *l = (3 * p[0] + q[0]) >> 1;
    *r = (3 * p[1] + q[1]) >> 1;
}
/* REPEAT at sample rp of a loop of len, windowed at both ends */
static inline void pb_loop(uint32_t rp, uint32_t len, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t e = len - 1u - rp;
    pb_at(rp, n, l, r);
    if (rp < e)
        e = rp;
    if (e < (uint32_t)SL_RAMP) {
        *l = (*l * (int32_t)e) >> SL_RAMP_LOG2;
        *r = (*r * (int32_t)e) >> SL_RAMP_LOG2;
    }
}

/* record the pair into frame f (the mean, >> 1: headroom) */
static inline uint32_t pb_rec(uint32_t f, int32_t l, int32_t r)
{
    if (!pf.odd) {
        pf.hl = l;
        pf.hr = r;
        pf.odd = 1;
        return f;
    }
    pf.odd = 0;
    pb_put(f, (pf.hl + l) >> 2, (pf.hr + r) >> 2);
    return f + 1u;
}

/* the buffer effect faded out: the next one, or the buffer back. Once per effect, out of the per-sample loop
 * (noinline: its calls stay out of perf_block's loop code) */
static __attribute__((noinline)) void perf_buf_done(void)
{
    if (pf.next != PF_N) {
        perf_buf_start(pf.next);
    } else {
        pf.mode = BM_NONE;
        if (sl_lent) {
            sl_lent = 0;
            perf_drop_slicer();
        }
    }
}

/* the buffer effect on one sample: the share pf.w of what it plays, the rest live */
static inline void perf_buf(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl = 0, yr = 0, on = 0;
    if (pf.mode == BM_LOOP) {
        if (!pf.state) {
            pf.wr = pb_rec(pf.wr, l, r);
            if (++pf.rp >= pf.len) {                /* (rp: samples recorded) */
                pf.state = 1;
                pf.rp = 0;
            }
        } else {
            pb_loop(pf.rp, pf.len, pf.wr, &yl, &yr);
            if (pf.xn) {                            /* the loop it switched from, fading out */
                int32_t ol, orr, g = (int32_t)pf.xn;
                pb_loop(pf.xrp, pf.xlen, pf.wr, &ol, &orr);
                yl += (ol * g) >> SL_RAMP_LOG2;
                yr += (orr * g) >> SL_RAMP_LOG2;
                pf.xn--;
                if (++pf.xrp >= pf.xlen)
                    pf.xrp = 0;
            }
            if (++pf.rp >= pf.len)
                pf.rp = 0;
            on = 1;
        }
    }
    pf.tw = on && pf.src != PF_N ? 32768 - perf_k[3] * 327 : 0;
    pf.w += clamp(pf.tw - pf.w, -SL_SLOPE, SL_SLOPE);
    if (pf.w) {
        *pl = pf_mix(l, yl, pf.w);
        *pr = pf_mix(r, yr, pf.w);
    } else if (pf.src == PF_N) {                    /* faded out: the next one, or the buffer back */
        perf_buf_done();
    }
}

/* one channel of a state-variable filter (k = 1): x in Q15 >> 2 */
static inline int32_t pf_svf(int32_t x, int32_t *z, const int16_t *c, int hp)
{
    int32_t v3 = x - z[1], v1 = (c[0] * z[0] + c[1] * v3) >> 14, v2 = z[1] + ((c[1] * z[0] + c[2] * v3) >> 14);
    z[0] = 2 * v1 - z[0];
    z[1] = 2 * v2 - z[1];
    return hp ? x - v1 - v2 : v2;
}

static __attribute__((noinline)) void perf_switch(uint32_t a) { perf_buf_select(perf_pick(a)); }

/* after the master level: the buffer effect, LPF, HPF, CRUSH on l / r (stereo, Q15) */
static __attribute__((noinline)) void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0, sh, hold, gp = pf.P ? pf.gph % pf.P : 0u, gh = pf.P / 2u;
    int32_t c = perf_k[1], lt, ht, ct, gd = 32768 - perf_k[3] * 327;
    if (c)
        pf.cc = c;
    ct = c ? 32768 : 0;
    sh = 4u + (uint32_t)pf.cc * 7u / 100u;
    hold = sh - 3u;
    if (pf.split)
        perf_switch(a);
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pf.mode != BM_NONE)
            perf_buf(&l, &r);
        lt = (a & PF_BIT(PF_LPF)) || perf_k[0] < 0 || pf.lc < PF_TOP ? 32768 : 0;
        pf.la += clamp(lt - pf.la, -SL_SLOPE, SL_SLOPE);
        if (pf.la) {
            int32_t fl = pf_svf(clamp(l >> 2, -32767, 32767), &pf.lz[0], pf.lk, 0) << 2;
            int32_t fr = pf_svf(clamp(r >> 2, -32767, 32767), &pf.lz[2], pf.lk, 0) << 2;
            l = pf_mix(l, fl, pf.la);
            r = pf_mix(r, fr, pf.la);
        } else {
            pf.lz[0] = pf.lz[1] = pf.lz[2] = pf.lz[3] = 0;
        }
        ht = (a & PF_BIT(PF_HPF)) || perf_k[0] > 0 || pf.hc > 0 ? 32768 : 0;
        pf.ha += clamp(ht - pf.ha, -SL_SLOPE, SL_SLOPE);
        if (pf.ha) {
            int32_t fl = pf_svf(clamp(l >> 2, -32767, 32767), &pf.hz[0], pf.hk, 1) << 2;
            int32_t fr = pf_svf(clamp(r >> 2, -32767, 32767), &pf.hz[2], pf.hk, 1) << 2;
            l = pf_mix(l, fl, pf.ha);
            r = pf_mix(r, fr, pf.ha);
        } else {
            pf.hz[0] = pf.hz[1] = pf.hz[2] = pf.hz[3] = 0;
        }
        {   /* CRUSH: fewer bits, a held sample (KNOB 2) */
            pf.cw += clamp(ct - pf.cw, -SL_SLOPE, SL_SLOPE);
            if (pf.cw) {
                if (++pf.cn >= hold) {
                    pf.cn = 0;
                    pf.chl = ((l >> sh) << sh) + (1 << (sh - 1u));
                    pf.chr = ((r >> sh) << sh) + (1 << (sh - 1u));
                }
                l = pf_mix(l, pf.chl, pf.cw);
                r = pf_mix(r, pf.chr, pf.cw);
            }
        }
        {   /* (QG-1) GATE: each 1/16 open for its first half, shut for the rest (KNOB 4 DEPTH: shut less) */
            int32_t gt = (a & PF_BIT(PF_GATE)) && gp >= gh ? gd : 0;
            if (++gp >= pf.P)
                gp = 0;
            if (gt | pf.gc) {
                pf.gc += clamp(gt - pf.gc, -SL_SLOPE, SL_SLOPE);
                l = mulq16(l, (uint32_t)(32768 - pf.gc) << 1);
                r = mulq16(r, (uint32_t)(32768 - pf.gc) << 1);
            }
        }
        bl[i] = l;
        br[i] = r;
    }
    pf.busy = pf.act || pf.mode != BM_NONE || pf.w || pf.la || pf.ha || pf.cw || pf.td || pf.mute || pf.gc || perf_k[0] ||
              perf_k[1] || perf_k[2];
}
