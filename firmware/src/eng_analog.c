/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ANALOG: two band-limited oscillators (saw / square / tri / sine / PWM),
 * noise, drive and a trapezoidal low-pass. (JIANT 0.4, TRIO folded in: TRIO's sounds play here, project.c) three more
 * WAVEs in a loop of their own (analog_ext): SYNC (osc 2's saw restarted by osc 1, DTN its ratio 1 .. 8.9: the sync
 * lead's sweep), RING (osc 1's saw times osc 2's sine at that ratio: bells, metal), SAW3 (three saws, osc 2 DTN cents up,
 * osc 3 down: TRIO's fat stack). MIX: osc 1 against the other(s) as before. INT (0.4, once KTR): osc 2 INT semitones
 * from osc 1, as a classic two-oscillator synth (fifths, octaves; SAW3: its osc 2). */
static const char *const N_ANALOG_WAVE[] = {"SAW", "SQR", "TRI", "SIN", "PWM", "SYNC", "RING", "SAW3", "TUBE", "FOLD"};
enum { AW_SYNC = 5, AW_RING, AW_SAW3, AW_TUBE, AW_FOLD };
/* (JIANT 0.5) FILTER TYPE (P_FTYPE): LP as before (analog_render's loop); HP BP from the same state-variable filter
 * (analog_ext's loop, every WAVE); COMB the filter open, the part's sum through a feedback comb tuned to the newest
 * note and CUT (+-32 semitones about it), RES its feedback (voice.c track_comb) */
enum { FT_LP, FT_HP, FT_BP, FT_COMB, FT_VOWEL, FT_SCREAM };
static inline int32_t svf_mode(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2, uint32_t ft, int32_t k)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    int32_t s1 = 2 * v1 - *ic1;
    if (ft == FT_SCREAM)
        s1 = softclip(s1 >> 1) << 1;
    *ic1 = clamp(s1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    if (ft == FT_VOWEL) {
        int32_t bp = (k * v1) >> 12;
        return (v2 >> 1) + bp - (softclip(in - bp) >> 2);
    }
    if (ft == FT_SCREAM)
        return v2 + (softclip(v1) >> 1);
    return ft == FT_HP ? in - v2 - ((k * v1) >> 12) : ft == FT_BP ? (k * v1) >> 12 : ft == FT_COMB ? in : v2;
}

static void analog_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[1] = v->ph[0] + 0x40000000u;
    v->s[0] = v->s[1] = 0;                            /* filter */
    if (!v->s[2])
        v->s[2] = 0x1234567 + (int32_t)v->age;        /* noise state */
}

/* (0.6.45) MORPH: the plain WAVEs (SAW SQR TRI SIN) on towards the next ones: the place, Q8 from WAVE (0..1536), the
 * knob and the SHAPE modulation (LFO / ENV -> SHAPE) together; -1: none (the WAVE as it is). (0.6.81) Round in a
 * loop: SIN on towards SAW (it did nothing from SIN) */
static int32_t an_mpos(const int16_t *p, int32_t mq8, const vmod_t *m)   /* (mq8: the morph Q8, track_t.mo_cur) */
{
    int32_t mo;
    if (p[P_E0] >= 4 || (!mq8 && m->shape == (64 << 8)))
        return -1;
    mo = clamp(mq8 + m->shape - (64 << 8), 0, 127 << 8);
    return clamp(p[P_E0] * 256 + mo * 768 / (127 << 8), 0, 1536);
}
/* (0.6.81) MORPH on PWM SYNC RING SAW3 (and the SHAPE modulation on the last three, which had none): how far, Q8
 * 0..127 << 8, the knob and SHAPE together */
static int32_t an_mext(int32_t mq8, const vmod_t *m)
{
    return clamp(mq8 + m->shape - (64 << 8), 0, 127 << 8);
}
/* (1.0-GW) TUBE and FOLD: a sine made dirty in the oscillator, computed (no table), before the filter, so its
 * fundamental stays (a dirty 808 keeps its sub): TUBE after GammaWave's Hearth, an asymmetric tube (even harmonics,
 * warmth; its DC taken off), FOLD after WRAITH's Blast, the sine of a sine (odd harmonics, a growl, metal). How dirty:
 * MORPH and the SHAPE modulation together (an_mext: ENV > SHAPE makes it dirty on the hit, clean as it rings) */
typedef struct { int32_t g, b, cb, dc, kf; } an_dirt_t;
static void an_dirt_set(an_dirt_t *d, int32_t mx)        /* mx: Q8, 0 .. 127 << 8 */
{
    int32_t a = clamp(mx, 0, 127 << 8) >> 8, k, sum = 0;
    d->g = 4096 + a * 200;                              /* TUBE: Q12, the sine x1 .. x7.2 into the curve .. */
    d->b = a * 60;                                      /* .. biased (up to ~0.23): its asymmetry */
    d->cb = softclip(d->b);
    for (k = 0; k < 16; k++)                            /* (its DC over a cycle, taken off: no thump at the gate) */
        sum += softclip(((SINE[k * 64] * d->g) >> 12) + d->b) - d->cb;
    d->dc = sum >> 4;
    d->kf = 5215 + a * 287;                             /* FOLD: sin(g sin x), g 1 .. 8 rad (the phase scale / 4) */
}
static inline int32_t an_dirt(uint32_t w, uint32_t ph, const an_dirt_t *d)
{
    int32_t s = osc_sine(ph);
    if (w == AW_TUBE)
        return softclip(((s * d->g) >> 12) + d->b) - d->cb - d->dc;
    return (sine_i((uint32_t)(s * d->kf) << 2) * 3 + s) >> 2;   /* (a quarter of the sine under it, as Blast's) */
}
static inline int32_t an_osc(uint32_t w, uint32_t ph, uint32_t inc)
{
    w &= 3u;
    return w == 1u ? osc_pulse(ph, inc, 0x80000000u) : w == 2u ? osc_tri(ph) : w == 3u ? osc_sine(ph) : osc_saw(ph, inc);
}
static inline int32_t an_morph(int32_t pos, uint32_t ph, uint32_t inc)
{
    int32_t a = an_osc((uint32_t)pos >> 8, ph, inc), f = pos & 255;
    return f ? a + (((an_osc(((uint32_t)pos >> 8) + 1u, ph, inc) - a) * f) >> 8) : a;
}

/* (0.6.45) MORPH: the plain WAVEs blended toward the next (an_mpos), osc 2 as analog_render's, the same noise, drive
 * and filter as analog_ext (a loop of its own: the others' as they were) */
static __attribute__((noinline)) void analog_morph(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m,
                                                   int32_t mpos0, int32_t mpos1)   /* (0.6.50: ramped mpos0 -> mpos1) */
{
    const int16_t *p = t->p;
    uint32_t i, inc1 = m->inc, ph0 = v->ph[0], ph1 = v->ph[1], ft = (uint32_t)p[P_FTYPE] % 6u;
    uint32_t inc2 = p[P_E1] || p[P_E7] ? cents_inc(m->pitch16 + p[P_E7] * 16, p[P_E1], 0) : inc1;
    int32_t m2 = p[P_E2] * 258, m1 = 32767 - m2, nz = p[P_E3] * 200, drv = p[P_E6], kd = 8192 - p[P_E5] * 7600 / 127;
    int32_t drive = 32768 + drv * 512, cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) << 2);
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_E5]);
    int32_t dq = ((mpos1 - mpos0) << 8) / (int32_t)(n ? n : 1u);
    for (i = 0; i < n; i++) {
        int32_t mpos = mpos0 + ((dq * (int32_t)i) >> 8);
        int32_t s = mulq15(an_morph(mpos, ph0, inc1), m1) + mulq15(an_morph(mpos, ph1, inc2), m2);
        ph0 += inc1;
        ph1 += inc2;
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        out[i] += voice_amp(soft_knee(svf_mode(&flt, s >> 1, &ic1, &ic2, ft, kd), 16000) << 1, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
}

/* PWM's width: SHAPE as before, (0.6.81) MORPH narrowing it further (toward ~4 %) */
static uint32_t an_pw(const track_t *t, const vmod_t *m)
{
    int32_t q = (m->shape - (64 << 8)) + (t->p[P_E0] == 4 ? t->mo_cur * 2 : 0);
    return 0x80000000u + (uint32_t)(clamp(q, -16384, 65000) << 15);
}

/* SYNC RING SAW3 (see the top): a render of their own (the other WAVEs' loop untouched), the same noise, drive and
 * filter */
static __attribute__((noinline)) void analog_ext(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E0], i, inc1 = m->inc, ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], r, inc2, inc3;
    uint32_t ft = (uint32_t)p[P_FTYPE] % 6u, pw = an_pw(t, m);
    int32_t mx = wave >= AW_SYNC ? an_mext(t->mo_cur, m) : 0;   /* (0.6.81) MORPH, SHAPE on SYNC RING SAW3 */
    int32_t det = p[P_E1] + (wave == AW_SAW3 ? mx * 40 / (127 << 8) : 0), m2 = p[P_E2] * 258, m1 = 32767 - m2, nz = p[P_E3] * 200, drv = p[P_E6], kd = 8192 - p[P_E5] * 7600 / 127;
    int32_t drive = 32768 + drv * 512, cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) << 2);
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_E5]);
    r = 16u + (uint32_t)det + (wave != AW_SAW3 ? (uint32_t)(mx * 64 / (127 << 8)) : 0u);   /* SYNC / RING: osc 2 at r / 16
                                                         * of osc 1 (1 .. 8.9; 0.6.81: MORPH up to 4 more) */
    inc2 = wave == AW_SAW3 ? cents_inc(m->pitch16 + p[P_E7] * 16, det, 0) : (inc1 >> 4) * r;   /* (SAW3: osc 2 at INT) */
    if (wave != AW_SAW3 && p[P_E7]) {                   /* (JIANT 0.5) SYNC / RING: INT moves osc 2 too, semitones */
        inc2 = (cents_inc(m->pitch16 + p[P_E7] * 16, 0, 0) >> 4) * r;
        r = inc1 >> 8 ? (inc2 / (inc1 >> 8)) >> 4 : r; /* (its ratio to osc 1, x16: the restart below) */
        if (r < 1u) r = 1u;
    }
    inc3 = wave == AW_SAW3 ? cents_inc(m->pitch16, -det, 0) : 0u;
    if (wave < AW_SYNC)                                 /* (JIANT 0.5: HP BP COMB) the plain WAVEs: osc 2 as analog_render */
        inc2 = det || p[P_E7] ? cents_inc(m->pitch16 + p[P_E7] * 16, det, 0) : inc1;
    for (i = 0; i < n; i++) {
        int32_t a = osc_saw(ph0, inc1), b, s;
        if (wave < AW_SYNC) {
            switch (wave) {
            case 1: a = osc_pulse(ph0, inc1, 0x80000000u); b = osc_pulse(ph1, inc2, 0x80000000u); break;
            case 2: a = osc_tri(ph0); b = osc_tri(ph1); break;
            case 3: a = osc_sine(ph0); b = osc_sine(ph1); break;
            case 4: a = osc_pulse(ph0, inc1, pw); b = osc_pulse(ph1, inc2, pw); break;
            default: b = osc_saw(ph1, inc2); break;
            }
        } else if (wave == AW_SAW3) {
            b = (osc_saw(ph1, inc2) + osc_saw(ph2, inc3)) >> 1;
            ph2 += inc3;
        } else if (wave == AW_SYNC) {
            b = osc_saw(ph1, inc2);
        } else {
            b = ((a >> 1) * (osc_sine(ph1) >> 1)) >> 13;   /* RING (32 bits) */
        }
        ph1 += inc2;
        if (wave == AW_SYNC && ph0 + inc1 < ph0)        /* osc 1 starts a cycle: osc 2 with it */
            ph1 = ((ph0 + inc1) >> 4) * r;              /* (the new cycle's fraction, at osc 2's rate) */
        ph0 += inc1;
        s = mulq15(a, m1) + mulq15(b, m2);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        out[i] += voice_amp(soft_knee(svf_mode(&flt, s >> 1, &ic1, &ic2, ft, kd), 16000) << 1, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
}

static __attribute__((noinline)) void analog_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E0], i;
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) << 2);
    tsvf_t flt;
    int32_t drive = 32768 + p[P_E6] * 512;                       /* 1x .. 3x */
    uint32_t inc1 = m->inc;
    uint32_t inc2 = det || p[P_E7] ? cents_inc(m->pitch16 + p[P_E7] * 16, det, 0) : inc1;   /* (JIANT 0.4) osc 2: INT
                                                         * semitones away, DTN cents */
    uint32_t pw = an_pw(t, m);
    int32_t m2 = mix * 258, m1 = 32767 - m2;                    /* osc mix Q15 */
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];                  /* state in locals: out[] may alias v->s[] */
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    tsvf_coef(&flt, cut, p[P_E5]);
    for (i = 0; i < n; i++) {
        int32_t a, b, s;
        switch (wave) {
        case 1:
            a = osc_pulse(ph0, inc1, 0x80000000u);
            b = osc_pulse(ph1, inc2, 0x80000000u);
            break;
        case 2:
            a = osc_tri(ph0);
            b = osc_tri(ph1);
            break;
        case 3:
            a = osc_sine(ph0);
            b = osc_sine(ph1);
            break;
        case 4:
            a = osc_pulse(ph0, inc1, pw);
            b = osc_pulse(ph1, inc2, pw);
            break;
        default:
            a = osc_saw(ph0, inc1);
            b = osc_saw(ph1, inc2);
            break;
        }
        ph0 += inc1;
        ph1 += inc2;
        s = mulq15(a, m1) + mulq15(b, m2);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);   /* pre-shifts: drive is up to 3x, no overflow */
        {   /* filter, linear up to half scale, then a soft knee (only resonance peaks saturate); soft_knee()
             * written out: the host compiler makes this loop 1 % slower with the call */
            int32_t y = tsvf_lp(&flt, s >> 1, &ic1, &ic2), a = y < 0 ? -y : y;
            if (a > 16000) {
                a = 16000 + (softclip((a - 16000) * 2) >> 1);
                y = y < 0 ? -a : a;
            }
            s = y << 1;
        }
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
}

/* (1.0-GW) TUBE FOLD: a render of their own (analog_render's and analog_ext's loops untouched: tests/target_budget.txt),
 * osc 2 the same wave at INT / DETUNE, the same noise, drive and filter (every FILTER TYPE); how dirty: once a block */
static __attribute__((noinline)) void analog_dirty(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E0], i, inc1 = m->inc, ph0 = v->ph[0], ph1 = v->ph[1], ft = (uint32_t)p[P_FTYPE] % 6u;
    uint32_t inc2 = p[P_E1] || p[P_E7] ? cents_inc(m->pitch16 + p[P_E7] * 16, p[P_E1], 0) : inc1;
    int32_t m2 = p[P_E2] * 258, m1 = 32767 - m2, nz = p[P_E3] * 200, drv = p[P_E6], kd = 8192 - p[P_E5] * 7600 / 127;
    int32_t drive = 32768 + drv * 512, cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) << 2);
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    tsvf_t flt;
    an_dirt_t dt;
    tsvf_coef(&flt, cut, p[P_E5]);
    an_dirt_set(&dt, an_mext(t->mo_cur, m));
    for (i = 0; i < n; i++) {
        int32_t s = mulq15(an_dirt(wave, ph0, &dt), m1) + mulq15(an_dirt(wave, ph1, &dt), m2);
        ph0 += inc1;
        ph1 += inc2;
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        out[i] += voice_amp(soft_knee(svf_mode(&flt, s >> 1, &ic1, &ic2, ft, kd), 16000) << 1, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
}

/* the engine's render: SYNC RING SAW3 (0.4) and the FILTER TYPEs HP BP COMB (0.5) in analog_ext, (1.0-GW) TUBE FOLD in
 * analog_dirty, the rest (LP) here (a dispatch of its own: analog_render's loop as before) */
static void analog_dispatch(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    int32_t m1 = an_mpos(t->p, t->mo_cur, m), m0 = an_mpos(t->p, t->mo_prev, m);
    if (m1 >= 0 || m0 >= 0) {                         /* (0.6.45) MORPH; (0.6.50) ramped from the last block's */
        int32_t base = (int32_t)t->p[P_E0] * 256;      /* (as it leaves or comes: the plain WAVE) */
        analog_morph(t, v, out, n, m, m0 < 0 ? base : m0, m1 < 0 ? base : m1);
    }
    else if ((uint32_t)t->p[P_E0] >= AW_TUBE)
        analog_dirty(t, v, out, n, m);
    else if ((uint32_t)t->p[P_E0] >= AW_SYNC || t->p[P_FTYPE])
        analog_ext(t, v, out, n, m);
    else
        analog_render(t, v, out, n, m);
}

static const preset_t ANALOG_PRESETS[] = {
    {"SAW LEAD", {0, 12, 64, 0, 90, 30, 10, 12}, {4, 70, 100, 50}, 20, 1, FX(0, 10, 45, 30), PAT(4)},
    {"SOFT PAD", {0, 20, 64, 4, 60, 10, 0, 0}, {80, 90, 110, 95}, 10, 0, FX(0, 60, 20, 70), PAT(5)},
    {"SQR BASS", {1, 0, 0, 0, 50, 70, 40, 0}, {0, 60, 40, 30}, 30, 1, FX(5, 0, 10, 10), PAT(2)},
    {"PWM STR", {4, 8, 40, 0, 75, 20, 0, 0}, {60, 80, 110, 85}, 8, 0, FX(0, 50, 20, 60), PAT(5)},
    {"ACID", {0, 0, 0, 0, 50, 100, 25, 0}, {0, 55, 20, 30}, 48, 1, FX(20, 0, 45, 15), PAT(1)},
    {"SINE KEY", {3, 6, 50, 0, 127, 0, 0, 0}, {2, 80, 30, 70}, 0, 0, FX(0, 30, 25, 40), PAT(6)},
    {"RAVE", {4, 30, 64, 0, 85, 20, 30, 7}, {20, 80, 110, 60}, 10, 1, FX(30, 40, 30, 30), PAT(13)},
    {"SUB BASS", {3, 0, 0, 0, 40, 0, 20, 0}, {0, 60, 100, 20}, 0, 1, FX(0, 0, 0, 10), PAT(8)},
    {"PLUCK", {0, 8, 50, 0, 30, 40, 0, 0}, {0, 88, 0, 60}, 55, 0, FX(0, 20, 50, 30), PAT(3)},
    {"BRASS", {0, 10, 64, 0, 45, 20, 10, 0}, {35, 70, 90, 45}, 40, 0, FX(0, 20, 20, 40), PAT(6)},
    {"WIND", {0, 0, 0, 90, 30, 90, 0, 0}, {60, 90, 60, 80}, 50, 0, FX(0, 30, 30, 70), PAT(5)},
    {"STRINGS", {0, 25, 64, 0, 70, 10, 0, 0}, {70, 90, 115, 90}, 5, 0, FX(0, 60, 20, 70), PAT(5)},
    /* (JIANT 0.4: TRIO's, folded in) SYNC (ratio 1 + 11/16), RING (1 + 18/16), SAW3 (9 ct a side) */
    {"SYNC LEAD", {5, 11, 90, 0, 82, 30, 10, 0}, {2, 70, 100, 40}, 18, 1, FX(0, 10, 40, 25), PAT(4)},
    {"RING BELL", {6, 18, 100, 0, 110, 20, 0, 0}, {0, 92, 0, 80}, 0, 0, FX(0, 20, 35, 55), PAT(7)},
    {"FAT BASS", {7, 9, 64, 0, 62, 45, 20, -12}, {0, 62, 60, 25}, 40, 1, FX(10, 0, 10, 10), PAT(2)},
    /* (GAMMAWAVE / WRAITH) industrial & darksynth signature patches */
    {"INGOT SUB", {3, 4, 48, 8, 44, 38, 65, -12}, {0, 64, 95, 28}, 26, 1, FX(32, 0, 0, 8), PAT(2)},
    {"WORM REESE", {7, 18, 64, 12, 56, 62, 75, -12}, {8, 85, 110, 48}, 34, 1, FX(45, 35, 15, 25), PAT(8)},
    {"GRAVE GROWL", {5, 22, 96, 16, 48, 84, 90, -12}, {2, 68, 80, 35}, 52, 1, FX(55, 15, 25, 20), PAT(1)},
    {"HEARTH LD", {0, 14, 64, 10, 86, 45, 60, 7}, {4, 76, 100, 52}, 28, 1, FX(40, 20, 45, 35), PAT(4)},
    {"WRAITH PAD", {4, 24, 64, 18, 64, 42, 25, 12}, {75, 95, 112, 98}, 16, 0, FX(18, 65, 35, 82), PAT(5)},
    {"SCREAM 303", {0, 0, 0, 4, 42, 114, 80, 0}, {0, 52, 15, 26}, 58, 1, FX(48, 0, 40, 18), PAT(1)},
    /* (POCKET GAMMAWAVE // GW-1) Latest GammaWave Wave 1/2 + Hearth Physics presets */
    {"SOLAR BASS", {7, 11, 64, 6, 54, 68, 72, -12}, {0, 62, 85, 32}, 44, 1, FX(38, 15, 20, 18), PAT(14)},
    {"PDRESO ACID", {5, 16, 82, 4, 46, 108, 64, 0}, {0, 56, 25, 28}, 54, 1, FX(42, 10, 38, 22), PAT(14)},
    {"BITSINE ARP", {6, 14, 70, 8, 88, 36, 35, 12}, {0, 74, 40, 52}, 32, 0, FX(22, 35, 50, 45), PAT(15)},
    {"SINGULARITY", {5, 24, 96, 18, 52, 88, 95, -12}, {4, 78, 92, 46}, 48, 1, FX(60, 25, 35, 30), PAT(16)},
    {"PHOTOSPHERE", {4, 22, 64, 14, 78, 40, 28, 7}, {48, 92, 110, 94}, 22, 0, FX(15, 60, 45, 85), PAT(17)},
    {"ANALOG DRFT", {0, 19, 64, 6, 72, 32, 18, 0}, {18, 84, 96, 76}, 24, 0, FX(12, 48, 40, 68), PAT(15)},
    /* (1.2-GW, issue #17: ambient and expressive) a pad that drifts by itself (a slow LFO on the filter, each note its own
     * mix), a string machine (three saws, the ensemble chorus), a drone on a fifth that breathes */
    {"EVOLVE PAD", {AW_SAW3, 46, 64, 4, 58, 22, 0, 0}, {92, 90, 112, 100}, 8, 0, FX(0, 64, 22, 82), PAT(5),
     {XP(P_LRATE, 14), XP(P_LWAVE, 1), XP(P_LFADE, 70), XP(P_LD_FLT, 16), XP(P_M1SRC, 5), XP(P_M1DST, 14)}},
    {"SOLINA", {AW_SAW3, 30, 70, 0, 88, 8, 0, 0}, {50, 90, 118, 82}, 0, 0, FX(0, 115, 10, 70), PAT(5),
     {XP(P_LRATE, 52), XP(P_LD_PIT, 2)}},
    {"DEEP DRONE", {0, 20, 50, 10, 42, 70, 20, 7}, {100, 90, 127, 110}, 0, 0, FX(10, 40, 30, 95), PAT(5),
     {XP(P_LRATE, 8), XP(P_LWAVE, 1), XP(P_LD_FLT, 20), XP(P_LD_AMP, 18)}},
    /* (QUANTUM // QG-1) 808 and drill slides (LEGATO glide), reese, the dirty waves, the filter types as sounds of their own, and
     * pads that move by themselves (the NOISE wander on the cutoff) */
    {"808 BOOM", {3, 0, 0, 0, 60, 0, 40, 0}, {0, 80, 60, 40}, 0, 1, FX(25, 0, 0, 5), PAT(2),
     {XP(P_VOICE, V_LEGATO), XP(P_GLIDE, 36)}},
    {"DRILL SLIDE", {3, 4, 30, 0, 64, 10, 55, -12}, {0, 90, 80, 45}, 0, 1, FX(35, 0, 0, 8), PAT(14),
     {XP(P_VOICE, V_LEGATO), XP(P_GLIDE, 58)}},
    {"REESE QUAKE", {AW_SAW3, 24, 64, 4, 50, 50, 70, -12}, {4, 80, 110, 40}, 30, 1, FX(40, 30, 10, 15), PAT(8),
     {XP(P_LRATE, 20), XP(P_LD_FLT, 14)}},
    {"TUBE KEYS", {AW_TUBE, 8, 50, 0, 80, 20, 30, 12}, {0, 82, 40, 60}, 20, 0, FX(10, 30, 30, 45), PAT(6)},
    {"FOLD LEAD", {AW_FOLD, 10, 64, 0, 92, 28, 40, 7}, {2, 72, 100, 45}, 22, 1, FX(20, 10, 45, 30), PAT(4),
     {XP(P_LRATE, 60), XP(P_LFADE, 50), XP(P_LD_PIT, 3)}},
    {"HYPER SAW", {AW_SAW3, 70, 64, 0, 96, 20, 15, 12}, {6, 80, 110, 55}, 10, 0, FX(0, 70, 40, 60), PAT(13)},
    {"COMB PLUCK", {0, 6, 50, 10, 70, 80, 0, 0}, {0, 70, 0, 55}, 40, 0, FX(0, 10, 40, 40), PAT(15),
     {XP(P_FTYPE, 3)}},
    {"VOWEL BASS", {0, 12, 64, 0, 60, 70, 50, -12}, {0, 70, 90, 30}, 36, 1, FX(30, 0, 10, 10), PAT(8),
     {XP(P_FTYPE, 4), XP(P_LRATE, 40), XP(P_LD_FLT, 24)}},
    {"SCREAM LEAD", {AW_SYNC, 14, 90, 4, 76, 90, 70, 0}, {2, 70, 95, 40}, 40, 1, FX(45, 10, 40, 25), PAT(4),
     {XP(P_FTYPE, 5)}},
    {"QUANTUM PAD", {AW_SAW3, 36, 64, 6, 62, 30, 10, 7}, {85, 92, 115, 100}, 12, 0, FX(0, 80, 35, 90), PAT(17),
     {XP(P_LRATE, 10), XP(P_LWAVE, 1), XP(P_LD_FLT, 18), XP(P_M1SRC, 14), XP(P_M1DST, 2), XP(P_M1AMT, 10)}},
    {"TRAP PLUCK", {0, 10, 50, 0, 40, 30, 10, 12}, {0, 76, 0, 50}, 50, 0, FX(0, 20, 55, 40), PAT(15)},
    {"CHERENKOV LD", {AW_RING, 20, 80, 0, 90, 30, 20, 0}, {2, 74, 90, 50}, 16, 1, FX(10, 20, 50, 40), PAT(4),
     {XP(P_LRATE, 58), XP(P_LFADE, 60), XP(P_LD_PIT, 2)}},
};

static const engine_t ENG_ANALOG = {
    .name = "ANALOG",
    .page_title = {"OSC", "FLT"},
    .edit = {
        {"WAVE", F_ENUM, 0, 9, 0, N_ANALOG_WAVE, 0},  /* (1.0-GW: TUBE FOLD appended) */
        {"DETUNE", F_INT, 0, 127, 10, 0, "ct"},
        {"MIX", F_PCT, 0, 127, 64, 0, 0},
        {"NOISE", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 30, 0, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
        {"INT", F_SEMI, -24, 24, 0, 0, 0},             /* (JIANT 0.4: osc 2's interval, was KTR: the filter's key
                                                         * tracking, fixed at its old default, half) */
    },
    .presets = ANALOG_PRESETS,
    .npresets = NELEM(ANALOG_PRESETS),
    .note_on = analog_note_on,
    .render = analog_dispatch,
    .knob = {P_E4, P_E5, P_ATK, P_REL},
    .keep = 0x03,                /* the filter */
};
