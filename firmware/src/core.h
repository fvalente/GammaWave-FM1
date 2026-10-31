/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA core types: tracks, voices, engines, parameters.
 * Four tracks, each a synth part: its own engine, preset, parameters, voices and 64-step
 * pattern. The parts share one budget of NVOICE sounding voices (voice.c). Drums are the DRUM
 * engine (General MIDI map) on any part; the SAMPLE engine's PERC set is retired (drum_from_perc).
 * Sections: sizes, parameters, voices and engines, tracks and the song, system. */
#include <stdint.h>

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 8                 /* voices per part, and the budget shared by all parts */
#define NPART 6                  /* synth parts: tracks 1..6 (JIANT 0.6: was 4) */
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#define NSTEP 64
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later) */
#ifndef FELUCCA_FM4
#define FELUCCA_FM4 0            /* the DIGITAL engine (eng_digital.c, four-operator FM): kept in the tree, not built
                                  * by default; replaced by FM6, its sounds convert (fm4_convert.c) */
#endif
#define NENGINES 14              /* the stores' engine numbers 0..13 (engines.c ENGINES[], append-only) */
#define ENGI_DIGITAL 1u          /* reserved without FELUCCA_FM4: never selectable (eng_ok), its sounds load as FM6 */
#define NENG_SHOWN (NENGINES - !FELUCCA_FM4 - 5)   /* the engines one can pick: PRESETS, the EDIT layer, the editor,
                                                * in the display order of engines.c ENGINE_ORDER (PHYS 9, and in
                                                * JIANT SAMPLE 4, GRAIN 8, SLICE 13: retired) */
/* (JIANT 0.6.71) the second core (hal/fm1_multicore.h; fx.c mix_block hands it some synth tracks). MC_CPU(): the core
 * running this, for the scratch each core has its own of (fx.c part_buf and the mix, mod.c's matrix, fm6_core.c's
 * buses, voice.c's filter flags). The host (tests): no second core; mc_host runs its jobs at once, as core 1 */
#ifndef FELUCCA_DUAL_CORE
#define FELUCCA_DUAL_CORE 0
#endif
#if FELUCCA_DUAL_CORE
#define MC_CPU() fm1_cnum()
#else
static uint8_t mc_host, mc_host_cpu;     /* tests: 1 = jobs handed over (run at once); the core "running" */
#define MC_CPU() ((uint32_t)mc_host_cpu)
#endif
/* (JIANT 0.6.78) the hottest audio code in RAM, for core 0 (felucca.c FELUCCA_HOTRAM): app.ld puts these functions'
 * sections (-ffunction-sections) in .hot_text by name, so the compiler builds them as before (this clang inlines
 * nothing across a section attribute, not even a static inline). Core 1 keeps running the flash copies (fx.c
 * mix_part_m, fx_run): it now and then reads a word of RAM wrong (hal/fm1_multicore.h) */
#ifndef FELUCCA_HOTRAM
#define FELUCCA_HOTRAM 1
#endif
#define UP_SLOTS 32u             /* user presets (upreset.c) */
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS,
    F_PCTINV                     /* (JIANT 0.6.37) a share of 127 shown and turned the other way: 0 is 100 % */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum {                          /* per-track parameters */
    P_LEVEL,
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ED_FX,     /* P_ED_FX (once unused): JIANT 0.4's ENV LOOP, P_ELOOP */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_DIST, P_CHOR, P_DLY, P_REV,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_ALLOC, P_DETUNE,
    P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH,      /* SLICER insert (slicer.c) */
    P_M1SRC, P_M1DST, P_M1AMT,                 /* modulation matrix (mod.c): 4 slots of SRC, DST, AMT; */
    P_M2SRC, P_M2DST, P_M2AMT,                 /* new common parameters go just before P_E0 (user presets */
    P_M3SRC, P_M3DST, P_M3AMT,                 /* and projects map by count) */
    P_M4SRC, P_M4DST, P_M4AMT,
    P_FM1_ATK, P_FM1_DEC, P_FM1_SUS, P_FM1_REL, P_FM1_LEVEL,
    P_FM2_ATK, P_FM2_DEC, P_FM2_SUS, P_FM2_REL, P_FM2_LEVEL,
    P_FM3_ATK, P_FM3_DEC, P_FM3_SUS, P_FM3_REL, P_FM3_LEVEL,
    P_FM4_ATK, P_FM4_DEC, P_FM4_SUS, P_FM4_REL, P_FM4_LEVEL,
    P_SOFS, P_POFS,                            /* (JIANT; the former chord keys) the sequence offset: OFS steps, PIT semitones */
    P_LN0, P_LN1, P_LN2, P_LN3, P_LN4, P_LN5, P_LN6, P_LN7,   /* DRUM lane levels (eng_drum.c, #97): KICK .. BELL */
    P_DTYPE, P_DTONE,                          /* (JIANT, FUNB) DIST's type and tone (fx.c track_dist) */
    P_MSLEN, P_MSDIV, P_MSSLW,                 /* (JIANT, FUNB) the modulation sequence (mod.c MS_STEP): its length, */
    P_MS0, P_MS15 = P_MS0 + 15,                /* rate and slew, its 16 levels */
    P_FTYPE,                                   /* (JIANT 0.5) the filter's type: LP HP BP COMB (eng_analog.c, voice.c) */
    P_FCUT, P_FRES,                            /* .. every other engine's: a filter on the part's sum (voice.c track_filter) */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};
#define P_LRTRG P_FM1_ATK              /* (JIANT 0.6.12) LOFI BYTE / FLOAT's RTRG: a note restarts the formula's time (voice.c
                                         * voice_start), in a slot of the retired DIGITAL's (0: OFF, its default) */
#define P_TCHAN P_FM1_DEC              /* (JIANT 0.6.37) SHIFT > CHANCE: how much of the track's steps it skips, 0 none (100 %,
                                         * every project and preset before it); seq.c trk_chance; DIGITAL's OP1 DEC slot */
/* (JIANT 0.6.43) MUTATE (project.c mutate_poll), in DIGITAL's OP1 REL / OP2 slots (0, every project and preset before it:
 * OFF, 0 %, ORBIT): SOUND and PATTERN every OFF / 1 / 2 / 4 / 8 / 16 / 32 bars, AMOUNT, MODE ORBIT / DRIFT */
#define P_MSND P_FM1_REL
#define P_MPAT P_FM2_ATK
#define P_MAMT P_FM2_DEC
#define P_MMODE P_FM2_REL
#define P_MORPH P_FM3_ATK              /* (JIANT 0.6.45) EDIT > MORPH: ANALOG PHASE LOFI's wave on towards the next ones
                                         * (0: as WAVE has it; every project and preset before it), in DIGITAL's slot */
#define P_CLEN P_FM3_DEC               /* (JIANT 0.6.47) SHIFT > C.LEN: the step components as a lane of their own, steps
                                         * 1..C.LEN's read in turn as the track plays (polymetric over the steps); 0 OFF
                                         * (each step its own: every project and preset before it); seq.c comp_lane */
#define P_DXIMG P_FM3_REL              /* (JIANT 0.6.48) a DRUM track's EDIT > IMAGE: one knob through the kit's pan maps
                                         * (eng_drum.c dx_img_pan; 0: every sound in the middle, as before) */
/* (JIANT 0.6.81) SCL > CHORD (seq.c chord_build), in DIGITAL's OP4 slots (0, every project and preset before it: OFF,
 * CLOSE, no strum) */
#define P_CHRD P_FM4_ATK               /* OFF; in the scale: TRIAD 7TH ADD9 SUS2 SUS4; any key: MAJ MIN 7 MAJ7 MIN7 POWER */
#define P_CVOI P_FM4_DEC               /* CLOSE, LEAD (each chord the inversion nearest the last), OPEN, +BASS */
#define P_CSTR P_FM4_REL               /* STRUM: 0 at once; up to ~63 ms between the notes, low to high */
/* (JIANT 0.6.87) SEQ > HUMAN, in DIGITAL's OP1 SUS / LEVEL and OP2 SUS slots (127, every project and preset before
 * it: shown as 0 %, none): how much each note's VELOCITY, TIME (a little late) and GATE vary at random as the sequencer
 * plays them (seq.c hum_amt); stored the other way round, 127 - the amount */
#define P_HVEL P_FM1_SUS
#define P_HTIM P_FM1_LEVEL
#define P_HGAT P_FM2_SUS
#define P_HRQ P_FM2_LEVEL              /* (0.6.88) SEQ > TIMING's REC Q: how much live recording snaps to the grid, 127 (every
                                         * project before it) 100 %: on it; less: the notes keep that much of where they were */
#define P_ELOOP P_ED_FX                /* (JIANT 0.4) ENV LOOP: OFF / ON (voice.c env_tick) */
/* (JIANT 0.6.9) AUTO PAN (fx.c mix_part), in the slots of the old per-track SLICER (one on a bus since 0.6.3, its
 * slots stored, never read but by proj_sl_migrate). Laid so what old stores hold means OFF: DPTH where SLCR was (0, or
 * 1..2 from a project of before 0.6.3: inaudible), RATE where PAT was (1..16: slow), WAVE where RATE was (1: TRI), SYNC
 * where DEPTH was (127: BEAT) */
#define P_APDEP P_SLCR                 /* depth 0..127 (0: off) */
#define P_APRATE P_SLPAT               /* FREE: 0..127 as the LFO's RATE; BEAT: 0..7, 4 BARS .. 1/32 (fx.c AP_SIXT) */
#define P_APWAVE P_SLRATE              /* SIN TRI SAW SQR S&H (N_LWAVE) */
#define P_APSYNC P_SLDEPTH             /* FREE / BEAT (any value but 0: BEAT) */

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_DREL, G_ROUTE, G_INFO,   /* G_DREL (JIANT): DUCK's release; was G_SYNC, a placeholder ("--", 0..0) */   /* G_ROUTE: MIDI IN, 0 CH1-4 (channels 1..4 -> parts 1..4, 5..16 ignored), 1 SEL (seq.c) */
    G_SLOT, G_NAME, G_LOAD, G_SAVE,
    G_ENGSEL, G_CLIP,           /* no page: the editor switches the engine with a SET of G_ENGSEL. G_CLIP (JIANT):
                                 * the master clipper; was G_ENGGO, unused (0..0) */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING (fx.c). Was G_DRCH, the GM drum part's MIDI
                                 * channel (inert since 1.0, never read); projects of formats before FUN7 load it
                                 * as ROOM (project.c proj_rtype_room) */
    G_PUNCH, G_DUCK,            /* (JIANT) the drum bus's PUNCH and the kick's DUCK of the synths (fx.c, eng_drum.c).
                                 * Were G_DRLVL / G_DRREV, inert since 1.0 (0..0): the GM drum part's level and
                                 * reverb send, read only by the import of a project of before 1.0
                                 * (proj_drums_to_part, which clears them after) */
    G_NSTORE,                   /* the 27 a project's header holds (FUN7 on: their ids kept) */
    G_DHPF = G_NSTORE,          /* (JIANT, FUNB: after the punch-in lane, project.c) the delay's feedback low cut (0.4:
                                 * in DLY's TONE; stored, unused); 0.6.9: the CHORUS's STEREO (G_CSTER), */
    G_WIDTH,                    /* the buses' stereo width (the delay's right echo later, the chorus's taps apart), */
    G_RMOD, G_RRATE,            /* the reverb's modulation (ROOM: its combs' lengths; SPRING: its wobble) depth, rate, */
    G_RPRE,                     /* its pre-delay */
    G_RFILT, G_RWIDE,           /* (JIANT 0.3) the reverb's tone (darker / brighter) and width, */
    G_DPIT, G_DSPRY,            /* the delay's grains: their pitch (0: a plain delay) and spray */
    G_STRN,                     /* (JIANT 0.4) every sequence moved this many steps of its scale (seq.c seq_step) */
    G_SLMODE, G_SLPAT, G_SLRATE, G_SLDEP,   /* (JIANT 0.6.3) the SLICER, one on a bus (slicer.c): OFF / GATE / STUT, its */
    G_SLT1, G_SLT6 = G_SLT1 + 5, G_SLFX,    /* pattern, step, depth; which tracks go in (and FX: the delay, reverb and */
    G_SLPLEN, G_SLPQ,                       /* chorus returns); the repeats' pitch sequence: its length, CHR / SCL, */
    G_SLP0, G_SLP15 = G_SLP0 + 15,          /* its 16 steps (semitones) */
    G_DMOD,                     /* (0.6.86) the delay's tape drift (WOW and FLUTTER) depth (project.c: its own block, 'DM') */
    G_DTYPE, G_DPING,           /* (0.6.90) the delay's TYPE (ANALOG / DIGITAL) and PING-PONG (project.c: block 'DT') */
    G_CTYPE,                    /* (QG-1) the CHORUS bus's TYPE: CHORUS (the ensemble) / PHASER (fx.c cho_phaser; project.c:
                                 * bit 1 of block 'DT's TYPE byte, which a firmware before it masks off) */
    G_COUNT
};
#define G_SL_N (G_SLP15 + 1 - G_SLMODE)     /* the SLICER's globals (stored together: project.c, song_chain.c) */
#define G_CSTER G_DHPF                 /* (JIANT 0.6.9) CHORUS STEREO -64..63: 0 as before (G_DHPF's 0), - narrower .. mono */
#define G_SYNC G_DREL                   /* (the old names, for the formats' importers and their tests) */
#define G_ENGGO G_CLIP
#define G_DRLVL G_PUNCH
#define G_DRREV G_DUCK

/* stored parameters of an older layout -> today's P_* order. A store keeps np = the P_COUNT it was
 * written with; common parameters are only ever added just before P_E0, so the first np - 8 are
 * P_LEVEL.. in order and the last 8 are P_E0..P_E7; the parameters a store does not have take def[]
 * (user presets, projects of formats 1 and 2) */
static void params_by_count(int16_t *out, const int16_t *in, uint32_t np, const int16_t *def)
{
    uint32_t i, nc = np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? in[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = in[nc + i];
}

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_ANALOG 0u           /* (JIANT 0.5: named for its FILTER page) */
#define ENGI_PHYS 9u
#define ENGI_DRUM 10u
#define ENGI_GRAIN 8u
#define ENGI_SLICE 13u
#define ENGI_SAMPLE 4u
#define ENGI_TRIO 6u             /* (JIANT 0.4) folded into ANALOG (analog_from_trio) */
/* (JIANT) the retired engines: PHYS, and the sample engines SAMPLE GRAIN SLICE (their 123 KB of samples and GRAIN's
 * 27 KB of RAM went to DRUM-X and the effects). Their numbers stay reserved (engines.c ENG_GONE), never offered; a sound
 * of theirs that arrives -- a project, a user preset, the editor -- plays as ANALOG's first preset (SAMPLE's old PERC
 * set as DRUM first: drum_from_perc) */
static inline int eng_gone(uint32_t e)
{
    return e == ENGI_PHYS || e == ENGI_SAMPLE || e == ENGI_GRAIN || e == ENGI_SLICE || e == ENGI_TRIO;
}
/* (JIANT 0.4) a sound stored before 0.4 -> today's E values, in place: LOFI (3) {.., SWP, VIB, ARP, TONE} -> {.., CUT
 * (its TONE), VIB, BEND 0, MASK 0}; ANALOG (0) KTR -> INT 0 (the key tracking fixed at its old default). Projects
 * (project.c: the kit's marker below 3, or an older format) and user presets (upreset.c: no UP_V04_MARK) */
static void sound_v04(uint32_t engine, int16_t *e)
{
    if (engine == 3u) {
        e[4] = e[7];
        e[6] = e[7] = 0;
    } else if (engine == 0u) {
        e[7] = 0;
    }
}
/* (JIANT 0.4) TRIO (engine 6) folded into ANALOG: its sound's E values -> ANALOG's, in place, the nearest WAVE (its
 * three saws SAW3, its sync sets SYNC, its ring sets RING, pulses PWM, triangles TRI, noise as NOIS), INT2 as the
 * SYNC / RING ratio (DTN: 16 x (2^(st/12) - 1)), DTN, CUT and RES kept; 1 = it was TRIO (its engine is ANALOG now):
 * projects, user presets, the editor's PRESET and SET (project.c, upreset.c, ui.c). {WAVE INT2 INT3 DTN MODE CUT RES
 * PW} -> {WAVE DTN MIX NOIS CUT RES DRV KTR} */
static int analog_from_trio(uint32_t engine, int16_t *e)
{
    static const uint8_t W[16] = {7, 4, 4, 7, 2, 0, 1, 4, 0, 5, 5, 5, 6, 6, 6, 6};
    static const uint8_t R[25] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 14, 16, 18, 20, 22, 25, 27, 30, 34, 37, 41, 45, 49,
                                  54};   /* (16 x (2^(st/12) - 1), st 0..24) */
    uint32_t w = (uint32_t)e[0] & 15u;
    int32_t st = e[1] < 0 ? -e[1] : e[1];
    if (engine != ENGI_TRIO)
        return 0;
    e[0] = W[w];
    e[1] = W[w] >= 5u ? (int16_t)R[st > 24 ? 24 : st] : (int16_t)(e[3] > 0 ? e[3] : 6);
    e[2] = (int16_t)(w == 8u ? 0 : 64);
    e[3] = (int16_t)(w == 7u ? 60 : w == 8u ? 100 : 0);
    e[4] = e[5];                                        /* CUT */
    e[5] = e[6];                                        /* RES */
    e[6] = 0;
    e[7] = 64;
    return 1;
}
/* PHYS MODEL DRUM (MODEL 4, before 1.0) -> the DRUM engine, its E values in place: {MODEL, TUNE, TONE, DECY,
 * SNAP, ACC, KICK, PERC} -> {MRPH 64, TUNE, TONE, DECY, NOIS 64, FM 0, -, DRV 0}. 1 = it was one (its engine is
 * ENGI_DRUM now); projects (project.c) and user presets (upreset.c) */
static int drum_from_phys(uint32_t engine, int16_t *e)
{
    if (engine != ENGI_PHYS || e[0] != 4)
        return 0;
    e[0] = e[4] = 64;
    e[5] = e[6] = e[7] = 0;                             /* (ACC is gone: E5 is FM now, off) */
    return 1;
}

/* SAMPLE SET 4 was PERC, the GM-mapped drum kit (tools/gen_samples.py); retired after 1.0.2: its index stays
 * (SET / GRAIN SRC 4 is an alias of PIANO; 5..7, once USR1..3, clamp to it). A sound that selected it is the DRUM engine with
 * its default kit (eng_drum.c DRUM_PRESETS[0]: the same GM key map, so its patterns still play as drums): its E
 * values become the kit's, the rest of the sound (mix, sends, matrix, ..) stays. 1 = it was one (its engine is
 * ENGI_DRUM now); projects (project.c proj_perc), user presets (upreset.c up_migrate), factory preset 4 and
 * favourites (ui.c, settings_persist.c). Idempotent */
#define SMP_SET_PERC 4u
#define DRUM_KIT_E {64, 64, 70, 64, 64, 0, 0, 0}   /* {MRPH, TUNE, TONE, DECY, NOIS, FM, WARP, DRV} (eng_drum.c) */
static int drum_from_perc(uint32_t engine, int16_t *e)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    uint32_t i;
    if (engine != ENGI_SAMPLE || e[0] != (int16_t)SMP_SET_PERC)
        return 0;
    for (i = 0; i < 8u; i++)
        e[i] = KIT[i];
    return 1;
}

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */
typedef struct {
    uint8_t note, vel, gate, active;
    uint8_t stage;               /* env: 0 off, 1 attack, 2 decay/sustain, 3 release, 4 given up (fading) */
    uint8_t kfade;               /* (0.6.74) stage 4: the fade's blocks so far (voice.c env_tick) */
    uint16_t fin_d;              /* (0.6.81) a chord glide's new note fading in (seq.c chord_glide); 0.6.82: as its glide
                                  * goes, the distance it set out from (1/16 st; 0 none) */
    int32_t env;                 /* Q24 */
    int32_t env_out;             /* last control-rate amplitude, Q15 */
    int32_t pitch16, pitch_cur;  /* 1/16 semitone, with glide */
    int32_t gstep;               /* glide TIME mode: 1/16 st per control tick, Q8 (0.6.82: slow glides smooth), 0 = RATE */
    int32_t fine;                /* unison detune: phase increment * (1 + fine / 4096) */
    uint32_t ph[3];
    int32_t s[8];                /* engine state (filters, envs) */
    uint32_t age;
    uint8_t mvel;                /* mod.c: the note-on velocity (before UNISON scaling) and RAND of the note */
    uint8_t gacc;                /* (0.6.82) the glide's fraction of a 1/16 st carried (gstep Q8) */
    int16_t mrnd;
} voice_t;

typedef struct {                 /* per-voice control-rate modulation, computed in voice.c */
    uint32_t inc;                /* phase increment of the base pitch */
    int32_t pitch16;
    int32_t amp0, amp1;          /* Q15 ramp over the block */
    int32_t cutoff;              /* 0..127 << 8 */
    int32_t shape;               /* 0..127 << 8 */
    int32_t envq15;              /* env value (for engines that use it as a mod source) */
    int32_t fine;                /* the residual below 1/16 semitone in inc: unison detune, TUNE, bend (1/4096) */
} vmod_t;

typedef struct {
    const char *name;
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* 1 = MONO (bass / lead), 0 = POLY */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
    uint8_t fx[4];               /* DIST, CHORUS, DELAY, REVERB sends */
    uint8_t pat;                 /* suggested pattern (PATTERNS[pat - 1], 0 = none): only a hint. A sound load
                                  * never touches the steps; SEQ > PATTERNS loads a pattern (ui.c pat_load). The
                                  * arp is the track's too: a preset does not set it */
    struct { uint8_t id; int8_t v; } x[6];   /* (1.2-GW) up to six more of the sound's values a preset sets (P_* + 1,
                                  * 0 = none; its value as is): the LFO, the matrix, VOICE, GLIDE .. so a pad moves by
                                  * itself. None in the presets before 1.2: they load as they did */
} preset_t;
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define XP(id, v) {(id) + 1, (v)}
#define PAT(n) .pat = (n)

struct track;
typedef struct {                 /* an engine (engines.c ENGINES[]; the eng_*.c files) */
    const char *name;            /* "ANALOG" (PRESETS, the editor) */
    const char *page_title[2];   /* EDIT 1 and EDIT 2 */
    param_desc_t edit[8];        /* P_E0..P_E7 */
    const preset_t *presets;
    uint8_t npresets;
    uint8_t knob[4];             /* HOME: the four parameters on KNOB 1..4 */
    uint8_t poly;                /* voice cap for POLY and UNISON, 0 = NVOICE */
    uint8_t sampled;             /* 1 = plays recorded material (a position, not a phase): voice.c keeps
                                  * no phases over a retrigger, spreads none for UNISON, renders at SUS 0 */
    uint8_t keep;                /* bit k: s[k] is kept when a sounding voice is retriggered (filters) */
    uint8_t oneshot;             /* 1 = hits (DRUM): always POLY, no glide; amp ends its voices (eng_drum.c) */
    void (*note_on)(struct track *t, voice_t *v);
    void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
    /* optional (0 = none): the voice amplitude instead of the ADSR curve, once per control tick;
     * gets the ADSR value (Q15, env_tick already ran: it still gates the voice), returns Q15 */
    int32_t (*amp)(struct track *t, voice_t *v, int32_t adsr);
    /* optional: a mode-dependent descriptor of EDIT k (the same range and default as edit[k],
     * another label / value names), 0 = edit[k] */
    const param_desc_t *(*desc)(const struct track *t, uint32_t k);
    /* optional: once per block and part, before its voices (also with no voice sounding) */
    void (*block)(struct track *t);
    /* optional: the note of key k (0 = the lowest) when the engine maps the keys itself, else -1
     * (then the scale keyboard of seq.c kb_map) */
    int32_t (*keys)(const struct track *t, uint32_t k);
    /* 1 = the engine's own envelopes are the voice's amplitude (FM6): voice.c renders it at 1.0 (no ADSR, no
     * velocity; LFO -> AMP, the matrix's AMP and the fades still apply; ENV is 0) and the voice ends when
     * done() says so (once per control tick, before the render), not at the end of the ADSR's release */
    uint8_t ownenv;
    int (*done)(struct track *t, voice_t *v);
} engine_t;

/* ------------------------------------------------- tracks, the song --- */
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
#define SF_SLIDE 2u
#define SF_RATCH_SH 3u                   /* RATCH: the hits of a NOTE step - 1 (0..3: x1..x4, seq.c seq_ratchet); */
#define SF_RATCH (3u << SF_RATCH_SH)     /* bits 3..4, so a user preset's pattern (flag 4 = tie) carries it too */
#define NLANE 8                  /* drum lanes of a step (the DRUM engine's: eng_drum.c DRUM_LANE_NOTE) */
typedef struct {                 /* acid-style step: up to 4 notes (POLY), time, accent, slide; drum hits */
    uint8_t note[4];
    uint8_t n;                   /* notes in use, 0 = empty */
    uint8_t time;                /* ST_NOTE / ST_TIE / ST_REST */
    uint8_t flags;               /* SF_ACCENT | SF_SLIDE | SF_RATCH */
    uint8_t vel;
    uint8_t hit;                 /* bit l: lane l hits (its GM note, on any engine): the DRUM grid */
    uint8_t acc;                 /* bit l: that hit is accented (velocity 127) */
    uint8_t probability;         /* 0 = legacy 100%; 1..100 = percent, 101 = silent */
    uint8_t comp, carg;          /* (JIANT 0.6.39) the step's component (CP_*, 0 none) and its arguments (seq.c) */
    uint8_t comp2, carg2;        /* (0.6.82) a second component (SEQ > COMP 2), the same way */
    uint8_t cwhen;               /* (0.6.82) WHEN each plays: bits 0-1 comp's, 2-3 comp2's (CW_*) */
    uint8_t ctime;               /* (0.6.82) bits 0-2 DIV: RATCH's and ECHO's parts a step/2 .. /8 apart (0: the step
                                  * shared out, as before); bits 3-6 LEN: a part's gate n/16 (0: the track's GATE) */
} step_t;
/* (0.6.82) WHEN a component plays: ALWAYS, on odd loops (1:2), on even loops (2:2), SKIP (kept, not played) */
enum { CW_ALWAYS, CW_ODD, CW_EVEN, CW_SKIP };
#define CW_OF(s, k) (((uint32_t)(s)->cwhen >> ((k) * 2u)) & 3u)
enum { EV_ALWAYS, EV_1_2, EV_2_2, EV_SKIP, EV_1_3, EV_2_3, EV_3_3, EV_1_4, EV_2_4, EV_3_4, EV_4_4, EV_1_8, EV_FIRST,
       EV_NOT1, EV_N };
#define ST_EVERY(s) ((uint32_t)(s)->cwhen & 15u)
#define ST_EVERY_SET(s, e) ((s)->cwhen = (uint8_t)(((s)->cwhen & 0xF0u) | ((uint32_t)(e) & 15u)))
/* (0.6.88) TIMING: the step's own place off the grid, -8 .. +7 sixteenths of a step (0 on it), in cwhen's bits 4-7
 * (stored with it, project.c); recorded off the grid (REC Q) or set on SEQ > TIMING. seq.c: late by a queue, early by
 * ending the step before it that much sooner */
#define ST_MICRO(s) ((int32_t)(int8_t)(s)->cwhen >> 4)
#define ST_MICRO_SET(s, m) ((s)->cwhen = (uint8_t)(((s)->cwhen & 15u) | (((uint32_t)(m) & 15u) << 4)))
#define CT_DIV(s) ((uint32_t)(s)->ctime & 7u)
#define CT_LEN(s) (((uint32_t)(s)->ctime >> 3) & 15u)
#define CT_NDIV 6u
static const uint8_t CT_DIVS[CT_NDIV] = {0, 2, 3, 4, 6, 8};
/* (JIANT 0.6.39) step components, OP-Z style (SEQ > COMP): one a step, on a NOTE step of a melodic track.
 * ARP NOTES / SEQ / SCALE: the step plays an arpeggio, RATE notes in its length, over the TIEs after it; its notes
 * (NOTES: the step's own, SEQ: every note of the track's pattern, both over RANGE octaves; SCALE: the scale up from
 * the step's lowest note, RANGE: 3 5 8 15 degrees) in the ORDER. TRIG: the step plays on some loops of the pattern
 * only (1:2 the first of every two, .., FIRST, NOT FIRST). carg: ARP ORDER bits 0-1, RATE 2-4, RANGE 5-6; TRIG the
 * condition. */
/* (0.6.42) RAND: the notes up / down the scale at random (carg 1..7 degrees); OCT: an octave (carg: +1, -1, +1 every
 * other loop, -1 every other loop, + / - by turns, at random); TONAL: the rest of the loop moved (carg 0..14: -7..+7
 * degrees); BEND: the notes glide over the step (carg 0..24: -12..+12 semitones); ECHO: repeats fading (carg 0..2:
 * x2..x4; 0.6.82: .. x8); PULSE: its notes again on the empty steps after it (carg 0..3: every 1..4); JUMP: then step carg + 1;
 * REV: back from here to step 1 (ping-pong); FILL: only while FX is held; KILL: what the track sounds cut first */
enum { CP_NONE, CP_ARPN, CP_ARPQ, CP_ARPS, CP_TRIG, CP_RAND, CP_OCT, CP_TONAL, CP_BEND, CP_ECHO, CP_PULSE, CP_JUMP,
       CP_REV, CP_FILL, CP_KILL, CP_WALK, CP_CLIMB, CP_N };
#define CP_WRATE(a) ((uint32_t)(a) & 15u)
#define CP_WMODE(a) (((uint32_t)(a) >> 4) & 3u)
#define CP_WRNG(a) (((uint32_t)(a) >> 6) & 3u)
#define CP_WARG(r, m, g) ((uint8_t)(((r) & 15u) | ((m) & 3u) << 4 | ((g) & 3u) << 6))
#define CP_NWRATE 12u
static const uint8_t CP_WRATES[CP_NWRATE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16};
static const uint8_t CP_WDEGS[4] = {3, 5, 7, 12};
/* the range of carg each component's KNOB 3 turns (ARP: KNOB 3 / 4 their own) */
static const uint8_t CP_AMAX[CP_N] = {0, 0, 0, 0, 11, 7, 5, 14, 24, 6, 3, 63, 0, 0, 0, 0, 0};
static const uint8_t CP_ADEF[CP_N] = {0, 0, 0, 0, 0, 2, 0, 9, 7, 1, 0, 0, 0, 0, 0, 0x40, 0x00};
static const uint8_t CP_AMIN[CP_N] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define CP_ORD(a) ((uint32_t)(a) & 3u)            /* UP DOWN UP-DN RND */
/* (0.6.62) JUMP: its step (bits 0-5) and TIMES (bits 6-7); REV: TIMES (bits 0-1). TIMES: ALWAYS, x1 (once a pass of
 * the pattern, then it plays on), x2, 1:2 (once, every other pass). 0, ALWAYS: as a JUMP / REV of before (seq.c fl_ok) */
#define CP_JTO(a) ((uint32_t)(a) & 63u)
#define CP_TIMES(c, a) ((c) == CP_JUMP ? ((uint32_t)(a) >> 6) & 3u : (uint32_t)(a) & 3u)
#define CP_RATE(a) (((uint32_t)(a) >> 2) & 7u)    /* 0..4: x2 x3 x4 x6 x8 */
#define CP_RNG(a) (((uint32_t)(a) >> 5) & 3u)     /* 0..3: 1..4 octaves (SCALE: 3 5 8 15 degrees) */
#define CP_ARG(o, r, g) ((uint8_t)(((o) & 3u) | ((r) & 7u) << 2 | ((g) & 3u) << 5))
#define CP_NRATE 5u
#define CP_NTRIG 12u
static const uint8_t CP_RATES[CP_NRATE] = {2, 3, 4, 6, 8};
static const uint8_t CP_DEGS[4] = {3, 5, 8, 15};
static const uint8_t CP_TRIGS[CP_NTRIG][2] = {{1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
                                              {1, 8}, {0, 1}, {0, 2}};   /* (a of b; {0, 1} FIRST, {0, 2} NOT FIRST) */
static int cp_arp(const step_t *s) { return s->comp >= CP_ARPN && s->comp <= CP_ARPS; }
static int cp_isarp(uint32_t c) { return c >= CP_ARPN && c <= CP_ARPS; }
typedef struct {                 /* a step as formats 1..4 (projects to FUN4) stored it: no hits */
    uint8_t note[4];
    uint8_t n, time, flags, vel;
} step8_t;

/* Probability keeps zero-initialized and legacy patterns at 100%. */
static uint32_t step_chance(const step_t *s) { return !s->probability ? 100u : s->probability <= 100u ? s->probability : 0u; }
static void step_set_chance(step_t *s, uint32_t chance) { s->probability = (uint8_t)(chance >= 100u ? 0u : chance ? chance : 101u); }
/* RATCH: a NOTE step plays its notes and hits this many times (1..4), in equal parts of the step; every older
 * pattern holds 0 there, x1 */
static uint32_t step_ratchet(const step_t *s) { return ((s->flags & SF_RATCH) >> SF_RATCH_SH) + 1u; }
static void step_set_ratchet(step_t *s, uint32_t hits)
{
    s->flags = (uint8_t)((s->flags & ~SF_RATCH) | ((hits < 1u ? 0u : hits > 4u ? 3u : hits - 1u) << SF_RATCH_SH));
}
#define MOTION_MAX 64u
/* Four tracks x64 steps fit one byte (JIANT 0.6, six tracks: the track's bit 2 is value's bit 12 flipped against its
 * sign, MOTION_TRK; values stay within -64..127, so an old record reads as track 0..3). Values retain their signed parameter range.
 * param: the P_* id (< 128: P_COUNT is at most 127, project.c), bit 7 (MOTION_LOCK, 1.1) a parameter lock: the value
 * sounds on that step only and goes back after it (motion.c motion_step); without it an automation event, the
 * value holds until another one changes it. One record per (track, step, id), of either kind */
#define MOTION_LOCK 0x80u
#define MOTION_ID(e) ((uint32_t)(e)->param & 0x7Fu)
#define MOTION_HI(e) ((uint32_t)(((e)->value >> 12) ^ ((e)->value >> 15)) & 1u)
#define MOTION_TRK(e) ((uint32_t)(e)->place >> 6 | MOTION_HI(e) << 2)
#define MOTION_STEP(e) ((uint32_t)(e)->place & 63u)
#define MOTION_PLACE(e) (MOTION_TRK(e) << 6 | MOTION_STEP(e))   /* track << 6 | step */
#define MOTION_VAL(e) ((int16_t)((e)->value ^ (int32_t)(MOTION_HI(e) << 12)))
typedef struct { uint8_t place, param; int16_t value; } motion_event_t;
typedef struct { uint8_t count, on, rsv[2]; motion_event_t event[MOTION_MAX]; } motion_store_t;
static void motion_at(motion_event_t *e, uint32_t place, int16_t v)   /* place: track << 6 | step; v: the value */
{
    e->place = (uint8_t)(place & 255u);
    e->value = (int16_t)(v ^ (int32_t)(((place >> 8) & 1u) << 12));
}
_Static_assert(sizeof(motion_store_t) == 260u, "motion disk layout");
#define CHAIN_ROWS 16u
typedef struct { uint8_t slot, bars; } chain_row_t;   /* a song row: section A..D (a project slot), 1..CHAIN_BARS bars */
#define CHAIN_BARS 64u
typedef struct {
    uint8_t count, rsv[3];
    chain_row_t row[CHAIN_ROWS];
} chain_config_t;

typedef struct track {
    int16_t p[P_COUNT];
    uint8_t engine, preset;      /* engine: what the audio ISR renders */
    uint8_t eng_req;             /* engine the UI asked for (the ISR switches at a block start) */
    uint8_t user;                /* user preset slot + 1 the sound came from (UI), 0 = none */
    voice_t v[NVOICE];
    /* LFO */
    uint32_t lfo_ph;
    int32_t lfo_val;             /* Q15 */
    int32_t lfo_fade;            /* Q15 ramp after note-on */
    uint32_t lfo_rnd;
    /* keyboard / arp input: held notes in press order */
    uint8_t held[16];
    uint8_t nheld;
    uint8_t arp_phys;            /* keys physically held for the arp */
    uint8_t latched;             /* HOLD: keep notes after release */
    /* arp runtime */
    uint32_t arp_pos;            /* q8 samples into the current arp step */
    uint32_t arp_idx;
    uint8_t arp_note;            /* sounding arp note, 0 = none */
    uint8_t arp_walk;            /* WALK: the place in the note list it stands on */
    uint8_t arp_ch[3];           /* CHORD: the other notes sounding with arp_note, 0 = none */
    uint32_t arp_off;            /* q8 sample time of its note-off */
    /* sequencer */
    step_t step[NSTEP];
    uint32_t seq_pos;            /* q8 samples into the current step */
    uint16_t seq_idx;
    uint8_t seq_notes[4 + NLANE];   /* sounding seq notes (the step's notes, then its hits) */
    uint8_t seq_n;
    uint8_t seq_hold;            /* last step slides: keep the notes until the next step */
    uint8_t slide_glide;         /* next legato note glides (slide) */
    uint8_t rat_left;            /* RATCH: repeats of the playing step still to come (seq.c seq_ratchet) */
    uint8_t arp_from;            /* (0.6.39) an ARP component playing: its step + 1 (0: none), over the TIEs after it */
    uint8_t arp_k;               /* .. the arpeggio's next note (its position in the ORDER) */
    uint16_t loops;              /* (0.6.39) the pattern's loops since PLAY (TRIG components) */
    int8_t tonal;                /* (0.6.42) TONAL: the rest of the loop this many degrees up / down */
    uint8_t pulse_from, pulse_k; /* .. PULSE: its step + 1 (0: none), the empty steps since */
    uint8_t jump_to, rev;        /* .. JUMP: the next step + 1 (0: the next one); REV: backwards to step 1 */
    uint16_t mo_cur, mo_prev;    /* (0.6.50) MORPH as the engines play it: Q8, slewed a block at a time (voice.c
                                  * track_render) and ramped across the block from mo_prev (no step: no click) */
    uint8_t cpos, cv_on;         /* (0.6.47) C.LEN: the component lane's next step; the step playing is cvs */
    step_t cvs, cva, cvp;        /* .. the step playing with the lane's component; the ARP's and PULSE's (CV_IDX) */
    uint32_t seq_off;
    uint8_t seq_active;          /* any step programmed */
    uint8_t rskip_idx;           /* live recording put notes into the step about to play: */
    uint8_t rskip_n, rskip[NLANE];   /* do not trigger them again there (they sound already) */
    /* live recording of held notes (seq.c rec_hold): the steps they are held into become TIEs */
    uint8_t rh_n, rh_note[4];    /* recorded notes still held, 0 = none */
    uint8_t rh_start;            /* the step they were recorded into */
    uint8_t rh_ties;             /* TIE steps written after it */
    uint8_t rh_last;             /* the last of them; rh_bak: what it held (an early release puts it back) */
    step_t rh_bak;
    /* mono */
    uint8_t mono_stack[8];
    uint8_t nmono;
    uint8_t mono_note;           /* note the MONO / LEGATO / UNISON voice(s) play, 0 = none */
    uint8_t rr;                  /* POLY ROTATE: next voice to try */
    /* mix runtime */
    int32_t peak;
    int32_t dist_hp, dist_lp1, dist_lp2;   /* DIST insert state (fx.c) */
    int32_t dist_dc, dist_hold;  /* (JIANT) RECT's DC; CRUSH's held sample */
    uint8_t dist_hc;             /* .. CRUSH's hold count */
    int32_t ms_v;                /* (JIANT) the modulation sequence's level now (mod.c MS_STEP, Q15, slewed) */
    uint8_t tail;                /* blocks to mix after the last voice (the DIST tail) */
    int16_t armp, aholdp;        /* P_AMODE / P_AHOLD as last seen by the ISR */
    /* engine switch (voice.c engine_block): the old engine's voices fade out, then it switches */
    uint8_t xf_on, xf;           /* fading; blocks of the fade still to render */
    uint8_t xf_len, xf_dip;      /* (0.6.41) the fade's length; a dip (an engine's list value changed: the voices stay) */
    uint8_t xin, xin_len;        /* (0.6.41) fading in after it: blocks still to come, of xin_len */
    int16_t pe_old[8];           /* P_E0..P_E7 of the sounding engine: the fade renders with these */
    uint8_t xp_n, xp_note[4], xp_vel[4];   /* note-ons during the fade, played on the new engine */
    /* modulation matrix (mod.c): MIDI performance controllers of the track's channel, 0..127 */
    uint8_t mw, at;              /* CC1 mod wheel, channel aftertouch */
    uint8_t ex_off;              /* 127 - CC11 expression (0: full, the MIDI default) */
    uint8_t m_vel, m_key, m_vi;  /* the latest note-on: velocity, note, voice index (per-block destinations) */
    int16_t m_rnd;               /* .. its RAND */
    int32_t m_env;               /* the amp envelope of voice m_vi, last block (Q15) */
    /* the punch-in MIDI effects (pfx.c), the ISR's: what acts on this track this block, never saved */
    int8_t trn;                  /* ARP TRNS (seq.c): the semitones the keys set (from C4), on the steps' notes */
    uint8_t pfx;                 /* PFX_* */
    int16_t pfx_pit;             /* OCT- / OCT+: 1/16 semitones on every voice */
    int16_t pfx_rpit;            /* RANDOM: the next note's offset (1/16 semitones), drawn by trk_note_on */
    int8_t dpit;                 /* (0.6.82) the sequencer's next drum hits: their tune (semitones; a step's OCT, RAND,
                                  * TONAL, ARP SCALE on a kit) */
    int16_t dbend;               /* .. BEND: where the hit glides to over dbt blocks (1/16 semitones; eng_drum.c) */
    uint16_t dbt;
    uint8_t ss_n, ss_i;          /* (0.6.84) STRUM on the sequencer's chords: the step's notes still to come in, in */
    uint8_t ss_nn[4 + NLANE], ss_vv[4 + NLANE];  /* order low to high (seq.c seq_step, seq_tick), ss_due samples to the next */
    uint32_t ss_due, ss_gap;     /* (.. and the gap between them; 0.6.88: 32 bits, a long TIMING) */
    uint16_t ext;                /* (0.6.88) TIMING: samples the playing step began before its place (it is early) */
    int8_t mic;                  /* .. the playing step's own TIMING (ST_MICRO), late: seq_step queues its notes */
    uint8_t pfx_half;            /* 1/2 TEMPO: the odd sample carried; pfx_rs: let go, back to the real place */
    uint8_t pfx_rs;
    uint16_t pfx_idx0;           /* .. where it was pressed (step, samples into it) and the samples since */
    uint32_t pfx_pos0, pfx_el;
    uint8_t pfx_ln[4], pfx_lv[4], pfx_lnn;   /* the last notes struck together (a step's), STUTTER / ARP repeat them */
    uint32_t pfx_lblk;           /* .. the block they were struck in */
    uint8_t pfx_sn[4], pfx_snn;  /* the notes a repeat holds now */
    uint8_t pfx_si;              /* ARP: the next of them */
    uint32_t pfx_sc;             /* samples since the last repeat */
} track_t;
enum { PFX_RND = 1, PFX_HALF = 2, PFX_REP = 4, PFX_DEC = 8, PFX_ATK = 16 };   /* (track_t.pfx) */

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t grid;                /* 1: the keys are the DRUM grid (ui.c grid_on): only the lane keys play (seq.c);
                                  * 2: NAME (ui_name.c): no key plays */
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
} song_t;

static track_t trk[NTRK];        /* the instrument: four parts */
static song_t song;
#define TSEL (&trk[song.sel])    /* the selected track */

/* SWING of a track's step clock: the track's own plus the global one, at most 100 (#31). The sequencer
 * (seq.c step_samples) and the SLICER (slicer.c sl_enter) both time steps with it. At 100 the even
 * steps are 1.4 x the straight length and the odd ones 0.6 x. */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}
/* the length of step idx of a straight length `base`, swung: even steps longer, odd ones shorter */
static inline uint32_t swing_step_len(const track_t *t, uint32_t base, uint32_t idx)
{
    int32_t sw = track_swing(t) * (int32_t)base / 250;
    return base + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));
