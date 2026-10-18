/* SPDX-License-Identifier: GPL-3.0-only */
/* NOTE RUSH 37 on the host: plays a game through the simulated panel (tests/panel.h) and writes the screen as PPM
 * files (build/shots/NN-name.ppm): the title, each phase, a right answer, game over. Also checks the game's rules.
 *   cc -O1 -Ibuild/gen -o build/nr_shot tests/nr_shot.c && build/nr_shot */
#include "panel.h"
#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void shot(const char *name)
{
    static int n;
    char p[256];
    FILE *f;
    int i;
    snprintf(p, sizeof p, "build/shots/%02d-%s.ppm", n++, name);
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
static uint32_t sim_voices_n(void) { uint32_t v, n = 0; for (v = 0; v < NVOICE; v++) n += vs[v].stage != 0; return n; }
static void key(int k) { panel_key((uint32_t)k, 1, 100); panel_ms(40); panel_key((uint32_t)k, 0, 0); panel_ms(40); }
static int target_key(void) { return (int)note_midi(ui.cur) - NR_KEY0; }
static void wait_ask(void) { int i; for (i = 0; i < 400 && ui.gs != GS_ASK; i++) panel_ms(20); }

int main(void)
{
    int i;
    panel_init();
    panel_ms(300);
    shot("title");
    CHECK(ui.gs == GS_TITLE, "starts on the title");
    panel_button(B_RIGHT, 1); panel_ms(30); panel_button(B_RIGHT, 0); panel_ms(30);
    CHECK(ui.mode == MODE_NOME, "> picks the next mode (%d)", ui.mode);
    panel_button(B_LEFT, 1); panel_ms(30); panel_button(B_LEFT, 0); panel_ms(30);
    CHECK(ui.mode == MODE_ALL, "< back");
    key(12);                                       /* any key: start */
    CHECK(ui.gs == GS_INTRO && ui.unlocked == NR_START, "a key starts: the intro (%d)", ui.gs);
    panel_ms(200);
    shot("intro");
    for (i = 0; i < 400 && ui.gs != GS_PHASE; i++) panel_ms(20);
    CHECK(ui.gs == GS_PHASE && ui.phase == PH_NOME, "the first phase's banner");
    panel_ms(100);
    shot("phase-nome");
    wait_ask();
    panel_ms(100);
    shot("ask-nome");
    /* NOME: any octave */
    key(target_key() + 12 <= 36 ? target_key() + 12 : target_key() - 12);
    CHECK(ui.gs == GS_OK && ui.combo == 3, "NOME: the right name an octave off counts (%d, %d)", ui.gs, ui.combo);
    panel_ms(60);
    shot("ok-nome");
    wait_ask(); key(target_key());
    wait_ask(); key(target_key());
    for (i = 0; i < 100 && ui.gs != GS_PHASE; i++) panel_ms(20);
    CHECK(ui.phase == PH_PAUTA && ui.combo == 9, "three right: PAUTA (%d, %d)", ui.phase, ui.combo);
    wait_ask();
    panel_ms(1500);
    shot("ask-pauta");
    key(target_key());
    CHECK(ui.combo == 13, "PAUTA +4 (%d)", ui.combo);
    wait_ask(); key(target_key());
    wait_ask(); key(target_key());
    wait_ask();
    CHECK(ui.phase == PH_OUVIDO, "then OUVIDO (%d)", ui.phase);
    panel_ms(200);
    shot("ask-ouvido");
    CHECK(sim_voices_n() > 0, "OUVIDO: the note sounds");
    key(target_key()); panel_ms(40);
    shot("ok-ouvido");
    wait_ask(); key(target_key());
    wait_ask(); key(target_key());
    for (i = 0; i < 100 && ui.gs != GS_INTRO; i++) panel_ms(20);
    CHECK(ui.gs == GS_INTRO && ui.unlocked == NR_START + 1u, "all three phases: a new note (%d)", ui.unlocked);
    panel_ms(100);
    shot("new-note");
    wait_ask();
    CHECK(ui.phase == PH_NOME, "back to NOME");
    /* PAUTA needs the very key: play every note of the staff in turn on screen */
    {
        int t = target_key(), wrong = (t % 12 == 1) ? t + 1 : t + 1;   /* (a semitone off: wrong) */
        key(wrong);
        CHECK(ui.gs == GS_OVER && ui.why == WHY_KEY, "a wrong key: game over");
        panel_ms(700);
        shot("over-key");
        CHECK(ui.best == 36 && ui.newbest, "the record (%d)", ui.best);
        CHECK(settings.rsv[0] == 36, "kept in the settings");
    }
    key(12);
    CHECK(ui.gs == GS_OVER, "a key too soon after game over: nothing");
    panel_ms(1000);
    key(12);
    CHECK(ui.gs == GS_INTRO && ui.combo == 0, "a key after: again");
    wait_ask();
    panel_ms(NR_LIMIT_MS + 200);
    CHECK(ui.gs == GS_OVER && ui.why == WHY_TIME, "6 s: TEMPO");
    shot("over-time");
    /* the staff, every note */
    for (i = 0; i < (int)NR_NOTES; i++) {
        char nm[16];
        ui.gs = GS_ASK, ui.phase = PH_PAUTA, ui.cur = (uint8_t)i, ui.ask_t = fm1_ms, ui.limit = 99999, ui.dirty = 1;
        panel_ms(40);
        snprintf(nm, sizeof nm, "staff-%02d", i);
        if (i == 0 || i == 6 || i == 14) shot(nm);
    }
    panel_button(B_PATCH, 1); panel_ms(30); panel_button(B_PATCH, 0); panel_ms(30);
    CHECK(ui.gs == GS_TITLE, "PATCH: the title");
    panel_init();                                  /* a power cycle: the record read back */
    CHECK(ui.best == 36, "the record after a power cycle (%d)", ui.best);
    printf("%s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
