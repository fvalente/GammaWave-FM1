/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST insert, then sends into three
 * shared buses (chorus, tempo delay, reverb). Mono buses, stereo dry mix. */
#define DLY_LEN 65536u           /* 1.49 s: 1/4 at 40 BPM fits */
#define CHO_LEN 2048u
static int16_t dly_buf[DLY_LEN] __attribute__((section(".pool")));
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[2] = {556, 441};
static int16_t rev_comb[1116 + 1188 + 1277 + 1356] __attribute__((section(".pool")));
static union {                          /* ROOM's allpasses; SPRING's allpass chain (int32: no clamps) */
    int16_t ap[556 + 441];
    int32_t sp[(556 + 441) / 2];
} rev_u __attribute__((section(".pool")));
#define rev_ap (rev_u.ap)
#define RPRE_LEN 2048u                   /* (JIANT) the reverb's pre-delay line, at half rate (4 KB): up to 92 ms */
static int16_t rpre_buf[RPRE_LEN] __attribute__((section(".pool")));
static int32_t rpre_out[CTL];                     /* .. the block out of it (the reverb's input) */
static struct {
    uint32_t dly_w, cho_w, cho_ph;
    int32_t dly_lp, dly_hp;              /* the delay's feedback: high cut (COLR), low cut (JIANT: HPF) */
    uint32_t rpre_w, rm_ph;              /* (JIANT) the pre-delay's write count (samples); the reverb modulation's phase */
    int32_t rm_m[4], rf_lp;              /* .. the combs' offsets (Q8 samples); the reverb's input filter (FILT) */
    uint32_t gr_ph, gr_sp[2];            /* .. the GRAIN delay: grain 0's phase (Q16), each grain's spray (samples) */
    int32_t gr_s[2];                     /* .. (0.6.81) each grain's PITCH (its ratio - 1, Q16), taken as it starts */
    uint32_t dl_now, dl_old, dr_now, dr_old;   /* (0.6.81) the delay's read points (left / mid, the right echo) and the
                                         * ones faded from: TIME, WIDE, PITCH turned crossfade, never a jump */
    uint16_t dl_xf, dr_xf;               /* .. the fades left (DXF_LEN .. 0 samples) */
    uint8_t gr_on, gr_old;               /* .. the GRAIN read now, and in the read faded from */
    uint8_t primed;                      /* .. set on the first block (no fade nor glide from 0 then) */
    uint32_t dw_ph, df_ph;               /* .. (0.6.86) the tape's WOW and FLUTTER phases */
    int32_t dw_m;                        /* .. the read points' offset by them now (Q8 samples) */
    int32_t dt1, dt2, rt1, rt2;          /* .. (0.6.86) the resonant TONE (delay loop) and FILT (reverb input) filters */
    int32_t cd_cur, cw_cur, dm_cur;      /* .. the chorus's DEPTH, its side's width, the delay's MIX as they are (glide) */
    int32_t ph_s[4], ph_y;               /* .. (QG-1) the PHASER's four all-pass states and its feedback */
    int32_t rpre_s;                      /* .. the even sample, averaged with the odd one into the line */
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING, 0.5: 2 SHIMMER 3 RESO 4 CLOUD) */
    uint32_t sh_w, sh_ph;                /* (JIANT 0.5) SHIMMER: its line's write count, the grains' phase (Q16, 0.6.12) */
    int32_t rs_lp[4];                    /* .. RESO: the combs' loop low-passes */
    uint32_t rs_per[4];                  /* .. (0.6.5) their periods now (Q8), gliding to the chord's */
    uint8_t rs_root;                     /* .. the note FOLW last heard + 1 (0: none yet) */
    uint16_t rs_w;                       /* .. their write index (each its own line, 1024) */
    uint32_t cl_w, cl_rng;               /* .. CLOUD: its line's write count (half rate), its random numbers */
    int32_t cl_next, cl_lp, cl_half, cl_gl, cl_gr;     /* .. the samples to the next grain, the output's low-pass, the odd sample */
    int32_t cl_dc;                       /* .. (1.2.1-GW, JIANT 0.9.14) its loop's DC (Q8 samples) */
    uint16_t cl_fill;                    /* .. (1.2.1-GW, JIANT 0.9.16) sound written into its line since it was cleared
                                          * (full-rate samples, to 32768) */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    uint8_t sp_ns;                       /* (0.6.70) SPRING's allpass stages last block (SP_N, LOAD ECO SP_N_ECO) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
} fx;

/* SPRING (G_RTYPE 1): one spring of a spring tank, mono like the other buses, in the ROOM's own buffers (no
 * RAM of its own): the input and the loop's return -> a low cut (~110 Hz: a spring carries little bass) ->
 * SP_N stretched first-order allpasses, (a + z^-4) / (1 + a z^-4) (after Valimaki, Parker and Abel: below
 * fs / 8 = 5.5 kHz the group delay rises with frequency, the chirp; each pass round the loop adds more of
 * it: the "boing", the drips) -> the loop's delay line (rev_comb, SP_LEN) -> back through a one-pole
 * low-pass (DAMP) and the decay gain (SIZE). The output: the spring's far end, half way along the loop
 * (the first sound 15 .. 30 ms after the send: the tank's own pre-delay; a slow wobble of a sample or two
 * on it), plus a second, quieter pickup at three quarters (a shorter spring beside it: denser). SIZE sets
 * the loop's length (30 .. 60 ms) and its decay; DAMP the loop's low-pass. The allpasses' states: 4
 * samples each (int32), in rev_ap's memory. Changing the model fades the old one's block out and clears both buffers. */
#define SP_LEN 4096u                     /* the loop's line in rev_comb (4937 samples) */
#define SP_MASK (SP_LEN - 1u)
#define SP_N 10u                         /* allpass stages */
#define SP_N_ECO 4u                      /* (0.6.70) .. on LOAD ECO: the chain was most of its cost (heavier than ROOM) */
#define SP_A 2867                        /* their coefficient, Q12 (0.7: Q12 keeps (x - o) * a in 32 bits up to
                                          * |x - o| < 749000, far past any peak the chain reaches) */
_Static_assert(sizeof rev_comb / 2u >= SP_LEN && sizeof rev_u.sp / 4u >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");

/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain (straight into tanh over the full band, it would sound like a
 * broken digital fuzz). State per part (track_t dist_*). */
/* (JIANT) DIST TYPE (P_DTYPE): SOFT the biased tanh as before; HARD a hard clip; FOLD a triangle wavefolder (the drive
 * folds the wave back on itself: more partials as it rises); CRUSH fewer bits and a held sample (the drive takes both
 * down); RECT full-wave rectified (an octave up, fuzz), its DC taken out. Then the two-pole low-pass, TONE (P_DTONE)
 * moving it darker / brighter, and the make-up. DRIVE 0: out of the chain */
enum { DT_SOFT, DT_HARD, DT_FOLD, DT_CRUSH, DT_RECT, DT_HEARTH, DT_BLAST, DT_PLASMA, DT_FLUX };
static __attribute__((noinline)) int32_t dist_shape(track_t *t, uint32_t type, int32_t x, int32_t g, int32_t d)
{
    int32_t v = ((x >> 2) * g) >> 10, a;
    switch (type) {
    case DT_HARD:
        return clamp(v, -22000, 22000);
    case DT_FOLD:                                        /* folded back at +-T (T 22000), up to 5T: three folds */
        v = clamp(v, -110000, 110000);
        for (a = 0; a < 3; a++)
            v = v > 22000 ? 44000 - v : v < -22000 ? -44000 - v : v;
        return v;
    case DT_CRUSH: {
        uint32_t hold = 1u + (uint32_t)d / 12u, sh = 4u + (uint32_t)d / 12u;   /* (up to 11x held, 4 .. 14 bits off) */
        if (!t->dist_hc) {
            t->dist_hold = (int32_t)((uint32_t)(clamp(x, -131072, 131068) >> sh) << sh);
            t->dist_hc = (uint8_t)hold;
        }
        t->dist_hc--;
        return clamp(((t->dist_hold >> 2) * g) >> 10, -26000, 26000);   /* (the drive's gain as the others: level kept) */
    }
    case DT_RECT:
        a = x < 0 ? -x : x;
        t->dist_dc += (a - t->dist_dc) >> 9;             /* (its mean: the DC the rectifier adds) */
        return softclip((((a - t->dist_dc) >> 2) * g) >> 10);
    case DT_HEARTH: {                                    /* GammaWave Hearth: tube Heat + HF Sparks + sub Weight */
        int32_t h = softclip(v + 4800) - softclip(4800);
        int32_t dx = x - t->dist_hold;
        int32_t sp, wt;
        t->dist_hold = x;
        sp = softclip(((dx >> 2) * g) >> 10) >> 2;
        a = x < 0 ? -x : x;
        t->dist_dc += (a - t->dist_dc) >> 7;
        wt = (x >= 0 ? t->dist_dc : -t->dist_dc) >> 3;
        return clamp(h + sp + wt, -28000, 28000);
    }
    case DT_BLAST: {                                     /* WRAITH Blast: 4-stage sinusoidal wavefolder + sub body */
        int32_t f1 = sine_i((uint32_t)(v * 42000));
        int32_t f2 = sine_i((uint32_t)(f1 * (48000 + d * 600)));
        return (f2 * 3 + softclip(v)) >> 2;
    }
    case DT_PLASMA: {                                    /* GammaWave Singularity/Plasma: envelope-warped AM saturation */
        int32_t warp, core;
        a = v < 0 ? -v : v;
        t->dist_dc += (a - t->dist_dc) >> 5;
        warp = sine_i((uint32_t)((v + (t->dist_dc >> 1)) * (36000 + d * 500)));
        core = softclip(v + (warp >> 1));
        return clamp(core + ((core * warp) >> 15), -28000, 28000);
    }
    case DT_FLUX: {                                      /* GammaWave Flux: thermal bias drift + Geiger bit-starve */
        int32_t jit, q;
        t->dist_hc++;
        if ((t->dist_hc & 7u) == 0u)
            t->dist_hold = (int32_t)((uint32_t)t->dist_hold * 1664525u + 1013904223u);
        jit = (t->dist_hold >> 19) * d >> 6;
        q = (v + jit) & ~(int32_t)((1u << (3u + (uint32_t)d / 20u)) - 1u);
        return softclip(q + (jit >> 1));
    }
    default:
        return softclip(v + 2400) - softclip(2400);
    }
}
/* (JIANT 0.6.8, the crackles turning DIST up from 0) DST 1..8 blends the dry part into the drive (it was all drive
 * at 1: the low cut and the clip at once), the blend eased a block at a time and ramped across it; at 0 out of the
 * chain once the blend is down */
static int32_t dist_w[NTRK];                            /* the blend the last block ended at, Q12 */
static void track_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, b0 = softclip(2400), w0, w1, dw, full;
    uint32_t type = (uint32_t)t->p[P_DTYPE], ti = (uint32_t)(t - trk) % NTRK;
    w0 = dist_w[ti];
    w1 = d >= 8 ? 4096 : d * 512;
    if (w1 != w0)                                       /* (half the way a block, then there) */
        w1 = w1 - w0 > 64 || w0 - w1 > 64 ? w0 + (w1 - w0) / 2 : w1;
    dist_w[ti] = w1;
    if (!w0 && !w1)
        return;                                         /* out of the chain */
    if (!w0)                                            /* in: the filters from silence, the low cut from here */
        t->dist_lp1 = t->dist_lp2 = 0, t->dist_hp = 0;
    dw = (w1 - w0) >> CTL_LOG2;
    full = w0 >= 4096 && w1 >= 4096;
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = clamp(32000 - d * 95 + t->p[P_DTONE] * 300, 2500, 32767);   /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = type == DT_HARD || type == DT_FOLD || type == DT_CRUSH || type == DT_BLAST ? 30000 - d * 60 : 30000 - d * 110;   /* make-up */
    if (type > DT_FLUX)
        type = DT_SOFT;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = type == DT_SOFT ? softclip((((x >> 2) * g) >> 10) + 2400) - b0 : dist_shape(t, type, x, g, d);
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        y = mulq15(t->dist_lp2, mk);
        b[i] = full ? y : b[i] + ((((y - b[i]) >> 2) * (w0 + dw * (i + 1))) >> 10);   /* (>> 2: fits 32 bits) */
    }
}

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* (JIANT) CLIP (G_CLIP 0..100): the mix driven x1 .. x4 into the soft clip ahead of the limiter, its level kept near
 * -4 dBFS (as DRUM's DRV); 0: out of the chain. Set each block (master_begin) */
static int32_t clip_g, clip_mk;
/* the mix (dry + wet, before the MASTER knob: the same saturation at any volume) through CLIP, in place; wet
 * cleared (it is in the mix now). Its own loop, only while CLIP is on: the ISR's master loop stays as it was */
static int32_t mix_l[CTL], mix_r[CTL], wet[CTL];
/* (JIANT 0.6.75) the buses run a block behind, on the second core when there is one (mix_block): what they give, kept
 * apart until the next block's mix takes it: their sides (the chorus's, the delay's, the reverbs' WIDE: left and right
 * as they were added to mix_l / mix_r), their return; the sends they work on (the block before's); their scratch */
static int32_t fx_sl[CTL], fx_sr[CTL], fx_wet[CTL], fxq_c[CTL], fxq_d[CTL], fxq_r[CTL], fx_tmp[CTL];
static __attribute__((noinline)) void clip_block(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {                           /* (>> 4: the sums may be far above Q15; << 2 back; 0.6.14:
                                                         * rounded, not floored: -1 .. -15 of a tail's residue came
                                                         * out -1, x4 drive, a -96 offset with a hiss on the side
                                                         * that held it, while +1 .. +15 were 0) */
        int32_t l = (mix_l[i] + wet[i] + 8) >> 4, r = (mix_r[i] + wet[i] + 8) >> 4;
        mix_l[i] = ((softclip(l * clip_g >> 10) * clip_mk + 16384) >> 15) << 2;
        mix_r[i] = ((softclip(r * clip_g >> 10) * clip_mk + 16384) >> 15) << 2;
        wet[i] = 0;
    }
}
static __attribute__((noinline)) void master_begin(void)
{
    int32_t a = clip_eff;
    clip_g = a > 0 ? 4096 + a * 12288 / 100 : 0;
    if (clip_g)                                         /* (JIANT 0.5) made up at a mix's usual level (-15 dBFS), not near
                                                         * full scale: CLIP adds saturation, not loudness (the leveler) */
        clip_mk = (int32_t)((6000u << 15) / (uint32_t)softclip((6000 * clip_g) >> 12));
}
/* (JIANT 0.5) the LEVELER, always on: the mix (dry + wet, before the MASTER knob) followed block by block (its peak,
 * ~45 ms up, ~1.5 s down: no pumping) and brought toward LEV_T at 2:1 (the gain the square root of LEV_T / level, -9 .. +6 dB),
 * ramped across the block; the gain itself smoothed (~190 ms); below LEV_GATE (-40 dB) it eases back to unity, so a tail or silence is not
 * pumped up. The peak
 * limiter and the soft clip (master_out) after it catch what is left: quiet patches and CLIP's drive sit at one
 * loudness */
#define LEV_T 12000                                     /* (the mix's scale: Q15 at MASTER full) ~ -9 dBFS peaks: the
                                                         * limiter's headroom (0.5.1: was 16000, it limited 95 % of the time) */
#define LEV_GATE 400                                    /* -38 dB */
static int32_t lev_env, lev_g = 32768, lev_cur = 32768, lev_dg;   /* the gain now and its step a sample (master_out) */
static __attribute__((noinline)) void lev_block(uint32_t n)
{
    uint32_t i;
    int32_t pk = 0, g1;
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i] + wet[i], r = mix_r[i] + wet[i];
        l = l < 0 ? -l : l;
        r = r < 0 ? -r : r;
        if (l > pk) pk = l;
        if (r > pk) pk = r;
    }
    lev_env += pk > lev_env ? (pk - lev_env) >> 6 : -((lev_env - pk) >> 11);   /* (0.5.1: ~45 ms up, ~1.5 s down: it
                                                         * rides the song's loudness, not each hit; was ~1 / ~46 ms) */
    g1 = 32768;                                         /* (below the gate: back to unity, slowly) */
    if (lev_env > LEV_GATE && pk > LEV_GATE) {         /* (0.5.1: the block itself above the gate too: a tail
                                                         * dying under it is not lifted while the slow follower falls) */
        uint32_t q = ((uint32_t)LEV_T << 12) / (uint32_t)lev_env, rt = 0, b = 1u << 30;   /* LEV_T / level, Q12 */
        q <<= 8;                                        /* (Q20: its square root Q10) */
        while (b > q) b >>= 2;
        while (b) {
            if (q >= rt + b) { q -= rt + b; rt = (rt >> 1) + b; }
            else rt >>= 1;
            b >>= 2;
        }
        g1 = clamp((int32_t)rt << 5, 11500, 65536);     /* Q15: x0.35 .. x2 */
    }
    g1 = lev_g + ((g1 - lev_g) >> (lev_env > LEV_GATE && pk > LEV_GATE ? 8 : 5));   /* (smoothed: ~190 ms; gated, under -38 dB: ~25 ms
                                                         * back to unity, no gain left on a tail's offset) */
    lev_cur = lev_g;                                    /* (master_out ramps it across the block, after the DC block) */
    lev_dg = (g1 - lev_g) / (int32_t)n;
    lev_g = g1;
}
/* DUCK (G_DUCK 0..100, G_DREL its release 40 .. 600 ms): a kick (eng_drum.c duck_hit) takes the parts that are not
 * DRUM down by up to 30 dB in ~2 ms, holds them there ~25 ms, and they come back with the release, shaped as a
 * sidechain pump (the gain follows 1 - (1 - env)^2: low for longer, then a quick rise). duck_g0 / duck_g1: the gain
 * (Q15) at the block's start and end, ramped by mix_part */
static int32_t duck_env, duck_g0 = 32767, duck_g1 = 32767;
static uint8_t duck_att, duck_hold;
static __attribute__((noinline)) void duck_block(void)
{
    int32_t dep = song.g[G_DUCK] * 31731 / 100, e;       /* (1 - 0.032: -30 dB at 100) */
    if (duck_hit) {
        duck_hit = 0;
        duck_att = 1;
        duck_hold = (uint8_t)(1103u / CTL);              /* (25 ms in blocks) */
    }
    duck_g0 = duck_g1;
    if (duck_att) {
        duck_env += (32767 - duck_env) >> 1;
        if (duck_env > 31000)
            duck_att = 0;
    } else if (duck_hold) {
        duck_hold--;
    } else if (duck_env) {
        duck_env = (int32_t)(((uint32_t)duck_env * dv_kb(40000u + (uint32_t)song.g[G_DREL] * 5600u)) >> 16);
    }
    e = (duck_env * (65536 - duck_env)) >> 15;           /* 1 - (1 - env)^2 */
    duck_g1 = 32767 - mulq15(dep, e > 32767 ? 32767 : e);
}
/* MENU > USB LEVEL FIXED (for #42: record over USB with the speaker turned down): the mix goes to master_out at the
 * full MASTER level, USB audio takes that (audio.c uac_tap), and only then does MASTER scale what the DAC gets
 * (usb_fixed_dac). MASTER (0, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
static const uint8_t REV_LITE[3][5] = {{0, 1, 2, 3, 4}, {0, 1, 0, 1, 0}, {1, 1, 1, 1, 1}};   /* (0.6.29) LOAD FULL, LITE, ECO
                                                         * (voice.c sys_load): each reverb's lighter kin */
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
/* the MASTER pot (ADC 0..1023, smoothed in main.c) to its level, square law: 0 .. 4088 (Q12) */
static inline uint32_t master_of_pot(uint32_t k10) { return (k10 * k10) >> 8; }
static __attribute__((noinline)) void usb_fixed_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i;
    int32_t m = (int32_t)song.master_q12;
    for (i = 0; i < 2u * n; i++)
        out[i] = (out[i] * m) >> 12;                /* (|out| <= 32767 after the soft clip: fits) */
}
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (#42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself, late,
 * and cancelled up to 6 dB of it; now under 0.5 dB) */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
static inline int32_t spk_bass(int32_t m)
{
    int32_t a, t, u;
    sb_lp1 += ((m - sb_lp1) * 692) >> 15;
    sb_lp2 += ((sb_lp1 - sb_lp2) * 692) >> 15;
    sb_lp3 += ((sb_lp2 - sb_lp3) * 692) >> 15;
    sb_lp4 += ((sb_lp3 - sb_lp4) * 692) >> 15;
    a = sb_lp4 < 0 ? -sb_lp4 : sb_lp4;
    if (a > sb_env)
        sb_env += (a - sb_env) >> 2;
    else if (sb_env > 0)
        sb_env -= (sb_env >> 11) + 1;
    t = clamp(sb_lp4 * 8, -sb_env, sb_env);
    sb_h1 += (t - sb_h1) >> 5;
    u = t - sb_h1;
    sb_h2 += (u - sb_h2) >> 5;
    u -= sb_h2;
    sb_hl += (u - sb_hl) >> 3;
    return sb_hl * 3;
}

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;    /* rounded step stopped at |x - lc| < 32: an offset) */
        int32_t b = fx_lowcut == 2u ? spk_bass((*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    *l = (clamp(*l, -262143, 262143) * (lev_cur >> 4)) >> 11;   /* (JIANT 0.5) the leveler's gain (lev_block) */
    *r = (clamp(*r, -262143, 262143) * (lev_cur >> 4)) >> 11;
    lev_cur += lev_dg;
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 11) + 1;   /* (0.5.1: ~45 ms back, was ~90) */
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    uint32_t bpm = song.g[G_BPM] >= 40 ? (uint32_t)song.g[G_BPM] : 120u;   /* (1.2.1-GW, Felucca #282: a BPM 0 for a
                                                         * moment never divides) */
    return song.g[G_CLOCK] && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / bpm;
}
static uint32_t div_samples(uint32_t div)
{
    uint32_t quarter = beat_samples();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}

#include "perform.c"                                 /* the FX hold layer's effects (the master) */
static void seq_note_on(track_t *t, uint32_t note, uint32_t vel);   /* (0.6.72: seq.c, a note MIDI OUT ALL sends) */
static void seq_note_off(track_t *t, uint32_t note);
#include "pfx.c"                                   /* .. its punch-in MIDI effects (on the notes) */
#include "click.c"                                   /* the metronome's click (after the master: audio.c) */

/* (JIANT) GRAIN delay (G_DPIT / G_DSPRY not 0): the line is read by two grains of GR_LEN samples, half a grain
 * apart, each faded in and out (triangles: their sum is flat), each sweeping its read point so it plays PITCH
 * semitones up / down (as a tape pitch shifter); a grain starting again jumps back, SPRY further by a random
 * amount (up to ~190 ms). In the feedback: each repeat a further PITCH away (shimmer, falling echoes), scattered.
 * The same line (dly_buf), no RAM of its own */
#define GR_LEN 2048u
#define DXF_LOG2 10u
#define DXF_LEN (1u << DXF_LOG2)                         /* (0.6.81) a read point's crossfade: ~21 ms */
static const uint32_t GR_RATIO[25] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858, 65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715, 131072};   /* 2^(st/12), Q16 */
/* (0.6.81) the grains' clock, a sample: a grain starting again takes its spray and the PITCH now (turned in the middle
 * of a grain, its read point jumped: a click at every step of the knob) */
static void dly_grain_adv(void)
{
    uint32_t j;
    fx.gr_ph = (fx.gr_ph + 65536u / GR_LEN) & 0xFFFFu;
    for (j = 0; j < 2u; j++)
        if (((fx.gr_ph + j * 32768u) & 0xFFFFu) < 65536u / GR_LEN) {
            fx.gr_sp[j] = song.g[G_DSPRY] ? rng() % ((uint32_t)song.g[G_DSPRY] * 64u + 1u) : 0u;
            fx.gr_s[j] = (int32_t)GR_RATIO[clamp(song.g[G_DPIT], -12, 12) + 12] - 65536;
        }
}
static __attribute__((noinline)) int32_t dly_grain(uint32_t dl)
{
    int32_t x = 0;
    uint32_t j;
    for (j = 0; j < 2u; j++) {
        int32_t s = fx.gr_s[j];
        uint32_t e = (GR_LEN * (uint32_t)(s < 0 ? -s : s)) >> 8;   /* the read point's sweep over a grain, Q8 */
        uint32_t ph = (fx.gr_ph + j * 32768u) & 0xFFFFu, d, di, gl = dl;
        int32_t a, b, w;
        if (gl + fx.gr_sp[j] > DLY_LEN - 2u - GR_LEN * 2u)   /* (JIANT 0.5) a long TIME (1/1 and up: the whole line): */
            gl = DLY_LEN - 2u - GR_LEN * 2u - fx.gr_sp[j];  /* room for the sweep, else the read point sat clamped (no pitch).
                                                         * (0.6.19: each grain its own: grain 0's new spray moved grain 1's
                                                         * read point in the middle of its window: a click every grain) */
        d = ((gl + fx.gr_sp[j]) << 8) + ((e * ((s > 0 ? 65535u - ph : ph) >> 4)) >> 12);   /* Q8: up, the delay
                                                         * shrinks over the grain; down, it grows */
        di = d >> 8;
        if (di > DLY_LEN - 2u)
            di = DLY_LEN - 2u;
        a = dly_buf[(fx.dly_w - di) & (DLY_LEN - 1u)];
        b = dly_buf[(fx.dly_w - di - 1u) & (DLY_LEN - 1u)];
        w = (int32_t)(ph < 32768u ? ph : 65535u - ph);  /* the triangle, 0 .. 32767 */
        x += ((a + (((b - a) * (int32_t)(d & 255u)) >> 8)) * w) >> 15;
    }
    return x;
}

/* (0.6.86) the delay as an analog one (a tape, a BBD, a PT2399) feels: FEEDBK past 100 % grows into itself,
 * held by a saturation in the loop (a cubic, gain 1 for small signals, smoothly to full scale: it was a hard clip,
 * and the knob's top 84 %, so it never built up); each pass a little darker (~8 kHz, TONE lower darker still) and
 * thinner (~80 Hz, TONE higher thinner still); the read points drift by WOW (0.6 Hz, +-6 samples) and FLUTTER
 * (5.3 Hz, +-1), so each repeat bends a little further from the last. FEEDBK 0..80 as before */
static int32_t dly_fb(int32_t v)                    /* FEEDBK (0..120) -> the loop's gain, Q15: 100 = 1 */
{
    return v <= 80 ? v * 230 : v <= 100 ? 18400 + (v - 80) * 718 : 32760 + (v - 100) * 245;
}
static inline int32_t dly_sat(int32_t u)            /* Q15 in, out within +-32767 */
{   /* (0.6.89) exact up to half scale (a repeat the very copy of what came in), then a quadratic knee to full scale at
     * 1.5, flat there (the loop past 100 % grows into it and holds); it was a cubic everywhere (3.7 % off at half scale) */
    int32_t a = u < 0 ? -u : u, d;
    if (a <= 16384)
        return u;
    d = a - 16384;
    if (d > 32768) d = 32768;
    a = 16384 + d - ((d * d) >> 16);
    if (a > 32767) a = 32767;
    return u < 0 ? -a : a;
}
/* (0.6.86) TONE / FILT resonant: one knob, the middle open; one way a low-pass sweeping down, the other a low cut
 * sweeping up, both resonant (Q 0.7 near the middle, up to qmax from a third of the way): the band at the cutoff
 * stands out, so the knob carves where the repeats / the room sit. The level evened out (x 1/Q: the peak about
 * unity, what is around it a little lower). v -64..63; lo the cutoff index (CUTOFF_HZ) at the far low-pass end, hi at
 * the far low cut end, top the low-pass's start; kmin the damping at the most resonant (Q12) */
typedef struct { tsvf_t c; int32_t k, g, hp; } tone_t;
static int tone_set(tone_t *t, int32_t v, int32_t top, int32_t lo, int32_t base, int32_t hi, int32_t kmin, int half)
{
    int32_t a = v < 0 ? -v : v;
    if (!v) return 0;
    t->k = a >= 21 ? kmin : 5793 - (5793 - kmin) * a / 21;
    t->hp = v > 0;
    tsvf_coef_k(&t->c, (v < 0 ? top - (top - lo) * a / 64 : base + (hi - base) * a / 63) << 8, t->k);
    t->g = t->k >= 4096 ? 32767 : half ? (32767 + t->k * 8) >> 1 : t->k * 8;   /* (1/Q, Q15; half: half way to it) */
    return 1;
}
static inline int32_t tone_run(const tone_t *t, int32_t in, int32_t *ic1, int32_t *ic2)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (t->c.a1 * *ic1 + t->c.a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((t->c.a2 * *ic1 + t->c.a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return mulq15(t->hp ? in - v2 - ((t->k * v1) >> 12) : v2, t->g);
}
static void dly_wow_adv(uint32_t n, int32_t *m0, int32_t *dm)   /* the block's drift: from *m0, *dm a sample */
{
    int32_t m, d = song.g[G_DMOD];                      /* (0.6.86) MOD: 0 none, 20 a tape's (the default), 127 seasick */
    fx.dw_ph += (uint32_t)(0.6 * 4294967296.0 / FS) * n;
    fx.df_ph += (uint32_t)(5.3 * 4294967296.0 / FS) * n;
    m = ((osc_sine(fx.dw_ph) * d) >> 12) + ((osc_sine(fx.df_ph) * d) >> 15);   /* (Q8: WOW +-d / 3.9, FLUTTER +-d / 16 samples) */
    *m0 = fx.dw_m;
    *dm = (m - fx.dw_m) / (int32_t)n;
    fx.dw_m = *m0 + *dm * (int32_t)n;
}
/* the line buf (written at w) d samples back, plus m (Q8): on a sample exact; between two (MOD's drift) a 4-point
 * Hermite (0.6.89: the linear one was a low-pass on every pass, the repeats dulled; as DaisySP's DelayLine ReadHermite,
 * MIT) */
static inline int32_t dly_rd_of(const int16_t *buf, uint32_t w, uint32_t d, int32_t m)
{
    int32_t q = (int32_t)(d << 8) + m, f = q & 255, xm1, x0, x1, x2, c1, c2, c3;
    uint32_t i = w - (uint32_t)(q >> 8);
    x0 = buf[i & (DLY_LEN - 1u)];
    if (!f)
        return x0;
    xm1 = buf[(i + 1u) & (DLY_LEN - 1u)];               /* (newer) */
    x1 = buf[(i - 1u) & (DLY_LEN - 1u)];                /* (older: further back by f) */
    x2 = buf[(i - 2u) & (DLY_LEN - 1u)];
    c1 = (x1 - xm1) >> 1;
    c2 = xm1 - ((5 * x0) >> 1) + 2 * x1 - (x2 >> 1);
    c3 = ((x2 - xm1) >> 1) + ((3 * (x0 - x1)) >> 1);
    return x0 + ((((((c3 * f) >> 8) + c2) * f >> 8) + c1) * f >> 8);
}
static inline int32_t dly_rd(uint32_t d, int32_t m) { return dly_rd_of(dly_buf, fx.dly_w, d, m); }
/* (1.2.1-GW, JIANT 0.8.4) the farthest back a read may sit, in samples short of the line's length: MOD's drift (at 127
 * up to 4.46 samples further, dly_wow_adv), the Hermite's two older taps, one to spare. Past it the read wrapped onto
 * the newest samples (at the longest TIME with MOD on: the input leaked into the wet, -7.5 dB at MOD 127) */
#define DLY_EDGE 8u
static uint32_t delay_samples(void)
{
    uint32_t g = (uint32_t)song.g[G_DTIME];
    uint32_t s = g < DT_FREE ? div_samples(g) : (uint32_t)DTIME_MS[(g - DT_FREE) % 48u] * 441u / 10u;   /* (JIANT 0.4: free) */
    return s < 16u ? 16u : s > DLY_LEN - DLY_EDGE ? DLY_LEN - DLY_EDGE : s;
}

/* (JIANT 0.5) the combs' feedback from SIZE: 0.76 .. 0.99, the top of the knob a long tail (was 0.957 at most) */
static int32_t rev_size(void)
{
    int32_t g = song.g[G_RSIZE];
    return 25000 + g * 40 + g * g / 7;
}
/* ROOM (G_RTYPE 0): 4 damped combs + 2 allpasses (Freeverb-like, mono), added to out (WIDE: rev_room_mod) */
static __attribute__((noinline)) void rev_room(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k;
    int32_t size = rev_size(), damp = 32767 - song.g[G_RDAMP] * 200;
    for (i = 0; i < n; i++) {
        int32_t a = 0;
        int16_t *c = rev_comb;
        int32_t in = mulq15(rev_in[i], 2580);           /* 1/8 at -4 dB: level as before the allpass fix */
        for (k = 0; k < 4u; k++) {
            int32_t o = c[fx.comb_i[k]];
            fx.comb_lp[k] = o + mulq15(fx.comb_lp[k] - o, 32767 - damp);
            c[fx.comb_i[k]] = (int16_t)clamp(in + mulq15(fx.comb_lp[k], size), -32768, 32767);
            if (++fx.comb_i[k] >= REV_COMB[k])
                fx.comb_i[k] = 0;
            a += o;
            c += REV_COMB[k];
        }
        c = rev_ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[fx.ap_i[k]];
            int32_t v = a + (o >> 1);
            c[fx.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
            a = o - a;                                  /* Freeverb: out = buf - in (o - v is a notch comb) */
            if (++fx.ap_i[k] >= REV_AP[k])
                fx.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a;
    }
}

/* (JIANT) ROOM with its networks modulated (G_RMOD > 0): each comb read up to ~5.7 ms nearer (between samples, the
 * depth square law, the offset ramped sample by sample: no steps), by a slow sine of G_RRATE a quarter apart per
 * comb: the resonances drift and detune, the ringing smears into a chorused, wide tail. WIDE wd (0.3): combs 1, 3
 * against 2, 4 as a side (left +, right -) into mix_l / mix_r. Its own loop: with MOD and WIDE 0 the plain one runs
 * (MOD 0 here: offsets 0, the plain one's sound) */
static __attribute__((noinline)) void rev_room_mod(const int32_t *rev_in, int32_t *out, uint32_t n, int32_t wd)
{
    uint32_t i, k;
    int32_t size = rev_size(), damp = 32767 - song.g[G_RDAMP] * 200, m1[4], dm[4];
    int32_t depth = song.g[G_RMOD] ? 64 + song.g[G_RMOD] * song.g[G_RMOD] * 4 : 0;   /* Q8 samples: 0.25 .. 252 (~5.7 ms) */
    fx.rm_ph += LFO_INC[song.g[G_RRATE] & 127];
    for (k = 0; k < 4u; k++) {
        m1[k] = (int32_t)(((uint32_t)(osc_sine(fx.rm_ph + k * 0x40000000u) + 32768) * (uint32_t)depth) >> 16);
        if ((uint32_t)fx.rm_m[k] > 0xFFFFu)             /* (0.6.76) past any depth (252 Q8): a wrong word would read */
            fx.rm_m[k] = m1[k];                         /* far outside the combs; from the new offset */
        dm[k] = (m1[k] - fx.rm_m[k]) / (int32_t)CTL;    /* (Q8, per sample) */
    }
    for (i = 0; i < n; i++) {
        int32_t a = 0, sd = 0;
        int16_t *c = rev_comb;
        int32_t in = mulq15(rev_in[i], 2580);
        for (k = 0; k < 4u; k++) {
            int32_t m = fx.rm_m[k] += dm[k];
            uint32_t L = REV_COMB[k], j0 = fx.comb_i[k] + ((uint32_t)m >> 8), j1;
            int32_t o0, o1, o;
            if (j0 >= L)
                j0 -= L;
            j1 = j0 + 1u >= L ? 0u : j0 + 1u;
            o0 = c[j0];
            o1 = c[j1];
            o = o0 + (((o1 - o0) * (m & 255)) >> 8);    /* (a shorter delay: nearer the write) */
            fx.comb_lp[k] = o + mulq15(fx.comb_lp[k] - o, 32767 - damp);
            c[fx.comb_i[k]] = (int16_t)clamp(in + mulq15(fx.comb_lp[k], size), -32768, 32767);
            if (++fx.comb_i[k] >= L)
                fx.comb_i[k] = 0;
            a += o;
            sd += (k & 1u) ? -o : o;
            c += L;
        }
        if (wd) {
            sd = (sd * wd) >> 7;
            fx_sl[i] += sd;
            fx_sr[i] -= sd;
        }
        c = rev_ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[fx.ap_i[k]];
            int32_t v = a + (o >> 1);
            c[fx.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
            a = o - a;
            if (++fx.ap_i[k] >= REV_AP[k])
                fx.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a;
    }
    for (k = 0; k < 4u; k++)
        fx.rm_m[k] = m1[k];                             /* (exact at the block's end) */
}

/* (0.6.70) SPRING's chain shortened / lengthened (LOAD): the stages coming back start silent, not with what they held */
static __attribute__((noinline)) void sp_chain(int32_t *ap, uint32_t ns)
{
    uint32_t j, k;
    for (j = 0; j < 4u; j++)
        for (k = ns + 1u; k <= SP_N; k++)
            ap[j * (SP_N + 1u) + k] = 0;
    fx.sp_ns = (uint8_t)ns;
}
/* SPRING (see the top), added to out */
static __attribute__((noinline)) void rev_spring(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)song.g[G_RSIZE];
    int32_t g = 19661 + (int32_t)s * 85 + (int32_t)(s * s) / 16;   /* the loop's gain: 0.6 .. 0.96 (0.5: was 0.93) */
    int32_t kl = 26000 - song.g[G_RDAMP] * 160;         /* its low-pass: ~9 kHz .. ~1.3 kHz */
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, L, L2, L3, f, w;
    int16_t *ln = rev_comb;
    int32_t *ap = rev_u.sp;
    uint32_t ns = sys_load == LOAD_ECO ? SP_N_ECO : SP_N;   /* (0.6.70) LOAD ECO: a shorter chain */
    if (!fx.sp_size)
        fx.sp_size = len;
    fx.sp_size += clamp(len - fx.sp_size, -256, 256);   /* SIZE glides (a sample a block at most) */
    L = fx.sp_size >> 8;
    fx.sp_ph += 2u * LFO_INC[song.g[G_RMOD] ? song.g[G_RRATE] & 127 : 24];   /* the wobble: a slow sine, 1.5 samples */
    w = (fx.sp_size >> 1) + ((osc_sine(fx.sp_ph) * (3 + song.g[G_RMOD] / 2)) >> 8);   /* deep (MOD: up to 66); the far end, Q8 */
    L2 = w >> 8;
    f = w & 255;
    L3 = (L * 3) >> 2;
    if (ns != fx.sp_ns)
        sp_chain(ap, ns);
    for (i = 0; i < n; i++) {
        uint32_t wp = fx.sp_w, j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(rev_in[i], 2580), r = ln[(wp - (uint32_t)L) & SP_MASK], p, o;
        int32_t t0 = ln[(wp - (uint32_t)L2) & SP_MASK], t1 = ln[(wp - (uint32_t)L2 - 1u) & SP_MASK];
        fx.sp_lp += mulq15(r - fx.sp_lp, kl);
        o = fx.sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;           /* towards 0: a loop of floors would hold an offset */
        o = x - fx.sp_hp + fx.sp_he;                    /* the low cut, its step's remainder kept (as */
        fx.sp_he = o & 63;                              /* dc_block): no dead band to hold an offset in the loop */
        fx.sp_hp += o >> 6;
        x -= fx.sp_hp;
        p = ap[j];                                      /* the chain: ap[j + k], stage k's output 4 samples ago */
        ap[j] = x;
        for (k = 1; k <= ns; k++) {                     /* (lossless: bounded by the loop's input, no clamp) */
            int32_t v = (x - ap[j + k]) * SP_A;         /* towards 0, as the loop's gain: floors would feed */
            o = ap[j + k];                              /* the loop a little offset and noise for ever */
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)clamp(x, -32768, 32767);
        fx.sp_w = (uint16_t)(wp + 1u);
        out[i] += (t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & SP_MASK] * 2;
    }
}
/* (JIANT 0.3) SPRING's WIDE, after its block: a tap a quarter along the loop against 3/4 as the side (left +,
 * right -; both older than the block, not written in it) */
static __attribute__((noinline)) void rev_spring_side(uint32_t n, int32_t wd)
{
    const int16_t *ln = rev_comb;
    uint32_t i, L = (uint32_t)fx.sp_size >> 8, L3 = (L * 3u) >> 2, wp = (uint32_t)fx.sp_w - n;
    for (i = 0; i < n; i++, wp++) {
        int32_t sd = ((ln[(wp - L3) & SP_MASK] - ln[(wp - (L >> 2)) & SP_MASK]) * 3 * wd) >> 7;
        fx_sl[i] += sd;
        fx_sr[i] -= sd;
    }
}


/* (JIANT 0.5) three more models on the reverb send, no parameter of their own (the REVERB pages' knobs, read per model):
 *   SHIMMER (2)  ROOM with its output an octave up fed back into it (two crossfaded grains reading its own line twice
 *                as fast: a pitch shifter): each pass round the loop an octave higher, a halo of rising partials.
 *                MOD the shimmer's amount; SIZE DAMP PRE as ROOM
 *   RESO (3)     four combs tuned to the song's scale (ROOT and SCALE: its I III V VII from C3), a sympathetic string
 *                each: whatever is sent rings in the key. SIZE their sustain (to ~0.995), DAMP their brightness, MOD the
 *                voicing open (the V and VII an octave up), PRE as ever. In ROOM's comb memory
 *   CLOUD (4)    a granular cloud: the send recorded (half rate, ~0.74 s) and read by up to six grains, each from a
 *                random point of the last SIZE of it, 70 .. 380 ms long, faded in and out, at the pitch of the note or
 *                (MOD: the more, the oftener) an octave up / down or a fifth up; RATE their density, DAMP the tone,
 *                WIDE them left and right. SIZE full: FREEZE (the line stops being written: the cloud holds) */
static uint32_t scale_mask(const track_t *t);           /* seq.c */
#define SH_LEN 2048u
static int16_t shim_buf[SH_LEN] __attribute__((section(".pool")));
static int32_t shim_fb[CTL];
/* (0.6.12) AIR (G_RPRE on SHIMMER and CLOUD, -24..24 st): the pitch moved, the tone with it. A semitone count as a
 * speed, Q16: 2^(st / 12) (-24 .. 36) */
static uint32_t semi_q16(int32_t st)
{
    static const uint16_t R[12] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858};
    int32_t o = (st + 48) / 12 - 4, k = (st + 48) % 12;   /* (an octave down, Q15: then shifted) */
    uint32_t r = (uint32_t)R[k] << 1;                   /* 2^(k / 12), Q16 */
    return o >= 0 ? r << o : r >> -o;
}
static int32_t rev_air(void)
{
    return song.g[G_RTYPE] == 2 || song.g[G_RTYPE] == 4 ? clamp(song.g[G_RPRE], -24, 24) : 0;
}
static int32_t sh_tlp;                                  /* (0.6.12) SHIMMER's tone: AIR's low-pass / its shelf */
static __attribute__((noinline)) void rev_shimmer(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t in[CTL], t[CTL], amt = song.g[G_RMOD] * 190;   /* Q15: up to ~0.74 */
    int32_t air = rev_air(), st = clamp(12 + air, -12, 24), tk = air < 0 ? 32767 + air * 1150 : 9000;
    int32_t step = (int32_t)semi_q16(st) - 65536;       /* (the delay changes by 1 - speed a sample, Q16) */
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        in[i] = rev_in[i] + mulq15(shim_fb[i], amt);
        t[i] = 0;
    }
    rev_room(in, t, n);
    for (i = 0; i < n; i++) {
        int32_t x = 0, y = t[i];
        if (air) {                                      /* AIR: down a low-pass (dark), up a high shelf (air) */
            sh_tlp += mulq15(y - sh_tlp, tk);
            y = air < 0 ? sh_tlp : y + (((y - sh_tlp) >> 3) * air) / 3;
        }
        out[i] += y;
        shim_buf[fx.sh_w & (SH_LEN - 1u)] = (int16_t)clamp(y >> 1, -32768, 32767);
        for (j = 0; j < 2u; j++) {                      /* two grains, the delay sliding: read at octave + AIR's speed */
            uint32_t q = fx.sh_ph + j * ((SH_LEN / 2u) << 16), ph = (q >> 16) & (SH_LEN - 1u), d = SH_LEN - 1u - ph;
            int32_t w = (int32_t)(ph < SH_LEN / 2u ? ph : SH_LEN - 1u - ph);   /* the triangles: their sum flat */
            int32_t f = (int32_t)((q >> 8) & 255u), x0 = shim_buf[(fx.sh_w - d) & (SH_LEN - 1u)];
            int32_t x1 = shim_buf[(fx.sh_w - d + 1u) & (SH_LEN - 1u)];
            x += ((x0 + (((x1 - x0) * f) >> 8)) * w) >> 9;
        }
        fx.sh_ph += (uint32_t)step;
        fx.sh_w++;
        shim_fb[i] = x;                                 /* (into the next block's input) */
    }
}
#define RS_LEN 1024u                                     /* each comb's line, in rev_comb: down to ~43 Hz */
_Static_assert(sizeof rev_comb / 2u >= 4u * RS_LEN, "RESO in ROOM's buffers");
/* (JIANT 0.6.5) RESO's two knobs of its own, in the slots of PRE and DAMP (no pre-delay, a fixed tone on RESO):
 * PITCH (G_RPRE 0..14) the chord that many steps of the scale up; FOLW (G_RDAMP 0..6) OFF / T1..T6: the chord built on
 * the lowest note that track sounds (held after it stops), so it moves with the sequence. The strings glide there */
static uint32_t reso_pitch(void) { return song.g[G_RTYPE] == 3 ? (uint32_t)clamp(song.g[G_RPRE], 0, 14) : 0u; }
static uint32_t reso_folw(void) { return song.g[G_RDAMP] >= 1 && song.g[G_RDAMP] <= 6 ? (uint32_t)song.g[G_RDAMP] : 0u; }
static __attribute__((noinline)) void rev_reso(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    const track_t *t = &trk[0];
    uint32_t i, k, per[4], mask, s, fol = reso_folw(), up = reso_pitch();
    int32_t sz = song.g[G_RSIZE], dk = 32767 - 60 * 200, note[4], deg = 0, nn;
    int32_t fb = sz <= 100 ? 26000 + sz * 52 : clamp(31200 + (sz - 100) * 58, 0, 32740);   /* (0.5.1: the top of SIZE
                                                         * rings on for tens of seconds: 0.9992 at 127, was 0.995) */
    for (k = 0; k < NTRK; k++)                           /* the song's scale: the first melodic track's (they share it) */
        if (ENGINES[eng_idx(trk[k].eng_req)] != &ENG_DRUM) { t = &trk[k]; break; }
    mask = scale_mask(t) & 0xFFFu;
    if (!mask) mask = 0xFFFu;
    nn = 48 + t->p[P_ROOT];
    if (fol) {                                           /* FOLW: the lowest note the track sounds */
        const track_t *ft = &trk[fol - 1u];
        uint32_t lo = 0xFFu;
        for (k = 0; k < NVOICE; k++)
            if (ft->v[k].active && ft->v[k].gate && ft->v[k].note < lo)
                lo = ft->v[k].note;
        if (lo != 0xFFu)
            fx.rs_root = (uint8_t)(lo + 1u);
        if (fx.rs_root) {
            nn = 48 + (int32_t)(fx.rs_root - 1u) % 12;          /* (its pitch class from C3, down onto the scale) */
            for (k = 0; k < 12u && !((mask >> (uint32_t)((nn - t->p[P_ROOT] + 120) % 12)) & 1u); k++)
                nn--;
        }
    }
    for (k = 0; k < up; nn++)                            /* PITCH: that many steps of the scale up */
        if ((mask >> (uint32_t)((nn + 1 - t->p[P_ROOT] + 120) % 12)) & 1u)
            k++;
    for (k = 0, s = 0; k < 4u && s < 48u; s++, nn++)     /* the I, III, V, VII from there up */
        if ((mask >> (uint32_t)((nn - t->p[P_ROOT] + 120) % 12)) & 1u) {
            if (deg % 2 == 0)
                note[k++] = nn;
            deg++;
        }
    for (; k < 4u; k++) note[k] = note[k - 1] + 12;
    if (song.g[G_RMOD] > 64) { note[2] += 12; note[3] += 12; }   /* MOD: the voicing open */
    for (k = 0; k < 4u; k++) {
        uint32_t inc = pitch_inc((uint32_t)clamp(note[k] * 16, 0, 2047));
        per[k] = 0xFFFFFFFFu / ((inc >> 8) | 1u);          /* the period, Q8 */
        if (per[k] > (RS_LEN - 2u) << 8) per[k] = (RS_LEN - 2u) << 8;
        if (!fx.rs_per[k])
            fx.rs_per[k] = per[k];
        fx.rs_per[k] += (uint32_t)(((int32_t)per[k] - (int32_t)fx.rs_per[k]) / 6);   /* (a glide, ~6 blocks) */
        per[k] = fx.rs_per[k];
    }
    for (i = 0; i < n; i++) {
        int32_t in = mulq15(rev_in[i], 3600), a = 0;
        for (k = 0; k < 4u; k++) {
            int16_t *c = rev_comb + k * RS_LEN;
            uint32_t d = per[k] >> 8, f = per[k] & 255u;
            int32_t x0 = c[(fx.rs_w - d) & (RS_LEN - 1u)], x1 = c[(fx.rs_w - d - 1u) & (RS_LEN - 1u)];
            int32_t o = x0 + (((x1 - x0) * (int32_t)f) >> 8);
            fx.rs_lp[k] = o + mulq15(fx.rs_lp[k] - o, 32767 - dk);
            c[fx.rs_w & (RS_LEN - 1u)] = (int16_t)clamp(in + mulq15(fx.rs_lp[k], fb), -32768, 32767);
            a += o;
        }
        fx.rs_w++;
        out[i] += a;
    }
}
#define CL_LEN 16384u                                    /* half rate: ~0.74 s */
#define CL_N 12u
#define CL_WMAX 320                                      /* (1.2.1-GW, JIANT 0.9.14 / 0.9.16) the grains' windows summed:
                                                          * 1.25 grains' worth at most */
#define CL_BACK (CL_LEN - 6000u - 1024u)                 /* .. the farthest back a grain may start (rev_cloud) */
static int16_t cloud_buf[CL_LEN] __attribute__((section(".pool")));
/* (1.2.1-GW, JIANT 0.9.14) CLOUD's int16 line written through a soft knee: linear to -2.5 dBFS (bit for bit as before
 * there), then tanh up to ~-0.1 dB: a hot send rounds off like tape, never a hard clamp in the loop */
static inline int32_t rv_knee(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a <= 24576)
        return x;
    a = 24576 + (softclip(clamp(a - 24576, 0, 16384) * 4) >> 2);
    return x < 0 ? -a : a;
}
static struct { uint32_t pos, inc; uint16_t len, age; int16_t pl, pr; } cl_g[CL_N];
static uint32_t cl_rand(void) { fx.cl_rng = fx.cl_rng * 1664525u + 1013904223u; return fx.cl_rng >> 8; }
/* CLOUD (G_RTYPE 4, JIANT 0.5, made wet in 0.5.1): a granular wash. The input and the cloud's own tail fed back
 * (SIZE: the tail ~0.09, the grains ~0.43: the wash builds on itself) are written at half rate into a 0.74 s line;
 * up to 12 overlapping
 * grains (RATE: their density; 45 .. 140 ms long, triangle windows) read it from anywhere behind the write, MOD of
 * them an octave up, down or a fifth up; each grain panned at random (WIDE: how far, left and right). The grains go
 * through ROOM's network (SIZE its decay, DAMP its tone; MOD chorusing it, WIDE its combs apart): the grains are not
 * heard dry, they bloom into a tail around the sound. SIZE full freezes the line (the cloud holds) */
static __attribute__((noinline)) void rev_cloud(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    static const uint32_t PITCH[4] = {65536u, 131072u, 32768u, 98184u};   /* x1, an octave up, down, a fifth up (Q16) */
    uint32_t i, k, freeze = song.g[G_RSIZE] >= 127 && fx.cl_fill >= 32768u;   /* (1.2.1-GW, JIANT 0.9.16: a FREEZE
                                                         * of an empty line held silence, "CLOUD off at SIZE 100 %"; now
                                                         * the line first takes a line's worth of sound, ~0.74 s) */
    int32_t gap = 1700 - song.g[G_RRATE] * 11, dk = 32767 - song.g[G_RDAMP] * 220, wd = song.g[G_RWIDE], air = rev_air();
    uint32_t ar = semi_q16(air);                         /* (0.6.12) AIR: the grains up / down, the tone with them */
    if (air < 0) dk = clamp(dk + air * 1100, 1500, 32767);
    int32_t fb = 0, fg = song.g[G_RSIZE] * 110, g[CTL], t[CTL];   /* (0.6.14: the tail no more back into the line, was
                                                         * SIZE x24: past SIZE ~80 the loop rang on for ever, any share
                                                         * of it sustained; now it dies out: ~5 s at 110, at 126 a
                                                         * faint wash for long, nearly the FREEZE; Q15: the grains,
                                                         * ~0.43, back into the line) */
    for (i = 0; i < n; i++) {
        int32_t l = 0, r = 0, m;
        if (!freeze) {                                   /* the line: the input and the tail, two samples into one */
            int32_t x = (rev_in[i] >> 1) + mulq15(shim_fb[i], fb) + mulq15(fx.cl_lp, fg);
            fx.cl_dc += ((x << 8) - fx.cl_dc) >> 9;      /* (1.2.1-GW, JIANT 0.9.14) a low cut (~14 Hz, Q8) in the loop: */
            x -= (fx.cl_dc + 128) >> 8;                  /* ROOM's floors gave it a DC that went round for ever */
            if (fx.cl_w & 1u)
                cloud_buf[(fx.cl_w >> 1) & (CL_LEN - 1u)] = (int16_t)rv_knee((fx.cl_half + x) >> 1);
            else
                fx.cl_half = x;
            fx.cl_w++;
            if (fx.cl_fill < 32768u && (fx.cl_fill || x)) fx.cl_fill++;
        }
        if (--fx.cl_next <= 0) {                         /* a grain is born, in the first one free */
            fx.cl_next = gap / 2 + (int32_t)(cl_rand() % (uint32_t)gap);
            uint32_t cap = load_tight ? CL_N / 2u : CL_N;   /* (0.6.16: the load budget: half the grains) */
            for (k = 0; k < cap && cl_g[k].len; k++) {}
            if (k < cap) {
                uint32_t pk = cl_rand() % 100u < (uint32_t)song.g[G_RMOD] * 3u / 4u ? 1u + cl_rand() % 3u : 0u;
                int32_t pan = (int32_t)(cl_rand() % 257u) - 128;   /* -128 .. 128, times WIDE */
                pan = pan * wd >> 7;
                uint32_t inc = (uint32_t)(((uint64_t)(PITCH[pk] >> 1) * ar) >> 16), back;
                cl_g[k].len = (uint16_t)(2000u + cl_rand() % 4200u);
                if (inc > 32768u && (((uint64_t)cl_g[k].len * (inc - 32768u)) >> 16) > CL_BACK)   /* (1.2.1-GW, JIANT */
                    cl_g[k].len = (uint16_t)(((uint64_t)CL_BACK << 16) / (inc - 32768u));   /* 0.9.14) AIR up: shorter,
                                                         * so that it fits behind the write head (past CL_BACK the span
                                                         * below went negative: grains read across the write, a click) */
                back = inc > 32768u ? (uint32_t)(((uint64_t)cl_g[k].len * (inc - 32768u)) >> 16) : 0u;   /* (faster than
                                                         * the line is written: started that much further back) */
                cl_g[k].pos = ((fx.cl_w >> 1) - 200u - back - cl_rand() % (CL_LEN - 6000u - back)) << 16;
                cl_g[k].inc = inc + (cl_rand() % 129u) - 64u;   /* (the line at half rate; each grain
                                                         * a few cents off: a chorused shimmer, not one pitch) */
                cl_g[k].age = 0;
                cl_g[k].pl = (int16_t)(128 - pan);       /* (0 .. 256: 128 the centre) */
                cl_g[k].pr = (int16_t)(128 + pan);
            }
        }
        if (!(i & 1u)) {                                 /* (0.6.12) the grains every other sample, as the line is
                                                         * written (half rate): half the work; in between, the two
                                                         * averaged (a linear step, half a sample late) */
            int32_t gl = 0, gr = 0, ws = 0;
            for (k = 0; k < CL_N; k++) {
                if (!cl_g[k].len) continue;
                {
                    uint32_t p = cl_g[k].pos >> 16, f = (cl_g[k].pos >> 8) & 255u, a = cl_g[k].age, L = cl_g[k].len;
                    int32_t x0 = cloud_buf[p & (CL_LEN - 1u)], x1 = cloud_buf[(p + 1u) & (CL_LEN - 1u)];
                    int32_t x = x0 + (((x1 - x0) * (int32_t)f) >> 8), w = (int32_t)((a < L - a ? a : L - a) * 2u * 256u / L);
                    x = (x * w) >> 8;
                    ws += w;
                    gl += (x * cl_g[k].pl) >> 7;
                    gr += (x * cl_g[k].pr) >> 7;
                    cl_g[k].pos += cl_g[k].inc << 1;
                    cl_g[k].age = (uint16_t)(a + 2u);
                    if (cl_g[k].age >= cl_g[k].len) cl_g[k].len = 0;
                }
            }
            if (ws > CL_WMAX) {                          /* (1.2.1-GW, JIANT 0.9.14 / 0.9.16) the overlap at most */
                int32_t q = (CL_WMAX << 11) / ws;        /* CL_WMAX / 256 grains' worth: the windows' sum grows with */
                gl = (gl * q) >> 11;                     /* RATE (up to 12 grains), and x SIZE's feedback (0.43) it */
                gr = (gr * q) >> 11;                     /* passed 1 from RATE ~90: the line ran away into its clamps, */
            }                                            /* the crackle when pushed. Now it always dies away */
            l = (gl + fx.cl_gl) >> 1;
            r = (gr + fx.cl_gr) >> 1;
            fx.cl_gl = gl;
            fx.cl_gr = gr;
        } else {
            l = fx.cl_gl;
            r = fx.cl_gr;
        }
        m = (l + r) >> 1;
        fx.cl_lp = m + mulq15(fx.cl_lp - m, 32767 - dk);
        g[i] = fx.cl_lp;                                 /* (into the network; 1.2.1-GW: at half the level it was, x2
                                                         * at its output: 6 dB more room before ROOM's int16 lines) */
        if (air > 0)                                     /* (AIR up: the grains' highs lifted) */
            g[i] += ((m - fx.cl_lp) * air) / 24;
        t[i] = 0;
        if (wd) {                                        /* the grains' own place, left and right (a side) */
            int32_t sd = ((l - r) * wd) >> 9;
            fx_sl[i] += sd;
            fx_sr[i] -= sd;
        }
    }
    if (song.g[G_RMOD] || wd)
        rev_room_mod(g, t, n, wd * 2);                   /* (its side x2 as well: the network at half the level) */
    else
        rev_room(g, t, n);
    for (i = 0; i < n; i++) {
        out[i] += t[i] * 3 + (g[i] >> 1);                /* the bloom (+3.5 dB, 0.5.1), the grains themselves under it */
        shim_fb[i] = t[i];                               /* (the tail into the line, next block) */
    }
}
/* the model ty's block into out */
static void rev_run(uint32_t ty, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    switch (ty) {
    case 1: rev_spring(rev_in, out, n); if (song.g[G_RWIDE]) rev_spring_side(n, song.g[G_RWIDE]); break;
    case 2: rev_shimmer(rev_in, out, n); break;
    case 3: rev_reso(rev_in, out, n); break;
    case 4: rev_cloud(rev_in, out, n); break;
    default:
        if (song.g[G_RMOD] || song.g[G_RWIDE]) rev_room_mod(rev_in, out, n, song.g[G_RWIDE]);
        else rev_room(rev_in, out, n);
        break;
    }
}

/* the reverb's buffers and states to silence (the model changed) */
static void rev_clear(void)
{
    uint32_t i;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        rev_comb[i] = 0;
    for (i = 0; i < sizeof rev_u.ap / 2u; i++)          /* (int16: 997 of them, an odd count the int32 view misses one of) */
        rev_u.ap[i] = 0;
    for (i = 0; i < 4u; i++)
        fx.comb_lp[i] = fx.rs_lp[i] = 0;
    fx.sp_lp = fx.sp_hp = fx.sp_he = 0;
    for (i = 0; i < SH_LEN; i++)                        /* (JIANT 0.5) SHIMMER's line, CLOUD's grains */
        shim_buf[i] = 0;
    for (i = 0; i < CTL; i++)
        shim_fb[i] = 0;
    sh_tlp = 0;
    for (i = 0; i < CL_N; i++)
        cl_g[i].len = 0;
    fx.cl_gl = fx.cl_gr = 0;
    fx.cl_dc = 0;
    fx.cl_fill = 0;
}

static int32_t part_buf[CTL];                            /* a part's block (mix_part); the fade of a model change */
static int32_t c1_buf[CTL];                              /* (0.6.71) the second core's part_buf (mix_part) */

/* (0.5.1) the chorus ensemble: three taps of buf (written at w), the LFO phase ph a third apart each, base delays
 * 300 / 420 / 540 samples plus the sweep (depth Q8); returns the middle (Q15 x2 at unity), *sd the left - right */
static inline int32_t cho_tap(const int16_t *buf, uint32_t w, int32_t r)
{
    uint32_t ri = (uint32_t)r >> 8;
    int32_t f = r & 255, c0 = buf[(w - ri) & (CHO_LEN - 1u)], c1 = buf[(w - ri - 1u) & (CHO_LEN - 1u)];
    return c0 + (((c1 - c0) * f) >> 8);
}
static inline int32_t cho_ens(const int16_t *buf, uint32_t w, uint32_t ph, int32_t depth, int32_t *sd)
{
    int32_t a = cho_tap(buf, w, (300 << 8) + ((osc_sine(ph) + 32768) * depth >> 8));
    int32_t b = cho_tap(buf, w, (420 << 8) + ((osc_sine(ph + 0x55555555u) + 32768) * depth >> 8));
    int32_t c = cho_tap(buf, w, (540 << 8) + ((osc_sine(ph + 0xAAAAAAAAu) + 32768) * depth >> 8));
    *sd = a - b;
    return ((a + b + c) * 21845) >> 15;                 /* (x2 / 3 each: the sum at x2, as the one tap was) */
}
/* (QG-1) CHORUS > TYPE PHASER: four first-order all-passes (H = (a + z^-1) / (1 + a z^-1)) swept together by the chorus's
 * LFO, their notches ~200 Hz .. ~3 kHz at DEPTH 100 %, a little feedback; out x + the chain (the notches inside the bus,
 * the same x2 as the ensemble), *sd stage 2 - stage 4 (the sides moving apart). No line, no taps: lighter than it */
static inline int32_t cho_phaser(int32_t x, uint32_t ph, int32_t depth, int32_t *sd)
{
    int32_t a = 900 + (int32_t)(((uint32_t)(osc_sine(ph) + 32768) >> 1) * (uint32_t)depth >> 15) - 32767, y2 = 0, k;
    int32_t v = x + ((fx.ph_y * 18000) >> 15);
    for (k = 0; k < 4; k++) {
        int32_t s = fx.ph_s[k], w = clamp(v - ((a * s) >> 15), -(1 << 20), 1 << 20);
        v = ((a * w) >> 15) + s;
        fx.ph_s[k] = w;
        if (k == 1) y2 = v;
    }
    fx.ph_y = v;
    *sd = y2 - v;
    return x + v;
}

/* process the three buses for one block; sends in, wet stereo-equal out */
/* (0.6.14) the buses' gate: their feedback (the delay's low-pass, ROOM's combs, CLOUD's grains) rounds to a small
 * signal that never reaches 0 (a limit cycle, ~-55 dB, the CLIP's drive lifting it into a hiss with nothing playing).
 * Nothing sent to them for 1.5 s and their output that small: faded to 0 in ~25 ms; anything sent opens it at once (the
 * residue is all it hides). CLOUD's FREEZE (SIZE 127) is meant to hold: never gated */
#define FXG_QUIET ((uint32_t)FS * 3u / 2u / CTL)
#define FXG_FLOOR 64
#define FXG_QUIET2 ((uint32_t)FS * 4u / CTL)         /* (and after 4 s, a residue up to ~-36 dB: RESO's strings, a
                                                       * pitched delay's; after 8 s, ~-30 dB: CLOUD's grains) */
#define FXG_FLOOR2 512
#define FXG_QUIET3 ((uint32_t)FS * 8u / CTL)
#define FXG_FLOOR3 1024
static uint32_t fxg_quiet;
static int32_t fxg_dpk;                              /* (0.6.89) the delay line's peak this block (fx_buses) */
static int32_t fxg_g = 32767;
static void fx_gate(int32_t *wet, uint32_t n)
{
    int32_t pk = 0, g0 = fxg_g, d;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = wet[i] < 0 ? -wet[i] : wet[i];
        if (a > pk) pk = a;
    }
    int dly = fxg_dpk >= FXG_FLOOR && !fx.gr_on;       /* (0.6.89) a plain delay's tail still there: only the first floor
                                                         * (its residue: the loop's DC block keeps it under it); the
                                                         * higher ones cut a long FEEDBK tail off at 4 or 8 s */
    fxg_dpk = 0;
    if (((fxg_quiet > FXG_QUIET && pk < FXG_FLOOR) || (!dly && fxg_quiet > FXG_QUIET2 && pk < FXG_FLOOR2) ||
         (!dly && fxg_quiet > FXG_QUIET3 && pk < FXG_FLOOR3)) &&
        !(song.g[G_RTYPE] == 4 && song.g[G_RSIZE] >= 127))
        fxg_g = g0 > 1024 ? g0 - 1024 : 0;
    else if (g0 < 32767 && (g0 > 0 || fxg_quiet == 0u || dly))   /* (0.6.89: or the delay growing by itself) */   /* (0.6.19) closing and the residue swells again (a pitched
                                                         * delay's feedback): back up as it went down, ~25 ms (it
                                                         * snapped open: a click); */
        fxg_g = g0 < 32767 - (fxg_quiet ? 1024 : 4096) ? g0 + (fxg_quiet ? 1024 : 4096) : 32767;
    else if (g0 == 0 && !(song.g[G_RTYPE] == 4 && song.g[G_RSIZE] >= 127))   /* .. closed: it stays closed until
                                                         * something is sent (CLOUD's FREEZE: open) */
        fxg_g = 0;                                      /* (something sent: the branch above opens it) */
    else
        fxg_g = 32767;
    if (g0 == 32767 && fxg_g == 32767)
        return;
    d = (fxg_g - g0) / (int32_t)n;
    for (i = 0; i < n; i++, g0 += d)
        wet[i] = mulq15(wet[i], g0);
}
/* (0.6.81) a value's step a sample from cur toward to: a quarter of the way over the block, one at least (never past) */
static int32_t fx_glide(int32_t cur, int32_t to, uint32_t n)
{
    int32_t d = to - cur, s = (d >> 2) / (int32_t)n;
    if (!s && d)
        s = d > 0 ? 1 : -1;
    if (s * (int32_t)n > d && d > 0) s = d / (int32_t)n;
    if (s * (int32_t)n < d && d < 0) s = d / (int32_t)n;
    return s;
}
static inline __attribute__((always_inline)) void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet,
                     uint32_t n)
{
    uint32_t i, dl = delay_samples();
    int32_t dcd, dcw, ddm;
    {   /* (0.6.14) anything sent to the buses this block: their gate open (fx_gate) */
        int32_t any = 0;
        for (i = 0; i < n; i++)
            any |= cho_in[i] | dly_in[i] | rev_in[i];
        fxg_quiet = any ? 0u : fxg_quiet < 0xFFFFFFu ? fxg_quiet + 1u : fxg_quiet;
    }                                                   /* (0.6.19: it opens over ~6 ms, fx_gate: at once, the
                                                         * residue in the lines stepped in: a tick on the next note) */
    int32_t fb = dly_fb(song.g[G_DFDBK]), tn = song.g[G_DCOLOR] - 64;   /* (JIANT 0.4) TONE: one knob, low-pass .. open .. low cut */
    int32_t wm, dwm, dig = song.g[G_DTYPE] == 1;        /* (0.6.89: TONE in the middle: each repeat the very copy) */
    tone_t dtn, rtn;
    int dton = dig ? tone_set(&dtn, tn > 63 ? 63 : tn, 113, 43, 18, 93, 5793, 0)   /* (0.6.90) DIGITAL: TONE plain (Q 0.7) */
                   : tone_set(&dtn, tn > 63 ? 63 : tn, 113, 43, 18, 93, 2900, 1);   /* (0.6.89 half the level evened: the band
                                                         * at the cutoff above unity, the extremes self-oscillate) */
    if (!dton) fx.dt1 = fx.dt2 = 0;   /* (8 kHz .. 250 Hz | 73 Hz .. 3 kHz, Q 1.4) */
    int32_t dmix = song.g[G_DMIX] * 258;
    int32_t cdepth = song.g[G_CDEPTH] * 6, rt;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    int32_t hk = 5, wd = song.g[G_WIDTH];             /* (the loop's DC block: ~1 Hz, 0.6.89); WIDTH */
    int32_t gr = !sys_load && (song.g[G_DPIT] || song.g[G_DSPRY]);   /* (JIANT) the GRAIN delay (LOAD LITE / ECO: plain) */
    /* (0.6.90) PING-PONG (not with the GRAIN): the line read at TIME (left) and twice TIME (right, the loop's tap), the
     * loop's gain squared, the right x FEEDBK: the repeats left, right, left .., each FEEDBK softer; TIME at most half
     * the line (0.74 s); WIDE out of it */
    int32_t png = song.g[G_DPING] == 1 && !gr, fb2 = (int32_t)(((int64_t)fb * fb) >> 15);
    if (dig && fb > 32767) fb = 32767;                  /* (0.6.90) DIGITAL: 100 % at most, a repeat held for ever */
    if (dig && fb2 > 32767) fb2 = 32767;
    if (png && dl > (DLY_LEN - DLY_EDGE) / 2u) dl = (DLY_LEN - DLY_EDGE) / 2u;   /* (1.2.1-GW: DLY_EDGE, was 4) */
    int32_t cho = sys_load != LOAD_ECO;                /* (0.6.30) LOAD ECO: no chorus */
    int32_t phz = song.g[G_CTYPE] == 1, pdep = song.g[G_CDEPTH] * 96;   /* (QG-1) the PHASER, its sweep */
    int32_t cs = song.g[G_CSTER], cw = 80 + wd / 2;    /* (0.6.9) the chorus's side: 0 as before, - to mono, + ~2x */
    cw = cs >= 0 ? cw + cs * 2 : (cw * (64 + cs)) >> 6;
    uint32_t dr = dl + (wd && !png ? div_samples(3) * (uint32_t)wd / 127u : 0u);   /* the right echo: up to 1/32 later */
    if (dr > DLY_LEN - DLY_EDGE)
        dr = DLY_LEN - DLY_EDGE;                       /* (1.2.1-GW: inside the line, MOD's drift too) */
    /* (0.6.81) a new read point (TIME, WIDE) or read (the GRAIN in or out): faded into over DXF_LEN samples from the
     * old one, the next change once that fade is done (each step of the knob jumped the read: a click) */
    if (!fx.primed) {
        fx.primed = 1;
        fx.dl_now = dl; fx.dr_now = dr; fx.gr_on = (uint8_t)gr;
        fx.cd_cur = cdepth; fx.cw_cur = cw; fx.dm_cur = dmix;
    }
    if (!fx.dl_xf && (dl != fx.dl_now || (uint32_t)gr != fx.gr_on)) {
        fx.dl_old = fx.dl_now; fx.gr_old = fx.gr_on;
        fx.dl_now = dl; fx.gr_on = (uint8_t)gr;
        fx.dl_xf = DXF_LEN;
    }
    if (!fx.dr_xf && dr != fx.dr_now) {
        fx.dr_old = fx.dr_now;
        fx.dr_now = dr;
        fx.dr_xf = DXF_LEN;
    }
    /* .. and the chorus's DEPTH, its width and the delay's MIX glide across the block (a quarter of the way) */
    dcd = fx_glide(fx.cd_cur, cdepth, n);
    dcw = fx_glide(fx.cw_cur, cw, n);
    ddm = fx_glide(fx.dm_cur, dmix, n);
    dly_wow_adv(n, &wm, &dwm);
    if (tone_set(&rtn, song.g[G_RFILT], 121, 50, 14, 79, 1600, 1)) {   /* (JIANT) the reverb's tone: FILT < 0 a low-pass (~12 kHz ..
                                                         * ~350 Hz), > 0 a low cut (~60 Hz .. ~1.5 kHz); 0.6.86 resonant (Q 2.5) */
        for (i = 0; i < n; i++)
            rpre_out[i] = tone_run(&rtn, rev_in[i], &fx.rt1, &fx.rt2);
        rev_in = rpre_out;
    } else
        fx.rt1 = fx.rt2 = 0;
    if (song.g[G_RPRE] > 0 && song.g[G_RTYPE] <= 1) {   /* (JIANT) the reverb's pre-delay (in place; RESO: PITCH;
                                                         * 0.6.12: SHIMMER, CLOUD: AIR) */
        uint32_t pd = (uint32_t)song.g[G_RPRE] * 20u;   /* (ms -> half-rate samples: 2000 at 100) */
        for (i = 0; i < n; i++) {                       /* half rate: two samples averaged in, read back between */
            uint32_t h = fx.rpre_w >> 1;
            int32_t a = rpre_buf[(h - pd) & (RPRE_LEN - 1u)], b = rpre_buf[(h - pd + 1u) & (RPRE_LEN - 1u)];
            if (fx.rpre_w & 1u)
                rpre_buf[h & (RPRE_LEN - 1u)] = (int16_t)clamp((fx.rpre_s + (rev_in[i] >> 3)) >> 1, -32768, 32767);
            else
                fx.rpre_s = rev_in[i] >> 3;
            rpre_out[i] = ((fx.rpre_w & 1u) ? (a + b) >> 1 : a) * 8;
            fx.rpre_w++;
        }
        rev_in = rpre_out;
    }
    for (i = 0; i < n; i++) {
        int32_t y = 0, x, side = 0, xr = 0, wr = 0;
        /* chorus (0.5.1: an ensemble): three taps of the line, ~7, 9.5 and 12 ms, each swept by the LFO a third of a
         * turn apart; the first left, the second right, the third in the middle: always stereo (WIDTH wider) */
        cho_buf[fx.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        fx.cho_ph += cinc;
        fx.cd_cur += dcd; fx.cw_cur += dcw; fx.dm_cur += ddm;
        if (cho) {
            int32_t sd;
            y += phz ? cho_phaser(cho_buf[fx.cho_w & (CHO_LEN - 1u)], fx.cho_ph, pdep, &sd)
                     : cho_ens(cho_buf, fx.cho_w, fx.cho_ph, fx.cd_cur, &sd);
            sd = (sd * fx.cw_cur) >> 7;
            side += sd;
        }
        fx.cho_w++;
        /* delay with a low-passed (COLR) and low-cut (HPF) feedback */
        if (fx.gr_on || (fx.dl_xf && fx.gr_old))
            dly_grain_adv();
        wm += dwm;
        x = fx.gr_on ? dly_grain(fx.dl_now) : dly_rd(fx.dl_now, wm);
        if (fx.dl_xf) {                                 /* (0.6.81) the old read point fading out */
            int32_t xo = fx.gr_old ? dly_grain(fx.dl_old) : dly_rd(fx.dl_old, wm);
            x += ((xo - x) * (int32_t)fx.dl_xf) >> DXF_LOG2;
            fx.dl_xf--;
        }
        if (png) {                                      /* (0.6.90) PING-PONG: the right, twice as far back */
            xr = dly_rd(fx.dl_now * 2u, wm);
            if (fx.dl_xf) {
                int32_t xo = dly_rd(fx.dl_old * 2u, wm);
                xr += ((xo - xr) * (int32_t)fx.dl_xf) >> DXF_LOG2;
            }
            wr = 2;
        } else if (wd || fx.dr_xf || fx.dr_now != fx.dl_now) {   /* (JIANT) the right echo later (0.6.81: on until it has
                                                         * faded back onto the left's, WIDE to 0 stepped; read before
                                                         * the line is written, as the left: at WIDE 0, the same sample) */
            xr = dly_rd(fx.dr_now, wm);
            if (fx.dr_xf) {
                int32_t xo = dly_rd(fx.dr_old, wm);
                xr += ((xo - xr) * (int32_t)fx.dr_xf) >> DXF_LOG2;
                fx.dr_xf--;
            }
            wr = 1;
        }
        fx.dly_lp = x;                                  /* (0.6.89: no fixed low-pass in the loop; the DC block's
                                                         * state 8 bits finer below: rounded coarse, it held a DC of up
                                                         * to 364 in the loop) */
        { int32_t ax = x < 0 ? -x : x; if (ax > fxg_dpk) fxg_dpk = ax; }   /* (the delay's own level: fx_gate) */
        if (wr == 2) {                                  /* (PING-PONG: the loop from the right tap; its level, too) */
            int32_t ax = xr < 0 ? -xr : xr; if (ax > fxg_dpk) fxg_dpk = ax;
            fx.dly_hp += (((xr << 8) - fx.dly_hp) * hk + 16384) >> 15;
        } else
            fx.dly_hp += (((x << 8) - fx.dly_hp) * hk + 16384) >> 15;
        {
            int32_t lf = (wr == 2 ? xr : x) - ((fx.dly_hp + 128) >> 8), w;
            if (dton) lf = tone_run(&dtn, lf, &fx.dt1, &fx.dt2);
            w = (dly_in[i] >> 1) + mulq15(clamp(lf, -32767, 32767), wr == 2 ? fb2 : fb);
            dly_buf[fx.dly_w & (DLY_LEN - 1u)] = (int16_t)(dig ? clamp(w, -32767, 32767) : dly_sat(w));   /* (DIGITAL: clean, a clip
                                                         * at full scale only) */
        }
        if (wr == 2)
            xr = mulq15(clamp(xr, -65535, 65535), fb > 32767 ? 32767 : fb);   /* (the right: one repeat later, FEEDBK softer) */
        if (wr) {                                       /* mid and side */
            side += mulq15(x - xr, fx.dm_cur);
            x = (x + xr) >> 1;
        }
        fx.dly_w++;
        y += mulq15(x << 1, fx.dm_cur);
        wet[i] = y;
        if (side) {                                     /* (the side: left +, right -) */
            if (fxg_g < 32767)                          /* (0.6.14: gated as the rest, fx_gate) */
                side = mulq15(side, fxg_g);
            fx_sl[i] += side;
            fx_sr[i] -= side;
        }
    }
    rt = (uint32_t)song.g[G_RTYPE] <= 4u ? song.g[G_RTYPE] : 0;
    rt = REV_LITE[sys_load % 3u][rt];                   /* (0.6.29) LOAD LITE: SHIMMER, CLOUD as ROOM, RESO as SPRING; ECO:
                                                         * all SPRING */
    if (rt != fx.rtype) {                               /* the model changed: the old one's block fades out, */
        int32_t *t = fx_tmp, g = 65536, d = 65536 / (int32_t)n;   /* its buffers are cleared, the new */
        for (i = 0; i < n; i++)                                     /* one starts from silence */
            t[i] = 0;
        rev_run(fx.rtype, rev_in, t, n);
        for (i = 0; i < n; i++, g -= d)
            wet[i] += mulq16(t[i], (uint32_t)g);
        rev_clear();
        fx.rtype = (uint8_t)rt;
        fx_gate(wet, n);
        return;
    }
    rev_run(rt, rev_in, wet, n);
    fx_gate(wet, n);
}

/* one block of the whole mix (shared with hostsim.c): events -> each part (with its modulation matrix)
 * -> dist -> level / pan / sends -> buses -> (the SLICER's bus) -> master; out: stereo Q15 */
static void events_block(uint32_t n);                    /* seq.c */
static int32_t send_c[CTL], send_d[CTL], send_r[CTL];   /* (mix_l mix_r wet: above, clip_block) */
/* (JIANT 0.6.71) what the second core's parts add, summed into the above after it is done (mix_block) */
static struct { int32_t l[CTL], r[CTL], c[CTL], d[CTL], rv[CTL], sl[CTL], sr[CTL]; } c1_mix;
/* a part's level (Q12) over the block from l0 to l1, ducked: its start << 8, its step a sample in *dl (a block is
 * CTL: no divide) */
static __attribute__((noinline)) int32_t duck_ramp(int32_t l0, int32_t l1, int32_t *dl)
{
    int32_t a = ((l0 * (duck_g0 >> 3)) >> 12) << 8;
    *dl = ((((l1 * (duck_g1 >> 3)) >> 12) << 8) - a) >> CTL_LOG2;
    return a;
}
/* (JIANT 0.6.8, the crackles turning LEVEL / PAN) each part's LEVEL (Q12 << 8) and PAN (-64..63 << 8) as the mix has
 * them: a quarter of the way to the knob's a block (~3 ms), ramped across it, never a step */
static int32_t mp_lvl[NTRK], mp_pan[NTRK];
static uint8_t mp_seen[NTRK];                             /* the part's first block: at once (0.6.71: a byte a part,
                                                          * two cores write two parts at once) */
/* (JIANT 0.6.9) AUTO PAN: the part's PAN swung by an LFO of its own, DPTH up to the whole field either way, its RATE
 * FREE (the LFO's rates) or on the tempo (BEAT: 4 bars .. 1/32, in time with the bar while playing, as X-MOD), SIN TRI
 * SAW SQR or S&H; through mp_pan's glide (no step even on SQR / S&H). Per part: the phase and the S&H value */
static uint32_t ap_ph[NTRK], ap_free[NTRK];
static int16_t ap_sh[NTRK];
static uint32_t ap_rng = 0x2545F491u;
static const uint8_t AP_SIXT[8] = {128, 64, 32, 16, 8, 4, 2, 1};   /* a cycle in 1/32 notes: 4 BARS .. 1/32 (N_APDIV) */
static __attribute__((noinline)) int32_t autopan(const track_t *t, uint32_t ti, uint32_t n)   /* the offset, pan << 8 */
{
    const int16_t *p = t->p;
    uint32_t ph, old = ap_ph[ti], w = (uint32_t)clamp(p[P_APWAVE], 0, 4);
    int32_t v;
    if (p[P_APSYNC]) {                                    /* BEAT: from the transport's grid (free while stopped) */
        uint32_t len = beat_samples() * AP_SIXT[clamp(p[P_APRATE], 0, 7)] / 8u, pos;
        if (len < 64u)
            len = 64u;
        ap_free[ti] += n;
        pos = song.playing ? ms_clock : ap_free[ti];
        ph = (pos % len) * (0xFFFFFFFFu / len);
    } else {
        ph = old + LFO_INC[p[P_APRATE] & 127] * (n / CTL);   /* (the LFO's rates: once a block, as track_lfo_tick) */
    }
    ap_ph[ti] = ph;
    switch (w) {
    case 1: v = ph < 0x80000000u ? (int32_t)(ph >> 15) - 32768 : 98303 - (int32_t)(ph >> 15); break;   /* TRI */
    case 2: v = (int32_t)(ph >> 16) - 32768; break;                                                   /* SAW */
    case 3: v = ph < 0x80000000u ? 32767 : -32767; break;                                             /* SQR */
    case 4:                                                                                             /* S&H */
        if (ph < old) {                                   /* (a new cycle: a new place) */
            ap_rng = ap_rng * 1664525u + 1013904223u;
            ap_sh[ti] = (int16_t)((int32_t)(ap_rng >> 16) - 32768);
        }
        v = ap_sh[ti];
        break;
    default: v = osc_sine(ph); break;                                                                  /* SIN */
    }
    return ((v >> 7) * clamp(p[P_APDEP], 0, 127)) >> 1;   /* (+-256 x 127 / 2: +-64 << 8 at full depth) */
}

/* (JIANT 0.6.48) a DRUM part's IMAGE and SENDs (eng_drum.c dx_sl dx_sr dx_sb), after mix_part's loop put its sum in: what
 * each side gives up (at the part's LEVEL ramp lq and PAN gains gl gr, as that loop), each bus fed by its sounds; pm0 ..
 * pm1 the FX layer's mute over the block (perform.c perf_mute: 32768 open) */
static __attribute__((noinline)) void drum_mix(int32_t *ol, int32_t *or_, uint32_t n, int32_t lq, int32_t dl, int32_t gl,
                                               int32_t dgl, int32_t gr, int32_t dgr, int32_t pm0, int32_t pm1,
                                               int32_t *const sd[3])   /* (0.6.71: this core's sends) */
{
    uint32_t i, b, on = dx_xon;
    int32_t *sl = dx_sl, *sr = dx_sr, (*sb)[CTL] = dx_sb;   /* (1.2.1-GW, JIANT 0.7.8: this core's) */
    int32_t dpm = (pm1 - pm0) / (int32_t)(n ? n : 1u), mute = pm0 != 32768 || pm1 != 32768;
    if (n > CTL)
        n = CTL;
    if (mute)                                           /* (the FX layer's mute: on what is taken and sent too) */
        for (i = 0; i < n; i++) {
            uint32_t g = (uint32_t)(pm0 + dpm * (int32_t)i) << 1;
            sl[i] = mulq16(sl[i], g);
            sr[i] = mulq16(sr[i], g);
            sb[0][i] = mulq16(sb[0][i], g);
            sb[1][i] = mulq16(sb[1][i], g);
            sb[2][i] = mulq16(sb[2][i], g);
        }
    if (on & 1u) {                                      /* what each side gives up, at the part's LEVEL and PAN */
        int32_t q = lq, l = gl, r = gr;
        for (i = 0; i < n; i++, q += dl, l += dgl, r += dgr) {
            ol[i] -= ((((sl[i] >> 2) * (q >> 8)) >> 10) * l) >> 12;
            or_[i] -= ((((sr[i] >> 2) * (q >> 8)) >> 10) * r) >> 12;
        }
    }
    for (b = 0; b < 3u; b++)                            /* each bus its sounds' */
        if ((on >> (b + 1u)) & 1u) {
            const int32_t *x = sb[b];
            int32_t *d = sd[b], q = lq;
            for (i = 0; i < n; i++, q += dl)
                d[i] += ((x[i] >> 2) * (q >> 8)) >> 10;
        }
}

/* one synth part into the dry mix and the sends; a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static inline __attribute__((always_inline)) void mix_part(track_t *t, uint32_t n)   /* (0.6.78: inlined in both
                                                         * copies of mix_part_m, flash and RAM) */
{
    uint32_t i, c1 = MC_CPU() & 1u;                     /* (0.6.71) on the second core: its own buffers */
    int32_t *b = c1 ? c1_buf : part_buf;
    if ((uint32_t)(t - trk) >= trk_live) {              /* (0.6.18) TRACKS 4: asleep, nothing rendered. (0.6.19) Its
                                                         * voices first fade out over a block (voice_kill: they were
                                                         * cut at once, a click), its mute let go (perform.c) */
        uint32_t k, on = 0;
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active) {
                if (t->v[k].stage != 4u)
                    voice_kill(&t->v[k]);
                on = 1;
            }
        pf.mg[(uint32_t)(t - trk) % NTRK] = 32768;
        if (!on) {
            mp_seen[(uint32_t)(t - trk) % NTRK] = 0;
            return;
        }
    }
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    if (track_render(t, b, n))
        t->tail = 16;                                   /* blocks of DIST state to run out after the last voice */
    else if (!t->tail || !t->p[P_DIST] || !--t->tail) {
        mp_seen[(uint32_t)(t - trk) % NTRK] = 0;       /* (silent: the next sound at its LEVEL and PAN) */
        if (MOD.on)
            mod_end(t);
        return;
    }
    {
        uint32_t ti = (uint32_t)(t - trk) % NTRK;
        int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127] << 8, pan = t->p[P_PAN] << 8, l0, p0, lq, dl, gl, gr, dgl, dgr;
        if (t->p[P_APDEP] > 0)                          /* (0.6.9) AUTO PAN */
            pan = clamp(pan + autopan(t, ti, n), -(64 << 8), 63 << 8);
        if (!mp_seen[ti]) {
            mp_lvl[ti] = lvl;
            mp_pan[ti] = pan;
            mp_seen[ti] = 1;
        }
        l0 = mp_lvl[ti];
        p0 = mp_pan[ti];
        mp_lvl[ti] += (lvl - l0) >> 2;
        mp_pan[ti] += (pan - p0) >> 2;
        if (mp_lvl[ti] - lvl < 256 && lvl - mp_lvl[ti] < 256) mp_lvl[ti] = lvl;   /* (there: exactly) */
        if (mp_pan[ti] - pan < 64 && pan - mp_pan[ti] < 64) mp_pan[ti] = pan;
        lq = l0;
        dl = (mp_lvl[ti] - l0) >> CTL_LOG2;
        gl = 4096 - (p0 > 0 ? p0 >> 2 : 0);                 /* (pan << 8: x64 is >> 2) */
        gr = 4096 + (p0 < 0 ? p0 >> 2 : 0);
        dgl = ((4096 - (mp_pan[ti] > 0 ? mp_pan[ti] >> 2 : 0)) - gl) >> CTL_LOG2;
        dgr = ((4096 + (mp_pan[ti] < 0 ? mp_pan[ti] >> 2 : 0)) - gr) >> CTL_LOG2;
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        int32_t xmax = c > d ? c : d, *ol = c1 ? c1_mix.l : mix_l, *or_ = c1 ? c1_mix.r : mix_r;
        int32_t *sc = c1 ? c1_mix.c : send_c, *sd = c1 ? c1_mix.d : send_d, *sr = c1 ? c1_mix.rv : send_r;
        if (slicer_routes((uint32_t)(t - trk))) {       /* (JIANT 0.6.3) into the SLICER's bus, not the dry mix */
            ol = c1 ? c1_mix.sl : sl_l;
            or_ = c1 ? c1_mix.sr : sl_r;
        }
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        if ((duck_g0 < 32767 || duck_g1 < 32767) && ENGINES[t->engine] != &ENG_DRUM)   /* DUCK: the kick's */
            lq = duck_ramp(l0 >> 8, mp_lvl[ti] >> 8, &dl);
        track_dist(t, b, n);
        int32_t pm0 = 32768, pm1 = 32768;               /* (0.6.48: drum_mix's; pf.mg counts only while muting) */
        if ((pf.mute >> (t - trk)) & 1u) {
            pm0 = pf.mg[ti];
            perf_mute((uint32_t)(t - trk), b, n);       /* perform.c: a black key in the FX layer */
            pm1 = pf.mg[ti];
        }
        if (dx_xon && ENGINES[t->engine] == &ENG_DRUM)  /* (0.6.48) IMAGE, SENDs: from the same ramps as below */
        {
            int32_t *const sx[3] = {sc, sd, sr};
            drum_mix(ol, or_, n, lq, dl, gl, dgl, gr, dgr, pm0, pm1, sx);
        }
        for (i = 0; i < n; i++, lq += dl, gl += dgl, gr += dgr) {
            int32_t x = ((b[i] >> 2) * (lq >> 8)) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                sc[i] += mulq15(xs, c);
            if (d)
                sd[i] += mulq15(xs, d);
            if (r)
                sr[i] += mulq15(xs, r);
            ol[i] += (x * gl) >> 12;
            or_[i] += (x * gr) >> 12;
        }
        t->peak = pk;
    }
    if (MOD.on)
        mod_end(t);                                     /* the stored values back */
}

/* (JIANT 0.5.1) DRUM-X X-MOD's LFO, a block: its cycle RATE's bars or notes of the tempo, on the transport's grid while
 * it plays (ms_clock: in time with the bar), free while stopped; dx_mot_v (eng_drum.c) the MORPH offset, +-DPTH/2 */
static uint32_t dx_mot_free, dx_mot_cyc;
static int32_t dx_mot_sh;
static __attribute__((noinline)) void dx_mot_block(uint32_t n)
{
    static const uint8_t Q8[9] = {0, 64, 32, 16, 8, 4, 2, 1, 1};   /* the cycle in 1/16 notes x 4 .. (1/32: half) */
    uint32_t r = dx_mot[XM_RATE], len, pos, ph, cyc;
    int32_t w;
    dx_mot_free += n;
    if (!r || r > 8u || !dx_mot[XM_DPTH]) {
        dx_mot_v = 0;
        return;
    }
    len = beat_samples() * Q8[r] / 4u;                  /* (a quarter = 4 sixteenths) */
    if (r == 8u)
        len = beat_samples() / 8u;
    if (len < 64u)
        len = 64u;
    pos = song.playing ? ms_clock : dx_mot_free;
    cyc = pos / len;
    ph = (uint32_t)(((uint64_t)(pos % len) << 32) / len);
    switch (dx_mot[XM_SHPE]) {
    case 1: w = ph < 0x80000000u ? (int32_t)(ph >> 15) - 32768 : 98303 - (int32_t)(ph >> 15); break;   /* TRI */
    case 2: w = (int32_t)(ph >> 16) - 32768; break;                                                   /* SAW */
    case 3: w = 32767 - (int32_t)(ph >> 16); break;                                                   /* RAMP */
    case 4:                                                                                             /* S&H */
        if (cyc != dx_mot_cyc) {
            dx_mot_rng = dx_mot_rng * 1664525u + 1013904223u;
            dx_mot_sh = (int32_t)(dx_mot_rng >> 16) - 32768;
        }
        w = dx_mot_sh;
        break;
    default: w = osc_sine(ph); break;                                                                  /* SINE */
    }
    dx_mot_cyc = cyc;
    dx_mot_v = (w * dx_mot[XM_DPTH]) >> 16;             /* (+-DPTH / 2) */
}

/* the master with the FX layer's effects between its level and master_out (perform.c) */
static __attribute__((noinline)) void perf_master(int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
    }
    perf_block(mix_l, mix_r, n);
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}

/* (JIANT 0.6.71) the second core (hal/fm1_multicore.h). Each block, while core 0 renders its parts, core 1 renders
 * some synth parts of its own into its own buffers (c1_buf, c1_mix; mod.c, fm6_core.c, voice.c keep theirs a core);
 * they are added in after it is done: the same sum, the same sound as one core. The DRUM parts go together, on one core
 * or the other (DRUM-X's kit and its IMAGE / SEND buffers are one for all; 0.6.71: always core 0). The parts go to
 * whichever core has less to do, by what each took the last blocks (0.6.73: measured; was their sounding voices and
 * engines, mc_split). A core 1 that misread, faulted or did not come back is held for good: its block of those parts is
 * dropped and core 0 renders every part from the next one. REC held at power-on: core 0 alone (mc_off). MENU > ABOUT
 * says which (mc_status) */
#if FELUCCA_DUAL_CORE
#define mc_run(f, a) fm1_mc_run(f, a)
#define mc_wait() fm1_mc_wait()
#define mc_live() (fm1_mc_on != 0)
#else
static int mc_run(void (*f)(uint32_t), uint32_t a)      /* (the host: at once, as core 1) */
{
    if (!mc_host)
        return 0;
    mc_host_cpu = 1;
    f(a);
    mc_host_cpu = 0;
    return 1;
}
static int mc_wait(void) { return 0; }
#define mc_live() (mc_host != 0)
#endif
static volatile uint8_t mc_off;                          /* REC held at power-on: one core this session (main.c) */
static volatile int8_t mc_cur = -1;                      /* the part core 1 is in (a job given up: its matrix put back) */
static uint8_t mc_mask_last;                             /* the parts core 1 had the last block (MENU > ABOUT, tests) */
static uint32_t mc_jobs;                                 /* (0.6.72) blocks core 1 rendered parts in (console status) */
static void mix_part_m(track_t *t, uint32_t n);         /* (below) */
#define MC_FX 0x80u                                       /* (0.6.75) mc_job: the buses too */
static void fx_run(uint32_t n);                          /* (below) */
static __attribute__((noinline)) void mc_job(uint32_t a)  /* core 1: the buses (MC_FX), the parts of mask a & 0x3F, a >> 8
                                                         * samples */
{
    uint32_t i, n = a >> 8;
    if (a & MC_FX)
        fx_run(n);
    for (i = 0; i < n; i++)
        c1_mix.l[i] = c1_mix.r[i] = c1_mix.c[i] = c1_mix.d[i] = c1_mix.rv[i] = c1_mix.sl[i] = c1_mix.sr[i] = 0;
    for (i = 0; i < NPART; i++)
        if ((a >> i) & 1u) {
            mc_cur = (int8_t)i;
            mix_part_m(&trk[i], n);
        }
    mc_cur = -1;
}
static uint32_t mc_cost(const track_t *t)               /* a part's share of the work, roughly: its voices by engine */
{
    const engine_t *e = ENGINES[t->engine];
    uint32_t k, v = 0, w = e == &ENG_FM6 ? 4u : e == &ENG_LOFI || e == &ENG_NOISE ? 2u : 3u;
    for (k = 0; k < NVOICE; k++)
        v += t->v[k].active;
    return v ? 1u + v * w : 0u;
}
/* (0.6.73) what each part really took, measured as it renders (on either core: TIMER4, 24 MHz), smoothed over a few
 * blocks; the split goes by it, the voices only breaking ties and starting a part that was silent (the host has no
 * clock: the voices alone) */
#if FELUCCA_DUAL_CORE
#define MC_TICKS() fm1_mc_ticks()
#define MC_VW 24u                                        /* a voice-weight unit in ticks (~1 us) */
#else
#define MC_TICKS() 0u
#define MC_VW 1u
#endif
static uint32_t mc_tk[NTRK], mc_fx_tk;                  /* (and the buses': fx_run) */
static inline __attribute__((always_inline)) void mix_part_t(track_t *t, uint32_t n)   /* mix_part, timed */
{
    uint32_t t0 = MC_TICKS(), i = (uint32_t)(t - trk) % NTRK, d;
    mix_part(t, n);
    d = MC_TICKS() - t0;
    mc_tk[i] = d > mc_tk[i] ? mc_tk[i] + ((d - mc_tk[i]) >> 1) : mc_tk[i] - ((mc_tk[i] - d) >> 3);   /* (up fast) */
}
static void mix_part_m(track_t *t, uint32_t n) { mix_part_t(t, n); }   /* (core 1: from the flash) */
#if FELUCCA_HOTRAM                                       /* (0.6.78) core 0's copy, in RAM (app.ld .hot_text) */
static __attribute__((noinline)) void mix_part_m0(track_t *t, uint32_t n) { mix_part_t(t, n); }
#else
#define mix_part_m0 mix_part_m
#endif
/* the parts for core 1 this block (a bit each): whichever core has less to do takes the dearest left. The DRUM parts
 * go together, as one (DRUM-X's kit and its IMAGE / SEND buffers are one for all): on one core or the other */
static __attribute__((noinline)) uint32_t mc_split(uint32_t c1)   /* c1: what core 1 has already (0.6.75: the buses) */
{
    uint32_t i, j, c0 = 0, mask = 0, cost[NPART + 1], unit[NPART + 1], nu = 0, left = 0;
    cost[0] = unit[0] = 0;
    nu = 1;                                              /* (item 0: the DRUM parts) */
    for (i = 0; i < NPART && i < trk_live; i++) {
        uint32_t v = mc_cost(&trk[i]), c = v ? mc_tk[i] + v * MC_VW : 0u;
        if (!c)
            continue;
        if (ENGINES[trk[i].engine] == &ENG_DRUM) {
            cost[0] += c;
            unit[0] |= 1u << i;
        } else {
            cost[nu] = c;
            unit[nu++] = 1u << i;
        }
    }
    for (i = 0; i < nu; i++)
        if (unit[i])
            left |= 1u << i;
    while (left) {
        for (j = nu, i = 0; i < nu; i++)
            if (((left >> i) & 1u) && (j == nu || cost[i] > cost[j]))
                j = i;
        left &= ~(1u << j);
        if (c1 < c0) {
            c1 += cost[j];
            mask |= unit[j];
        } else {
            c0 += cost[j];
        }
    }
    return mask;
}
/* (0.6.75) the buses, a block behind: on the sends of the block before (fxq_*), their return into fx_wet, their sides
 * into fx_sl / fx_sr; mix_block puts them in the mix. On core 1 while core 0 renders its parts (mc_job), else on core
 * 0 at the same place: the same sound either way (a block, 0.7 ms, later than before 0.6.75: inaudible) */
/* (0.6.76) out of range: the buses' return or side past FX_SANE, 2^20. The most they reach: ~45000 at any setting,
 * ~290000 with any word of their state (fx) wrong (their lines are 16-bit and clamped: nothing grows through the
 * feedback); core 1's known misread, 0x00200000 in a 32-bit load (a send, a sum, a spilled value), is 2^21. The bias +
 * OR: one bit test for the whole block */
#define FX_SANE (1u << 20)
static volatile uint8_t fx_bad;
static uint32_t fx_trips;                                /* (console status: fx_trips) */
static int fx_insane(uint32_t n)
{
    uint32_t i, m = 0;
    for (i = 0; i < n; i++)                             /* (fx_sr is -fx_sl) */
        m |= ((uint32_t)fx_wet[i] + FX_SANE) | ((uint32_t)fx_sl[i] + FX_SANE);
    return m >= 2u * FX_SANE;
}
static inline __attribute__((always_inline)) void fx_run_t(uint32_t n)
{
    uint32_t i, t0 = MC_TICKS(), d;
    for (i = 0; i < n; i++)
        fx_sl[i] = fx_sr[i] = 0;
    fx_buses(fxq_c, fxq_d, fxq_r, fx_wet, n);
    if (fx_insane(n))
        fx_bad = 1;                                     /* (mix_block: fx_trip) */
    d = MC_TICKS() - t0;
    mc_fx_tk = d > mc_fx_tk ? mc_fx_tk + ((d - mc_fx_tk) >> 1) : mc_fx_tk - ((mc_fx_tk - d) >> 3);
}
static void fx_run(uint32_t n) { fx_run_t(n); }         /* (core 1: from the flash) */
#if FELUCCA_HOTRAM                                       /* (0.6.78) core 0's copy, in RAM */
static __attribute__((noinline)) void fx_run0(uint32_t n) { fx_run_t(n); }
#else
#define fx_run0 fx_run
#endif

static __attribute__((noinline)) void mc_join(uint32_t n)   /* core 1's parts into the mix */
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        mix_l[i] += c1_mix.l[i];
        mix_r[i] += c1_mix.r[i];
        send_c[i] += c1_mix.c[i];
        send_d[i] += c1_mix.d[i];
        send_r[i] += c1_mix.rv[i];
        sl_l[i] += c1_mix.sl[i];
        sl_r[i] += c1_mix.sr[i];
    }
}
static __attribute__((noinline)) void mc_lost(void)    /* core 1 given up mid-job: a part's matrix values put back */
{
    int32_t k = mc_cur;
    if (k >= 0 && k < (int32_t)NPART && mod_c[1].on) {
        mod_c[0] = mod_c[1];
        mod_end(&trk[k]);
    }
    mc_cur = -1;
#if FELUCCA_DUAL_CORE
    fm1_mc_bar_set();                                    /* one core until power-off (hal/fm1_multicore.h) */
#endif
}

/* (0.6.76) the buses went out of range (fx_run): their block muted, every line and state of theirs silenced (one
 * long block, once), and core 1 given up as a wrong read if it ran them: a wrong word in a reverb's or the delay's
 * state could ring for seconds or grow through the feedback; no check word can catch that */
static __attribute__((noinline)) void fx_trip(uint32_t c1, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        fx_wet[i] = fx_sl[i] = fx_sr[i] = 0;
    for (i = 0; i < CTL; i++)
        fxq_c[i] = fxq_d[i] = fxq_r[i] = 0;
    for (i = 0; i < DLY_LEN; i++)
        dly_buf[i] = 0;
    for (i = 0; i < CHO_LEN; i++)
        cho_buf[i] = 0;
    for (i = 0; i < 4u; i++)
        fx.ph_s[i] = 0;
    fx.ph_y = 0;
    for (i = 0; i < RPRE_LEN; i++)
        rpre_buf[i] = 0;
    for (i = 0; i < CL_LEN; i++)
        cloud_buf[i] = 0;
    rev_clear();
    fx.dt1 = fx.dt2 = fx.rt1 = fx.rt2 = 0;
    fx.dly_lp = fx.dly_hp = fx.rf_lp = fx.rpre_s = fx.cl_lp = fx.cl_half = 0;
    fx_bad = 0;
    fx_trips++;
    if (c1) {
#if FELUCCA_DUAL_CORE
        fm1_mc_lost = 1u;                                /* CORES 1 (BAD READ) */
        fm1_mc_hold();
#else
        mc_host = 0;
#endif
        mc_lost();
    }
}

static const char *mc_status(void)                      /* MENU > ABOUT's line */
{
#if FELUCCA_DUAL_CORE
    static const char *const LOST[4] = {"CORES 1 (SINCE A FAULT)", "CORES 1 (BAD READ)", "CORES 1 (NO ANSWER)",
                                        "CORES 1 (FAULT)"};
    if (fm1_mc_on)
        return "CORES 2";
    if (mc_off)
        return "CORES 1 (REC AT POWER-ON)";
    if (fm1_mc_lost)
        return LOST[fm1_mc_lost & 3u];
    return fm1_mc_barred() ? LOST[0] : "CORES 1 (NO START)";
#else
    return mc_host ? "CORES 2" : "CORES 1";
#endif
}

static __attribute__((noinline)) void mix_block(int32_t *out, uint32_t n)   /* (0.6.71: kept out of the ISR's
                                                                               * body, as before: target_budget.py) */
{
    uint32_t i, c1m = 0, c1j = 0;
    int perf;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++)
        send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = sl_l[i] = sl_r[i] = 0;
    pfx_rec_held = perf_kill ? 0u : perf_held & PF_MIDI;
    pfx_block(n, (perf_kill ? 0u : (perf_held | perf_latched) & PF_MIDI) | scene_pfx);
    events_block(n);
    if (song.playing)                                   /* (JIANT) the modulation sequences' clock (mod.c): the block's end on
                                                         * the transport's grid (through tempo changes, an external clock) */
        ms_clock = clk_pos == CLK_START ? ms_clock + n : clk_n * div_samples(2) + clk_pos + n;
    macro_master();                                     /* CLIP / PNCH with the matrix (mod.c) */
    dx_mot_block(n);                                    /* (0.5.1) DRUM-X X-MOD's LFO */
    master_begin();
    duck_block();
    perf = perf_begin(n);                               /* the FX hold layer at work (perform.c) */
    if (!mc_off && mc_live()) {                         /* (0.6.71) core 1 at its parts meanwhile; (0.6.75) and the buses */
        c1m = mc_split(mc_fx_tk + 4u * MC_VW);
        if (mc_run(mc_job, c1m | MC_FX | n << 8))
            c1j = 1;
        else
            c1m = 0;
    }
    if (!c1j)
        fx_run0(n);                                     /* (0.6.75) the buses here: one core */
    for (i = 0; i < NPART; i++)
        if (!((c1m >> i) & 1u))
            mix_part_m0(&trk[i], n);
    if (c1j) {
        if (!mc_wait()) {
            if (c1m)
                mc_join(n);
            mc_jobs++;
        } else {
            mc_lost();                                  /* (its parts' block and the buses' dropped; one core from here) */
            for (i = 0; i < n; i++)
                fx_wet[i] = fx_sl[i] = fx_sr[i] = 0;
        }
    }
    if (fx_bad)
        fx_trip(c1j, n);
    mc_mask_last = (uint8_t)c1m;
    if (pfx_any)
        pfx_end();                                      /* (DEC- / DEC+ back) */
    if (perf)
        perf_pre(mix_l, mix_r, sl_l, sl_r, send_d, send_r, n);   /* (THROW: the SLICER's bus too) */
    for (i = 0; i < n; i++) {                           /* (0.6.75) the buses' return and sides (the block before's sends); */
        wet[i] = fx_wet[i];                             /* this block's sends for the next */
        mix_l[i] += fx_sl[i];
        mix_r[i] += fx_sr[i];
        fxq_c[i] = send_c[i];
        fxq_d[i] = send_d[i];
        fxq_r[i] = send_r[i];
    }
    if (slicer_fx())                                    /* (JIANT 0.6.3) FX ON: the returns through the SLICER */
        for (i = 0; i < n; i++) {
            sl_l[i] += wet[i];
            sl_r[i] += wet[i];
            wet[i] = 0;
        }
    slicer_bus(sl_l, sl_r, n);                          /* (its clock always; the bus when on) */
    if (slicer_on())
        for (i = 0; i < n; i++) {
            mix_l[i] += sl_l[i];
            mix_r[i] += sl_r[i];
        }
    if (clip_g)
        clip_block(n);
    lev_block(n);                                       /* (JIANT 0.5) the leveler: the loudness held */
    if (perf) {
        perf_master(out, n);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t l = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        int32_t r = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
