/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: four slots. The slots live in .noinit RAM: they
 * survive resets and UBOOT entry. With FELUCCA_FLASH (default) every save
 * also goes to flash through storage.c, and an
 * empty RAM slot is filled from flash on load.
 *
 * Format 7 ("FUN7", written) stores P_COUNT (byte 66) and maps an older count as user presets do: FUN7 of
 * 89 parameters (before the chord keys P_CHRD / P_VOIC) loads its engine values at today's P_E0..P_E7,
 * the chord keys OFF / CLOSE, and its motion events' ids from its P_E0 on move up with them (proj_motion_ids).
 * 68 + 4 x (91 + 2 + 64 x 9) + chain + motion = 3040 of the 3372 bytes before the name: 332 spare, room for
 * 83 more track parameters (4 bytes each).
 * Format 6 ("FUN6") adds a 36-byte song chain before the checksum.
 * Format 5 ("FUN5", read only) = format 4 with the drum grid: a step is 10 bytes (step_t: its lane hits and
 * their accents after the 8 bytes it was), and each track has 40 reserved bytes (lane[8][5], written 0: room
 * for per-lane sounds of the DRUM engine). 4 ("FUN4": PROJ_NP_V4 parameters, the modulation matrix), 3
 * ("FUN3": PROJ_NP_V3, the SLICER), 2 ("FUN2") and 1 ("FUN1": PROJ_NP_V2) are read and converted, mapped by
 * count as user presets are (the first np - 8 are P_LEVEL.. in order, the last 8 P_E0..P_E7; the
 * parameters added since take their defaults: the SLICER OFF, every matrix slot OFF). Their steps get no
 * hits; on a DRUM track their notes that are a lane's note become its hits (proj_grid: the same notes and
 * velocities play, the grid shows them as its own).
 * Their engine bytes are kept: formats 1 and 2 had engines 0..7 (ANALOG .. WHEEL), and the engines
 * added since (SLICE 8, ..) were appended, no index moved.
 *
 * Track 4 was the GM drum part until 1.0 (no engine: its byte 0; its level and reverb send in the
 * globals G_DRLVL / G_DRREV, which are inert now). A project says which it has in `parts`: NPART
 * when written since, 0 before (a reserved byte, always written 0: the format and its size did not
 * change). proj_drums_to_part turns such a track 4 into a DRUM part with its default kit (the GM map, so
 * its drum steps still play drums; until 1.0.2 it was the SAMPLE engine's PERC set), keeping its steps, its
 * pattern and mix parameters (LEN DIV SWING GATE, PAN MUTE), its SLICER, and the drum level and reverb send
 * as LEVEL and REV.
 *
 * The byte `phys` (reserved, always 0, before 1.0) says what a PHYS track's MODEL means: 0 MODEL 2 was
 * DUST (dropped; PHYS itself is retired in TONIC: proj_phys_gone); 1 MODEL 4 was DRUM (the kit is the DRUM
 * engine since: such a track loads as DRUM, core.h drum_from_phys); 2 (PROJ_PHYS) as today. proj_phys.
 *
 * Format 8 ("FUN8", written since 1.0) = FUN7 with each track's FM6 patch (eng_fm6.c, the 128-byte packed
 * record, 4 x 128 bytes just before the name): a project is self-contained. On load an FM6 track's SLOT shows F n
 * when its patch is that factory one, else OWN (eng_fm6.c fm6_adopt; a stored 8..34, the B slots of the patch bank
 * before 1.0.3, is OWN too: the project has the patch).
 * It is 3584 bytes (FUN7: 3388); FUN7 is read (its tracks get the init patch). The retained cache (proj_slot,
 * .noinit) grew with it: after an update its slot 1 still starts with a FUN7 record, which is read; the other
 * slots fail their hash and come back from flash (persist_boot).
 *
 * Format 9 ("FUN9", written since 1.1) = FUN8 with 64 more bytes: the DRUM lane levels (P_LN0..P_LN7, #97) made
 * P_COUNT 99 (P_E0 91), and 68 + 4 x (99 + 2 + 576) + chain + motion = 3072 no longer fit before FUN8's patches
 * (3056). 3648 bytes, the patches at PROJ_FM6_OFF (3120): 48 spare, room for 12 more track parameters. FUN8 (3584,
 * its 91 parameters mapped by count: the lane levels 100 %) and FUN7 are read; the retained cache grew again (its
 * slot 1 still starts with the older record, read as above).
 *
 * Format A ("FUNA", written since JIANT 0.1) = FUN9 with 96 more bytes: the section's DRUM-X kit (drumx_voice.c
 * dx_lane_t[8], 88 bytes, then the group mutes, the sounds' mutes (dx_mute, 2 bytes), 1 = the SHIFT offsets (P_SOFS P_POFS, once the chord keys: 0 before it) and 5 reserved, written 0) at PROJ_DX_OFF, just before the FM6 patches. 3744 bytes (a
 * flash object holds 3840); the serialized part still ends by 3120 (FUN9's 48 spare bytes stay). FUN9 is read
 * (its kit: the factory one, DX_KIT_DEF), as every older format. JIANT 0.1 also keeps the punch-in lane (pfx.c) in
 * the 48 spare bytes before the kit: 64 nibbles at PROJ_PFX_OFF (3072), its tracks at 3104 (a FUN9 or an earlier
 * FUNA has zeros there: no lane), and the macros M1..M4 (mod.c macro_v, one byte each) right after, at 3105.
 *
 * Format B ("FUNB", written since JIANT 0.2) = FUNA with 96 more bytes (3840: a flash object's whole payload): 21 track
 * parameters before P_E0 (DIST TYPE TONE, the modulation sequence's LEN DIV SLEW and 16 levels: P_COUNT 120, P_E0 112; JIANT 0.5: the FILTER TYPE CUT RES, 123 and 115, no spare byte left),
 * so the data ends at 3156 and everything after it moved up by 96 (the punch-in block at PROJ_PFX_OFF 3168, the kit at
 * 3216); and nine globals past the header's 27 (G_DHPF .. G_DSPRY) after the macros: a 2 at PROJ_GX_OFF (1: the
 * first five, G_DHPF .. G_RPRE, saved by JIANT 0.2), then biased
 * bytes. FUNA (99 parameters, no new globals) and older load mapped by count, the new values their defaults.
 *
 * Parameter locks (1.1, core.h MOTION_LOCK) are motion records with bit 7 of their id byte set (P_COUNT 99 < 128:
 * the bit is free): no byte moved for them, and every project before 1.1 has none. FUN9 holds them. A FUN8 / FUN7
 * record of a 1.1 development build may hold some: on load their ids move to today's positions as the others do,
 * the lock bit kept (proj_motion_ids). Firmware before 1.1 refuses a project that holds a lock (an id out of its
 * range), as with RATCH below; FUN9 it refuses anyway (a newer format).
 *
 * A step's RATCH (core.h SF_RATCH, the hits - 1, 0..3) is in bit 7 of its velocity byte (bit 0) and of its chance
 * byte (bit 1) of FUN7 / FUN8: bits every firmware before wrote 0, so every older project loads with its steps x1,
 * and no format or size changed. Firmware before RATCH refuses a project that holds a ratchet (those bytes out
 * of their range), as it would a newer format. FUN6..FUN1 have none (x1).
 *
 * DIGITAL (engine 1) was retired in 1.0 (fm4_convert.c): a track of it, in any format, loads as FM6 with the
 * patch converted from its values as the track's own (proj_fm4, on every import; the stored record keeps what it
 * holds until saved again). Its motion events on the EDIT or OP ENV values are dropped.
 *
 * SAMPLE's SET 4, PERC (the GM drum kit), was retired after 1.0.2: a SAMPLE track that selects it, in any
 * format, loads as the DRUM engine with its default kit (core.h drum_from_perc: the same GM key map, its steps
 * as they are, the rest of its sound kept; proj_perc, on every import as proj_fm4). Its motion events on the
 * EDIT values are dropped (SAMPLE's meanings, not DRUM's).
 *
 * Built on the Mac too (tests/project_test.c, -DPROJ_HOST): the part above the #ifndef
 * PROJ_HOST needs core.h, params.c (TP) and engines.c. */
#define PROJ_MAGIC 0x46554E43u                 /* "FUNC" (JIANT 0.6): six tracks, the steps packed (see proj_pack) */
#define PROJ_MAGIC_VB 0x46554E42u                 /* "FUNB": FUNA, 96 bytes longer (JIANT: DIST TYPE TONE, the modulation
                                                * sequence: 120 parameters; the buses' new globals) */
#define PROJ_MAGIC_VA 0x46554E41u              /* "FUNA": FUN9 + the DRUM-X kit (JIANT 0.1) */
#define PROJ_MAGIC_V9 0x46554E39u              /* "FUN9": FUN8, 64 bytes longer (the DRUM lane levels, 99 parameters) */
#define PROJ_MAGIC_V8 0x46554E38u              /* "FUN8": FUN7 + the tracks' FM6 patches */
#define PROJ_MAGIC_V7 0x46554E37u              /* "FUN7": serialized (byte params, packed steps), chain, motion */
#define PROJ_MAGIC_V6 0x46554E36u              /* FUN6: 69 parameters, drum grid, chain */
#define PROJ_MAGIC_V5 0x46554E35u              /* "FUN5": the grid, without the chain; read only */
#define PROJ_MAGIC_V4 0x46554E34u              /* "FUN4": four tracks, PROJ_NP_V4 parameters, 8-byte steps; read only */
#define PROJ_MAGIC_V3 0x46554E33u              /* "FUN3": four tracks, PROJ_NP_V3 parameters; read only */
#define PROJ_MAGIC_V2 0x46554E32u              /* "FUN2": four tracks, PROJ_NP_V2 parameters; read only */
#define PROJ_MAGIC_V1 0x46554E31u              /* "FUN1": one instrument; loads into track 1 */
#define PROJ_NP_V2 53u                         /* P_COUNT of formats 1 and 2 (P_E0 was 45) */
#define PROJ_NG_V2 27u                         /* G_COUNT of formats 1 and 2 */
#define PROJ_NP_V3 57u                         /* P_COUNT of format 3 (P_E0 was 49) */
#define PROJ_NP_V4 69u                         /* P_COUNT of format 4 (P_E0 61) */
#define PROJ_LT 4u                             /* the tracks of every format before FUNC */
#define PROJ_PHYS 2u                           /* project_t.phys: PHYS without DUST and DRUM (see the top) */
#define PROJ_NAME_LEN 12u                      /* the name: FUN7 bytes PROJ_NAME_OFF.. (the reserved tail's end) */
typedef struct {                               /* one track */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
} proj_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel;                               /* the selected track */
    uint8_t parts;                             /* NPART; 0: track 4 is the old GM drum part (see the top) */
    uint8_t phys;                              /* PROJ_PHYS: PHYS MODEL values as today; 1: MODEL 4 was DRUM;
                                                * 0 (a reserved byte before 1.0): MODEL 2 was DUST */
    uint8_t rsv;
    proj_trk_t t[NTRK];
    chain_config_t chain;
    motion_store_t motion;
    uint8_t fm6[NTRK][FM6_PACKED];             /* each track's FM6 patch, packed (eng_fm6.c) */
    dx_lane_t dx[8];                           /* the DRUM-X kit (drumx_voice.c) */
    uint16_t dx_mute;                          /* its mutes (eng_drum.c: the groups DXG_* in the first reserved byte, the
                                                * sounds' DXM_LANE in the second) */
    uint8_t dx_mot[4];                         /* (0.5.1) the kit's X-MOD (eng_drum.c), in its reserved bytes 3..6 */
    uint8_t dx_tone;                           /* (0.6.36) the kit's TONE (drumx_voice.c: BP, FEEDBK), its byte 7 */
    uint8_t dx_snd[8];                         /* (0.6.48) its sounds' SENDs (eng_drum.c): FUNC's 'D' 'S' block, 0 before */
    uint8_t dx_lay[8];                         /* (0.6.49) its sounds' LAYER, FEEDBK (drumx_voice.c): 'D' 'L', 0 before */
    uint8_t dx_mdl[8];                         /* (1.0-GW) its sounds' MODEL, CLIP (drumx_voice.c): 'D' 'X', 0 before */
    uint8_t fugue[NTRK][3][2];                 /* (0.6.52) SEQ > FUGUE's heads (fugue.c): RATE | DIR << 4, INTERVAL (int8);
                                                * FUNC's 'F' 'G' block, none before */
    uint8_t pfx_lane[32], pfx_ltgt;            /* the punch-in lane (pfx.c), in FUN9's spare bytes (PROJ_PFX_OFF) */
    uint8_t macro[4];                          /* the macros M1..M4 (mod.c macro_v), 0..127: after the lane's tracks */
    char name[PROJ_NAME_LEN];                  /* the project's name: upper-case ASCII 32..126, 0-padded; "" = none */
    uint32_t sum;
} project_t;
/* Historical FUN5/6 types are frozen, independent of today's P_COUNT/step_t. */
typedef struct { uint8_t note[4], n, time, flags, vel, hit, acc; } step10_t;
typedef struct { int16_t p[69]; uint8_t engine, preset; step10_t step[NSTEP]; uint8_t lane[NLANE][5]; } proj_trk_v5_t;
typedef struct {                               /* format 5, before the song chain */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, parts, phys, rsv;
    proj_trk_v5_t t[PROJ_LT];
    uint32_t sum;
} project_v5_t;
typedef struct { uint32_t magic, size; int16_t g[PROJ_NG_V2]; uint8_t sel, parts, phys, rsv;
    proj_trk_v5_t t[PROJ_LT]; chain_config_t chain; uint32_t sum; } project_v6_t;
_Static_assert(sizeof(project_v5_t) == 3352u && sizeof(project_v6_t) == 3388u, "frozen formats 5 / 6 sizes");
/* Serialized FUN7 keeps the retained cache's exact extent. Params are biased
 * bytes, steps pack n/time/flags. Reserved tail is zero and covered by hash.
 * The tail's last 12 bytes (PROJ_NAME_OFF, just before the hash) are the project's name since 1.0:
 * ASCII 32..126 (upper case), 0-padded, all 0 = no name ("PROJECT A"). Firmware before wrote them 0 and
 * never reads them, so every FUN7 file stays valid both ways; FUN6..FUN1 imports get no name.
 * FUN8: the same, 3584 bytes, the four packed FM6 patches at PROJ_FM6_OFF (before the name); 16 bytes of the
 * reserved tail were left for parameters added later. FUN9: the same, 3648 bytes (see the top). */
#define PROJ_STORE_SIZE 3840u                  /* FUNB (a flash object's whole payload) */
#define PROJ_STORE_VA 3744u                    /* FUNA */
#define PROJ_STORE_V9 3648u                    /* FUN9 */
#define PROJ_STORE_V8 3584u                    /* FUN8 */
#define PROJ_STORE_V7 3388u                    /* FUN7 */
#define PROJ_NAME_OFF (PROJ_STORE_SIZE - 4u - PROJ_NAME_LEN)
#define PROJ_FM6_OFF (PROJ_NAME_OFF - PROJ_LT * FM6_PACKED)   /* (FUNB's layout: the tail kept by FUNC) */
#define PROJ_DX_SIZE 96u                       /* the kit, 88 bytes, and 8 reserved */
#define PROJ_DX_OFF (PROJ_FM6_OFF - PROJ_DX_SIZE)
#define PROJ_PFX_OFF (PROJ_DX_OFF - 48u)        /* the punch-in lane (32) and its tracks (1): FUN9's 48 spare bytes, 0 in
                                                 * every FUN9 / FUNA written before it (proj_pack clears the store) */
typedef union { uint32_t align; uint8_t raw[PROJ_STORE_SIZE]; } project_store_t;
_Static_assert(G_NSTORE == 27u && G_NSTORE == PROJ_NG_V2, "FUN7 globals retain original IDs");
_Static_assert(sizeof(project_store_t) == 3840u && PROJ_STORE_V7 == sizeof(project_v6_t), "FUNB / FUN7 sizes");
_Static_assert(sizeof(dx_lane_t[8]) == 88u && PROJ_DX_OFF == 3216u, "the kit's place: FUNA's, 96 bytes on");
#define PROJ_GX_OFF (PROJ_PFX_OFF + 37u)        /* (FUNB) after the lane, its tracks and the macros: 1, then the globals */
#define G_GX_END G_SLMODE                       /* (0.6.3: the SLICER's globals after them, in their own block) */
_Static_assert(37u + 1u + (G_GX_END - G_NSTORE) <= 48u, "the new globals in the punch-in block's spare bytes");
_Static_assert(G_GX_END - G_NSTORE == 10u, "marker 3 holds ten new globals");
_Static_assert(68u + PROJ_LT * (P_COUNT + 2u + NSTEP * 9u) + sizeof(chain_config_t) + sizeof(motion_store_t) <= PROJ_PFX_OFF,
               "the punch-in lane after the serialized part");
typedef struct {                               /* a track of format 4, read only */
    int16_t p[PROJ_NP_V4];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v4_t;
typedef struct {                               /* format 4 (1.0 development builds), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, parts, phys, rsv;
    proj_trk_v4_t t[PROJ_LT];
    uint32_t sum;
} project_v4_t;
typedef struct {                               /* a track of format 3, read only */
    int16_t p[PROJ_NP_V3];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v3_t;
typedef struct {                               /* format 3 (0.9 .. 1.0), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, parts, rsv[2];
    proj_trk_v3_t t[PROJ_LT];
    uint32_t sum;
} project_v3_t;
#define PROJ_DEF_SOUND 0xFFu                   /* preset byte: the track's power-on sound, no steps (format 1) */
#define PROJ_DEF_KEEP 0xFEu                    /* .. DRUM's kit (once SAMPLE PERC), steps and the rest kept (old drums) */
typedef struct {                               /* a track of formats 1 and 2, read only */
    int16_t p[PROJ_NP_V2];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v2_t;
typedef struct {                               /* format 2 (until 0.9), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, rsv[3];
    proj_trk_v2_t t[PROJ_LT];
    uint32_t sum;
} project_v2_t;
typedef struct {                               /* format 1 (until 0.5 beta), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    proj_trk_v2_t t;
    uint32_t sum;
} project_v1_t;
_Static_assert(sizeof(project_v2_t) == 2552u && sizeof(project_v1_t) == 688u && sizeof(project_v3_t) == 2584u &&
               sizeof(project_v4_t) == 2680u, "formats 1 / 2 / 3 / 4 as they were stored");
project_store_t proj_slot[4] __attribute__((section(".noinit")));

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC && q->size == sizeof *q &&
    q->sum == proj_sum(q) && chain_valid(&q->chain) && motion_valid(&q->motion); }

/* G_RTYPE (id 24) was G_DRCH, the GM drum part's MIDI channel (0..16, 10 by default) until 1.0: a project of a
 * format before FUN7 may hold any channel there. FUN7 came after it was inert (always 0), so only those imports
 * set it: ROOM, the only reverb they knew */
static void proj_rtype_room(int16_t *g) { g[G_RTYPE] = 0; }

/* the globals of formats 1 and 2 (G_* unchanged since; any added later: their defaults) */
static void proj_g_from_v2(int16_t *g, const int16_t *g2)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        g[i] = i < PROJ_NG_V2 ? g2[i] : GP[i].def;
    proj_rtype_room(g);
}

/* np stored parameters, engine, preset and steps of an older track -> today's, mapped by count (see the top);
 * the steps without hits */
static void proj_trk_from(proj_trk_t *d, const int16_t *p, uint32_t np, uint8_t engine, uint8_t preset, const step8_t *step)
{
    int16_t def[P_E0];
    uint32_t k;
    for (k = 0; k < P_E0; k++)
        def[k] = TP[k].def;
    params_by_count(d->p, p, np, def);
    d->engine = engine;                         /* (indices 0..7 of formats 1 and 2 as they were) */
    d->preset = preset;
    for (k = 0; k < NSTEP; k++) {
        step_t *s = &d->step[k];
        memcpy(s->note, step[k].note, 4);
        s->n = step[k].n;
        s->time = step[k].time;
        s->flags = step[k].flags & 3u;          /* accent, slide: x1 (no RATCH then) */
        s->vel = step[k].vel;
        s->hit = s->acc = 0;
    }
}

/* the DRUM tracks of a project of before the grid: their lanes' notes as hits (see the top) */
static void proj_grid(project_t *q)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++)
        if (q->t[k].engine == ENGI_DRUM)
            for (i = 0; i < NSTEP; i++)
                step_to_grid(&q->t[k].step[i]);
    q->sum = proj_sum(q);
}
static void proj_trk_from_v2(proj_trk_t *d, const proj_trk_v2_t *s)
{
    proj_trk_from(d, s->p, PROJ_NP_V2, s->engine, s->preset, s->step);
}
/* (JIANT 0.6) the tracks from `from` on of a project read from a format of PROJ_LT tracks: their power-on sounds
 * (PROJ_DEF_SOUND: project_restore_runtime gives TRK_DEF's), no steps */
static void proj_new_tracks(project_t *q, uint32_t from)
{
    uint32_t i, k;
    for (i = from; i < NTRK; i++) {
        for (k = 0; k < P_COUNT; k++)
            q->t[i].p[k] = param_desc_of(trk_def_engine(i), k)->def;
        q->t[i].engine = (uint8_t)trk_def_engine(i);
        q->t[i].preset = PROJ_DEF_SOUND;
        for (k = 0; k < NSTEP; k++) {
            memset(&q->t[i].step[k], 0, sizeof q->t[i].step[k]);
            q->t[i].step[k].time = ST_REST;
        }
    }
}

/* a project written with the GM drum part as track 4 (parts 0) -> track 4 a part (see the top);
 * project_load gives it DRUM's kit (PROJ_DEF_KEEP; the SAMPLE PERC sound until 1.0.2). Idempotent */
static void proj_drums_to_part(project_t *q)
{
    proj_trk_t *d = &q->t[PROJ_LT - 1u];
    if (q->parts >= PROJ_LT)                    /* (4: a FUN7 .. FUNB, NPART: today's) */
        return;
    d->engine = ENGI_DRUM;                      /* DRUM's first kit: independent of today's power-on drum sound */
    d->preset = PROJ_DEF_KEEP;
    d->p[P_LEVEL] = (int16_t)clamp(q->g[G_DRLVL], 0, 127);   /* the drum part's level and reverb send */
    d->p[P_REV] = (int16_t)clamp(q->g[G_DRREV], 0, 127);
    q->g[G_DRLVL] = q->g[G_DRREV] = 0;          /* (their ids are PUNCH and DUCK now: off) */
    q->parts = NPART;
    q->sum = proj_sum(q);
}

/* PHYS tracks of a project written before PROJ_PHYS (see the top) -> today's: DUST as MODAL bowed, MODEL
 * DRUM as the DRUM engine (its E values moved, the preset its first). Idempotent */
static void proj_phys(project_t *q)
{
    uint32_t k;
    if (q->phys >= PROJ_PHYS)
        return;
    for (k = 0; k < NTRK; k++) {
        proj_trk_t *d = &q->t[k];
        if (d->engine != ENGI_PHYS)
            continue;
        if (drum_from_phys(d->engine, &d->p[P_E0])) {
            d->engine = ENGI_DRUM;
            d->preset = 0;
        }
    }
    q->phys = PROJ_PHYS;
    q->sum = proj_sum(q);
}

/* DIGITAL tracks (engine 1; without FELUCCA_FM4) -> FM6 with the converted patch as the track's own (fm4_convert.c,
 * whatever format the project is: the conversion runs on every load, the stored record keeps what it holds until it
 * is saved again). Their motion events on the EDIT values or the OP ENV values go: DIGITAL's meanings do not carry
 * over to FM6's macros. Idempotent */
static void proj_fm4(project_t *q)
{
#if !FELUCCA_FM4
    uint32_t k, i, n, hit = 0;
    for (k = 0; k < NTRK; k++) {
        proj_trk_t *d = &q->t[k];
        uint8_t v[FP_SIZE + 1u];
        uint32_t pr;
        if (d->engine != ENGI_DIGITAL)
            continue;
        for (i = 0; i < P_COUNT; i++)
            d->p[i] = (int16_t)clamp(d->p[i], param_desc_of(ENGI_DIGITAL, i)->min, param_desc_of(ENGI_DIGITAL, i)->max);
        pr = fm4_convert(d->p, v);
        fm6_pack(v, q->fm6[k]);
        d->engine = ENGI_FM6;
        if (d->preset < PROJ_DEF_KEEP)
            d->preset = (uint8_t)pr;
        hit |= 1u << k;
    }
    if (!hit)
        return;
    for (i = n = 0; i < q->motion.count && i < MOTION_MAX; i++) {
        const motion_event_t *e = &q->motion.event[i];
        if (((hit >> MOTION_TRK(e)) & 1u) && (MOTION_ID(e) >= P_E0 || (MOTION_ID(e) >= P_FM1_ATK && MOTION_ID(e) <= P_FM4_LEVEL)))
            continue;
        q->motion.event[n++] = *e;
    }
    for (i = n; i < q->motion.count && i < MOTION_MAX; i++)
        memset(&q->motion.event[i], 0, sizeof q->motion.event[i]);
    q->motion.count = (uint8_t)n;
    q->sum = proj_sum(q);
#else
    (void)q;
#endif
}

/* tracks of a retired engine (PHYS 9 in TONIC; SAMPLE 4, GRAIN 8, SLICE 13 in JIANT: core.h eng_gone) -> ANALOG with its first preset's EDIT values
 * and envelope; the mix, the sends, the matrix and the steps stay. Their motion events on the EDIT values go (PHYS's
 * meanings do not carry over). After proj_phys and proj_perc (a PHYS DRUM of before 1.0, a SAMPLE PERC are DRUM by
 * then). Idempotent */
static void proj_phys_gone(project_t *q)
{
    const preset_t *pr = &ENGINES[ENGI_PHYS_TO]->presets[0];
    uint32_t k, i, n, hit = 0;
    for (k = 0; k < NTRK; k++) {
        proj_trk_t *d = &q->t[k];
        if (!eng_gone(d->engine))
            continue;
        if (analog_from_trio(d->engine, &d->p[P_E0])) { /* (JIANT 0.4) TRIO: ANALOG's nearest, its envelope kept */
            d->engine = ENGI_PHYS_TO;
            if (d->preset < PROJ_DEF_KEEP)
                d->preset = 0;
            hit |= 1u << k;
            continue;
        }
        d->engine = ENGI_PHYS_TO;
        for (i = 0; i < 8u; i++)
            d->p[P_E0 + i] = (int16_t)pr->e[i];
        d->p[P_ATK] = pr->env[0];
        d->p[P_DEC] = pr->env[1];
        d->p[P_SUS] = pr->env[2];
        d->p[P_REL] = pr->env[3];
        if (d->preset < PROJ_DEF_KEEP)
            d->preset = 0;
        hit |= 1u << k;
    }
    if (!hit)
        return;
    for (i = n = 0; i < q->motion.count && i < MOTION_MAX; i++) {
        const motion_event_t *e = &q->motion.event[i];
        if (((hit >> MOTION_TRK(e)) & 1u) && MOTION_ID(e) >= P_E0)
            continue;
        q->motion.event[n++] = *e;
    }
    for (i = n; i < q->motion.count && i < MOTION_MAX; i++)
        memset(&q->motion.event[i], 0, sizeof q->motion.event[i]);
    q->motion.count = (uint8_t)n;
    q->sum = proj_sum(q);
}

/* SAMPLE tracks of the retired PERC set (SET 4) -> DRUM with its default kit (see the top; core.h drum_from_perc,
 * whatever format the project is: on every load, the stored record keeps what it holds until it is saved again).
 * Their motion events on the EDIT values go. Idempotent */
static void proj_perc(project_t *q)
{
    uint32_t k, i, n, hit = 0;
    for (k = 0; k < NTRK; k++) {
        proj_trk_t *d = &q->t[k];
        if (!drum_from_perc(d->engine, &d->p[P_E0]))
            continue;
        d->engine = ENGI_DRUM;
        if (d->preset < PROJ_DEF_KEEP)
            d->preset = 0;
        hit |= 1u << k;
    }
    if (!hit)
        return;
    for (i = n = 0; i < q->motion.count && i < MOTION_MAX; i++) {
        const motion_event_t *e = &q->motion.event[i];
        if (((hit >> MOTION_TRK(e)) & 1u) && MOTION_ID(e) >= P_E0)
            continue;
        q->motion.event[n++] = *e;
    }
    for (i = n; i < q->motion.count && i < MOTION_MAX; i++)
        memset(&q->motion.event[i], 0, sizeof q->motion.event[i]);
    q->motion.count = (uint8_t)n;
    q->sum = proj_sum(q);
}

/* a format 4 project (n bytes in *v4) -> slot q as format 6 */
static int proj_from_v4(project_t *q, const project_v4_t *v4, int n)
{
    uint32_t i;
    if (n != (int)sizeof *v4 || v4->magic != PROJ_MAGIC_V4 || v4->size != sizeof *v4 ||
        v4->sum != proj_hash(v4, sizeof *v4 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_v2(q->g, v4->g);
    proj_rtype_room(q->g);
    q->sel = v4->sel;
    q->parts = v4->parts;
    q->phys = v4->phys;
    for (i = 0; i < PROJ_LT; i++)
        proj_trk_from(&q->t[i], v4->t[i].p, PROJ_NP_V4, v4->t[i].engine, v4->t[i].preset, v4->t[i].step);
    proj_new_tracks(q, PROJ_LT);
    q->sum = proj_sum(q);
    proj_drums_to_part(q);
    return 1;
}

/* a format 3 project (n bytes in *v3) -> slot q as format 6 */
static int proj_from_v3(project_t *q, const project_v3_t *v3, int n)
{
    uint32_t i;
    if (n != (int)sizeof *v3 || v3->magic != PROJ_MAGIC_V3 || v3->size != sizeof *v3 ||
        v3->sum != proj_hash(v3, sizeof *v3 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_v2(q->g, v3->g);
    proj_rtype_room(q->g);
    q->sel = v3->sel;
    q->parts = v3->parts;
    for (i = 0; i < PROJ_LT; i++)
        proj_trk_from(&q->t[i], v3->t[i].p, PROJ_NP_V3, v3->t[i].engine, v3->t[i].preset, v3->t[i].step);
    proj_new_tracks(q, PROJ_LT);
    q->sum = proj_sum(q);
    proj_drums_to_part(q);                     /* (a format 3 of firmware before 1.0: parts 0) */
    return 1;
}

/* a format 2 project (n bytes in *v2) -> slot q as format 6 */
static int proj_from_v2(project_t *q, const project_v2_t *v2, int n)
{
    uint32_t i;
    if (n != (int)sizeof *v2 || v2->magic != PROJ_MAGIC_V2 || v2->size != sizeof *v2 ||
        v2->sum != proj_hash(v2, sizeof *v2 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_v2(q->g, v2->g);
    q->sel = v2->sel;
    for (i = 0; i < PROJ_LT; i++)
        proj_trk_from_v2(&q->t[i], &v2->t[i]);
    proj_new_tracks(q, PROJ_LT);
    proj_drums_to_part(q);                     /* (format 2 had the drum track: parts 0) */
    return 1;
}

/* a format 1 project (n bytes in *v1) -> slot q as format 6: the instrument becomes track 1,
 * tracks 2..4 start empty (their sounds as at power-on) */
static int proj_from_v1(project_t *q, const project_v1_t *v1, int n)
{
    uint32_t i;
    if (n != (int)sizeof *v1 || v1->magic != PROJ_MAGIC_V1 || v1->size != sizeof *v1 ||
        v1->sum != proj_hash(v1, sizeof *v1 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_v2(q->g, v1->g);
    q->parts = NPART;                          /* (format 1 had no track 4) */
    proj_trk_from_v2(&q->t[0], &v1->t);
    (void)i;
    proj_new_tracks(q, 1u);                    /* the other tracks: their defaults, no steps */
    q->sum = proj_sum(q);
    return 1;
}

/* n bytes of a stored project (any format) -> slot q as format 6; 0 = not a project */
static int proj_unpack(project_t *q, const uint8_t *b, uint32_t size);
/* the init patch on every track (a project of a format before FUN8), the factory DRUM-X kit (before FUNA) */
static void proj_fm6_init(project_t *q)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        memcpy(q->fm6[t], FM6_INIT, FM6_PACKED);
    memcpy(q->dx, DX_KIT_DEF, sizeof q->dx);
    q->dx_mute = 0;
    memset(q->dx_mot, 0, sizeof q->dx_mot);
    q->dx_tone = 0;
    memset(q->dx_snd, 0, sizeof q->dx_snd);
    memcpy(q->dx_lay, DX_LAY_DEF, sizeof q->dx_lay);   /* (0.6.49: a new project: the factory kit's) */
    memset(q->dx_mdl, 0, sizeof q->dx_mdl);            /* (1.0-GW: every sound CLASSIC) */
    memset(q->fugue, 0, sizeof q->fugue);              /* (0.6.52: no FUGUE) */
    q->sum = proj_sum(q);
}
static int proj_import_old(project_t *q, const void *b, int n);
static int proj_import_any(project_t *q, const void *b, int n);
static int proj_unpack_c(project_t *q, const uint8_t *b);
/* every track's engine one this firmware has (DIGITAL, 1, is one: it converts): a project of another number
 * is not one (it never was: no engine number past the last was ever stored) */
static int proj_engines_ok(const project_t *q)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        if (q->t[t].engine >= NENGINES)
            return 0;
    return 1;
}
/* n bytes of a stored project (any format) -> q as today's, DIGITAL and SAMPLE PERC tracks converted; 0 = not a
 * project */
/* (JIANT 0.6.3) a project of before the SLICER's bus (no block of it): the first track's SLICER that was on is the bus's
 * (its mode, pattern, rate, depth), the tracks whose SLICER was on go in, the effects' returns not (the old one cut the
 * dry sound before the sends); none on: the bus at its defaults (OFF, everything in) */
static uint8_t proj_sl_found;                           /* proj_unpack_c read the SLICER's block */
/* (JIANT 0.6.9) the old SLICER's slots are AUTO PAN's now (core.h): read by the migration, then AUTO PAN off */
static void proj_ap_clear(project_t *q)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        q->t[i].p[P_APDEP] = TP[P_APDEP].def;
        q->t[i].p[P_APRATE] = TP[P_APRATE].def;
        q->t[i].p[P_APWAVE] = TP[P_APWAVE].def;
        q->t[i].p[P_APSYNC] = TP[P_APSYNC].def;
    }
}
static void proj_sl_migrate(project_t *q)
{
    uint32_t i, first = NTRK;
    for (i = 0; i < G_SL_N; i++)
        q->g[G_SLMODE + i] = GP[G_SLMODE + i].def;
    for (i = 0; i < NTRK; i++)
        if (q->t[i].p[P_SLCR] > 0 && q->t[i].p[P_SLCR] <= 2 && first == NTRK)
            first = i;
    if (first == NTRK) {
        proj_ap_clear(q);
        return;
    }
    q->g[G_SLMODE] = q->t[first].p[P_SLCR];
    q->g[G_SLPAT] = (int16_t)clamp(q->t[first].p[P_SLPAT], 1, 16);
    q->g[G_SLRATE] = (int16_t)clamp(q->t[first].p[P_SLRATE], 0, 5);
    q->g[G_SLDEP] = (int16_t)clamp(q->t[first].p[P_SLDEPTH], 0, 127);
    for (i = 0; i < NTRK; i++)
        q->g[G_SLT1 + i] = (int16_t)(q->t[i].p[P_SLCR] > 0 && q->t[i].p[P_SLCR] <= 2);
    q->g[G_SLFX] = 0;
    proj_ap_clear(q);
}
static int proj_import(project_t *q, const void *b, int n)
{
    proj_sl_found = 0;
    if (!proj_import_any(q, b, n) || !proj_engines_ok(q))
        return 0;
    if (!proj_sl_found) {
        proj_sl_migrate(q);
        q->sum = proj_sum(q);
    }
    proj_fm4(q);
    proj_perc(q);                                       /* (SAMPLE PERC: DRUM, before the retired engines) */
    proj_phys_gone(q);
    return 1;
}
static int proj_import_any(project_t *q, const void *b, int n)
{
    if (n == PROJ_STORE_SIZE && ((const uint32_t *)b)[0] == PROJ_MAGIC)
        return proj_unpack_c(q, b);                     /* (JIANT 0.6: six tracks, packed) */
    if (n == PROJ_STORE_SIZE && ((const uint32_t *)b)[0] == PROJ_MAGIC_VB)
        return proj_unpack(q, b, PROJ_STORE_SIZE);
    if (n == PROJ_STORE_SIZE && ((const uint32_t *)b)[1] >= 8u && ((const uint32_t *)b)[1] < PROJ_STORE_SIZE)
        n = (int)((const uint32_t *)b)[1];      /* a retained slot holding an older, shorter record: its own size
                                                 * (every format checks its magic and hash) */
    if (n == (int)PROJ_STORE_VA && ((const uint32_t *)b)[0] == PROJ_MAGIC_VA)
        return proj_unpack(q, b, PROJ_STORE_VA);      /* (JIANT 0.1: 99 parameters, no new globals) */
    if (n == (int)PROJ_STORE_V9 && ((const uint32_t *)b)[0] == PROJ_MAGIC_V9)
        return proj_unpack(q, b, PROJ_STORE_V9);
    if (n == (int)PROJ_STORE_V8 && ((const uint32_t *)b)[0] == PROJ_MAGIC_V8)
        return proj_unpack(q, b, PROJ_STORE_V8);
    if (n == (int)PROJ_STORE_V7 && ((const uint32_t *)b)[0] == PROJ_MAGIC_V7)
        return proj_unpack(q, b, PROJ_STORE_V7);
    if (n == (int)sizeof *q && proj_ok((const project_t *)b)) {
        memcpy(q, b, sizeof *q);
        proj_sl_found = 1;                              /* (today's own: its SLICER globals) */
        proj_drums_to_part(q);
        proj_phys(q);
        return 1;
    }
    if (!proj_import_old(q, b, n))
        return 0;
    proj_fm6_init(q);
    {
        uint32_t t;
        for (t = 0; t < NTRK; t++)                      /* (JIANT 0.4) LOFI's, ANALOG's E values as today's */
            sound_v04(q->t[t].engine, &q->t[t].p[P_E0]);
        q->sum = proj_sum(q);
    }
    return 1;
}
static int proj_import_old(project_t *q, const void *b, int n)
{
    if (n == (int)sizeof(project_v5_t) || n == (int)sizeof(project_v6_t)) {
        const project_v5_t *v = b;
        const project_v6_t *v6 = b;
        uint32_t bytes = n, i, k;
        if (((bytes == sizeof *v && v->magic == PROJ_MAGIC_V5) ||
             (bytes == sizeof *v6 && v->magic == PROJ_MAGIC_V6 && chain_valid(&v6->chain))) &&
            v->size == bytes && ((const uint32_t *)b)[bytes / 4u - 1u] == proj_hash(b, bytes - 4u)) {
            memset(q, 0, sizeof *q);
            q->magic = PROJ_MAGIC; q->size = sizeof *q;
            proj_g_from_v2(q->g, v->g);
            proj_rtype_room(q->g);
            q->sel = v->sel; q->parts = v->parts; q->phys = v->phys;
            for (i = 0; i < PROJ_LT; i++) {
                int16_t def[P_COUNT];
                if (v->t[i].engine >= NENGINES) return 0;
                for (k = 0; k < P_COUNT; k++) def[k] = param_desc_of(v->t[i].engine, k)->def;
                params_by_count(q->t[i].p, v->t[i].p, 69u, def);
                q->t[i].engine = v->t[i].engine; q->t[i].preset = v->t[i].preset;
                for (k = 0; k < NSTEP; k++) {
                    memcpy(&q->t[i].step[k], &v->t[i].step[k], sizeof(step10_t));
                    q->t[i].step[k].flags &= 3u;       /* (x1: no RATCH then) */
                }
            }
            proj_new_tracks(q, PROJ_LT);
            if (bytes == sizeof *v6) q->chain = v6->chain; else chain_defaults(&q->chain);
            q->sum = proj_sum(q); proj_drums_to_part(q); proj_phys(q);
            return 1;
        }
    }
    if (proj_from_v4(q, (const project_v4_t *)b, n) || proj_from_v3(q, (const project_v3_t *)b, n) ||
        proj_from_v2(q, (const project_v2_t *)b, n) || proj_from_v1(q, (const project_v1_t *)b, n)) {
        proj_phys(q);                          /* (formats 1..3 had no PHYS track: only the byte) */
        proj_grid(q);                          /* (after it: a PHYS DRUM track is DRUM now) */
        return 1;
    }
    return 0;
}

/* 12 stored name bytes -> d (PROJ_NAME_LEN + 1): up to the first 0, upper case; a byte outside 32..126 makes it
 * no name (a damaged tail never refuses the project) */
static uint32_t proj_name_get(char *d, const uint8_t *s)   /* its length */
{
    uint32_t i;
    for (i = 0; i < PROJ_NAME_LEN && s[i]; i++) {
        if (s[i] < 32u || s[i] > 126u) { i = 0; break; }
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32u : s[i]);
    }
    d[i] = 0;
    return i;
}

/* A stable serialized schema: first header retains FUN6's fields; at byte66
 * np, format flags, then four byte-param tracks and nine-byte steps; FUN8: the patches at PROJ_FM6_OFF (FUNB:
 * proj_unpack reads them all; JIANT 0.6 writes FUNC, below). */
/* (JIANT 0.6) FUNC: six tracks in FUNB's 3840 bytes (a flash object's payload, a section slot), packed. Bytes 0..67
 * as FUNB's (b[66] the parameters stored, np; b[67] the tracks: 6); then each track: engine, preset; a byte, 1 when its
 * FM6 patch follows (128 bytes: an FM6 track's, or one not the init patch); a map of its parameters that are not the
 * engine's default ((np + 7) / 8 bytes) and those alone (biased bytes, +64); a 64-bit map of its steps that are not
 * empty (step_clear's) and those alone, each: n | time << 3 | flags << 5 | X << 7; with X a byte x (bit 0 hit and
 * accent follow, bit 1 the chance follows, bit 2 RATCH bit 1, bit 3 all four notes); its n notes (4 with x's bit 3);
 * velocity | RATCH bit 0 << 7; hit, accent; chance. Then the chain, the motion's 4 header bytes and its records, (0.6.3) 'S' 'L' and the SLICER's globals. The tail at fixed places
 * from the end: the punch-in lane, the macros and the new globals (PROJC_PFX_OFF), the DRUM-X kit (PROJC_DX_OFF), the
 * name, the hash. A project too full to fit: proj_pack 0 and proj_full set (SECTION FULL) */
#define PROJC_DX_OFF (PROJ_NAME_OFF - PROJ_DX_SIZE)
#define PROJC_PFX_OFF (PROJC_DX_OFF - 48u)
#define PROJC_GX_OFF (PROJC_PFX_OFF + 37u)
#define PROJC_PMAP ((P_COUNT + 7u) / 8u)
static uint8_t proj_full;                               /* the last proj_pack failed for room (not for a value) */
static int proj_step_empty(const step_t *s)
{
    return !s->n && s->time == ST_REST && !s->flags && !s->vel && !s->hit && !s->acc && !s->probability && !s->comp &&
           !s->comp2 && !s->cwhen && !s->ctime;
}
static uint32_t proj_step_x(const step_t *s, uint32_t *nn)   /* its x byte (0: none), *nn the notes stored */
{
    uint32_t r = step_ratchet(s) - 1u, j, all = 0;
    for (j = s->n; j < 4u; j++)
        all |= s->note[j];
    *nn = all ? 4u : s->n;
    return (s->hit || s->acc ? 1u : 0u) | (s->probability ? 2u : 0u) | (r & 2u ? 4u : 0u) | (all ? 8u : 0u) |
           (s->comp ? 16u : 0u) |                       /* (0.6.39) a component: two bytes, its type and arguments */
           (s->comp2 || s->cwhen || s->ctime ? 32u : 0u);   /* (0.6.82) the second, WHEN, DIV / LEN: four more */
}
static uint32_t proj_step_size(const step_t *s)
{
    uint32_t nn, x = proj_step_x(s, &nn);
    return 2u + nn + (x ? 1u + (x & 1u ? 2u : 0u) + (x & 2u ? 1u : 0u) + (x & 16u ? 2u : 0u) + (x & 32u ? 4u : 0u) : 0u);
}
static int proj_fm_kept(const project_t *q, uint32_t t)
{
    return q->t[t].engine == ENGI_FM6 || memcmp(q->fm6[t], FM6_INIT, FM6_PACKED) != 0;
}
static int16_t proj_pdef(uint32_t engine, uint32_t i) { return param_desc_of(engine, i)->def; }
/* the bytes q takes as FUNC up to the tail (PROJC_PFX_OFF: the room); more than the room: SECTION FULL */
static int proj_fg_used(const project_t *q)            /* (0.6.52) a FUGUE head on: its block is written */
{
    uint32_t t, h;
    for (t = 0; t < NTRK; t++)
        for (h = 0; h < 3u; h++)
            if (q->fugue[t][h][0] & 15u)
                return 1;
    return 0;
}
static uint32_t proj_packed_size(const project_t *q)
{
    uint32_t n = 68u + sizeof q->chain + 4u + q->motion.count * sizeof q->motion.event[0] + 2u + G_SL_N, t, i;
    if (q->g[G_DMOD] != GP[G_DMOD].def)                 /* (0.6.86) the delay's MOD's, when not the default */
        n += 3u;
    if (q->g[G_DTYPE] || q->g[G_DPING] || q->g[G_CTYPE])   /* (0.6.90) its TYPE and PING-PONG's (QG-1: the chorus's TYPE) */
        n += 4u;
    for (i = 0; i < sizeof q->dx_snd; i++)              /* (0.6.48) the SENDs' block, when one is set */
        if (q->dx_snd[i]) { n += 2u + sizeof q->dx_snd; break; }
    for (i = 0; i < sizeof q->dx_lay; i++)              /* (0.6.49) the LAYERs' */
        if (q->dx_lay[i]) { n += 2u + sizeof q->dx_lay; break; }
    for (i = 0; i < sizeof q->dx_mdl; i++)              /* (1.0-GW) the MODELs' */
        if (q->dx_mdl[i]) { n += 2u + sizeof q->dx_mdl; break; }
    if (proj_fg_used(q))                                /* (0.6.52) FUGUE's */
        n += 2u + sizeof q->fugue;
    for (t = 0; t < NTRK; t++) {
        uint32_t e = q->t[t].engine < NENGINES ? q->t[t].engine : 0u;
        n += 3u + PROJC_PMAP + 8u + (proj_fm_kept(q, t) ? FM6_PACKED : 0u);
        for (i = 0; i < P_COUNT; i++)
            n += q->t[t].p[i] != proj_pdef(e, i);
        for (i = 0; i < NSTEP; i++)
            n += proj_step_empty(&q->t[t].step[i]) ? 0u : proj_step_size(&q->t[t].step[i]);
    }
    return n;
}
static int proj_pack(project_store_t *out, const project_t *q)
{
    uint8_t *b = out->raw; uint32_t pos = 68u, t, i; uint32_t magic = PROJ_MAGIC, size = PROJ_STORE_SIZE, sum, mlen;
    proj_full = 0;
    for (t = 0; t < NTRK; t++) {                        /* the values first: out untouched when one is wrong */
        if (q->t[t].engine >= NENGINES) return 0;
        for (i = 0; i < FM6_PACKED; i++)
            if (q->fm6[t][i] > 127u) return 0;
        for (i = 0; i < P_COUNT; i++)
            if (q->t[t].p[i] < -64 || q->t[t].p[i] > 127) return 0;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            if (s->n > 4u || s->time > ST_REST || (s->flags & ~(3u | SF_RATCH)) || s->probability > 101u ||
                s->comp >= CP_N || s->comp2 >= CP_N || CT_DIV(s) >= CT_NDIV || s->ctime > 127u) return 0;
        }
    }
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion) || P_COUNT > 127u || !dx_kit_ok(q->dx)) return 0;
    if (proj_packed_size(q) > PROJC_PFX_OFF) { proj_full = 1; return 0; }
    memset(out, 0, sizeof *out); memcpy(b, &magic, 4); memcpy(b + 4, &size, 4);
    memcpy(b + 8, q->g, G_NSTORE * 2u); b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = P_COUNT;
    b[67] = NTRK;
    for (t = 0; t < NTRK; t++) {
        uint32_t e = q->t[t].engine, map, fm = (uint32_t)proj_fm_kept(q, t);
        b[pos++] = (uint8_t)e; b[pos++] = q->t[t].preset; b[pos++] = (uint8_t)fm;
        if (fm) { memcpy(b + pos, q->fm6[t], FM6_PACKED); pos += FM6_PACKED; }
        map = pos;
        pos += PROJC_PMAP;
        for (i = 0; i < P_COUNT; i++)
            if (q->t[t].p[i] != proj_pdef(e, i)) {
                b[map + i / 8u] |= (uint8_t)(1u << (i & 7u));
                b[pos++] = (uint8_t)(q->t[t].p[i] + 64);
            }
        map = pos;
        pos += 8u;                                      /* (the steps' map, filled below) */
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            uint32_t r = step_ratchet(s) - 1u, nn, x, j;   /* (what proj_unpack_c checks, so a saved project always
                                                         * loads: notes and velocity 0..127, accents only on hits) */
            if (proj_step_empty(s))
                continue;
            x = proj_step_x(s, &nn);
            b[map + i / 8u] |= (uint8_t)(1u << (i & 7u));
            b[pos++] = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5 | (x ? 128u : 0u));
            if (x)
                b[pos++] = (uint8_t)x;
            for (j = 0; j < nn; j++)
                b[pos++] = s->note[j] > 127u ? 127u : s->note[j];
            b[pos++] = (uint8_t)((s->vel > 127u ? 127u : s->vel) | (r & 1u) << 7);
            if (x & 1u) { b[pos++] = s->hit; b[pos++] = s->acc & s->hit; }
            if (x & 2u) b[pos++] = s->probability;
            if (x & 16u) { b[pos++] = s->comp; b[pos++] = s->carg; }
            if (x & 32u) { b[pos++] = s->comp2; b[pos++] = s->carg2; b[pos++] = s->cwhen; b[pos++] = s->ctime; }
        }
    }
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    mlen = 4u + q->motion.count * sizeof q->motion.event[0];
    memcpy(b + pos, &q->motion, mlen);
    pos += mlen;
    b[pos++] = 'S'; b[pos++] = 'L';                     /* (0.6.3) the SLICER's globals (biased bytes) */
    for (i = 0; i < G_SL_N; i++)
        b[pos++] = (uint8_t)(clamp(q->g[G_SLMODE + i], -64, 127) + 64);
    for (i = 0; i < sizeof q->dx_snd && !q->dx_snd[i]; i++)
        ;
    if (i < sizeof q->dx_snd) {                         /* (0.6.48) the kit's SENDs, when one is set */
        b[pos++] = 'D'; b[pos++] = 'S';
        memcpy(b + pos, q->dx_snd, sizeof q->dx_snd);
        pos += sizeof q->dx_snd;
    }
    for (i = 0; i < sizeof q->dx_lay && !q->dx_lay[i]; i++)
        ;
    if (i < sizeof q->dx_lay) {                         /* (0.6.49) the kit's LAYERs and FEEDBKs */
        b[pos++] = 'D'; b[pos++] = 'L';
        memcpy(b + pos, q->dx_lay, sizeof q->dx_lay);
        pos += sizeof q->dx_lay;
    }
    if (proj_fg_used(q)) {                              /* (0.6.52) FUGUE's heads */
        b[pos++] = 'F'; b[pos++] = 'G';
        memcpy(b + pos, q->fugue, sizeof q->fugue);
        pos += sizeof q->fugue;
    }
    if (q->g[G_DMOD] != GP[G_DMOD].def) {               /* (0.6.86) the delay's MOD (biased), when not the default */
        b[pos++] = 'D'; b[pos++] = 'M';
        b[pos++] = (uint8_t)(clamp(q->g[G_DMOD], 0, 127) + 64);
    }
    if (q->g[G_DTYPE] || q->g[G_DPING] || q->g[G_CTYPE]) {   /* (0.6.90) the delay's TYPE, PING-PONG; (QG-1) the chorus's
                                                         * TYPE in bit 1 (a firmware before it reads bit 0 only) */
        b[pos++] = 'D'; b[pos++] = 'T';
        b[pos++] = (uint8_t)(clamp(q->g[G_DTYPE], 0, 1) | clamp(q->g[G_CTYPE], 0, 1) << 1);
        b[pos++] = (uint8_t)clamp(q->g[G_DPING], 0, 1);
    }
    for (i = 0; i < sizeof q->dx_mdl && !q->dx_mdl[i]; i++)
        ;
    if (i < sizeof q->dx_mdl) {                         /* (1.0-GW) the kit's MODELs and CLIPs, when one is set (last:
                                                         * a firmware before it reads the rest as it did) */
        b[pos++] = 'D'; b[pos++] = 'X';
        memcpy(b + pos, q->dx_mdl, sizeof q->dx_mdl);
        pos += sizeof q->dx_mdl;
    }
    memcpy(b + PROJC_PFX_OFF, q->pfx_lane, sizeof q->pfx_lane);
    b[PROJC_PFX_OFF + sizeof q->pfx_lane] = q->pfx_ltgt > 2u ? 0u : q->pfx_ltgt;
    for (i = 0; i < 4u; i++)
        b[PROJC_PFX_OFF + sizeof q->pfx_lane + 1u + i] = q->macro[i] & 127u;
    b[PROJC_GX_OFF] = 3u;                               /* the new globals (biased bytes, as FUNB's) */
    for (i = G_NSTORE; i < G_GX_END; i++)
        b[PROJC_GX_OFF + 1u + i - G_NSTORE] = (uint8_t)(clamp(q->g[i], -64, 127) + 64);
    memcpy(b + PROJC_DX_OFF, q->dx, sizeof q->dx);
    b[PROJC_DX_OFF + sizeof q->dx] = q->dx_mute & DXG_ALL;
    b[PROJC_DX_OFF + sizeof q->dx + 1u] = (uint8_t)(q->dx_mute >> 8);
    b[PROJC_DX_OFF + sizeof q->dx + 2u] = 4u;           /* (FUNB's marker 4: the values as today's) */
    memcpy(b + PROJC_DX_OFF + sizeof q->dx + 3u, q->dx_mot, sizeof q->dx_mot);
    b[PROJC_DX_OFF + sizeof q->dx + 7u] = q->dx_tone;   /* (0.6.36) TONE: 0 before it (off) */
    {   /* the name (0-padded; stops at the first 0) */
        char n[PROJ_NAME_LEN + 1u];
        memcpy(b + PROJ_NAME_OFF, n, proj_name_get(n, (const uint8_t *)q->name));
    }
    sum = proj_hash(b, PROJ_STORE_SIZE - 4u); memcpy(b + PROJ_STORE_SIZE - 4u, &sum, 4);
    return 1;
}
#ifdef PROJ_FUNB_WRITER
/* (tests only) the first PROJ_LT tracks of q as FUNB, as JIANT 0.2 .. 0.5 wrote it: the older formats' imports are
 * tested from it */
static int proj_pack_vb(project_store_t *out, const project_t *q)
{
    uint8_t *b = out->raw; uint32_t pos = 68u, t, i; uint32_t magic = PROJ_MAGIC_VB, size = PROJ_STORE_SIZE, sum;
    for (t = 0; t < PROJ_LT; t++)
        for (i = 0; i < FM6_PACKED; i++)
            if (q->fm6[t][i] > 127u) return 0;
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion) || P_COUNT > 127u) return 0;
    memset(out, 0, sizeof *out); memcpy(b, &magic, 4); memcpy(b + 4, &size, 4);
    memcpy(b + 8, q->g, G_NSTORE * 2u); b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = P_COUNT;
    for (t = 0; t < PROJ_LT; t++) {
        for (i = 0; i < P_COUNT; i++) {
            if (q->t[t].p[i] < -64 || q->t[t].p[i] > 127) return 0;
            b[pos++] = (uint8_t)(q->t[t].p[i] + 64);
        }
        b[pos++] = q->t[t].engine; b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            uint32_t r = step_ratchet(s) - 1u;          /* RATCH: bit 7 of the velocity and chance bytes (see the top) */
            if (s->n > 4u || s->time > ST_REST || (s->flags & ~(3u | SF_RATCH)) || s->probability > 101u) return 0;
            for (uint32_t j = 0; j < 4u; j++)           /* what proj_unpack checks, so a saved project always loads: */
                b[pos++] = s->note[j] > 127u ? 127u : s->note[j];   /* notes and velocity 0..127, accents */
            b[pos++] = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5);   /* only on hits */
            b[pos++] = (uint8_t)((s->vel > 127u ? 127u : s->vel) | (r & 1u) << 7); b[pos++] = s->hit;
            b[pos++] = s->acc & s->hit;
            b[pos++] = (uint8_t)(s->probability | (r >> 1) << 7);
        }
    }
    if (pos + sizeof q->chain + sizeof q->motion > PROJ_DX_OFF || !dx_kit_ok(q->dx)) return 0;
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    memcpy(b + pos, &q->motion, sizeof q->motion);
    memcpy(b + PROJ_PFX_OFF, q->pfx_lane, sizeof q->pfx_lane);
    b[PROJ_PFX_OFF + sizeof q->pfx_lane] = q->pfx_ltgt > 2u ? 0u : q->pfx_ltgt;
    for (i = 0; i < 4u; i++)
        b[PROJ_PFX_OFF + sizeof q->pfx_lane + 1u + i] = q->macro[i] & 127u;
    b[PROJ_GX_OFF] = 3u;                                /* the new globals (biased bytes, as the parameters; 1: the
                                                         * first five, G_DHPF .. G_RPRE; 2: nine, to G_DSPRY; 3: ten, G_STRN) */
    for (i = G_NSTORE; i < G_GX_END; i++)
        b[PROJ_GX_OFF + 1u + i - G_NSTORE] = (uint8_t)(clamp(q->g[i], -64, 127) + 64);
    memcpy(b + PROJ_DX_OFF, q->dx, sizeof q->dx);
    b[PROJ_DX_OFF + sizeof q->dx] = q->dx_mute & DXG_ALL;
    b[PROJ_DX_OFF + sizeof q->dx + 1u] = (uint8_t)(q->dx_mute >> 8);
    b[PROJ_DX_OFF + sizeof q->dx + 2u] = 4u;                    /* 1 SHIFT: OFS / PIT where the chord keys were; 2 DRUM's E5 FM;
                                                                 * 4 (0.5.4) LOFI BYTE / FLOAT's MOTN, GRIT / DCY;
                                                                 * 3 (0.4) LOFI's and ANALOG's new E values (core.h sound_v04) */
    memcpy(b + PROJ_DX_OFF + sizeof q->dx + 3u, q->dx_mot, sizeof q->dx_mot);   /* (0.5.1) X-MOD: 0 before it (off) */
    b[PROJ_DX_OFF + sizeof q->dx + 7u] = q->dx_tone;
    memcpy(b + PROJ_FM6_OFF, q->fm6, PROJ_LT * FM6_PACKED);
    {   /* the name (0-padded; stops at the first 0) */
        char n[PROJ_NAME_LEN + 1u];
        memcpy(b + PROJ_NAME_OFF, n, proj_name_get(n, (const uint8_t *)q->name));
    }
    sum = proj_hash(b, PROJ_STORE_SIZE - 4u); memcpy(b + PROJ_STORE_SIZE - 4u, &sum, 4);
    return 1;
}
#endif
/* a FUNC store (3840 bytes) -> q; 0 = not one (its hash, a value out of range, a length past its room) */
static int proj_unpack_c(project_t *q, const uint8_t *b)
{
    uint32_t pos = 68u, t, i, size, sum, np = b[66], nt = b[67], room = PROJC_PFX_OFF;
    memcpy(&size, b + 4, 4); memcpy(&sum, b + PROJ_STORE_SIZE - 4u, 4);
    if (size != PROJ_STORE_SIZE || sum != proj_hash(b, PROJ_STORE_SIZE - 4u) || np < 8u || np > P_COUNT || !nt || nt > NTRK)
        return 0;
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC; q->size = sizeof *q;
    memcpy(q->g, b + 8, G_NSTORE * 2u); q->sel = b[62];
    for (i = G_NSTORE; i < G_COUNT; i++) {
        uint32_t gm = b[PROJC_GX_OFF];
        q->g[i] = gm && gm <= 3u && i < G_NSTORE + (gm == 1u ? 5u : gm == 2u ? 9u : 10u)
                      ? (int16_t)clamp((int32_t)b[PROJC_GX_OFF + 1u + i - G_NSTORE] - 64, GP[i].min, GP[i].max) : GP[i].def;
    }
    q->parts = b[63]; q->phys = b[64];
    for (t = 0; t < nt; t++) {
        int16_t values[P_COUNT], def[P_COUNT];
        uint32_t map, e, nc = np - 8u, pm = (np + 7u) / 8u;
        if (pos + 3u > room) return 0;
        q->t[t].engine = (uint8_t)(e = b[pos++]); q->t[t].preset = b[pos++];
        if (e >= NENGINES || b[pos] > 1u) return 0;
        if (b[pos++]) {
            if (pos + FM6_PACKED > room) return 0;
            for (i = 0; i < FM6_PACKED; i++) q->fm6[t][i] = b[pos++] & 0x7Fu;
        } else {
            memcpy(q->fm6[t], FM6_INIT, FM6_PACKED);
        }
        for (i = 0; i < P_COUNT; i++) def[i] = proj_pdef(e, i);
        map = pos;
        pos += pm;
        if (pos > room) return 0;
        for (i = 0; i < np; i++) {                      /* (an older np: params_by_count's places, their defaults) */
            if (!((b[map + i / 8u] >> (i & 7u)) & 1u)) {
                values[i] = def[i < nc ? i : P_E0 + i - nc];
                continue;
            }
            if (pos >= room || b[pos] > 191u) return 0;
            values[i] = (int16_t)b[pos++] - 64;
        }
        params_by_count(q->t[t].p, values, np, def);
        if (pos + 8u > room) return 0;
        map = pos;
        pos += 8u;
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i]; uint32_t meta, x = 0, nn, j, r;
            if (!((b[map + i / 8u] >> (i & 7u)) & 1u)) {
                s->time = ST_REST;                      /* (an empty step: step_clear's) */
                continue;
            }
            if (pos + 1u > room) return 0;
            meta = b[pos++];
            s->n = meta & 7u; s->time = (meta >> 3) & 3u; s->flags = (meta >> 5) & 3u;
            if (s->n > 4u || s->time > ST_REST) return 0;
            nn = s->n;
            if (meta & 128u) {
                if (pos + 1u > room) return 0;
                x = b[pos++];
                if (!x || (x & ~63u)) return 0;
                if (x & 8u) nn = 4u;
            }
            if (pos + nn + 1u > room) return 0;
            for (j = 0; j < nn; j++)
                if ((s->note[j] = b[pos++]) > 127u) return 0;
            s->vel = b[pos] & 127u; r = b[pos++] >> 7;
            if (x & 1u) {
                if (pos + 2u > room) return 0;
                s->hit = b[pos++]; s->acc = b[pos++];
                if (s->acc & ~s->hit) return 0;
            }
            if (x & 2u) {
                if (pos + 1u > room || (s->probability = b[pos++]) > 101u || !s->probability) return 0;
            }
            s->comp = s->carg = 0;
            if (x & 16u) {                              /* (0.6.39) a component */
                if (pos + 2u > room || (s->comp = b[pos++]) >= CP_N || !s->comp) return 0;
                s->carg = b[pos++];
            }
            s->comp2 = s->carg2 = s->cwhen = s->ctime = 0;
            if (x & 32u) {                              /* (0.6.82) the second component, WHEN, DIV and LEN */
                if (pos + 4u > room || (s->comp2 = b[pos++]) >= CP_N) return 0;
                s->carg2 = b[pos++]; s->cwhen = b[pos++]; s->ctime = b[pos++];
                if (CT_DIV(s) >= CT_NDIV || s->ctime > 127u) return 0;
            }
            r |= (x >> 1) & 2u;
            step_set_ratchet(s, r + 1u);
        }
    }
    if (pos + sizeof q->chain + 4u > room) return 0;
    memcpy(&q->chain, b + pos, sizeof q->chain); pos += sizeof q->chain;
    memcpy(&q->motion, b + pos, 4u); pos += 4u;
    if (q->motion.count > MOTION_MAX || pos + q->motion.count * sizeof q->motion.event[0] > room) return 0;
    memcpy(q->motion.event, b + pos, q->motion.count * sizeof q->motion.event[0]);
    pos += q->motion.count * sizeof q->motion.event[0];
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion)) return 0;
    if (pos + 2u + G_SL_N <= room && b[pos] == 'S' && b[pos + 1u] == 'L') {   /* (0.6.3) the SLICER's globals */
        for (i = 0; i < G_SL_N; i++)
            q->g[G_SLMODE + i] = (int16_t)clamp((int32_t)b[pos + 2u + i] - 64, GP[G_SLMODE + i].min, GP[G_SLMODE + i].max);
        proj_sl_found = 1;
        pos += 2u + G_SL_N;
    }
    memset(q->dx_snd, 0, sizeof q->dx_snd);
    memset(q->dx_lay, 0, sizeof q->dx_lay);
    if (pos + 2u + sizeof q->dx_snd <= room && b[pos] == 'D' && b[pos + 1u] == 'S') {   /* (0.6.48) the kit's SENDs */
        memcpy(q->dx_snd, b + pos + 2u, sizeof q->dx_snd);
        pos += 2u + sizeof q->dx_snd;
    }
    if (pos + 2u + sizeof q->dx_lay <= room && b[pos] == 'D' && b[pos + 1u] == 'L') {   /* (0.6.49) its LAYERs */
        memcpy(q->dx_lay, b + pos + 2u, sizeof q->dx_lay);
        pos += 2u + sizeof q->dx_lay;
    }
    memset(q->fugue, 0, sizeof q->fugue);
    if (pos + 2u + sizeof q->fugue <= room && b[pos] == 'F' && b[pos + 1u] == 'G') {   /* (0.6.52) FUGUE's heads */
        uint32_t k;
        memcpy(q->fugue, b + pos + 2u, sizeof q->fugue);
        for (k = 0; k < sizeof q->fugue; k += 2u) {     /* (in range) */
            uint8_t *f = &q->fugue[0][0][0] + k;
            if ((f[0] & 15u) >= 10u || (f[0] >> 4) > 3u) f[0] = 0;
            if ((int8_t)f[1] < -14 || (int8_t)f[1] > 14) f[1] = 0;
        }
        pos += 2u + sizeof q->fugue;
    }
    if (pos + 3u <= room && b[pos] == 'D' && b[pos + 1u] == 'M') {   /* (0.6.86) the delay's MOD (else its default) */
        q->g[G_DMOD] = (int16_t)clamp((int32_t)b[pos + 2u] - 64, GP[G_DMOD].min, GP[G_DMOD].max);
        pos += 3u;
    }
    if (pos + 4u <= room && b[pos] == 'D' && b[pos + 1u] == 'T') {   /* (0.6.90) its TYPE, PING-PONG (else ANALOG, OFF) */
        q->g[G_DTYPE] = (int16_t)(b[pos + 2u] & 1u);
        q->g[G_CTYPE] = (int16_t)(b[pos + 2u] >> 1 & 1u);
        q->g[G_DPING] = (int16_t)(b[pos + 3u] & 1u);
        pos += 4u;
    }
    memset(q->dx_mdl, 0, sizeof q->dx_mdl);
    if (pos + 2u + sizeof q->dx_mdl <= room && b[pos] == 'D' && b[pos + 1u] == 'X') {   /* (1.0-GW) its MODELs */
        uint32_t k;
        memcpy(q->dx_mdl, b + pos + 2u, sizeof q->dx_mdl);
        for (k = 0; k < sizeof q->dx_mdl; k++)          /* (in range: CLASSIC, SNARE, CLAP; CLIP 0..15) */
            if (DXX_MODEL(q->dx_mdl[k]) > DXX_CLAP || q->dx_mdl[k] >> 6) q->dx_mdl[k] = 0;
        pos += 2u + sizeof q->dx_mdl;
    }
    proj_new_tracks(q, nt);                             /* (a FUNC of fewer tracks: the rest at power-on) */
    for (t = nt; t < NTRK; t++)
        memcpy(q->fm6[t], FM6_INIT, FM6_PACKED);
    memcpy(q->pfx_lane, b + PROJC_PFX_OFF, sizeof q->pfx_lane);
    q->pfx_ltgt = b[PROJC_PFX_OFF + sizeof q->pfx_lane] > 2u ? 0u : b[PROJC_PFX_OFF + sizeof q->pfx_lane];
    for (i = 0; i < 4u; i++)
        q->macro[i] = b[PROJC_PFX_OFF + sizeof q->pfx_lane + 1u + i] & 127u;
    memcpy(q->dx, b + PROJC_DX_OFF, sizeof q->dx);
    if (!dx_kit_ok(q->dx)) return 0;
    q->dx_mute = (uint16_t)((b[PROJC_DX_OFF + sizeof q->dx] & DXG_ALL) | b[PROJC_DX_OFF + sizeof q->dx + 1u] << 8);
    memcpy(q->dx_mot, b + PROJC_DX_OFF + sizeof q->dx + 3u, sizeof q->dx_mot);
    q->dx_tone = b[PROJC_DX_OFF + sizeof q->dx + 7u];
    q->dx_mot[XM_RATE] = q->dx_mot[XM_RATE] > 8u ? 0u : q->dx_mot[XM_RATE];
    q->dx_mot[XM_DPTH] &= 127u;
    q->dx_mot[XM_SHPE] = q->dx_mot[XM_SHPE] > 4u ? 0u : q->dx_mot[XM_SHPE];
    q->dx_mot[XM_RAND] &= 127u;
    {
        char n[PROJ_NAME_LEN + 1u];
        memcpy(q->name, n, proj_name_get(n, b + PROJ_NAME_OFF));
    }
    q->parts = NPART;
    q->sum = proj_sum(q);
    return 1;
}
/* the motion of a FUN7 / FUN8 written with np parameters (np < P_COUNT: a FUN7 of 89 before the chord keys, every
 * FUN8 of 91 before the DRUM lane levels) -> today's ids: an event names a parameter id, and the ids from that
 * store's P_E0 (np - 8) on moved up with P_E0, as its values did (params_by_count); the ones below kept theirs. A
 * lock's bit 7 (MOTION_LOCK) stays: the id moves under it (a FUN8 / FUN7 of a 1.1 development build may hold
 * locks). 0: an id that store could not have named (np or more) */
static int proj_motion_ids(motion_store_t *m, uint32_t np)
{
    uint32_t i;
    for (i = 0; i < m->count && i < MOTION_MAX; i++) {
        uint32_t id = MOTION_ID(&m->event[i]);
        if (id >= np) return 0;
        if (np < P_COUNT && id >= np - 8u)
            m->event[i].param = (uint8_t)((m->event[i].param & MOTION_LOCK) | (id + P_COUNT - np));
    }
    return 1;
}
/* a stored FUNA (st = PROJ_STORE_SIZE), FUN9 (PROJ_STORE_V9: no kit, the factory one), FUN8 (PROJ_STORE_V8: as FUN9,
 * its tail 64 bytes shorter) or FUN7 (PROJ_STORE_V7: no patches, the init one) */
static int proj_unpack(project_t *q, const uint8_t *b, uint32_t st)
{
    uint32_t pos = 68u, t, i, magic, size, sum, np = b[66], v7 = st == PROJ_STORE_V7;
    uint32_t vb = st == PROJ_STORE_SIZE, va = vb || st == PROJ_STORE_VA;   /* (va: FUNA or FUNB, the kit's layout) */
    uint32_t name_off = st - 4u - PROJ_NAME_LEN, fm6_off = name_off - PROJ_LT * FM6_PACKED;
    uint32_t end = v7 ? name_off : va ? fm6_off - PROJ_DX_SIZE : fm6_off;
    memcpy(&magic, b, 4); memcpy(&size, b + 4, 4); memcpy(&sum, b + st - 4u, 4);
    if (magic != (v7 ? PROJ_MAGIC_V7 : st == PROJ_STORE_V8 ? PROJ_MAGIC_V8 : st == PROJ_STORE_V9 ? PROJ_MAGIC_V9 :
                  st == PROJ_STORE_VA ? PROJ_MAGIC_VA : PROJ_MAGIC_VB) ||
        size != st || sum != proj_hash(b, st - 4u) ||
        np < 8u || np > P_COUNT || 68u + PROJ_LT * (np + 2u + NSTEP * 9u) + sizeof q->chain + sizeof q->motion > end)
        return 0;
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC; q->size = sizeof *q;
    memcpy(q->g, b + 8, G_NSTORE * 2u); q->sel = b[62];
    for (i = G_NSTORE; i < G_COUNT; i++) {              /* the new globals: FUNB's (marker 1: the first five, 2: nine, 3: ten), */
        uint32_t gm = vb ? b[end - 48u + 37u] : 0u;     /* else their defaults */
        q->g[i] = gm && gm <= 3u && i < G_NSTORE + (gm == 1u ? 5u : gm == 2u ? 9u : 10u)
                      ? (int16_t)clamp((int32_t)b[end - 48u + 38u + i - G_NSTORE] - 64, GP[i].min, GP[i].max) : GP[i].def;
    }
    q->parts = b[63]; q->phys = b[64];
    if (v7 && (q->g[G_RTYPE] < 0 || q->g[G_RTYPE] > 1))   /* a FUN7 may still hold the old drum channel there */
        proj_rtype_room(q->g);
    for (t = 0; t < PROJ_LT; t++) {
        int16_t values[P_COUNT], def[P_COUNT];
        for (i = 0; i < np; i++) { if (b[pos] > 191u) return 0; values[i] = (int16_t)b[pos++] - 64; }
        q->t[t].engine = b[pos++]; q->t[t].preset = b[pos++];
        if (q->t[t].engine >= NENGINES) return 0;
        for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(q->t[t].engine, i)->def;
        params_by_count(q->t[t].p, values, np, def);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i]; uint32_t meta;
            memcpy(s->note, b + pos, 4); pos += 4; meta = b[pos++];
            s->n = meta & 7u; s->time = (meta >> 3) & 3u; s->flags = (meta >> 5) & 3u;
            s->vel = b[pos] & 127u; meta |= (b[pos++] & 128u) << 1;      /* (RATCH bit 0 at bit 8 of meta) */
            s->hit = b[pos++]; s->acc = b[pos++];
            s->probability = b[pos] & 127u; meta |= (b[pos++] & 128u) << 2;   /* (bit 1 at bit 9) */
            if ((meta & 255u) > 127u || s->n > 4u || s->time > ST_REST || s->probability > 101u) return 0;
            for (uint32_t j = 0; j < 4u; j++) if (s->note[j] > 127u) return 0;
            if (s->acc & ~s->hit) return 0;
            step_set_ratchet(s, (meta >> 8) + 1u);
        }
    }
    memcpy(&q->chain, b + pos, sizeof q->chain); pos += sizeof q->chain;
    memcpy(&q->motion, b + pos, sizeof q->motion);
    if (!proj_motion_ids(&q->motion, np) || !chain_valid(&q->chain) || !motion_valid(&q->motion)) return 0;
    for (t = 0; t < NTRK; t++) {
        if (v7 || t >= PROJ_LT)
            memcpy(q->fm6[t], FM6_INIT, FM6_PACKED);
        else
            for (i = 0; i < FM6_PACKED; i++)
                q->fm6[t][i] = b[fm6_off + t * FM6_PACKED + i] & 0x7Fu;
    }
    memset(q->pfx_lane, 0, sizeof q->pfx_lane);
    q->pfx_ltgt = 0;
    memset(q->macro, 0, sizeof q->macro);
    if (va) {                                           /* the punch-in lane (JIANT's; a FUN9's spare is zeros) */
        uint32_t lo = end - 48u;
        memcpy(q->pfx_lane, b + lo, sizeof q->pfx_lane);
        q->pfx_ltgt = b[lo + sizeof q->pfx_lane] > 2u ? 0u : b[lo + sizeof q->pfx_lane];
        for (i = 0; i < 4u; i++)                        /* the macros (0 before them) */
            q->macro[i] = b[lo + sizeof q->pfx_lane + 1u + i] & 127u;
    }
    if (va) {                                           /* the DRUM-X kit (a broken one: not a project) */
        memcpy(q->dx, b + end, sizeof q->dx);
        if (!dx_kit_ok(q->dx)) return 0;
        q->dx_mute = (uint16_t)((b[end + sizeof q->dx] & DXG_ALL) | b[end + sizeof q->dx + 1u] << 8);
        memcpy(q->dx_mot, b + end + sizeof q->dx + 3u, sizeof q->dx_mot);
        q->dx_tone = b[end + sizeof q->dx + 7u];
        q->dx_mot[XM_RATE] = q->dx_mot[XM_RATE] > 8u ? 0u : q->dx_mot[XM_RATE];   /* (out of range: off) */
        q->dx_mot[XM_DPTH] &= 127u;
        q->dx_mot[XM_SHPE] = q->dx_mot[XM_SHPE] > 4u ? 0u : q->dx_mot[XM_SHPE];
        q->dx_mot[XM_RAND] &= 127u;
    }
    if (!va || b[end + sizeof q->dx + 2u] < 1u || b[end + sizeof q->dx + 2u] > 4u)   /* written before SHIFT: */
        for (t = 0; t < PROJ_LT; t++)                      /* its chord keys are no offset */
            q->t[t].p[P_SOFS] = q->t[t].p[P_POFS] = 0;
    if (!va || b[end + sizeof q->dx + 2u] < 2u || b[end + sizeof q->dx + 2u] > 4u)   /* (JIANT) before FM: DRUM's E5 */
        for (t = 0; t < PROJ_LT; t++)                      /* was ACC: FM off */
            if (q->t[t].engine == ENGI_DRUM)
                q->t[t].p[P_E5] = 0;
    if (!va || b[end + sizeof q->dx + 2u] < 3u || b[end + sizeof q->dx + 2u] > 4u)   /* (JIANT 0.4) LOFI's, ANALOG's E
                                                         * values as today's */
        for (t = 0; t < PROJ_LT; t++)
            sound_v04(q->t[t].engine, &q->t[t].p[P_E0]);
    if (!va || b[end + sizeof q->dx + 2u] < 4u || b[end + sizeof q->dx + 2u] > 4u)   /* (0.5.4) a LOFI BYTE / FLOAT: E4 was
                                                         * the old CUT (MOTN as written: 64), E5 its RES (GRIT, DCY: 0) */
        for (t = 0; t < PROJ_LT; t++)
            if (q->t[t].engine == 3u && q->t[t].p[P_E1] >= 5)
                q->t[t].p[P_E4] = 64, q->t[t].p[P_E5] = 0;
    if (!va) {
        memcpy(q->dx, DX_KIT_DEF, sizeof q->dx);
        q->dx_mute = 0;
        memset(q->dx_mot, 0, sizeof q->dx_mot);
        q->dx_tone = 0;
    }
    memset(q->dx_snd, 0, sizeof q->dx_snd);            /* (0.6.48: none before FUNC's block) */
    memset(q->dx_lay, 0, sizeof q->dx_lay);            /* (0.6.49: none: the kit as it sounded) */
    memset(q->dx_mdl, 0, sizeof q->dx_mdl);            /* (1.0-GW) */
    memset(q->fugue, 0, sizeof q->fugue);              /* (0.6.52) */
    proj_new_tracks(q, PROJ_LT);                       /* (JIANT 0.6: tracks 5, 6 their power-on sounds) */
    {
        char n[PROJ_NAME_LEN + 1u];
        memcpy(q->name, n, proj_name_get(n, b + name_off));
    }
    q->sum = proj_sum(q); proj_drums_to_part(q); proj_phys(q);
    return 1;
}

#ifndef PROJ_HOST
/* the GM drum part of a project of before 1.0 (proj_drums_to_part): DRUM's first kit, as a preset load sets it
 * (until 1.0.2 the SAMPLE engine's PERC set, retired since) */
static void proj_legacy_drums(track_t *t)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const preset_t *pr = &ENGINES[ENGI_DRUM]->presets[0];
    uint32_t i;
    t->eng_req = ENGI_DRUM;
    t->preset = 0;
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}

static project_t proj_scratch __attribute__((section(".lowbuf")));   /* (0.6.78: below the RAM code, app.ld) */   /* decoded main-loop work, never audio ISR (0.6: pool) */
static char proj_name[PROJ_NAME_LEN + 1u]    /* the name of the music as it is now (loaded, saved, the editor's */
    __attribute__((section(".pool")));       /* runtime restore); "" = none. A save takes it unless one is given */
#define PROJ_NO_SLOT 0xFFu
static uint8_t proj_cur = PROJ_NO_SLOT;      /* the slot the music was loaded from or last saved to (a rename of it
                                              * renames the music too); PROJ_NO_SLOT none (the editor's restore) */
/* the flash object of section `slot` of the current song (storage.c: song 1 is the four project slots) */
#if FELUCCA_FLASH
static uint32_t proj_obj(uint32_t slot)
{
    return song_cur ? OBJ_SONG1 + (song_cur - 1u) * 4u + (slot & 3u) : OBJ_PROJECT0 + (slot & 3u);
}
#else
#define proj_obj(slot) (slot)                /* (no flash: nothing stored) */
#endif
static union {                               /* serialized main-loop work; no retained expansion */
    project_store_t s;
    uint8_t raw[3840];                         /* (the staging of a backup object, up to a storage object: editor_backup.c) */
} proj_wire_u __attribute__((section(".pool")));   /* (1.2.1-GW, JIANT 0.8.4: in the pool, was .bss; main loop only) */
#define proj_wire (proj_wire_u.s)
static uint8_t proj_wire_gen;                /* +1 whenever proj_wire is rewritten (a backup's runtime copy lives there) */

static void proj_steps(step_t *s)            /* a loaded sequence stays inside its fixed fields */
{
    uint32_t i, j;
    for (i = 0; i < NSTEP; i++) {
        if (s[i].n > 4u) s[i].n = 4;
        if (s[i].time > ST_REST) s[i].time = ST_REST;
        for (j = 0; j < 4u; j++) s[i].note[j] &= 127u;
        s[i].acc &= s[i].hit;
    }
}

/* an imported older format may hold values FUN7 cannot pack: keep them inside the fields and ranges */
static void proj_bound(project_t *q)
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        proj_steps(q->t[t].step);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i];
            s->flags &= 3u | SF_RATCH;
            s->vel &= 127u;
            if (s->probability > 101u) s->probability = 0;
        }
        for (i = 0; i < P_COUNT; i++) {
            const param_desc_t *d = param_desc_of(q->t[t].engine % NENGINES, i);
            q->t[t].p[i] = (int16_t)param_fit(d, q->t[t].p[i]);   /* (a retired KIT: the kit it plays) */
        }
    }
    q->sum = proj_sum(q);
}

#if FELUCCA_FLASH
/* slot from flash into RAM (format 7, or format 6 / 5 / 4 / 3 / 2 / 1 converted) */
static void proj_fetch(uint32_t slot)
{
    project_store_t *q = &proj_slot[slot & 3u];
    int n;
    proj_wire_gen++;
    n = st_load(proj_obj(slot), &proj_wire, sizeof proj_wire);
    if (!proj_import(&proj_scratch, &proj_wire, n))
        memset(q->raw, 0, 4);
    else {
        proj_bound(&proj_scratch);
        if (!proj_pack(q, &proj_scratch))
            memset(q->raw, 0, 4);
    }
}
#endif

static void project_capture(project_t *p)
{
    uint32_t i;
    uint32_t f = motion_guard();
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    p->chain = chain_config;
    for (i = 0; i < NTRK; i++) {
        for (uint32_t j = 0; j < P_COUNT; j++) p->t[i].p[j] = motion_base_value(&trk[i], j);
        p->t[i].engine = trk[i].eng_req;
        p->t[i].preset = trk[i].preset;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
        fm6_pack(fm6_patch[i], p->fm6[i]);
    }
    p->motion = motion;
    memcpy(p->dx, dx_kit, sizeof p->dx);
    p->dx_mute = dx_mute;
    memcpy(p->dx_mot, dx_mot, sizeof p->dx_mot);
    p->dx_tone = dx_tone;
    memcpy(p->dx_snd, dx_snd, sizeof p->dx_snd);        /* (0.6.48) */
    memcpy(p->dx_lay, dx_lay, sizeof p->dx_lay);        /* (0.6.49) */
    memcpy(p->dx_mdl, dx_mdl, sizeof p->dx_mdl);        /* (1.0-GW) */
    {                                                   /* (0.6.52) FUGUE */
        uint32_t a, h;
        for (a = 0; a < NTRK; a++)
            for (h = 0; h < FG_HEADS; h++) {
                p->fugue[a][h][0] = (uint8_t)(fg_set[a][h].rate | fg_set[a][h].dir << 4);
                p->fugue[a][h][1] = (uint8_t)fg_set[a][h].ivl;
            }
    }
    memcpy(p->pfx_lane, pfx_lane, sizeof p->pfx_lane);
    p->pfx_ltgt = pfx_ltgt;
    memcpy(p->macro, macro_v, sizeof p->macro);
    motion_unguard(f);
    memcpy(p->name, proj_name, str_len(proj_name));
    p->sum = proj_sum(p);
}

/* the music as it is now -> slot, named `name` (0: the current name; "" none); 0 saved, nonzero refused or failed.
 * The current name becomes the saved one */
static int project_save_as(uint32_t slot, const char *name)
{
    project_t *p = &proj_scratch;
    if (transport_busy()) {                            /* a flash erase silences the audio and stalls the */
        ui_message("STOP TO SAVE");                     /* sequencer (storage_hw.c): only while stopped */
        return 1;
    }
    project_capture(p);
    if (name) {
        memset(p->name, 0, sizeof p->name);
        memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    }
    proj_wire_gen++;
    if (!proj_pack(&proj_wire, p)) { ui_message(proj_full ? "SECTION FULL" : "SAVE FORMAT ERROR"); return 2; }

#if FELUCCA_FLASH
    if (flash_ok) {
        if (st_save(proj_obj(slot), &proj_wire, sizeof proj_wire)) {
            ui_message("SAVE ERROR");
            return 2;
        }
        memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
        proj_name_get(proj_name, (const uint8_t *)p->name);
        proj_cur = (uint8_t)(slot & 3u);
        live.cur = (int8_t)(slot & 3u);                 /* (0.6.12: the section playing, SAVE > PROJECT shows it) */
        live.dirty &= (uint8_t)~(1u << (slot & 3u));    /* (stored in flash now) */
        ui_message("SAVED");
        return 0;
    }
#endif
    memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = (uint8_t)(slot & 3u);
    live.cur = (int8_t)(slot & 3u);
    ui_message("SAVED (NO FLASH)");
    return 0;
}
static int project_save(uint32_t slot) { return project_save_as(slot, 0); }
static void project_cur_name(char *b) { str_cpy(b, proj_name, PROJ_NAME_LEN + 1u); }   /* b: 13 bytes */

/* slot's name -> b (PROJ_NAME_LEN + 1 bytes); 0 = an empty slot (b ""). Uses proj_scratch */
static int project_name(uint32_t slot, char *b)
{
    b[0] = 0;
    if (!proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t)))
        return 0;
    proj_name_get(b, (const uint8_t *)proj_scratch.name);
    return 1;
}

/* a stored project renamed in place (nothing else of it changes; the music playing is not touched, but the slot
 * it was loaded from / saved to: its name is the new one, the next SAVE's prefill): 0 done, 1 refused (playing,
 * an empty slot), 2 failed (the slot as it was) */
static int project_rename(uint32_t slot, const char *name)
{
    project_t *p = &proj_scratch;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 1;
    }
    if (!proj_import(p, &proj_slot[slot & 3u], sizeof(project_store_t))) {
        ui_message("EMPTY SLOT");
        return 1;
    }
    memset(p->name, 0, sizeof p->name);
    memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    proj_wire_gen++;
    if (!proj_pack(&proj_wire, p)) { ui_message(proj_full ? "SECTION FULL" : "SAVE FORMAT ERROR"); return 2; }
#if FELUCCA_FLASH
    if (flash_ok && st_save(proj_obj(slot), &proj_wire, sizeof proj_wire)) {
        ui_message("SAVE ERROR");
        return 2;
    }
#endif
    memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
    if (proj_cur == (slot & 3u))
        proj_name_get(proj_name, (const uint8_t *)p->name);
#if FELUCCA_FLASH
    if (flash_ok) { ui_message("RENAMED"); return 0; }
#endif
    ui_message("RENAMED (NO FLASH)");
    return 0;
}

/* the section to RECALL from: the one playing (stored or jumped to), else the slot the music was loaded from or
 * saved to; -1 none (or that slot empty) */
static int project_recall_slot(void)
{
    int s = live.cur >= 0 ? live.cur : proj_cur < 4u ? (int)proj_cur : -1;
    return s >= 0 && project_used((uint32_t)s) ? s : -1;
}

/* EDIT layer RECALL (JIANT): track k's sound as section slot stored it, the safe state to come back to: its engine
 * and sound (the parameters a sound load sets: not LEVEL PAN MUTE, the pattern's, the SLICER: param_kept), its FM6
 * patch; a DRUM track also the section's DRUM-X kit. The steps and the motion stay. 0 done, 1 nothing stored there */
static void sound_put(project_t *p, uint32_t k);
static int project_recall_sound(uint32_t slot, uint32_t k)
{
    project_t *p = &proj_scratch;
    if (slot > 3u || !proj_import(p, &proj_slot[slot], sizeof(project_store_t)) || !proj_ok(p) || !proj_engines_ok(p))
        return 1;
    sound_put(p, k);
    return 0;
}
/* track k's sound (engine, sound parameters, FM6 patch; DRUM: the DRUM-X kit) from project p; the rest plays on */
static void sound_put(project_t *p, uint32_t k)
{
    track_t *t = &trk[k % NTRK];
    const proj_trk_t *s;
    uint32_t i, e;
    proj_phys(p);                                       /* (as project_restore_runtime: the old formats' tracks) */
    proj_fm4(p);
    proj_perc(p);
    proj_phys_gone(p);
    s = &p->t[k % NTRK];
    e = s->engine;
    load_begin(t, LOAD_SOUND);
    panic_req |= (uint8_t)(1u << (k % NTRK));
    fm1_irq_off();
    t->eng_req = (uint8_t)e;
    t->user = 0;
    t->preset = (uint8_t)(ENGINES[e]->npresets ? (s->preset >= PROJ_DEF_KEEP ? 0u : s->preset) % ENGINES[e]->npresets : 0u);
    for (i = 0; i < P_COUNT; i++)
        if (!param_kept(i))
            t->p[i] = (int16_t)param_fit(param_desc_of(e, i), s->p[i]);
    {
        uint8_t v[FP_SIZE + 1u];
        fm6_unpack(p->fm6[k % NTRK], v);
        fm6_set_patch(k % NTRK, v);
        fm6_adopt(k % NTRK);
    }
    if (e == ENGI_DRUM) {
        memcpy(dx_kit, dx_kit_ok(p->dx) ? p->dx : DX_KIT_DEF, sizeof dx_kit);
        memcpy(dx_mot, p->dx_mot, sizeof dx_mot);
        dx_tone = p->dx_tone;
        memcpy(dx_snd, p->dx_snd, sizeof dx_snd);
        memcpy(dx_lay, p->dx_lay, sizeof dx_lay);
        memcpy(dx_mdl, p->dx_mdl, sizeof dx_mdl);
    }
    fm1_irq_on();
    if (t == TSEL)
        sync_reload = 1;
    load_end(t);
    ui.force = 1;
}

/* (0.6.52) a section's FUGUE settings into the heads' (fugue.c fg_set) */
static void fg_load(const project_t *p)
{
    uint32_t a, h;
    for (a = 0; a < NTRK; a++)
        for (h = 0; h < FG_HEADS; h++) {
            uint32_t r = p->fugue[a][h][0] & 15u;
            fg_set[a][h].rate = (uint8_t)(r < FG_NRATE ? r : 0u);
            fg_set[a][h].dir = (uint8_t)((p->fugue[a][h][0] >> 4) & 3u);
            fg_set[a][h].ivl = (int8_t)clamp((int32_t)(int8_t)p->fugue[a][h][1], -14, 14);
        }
}
static uint8_t proj_lfo_keep;                          /* (1.2.1-GW) song_main.c section_load (a song's next section),
                                                         * undo_do: the music going on, the LFOs' FADE not again */
static int project_restore_runtime(const project_t *input)
{
    project_t *p = &proj_scratch;
    uint32_t i, k;
    if (!proj_ok(input) || !proj_engines_ok(input)) return 1;
    if (p != input) memcpy(p, input, sizeof *p);
    proj_drums_to_part(p);                              /* a RAM slot of firmware before 1.0 */
    proj_phys(p);                                       /* .. before PHYS lost DUST and DRUM */
    proj_fm4(p);                                        /* .. that had DIGITAL tracks */
    proj_perc(p);                                       /* .. or SAMPLE PERC tracks (DRUM) */
    proj_phys_gone(p);                                  /* .. or tracks of a retired engine (core.h eng_gone: ANALOG) */
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    seq_stop();
    transport_req = 0;
    chain.ended = 0;                                    /* (a song stopped by this load: the load wins, song_poll keeps out) */
    memcpy(dx_kit, dx_kit_ok(p->dx) ? p->dx : DX_KIT_DEF, sizeof dx_kit);   /* (the section's DRUM-X kit) */
    dx_mute_set(p->dx_mute);
    memcpy(dx_mot, p->dx_mot, sizeof dx_mot);           /* (0.5.1: the kit's X-MOD) */
    dx_tone = p->dx_tone;                               /* (0.6.36: its TONE) */
    memcpy(dx_snd, p->dx_snd, sizeof dx_snd);           /* (0.6.48: its SENDs) */
    memcpy(dx_lay, p->dx_lay, sizeof dx_lay);           /* (0.6.49: its LAYERs) */
    memcpy(dx_mdl, p->dx_mdl, sizeof dx_mdl);           /* (1.0-GW: its MODELs) */
    fg_load(p);                                         /* (0.6.52: its FUGUE) */
    memcpy(pfx_lane, p->pfx_lane, sizeof pfx_lane);    /* (the section's punch-in lane) */
    pfx_ltgt = p->pfx_ltgt > 2u ? 0u : p->pfx_ltgt;
    for (i = 0; i < 4u; i++)
        macro_v[i] = p->macro[i] & 127u;            /* (the macros: a load sets them; a section jump keeps them) */
                                                        /* (the rows are the song's, not a section's: kept) */
    motion = p->motion;
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = 0;
    ui.song_row = 0;
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = s->engine;                         /* (< NENGINES: proj_engines_ok) */
        t->eng_req = (uint8_t)e;
        t->user = 0;                                    /* (no user preset slot is saved) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range (param_fit) */
            const param_desc_t *d = param_desc_of(e, i);
            t->p[i] = (int16_t)param_fit(d, s->p[i]);
        }
        t->preset = (uint8_t)(ENGINES[e]->npresets ? (s->preset >= PROJ_DEF_KEEP ? 0u : s->preset) % ENGINES[e]->npresets : 0u);
        memcpy(t->step, s->step, sizeof t->step);
        proj_steps(t->step);
        {   /* the project's own FM6 patch, never reloaded from SLOT: F n if it is that factory patch, else OWN */
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(p->fm6[k], v);
            fm6_set_patch(k, v);
            fm6_adopt(k);
        }
    }
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    if (!proj_lfo_keep)
        lfo_fade_restart((1u << NTRK) - 1u);            /* (1.2.1-GW) the LFOs' FADE from 0 (FREE: only now) */
    fm1_irq_on();
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = PROJ_NO_SLOT;                            /* (project_load: its slot) */
    load_depth++;                                       /* (ui.c) these loads are the project's, not the user's */
    for (k = 0; k < NTRK; k++) {                        /* the power-on sounds: format 1 (tracks 2..4), old drums */
        track_t *t = &trk[k];
        int16_t keep[P_COUNT];
        if (p->t[k].preset == PROJ_DEF_SOUND) {
            apply_preset_to(t, TRK_DEF[k][1]);
            track_defaults_steps(t);
        } else if (p->t[k].preset == PROJ_DEF_KEEP) {   /* the steps, LEVEL PAN MUTE, LEN DIV SWING GATE kept */
            memcpy(keep, t->p, sizeof keep);
            fm1_irq_off();                           /* publish the legacy sound as one bounded parameter batch */
            proj_legacy_drums(t);                     /* (keeps the SLICER: param_kept) */
            t->p[P_REV] = keep[P_REV];                  /* and the drums' reverb send */
            for (i = P_AMODE; i <= P_TRANS; i++)        /* the drum part had no arp or scale */
                t->p[i] = TP[i].def;
            fm1_irq_on();
        }
        pat_sig[k] = ~steps_sig(t);                     /* a project's steps are the user's */
    }
    load_depth--;
    sync_reload = 1;
    ui.force = 1;
    ui_message("LOADED");
    return 0;
}
/* (0.6.21) UNDO (GLO held + G4): one step back over the last big change (HOME's DICE ALL, a sound diced, a clear, an
 * INIT / RECALL, a pattern or project loaded). undo_mark keeps the music as it is (packed as a project: ~3.8 KB);
 * undo_do brings it back and keeps what it replaced, so a second UNDO goes forward again. The transport plays on */
static project_store_t undo_st[2] __attribute__((section(".lowbuf")));   /* (0.6.78: app.ld LOWBUF) */
/* (JIANT 0.6.40) INIT SYSTEM (SAVE > TOOLS 5, its dialog; UNDO brings the music back) and the power-on (main.c, with
 * RESTORE LAST OFF): the six tracks their engine (TRK_DEF's) at its defaults and first sound, as INIT SOUND does; no
 * steps, automation or locks; the mix, the global effects, the tempo and the SLICER at their defaults; the song empty,
 * the macros at 0, the punch-in lane clear, the factory DRUM-X kit (X-MOD, TONE off, nothing muted), every FM6 patch
 * the init voice. What is saved (projects, user presets) and the setup (CLOCK, TUNE, MIDI, ROUTE, the project slot,
 * MENU) stay */
static void system_init(void)
{
    static const uint8_t KEEP[5] = {G_CLOCK, G_TUNE, G_MIDI, G_ROUTE, G_SLOT};
    int16_t kept[5];
    uint32_t i;
    for (i = 0; i < 5u; i++) kept[i] = song.g[KEEP[i]];
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < 5u; i++) song.g[KEEP[i]] = kept[i];
    chain_defaults(&chain_config);
    memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
    memset(pfx_lane, 0, sizeof pfx_lane);
    for (i = 0; i < 4u; i++) macro_v[i] = 0;
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    memset(dx_mot, 0, sizeof dx_mot);
    dx_tone = 0;
    memset(dx_snd, 0, sizeof dx_snd);
    memcpy(dx_lay, DX_LAY_DEF, sizeof dx_lay);
    memset(dx_mdl, 0, sizeof dx_mdl);
    memset(fg_set, 0, sizeof fg_set);                   /* (0.6.52) no FUGUE */
    dx_mute_set(0);
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        motion_clear(t);
        load_begin(t, LOAD_SOUND | LOAD_PAT);
        track_defaults(t);
        init_sound_of(t, trk_def_engine(i));          /* (its defaults and first sound, dry; FM6 the init voice) */
        motion_clear(t);
        load_end(t);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = 0;
        latch_drop_req |= (uint8_t)(1u << i);
    }
    proj_name[0] = 0;
    ui.force = 1;
}
/* (JIANT 0.6.43) MUTATE: each track's SOUND and PATTERN change by themselves every OFF / 1 .. 32 bars, on the bar (the
 * change made in the bar before's last sixteenth, so it lands on the one), by AMOUNT, in the clock with the rest of the
 * gear. ORBIT (the default): around the anchor, the music as it was when MUTATE came on, PLAY started or KEEP (the
 * MUTATE page's OCT+) took it, so it varies for minutes and stays itself; DRIFT: from what it is now, on and on.
 * SOUND: the engine's eight values and the filter, its envelope, DEC, REL and the sends (never LEVEL, PAN, MUTE; FM6's
 * SLOT stays); a list value now and then. PATTERN: notes up / down the scale, accents, a note added off the beat or
 * one taken (never on a beat); DRUM: hits on and off, never the kick. GLO + A4 freezes them all; main loop only */
static project_store_t mut_anchor __attribute__((section(".pool")));
static struct { uint8_t ok, playing; uint32_t en, done, sig[NTRK]; } mut;   /* sig: each track's steps as the anchor or
                                                         * the last mutation left them (0.6.68) */
static uint32_t mut_every(const track_t *t, uint32_t id) { return MUT_BARS[(uint32_t)clamp(t->p[id], 0, 6)]; }
static uint32_t mut_enabled(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < NTRK; k++)
        if (k < trk_live && (mut_every(&trk[k], P_MSND) || mut_every(&trk[k], P_MPAT)))
            m |= 1u << k;
    return m;
}
static void mut_keep(void)
{
    project_capture(&proj_scratch);
    mut.ok = (uint8_t)proj_pack(&mut_anchor, &proj_scratch);
    for (uint32_t k = 0; k < NTRK; k++)
        mut.sig[k] = steps_sig(&trk[k]);
}
static int32_t mut_rr(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }
static void mut_value(track_t *t, uint32_t id, const int16_t *base, uint32_t amt)
{
    const param_desc_t *d = track_desc(t, id);
    int32_t v = base[id];
    if (d->max <= d->min)
        return;
    if (d->fmt == F_ENUM)
        v = rng() % 254u < amt ? mut_rr(d->min, d->max) : v;
    else {
        int32_t span = (d->max - d->min) * (int32_t)amt / 256;
        v += mut_rr(-span, span);
    }
    t->p[id] = (int16_t)clamp(v, d->min, d->max);
}
static void mut_sound(track_t *t, const proj_trk_t *a)
{
    static const uint8_t COMMON[7] = {P_FCUT, P_FRES, P_ED_FLT, P_DEC, P_REL, P_DLY, P_REV};
    uint32_t i, amt = (uint32_t)clamp(t->p[P_MAMT], 0, 127), e = t->eng_req % NENGINES;
    const int16_t *base = a ? a->p : t->p;
    for (i = 0; i < 8u; i++)
        if (!(e == ENGI_FM6 && i == 7u))                /* (FM6's SLOT: its patch stays) */
            mut_value(t, P_E0 + i, base, amt);
    for (i = 0; i < 7u; i++)
        if (!(e == ENGI_ANALOG && (COMMON[i] == P_FCUT || COMMON[i] == P_FRES)))   /* (ANALOG: its own CUT RES) */
            mut_value(t, COMMON[i], base, amt);
}
static void mut_pattern(track_t *t, const proj_trk_t *a)
{
    uint32_t i, len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, amt = (uint32_t)clamp(t->p[P_MAMT], 0, 127), l;
    int kit = drum_track(t);
    int32_t last = -1;
    for (i = 0, l = 0; i < len && i < NSTEP; i++)       /* (0.6.68) an empty pattern stays empty: nothing out of */
        l += a ? step_on(&a->step[i]) : step_on(&t->step[i]);   /* silence (a cleared track, a blank kit) */
    if (!l)
        return;
    for (i = 0; i < len && i < NSTEP; i++) {
        step_t s = a ? a->step[i] : t->step[i];
        if (kit) {                                      /* hits on and off, the kick never */
            for (l = 1; l < NLANE; l++)
                if (rng() % 640u < amt) {
                    s.hit ^= (uint8_t)(1u << l);
                    s.acc &= s.hit;
                }
            if (s.hit && s.time != ST_NOTE) s.time = ST_NOTE;
            if (!s.hit && !s.n) s.time = ST_REST;
        } else if (s.time == ST_NOTE && s.n) {
            uint32_t j;
            if (i % 4u && rng() % 1024u < amt) {        /* one taken (never on a beat) */
                s.n = 0; s.time = ST_REST;
            } else {
                if (rng() % 256u < amt) {               /* up / down the scale */
                    int32_t d = mut_rr(-2, 2);
                    if (!d) d = 1;
                    for (j = 0; j < s.n && j < 4u; j++)
                        s.note[j] = (uint8_t)clamp(scale_steps(t, s.note[j], d), 0, 127);
                }
                if (rng() % 512u < amt) s.flags ^= SF_ACCENT;
                last = s.note[0];
            }
        } else if (s.time == ST_REST && (i & 1u) && last >= 0 && rng() % 1024u < amt) {   /* one added, off the beat */
            memset(&s, 0, sizeof s);
            s.n = 1; s.time = ST_NOTE; s.vel = 90;
            s.note[0] = (uint8_t)clamp(scale_steps(t, last, mut_rr(-1, 1)), 0, 127);
        }
        t->step[i] = s;
    }
}
static uint32_t mut_progress(uint32_t k)
{
    const track_t *t = &trk[k % NTRK];
    uint32_t s = mut_every(t, P_MSND), q = mut_every(t, P_MPAT), e = s && q ? (s < q ? s : q) : s ? s : q;
    if (!e || k >= trk_live)
        return 0xFFFFu;
    if (!song.playing || mut_freeze)
        return 0;
    return (((bar_n % e) * 4u + beat_n) * 256u) / (e * 4u);
}
static void mutate_poll(void)
{
    uint32_t en = mut_enabled(), k, nx, anc;
    for (k = 0; k < NTRK; k++)                          /* turned on: an AMOUNT to hear (0: half) */
        if (((en & ~mut.en) >> k) & 1u && !trk[k].p[P_MAMT])
            trk[k].p[P_MAMT] = 64;
    if (en & ~mut.en)
        mut_keep();                                     /* (the anchor: the music now) */
    mut.en = en;
    for (k = 0; k < NTRK; k++)
        if (mut_flash[k]) mut_flash[k]--;
    if (!song.playing) {
        mut.playing = 0;
        return;
    }
    if (!mut.playing) {                                 /* PLAY: the anchor as it starts */
        mut.playing = 1;
        mut.done = 0;
        if (en) mut_keep();
    }
    if (!en || mut_freeze)
        return;
    nx = bar_n + 1u;
    if (beat_n != 3u || beat_pos < beat_samples() * 3u / 4u || mut.done == nx)
        return;
    mut.done = nx;
    for (k = 0; k < NTRK; k++)                          /* (0.6.68) steps written or cleared since: the anchor is the
                                                         * music now (a cleared sequence never comes back from it) */
        if (((en >> k) & 1u) && steps_sig(&trk[k]) != mut.sig[k]) {
            mut_keep();
            break;
        }
    anc = mut.ok && proj_import(&proj_scratch, &mut_anchor, sizeof mut_anchor);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t s = mut_every(t, P_MSND), q = mut_every(t, P_MPAT), hit = 0;
        const proj_trk_t *a = anc && !t->p[P_MMODE] && proj_scratch.t[k].engine == t->eng_req % NENGINES ?
                              &proj_scratch.t[k] : 0;  /* (ORBIT, the same engine: around it; else from now) */
        if (!((en >> k) & 1u))
            continue;
        if (s && nx % s == 0u) { mut_sound(t, a); hit = 1; }
        if (q && nx % q == 0u) { mut_pattern(t, a); hit = 1; }
        if (hit) mut_flash[k] = 12;
    }
    for (k = 0; k < NTRK; k++)
        mut.sig[k] = steps_sig(&trk[k]);
}
static void undo_mark(void)
{
    project_capture(&proj_scratch);
    undo_ok = (uint8_t)(proj_pack(&undo_st[0], &proj_scratch) ? 1u : 0u);
}
static int undo_do(void)                                /* 0: done */
{
    char name[PROJ_NAME_LEN + 1u];
    uint8_t cur = proj_cur, play = song.playing;
    if (!undo_ok)
        return 1;
    project_capture(&proj_scratch);
    if (!proj_pack(&undo_st[1], &proj_scratch) || !proj_import(&proj_scratch, &undo_st[0], sizeof undo_st[0]))
        return 1;
    str_cpy(name, proj_name, sizeof name);
    {
        int rc;
        proj_lfo_keep = 1;                              /* (1.2.1-GW: it plays on, FREE LFOs not faded again) */
        rc = project_restore_runtime(&proj_scratch);
        proj_lfo_keep = 0;
        if (rc)
            return 1;
    }
    proj_cur = cur;                                     /* (the project's slot and name stay) */
    str_cpy(proj_name, name, sizeof proj_name);
    undo_st[0] = undo_st[1];                            /* (UNDO again: what it replaced) */
    if (play)
        transport_req = 1;                              /* (the restore stopped it: it plays on) */
    return 0;
}

/* (0.6.65) the sound dice's CANCEL (ui_input.c CF_DICE_SOUND): track k's sound back from the undo mark taken just
 * before the dice; only that track's sound, so the transport and the rest play on. 0 done */
static int undo_sound(uint32_t k)
{
    if (!undo_ok || !proj_import(&proj_scratch, &undo_st[0], sizeof undo_st[0]))
        return 1;
    sound_put(&proj_scratch, k);
    return 0;
}

static void project_load(uint32_t slot)
{
#if FELUCCA_FLASH
    if (flash_ok && !proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t))) proj_fetch(slot);
#endif
    if (!proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t))) { ui_message("EMPTY SLOT"); return; }
    proj_bound(&proj_scratch);                          /* a retained RAM slot of an older format: as from flash */
    if (!project_restore_runtime(&proj_scratch)) {
        proj_cur = (uint8_t)(slot & 3u);
        live.cur = (int8_t)(slot & 3u);                 /* (0.6.12: the section playing) */
    }
}

/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
#include "settings_persist.c"
#if FELUCCA_FLASH
static persist_t persist_saved;
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
#endif

static void song_index_load(void);                /* song_main.c */
static void song_index_boot(void);

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (settings_import(&p, n)) {
            persist_saved = p;
            settings_jiant_once(&p);               /* (0.6.81: the saved copy without the mark: saved again later) */
        }
    }
    song_index_load();                             /* the current song: whose sections fill the slots */
    {   /* projects: fill empty RAM slots from flash, so the slot list is right after power-on */
        uint32_t i;
        for (i = 0; i < 4u; i++)
            if (!proj_import(&proj_scratch, &proj_slot[i], sizeof(project_store_t)))
                proj_fetch(i);
    }
    song_index_boot();                             /* the current song's rows */
    up_boot();                                     /* user presets */
#endif
}

static int project_used(uint32_t slot) { return proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t)); }

#include "song_main.c"                          /* the song: the main loop's part (song_chain.c) */

static void settings_poll(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!persist_pending || !flash_ok || transport_busy() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

/* The autosave (1.2, Discussion #130): the music as it is (what a project holds: every track's sound, steps, LEN DIV
 * SWING GATE, mix, the song, the automation and locks, the FM6 patches, the name) kept in its own storage object
 * (OBJ_AUTOSAVE, A/B at 0xE5000 / 0xE6000, never one of the four project slots), and put back at power-on
 * (autosave_boot) with MENU > SYSTEM > RESTORE LAST ON (the default). Flash wear: a 4 KiB erase per write, the two
 * sectors taking turns, so a write only when
 *   - something a project holds changed since the last one (autosave_sig: a hash of those values, no copy; not the
 *     selected track, nor the PROJECT page's own controls),
 *   - and it has stayed the same for AS_IDLE_MS (10 s) with the transport stopped, no key or button down, no voice
 *     sounding and no editor transfer (a backup or a sample: autosave_hold); an erase stops the audio for its
 *     ~40 ms, as every save does, so it never happens while anything plays,
 *   - and at least AS_GAP_MS (60 s) after the one before (AS_RETRY_MS after a failed write).
 * So at most one write a minute while editing, none while playing or idle: about 100,000 erase cycles per sector
 * give over 200,000 writes, a write a minute for 8 hours a day for over a year, far more with real pauses.
 * RESTORE LAST OFF writes none (and restores none). Never after a crash, a hang or a failed boot (main.c): the music
 * that was playing then could be what crashed. Checked every AS_POLL_MS; the main loop only (never the audio ISR) */
#define AS_POLL_MS 250u
#define AS_IDLE_MS 10000u
#define AS_GAP_MS 60000u
#define AS_RETRY_MS 30000u
static struct {
    uint32_t saved, seen;                        /* the signature written last (or loaded at power-on), seen last */
    uint32_t t, last, poll;                      /* fm1_ms: seen changed / busy, the last write, the last check */
    uint32_t writes;                             /* (written since power-on: the host tests count them) */
    uint8_t wrote, err;                          /* a write since power-on (last is valid); the last one failed */
} as __attribute__((section(".pool")));

static uint32_t as_mix(uint32_t h, const void *p, uint32_t n)   /* FNV-1a, continued */
{
    const uint8_t *b = p;
    while (n--) {
        h ^= *b++;
        h *= 16777619u;
    }
    return h;
}

/* what an autosave would hold, hashed (no copy, no RAM): project_capture's values */
static uint32_t autosave_sig(void)
{
    uint32_t h = 2166136261u, i, j;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        for (j = 0; j < P_COUNT; j++) {
            int16_t v = motion_base_value(t, j);
            h = as_mix(h, &v, sizeof v);
        }
        h = as_mix(h, &t->eng_req, 1);
        h = as_mix(h, &t->preset, 1);
        h = as_mix(h, t->step, sizeof t->step);
        h = as_mix(h, fm6_patch[i], sizeof fm6_patch[i]);
    }
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_NAME && i != G_LOAD && i != G_SAVE)
            h = as_mix(h, &song.g[i], sizeof song.g[i]);
    h = as_mix(h, &motion, sizeof motion);
    h = as_mix(h, &chain_config, sizeof chain_config);
    return as_mix(h, proj_name, sizeof proj_name);
}

/* nothing going on that an erase (the audio stopped ~40 ms) would be heard in, or would race */
static int autosave_quiet(void)
{
    uint32_t k, i;
    if (transport_busy() || fm1_in.notes || fm1_in.buttons)
        return 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active)
                return 0;
    return 1;
}

static void autosave_hold(void) { as.t = fm1_ms; }   /* the editor's transfers: their staging RAM is proj_wire */

/* main loop, every pass (after settings_poll) */
/* (JIANT 0.6) how full the music is for a section (FUNC): 1.5 s after the last change, once, said at 90 % and when it
 * no longer fits (a section store or a save would say SECTION FULL; what is stored stays). Main loop, autosave_poll */
static struct { uint32_t sig, t; uint8_t due, said; } proj_fill;
static uint32_t proj_fill_pct(void)
{
    project_capture(&proj_scratch);
    return proj_packed_size(&proj_scratch) * 100u / PROJC_PFX_OFF;
}
static void proj_fill_poll(uint32_t sig)
{
    if (sig != proj_fill.sig) {
        proj_fill.sig = sig;
        proj_fill.t = fm1_ms;
        proj_fill.due = 1;
    } else if (proj_fill.due && fm1_ms - proj_fill.t >= 1500u) {
        uint32_t pct = proj_fill_pct();
        proj_fill.due = 0;
        if (pct > 100u && proj_fill.said < 2u)
            ui_message("TOO FULL TO SAVE");
        else if (pct >= 90u && pct <= 100u && !proj_fill.said)
            ui_message("SECTION 90% FULL");
        proj_fill.said = (uint8_t)(pct > 100u ? 2u : pct >= 90u ? (proj_fill.said ? proj_fill.said : 1u) : pct < 85u ? 0u : proj_fill.said);
    }
}
static void autosave_poll(void)
{
    static uint32_t fill_poll;
    if (fm1_ms - fill_poll >= 250u) {                   /* (JIANT 0.6) how full the music is, flash or not */
        fill_poll = fm1_ms;
        proj_fill_poll(autosave_sig());
    }
#if FELUCCA_FLASH
    uint32_t sig;
    if (!flash_ok || fm1_ms - as.poll < AS_POLL_MS)
        return;
    as.poll = fm1_ms;
    sig = autosave_sig();
    if (sig != as.seen || !autosave_quiet()) {
        as.seen = sig;
        as.t = fm1_ms;
        return;
    }
    if (sig == as.saved || !(ui_prefs & PREF_RESTORE_ON) || fm1_ms - as.t < AS_IDLE_MS ||
        (as.wrote && fm1_ms - as.last < (as.err ? AS_RETRY_MS : AS_GAP_MS)))
        return;
    as.last = fm1_ms;
    as.wrote = 1;
    project_capture(&proj_scratch);
    proj_wire_gen++;
    as.err = !proj_pack(&proj_wire, &proj_scratch) || st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) != 0;
    if (!as.err) {
        as.saved = sig;
        as.writes++;
    }
#endif
}

/* power-on, after the defaults (main.c): the autosave becomes the music when `allowed` (no crash, hang or failed boot
 * before) and RESTORE LAST is ON; an autosave that does not load (none, torn, damaged, another format) is ignored.
 * The music now (restored or not) is what is saved: nothing is written until it changes. 1 = restored */
static int autosave_boot(int allowed)
{
    int rc = 0;
#if FELUCCA_FLASH
    if (flash_ok && allowed && (ui_prefs & PREF_RESTORE_ON)) {
        int n;
        proj_wire_gen++;
        n = st_load(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire);
        if (n > 0 && proj_import(&proj_scratch, &proj_wire, n)) {
            proj_bound(&proj_scratch);
            if (!project_restore_runtime(&proj_scratch)) {
                ui_message("RESTORED");
                rc = 1;
            }
        }
    }
#else
    (void)allowed;
#endif
    as.saved = as.seen = autosave_sig();
    as.t = fm1_ms;
    return rc;
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_store_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif
#endif /* PROJ_HOST */
