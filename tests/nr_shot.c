/* SPDX-License-Identifier: GPL-3.0-only */
/* NOTE RUSH 37 on the host: plays games through the simulated panel (tests/panel.h), checks the rules and writes
 * the screen as PPM files (build/nrshots/NN-name.ppm).
 *   cc -O1 -Ibuild/gen -o build/nr_shot tests/nr_shot.c && build/nr_shot */
#include "panel.h"
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void shot(const char *name)
{
    static int n;
    char p[256];
    FILE *f;
    int i;
    snprintf(p, sizeof p, "build/nrshots/%02d-%s.ppm", n++, name);
    f = fopen(p, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        fputc(((c >> 11) & 31) * 255 / 31, f);
        fputc(((c >> 5) & 63) * 255 / 63, f);
        fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}
static uint32_t voices(void) { uint32_t v, n = 0; for (v = 0; v < NVOICE; v++) n += vs[v].stage != 0; return n; }
static void press(int k) { panel_key((uint32_t)k, 1, 100); panel_ms(20); }
static void lift(int k) { panel_key((uint32_t)k, 0, 0); panel_ms(20); }
static void key(int k) { press(k); lift(k); }
static void btn(int b) { panel_button((uint32_t)b, 1); panel_ms(20); panel_button((uint32_t)b, 0); panel_ms(20); }
static int target_key(void) { return (int)note_midi(ui.cur) - NR_KEY0; }
static void wait_gs(int gs) { int i; for (i = 0; i < 600 && ui.gs != gs; i++) panel_ms(20); }
static void answer(void) { wait_gs(GS_ASK); key(target_key()); }

/* the same key, the same velocity: each strike's first 0.3 s (left, right: RMS) */
#ifndef GAP_MS
#define GAP_MS 2500                                /* (the reverb's tail gone, mostly) */
#endif
static void strikes(double *spread_l, double *spread_bal)
{
    static float l[13230], r[13230];
    double lo = 1e9, hi = 0, blo = 1e9, bhi = -1e9;
    int n, i;
    for (n = 0; n < 6; n++) {
        double el = 0, er = 0;
        panel_ms(GAP_MS + 137 * n);                /* (irregular gaps: the orbit runs on between them) */
        panel_key(24, 1, 100);
        panel_render(l, r, 13230);
        panel_key(24, 0, 0);
        for (i = 0; i < 13230; i++)
            el += l[i] * l[i], er += r[i] * r[i];
        el = el / 13230, er = er / 13230;
        lo = el + er < lo ? el + er : lo, hi = el + er > hi ? el + er : hi;
        blo = el / (el + er) < blo ? el / (el + er) : blo, bhi = el / (el + er) > bhi ? el / (el + er) : bhi;
    }
    panel_ms(1500);
    *spread_l = hi / lo, *spread_bal = bhi - blo;
}

int main(void)
{
    int i;
    panel_init();
    panel_ms(300);
    shot("title");
    CHECK(ui.gs == GS_TITLE, "starts on the title");
    btn(B_RIGHT);
    CHECK(ui.mode == MODE_NOME, "> picks the next mode (%d)", ui.mode);
    btn(B_LEFT);
    CHECK(ui.mode == MODE_ALL, "< back");

    {                                              /* the same key: the same sound, every sound */
        double lv, bal;
        uint32_t k;
        for (k = 0; k < NSND; k++) {
            preset_load(SND[k]);
            strikes(&lv, &bal);
            CHECK(lv < 1.4 && bal < 0.08,   /* (FLAUTA, CORDAS: their breath, their bow: noise) */
                  "%s: the same key, the same level (x%.3f) and place (%.3f)", SND_NAME[k], lv, bal);
        }
        preset_load(SND[0]);
    }

    /* the VETOR (PARA) */
    {
        int16_t c0 = base_cut[0];
        uint32_t q = orb_q8;
        btn(B_PARA);
        CHECK(ui.gs == GS_VECTOR && vec.on && pt.g[G_SHAPE] == 0 && pt.g[G_RETRIG] == 0, "PARA: the VETOR, on, free running");
        panel_enc(0, 5); panel_ms(40);
        CHECK(pt.g[G_WARPY] == -54 && str_eq(ui.msg, "FREQ X 1.3"), "K1: FREQ X (%d, %s)", pt.g[G_WARPY], ui.msg);
        panel_enc(1, 13); panel_ms(40);
        CHECK(pt.g[G_MORPH] == -13, "K2: FREQ Y (%d)", pt.g[G_MORPH]);
        panel_enc(3, 5); panel_ms(40);
        CHECK(pt.c[0][CP_CUT] == clamp(c0 + 10, 0, 127) && pt.c[1][CP_CUT] == base_cut[1], "K4: corner 1 alone");
        panel_enc(7, 10); panel_ms(40);
        CHECK(vec.speed == 68 && orb_q8 > q, "K8: faster (%d, %u)", vec.speed, orb_q8);
        panel_wheel(1, 485); panel_ms(500);
        shot("vector");
        panel_wheel(1, 615); panel_ms(800);
        btn(B_PARA);
        CHECK(!vec.on && pt.g[G_RETRIG] == 1 && pt.c[0][CP_CUT] == c0 && orb_q8 == 256, "PARA: off, the sound as it comes");
        btn(B_PARA);
        CHECK(vec.on && pt.g[G_WARPY] == -54, "PARA: on again, as it was set");
        btn(B_PLAY);
        CHECK(ui.gs == GS_TITLE && vec.on, "PLAY: home, still on");
        panel_ms(60);
        shot("title-vector");
        panel_enc(0, 1); panel_ms(40);
        CHECK(pt.g[G_WARPY] == -52, "on, on the title too the knobs are the VETOR's");
        btn(B_PARA); btn(B_PARA); btn(B_PLAY);
        CHECK(!vec.on, "off again");
    }

    /* the PATCH screen: the sound's knobs, even with the VETOR on */
    btn(B_PARA);
    CHECK(vec.on, "VETOR on");
    btn(B_PLAY);
    btn(B_PATCH);
    CHECK(ui.gs == GS_SOUND, "PATCH: the sound");
    {
        int16_t aa = pt.g[G_AA], wy = pt.g[G_WARPY];
        panel_enc(2, 10); panel_ms(40);
        CHECK(pt.g[G_AA] == clamp(aa + 20, 0, 127) && pt.g[G_WARPY] == wy, "there K3 is ATAQUE, not the VETOR's");
        panel_enc(6, 10); panel_enc(7, 15); panel_ms(40);
        shot("sound");
        panel_enc(2, -10); panel_enc(6, -10); panel_enc(7, -15); panel_ms(40);
    }
    btn(B_PATCH);
    CHECK(ui.gs == GS_TITLE, "PATCH again: home");
    btn(B_PARA); btn(B_PARA); btn(B_PLAY);
    CHECK(!vec.on && ui.gs == GS_TITLE, "VETOR off again");

    /* the settings */
    btn(B_GLOBE);
    CHECK(ui.gs == GS_SETUP, "GLOBE: the settings");
    CHECK(ui.set[SET_TIME] == 1 && ui.set[SET_START] == 3 && ui.set[SET_SFP] == 3, "the web's defaults");
    btn(B_OCTDN);
    CHECK(ui.set[SET_TIME] == 0 && time_limit() == 0, "TEMPO down to SEM LIMITE");
    panel_ms(100);
    shot("setup");
    btn(B_GLOBE);
    CHECK(ui.gs == GS_TITLE && settings.rsv[0] == NR_SET_TAG && settings.rsv[1] == 0, "GLOBE again: saved");

    /* a game without a time limit */
    for (i = 0; i < 60; i++) panel_ms(20);
    press(12);
    panel_ms(100);
    CHECK(ui.gs == GS_TITLE && voices() > 0, "on the title a key just plays (%d, %u)", ui.gs, voices());
    lift(12);
    {                                              /* the sound's controls */
        int16_t cut = pt.c[0][CP_CUT], aa = pt.g[G_AA];
        panel_enc(0, 3); panel_ms(40);
        CHECK(pt.c[0][CP_CUT] == clamp(cut + 6, 0, 127), "K1: BRILHO");
        CHECK(str_eq(ui.msg, "BRILHO ") == 0 && ui.msg[0] == 'B', "K1 said in the header (%s)", ui.msg);
        panel_ms(60);
        shot("knob");
        panel_enc(2, -2); panel_ms(40);
        CHECK(pt.g[G_AA] == clamp(aa - 4, 0, 127), "K3: ATAQUE");
        panel_fader(0, 30); panel_ms(40);
        CHECK(pt.g[G_VOLUME] == 30, "F1: VOLUME (%d)", pt.g[G_VOLUME]);
        panel_fader(1, 90); panel_ms(40);
        CHECK(pt.g[G_RVMIX] == 90, "F2: REVERB");
        panel_fader(0, 100); panel_ms(40);
        panel_wheel(1, 485); panel_ms(400);
        CHECK(ui.mod > 20000, "the mod wheel up (%d)", ui.mod);
        { int32_t lo = 99999, hi = -99999; for (i = 0; i < 60; i++) { panel_ms(10); lo = orb_dx < lo ? orb_dx : lo; hi = orb_dx > hi ? orb_dx : hi; }
          CHECK(hi - lo > 8000, "its lightning moves the orbit's place (%d..%d)", lo, hi); }
        panel_wheel(1, 615); panel_ms(800);
        CHECK(ui.mod < 500 && orb_dx > -200 && orb_dx < 200, "mod wheel down: back on the path (%d, %d)", ui.mod, orb_dx);
    }
    btn(B_PLAY);
    CHECK(ui.gs == GS_INTRO, "PLAY starts");
    panel_ms(100);
    shot("start");
    wait_gs(GS_ASK);
    CHECK(ui.phase == PH_NOME && ui.limit == 0, "NOME, no limit");
    panel_ms(100);
    shot("ask-nome");
    panel_ms(10000);
    CHECK(ui.gs == GS_ASK, "SEM LIMITE: no game over after 10 s");
    key(target_key() + 12 <= 36 ? target_key() + 12 : target_key() - 12);
    CHECK(ui.gs == GS_OK && ui.combo == 3, "NOME: any octave (%d, %d)", ui.gs, ui.combo);
    CHECK(bolt.t == 0, "10 s to answer: no lightning");
    answer(); answer();
    wait_gs(GS_ASK);
    CHECK(ui.phase == PH_PAUTA && ui.combo == 9, "three right: PAUTA (%d, %d)", ui.phase, ui.combo);
    panel_ms(100);
    shot("ask-pauta");
    key(target_key());
    CHECK(ui.combo == 13 && bolt.t && bolt.n == 4, "PAUTA +4, quick: four bolts (%d %d %u)", bolt.n, ui.combo, bolt.t);
    panel_ms(100);
    shot("lightning");
    answer(); answer();
    wait_gs(GS_ASK);
    CHECK(ui.phase == PH_OUVIDO, "then OUVIDO (%d)", ui.phase);
    panel_ms(200);
    shot("ask-ouvido");
    CHECK(voices() > 0, "OUVIDO: the note sounds");
    answer(); answer(); answer();
    wait_gs(GS_INTRO);
    CHECK(ui.unlocked == 4u && new_note() == 10u, "all phases: a new note, F4 (%d, %d)", ui.unlocked, new_note());
    panel_ms(100);
    shot("intro");
    wait_gs(GS_ASK);
    CHECK(ui.phase == PH_NOME, "back to NOME");
    key(target_key() + 1);
    CHECK(ui.gs == GS_OVER && ui.why == WHY_KEY, "a wrong key: game over");
    panel_ms(300);
    shot("over-nome");
    wait_gs(GS_NAME);
    CHECK(ui.gs == GS_NAME, "a score for the ranking: the name");
    btn(B_OCTUP); btn(B_RIGHT); btn(B_OCTUP); btn(B_OCTUP); btn(B_RIGHT); btn(B_OCTDN);
    CHECK(ui.name[0] == 'B' && ui.name[1] == 'C' && ui.name[2] == 'Z', "the letters (%.3s)", ui.name);
    panel_ms(60);
    shot("name");
    btn(B_PLAY);
    CHECK(ui.gs == GS_BOARD && ui.rank == 0 && lb.e[0].score == 36, "saved, first (%d)", lb.e[0].score);
    panel_ms(60);
    shot("board");

    /* with the time: PAUTA alone, 6 s */
    btn(B_PLAY);
    btn(B_GLOBE); btn(B_OCTUP); btn(B_GLOBE);
    btn(B_RIGHT); btn(B_RIGHT);
    CHECK(ui.mode == MODE_PAUTA, "mode PAUTA");
    btn(B_PLAY);
    wait_gs(GS_ASK);
    CHECK(ui.phase == PH_PAUTA && ui.limit == 6000u, "PAUTA, 6 s");
    panel_ms(2500);
    shot("clock");
    key(target_key() + 2 <= 36 ? target_key() + 2 : target_key() - 2);
    CHECK(ui.gs == GS_OVER, "a wrong key");
    panel_ms(100);
    shot("over-pauta");
    wait_gs(GS_BOARD);
    CHECK(ui.gs == GS_BOARD, "no score: straight to the ranking");
    key(5);
    CHECK(ui.gs == GS_BOARD, "the ranking: a key just plays");
    btn(B_PLAY);
    CHECK(ui.gs == GS_TITLE, "PLAY: home");
    btn(B_PLAY);
    CHECK(ui.gs == GS_INTRO, "PLAY: again");
    wait_gs(GS_ASK);
    panel_ms(6200);
    CHECK(ui.gs == GS_OVER && ui.why == WHY_TIME, "6 s: TEMPO");
    shot("over-time");

    CHECK(ORDER[7] == 14u && ORDER[15] == 0u && note_midi(0) == 48u && note_midi(21) == 84u,
          "the middle octave, the upper, the lower last");
    for (i = 0; i < 2; i++) {                      /* the staff's ends: C3 (four ledger lines), C6 */
        ui.gs = GS_ASK, ui.phase = PH_PAUTA, ui.cur = (uint8_t)(i ? 21 : 0), ui.ask_t = fm1_ms, ui.limit = 0, ui.dirty = 1;
        panel_ms(40);
        shot(i ? "staff-c6" : "staff-c3");
    }
    btn(B_PLAY);
    panel_ms(60);
    shot("title-ranked");
    panel_init();                                  /* a power cycle: the ranking and the settings read back */
    CHECK(lb.e[0].score == 36 && lb.e[0].n[0] == 'B' && ui.set[SET_TIME] == 1, "kept over a power cycle");
    CHECK(!vec.on && vec.fx == -52 && vec.speed == 68, "the VETOR kept too (%d %d %d)", vec.on, vec.fx, vec.speed);
    printf("%s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
