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

    /* the VETOR (PARA): in, out; the brightness alone */
    {
        patch_t was;
        uint32_t c, k, other = 0;
        int16_t aa;
        panel_enc(2, 5); panel_ms(40);             /* (a PATCH setting first: kept through the VETOR) */
        aa = pt.g[G_AA];
        was = pt;
        btn(B_PARA);
        CHECK(ui.gs == GS_VECTOR, "PARA: the VETOR");
        panel_ms(500);
        CHECK(memcmp(&pt, &was, sizeof pt) == 0, "its corners at 0: the sound untouched");
        panel_enc(0, 5); panel_ms(40);
        CHECK(vec.fx == -54 && str_eq(ui.msg, "FREQ X 1.3"), "K1: FREQ X (%d, %s)", vec.fx, ui.msg);
        panel_enc(1, 13); panel_ms(40);
        CHECK(vec.fy == -13, "K2: FREQ Y (%d)", vec.fy);
        panel_enc(3, 15); panel_ms(40);
        CHECK(vec.cut[0] == 30 && str_eq(ui.msg, "BRILHO CANTO 1 30"), "K4: BRILHO CANTO 1 (%s)", ui.msg);
        panel_enc(6, -15); panel_ms(40);
        panel_enc(7, 10); panel_ms(40);
        CHECK(vec.speed == 68, "K8: VELOCIDADE (%d)", vec.speed);
        {   /* the brightness moves with the point; nothing else in the sound */
            int32_t lo = 999, hi = -999;
            for (i = 0; i < 80; i++) {
                panel_ms(25);
                lo = pt.c[0][CP_CUT] < lo ? pt.c[0][CP_CUT] : lo, hi = pt.c[0][CP_CUT] > hi ? pt.c[0][CP_CUT] : hi;
                for (c = 0; c < NC; c++)
                    for (k = 0; k < CP_N; k++)
                        other += k != CP_CUT && pt.c[c][k] != was.c[c][k];
                other += memcmp(pt.g, was.g, sizeof pt.g) != 0;
            }
            CHECK(hi - lo > 10, "the brightness follows the point (%d .. %d)", lo, hi);
            CHECK(other == 0, "and nothing else changes (%u)", other);
        }
        panel_wheel(1, 485); panel_ms(500);
        CHECK(vb.x || vb.y, "the mod wheel: its bolt throws the point");
        shot("vector");
        panel_wheel(1, 615); panel_ms(800);
        btn(B_PARA);
        CHECK(ui.gs == GS_TITLE && pt.g[G_AA] == aa, "PARA again: out; the PATCH setting kept");
        panel_enc(0, 1); panel_ms(40);
        CHECK(vec.fx == -54 && pt.c[0][CP_CUT] != 0, "out of it the knobs are the sound's again");
        panel_enc(0, -1); panel_ms(40);
        btn(B_PARA); btn(B_PLAY);
        CHECK(ui.gs == GS_TITLE, "PLAY: out too");
    }

    /* the PATCH screen */
    btn(B_PATCH);
    CHECK(ui.gs == GS_SOUND, "PATCH: the sound");
    {
        int16_t aa = pt.g[G_AA];
        panel_enc(2, 10); panel_ms(40);
        CHECK(pt.g[G_AA] == clamp(aa + 20, 0, 127), "there K3 is ATAQUE");
        panel_enc(6, 10); panel_enc(7, 15); panel_ms(40);
        shot("sound");
        panel_enc(2, -10); panel_enc(6, -10); panel_enc(7, -15); panel_ms(40);
    }
    btn(B_PATCH);
    CHECK(ui.gs == GS_TITLE, "PATCH again: home");

    /* the rack: what was set goes from sound to sound; what was not, each sound's own */
    {
        int16_t drv;
        btn(B_PATCH);
        panel_enc(2, 20); panel_ms(40);            /* ATAQUE to a value */
        panel_enc(0, -10); panel_ms(40);           /* BRILHO 20 darker than the sound's */
        panel_fader(3, 77); panel_ms(40);          /* CHORUS */
        btn(B_PLAY);
        btn(B_GLOBE); btn(B_RIGHT); btn(B_OCTUP); btn(B_GLOBE);   /* SOM: MARIMBA */
        CHECK(ui.set[SET_SOUND] == 1, "MARIMBA");
        drv = factory.g[G_DRIVE];
        CHECK(pt.g[G_AA] == rack.k[2] && rack.k[2] != RACK_UNSET, "ATAQUE went along (%d)", pt.g[G_AA]);
        CHECK(base_cut[0] == clamp(factory.c[0][CP_CUT] - 20, 0, 127), "BRILHO: 20 darker than MARIMBA's own (%d, %d)",
              base_cut[0], factory.c[0][CP_CUT]);
        CHECK(pt.g[G_DRIVE] == drv && pt.g[G_CHMIX] == 77, "DRIVE untouched: MARIMBA's own; CHORUS went along");
        panel_init();                              /* a power cycle */
        CHECK(pt.g[G_CHMIX] == 77 && rack.k[2] != RACK_UNSET, "the rack kept over a power cycle");
        btn(B_PATCH); btn(B_GLOBE);
        CHECK(pt.g[G_AA] == factory.g[G_AA] && pt.g[G_CHMIX] == factory.g[G_CHMIX], "PATCH, GLOBE: as it comes");
        btn(B_PLAY);
        btn(B_GLOBE); btn(B_RIGHT); btn(B_OCTDN); btn(B_GLOBE);   /* back to E.PIANO */
        CHECK(ui.set[SET_SOUND] == 0 && ui.gs == GS_TITLE, "E.PIANO again");
    }

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
        int16_t cut = base_cut[0], aa = pt.g[G_AA];
        panel_enc(0, 3); panel_ms(40);
        CHECK(base_cut[0] == clamp(cut + 6, 0, 127), "K1: BRILHO (the sound's own; the VETOR adds to it)");
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
        { int32_t lo = 99999, hi = -99999; for (i = 0; i < 60; i++) { panel_ms(10); lo = vb.x < lo ? vb.x : lo; hi = vb.x > hi ? vb.x : hi; }
          CHECK(hi - lo > 8000, "its lightning throws the VETOR's point (%d..%d)", lo, hi); }
        panel_wheel(1, 615); panel_ms(800);
        CHECK(ui.mod < 500 && vb.x > -200 && vb.x < 200, "mod wheel down: back on the figure (%d, %d)", ui.mod, vb.x);
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
    CHECK(vec.fx == -54 && vec.cut[0] == 30 && vec.speed == 68, "the VETOR kept too (%d %d %d)", vec.fx, vec.cut[0], vec.speed);
    printf("%s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
