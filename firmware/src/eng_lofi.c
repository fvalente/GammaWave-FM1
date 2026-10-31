/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chip voice: pulse/triangle/saw/noise/wave RAM, quantised amplitude and a held
 * (downsampled) output; SWEEP bends the pitch down after note-on.
 *
 * WAVE = WRAM plays a 32-sample, 4-bit wave table, read step-wise (no interpolation);
 * DUTY then picks one of 16 built-in tables (DUTY / 8, shown as "WAV#" with its name).
 * CHIP = STEP: a 4-bit stepped volume envelope with a frame-clock divider replaces the
 * ADSR curve (lofi_amp), the triangle becomes a 32-step 4-bit staircase, the pulse
 * duties are 12.5 / 25 / 50 / 75 %; CRSH then sets the envelope (shown as "DCY"). */
static const char *const N_CHIP[] = {"4BIT", "4B/2", "8BIT", "1BIT", "STEP"};
static const char *const N_RWAVE[] = {"PLS", "TRI", "SAW", "NOIS", "WRAM", "BYTE", "FLOAT"};
static const char *const N_RARP[] = {"OFF", "OCT", "MAJ", "MIN"};
enum { CHIP_4BIT, CHIP_4B2, CHIP_8BIT, CHIP_1BIT, CHIP_STEP };
enum { RW_PLS, RW_TRI, RW_SAW, RW_NOIS, RW_WRAM, RW_BYTE, RW_FLOAT };

/* wave RAM: 16 tables x 32 samples x 4 bits, two samples a byte (high nibble first);
 * our own shapes, from simple formulas (squares, sine, triangle, saws, harmonic sums,
 * one formant peak for the vowels). DC is taken out per table in lofi_render. */
static const char *const N_WRAM[] = {"SQ50", "SQ25", "SQ12", "SINE", "TRI", "SAW", "SAW2", "BAS1",
                                     "BAS2", "ORGN", "HOLW", "VOXA", "VOXO", "VOXE", "BUZZ", "STAIR", 0};
static const uint8_t WRAM[16][16] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ50 */
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ25 */
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ12 */
    {0x89, 0xAC, 0xDE, 0xEF, 0xFF, 0xEE, 0xDC, 0xA9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56},   /* SINE */
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10},   /* TRI */
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},   /* SAW */
    {0x01, 0x12, 0x33, 0x45, 0x56, 0x77, 0x89, 0x9A, 0x56, 0x67, 0x88, 0x9A, 0xAB, 0xCC, 0xDE, 0xEF},   /* SAW2: + its octave */
    {0xEE, 0xEF, 0xFF, 0xFF, 0xFF, 0xEE, 0xED, 0xDC, 0x11, 0x10, 0x00, 0x00, 0x00, 0x11, 0x12, 0x23},   /* BAS1: sloped square */
    {0xCE, 0xFF, 0xED, 0xBA, 0x9A, 0xBD, 0xEF, 0xFE, 0xC9, 0x64, 0x21, 0x00, 0x00, 0x01, 0x24, 0x69},   /* BAS2: 1 + 2 + 3 */
    {0x8B, 0xEF, 0xED, 0xBA, 0x9A, 0xAA, 0x98, 0x77, 0x88, 0x87, 0x65, 0x55, 0x65, 0x42, 0x10, 0x14},   /* ORGN: 1..4 */
    {0x8D, 0xFE, 0xDD, 0xDD, 0xCD, 0xDD, 0xDE, 0xFD, 0x82, 0x01, 0x22, 0x22, 0x32, 0x22, 0x21, 0x02},   /* HOLW: odd */
    {0x8F, 0xFC, 0xA7, 0x67, 0x89, 0x88, 0x88, 0x87, 0x88, 0x77, 0x77, 0x76, 0x78, 0x98, 0x53, 0x00},   /* VOXA: "ah" */
    {0x8F, 0xEE, 0xFD, 0xDB, 0x98, 0x66, 0x66, 0x77, 0x88, 0x89, 0x99, 0x97, 0x64, 0x22, 0x01, 0x10},   /* VOXO: "oh" */
    {0x7F, 0x96, 0xCF, 0xBB, 0xCC, 0xBB, 0xAA, 0x98, 0x77, 0x65, 0x54, 0x43, 0x34, 0x40, 0x39, 0x60},   /* VOXE: "ee" */
    {0xF3, 0x00, 0x12, 0x22, 0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},   /* BUZZ: spike + ring */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00},   /* STAIR: 4 levels */
};
#define Q4 2184                  /* one 4-bit step (2 * 2184 * 7.5 = 32760) */

/* CHIP = STEP: CRSH as the envelope, CRSH / 4 = b: 0 constant volume, 1..16 a one-shot decay
 * (divider period 0..15), 17..31 a looping one (period 1..15) */
static const char *const N_DCY[] = {"CONST", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10",
                                    "D11", "D12", "D13", "D14", "D15", "L1", "L2", "L3", "L4", "L5", "L6", "L7",
                                    "L8", "L9", "L10", "L11", "L12", "L13", "L14", "L15", 0};
static const param_desc_t LOFI_WAVNUM = {"WAV#", F_INT, 0, 127, 64, N_WRAM, 0};   /* = edit[2] but the names */
static const param_desc_t LOFI_DCY = {"DECAY", F_INT, 0, 127, 0, N_DCY, 0};        /* = edit[3] but the names */

/* (JIANT 0.4) WAVE = BYTE: bytebeat, an 8-bit formula of the time t played as the wave (after viznut's 2011
 * one-liners: the music is in the formula). DUTY picks one of 32 (ALGO, DUTY / 4), CRSH is the formulas' variable
 * a (VAR: 1 .. 16); t runs with the note played (C4: ~8 kHz, the classic rate; an octave up twice as fast), restarts
 * at each note-on; the hold (4B/2), the bits (CHIP), TONE and the rest as for the other waves. Our own picks and
 * variations; each a few operations a sample */
static const char *const N_BYTE[] = {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10", "B11",
                                     "B12", "B13", "B14", "B15", "B16", "B17", "B18", "B19", "B20", "B21", "B22",
                                     "B23", "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31", "B32", 0};
static const param_desc_t LOFI_ALGO = {"ALGO", F_INT, 0, 127, 64, N_BYTE, 0};    /* = edit[2] (BYTE) */
static const param_desc_t LOFI_VAR = {"VAR", F_PCT, 0, 127, 0, 0, 0};            /* = edit[3] (BYTE) */
/* (JIANT 0.5) BYTE's own: RES the filter's resonance (VIB's slot), LOOP (MASK's slot) t held in a window of 2^17 ..
 * 2^5 steps: the formula's long evolution (0: free) folded into a short cycle repeating at the note: a pitched tone */
static const param_desc_t LOFI_LOOP = {"LOOP", F_PCT, 0, 127, 0, 0, 0};           /* = edit[7] (BYTE) */
/* (JIANT 0.5) E4 (CUT) left: the filter is the FILTER page's (TYPE, CUT, RES: P_FTYPE P_FCUT P_FRES), on every engine */
static const param_desc_t LOFI_NONE = {"-", F_INT, 0, 0, 0, 0, 0};
/* (JIANT 0.5.1) each formula's start, in 512 steps of t: where it is already moving (some sit silent for up to
 * ~2 s from t = 0); a note starts there, LOOP's window too. The first 17 were rewritten in 0.5.1, away from the
 * well-worn one-liners (t * (42 & t >> 10), t & t >> 8 ..): rhythms and arpeggios after Tejeez, xpansive's
 * "lost in space", a melody of t >> 9 modulo 13, textures of t % 24 and t % 19 */
static const uint8_t BYTE_T0[32] = {0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0, 16, 0, 7, 16, 0, 1, 8, 15, 8, 0, 15, 23, 8, 0};
static __attribute__((noinline)) uint32_t lofi_byte(uint32_t f, uint32_t t, uint32_t a)
{
    switch (f & 31u) {
    case 0: return (~t >> 2) * ((127u & t * (7u & t >> 10)) < (245u & t * (2u + (5u & t >> 14))));   /* (Tejeez) */
    case 1: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (5u + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (Tejeez) */
    case 2: return ((-t & 4095u) * (255u & t * (t & t >> 13)) >> 12) + (127u & t * (234u & t >> 8 & t >> 3) >> (3u & t >> 14));   /* (xpansive) */
    case 3: return t * (((t >> 9) ^ ((t >> 9) - 1u) ^ 1u) % 13u);
    case 4: return ((t >> 13 | t % 24u) & (t >> 7 | t % 19u)) << 3;
    case 5: return t * (t ^ (t + (t >> 15 | 1u)) ^ (((t - 1280u) ^ t) >> 10));
    case 6: return (t * (0xCA98u >> (t >> 9 & 14u) & 15u)) | t >> 8;
    case 7: return (t * (t >> 8 | t >> 9) & 46u & t >> 8) ^ ((t & t >> 13) | t >> 6);
    case 8: return (t >> 4) * (13u & 0x8898A989u >> (t >> 11 & 30u));
    case 9: return t * (t >> ((8u * ((t >> 15) | (t >> 8))) & 31u) & (20u | (((t >> 19) * 5u) >> (t & 31u)) | t >> 3));
    case 10: return (t & t % 255u) - (t * 3u & t >> 13 & t >> 6);
    case 11: return (t * (t >> 9 | t >> 13) & (t >> 6 | t >> 11) & 127u) + ((t >> 4) * (3u + (t >> 12 & 3u)) & 64u);
    case 12: return t + (t & (t ^ t >> 6)) - t * ((t >> 9) & (t % 16u ? 2u : 6u) & t >> 9);
    case 13: return t * (t >> 11 & t >> 8 & 123u & t >> 3);
    case 14: return (t * 9u & t >> 4) | (t * 5u & t >> 7) | (t * 3u & t >> 10);
    case 15: return (t * ((t & 4096u ? (t % 65536u < 59392u ? 7u : t & 7u) : 16u) + (1u & t >> 14)) >> (3u & -t >> (t & 2048u ? 2 : 10))) | t >> (t & 16384u ? (t & 4096u ? 10 : 3) : 2);   /* (Tejeez) */
    case 16: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (a + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (a: VAR) */
    case 17: return t * ((t >> (a & 15u) | t >> 8) & 63u & t >> 4);
    case 18: return (t * a & t >> 7) | (t * 3u & t >> 10);
    case 19: return (t >> a | t) * (t >> 9 & 7u);
    case 20: return t * (t >> 8 & a) ^ t >> 4;
    case 21: return (t * a & t >> 8) * (t >> 12 & 3u);
    case 22: return t * (0x9AD5u >> (t >> (8u + (a & 3u)) & 14u) & 15u);
    case 23: return (t ^ t >> a) * (t >> 10 & 5u);
    case 24: return t >> 2 & (t * a >> 6 | t >> 9);
    case 25: return (t | t >> a | t >> 7) * (t >> 13 & 7u);
    case 26: return t * ((t >> 11 & 3u) + a) & t >> 6;
    case 27: return (t * (a | 1u) % (257u - (t >> 10 & 127u))) & 255u;
    case 28: return t * (t >> 10 & 7u & a) | t >> 5;
    case 29: return (t >> 5 & t >> 7) * a + (t >> 9 & 32u);
    case 30: return t * (t >> 12 & a ? 3u : 2u) & t >> 6;
    default: return (t * a >> 4 | t >> 3) ^ (t >> 7 & t >> 11);
    }
}
/* (JIANT 0.5.1; 0.5.6 a floatbeat generator) WAVE = FLOAT: floatbeat, bytebeat's sibling in floats. Each ALGO is a
 * formula of t, as BYTE's, but made of sines: sin(t x (t >> 11 ^ t >> 13 & 7)), FM whose index walks with t's bits,
 * melodies read from nibble tables, kicks and noise hats inside the formula, folds and powers of sines .. rhythm,
 * melody and timbre at once. t runs 32 steps a cycle of the note (the formula in tune with the key, its rhythm faster
 * up the keyboard, as bytebeat's), with 8 bits of fraction for the sines (x = t in Q8; S(x) = sin(2 pi x / 32)).
 * The same t as BYTE's: MOTN runs its slow bits at their own speed, VAR mutates it (lofi_mut), BEND folds it, LOOP
 * holds it in a window; FBK (E5, 0.6.4) feeds the output back into t, from a shimmer to chaos; CHIP the bits, RES the
 * filter's */
static const char *const N_FLOAT[] = {"F01", "F02", "F03", "F04", "F05", "F06", "F07", "F08", "F09", "F10", "F11",
                                      "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22",
                                      "F23", "F24", "F25", "F26", "F27", "F28", "F29", "F30", "F31", "F32", 0};
static const param_desc_t LOFI_FALGO = {"ALGO", F_INT, 0, 127, 64, N_FLOAT, 0};   /* = edit[2] (FLOAT) */
/* .. its CHIP the resolution: 4 bits, 4 bits held over two samples, FULL (smooth, the default), 1 bit, STEP (4 bits and
 * the stepped envelope) */
static const char *const N_FCHIP[] = {"4BIT", "4B/2", "FULL", "1BIT"};
static const param_desc_t LOFI_FCHIP = {"CHIP", F_ENUM, 0, 3, 2, N_FCHIP, 0};    /* = edit[0] (FLOAT; 0.5.5: no STEP) */
/* (0.5.4) BYTE's and FLOAT's own second page: MOTN (E4, where CUT was: the time's speed, 64 as written), GRIT (BYTE,
 * E5: the samples held longer, a rate crusher), FBK (FLOAT, E5: the output back into the formula's time) */
static const param_desc_t LOFI_MOTN = {"MOTION", F_INT, 0, 127, 64, 0, 0};          /* = edit[4] (BYTE, FLOAT) */
static const param_desc_t LOFI_GRIT = {"GRIT", F_PCT, 0, 127, 0, 0, 0};           /* = edit[5] (BYTE) */
static const param_desc_t LOFI_FDCY = {"FEEDBK", F_PCT, 0, 127, 0, 0, 0};            /* = edit[5] (FLOAT, 0.6.4: was DRV, a
                                                         * fold that silenced it and doubled the FILTER page's DIST) */
/* MOTN 0..127 -> the time's speed, Q8: x1/16 .. x1 (64) .. x15.5, exponential */
static uint32_t lofi_motion(int32_t m)
{
    uint32_t e = (uint32_t)clamp(m, 0, 127) + 64u;
    return ((16u + (e & 15u)) << (e >> 4)) >> 4;
}
/* (0.5.4) VAR as a mutation of the time (BYTE's t, FLOAT's step count): v >> 4 picks the way, v & 15 how much. 0..15
 * the time as written; then stairs that transpose as it goes, xor and or with itself shifted, a bit dropped (holes in
 * the rhythm), the low bits reversed (the phrase backwards), multiplied (faster, higher), scrambled */
static uint32_t lofi_mut(uint32_t t, uint32_t v)
{
    uint32_t k = v & 15u, m;
    switch ((v >> 4) & 7u) {
    case 0: return t;
    case 1: return t + (t >> (9u - k / 2u)) * (k + 1u);
    case 2: return t ^ (t >> (2u + k / 2u));
    case 3: return t | (t >> (1u + k / 2u));
    case 4: return t & ~(1u << (2u + k / 2u));
    case 5: m = (1u << (3u + k / 2u)) - 1u; return (t & ~m) | (~t & m);
    case 6: return t * (3u + k);
    default: return t ^ ((t >> 3) * ((k + 1u) * 0x9E3779B1u >> 24));
    }
}
#define S(x) sine_i((uint32_t)(x) << 19)                 /* sin of x (t in Q8): a turn every 32 t */
#define PM(y, k) ((uint32_t)(((int32_t)(y) * (int32_t)(k)) >> 6))   /* a sine (Q15) times k: k / 16 of a turn */
/* the floatbeat formula f at t (its integer) and x (t in Q8); aux: a formula's own state (a kick's phase) */
static __attribute__((noinline)) int32_t lofi_float(uint32_t f, uint32_t t, uint32_t x, int32_t *aux)
{
    static const uint8_t R[8] = {16, 18, 20, 21, 24, 27, 30, 32};   /* just ratios, Q4: 1 9/8 5/4 4/3 3/2 5/3 15/8 2 */
    uint32_t st = t >> 11, fr = t & 2047u, h;
    int32_t E = 32767 - (int32_t)(fr << 4), a, b;          /* (a step's envelope: 1 .. 0) */
    switch (f & 31u) {
    case 0: return S(x * (((t >> 11) ^ (t >> 13)) & 7u) + x) * E >> 15;                       /* sin(t (t>>11 ^ t>>13 & 7)) */
    case 1: return S(x + PM(S(x * 3u), (int32_t)((t >> 10) & 15u) * 3));                      /* FM, its index walking */
    case 2: return S((x >> 4) * R[(0x36364689u >> ((st & 7u) * 4u)) & 7u]) * E >> 15;         /* a melody, a nibble table */
    case 3: return S(x * (1u + ((t >> 12) & 3u))) * S(x * (((t >> 9) & 7u) + 1u)) >> 15;      /* a ring, rhythmic */
    case 4: {                                                                                  /* a kick, a bass */
        uint32_t k = fr < 1024u ? (1024u - fr) * (1024u - fr) >> 6 : 0u;
        *aux += (int32_t)k + 40;
        a = (st & 1u) ? 0 : sine_i((uint32_t)*aux << 13) * (int32_t)(1024u - (fr < 1024u ? fr : 1024u)) >> 10;
        return (a + (S(x >> 1) >> 1)) >> 1;
    }
    case 5: return S(x * ((((t >> 9) & (t >> 11)) & 7u) | 1u)) * E >> 15;                     /* Sierpinski partials */
    case 6: return softclip(S(x * (1u + (st & 1u))) * (1 + (int32_t)((t >> 11) & 7u) * 3) >> 1);   /* drive, rhythmic */
    case 7: return S((x >> 4) * R[(st * 3u + (st >> 2)) & 7u] * (1u + (st & 1u)));            /* fifths and octaves */
    case 8: {                                                                                  /* two arpeggios */
        static const uint8_t H[8] = {2, 3, 4, 5, 6, 8, 5, 3};
        return (S(x * H[(t >> 9) & 7u] >> 1) + S(x * H[(t >> 11) & 7u] >> 2)) >> 1;
    }
    case 9: h = (t * 2654435761u) >> 17;                                                       /* a snare, a tone */
        a = (st & 1u) ? (int32_t)h - 16384 : 0;
        return (mulq15(a, E) + S(x)) >> 1;
    case 10: return S(x + PM(S(x * (((t >> 12) & 7u) + 1u)), E >> 10));                       /* a pluck of FM */
    case 11: return S(x + PM(S(t << 2), 6));                                                  /* a vibrato, deep (4 Hz) */
    case 12: return S((uint32_t)(S(x) * (2 + (int32_t)((t >> 11) & 7u))) >> 2);               /* folded, rhythmic */
    case 13: a = S(x * (1u + (st & 1u))); b = S((x >> 4) * (24u + (st & 6u) * 4u));           /* rectified, leaping */
             return (a < 0 ? -a : a) - (b < 0 ? -b : b);
    case 14: return S(x & ~((1u << (4u + (st & 7u))) - 1u));                                  /* a staircase sine */
    case 15: return S(x * (((t >> 8) % 7u) + 1u)) * E >> 15;                                  /* partials mod 7 */
    case 16: {                                                                                 /* three against four */
        int32_t e3 = 32767 - (int32_t)((t % 3072u) * 10u), e4 = E;
        return (mulq15(S(x * (1u + (st & 1u))), e4) + mulq15(S((x >> 4) * (24u + (st & 2u) * 8u)), e3 > 0 ? e3 : 0)) >> 1;
    }
    case 17: return S((x >> 6) * (64u + ((t >> 6) & 255u))) * E >> 15;                        /* a riser each bar */
    case 18: return mulq15(S(x * (1u + st % 3u)) * ((t >> 10) & 1u ? 1 : 0) + S(x + (t << 2)), 16384);   /* beating, gated */
    case 19: {                                                                                 /* odd partials, built up */
        uint32_t k, n = 1u + ((t >> 11) & 3u);
        for (a = 0, k = 0; k < n; k++)
            a += S(x * (2u * k + 1u)) / (int32_t)(2u * k + 1u);
        return a * 3 >> 2;
    }
    case 20: return S(x * (2u + ((st * 5u) & 7u)) >> 1);                                      /* leaps */
    case 21: {                                                                                 /* a drum loop */
        uint32_t k = fr < 768u ? (768u - fr) * (768u - fr) >> 6 : 0u;
        *aux += (int32_t)k + 30;
        a = (st & 3u) == 0u ? sine_i((uint32_t)*aux << 13) * (int32_t)(768u - (fr < 768u ? fr : 768u)) / 768 : 0;
        b = (t >> (8u + (st & 3u))) & 1u ? (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(1024u - (t & 1023u)) >> 12 : 0;
        return (a + b + (S(x >> 1) >> 2)) >> 1;
    }
    case 22: return S(x * (1u + (((t >> 9) & (t >> 7)) & 7u)) + (((t >> 9) & (t >> 7) & 255u) << 9));   /* bytebeat in it */
    case 23: return S(x * 2u + PM(S(x * 5u), (int32_t)((t >> 11) & 3u) * 8));                 /* FM 5:2, stepped */
    case 24: a = S(x * (1u + (st & 3u))); return (int32_t)((int64_t)a * (a < 0 ? -a : a) >> 15) * E >> 15;   /* squared */
    case 25: return S(x & ~(1u << (8u + (st & 7u)))) * E >> 15;                              /* a bit of t dropped: jumps */
    case 26: return S((x >> 4) * (34u - ((t >> 11) & 15u)) >> 1) * E >> 15;                   /* a descending line */
    case 27: return (S(x) + mulq15(S(x * (((t >> 13) & 3u) + 2u)), E)) >> 1;                   /* a partial, struck */
    case 28: return S(x + PM(S(x * 2u), S(t) >> 10));                                         /* FM, its index a 1 Hz sine */
    case 29: return S((x >> 4) * (16u + ((t >> 7) & 63u)));                                   /* a glide up partials */
    case 30: h = ((st + 1u) * 2654435761u) >> 29; return S((x >> 4) * R[h]) * E >> 15;        /* a random line */
    default: a = ((st * 5u) & 7u) < 5u ? E : 0;                                               /* euclid hats, a chord */
        b = (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(512u - (t & 511u)) >> 11;
        h = (x >> 4) * R[(st >> 1) & 7u] >> 4;                                                 /* (the chord's root walks) */
        return (S(h * 16u) + S(h * 20u) + S(h * 24u)) / 4 + (mulq15(b, a) >> 1);
    }
}
#undef S
#undef PM

/* (JIANT 0.4) the filter and the deformations, for every wave: CUT a resonant low-pass (the ENV / LFO / matrix move it
 * as a synth's), BEND (BYTE: the time folded on itself, t ^ t >> s, new melodies and rhythms out of the same formula;
 * the other waves: the phase bent towards the cycle's start, as phase distortion: brighter, nasal), MASK (the 8-bit
 * sample's low bits flipped, up to six: grit, digital corruption). lofi_bend: a phase bent by the block's x, k0, k1 */
typedef struct { uint32_t x, k0, k1; } lofi_bend_t;
static void lofi_bend_set(lofi_bend_t *b, int32_t bend)
{
    b->x = 32768u - (uint32_t)bend * 250u;              /* (16-bit phase: the half way moved down to ~1/32) */
    b->k0 = (32768u << 16) / b->x;
    b->k1 = (32767u << 16) / (65535u - b->x);
}
static inline uint32_t lofi_bend(const lofi_bend_t *b, uint32_t ph)
{
    uint32_t p = ph >> 16;
    p = p < b->x ? (p * b->k0) >> 16 : 32768u + (((p - b->x) * b->k1) >> 16);
    return p << 16 | (ph & 0xFFFFu);
}
/* the sample out of the hold: quantized, MASKed, filtered, into out (the oscillators' shared tail) */
static inline int32_t lofi_mask(int32_t s, int32_t msk) { return msk ? (((s >> 8) ^ msk) << 8) : s; }

/* BYTE's samples (lofi_render's set-up done: inc the note's, the hold, the bits, the filter): t in v->s[2], its
 * fraction (Q24) in v->ph[0] */
static __attribute__((noinline)) void lofi_byte_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                      const vmod_t *m, uint32_t inc, int32_t hold, int32_t bits,
                                                      const tsvf_t *flt)
{
    const int16_t *p = t->p;
    uint32_t vr = (uint32_t)clamp(p[P_E3] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /* (0.6.81) LFO / ENV ->
                                                         * SHAPE move VAR (SHAPE did nothing on BYTE) */
    uint32_t f = (uint32_t)p[P_E2] >> 2, a = 1u + (vr >> 3), tt = (uint32_t)v->s[2], tf = v->ph[0], i;
    uint32_t ti = (inc >> 8) * 31u;                     /* t a sample, Q24: C4 (inc ~2.55e7) ~0.18 = 8 kHz */
    uint32_t ft = (uint32_t)p[P_FTYPE] % 6u;            /* (JIANT 0.5) the FILTER's type (COMB: open, voice.c combs) */
    int32_t kd = 8192 - (p[P_FRES] > 50 ? p[P_FRES] : 50) * 7600 / 127;   /* (0.5.4: the FILTER page's RES alone) */
    uint32_t bs = p[P_E6] ? 13u - (uint32_t)p[P_E6] / 11u : 0u;   /* BEND: the fold's shift, 13 .. 2 (0: none) */
    int32_t held = v->s[0], cnt = v->s[1], ic1 = v->s[4], ic2 = v->s[5], l1 = v->s[3], l2 = (int32_t)v->ph[2];
    uint32_t lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : 0xFFFFFFFFu;   /* LOOP: t's window */
    uint32_t t0 = (uint32_t)BYTE_T0[f & 31u] << 9;    /* (0.5.1: the formula's start) */
    uint32_t mul = lofi_motion(p[P_E4]), ts = v->ph[1], var = vr;   /* (0.5.4) MOTN: the slow time, Q8 */
    hold *= 1 + (p[P_E5] >> 3);                         /* (0.5.4) GRIT: each sample held up to 16 x longer */
    for (i = 0; i < n; i++) {
        if (--cnt <= 0) {
            uint32_t te = mul == 256u ? tt : (tt & 255u) | ((ts >> 8) & ~255u);   /* (MOTN: the slow bits at their own
                                                         * speed; 64: t as written) */
            uint32_t tl = lofi_mut((te & lm) + t0, var), tw = bs ? tl ^ (tl >> bs) : tl;   /* (VAR: mutated) */
            int32_t s = (int32_t)(lofi_byte(f, tw, a) & 255u) * 256 - 32640;
            cnt = hold;
            if (bits < 8)
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            held = s;
        }
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        ts += ((ti >> 8) * mul) >> 16;
        l1 += ((held >> 1) - l1) * 19500 >> 15;        /* (JIANT 0.5) the 8-bit steps rounded: two one-poles at
                                                         * ~6 kHz before the filter (BYTE was knife-sharp) */
        l2 += (l1 - l2) * 19500 >> 15;
        out[i] += voice_amp(soft_knee(svf_mode(flt, l2, &ic1, &ic2, ft, kd), 16000) << 1, m, i);
    }
    v->s[3] = l1;
    v->ph[2] = (uint32_t)l2;
    v->s[0] = held;
    v->s[1] = cnt;
    v->ph[1] = ts;
    v->s[2] = (int32_t)tt;
    v->s[4] = ic1;
    v->s[5] = ic2;
    v->ph[0] = tf;
}

/* FLOAT's samples (lofi_render's set-up done: inc the note's, the filter): t in v->s[2], its fraction (Q24) in v->ph[0],
 * MOTN's slow t (Q8) in v->ph[1], a formula's own state in v->s[3], the last output in v->s[0] (0.5.6) */
static __attribute__((noinline)) void lofi_float_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                       const vmod_t *m, uint32_t inc, const tsvf_t *flt)
{
    const int16_t *p = t->p;
    uint32_t f = (uint32_t)p[P_E2] >> 2, tt = (uint32_t)v->s[2], tf = v->ph[0], ts = v->ph[1], i;
    uint32_t ti = inc >> 3;                             /* (t a sample, Q24: 32 a cycle of the note) */
    uint32_t mul = lofi_motion(p[P_E4]), var = (uint32_t)clamp(p[P_E3] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /*
                                                         * (0.6.81: SHAPE moves VAR, as BYTE's) */
    uint32_t bs = p[P_E6] ? 13u - (uint32_t)p[P_E6] / 11u : 0u;   /* BEND: t ^ t >> 13 .. 2 */
    uint32_t lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : 0xFFFFFFFFu;   /* LOOP: t's window */
    uint32_t ft = (uint32_t)p[P_FTYPE] % 6u;
    int32_t rs = p[P_FRES] > 50 ? p[P_FRES] : 50, kd = 8192 - rs * 7600 / 127, y1 = v->s[4], y2 = v->s[5];
    int32_t aux = v->s[3], fb = v->s[0], fbk = p[P_E5];
    uint32_t chip = (uint32_t)p[P_E0], qs = chip == CHIP_8BIT ? 0u : chip == CHIP_1BIT ? 15u : 12u;   /* (CHIP: the bits) */
    for (i = 0; i < n; i++) {
        uint32_t te = mul == 256u ? tt : (tt & 255u) | ((ts >> 8) & ~255u), tl, tw;
        int32_t s;
        tl = lofi_mut(te & lm, var);
        tw = bs ? tl ^ (tl >> bs) : tl;
        if (fbk) {                                        /* FBK: the last output moves t, up to +-16 t (half a turn of
                                                         * the sines, Q8): the formula modulates itself, FM-like */
            int32_t d = (fb * fbk) >> 10;
            s = lofi_float(f, tw + (uint32_t)(d >> 8), (tw << 8 | tf >> 16) + (uint32_t)d, &aux);
        } else {
            s = lofi_float(f, tw, tw << 8 | tf >> 16, &aux);
        }
        if (qs)
            s = ((s + (1 << (qs - 1u))) >> qs) << qs;
        if (chip == CHIP_4B2 && (tt & 1u))                /* (4B/2: held over two t) */
            s = fb;
        fb = s;
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        ts += ((ti >> 8) * mul) >> 16;
        out[i] += voice_amp(soft_knee(svf_mode(flt, s >> 1, &y1, &y2, ft, kd), 16000) << 1, m, i);
    }
    v->s[0] = fb;
    v->s[2] = (int32_t)tt;
    v->s[3] = aux;
    v->s[4] = y1;
    v->s[5] = y2;
    v->ph[0] = tf;
    v->ph[1] = ts;
}

static const param_desc_t *lofi_desc(const track_t *t, uint32_t k)
{
    if (t->p[P_E1] == RW_FLOAT && (k == 0u || k == 2u || k == 3u))
        return k == 0u ? &LOFI_FCHIP : k == 2u ? &LOFI_FALGO : &LOFI_VAR;
    if (t->p[P_E1] == RW_BYTE && (k == 2u || k == 3u))
        return k == 2u ? &LOFI_ALGO : &LOFI_VAR;
    if ((t->p[P_E1] == RW_BYTE || t->p[P_E1] == RW_FLOAT) && (k == 4u || k == 5u || k == 7u))   /* (0.5.4: MOTN,
                                                         * GRIT / DCY where an empty slot and a second RES were) */
        return k == 4u ? &LOFI_MOTN : k == 7u ? &LOFI_LOOP : t->p[P_E1] == RW_BYTE ? &LOFI_GRIT : &LOFI_FDCY;
    if (k == 4u)
        return &LOFI_NONE;
    if (k == 2u && t->p[P_E1] == RW_WRAM)
        return &LOFI_WAVNUM;
    if (k == 3u && t->p[P_E0] == CHIP_STEP)
        return &LOFI_DCY;
    return 0;
}

/* the stepped envelope: volume 15..0 in v->s[6] bits 0..3, its divider in bits 4..7, a 240 Hz
 * frame clock (Q16 phase in v->s[7]); each frame the divider counts down and, at 0, reloads the
 * period and takes one step off the volume (a looping one starts again at 15 after 0) */
#define FRAME_Q16 ((240u * CTL * 65536u) / FS)       /* frames per control tick, Q16 */
static int32_t step_period(int32_t b) { return b <= 16 ? b - 1 : b - 16; }

static void lofi_note_on(track_t *t, voice_t *v)
{
    v->s[0] = 0;                 /* held sample */
    v->s[1] = 0;                 /* hold counter */
    v->s[2] = t->p[P_E1] >= RW_BYTE ? 0 : 0x7FFF;   /* LFSR (BYTE, FLOAT: its time t, from 0) */
    if (t->p[P_E1] >= RW_BYTE)                          /* (BYTE: its slow time; FLOAT: its phase, a step at once) */
        v->ph[1] = 0u;
    v->s[3] = 0;                 /* (BYTE: its smoothing's first pole; SWEEP retired in 0.4) */
    v->ph[2] = 0;                /* (.. its second; FLOAT: its patch's own state) */
    v->s[4] = v->s[5] = 0;       /* (JIANT 0.4) the filter's two states */
    v->s[6] = 15 | (step_period(t->p[P_E3] >> 2) & 15) << 4;   /* stepped env: every note-on restarts it */
    v->s[7] = 0;
    if (t->p[P_E1] == RW_FLOAT)                         /* (0.5.6: its t from 0, its fraction too) */
        v->ph[0] = v->ph[1] = 0;
}

/* CHIP = STEP: the voice amplitude is the stepped envelope, not the ADSR curve. The ADSR only
 * gates: held, it is kept at full; after note-off the staircase keeps running (as the chip's
 * envelope never sees a note-off) and the voice is cut when the ADSR release has fallen to 1 %
 * (about the REL time; REL 0 cuts at once) or a one-shot decay has reached 0. */
static int32_t lofi_amp(track_t *t, voice_t *v, int32_t adsr)
{
    int32_t b = t->p[P_E3] >> 2, vol = v->s[6] & 15, div = (v->s[6] >> 4) & 15;
    if (t->p[P_E0] != CHIP_STEP || t->p[P_E1] == RW_FLOAT)   /* (FLOAT: no STEP, its s[6] s[7] its own) */
        return adsr;
    if (!v->active)                                     /* the ADSR ended it (or the voice was given up) */
        return 0;
    if (v->stage == 4u)                                 /* (0.6.74) given up: env_tick's fade, not held at full */
        return adsr;
    if (v->stage == 3u) {
        if (v->env < (1 << 24) / 100 || (b >= 1 && b <= 16 && !vol)) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
            return 0;
        }
    } else {
        v->env = 1 << 24;                               /* the release starts from full */
    }
    if (!b)
        return 15 * Q4;
    v->s[7] += (int32_t)FRAME_Q16;
    while (v->s[7] >= 65536) {
        v->s[7] -= 65536;
        if (div) {
            div--;
        } else {
            div = step_period(b);
            if (vol)
                vol--;
            else if (b > 16)
                vol = 15;
        }
    }
    v->s[6] = vol | div << 4;
    return vol * Q4;
}

/* (0.6.45) MORPH: the wave it morphs towards (PLS TRI SAW NOIS WRAM), its sample with no state moved */
static int32_t lofi_mto(uint32_t w, uint32_t ph0, uint32_t duty, const uint8_t *wr, int32_t wdc, uint32_t lfsr)
{
    switch (w) {
    case RW_TRI: return osc_tri(ph0);
    case RW_SAW: return (int32_t)(ph0 >> 16) - 32768;
    case RW_NOIS: return (lfsr & 1u) ? 32767 : -32768;
    case RW_WRAM: {
        uint32_t k = ph0 >> 27, b = wr[k >> 1];
        b = (k & 1u) ? b & 15u : b >> 4;
        return ((int32_t)b * 2 - 15) * Q4 - wdc;
    }
    default: return (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
    }
}
static void lofi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    static const uint8_t STEP_DUTY[4] = {1, 2, 4, 6};   /* eighths: 12.5 / 25 / 50 / 75 % */
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], wave = (uint32_t)p[P_E1], i;
    uint32_t stepc = chip == CHIP_STEP, ft = (uint32_t)p[P_FTYPE] % 6u;   /* (JIANT 0.5) the FILTER's type */
    int32_t kd;
    uint32_t duty = stepc ? 0x20000000u * STEP_DUTY[(p[P_E2] / 32) & 3]
                          : 0x20000000u * (1u + (uint32_t)p[P_E2] / 32u);   /* 12.5 / 25 / 37.5 / 50 % */
    int32_t crush = stepc ? 0 : p[P_E3];                /* STEP: CRSH is the envelope */
    int32_t bits = chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4;
    int32_t hold = 1 + crush / 8 + (chip == 1u ? 1 : 0);
    int32_t held = v->s[0], cnt = v->s[1], ic1 = v->s[4], ic2 = v->s[5], msk = p[P_E7] >> 1;   /* (locals: out[] may
                                                         * alias v->s[]) */
    tsvf_t flt;
    lofi_bend_t bd;
    uint32_t inc, ph = v->ph[0], lfsr = (uint32_t)v->s[2];
    const uint8_t *wr = WRAM[((uint32_t)p[P_E2] >> 3) & 15u];
    int32_t wdc = 0, mf = 0, dmf = 0;                    /* (0.6.50: MORPH's blend Q8, ramped across the block) */
    if (wave == RW_WRAM) {                              /* the table's mean: no DC from lopsided shapes */
        int32_t sum = 0;
        for (i = 0; i < 16u; i++)
            sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
        wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;   /* (2 v - 15) Q4, or +-32767 at 1 bit */
    }
    inc = pitch_inc(clamp(m->pitch16, 0, 2047));       /* (the vibrato below; the track's LFO in m->pitch16 already) */
    {                                                   /* (JIANT 0.5) its RES the FILTER page's (or BYTE's, the more) */
        int32_t rs = 50;                                /* (0.5.4: BYTE's E5 is GRIT now, not a second RES) */
        if (p[P_FRES] > rs) rs = p[P_FRES];
        tsvf_coef(&flt, (p[P_FCUT] << 8) + m->cutoff, rs);   /* (JIANT 0.5: the FILTER page's CUT, was E4) */
        kd = 8192 - rs * 7600 / 127;
    }   /* CUT, a little
                                                         * resonance (BYTE: RES up to the edge) */
    if (p[P_E6])
        lofi_bend_set(&bd, p[P_E6]);
    if (p[P_E5] && wave < RW_BYTE)                      /* (BYTE, FLOAT: VIB's slot is RES) */
        inc += (uint32_t)(((int32_t)(inc >> 12) * (((osc_sine(v->ph[1]) >> 8) * p[P_E5]) >> 4)) >> 4);   /* no overflow */
    if (wave < RW_BYTE)
        v->ph[1] += 0x01000000u;                        /* (BYTE, FLOAT: their own time in ph[1]) */
    if (wave == RW_FLOAT) {                             /* (JIANT 0.5.1) floatbeat */
        lofi_float_render(t, v, out, n, m, inc, &flt);
        return;
    }
    if (wave == RW_BYTE) {                              /* (JIANT 0.4) bytebeat */
        lofi_byte_render(t, v, out, n, m, inc, 1 + (chip == 1u ? 1 : 0), bits, &flt);
        return;
    }
    {   /* (0.6.45) MORPH: from WAVE on towards the next ones (to WRAM), the knob and SHAPE's modulation */
        int32_t mo = clamp(t->mo_cur + m->shape - (64 << 8), 0, 127 << 8);
        int32_t mo0 = clamp(t->mo_prev + m->shape - (64 << 8), 0, 127 << 8);
        if (mo || mo0) {
            int32_t pos = clamp((int32_t)wave * 256 + mo * 768 / (127 << 8), 0, RW_WRAM * 256);
            int32_t p0 = clamp((int32_t)wave * 256 + mo0 * 768 / (127 << 8), 0, RW_WRAM * 256);
            wave = (uint32_t)pos >> 8;
            mf = (pos & 255) << 8;
            dmf = (p0 >> 8) == (pos >> 8) ? (mf - ((p0 & 255) << 8)) / (int32_t)(n ? n : 1u) : 0;
            mf -= dmf * (int32_t)n;
            if ((wave == RW_WRAM || (mf && wave + 1u == RW_WRAM)) && !wdc) {   /* (into WRAM: its mean, as above) */
                int32_t sum = 0;
                for (i = 0; i < 16u; i++)
                    sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
                wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;
            }
        }
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        if (--cnt <= 0) {
            uint32_t q = bits < 16, ph0 = p[P_E6] && wave != RW_NOIS ? lofi_bend(&bd, ph) : ph;   /* (BEND) */
            cnt = hold;
            switch (wave) {
            case RW_TRI:
                if (stepc) {                            /* 32 steps, 4 bits: 15..0, 0..15 */
                    uint32_t k = ph0 >> 27;
                    s = (int32_t)(k < 16u ? 15u - k : k - 16u) * (2 * Q4) - 15 * Q4;
                    q = 0;
                } else {
                    s = osc_tri(ph0);
                }
                break;
            case RW_SAW:
                s = (int32_t)(ph0 >> 16) - 32768;
                break;
            case RW_NOIS: {
                uint32_t l = lfsr;
                if ((ph + inc * (uint32_t)hold) < ph || chip == 3u)
                    l = (l >> 1) | (((l ^ (l >> 1)) & 1u) << 14);
                lfsr = l;
                s = (l & 1u) ? 32767 : -32768;
                break;
            }
            case RW_WRAM: {                             /* step-wise, no interpolation */
                uint32_t k = ph0 >> 27, b = wr[k >> 1];
                b = (k & 1u) ? b & 15u : b >> 4;
                s = (bits == 1 ? (b >= 8u ? 32767 : -32767) : ((int32_t)b * 2 - 15) * Q4) - wdc;
                q = 0;
                break;
            }
            default:                                    /* minus its mean: no DC for narrow pulses */
                s = (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
                break;
            }
            if (mf >> 8)                                /* (0.6.45) MORPH: towards the next wave */
                s += ((lofi_mto(wave + 1u, ph0, duty, wr, wdc, lfsr) - s) * (mf >> 8)) >> 8;
            if (q)                                      /* rounded, not floored (floor = DC) */
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            if (crush > 64)
                s = (s >> 12) << 12;
            held = lofi_mask(s, msk);
        }
        ph += inc;
        mf += dmf;                                      /* (0.6.50) MORPH's ramp */
        out[i] += voice_amp(soft_knee(svf_mode(&flt, held >> 1, &ic1, &ic2, ft, kd), 16000) << 1, m, i);
    }
    v->ph[0] = ph;
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)lfsr;
    v->s[4] = ic1;
    v->s[5] = ic2;
}

static const preset_t LOFI_PRESETS[] = {
    /* name, {CHIP, WAVE, DUTY, CRSH, CUT, VIB, BEND, MASK} (JIANT 0.4: BYTE first; CUT BEND MASK were SWP ARP TONE) */
    /* BYTE: 8 bits, ALGO B02 (DUTY 4 / 4 = 1); B17 (64 / 4 = 16) with VAR 40 (a = 6); B05 folded (BEND), resonant */
    {"BYTEBEAT", {2, 5, 4, 0, 64, 0, 0, 0}, {0, 80, 110, 40}, 0, 1, FX(0, 0, 25, 20), PAT(4)},
    {"BYTE VAR", {2, 5, 64, 40, 64, 0, 0, 0}, {0, 80, 110, 50}, 0, 1, FX(0, 20, 30, 25), PAT(3)},
    {"BYTE BENT", {2, 5, 16, 0, 64, 24, 70, 0}, {0, 80, 110, 45}, 0, 1, FX(10, 10, 30, 25), PAT(4)},   /* (0.5.4: GRIT 24) */
    {"PULSE LD", {0, 0, 32, 0, 127, 20, 0, 0}, {0, 60, 90, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"WAVE BASS", {1, 1, 0, 0, 90, 0, 0, 0}, {0, 50, 70, 20}, 0, 1, FX(0, 0, 10, 0), PAT(2)},
    {"8BIT KEYS", {2, 0, 96, 0, 110, 0, 30, 0}, {0, 60, 80, 40}, 0, 0, FX(0, 0, 30, 20), PAT(13)},
    /* wave RAM VOXA (DUTY 92 / 8 = 11), a little vibrato */
    {"WAVE LEAD", {0, 4, 92, 0, 110, 18, 0, 0}, {0, 70, 90, 30}, 0, 1, FX(0, 0, 40, 25), PAT(3)},
    /* STEP: 25 % pulse, DCY 34 = D7 (a 15-step decay over 0.5 s), REL ~54 ms of staircase after note-off */
    {"STEP LEAD", {4, 0, 40, 34, 127, 16, 0, 0}, {0, 64, 127, 55}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    /* (JIANT 0.5, appended: the stored numbers stay) BYTE B11 looped (LOOP 80: a short cycle, a tone at the note), resonant */
    {"BYTE TONE", {2, 5, 40, 0, 64, 0, 0, 80}, {0, 70, 100, 40}, 0, 1, FX(0, 0, 30, 20), PAT(4)},
    /* (JIANT 0.5.1, appended; 0.5.6 floatbeat) FLOAT: F02 FM whose index walks; F03 a nibble-table melody;
     * F29 FM breathing at 1 Hz, slow */
    {"FLOAT FM", {2, 6, 4, 0, 64, 0, 0, 0}, {0, 60, 90, 40}, 0, 1, FX(0, 15, 25, 25), PAT(4)},
    {"FLOAT ARP", {2, 6, 8, 0, 64, 0, 0, 0}, {0, 50, 80, 50}, 0, 1, FX(0, 25, 30, 30), PAT(3)},
    {"FLOAT PAD", {2, 6, 112, 0, 40, 0, 0, 0}, {60, 90, 110, 90}, 0, 1, FX(30, 10, 50, 40), PAT(3)},
    /* (GAMMAWAVE / WRAITH) glitch & radioactive byte/float patches */
    {"DATAMOSH", {2, 6, 68, 48, 84, 32, 45, 0}, {0, 75, 100, 45}, 0, 1, FX(25, 15, 35, 25), PAT(4)},
    {"GEIGER BIT", {3, 5, 96, 80, 72, 48, 85, 40}, {0, 65, 90, 35}, 0, 1, FX(35, 0, 30, 20), PAT(2)},
    /* (QUANTUM // QG-1, appended: the stored numbers stay) a chip arp, crushed keys, a vibrato pulse, a staircase pad */
    {"CHIP ARP", {0, 0, 32, 0, 127, 0, 0, 0}, {0, 55, 60, 30}, 0, 0, FX(0, 0, 35, 20), PAT(13)},
    {"CRUSH KEYS", {2, 0, 96, 60, 90, 10, 20, 0}, {0, 60, 80, 40}, 0, 0, FX(10, 10, 30, 30), PAT(6)},
    {"VIB PULSE", {0, 0, 64, 0, 120, 40, 0, 0}, {0, 60, 90, 40}, 0, 1, FX(0, 0, 40, 25), PAT(4)},
    {"STEP PAD", {4, 0, 64, 20, 100, 10, 0, 0}, {40, 90, 110, 80}, 0, 0, FX(0, 40, 40, 60), PAT(5)},
};

static const engine_t ENG_LOFI = {
    .name = "LOFI",
    .page_title = {"CHIP", "TONE"},
    .edit = {
        {"CHIP", F_ENUM, 0, 4, 0, N_CHIP, 0},
        {"WAVE", F_ENUM, 0, 6, 0, N_RWAVE, 0},
        {"DUTY", F_INT, 0, 127, 64, 0, 0},              /* WRAM: the table, DUTY / 8 (lofi_desc) */
        {"CRUSH", F_PCT, 0, 127, 0, 0, 0},               /* STEP: the envelope (lofi_desc) */
        {"CUT", F_CUTOFF, 0, 127, 110, 0, 0},          /* (JIANT 0.4: the filter, was SWP) */
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"BEND", F_PCT, 0, 127, 0, 0, 0},               /* (JIANT 0.4: the deformations, were ARP and TONE) */
        {"MASK", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = LOFI_PRESETS,
    .npresets = NELEM(LOFI_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .keep = 0x31,                /* the held sample and the filter */
    .amp = lofi_amp,
    .desc = lofi_desc,
};
