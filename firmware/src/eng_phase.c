/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Phase distortion. The waveforms are a C port of the oscillator of
 * CrispyZebra (Leo Kuroshita, GPL-3.0, github.com/hugelton/CrispyZebra), the
 * author's own phase-distortion core: a -cos table read through a bent phase whose
 * bend is DCW. WAVE2 alternates with WAVE every other cycle. A second
 * line (DTN) can be mixed or ring-modulated; SUB adds a sine an octave down.
 * Envelopes are Felucca's own (ADSR, ENV -> DCW).
 * (JIANT 0.4) FB: phase feedback, line 1's output (the mean of its last two samples, as a DX operator's) bends its
 * own phase further, up to a quarter cycle: from a harder edge through growl to noise; the ENV / LFO / matrix move it
 * as E8. */
static const char *const N_PD_WAVE[] = {"SAW", "SQR", "PLS", "DSIN", "SPLS", "RSAW", "RTRI", "RTRP"};
static const char *const N_PD_WAVE2[] = {"-", "SAW", "SQR", "PLS", "DSIN", "SPLS", "RSAW", "RTRI", "RTRP"};
static const char *const N_PD_LINE[] = {"MIX", "RING"};

static inline int32_t pd_cos(uint32_t ph16)             /* -cos, Q15, from the 16-bit PD phase */
{
    return -sine_i(((ph16 & 0xFFFFu) << 16) + 0x40000000u);
}

/* one wave's bend for a block: break points and the slopes of the bent phase
 * (Q16 reciprocals), so a sample costs a multiply instead of a divide (the
 * bent phase comes out the same or 1 lower in 65536) */
typedef struct {
    uint32_t w, x, peak, rez;
    uint32_t k0, k1;
} pd_t;

static inline uint32_t pd_slope(uint32_t span, uint32_t len) { return (span << 16) / (len ? len : 1u); }

static void pd_setup(pd_t *b, uint32_t w, uint32_t dcw)
{
    uint32_t x;
    b->w = w;
    switch (w) {
    case 0:                                                  /* SAW */
        x = 32768u + ((dcw * 32767u) >> 16);
        b->k0 = pd_slope(32768u, x);
        b->k1 = pd_slope(32767u, 65535u - x);
        break;
    case 1:                                                  /* SQUARE */
        x = 32768u - ((dcw * 31120u) >> 16);
        b->k0 = pd_slope(32768u, x);
        b->k1 = pd_slope(32767u, x);
        break;
    case 2:                                                  /* PULSE */
        x = 65535u - ((dcw * 63487u) >> 16);
        b->k0 = pd_slope(65535u, x);
        break;
    case 3:                                                  /* DOUBLE SINE */
        x = 65535u - ((dcw * 49151u) >> 16);
        b->k0 = pd_slope(65535u, x);
        b->k1 = pd_slope(65535u, 65535u - x);
        break;
    case 4:                                                  /* SAW-PULSE */
        x = 65535u - ((dcw * 32767u) >> 16);
        b->peak = (x * (32768u + ((dcw * 29491u) >> 16))) >> 16;
        if (!b->peak)
            b->peak = 1;
        b->k0 = pd_slope(32768u, b->peak);
        b->k1 = pd_slope(32767u, x - b->peak);
        break;
    default:                                                 /* RESONANCE */
        x = 0;
        b->rez = (dcw * 44000u) >> 16;
        break;
    }
    b->x = x;
}

/* one sample at phase ph (16 bit): bipolar Q15 */
static int32_t pd_wave(const pd_t *b, uint32_t ph)
{
    uint32_t pd, x = b->x;
    switch (b->w) {
    case 0:                                                  /* SAW */
        pd = ph < x ? (ph * b->k0) >> 16 : 32768u + (((ph - x) * b->k1) >> 16);
        break;
    case 1:                                                  /* SQUARE */
        if (ph < x)
            pd = (ph * b->k0) >> 16;
        else if (ph < 32768u)
            pd = 32768u;
        else if (ph < 32768u + x)
            pd = 32768u + (((ph - 32768u) * b->k1) >> 16);
        else
            pd = 65535u;
        break;
    case 2:                                                  /* PULSE */
        pd = ph < x ? (ph * b->k0) >> 16 : 65535u;
        break;
    case 3:                                                  /* DOUBLE SINE */
        pd = ph < x ? (ph * b->k0) >> 16 : ((ph - x) * b->k1) >> 16;
        break;
    case 4:                                                  /* SAW-PULSE */
        if (ph >= x)
            pd = 65535u;
        else if (ph < b->peak)
            pd = (ph * b->k0) >> 16;
        else
            pd = 32768u + (((ph - b->peak) * b->k1) >> 16);
        break;
    default: {                                               /* RESONANCE: windowed sine, rez = pitch of the core */
        uint32_t rp = (ph + ((ph * b->rez) >> 16) * 7u) & 0xFFFFu, win;
        int32_t core = (pd_cos(rp) * 5) >> 3;
        if (b->w == 5)
            win = 65535u - ph;                               /* saw window */
        else if (b->w == 6)
            win = ph < 32768u ? ph << 1 : (65535u - ph) << 1; /* triangle window */
        else
            win = ph < 16384u ? ph << 2 : ph > 49152u ? (65535u - ph) << 2 : 65535u;   /* trapezoid */
        if (win > 65535u)
            win = 65535u;
        return (core * (int32_t)win) >> 16;                  /* |core| <= 20479: fits 32 bits */
    }
    }
    return pd_cos(pd);
}

static void phase_note_on(track_t *t, voice_t *v)
{
    v->s[4] = t->p[P_E7] << 8;   /* (0.6.81) FB as it plays, Q8 (glides to the knob's) */
    v->ph[1] = 0;
    v->ph[2] = 0;
    v->s[0] = 0;                 /* WAVE / WAVE2 toggle, line 1 */
    v->s[1] = 0;                 /* line 2 */
    v->s[2] = v->s[3] = 0;       /* (FB) line 1's last two samples */
}

static void phase_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t w1 = (uint32_t)p[P_E0] & 7u, w2 = (uint32_t)p[P_E1], i;
    int32_t depth = (p[P_E2] << 8) + m->cutoff + mulq15(m->envq15, p[P_E3] * 256);
    uint32_t dcw, inc = m->inc, inc2;
    int32_t det = p[P_E4], line2 = det != 0 || p[P_E5], ring = p[P_E5], sub = p[P_E6] * 200;
    int32_t d16 = det * 16 / 100, rem = det * 16 - d16 * 100;   /* (cents_inc(), written out: the host
                                                                 * compiler makes the loop slower with it) */
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t tg0 = v->s[0], tg1 = v->s[1];                    /* WAVE / WAVE2 toggles */
    int32_t fb = v->s[4], f1 = v->s[2], f2 = v->s[3];        /* (JIANT 0.4) FB and its two samples; (0.6.81) FB
                                                              * ramped across the block to the knob's (Q8: a step of the
                                                              * knob jumped the phase: a click, louder the higher it was) */
    int32_t dfb = ((p[P_E7] << 8) - fb) / (int32_t)(n ? n : 1u);
    pd_t b1, b2, bm;
    int32_t mf = 0, dmf = 0;                                 /* (0.6.45) MORPH: line 1 towards the next WAVEs; (0.6.50)
                                                              * its blend ramped across the block, Q8 */
    depth = clamp(depth + (m->shape - (64 << 8)), 0, 127 << 8);
    dcw = (uint32_t)depth * 65535u / (127u << 8);
    dcw = (dcw * 56000u) >> 16;                              /* the classic range of the bend */
    inc2 = pitch_inc(clamp(m->pitch16 + d16, 0, 2047));
    inc2 += (uint32_t)((int32_t)(inc2 >> 12) * (rem * 2367 / 16000));
    if (t->mo_cur || t->mo_prev) {
        int32_t pos = (int32_t)w1 * 256 + t->mo_cur * 3 / 127, p0 = (int32_t)w1 * 256 + t->mo_prev * 3 / 127;   /* (x 768 / (127 << 8)) */
        w1 = ((uint32_t)pos >> 8) & 7u;
        mf = (pos & 255) << 8;
        dmf = (p0 >> 8) == (pos >> 8) ? (mf - ((p0 & 255) << 8)) / (int32_t)(n ? n : 1u) : 0;   /* (a wave crossed: no ramp) */
        mf -= dmf * (int32_t)n;                              /* (from the last block's) */
        pd_setup(&bm, (w1 + 1u) & 7u, dcw);
    }
    pd_setup(&b1, w1, dcw);
    pd_setup(&b2, w2 ? w2 - 1u : w1, dcw);                   /* WAVE2 (every other cycle) */
    for (i = 0; i < n; i++) {
        uint32_t old = ph0;
        int32_t s;
        {
            uint32_t pp = ((ph0 >> 16) + (uint32_t)((((f1 + f2) >> 1) * (fb >> 8)) >> 8)) & 0xFFFFu;
            fb += dfb;
            s = pd_wave(tg0 ? &b2 : &b1, pp);
            if (mf && !tg0)
                s += ((pd_wave(&bm, pp) - s) * (mf >> 8)) >> 8;
            mf += dmf;
        }
        f2 = f1;
        f1 = s;
        ph0 += inc;
        if (ph0 < old)
            tg0 ^= 1;
        if (line2) {
            uint32_t old2 = ph1;
            int32_t s2 = pd_wave(tg1 ? &b2 : &b1, ph1 >> 16);
            ph1 += inc2;
            if (ph1 < old2)
                tg1 ^= 1;
            s = ring ? mulq15(s, s2) : (s + s2) >> 1;     /* both within +-32767 */
        }
        if (sub) {
            ph2 += inc >> 1;
            s += mulq15(osc_sine(ph2), sub);
        }
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = tg0;
    v->s[1] = tg1;
    v->s[2] = f1;
    v->s[3] = f2;
    v->s[4] = p[P_E7] << 8;
}

static const preset_t PHASE_PRESETS[] = {
    /* name, {WAVE, WAVE2, DCW, ENV, DTN, LINE, SUB, -}, {A D S R}, fenv, mono */
    {"BRASS", {0, 0, 30, 90, 0, 0, 0, 0}, {8, 70, 90, 40}, 0, 0, FX(0, 20, 20, 40), PAT(6)},
    {"ORGAN", {3, 0, 40, 0, 0, 0, 0, 0}, {0, 127, 127, 30}, 0, 0, FX(0, 40, 0, 30), PAT(6)},
    {"STRING", {0, 4, 50, 40, 12, 0, 0, 0}, {40, 90, 100, 70}, 0, 0, FX(0, 50, 20, 60), PAT(5)},
    {"RESO", {5, 0, 60, 60, 0, 0, 0, 0}, {0, 70, 30, 60}, 0, 0, FX(0, 0, 40, 40), PAT(1)},
    {"BELL", {6, 0, 80, 50, 0, 0, 0, 0}, {0, 95, 0, 90}, 0, 0, FX(0, 0, 30, 70), PAT(7)},
    {"WIRE", {4, 7, 70, 40, 7, 0, 0, 0}, {10, 80, 80, 60}, 0, 0, FX(15, 30, 30, 40), PAT(4)},
    /* (JIANT 0.4) FB: the phase fed back */
    {"GROWL", {0, 0, 70, 80, 0, 0, 40, 55}, {2, 70, 90, 40}, 0, 1, FX(20, 10, 30, 30), PAT(4)},
    {"FB BASS", {1, 0, 45, 60, 0, 0, 70, 35}, {0, 60, 60, 25}, 0, 1, FX(10, 0, 10, 10), PAT(2)},
    /* (GAMMAWAVE / WRAITH) phase-distortion plasma patches */
    {"PLASMA CORE", {5, 4, 82, 90, 14, 1, 65, 78}, {0, 68, 85, 36}, 32, 1, FX(42, 20, 25, 25), PAT(2)},
    {"WARP REESE", {0, 5, 76, 65, 22, 0, 85, 64}, {6, 82, 105, 48}, 24, 1, FX(38, 45, 15, 30), PAT(8)},
    {"CRYPT BELL", {6, 7, 88, 70, 9, 1, 20, 48}, {0, 96, 0, 92}, 0, 0, FX(12, 30, 45, 80), PAT(7)},
    {"WITCHFIRE", {4, 6, 74, 55, 18, 0, 40, 70}, {45, 90, 110, 88}, 18, 0, FX(28, 55, 40, 75), PAT(5)},
    /* (1.2-GW, issue #17: ambient and expressive) a crystal pad (its DCW swept by a slow LFO), strings in a haze */
    {"CRYSTAL PAD", {3, 0, 50, 70, 20, 0, 20, 0}, {85, 90, 110, 100}, 0, 0, FX(0, 70, 30, 85), PAT(5),
     {XP(P_LRATE, 12), XP(P_LWAVE, 1), XP(P_LFADE, 60), XP(P_LD_SHP, 18)}},
    {"HAZE STRINGS", {0, 7, 40, 60, 30, 0, 10, 0}, {70, 90, 115, 90}, 0, 0, FX(0, 90, 20, 75), PAT(5),
     {XP(P_LRATE, 48), XP(P_LD_PIT, 2), XP(P_M1SRC, 5), XP(P_M1DST, 3), XP(P_M1AMT, 10)}},
    /* (QUANTUM // QG-1) keys, a phase 808, a sweep that opens by itself */
    {"QBIT KEYS", {3, 6, 60, 50, 8, 0, 10, 0}, {0, 85, 40, 60}, 0, 0, FX(0, 30, 35, 45), PAT(6)},
    {"PD EPIANO", {6, 3, 55, 60, 6, 0, 0, 0}, {0, 88, 30, 65}, 0, 0, FX(0, 40, 30, 50), PAT(6)},
    {"PD 808", {3, 0, 20, 30, 0, 0, 90, 20}, {0, 80, 60, 40}, 0, 1, FX(30, 0, 0, 5), PAT(2),
     {XP(P_VOICE, V_LEGATO), XP(P_GLIDE, 40)}},
    {"ION SWEEP", {5, 4, 70, 80, 16, 0, 30, 40}, {60, 90, 110, 90}, 10, 0, FX(10, 60, 40, 80), PAT(5),
     {XP(P_LRATE, 10), XP(P_LWAVE, 1), XP(P_LD_SHP, 20)}},
};

static const engine_t ENG_PHASE = {
    .name = "PHASE",
    .page_title = {"PHS", "LINE"},
    .edit = {
        {"WAVE", F_ENUM, 0, 7, 0, N_PD_WAVE, 0},
        {"WAVE2", F_ENUM, 0, 8, 0, N_PD_WAVE2, 0},
        {"DCW", F_PCT, 0, 127, 60, 0, 0},
        {"ENV", F_PCT, 0, 127, 64, 0, 0},
        {"DETUNE", F_INT, 0, 127, 0, 0, "ct"},
        {"LINE", F_ENUM, 0, 1, 0, N_PD_LINE, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"FB", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = PHASE_PRESETS,
    .npresets = NELEM(PHASE_PRESETS),
    .note_on = phase_note_on,
    .render = phase_render,
    .knob = {P_E2, P_E3, P_E4, P_REL},
};
