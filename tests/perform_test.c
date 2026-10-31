/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the FX hold layer's effects (firmware/src/perform.c), same sources as the firmware (through
 * hostsim.c).   build/host/perform_test [DEMO_DIR]          (run_tests.sh: build/perform_demo)
 * 1. timing: REPEAT starts on the next 1/16 of the transport (sample exact) and ends when let go (the live
 *    signal back, bit for bit, after the 2.9 ms ramp); the filters start at once.
 * 2. stereo: REPEAT keeps left and right apart (a silent right channel stays silent; each side follows its
 *    own input).
 * 3. a REPEAT too long for the loop (1/8 below 81 BPM) does nothing at all.
 * 4. the SLICER: its recordings are not used meanwhile and are dropped afterwards.
 * 5. the keys: a layer key plays nothing and sends no MIDI; a key held before FX stays a note; the white
 *    keys past the first 5 are the layer's too but do nothing.
 * 6. idle: nothing held, nothing ramping, the mix is bit-identical (and the goldens of regress.c too).
 * 7. no clicks, no overflow; the filters, the CRUSH and THROW macros and the mutes do what they say.
 * 8. cost: instructions per sample of the song, idle and with every effect at once (proc_pid_rusage).
 * 9. the punch-in MIDI effects (pfx.c): OCT+ doubles a held note's frequency and lets it back; A#4 DRM leaves a synth
 *    alone; DEC- shortens a kick's tail (DECY back after the block); STUTTER 1/16 strikes a kick 4 times a beat;
 *    1/2 TEMPO slows the sequencer and, let go, puts it where it would have been; RANDOM moves some notes.
 * Demos (WAV) into DEMO_DIR. TONIC removed REVERSE, TAPE STOP, FREEZE and OCT UP / DN (perform.c). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int check(const char *what, int ok)
{
    printf("perform: %-73s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* ------------------------------------------------------ a test signal --- */
#define NT (6u * 44100u)
static int32_t in_l[NT], in_r[NT], out_l[NT], out_r[NT];
static void test_signal(double fl, double al, double fr, double ar)
{
    uint32_t t;
    for (t = 0; t < NT; t++) {
        in_l[t] = (int32_t)lrint(al * sin(2 * M_PI * fl * t / FS));
        in_r[t] = (int32_t)lrint(ar * sin(2 * M_PI * fr * t / FS + 1.0));
    }
}
static void perf_reset(void)
{
    memset(&pf, 0, sizeof pf);
    pf.src = pf.next = PF_N;
    pf.lc = PF_TOP;
    for (uint32_t m = 0; m < NTRK; m++) pf.mg[m] = 32768;
    perf_held = perf_act = 0;
    perf_kill = 0;
    memset((void *)perf_k, 0, sizeof perf_k);
    perf_latched = 0;
    pfx_tgt = 0;
    pfx_any = 0;
    sl_lent = 0;
    slicer_reset();
}
/* the stage alone on the test signal: samples [t0, t1); ev: press (+e + 1) / let go (-(e + 1)) at block bt */
typedef struct { uint32_t t; int e; } ev_t;
static int busy_seen;
static int8_t run_crush;                           /* KNOB 2 (CRUSH) during run(), 0 = untouched */
static int8_t run_k4;                              /* KNOB 4 (DEPTH) during run() */
static void run(uint32_t bpm, int playing, const ev_t *ev, uint32_t nev, uint32_t t1)
{
    uint32_t t, k = 0;
    host_tracks_init();
    perf_reset();
    perf_k[1] = run_crush;
    perf_k[3] = run_k4;
    song.g[G_BPM] = (int16_t)bpm;
    song.playing = (uint8_t)playing;
    perf_start();
    busy_seen = 0;
    for (t = 0; t < t1; t += CTL) {
        while (k < nev && ev[k].t <= t) {
            perf_press((uint32_t)(ev[k].e > 0 ? ev[k].e - 1 : -ev[k].e - 1), ev[k].e > 0);
            k++;
        }
        memcpy(out_l + t, in_l + t, CTL * 4);
        memcpy(out_r + t, in_r + t, CTL * 4);
        clk_pos = t % div_samples(2);              /* (the transport's 1/16 at the block's start, as events_block leaves it) */
        clk_n = t / div_samples(2);
        if (perf_begin(CTL)) {
            busy_seen = 1;
            perf_block(out_l + t, out_r + t, CTL);
        }
    }
}
#define P16 5512u                                  /* a 1/16 at 120 BPM */
static int same(uint32_t a, uint32_t b)            /* out == in on [a, b), both sides */
{
    uint32_t t;
    for (t = a; t < b; t++)
        if (out_l[t] != in_l[t] || out_r[t] != in_r[t])
            return 0;
    return 1;
}
static int32_t err_vs(uint32_t a, uint32_t b, int32_t (*src)(const int32_t *, uint32_t), int side)
{
    uint32_t t;
    int32_t e = 0;
    for (t = a; t < b; t++) {
        int32_t d = abs((side ? out_r : out_l)[t] - src(side ? in_r : in_l, t));
        e = d > e ? d : e;
    }
    return e;
}
static uint32_t shift_d;                           /* the loop: in[t - shift_d] */
static int32_t delayed(const int32_t *x, uint32_t t) { return x[t - shift_d]; }
static int32_t peak(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    for (; a < b; a++)
        m = abs(x[a]) > m ? abs(x[a]) : m;
    return m;
}

/* ------------------------------------------------------------ 1. timing --- */
static int test_timing(void)
{
    int bad = 0;
    char what[160];
    test_signal(440, 12000, 277, 9000);
    {   /* REPEAT 1/16: pressed mid-1/16, records the next 1/16, then loops it */
        ev_t ev[] = {{992, PF_R16 + 1}, {20000, -(PF_R16 + 1)}};
        int32_t el, er, el2;
        run(120, 1, ev, 2, 40000);
        shift_d = P16;
        el = err_vs(2 * P16 + 160, 3 * P16 - 160, delayed, 0);
        er = err_vs(2 * P16 + 160, 3 * P16 - 160, delayed, 1);
        shift_d = 2 * P16;
        el2 = err_vs(3 * P16 + 160, 20000 - 160, delayed, 0);
        snprintf(what, sizeof what, "REPEAT 1/16: live until the 1/16 after the press + one 1/16 (exact)");
        bad += check(what, same(0, 2 * P16));
        snprintf(what, sizeof what, "  then the 1/16 from that 1/16, looped (error L %d R %d, 2nd pass %d of 12000)", el, er, el2);
        bad += check(what, el < 100 && er < 100 && el2 < 100);
        bad += check("  let go: the live signal again after the ramp, bit for bit", same(20000 + 160, 40000));
        bad += check("  the buffer given back, nothing left running", !sl_lent && pf.mode == BM_NONE && !pf.busy);
    }
    {   /* stopped: at once */
        ev_t ev[] = {{992, PF_R16 + 1}};
        run(120, 0, ev, 1, 12000);
        bad += check("stopped: a 1/16 effect starts at once", (perf_act & PF_BIT(PF_R16)) && !same(992, 12000));
    }
    {   /* the immediate ones: LPF moves at once */
        ev_t ev[] = {{992, PF_LPF + 1}};
        run(120, 1, ev, 1, 3000);
        bad += check("LPF starts at once (not on the 1/16)", same(0, 992) && !same(992, 3000));
    }
    return bad;
}

/* ------------------------------------------------------------ 2. stereo --- */
static int test_stereo(void)
{
    static const struct { const char *name; int e; } C[] = {{"REPEAT 1/8", PF_R8}};
    uint32_t c;
    int bad = 0;
    char what[160];
    for (c = 0; c < sizeof C / sizeof C[0]; c++) {
        ev_t ev[] = {{992, C[c].e + 1}, {60000, -(C[c].e + 1)}};
        int32_t pl, pr;
        double cl = 0, cr = 0, nl = 0, nr = 0;
        uint32_t t;
        test_signal(440, 12000, 0, 0);                    /* the right side silent */
        run(120, 1, ev, 2, 70000);
        pl = peak(out_l, 30000, 60000);
        pr = peak(out_r, 0, 70000);
        snprintf(what, sizeof what, "%s: a silent right side stays silent (L %d, R %d)", C[c].name, pl, pr);
        bad += check(what, pr == 0 && pl > 3000);
        test_signal(440, 12000, 3 * 440, 12000);          /* each side its own pitch */
        run(120, 1, ev, 2, 70000);
        for (t = 30000u; t < 50000u; t++) {
            cl += (double)out_l[t] * out_l[t - 1];
            nl += (double)out_l[t] * out_l[t];
            cr += (double)out_r[t] * out_r[t - 1];
            nr += (double)out_r[t] * out_r[t];
        }
        cl = nl > 0 ? cl / nl : 1;
        cr = nr > 0 ? cr / nr : 1;
        snprintf(what, sizeof what, "  each side keeps its own pitch (lag-1 correlation L %.3f, R %.3f)", cl, cr);
        bad += check(what, cl > 0.99 && cr < 0.99 && cr > 0.9 && cl - cr > 0.008);
    }
    return bad;
}

/* -------------------------------------------------------- 3. too long --- */
static int test_too_long(void)
{
    int bad = 0;
    ev_t ev[] = {{992, PF_R8 + 1}, {40000, -(PF_R8 + 1)}};
    test_signal(440, 12000, 277, 9000);
    run(72, 1, ev, 2, 50000);
    bad += check("REPEAT 1/8 at 72 BPM (417 ms, the loop holds 371): nothing happens, bit for bit",
                 same(0, 50000) && !busy_seen && !sl_lent && !(perf_avail() & PF_BIT(PF_R8)));
    run(120, 1, ev, 2, 50000);
    bad += check("REPEAT 1/8 at 120 BPM (250 ms): it plays", !same(0, 40000) && (perf_avail() & PF_BIT(PF_R8)));
    song.g[G_BPM] = 81;
    bad += check("  REPEAT 1/8 fits from 81 BPM, 1/16 from 41",
                 (perf_avail() & PF_BIT(PF_R8)) &&
                 (song.g[G_BPM] = 80, !(perf_avail() & PF_BIT(PF_R8))) &&
                 (song.g[G_BPM] = 41, perf_avail() & PF_BIT(PF_R16)) &&
                 (song.g[G_BPM] = 40, !(perf_avail() & PF_BIT(PF_R16))));
    {   /* a REPEAT held while the tempo slows past its fit stops cleanly */
        uint32_t t;
        int32_t mx = 0;
        host_tracks_init();
        perf_reset();
        song.g[G_BPM] = 120;
        song.playing = 1;
        perf_start();
        test_signal(110, 12000, 110, 12000);
        perf_press(PF_R8, 1);
        for (t = 0; t < 120000u; t += CTL) {
            uint32_t i;
            if (t == 60000u)
                song.g[G_BPM] = 72;
            memcpy(out_l + t % NT, in_l + t % NT, CTL * 4);
            memcpy(out_r + t % NT, in_r + t % NT, CTL * 4);
            if (perf_begin(CTL))
                perf_block(out_l + t % NT, out_r + t % NT, CTL);
            for (i = 1; i < CTL && t > 1000u; i++) {
                int32_t d = abs(out_l[t % NT + i] - out_l[t % NT + i - 1]);
                mx = d > mx ? d : mx;
            }
        }
        perf_press(PF_R8, 0);
        bad += check("  a REPEAT 1/8 held while the tempo drops below its fit: it ends, no click",
                     pf.mode == BM_NONE && !sl_lent && mx < 4 * 190);
    }
    return bad;
}

/* ------------------------------------------------------------ the song --- */
static void song_setup(void)
{
    static const uint8_t ACID[16] = {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50};
    static const uint8_t AM[4] = {57, 60, 64, 67};
    track_t *t1 = &trk[0], *t2 = &trk[1], *td = &trk[3];
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    perf_reset();
    song.g[G_BPM] = 120;
    host_preset(t1, 0, 4);
    host_preset(t2, 1, 5);
    host_drums(td);                                /* DRUM KIT (SAMPLE PERC until 1.0.2) */
    for (i = 0; i < 16u; i++) {
        uint8_t n = ACID[i], d[4], k = 0;
        put_step(t1, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, 0);
        put_step(t2, i, i % 8u == 0u ? 4u : 0u, AM, i % 8u == 0u ? ST_NOTE : ST_TIE, 0);
        if (i % 4u == 0u) d[k++] = 36;
        if (i % 2u == 0u) d[k++] = 42;
        if (i == 4u || i == 12u) d[k++] = 38;
        put_step(td, i, k, d, k ? ST_NOTE : ST_REST, 0);
    }
    trk[0].p[P_PAN] = -40;                         /* the mix itself stereo */
    trk[1].p[P_PAN] = 40;
}
static int32_t song_l[10u * 44100u + CTL], song_r[10u * 44100u + CTL];   /* (room for a 10 s render), and
                                                         * the last block may run past the frames asked */
static void song_render(uint32_t frames, void (*at)(uint32_t t))
{
    uint32_t t, i;
    int32_t o[2 * CTL];
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (at)
            at(t);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            song_l[t + i] = o[2 * i];
            song_r[t + i] = o[2 * i + 1];
        }
    }
    transport_req = 2;
    mix_block(o, CTL);
}

/* ------------------------------------------------------------ 4. SLICER --- */
static int slicer_ok;
static void stut_at(uint32_t t)
{
    uint32_t k;
    if (t == 44032u)
        perf_press(PF_R16, 1);
    (void)k;
    if (t > 50000u && t < 80000u && (sl.rec_on || sl.loop))
        slicer_ok = 0;
    if (t > 50000u && t < 80000u && !sl_lent)
        slicer_ok = 0;
    if (t == 80000u)
        perf_press(PF_R16, 0);
    if (t == 80000u + 320u && (sl_lent || sl.rec || sl.loop))
        slicer_ok = 0;
}
static int test_slicer(void)
{
    uint32_t k;
    song_setup();
    (void)k;
    song.g[G_SLMODE] = SL_STUT;                  /* (JIANT 0.6.3: the bus, everything in) */
    song.g[G_SLPAT] = 1;
    song.g[G_SLRATE] = 1;
    song.g[G_SLDEP] = 127;
    slicer_ok = 1;
    song_render(120000, stut_at);
    return check("SLICER STUT: live (no recording, no repeat) while REPEAT has the buffer, dropped after", slicer_ok);
}

/* -------------------------------------------------------------- 5. keys --- */
static int test_keys(void)
{
    int bad = 0;
    uint32_t mo0, fx = 1u << 3, k = 6;              /* (a button bit for FX; key 6: a white key, B3: LPF) */
    song_setup();
    usb.config = 1;
    mo_r = mo_w = 0;
    kb_mask = perf_mask = fx;
    fm1_in.buttons = fx;
    fm1_in.notes = 1u << k;
    keyboard_block();
    mo0 = mo_w;
    events_block(CTL);
    bad += check("a key pressed while FX is held: no voice, no MIDI, the layer's, its effect held",
                 !busy_now() && mo0 == 0u && (kb_layer >> k) & 1u && (perf_held & PF_BIT(perf_key(k))));
    fm1_in.buttons = 0;                             /* FX let go first: the effect stays with the key */
    keyboard_block();
    bad += check("  FX let go first: the key still holds its effect", (perf_held & PF_BIT(perf_key(k))) != 0u);
    fm1_in.notes = 0;
    keyboard_block();
    bad += check("  let go: no note-off, the effect off", mo_w == 0u && !kb_layer && !perf_held);
    fm1_in.notes = 1u << 7;                         /* a key, then FX: a note, and its note-off later */
    keyboard_block();
    fm1_in.buttons = fx;
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    bad += check("a key held before FX stays a note (note-on and note-off sent)", mo_w == 2u && !kb_layer);
    kb_mask = perf_mask = 0;                                /* no layer now (a menu): keys are notes */
    fm1_in.notes = 1u << 7;
    keyboard_block();
    bad += check("no layer (kb_mask 0): FX held, keys are notes", mo_w == 3u && !kb_layer);
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    keyboard_block();
    {   /* the white keys past the first 5 (D4 on), and black keys past the mutes: the layer's, nothing */
        uint32_t q, ok = 1, assigned = 0;
        for (q = 0; q < 27u; q++)
            if (PF_IS(perf_key(q)))
                assigned |= 1u << q;
        kb_mask = perf_mask = fx;
        mo0 = mo_w;
        for (q = 0; q < 27u; q++) {
            if ((assigned >> q) & 1u)
                continue;
            fm1_in.buttons = fx;
            fm1_in.notes = 1u << q;
            keyboard_block();
            ok &= (kb_layer >> q) & 1u && kb_note[q] == KB_SILENT && !perf_held && mo_w == mo0;
            fm1_in.notes = 0;
            keyboard_block();
            ok &= !kb_layer && !perf_held && mo_w == mo0;
        }
        fm1_in.buttons = 0;
        bad += check("the keys without an effect (G5, the black keys past the mutes): silent, nothing held",
                     ok && (uint32_t)__builtin_popcount(assigned) == PF_N + 4);   /* (15 white F3 .. F5, 6 mutes, QG-1: 4 black) */
        pfx_tgt = 0;                                /* (A#4 stepped it) */
        kb_mask = perf_mask = 0;
    }
    usb.config = 0;
    return bad;
}

/* -------------------------------------------------------------- 6. idle --- */
static int test_idle(void)
{
    static int32_t a_l[NT], a_r[NT];
    uint32_t t, diff = 0;
    int bad = 0, fd[2];
    pid_t pid;
    song_setup();
    if (pipe(fd) || (pid = fork()) < 0)
        return check("idle: fork", 0);
    if (!pid) {                                     /* the same state, FX held the whole time, no key */
        kb_mask = perf_mask = 1u << 3;
        fm1_in.buttons = 1u << 3;
        song_render(3u * FS, 0);
        close(fd[0]);
        if (write(fd[1], song_l, 3u * FS * 4u) < 0 || write(fd[1], song_r, 3u * FS * 4u) < 0)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    song_render(3u * FS, 0);
    {
        FILE *f = fdopen(fd[0], "rb");
        size_t got = fread(a_l, 4, 3u * FS, f) + fread(a_r, 4, 3u * FS, f);
        fclose(f);
        waitpid(pid, 0, 0);
        diff = got != 6u * FS;
    }
    for (t = 0; t < 3u * FS; t++)
        diff += a_l[t] != song_l[t] || a_r[t] != song_r[t];
    bad += check("idle (FX held, no key): the song bit for bit as without the layer", !diff);
    {   /* after an effect: once its ramps are over, the stage is skipped again */
        uint32_t f;
        int32_t o[2 * CTL];
        perf_press(PF_R16, 1);
        for (f = 0; f < FS; f += CTL)
            mix_block(o, CTL);
        perf_press(PF_R16, 0);
        for (f = 0; f < 512u; f += CTL)
            mix_block(o, CTL);
        bad += check("  after an effect is let go and its ramp is over: skipped again", !pf.busy && !perf_begin(CTL));
    }
    return bad;
}

/* ----------------------------------------------- 7. clicks, the rest --- */
static int test_misc(void)
{
    int bad = 0;
    char what[160];
    static const struct { const char *name; int e; uint32_t off; } C[] = {
        {"REPEAT 1/8", PF_R8, 30000}, {"REPEAT 1/16", PF_R16, 30000}, {"REPEAT 1/32", PF_R32, 30000},
        {"LPF", PF_LPF, 60000}, {"HPF", PF_HPF, 60000}};
    uint32_t c, t;
    int32_t own;
    test_signal(110, 16000, 110, 16000);
    for (own = 0, t = 1; t < NT; t++)
        own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
    for (c = 0; c < sizeof C / sizeof C[0]; c++) {
        ev_t ev[] = {{992, C[c].e + 1}, {C[c].off, -(C[c].e + 1)}};
        int32_t mx = 0;
        run(133, 1, ev, 2, C[c].off + 9000);
        for (t = 1; t < C[c].off + 9000; t++) {
            int32_t d = abs(out_l[t] - out_l[t - 1]);
            mx = d > mx ? d : mx;
        }
        snprintf(what, sizeof what, "no clicks: %s on a 110 Hz sine, in and out: largest step %d (sine %d)", C[c].name, mx, own);
        bad += check(what, mx <= 4 * own);
        bad += check("  back to the live signal, bit for bit", same(C[c].off + 3000, C[c].off + 9000));
    }
    {   /* the filters at their ends; full scale in, no overflow */
        ev_t ev[] = {{0, PF_LPF + 1}};
        test_signal(5000, 30000, 5000, 30000);
        run(120, 1, ev, 1, 100000);
        snprintf(what, sizeof what, "LPF held a bar: a 5 kHz sine down to %d of 30000", peak(out_l, 92000, 100000));
        bad += check(what, peak(out_l, 92000, 100000) < 3000 && peak(out_r, 92000, 100000) < 3000);
        ev[0].e = PF_HPF + 1;
        test_signal(80, 30000, 80, 30000);
        run(120, 1, ev, 1, 100000);
        snprintf(what, sizeof what, "HPF held a bar: an 80 Hz sine down to %d of 30000", peak(out_l, 92000, 100000));
        bad += check(what, peak(out_l, 92000, 100000) < 3000);
        for (t = 0; t < NT; t++) {                 /* full scale square waves, past Q15 */
            in_l[t] = (t / 37u) & 1u ? 120000 : -120000;
            in_r[t] = (t / 53u) & 1u ? 120000 : -120000;
        }
        {
            ev_t e2[] = {{0, PF_LPF + 1}, {0, PF_HPF + 1}, {0, PF_R16 + 1}};
            run_crush = 100;
            run(120, 1, e2, 3, 100000);
            run_crush = 0;
            snprintf(what, sizeof what, "4x full scale through LPF + HPF + KNOB 2 CRUSH + REPEAT: peak %d (no wrap)",
                     peak(out_l, 0, 100000) > peak(out_r, 0, 100000) ? peak(out_l, 0, 100000) : peak(out_r, 0, 100000));
            bad += check(what, peak(out_l, 0, 100000) < 300000 && peak(out_r, 0, 100000) < 300000);
        }
    }
    {   /* KNOB 1 left: a low-pass at once; KNOB 4: the REPEAT's level */
        test_signal(5000, 30000, 5000, 30000);
        host_tracks_init();
        perf_reset();
        song.g[G_BPM] = 120;
        song.playing = 1;
        perf_start();
        perf_k[0] = -100;
        for (t = 0; t < 20000u; t += CTL) {
            memcpy(out_l + t, in_l + t, CTL * 4);
            memcpy(out_r + t, in_r + t, CTL * 4);
            if (perf_begin(CTL))
                perf_block(out_l + t, out_r + t, CTL);
        }
        perf_k[0] = 0;
        snprintf(what, sizeof what, "KNOB 1 FILTER all the way left: a 5 kHz sine down to %d", peak(out_l, 10000, 20000));
        bad += check(what, peak(out_l, 10000, 20000) < 3000);
    }
    {   /* THROW (KNOB 3): the dry mix into the delay and reverb sends; mutes: a track ramps out */
        static int32_t ml[CTL], mr[CTL], sd[CTL], sr[CTL], z[CTL];
        uint32_t i;
        host_tracks_init();
        perf_reset();
        song.playing = 1;
        perf_k[2] = 100;
        for (t = 0; t < 1024u; t += CTL) {
            for (i = 0; i < CTL; i++)
                ml[i] = mr[i] = 8000, sd[i] = sr[i] = 0;
            if (perf_begin(CTL))
                perf_pre(ml, mr, z, z, sd, sr, CTL);
        }
        bad += check("KNOB 3 THROW at 100: the dry mix into both sends, full, the dry mix untouched",
                     sd[CTL - 1] > 7900 && sr[CTL - 1] > 7900 && ml[CTL - 1] == 8000 && mr[CTL - 1] == 8000);
        perf_k[2] = 0;
    }
    {
        int32_t o[2 * CTL];
        uint32_t f;
        int32_t before = 0, during = 0;
        song_setup();
        trk[1].p[P_SLEN] = 1; trk[3].p[P_SLEN] = 1;
        trk[1].step[0].n = 0; trk[3].step[0].n = 0;          /* only track 1 plays, dry */
        trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
        transport_req = 1;
        for (f = 0; f < 2u * FS; f += CTL) {
            uint32_t i;
            if (f == 44096u)
                perf_press(PF_M1, 1);
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                if (f > FS / 2u && f < 44096u) before = abs(o[2 * i]) > before ? abs(o[2 * i]) : before;
                if (f > FS + 512u) during = abs(o[2 * i]) > during ? abs(o[2 * i]) : during;
            }
        }
        perf_press(PF_M1, 0);
        transport_req = 2;
        mix_block(o, CTL);
        snprintf(what, sizeof what, "black key 1: track 1 muted while held (peak %d -> %d), P_MUTE untouched", before, during);
        bad += check(what, before > 1000 && during < 200 && !trk[0].p[P_MUTE]);
    }
    return bad;
}

/* -------------------------------------------------------------- 8. cost --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static double cost_run(int on)
{
    static const int ALL[] = {PF_R16, PF_LPF, PF_HPF, PF_M1 + 2, PF_GATE, PF_SWP};
    uint32_t f, k;
    uint64_t i0;
    int32_t o[2 * CTL];
    song_setup();
    transport_req = 1;
    for (f = 0; f < FS; f += CTL)
        mix_block(o, CTL);
    if (on == 1) {
        for (k = 0; k < sizeof ALL / sizeof ALL[0]; k++)
            perf_press((uint32_t)ALL[k], 1);
        perf_k[1] = perf_k[2] = 100;                /* the CRUSH and THROW macros */
    } else if (on == 3) {                           /* REPEAT 1/16 alone: the layer's own cost, for scale */
        perf_press(PF_R16, 1);
    }
    i0 = instr_now();
    for (f = 0; f < 2u * FS; f += CTL)
        mix_block(o, CTL);
    for (k = 0; k < PF_END; k++)
        perf_press(k, 0);
    perf_k[1] = perf_k[2] = perf_k[3] = 0;
    transport_req = 2;
    return i0 ? (double)(instr_now() - i0) / (2.0 * FS) : 0;
}
static int test_cost(void)
{
    double idle = cost_run(0), on = cost_run(1), rep = cost_run(3);
    char what[200];
    int bad;
    if (!idle) {
        printf("perform: cost: no instruction counter on this host\n");
        return 0;
    }
    snprintf(what, sizeof what, "cost: the song %.0f instructions / sample idle, %.0f with REPEAT+LPF+HPF+MUTE, K2 CRUSH, K3 THROW: +%.0f",
             idle, on, on - idle);
    bad = check(what, on - idle < 400);
    printf("perform: cost: over the idle song: REPEAT 1/16 alone +%.0f (the layer's own stage)\n", rep - idle);
    return bad;
}

/* ------------------------------------------------------------ demos --- */
static void demo_at(uint32_t t)
{
    static const struct { uint32_t t; int e; } D[] = {     /* press +(e + 1), let go -(e + 1) */
        {88192, PF_R8 + 1}, {110080, -(PF_R8 + 1)}, {110080, PF_R16 + 1}, {121088, PF_R32 + 1},
        {126592, -(PF_R32 + 1)}, {132096, -(PF_R16 + 1)}, {176384, PF_LPF + 1}, {264576, -(PF_LPF + 1)},
        {264576, PF_HPF + 1}, {308672, -(PF_HPF + 1)}, {440896, PF_M1 + 1}, {462848, -(PF_M1 + 1)}};
    uint32_t i;
    if (t == 462848u)
        perf_k[2] = 100;                            /* KNOB 3 THROW for a moment */
    if (t == 474880u)
        perf_k[2] = 0;
    for (i = 0; i < sizeof D / sizeof D[0]; i++)
        if (D[i].t == t)
            perf_press((uint32_t)abs(D[i].e) - 1u, D[i].e > 0);
}
static void demos(const char *dir)
{
    char path[512];
    FILE *f;
    uint32_t t;
    song_setup();
    song_render(NT - 4096u, demo_at);
    snprintf(path, sizeof path, "%s/perform_tour.wav", dir);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, NT - 4096u);
    for (t = 0; t < NT - 4096u; t++)
        wav_put(f, song_l[t], song_r[t]);
    fclose(f);
    printf("perform: demo %s (REPEAT 1/8, a 1/16 -> 1/32 roll, LPF, HPF, MUTE 1, KNOB 3 THROW)\n", path);
}

/* ------------------------------------------------- 9. punch-in MIDI --- */
static uint32_t zc(const int32_t *x, uint32_t a, uint32_t b)   /* zero crossings, low-passed (~350 Hz, 2 poles) */
{
    uint32_t i, c = 0;
    int32_t y1 = 0, y2 = 0, p = 0;
    for (i = a; i < b; i++) {
        y1 += (x[i] - y1) >> 5;
        y2 += (y1 - y2) >> 5;
        if (i > a + 400u)
            c += p < 0 && y2 >= 0;
        p = y2;
    }
    return c;
}
static void pfx_render(uint32_t frames, uint32_t press_at, uint32_t free_at, uint32_t e)
{
    uint32_t t, i;
    int32_t o[2 * CTL];
    for (t = 0; t < frames; t += CTL) {
        if (t == press_at)
            perf_press(e, 1);
        if (t == free_at)
            perf_press(e, 0);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            song_l[t + i] = o[2 * i];
    }
}
static int test_pfx(void)
{
    int bad = 0;
    uint32_t c0, c1, c2, i, hits = 0;
    track_t *t1 = &trk[0], *td = &trk[3];
    int32_t o[2 * CTL];
    /* OCT+ on a held note; then with DRM: the synth untouched */
    song_setup();
    for (i = 0; i < NTRK; i++) trk[i].p[P_DIST] = trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;
    host_preset(t1, 0, 1);
    trk_note_on(t1, 45, 100);
    pfx_render(4u * 44032u, 88064u, 132096u, PF_OCTU);   /* (its first second: the sound settles) */
    c0 = zc(song_l, 66048u, 88064u);
    c1 = zc(song_l, 110080u, 132096u);
    c2 = zc(song_l, 154112u, 176128u);
    bad += check("OCT+: a held note an octave up while held, back after", c1 > c0 * 1.7 && c1 < c0 * 2.3 &&
                 c2 > c0 * 0.85 && c2 < c0 * 1.15 && !t1->pfx_pit);
    pfx_tgt = 2;
    pfx_render(2u * 44032u, 22016u, 88064u, PF_OCTU);
    c1 = zc(song_l, 44032u, 66048u);
    c0 = c2;
    bad += check("  A#4 DRM: OCT+ leaves the synth alone", c1 > c0 * 0.85 && c1 < c0 * 1.15 && !t1->pfx_pit);
    pfx_tgt = 0;
    trk_note_off(t1, 45);
    /* DEC- on a kick: the tail shorter; DECY as it was outside the block */
    {
        double r0 = 0, r1 = 0;
        int16_t d0 = td->p[P_E3];
        uint32_t m;
        for (m = 0; m < 2u; m++) {
            for (i = 0; i < 44100u; i += CTL) mix_block(o, CTL);
            if (m) perf_press(PF_DSHT, 1);
            mix_block(o, CTL);                          /* (the effect in) */
            trk_note_on(td, 36, 100);
            pfx_render(11008u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0);
            for (i = 2205u; i < 8820u; i++) *(m ? &r1 : &r0) += (double)song_l[i] * song_l[i];
            if (m) perf_press(PF_DSHT, 0);
        }
        mix_block(o, CTL);
        bad += check("DEC-: a kick's tail shorter; DECY as stored outside the ISR", r1 < r0 * 0.5 && td->p[P_E3] == d0);
    }
    /* STUTTER 1/16 on the drums: the last kick 4 times a beat (120 BPM: 22050 samples) */
    song_setup();
    trk_note_on(td, 36, 100);
    mix_block(o, CTL);
    perf_press(PF_S8, 1);
    for (i = 0; i < 22016u; i += CTL) {
        mix_block(o, CTL);
        hits += drum_kit[3][DV_KICK].age == 1u;
    }
    perf_press(PF_S8, 0);
    mix_block(o, CTL);
    bad += check("STUTTER 1/8 (JIANT 0.4, was 1/16): the last kick twice in a beat; let go: none more", hits == 2u && !(td->pfx & PFX_REP));
    /* (JIANT 0.4) ATK+ on the drums: a hit's first ms softer (faded in); DEC- on a synth: its sustain 0 (a pluck), put
     * back after; ARP on the drums: a fill, each press another */
    {
        double e0 = 0, e1 = 0;
        uint32_t m;
        for (m = 0; m < 2u; m++) {
            song_setup();
            for (i = 0; i < 22016u; i += CTL) mix_block(o, CTL);
            if (m) perf_press(PF_ATK, 1);
            mix_block(o, CTL);
            mix_block(o, CTL);
            trk_note_on(td, 38, 110);
            pfx_render(4416u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0);
            for (i = 0; i < 441u; i++) *(m ? &e1 : &e0) += (double)song_l[i] * song_l[i];
            if (m) perf_press(PF_ATK, 0);
        }
        mix_block(o, CTL);
        bad += check("ATK+ on the drums: a snare's first 10 ms softer (faded in)", e1 < e0 * 0.5 && e0 > 0);
    }
    {
        int16_t s0;
        song_setup();
        s0 = t1->p[P_SUS] = 100;
        perf_press(PF_DSHT, 1);
        for (i = 0; i < 4u * CTL; i += CTL) mix_block(o, CTL);
        {
            int16_t during;
            uint32_t v;
            int32_t env = 0;
            trk_note_on(t1, 60, 100);
            for (i = 0; i < 22016u; i += CTL) mix_block(o, CTL);
            for (v = 0; v < NVOICE; v++)
                if (t1->v[v].active && t1->v[v].env > env) env = t1->v[v].env;
            during = t1->p[P_SUS];
            perf_press(PF_DSHT, 0);
            mix_block(o, CTL);
            bad += check("DEC- on a synth: its sustain 0 (a held note decays away), back after", env < (1 << 24) / 20 &&
                         during == s0 && t1->p[P_SUS] == s0);
            trk_note_off(t1, 60);
        }
    }
    {
        uint8_t f[2][16];
        uint32_t m, n, same = 0, notes[2] = {0, 0};
        for (m = 0; m < 2u; m++) {
            song_setup();
            perf_press(PF_ARP, 1);
            for (n = 0; n < 16u; n++) {
                f[m][n] = 0;
                for (i = 0; i < 5504u; i += CTL) {
                    mix_block(o, CTL);
                    if (td->pfx_snn && !f[m][n]) { f[m][n] = td->pfx_sn[0]; notes[m]++; }
                }
            }
            perf_press(PF_ARP, 0);
            mix_block(o, CTL);
        }
        for (n = 0; n < 16u; n++) same += f[0][n] == f[1][n];
        bad += check("ARP on the drums: a fill without a note struck first, another at the next press", notes[0] >= 8u &&
                     notes[1] >= 8u && same < 12u);
    }
    /* 1/2 TEMPO: the sequencer at half speed while held; let go, where it would have been */
    {
        uint16_t i0, ih;
        uint32_t p0;
        song_setup();
        song_render(4u * 22016u, 0);
        i0 = trk[0].seq_idx; p0 = trk[0].seq_pos;
        song_setup();
        transport_req = 1;
        for (i = 0; i < 4u * 22016u; i += CTL) {
            if (i == 22016u) perf_press(PF_HALF, 1);
            if (i == 3u * 22016u) { ih = trk[0].seq_idx; perf_press(PF_HALF, 0); }
            mix_block(o, CTL);
        }
        transport_req = 2;
        mix_block(o, CTL);
        bad += check("1/2 TEMPO: half the steps while held; let go, the sequencer where it would have been",
                     (uint16_t)(ih - 4u) <= 5u && trk[0].seq_idx == i0 && trk[0].seq_pos == p0);
    }
    /* RANDOM: some notes moved (an octave, a fifth, a fourth), none without it */
    {
        uint32_t moved = 0, far = 0, m;
        song_setup();
        host_preset(t1, 0, 1);
        for (m = 0; m < 2u; m++) {
            if (m) perf_press(PF_RND, 1);
            mix_block(o, CTL);
            for (i = 0; i < 24u; i++) {
                uint32_t v;
                trk_note_on(t1, 57, 100);
                for (v = 0; v < NVOICE; v++)
                    if (t1->v[v].active && t1->v[v].note == 57 && t1->v[v].gate) {
                        int32_t d = t1->v[v].pitch16 - 57 * 16;
                        moved += m ? d != 0 : (uint32_t)(d != 0) * 100u;
                        far += d > 12 * 16 || d < -12 * 16;
                    }
                trk_note_off(t1, 57);
                mix_block(o, CTL);
            }
            if (m) perf_press(PF_RND, 0);
        }
        bad += check("RANDOM: some notes moved (octave, fifth, fourth), none past an octave; none without it",
                     moved > 3u && moved < 100u && !far);
    }
    /* ARP TRNS: a key (F4: +5 from C4) transposes the track's sequence, plays nothing itself */
    {
        uint32_t seen = 0, wrong = 0, v;
        song_setup();
        t1->p[P_AMODE] = AM_TRNS;
        input_on(t1, 65, 100);
        input_off(t1, 65);
        transport_req = 1;
        for (i = 0; i < 4u * 22016u; i += CTL) {
            mix_block(o, CTL);
            for (v = 0; v < NVOICE; v++)
                if (t1->v[v].active && t1->v[v].gate) {
                    seen |= t1->v[v].note == 50u;           /* (step 1: A2 45, +5) */
                    wrong |= t1->v[v].note == 45u || t1->v[v].note == 65u;
                }
        }
        transport_req = 2;
        mix_block(o, CTL);
        bad += check("ARP TRNS: a key transposes the sequence by its interval from C4 (kept when let go), no note itself",
                     t1->trn == 5 && seen && !wrong);
    }
    /* the punch-in lane: REC armed, OCT+ held over steps 4..7 of the 64: recorded there (its tracks: DRM); the next
     * pass plays it on the drums (DRUM's pitch offset at step 5, none at step 10), not on the synths; stopped,
     * FX + G5 clears it */
    {
        uint32_t s16, st, ok = 1, pit5 = 0, pit10 = 1, syn = 0;
        song_setup();
        memset(pfx_lane, 0, sizeof pfx_lane);
        s16 = beat_samples() / 4u;
        song.rec = 1;
        pfx_tgt = 2;
        transport_req = 1;
        for (i = 0; i < 64u * s16; i += CTL) {
            st = (i / s16) & 63u;
            if (st == 4u && !(perf_held & PF_BIT(PF_OCTU))) perf_press(PF_OCTU, 1);
            if (st == 8u && (perf_held & PF_BIT(PF_OCTU))) perf_press(PF_OCTU, 0);
            mix_block(o, CTL);
        }
        song.rec = 0;
        pfx_tgt = 0;
        for (st = 0; st < 64u; st++)
            ok &= pfx_lane_at(st) == (st >= 4u && st < 8u ? (uint32_t)(PF_OCTU - PF_OCTD + 1) : 0u);
        ok &= pfx_ltgt == 2u;
        for (i = 64u * s16; i < 128u * s16; i += CTL) {
            st = ((i - 64u * s16) / s16) & 63u;
            mix_block(o, CTL);
            if (st == 5u) pit5 = td->pfx_pit == 192, syn |= t1->pfx_pit != 0;
            if (st == 10u) pit10 = td->pfx_pit;
        }
        transport_req = 2;
        mix_block(o, CTL);
        bad += check("punch-in lane: REC + OCT+ over steps 4..7 recorded (its tracks DRM); played back on the drums only",
                     ok && pit5 && !pit10 && !syn);
        perf_press(PF_CLR, 1);
        mix_block(o, CTL);
        perf_press(PF_CLR, 0);
        for (ok = 1, st = 0; st < 64u; st++) ok &= !pfx_lane_at(st);
        bad += check("  stopped, FX + G5 clears the lane", ok);
    }
    return bad;
}

/* (QG-1) the black keys' effects: VIBRATO swings the pitch both ways and lets it back; DIVE falls two octaves over a
 * beat, slow then fast; GATE shuts each 1/16's second half (and fades, no click); SWEEP closes the LPF mid-bar */
static int test_qg(void)
{
    int bad = 0;
    uint32_t i;
    int32_t o[2 * CTL], lo = 0, hi = 0, d1 = 0, d4 = 0;
    track_t *t1 = &trk[0];
    song_setup();
    host_preset(t1, 0, 1);
    trk_note_on(t1, 57, 100);
    perf_press(PF_VIB, 1);
    for (i = 0; i < 44100u; i += CTL) {
        mix_block(o, CTL);
        lo = t1->pfx_pit < lo ? t1->pfx_pit : lo;
        hi = t1->pfx_pit > hi ? t1->pfx_pit : hi;
    }
    perf_press(PF_VIB, 0);
    mix_block(o, CTL);
    mix_block(o, CTL);
    bad += check("VIBRATO: the pitch swung both ways (+-1/2 semitone), back when let go",
                 lo <= -6 && lo >= -9 && hi >= 6 && hi <= 9 && !t1->pfx_pit);
    perf_press(PF_DIVE, 1);
    for (i = 0; i < 22050u + 4u * CTL; i += CTL) {
        mix_block(o, CTL);
        if (i < 5513u) d1 = t1->pfx_pit;
        d4 = t1->pfx_pit;
    }
    perf_press(PF_DIVE, 0);
    mix_block(o, CTL);
    mix_block(o, CTL);
    bad += check("DIVE: slow then fast, two octaves down over a beat, back at once when let go",
                 d1 < 0 && d1 > -40 && d4 <= -380 && !t1->pfx_pit);
    trk_note_off(t1, 57);
    {
        ev_t ev[] = {{0, PF_GATE + 1}};
        int32_t op, sh, ed = 0;
        test_signal(220, 12000, 220, 12000);
        run(120, 1, ev, 1, 8u * P16);
        op = peak(out_l, 4u * P16 + 200u, 4u * P16 + P16 / 2u - 200u);
        sh = peak(out_l, 4u * P16 + P16 / 2u + 400u, 5u * P16 - 50u);
        for (i = 4u * P16 + 1u; i < 6u * P16; i++)
            ed = abs(out_l[i] - out_l[i - 1]) > ed ? abs(out_l[i] - out_l[i - 1]) : ed;
        bad += check("GATE: each 1/16 open for its first half, shut for the second, no step (click)",
                     op > 10000 && sh < 400 && ed < 600);
    }
    {
        ev_t ev[] = {{0, PF_SWP + 1}};
        int32_t a, m;
        test_signal(6000, 12000, 6000, 12000);
        run(120, 1, ev, 1, 10u * P16);
        a = peak(out_l, 2000u, 3000u);
        m = peak(out_l, 7u * P16, 9u * P16);
        bad += check("SWEEP: the LPF open at the bar's start, closed half way (a 6 kHz tone down)", a > 8000 && m < a / 4);
    }
    {   /* the lane: REC + DIVE over steps 2..5, VIBRATO over 8..9: codes 12 and 11 there; the next pass plays them */
        uint32_t s16, st, ok = 1, dv = 0, vb = 0, none = 1;
        song_setup();
        memset(pfx_lane, 0, sizeof pfx_lane);
        s16 = beat_samples() / 4u;
        song.rec = 1;
        transport_req = 1;
        for (i = 0; i < 64u * s16; i += CTL) {
            st = (i / s16) & 63u;
            if (st == 2u && !(perf_held & PF_BIT(PF_DIVE))) perf_press(PF_DIVE, 1);
            if (st == 6u && (perf_held & PF_BIT(PF_DIVE))) perf_press(PF_DIVE, 0);
            if (st == 8u && !(perf_held & PF_BIT(PF_VIB))) perf_press(PF_VIB, 1);
            if (st == 10u && (perf_held & PF_BIT(PF_VIB))) perf_press(PF_VIB, 0);
            mix_block(o, CTL);
        }
        song.rec = 0;
        for (st = 0; st < 64u; st++)
            ok &= pfx_lane_at(st) == (st >= 2u && st < 6u ? 12u : st >= 8u && st < 10u ? 11u : 0u);
        for (i = 64u * s16; i < 128u * s16; i += CTL) {
            st = ((i - 64u * s16) / s16) & 63u;
            mix_block(o, CTL);
            if (st == 5u) dv |= t1->pfx_pit < -20;
            if (st == 9u) vb |= t1->pfx_pit != 0;
            if (st >= 12u && st < 60u) none &= !t1->pfx_pit;
        }
        transport_req = 2;
        mix_block(o, CTL);
        bad += check("punch-in lane: DIVE and VIBRATO recorded (12, 11), played back where they were", ok && dv && vb && none);
        perf_press(PF_CLR, 1);
        mix_block(o, CTL);
        perf_press(PF_CLR, 0);
    }
    return bad;
}

int main(int argc, char **argv)
{
    int bad = 0;
    bad += test_timing();
    bad += test_stereo();
    bad += test_too_long();
    bad += test_slicer();
    bad += test_keys();
    bad += test_idle();
    bad += test_misc();
    bad += test_pfx();
    bad += test_qg();
    bad += test_cost();
    if (argc > 1)
        demos(argv[1]);
    printf("%s\n", bad ? "PERFORM TEST FAILED" : "perform test passed");
    return bad != 0;
}
