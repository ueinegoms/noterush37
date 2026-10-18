/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Wayne (NOTE RUSH 37), on Vector37 by M. Geisler (makkuro) */
/* NOTE RUSH 37: the screen (in Vector37's draw.c's place). 240 x 240 in five strips (gfx.c's canvas, one DMA
 * transfer each): the header (logo, combo), the clock, the body (two strips), the footer. Everything is drawn in
 * screen coordinates; each strip draws the scene and keeps what falls on it. Only the strips that changed go out
 * (game.c: ui.redraw); the clock goes out every frame while it runs. */
#define C_BG RGB(6, 4, 18)
#define UI_LOOP_MS 16u                             /* the main loop's pace (main.c) */
#define C_BAND RGB(16, 8, 38)
#define C_CY RGB(0, 240, 255)
#define C_MG RGB(255, 40, 200)
#define C_YE RGB(255, 230, 40)
#define C_GR RGB(40, 255, 120)
#define C_RD RGB(255, 50, 70)
#define C_TX RGB(225, 228, 255)
#define C_DM RGB(110, 96, 160)
#define C_LN RGB(48, 34, 92)
#define C_STAFF RGB(170, 160, 220)

static const uint16_t STRIP_Y[6] = {0, 34, 46, 122, 198, 240};
static int32_t strip_y0, strip_y1;

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void text_r(int32_t x1, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(x1 - text_w(f, s), y, f, s, c);
}
static void text_glitch(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, uint16_t c2)
{                                                  /* the cyberfunk look: a shifted ghost in the other neon */
    int32_t x = cx - text_w(f, s) / 2;
    cv_text(x + 3, y + 2, f, s, c2);
    cv_text(x, y, f, s, c);
}
static void num(char *b, uint32_t v)               /* (decimal) */
{
    char t[12];
    uint32_t n = 0, i;
    do
        t[n++] = (char)('0' + v % 10u), v /= 10u;
    while (v && n < sizeof t);
    for (i = 0; i < n; i++)
        b[i] = t[n - 1u - i];
    b[n] = 0;
}
static void ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t x, y;
    if (cy + ry < strip_y0 || cy - ry >= strip_y1)
        return;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
                cv_pset(cx + x, cy + y, c);
}
static uint16_t dim(uint16_t c, uint32_t k)       /* colour c at k / 256 */
{
    uint32_t r = (c >> 11) * k >> 8, g = ((c >> 5) & 63u) * k >> 8, b = (c & 31u) * k >> 8;
    return (uint16_t)(r << 11 | g << 5 | b);
}

/* ------------------------------------------------------------- the staff --- */
/* treble clef, step d: 0 = DÓ4 .. 14 = DÓ6; the lines MI4 SOL4 SI4 RÉ5 FÁ5 (steps 2..10), 10 px apart */
#define ST_X0 14
#define ST_X1 226
#define ST_MID 121                                 /* the SI4 line */
#define ST_NX 146                                  /* the note's place */
static int32_t step_y(int32_t d) { return ST_MID - (d - 6) * 5; }

/* the clef: a Catmull-Rom curve through these points (pixels from the SOL4 line, at the clef's x) */
static const int8_t CLEF[][2] = {{3, -3}, {0, -5}, {-5, -3}, {-7, 2}, {-4, 8}, {3, 10}, {9, 6}, {10, -1}, {6, -7},
                                 {0, -10}, {-6, -15}, {-9, -22}, {-6, -31}, {0, -39}, {4, -43}, {5, -37}, {1, -28},
                                 {-1, -15}, {1, 0}, {3, 15}, {2, 23}, {-2, 26}, {-6, 23}};
#define NCLEF (sizeof CLEF / sizeof CLEF[0])
static void brush(int32_t x, int32_t y, uint16_t c)   /* a 3 px round dot */
{
    cv_pset(x, y, c);
    cv_pset(x - 1, y, c);
    cv_pset(x + 1, y, c);
    cv_pset(x, y - 1, c);
    cv_pset(x, y + 1, c);
}
static void clef(int32_t x0, int32_t y0, uint16_t c)
{
    uint32_t i, t;
    if (y0 - 45 >= strip_y1 || y0 + 30 < strip_y0)
        return;
    for (i = 0; i + 1u < NCLEF; i++) {
        const int8_t *p0 = CLEF[i ? i - 1u : 0u], *p1 = CLEF[i], *p2 = CLEF[i + 1u], *p3 = CLEF[i + 2u < NCLEF ? i + 2u : i + 1u];
        for (t = 0; t < 16u; t++) {                /* t / 16 along the piece; x 4096 = 16^3, / 2 */
            int32_t a = (int32_t)t, a2 = a * a, a3 = a2 * a, k;
            int32_t w0 = -a3 + 32 * a2 - 256 * a, w1 = 3 * a3 - 80 * a2 + 8192, w2 = -3 * a3 + 64 * a2 + 256 * a, w3 = a3 - 16 * a2;
            int32_t x = 0, y = 0;
            for (k = 0; k < 2; k++) {
                int32_t v = (w0 * p0[k] + w1 * p1[k] + w2 * p2[k] + w3 * p3[k]) / 8192;
                if (k == 0)
                    x = v;
                else
                    y = v;
            }
            brush(x0 + x, y0 + y, c);
        }
    }
    ellipse(x0 - 4, y0 + 21, 3, 3, c);             /* the hook's ball */
}
static void staff(int32_t d, uint16_t col)        /* d < 0: the staff only */
{
    int32_t k, y;
    for (k = 2; k <= 10; k += 2)
        cv_rect(ST_X0, step_y(k), ST_X1 - ST_X0, 1, C_STAFF);
    clef(ST_X0 + 20, step_y(4), C_STAFF);
    if (d < 0)
        return;
    for (k = 0; k >= d; k -= 2)                    /* ledger lines: DÓ4 below, LÁ5 / DÓ6 above */
        cv_rect(ST_NX - 12, step_y(k), 25, 1, C_STAFF);
    for (k = 12; k <= d; k += 2)
        cv_rect(ST_NX - 12, step_y(k), 25, 1, C_STAFF);
    y = step_y(d);
    ellipse(ST_NX, y, 11, 8, dim(col, 60));        /* (its glow) */
    ellipse(ST_NX, y, 7, 5, col);
    if (d < 6)                                     /* the stem: up on the right below the middle line, else down */
        cv_rect(ST_NX + 6, y - 34, 2, 34, col);
    else
        cv_rect(ST_NX - 7, y, 2, 35, col);
}

/* ------------------------------------------------------------- the ear --- */
static void speaker(int32_t cx, int32_t cy, uint16_t col, uint16_t wave)
{
    int32_t x, y, r;
    cv_rect(cx - 30, cy - 9, 12, 18, col);
    for (x = 0; x < 16; x++)                       /* the cone */
        cv_rect(cx - 18 + x, cy - 9 - x * 9 / 10, 1, 18 + 2 * (x * 9 / 10), col);
    for (r = 12; r <= 36; r += 12)                 /* the waves: arcs of +-45 degrees */
        for (y = -r - 1; y <= r + 1; y++)
            for (x = 0; x <= r + 1; x++) {
                int32_t d2 = x * x + y * y, ay = y < 0 ? -y : y;
                if (ay <= x && d2 >= (r - 1) * (r - 1) && d2 <= (r + 1) * (r + 1))
                    cv_pset(cx + 2 + x, cy + y, wave);
            }
}

/* ------------------------------------------------------------- the scene --- */
static void note_label(char *b, uint32_t midi, int octave)   /* "SOL", "SOL4", "FÁ#" as the web names */
{
    str_cpy(b, CHROMA[midi % 12u], 8);
    if (octave) {
        uint32_t n = str_len(b);
        b[n] = (char)('0' + midi / 12u - 1u);
        b[n + 1u] = 0;
    }
}
static void header(void)
{
    char b[12];
    cv_rect(0, 0, 240, 33, C_BAND);
    cv_rect(0, 33, 240, 1, C_MG);
    cv_text(8, 9, &FONT_S, "NOTE RUSH", C_CY);
    b[0] = 'x';
    num(b + 1, ui.combo);
    text_r(232, 1, &FONT_L, b, ui.gs == GS_OK ? C_TX : C_YE);
}
static void clock_bar(void)
{
    int32_t w = 224;
    uint16_t c = C_LN;
    cv_rect(8, 38, 224, 5, C_LN);
    if (ui.gs == GS_ASK) {
        uint32_t left = ui.limit - (fm1_ms - ui.ask_t > ui.limit ? ui.limit : fm1_ms - ui.ask_t);
        uint32_t f = left * 256u / ui.limit;       /* 256 full .. 0 */
        w = (int32_t)(224u * f >> 8);
        c = f > 128u ? C_GR : f > 64u ? C_YE : C_RD;
    } else if (ui.gs == GS_OVER)
        w = 0;
    else
        c = ui.gs == GS_OK ? C_GR : C_DM;
    cv_rect(8, 38, w, 5, c);
}
static void body_ask(void)
{
    char b[12];
    int ok = ui.gs == GS_OK;
    uint16_t c = ok ? C_GR : C_CY;
    uint32_t m = note_midi(ui.cur);
    if (ui.phase == PH_NOME) {
        text_glitch(120, 82, &FONT_XXL, SOLFA[ui.cur % 7u], c, dim(C_MG, 150));
        text_c(120, 166, &FONT_S, "QUALQUER OITAVA", C_DM);
    } else if (ui.phase == PH_PAUTA)
        staff(ui.cur, ok ? C_GR : C_TX);
    else if (ok) {
        text_glitch(120, 88, &FONT_XL, SOLFA[ui.cur % 7u], C_GR, dim(C_MG, 150));
    } else {
        speaker(112, 108, C_CY, ui.tone_off ? C_MG : C_DM);
        text_c(120, 150, &FONT_L, "OU\xC7" "A", C_CY);
        text_c(120, 182, &FONT_S, "PLAY / PAD: DE NOVO", C_DM);
    }
    if (ok) {
        b[0] = '+';
        num(b + 1, ui.gain);
        text_r(232, 52, &FONT_L, b, C_GR);
        (void)m;
    }
}
static void body_over(void)
{
    char b[12];
    uint32_t m = note_midi(ui.cur);
    int oct = ui.phase == PH_PAUTA;
    text_glitch(120, 50, &FONT_L, ui.why == WHY_TIME ? "TEMPO!" : "ERROU!", C_RD, dim(C_MG, 120));
    text_c(64, 94, &FONT_S, "ERA", C_DM);
    note_label(b, m, oct);
    text_c(64, 112, &FONT_L, b, C_CY);
    text_c(176, 94, &FONT_S, ui.why == WHY_TIME ? "" : "TOCOU", C_DM);
    if (ui.why == WHY_KEY)
        note_label(b, ui.played, oct);
    else
        str_cpy(b, "...", sizeof b);
    text_c(176, 112, &FONT_L, b, ui.why == WHY_TIME ? C_DM : C_RD);
    text_c(120, 150, &FONT_S, "COMBO", C_DM);
    num(b, ui.combo);
    text_c(120, 164, &FONT_L, b, C_YE);
}
static void body(void)
{
    switch (ui.gs) {
    case GS_INTRO: {
        uint32_t d = new_note();
        char b[12];
        text_c(120, 50, &FONT_S, ui.unlocked > NR_START ? "NOVA NOTA!" : "CONHE\xC7" "A AS NOTAS", C_MG);
        staff((int32_t)d, C_YE);
        note_label(b, note_midi(d), 1);
        text_c(120, 162, &FONT_L, b, C_YE);
        break;
    }
    case GS_PHASE: {
        static const char *const HOW[PH_N] = {"LEIA O NOME, TOQUE A NOTA", "LEIA A PAUTA, TOQUE A TECLA",
                                              "OU\xC7" "A E TOQUE A NOTA"};
        text_c(120, 70, &FONT_S, "FASE", C_DM);
        text_glitch(120, 92, &FONT_XL, PHASE_NAME[ui.phase], C_MG, dim(C_CY, 140));
        text_c(120, 160, &FONT_S, HOW[ui.phase], C_TX);
        break;
    }
    case GS_ASK:
    case GS_OK:
        body_ask();
        break;
    case GS_OVER:
        body_over();
        break;
    }
}
static void footer(void)
{
    char b[24];
    uint32_t i;
    cv_rect(0, 198, 240, 1, C_LN);
    if (ui.gs == GS_OVER) {
        if (ui.newbest)
            text_c(120, 203, &FONT_S, (fm1_ms / 500u) & 1u ? "NOVO RECORDE!" : "", C_MG);
        else {
            str_cpy(b, "RECORDE ", sizeof b);
            num(b + str_len(b), ui.best);
            text_c(120, 203, &FONT_S, b, C_DM);
        }
        text_c(120, 221, &FONT_S, "TECLA: DE NOVO  PATCH: MENU", C_DM);
    } else {
        cv_text(8, 203, &FONT_S, PHASE_NAME[ui.phase], C_MG);
        for (i = 0; i < NR_SFP; i++) {             /* the phase's progress */
            int32_t x = 232 - 6 - (int32_t)(NR_SFP - 1u - i) * 16;
            ellipse(x, 211, 5, 5, i < ui.streak ? C_MG : C_LN);
        }
        str_cpy(b, "NOTAS ", sizeof b);
        num(b + str_len(b), ui.unlocked);
        str_cat(b, "/15", sizeof b);
        cv_text(8, 221, &FONT_S, b, C_DM);
        str_cpy(b, "REC ", sizeof b);
        num(b + str_len(b), ui.best > ui.combo ? ui.best : ui.combo);
        text_r(232, 221, &FONT_S, b, C_DM);
    }
}
static void title(void)
{
    char b[24];
    text_glitch(120, 12, &FONT_XXL, "NOTE", C_MG, dim(C_CY, 170));
    text_glitch(120, 74, &FONT_XXL, "RUSH", C_CY, dim(C_MG, 170));
    text_c(120, 142, &FONT_S, "SMK-37 \xB7 CYBERFUNK", C_DM);
    str_cpy(b, "< ", sizeof b);
    str_cat(b, MODE_NAME[ui.mode], sizeof b);
    str_cat(b, " >", sizeof b);
    text_c(120, 164, &FONT_L, b, C_YE);
    str_cpy(b, "RECORDE ", sizeof b);
    num(b + str_len(b), ui.best);
    text_c(120, 203, &FONT_S, b, C_DM);
    text_c(120, 221, &FONT_S, (fm1_ms / 500u) & 1u ? "" : "TOQUE UMA TECLA", C_CY);
}
static void scene(void)
{
    if (ui.gs == GS_TITLE)
        title();
    else {
        header();
        clock_bar();
        body();
        footer();
    }
    if (ui.msg[0]) {                               /* (the board layer's line, over the footer) */
        cv_rect(0, 199, 240, 41, C_BAND);
        text_c(120, 212, &FONT_S, ui.msg, C_TX);
    }
}
static void strip(uint32_t i)
{
    strip_y0 = STRIP_Y[i], strip_y1 = STRIP_Y[i + 1u];
    cv_begin(240, (uint32_t)(strip_y1 - strip_y0), C_BG);
    cv_ox = 0;
    cv_oy = -strip_y0;
    scene();
    cv_oy = 0;
    cv_blit(0, (uint32_t)strip_y0);
}

static void draw_frame(void)
{
    static uint8_t shown_gs = 0xFF;
    uint32_t m = ui.redraw, i;
    if (ui.dirty || ui.gs != shown_gs)
        m = D_ALL;
    if (ui.gs == GS_ASK)
        m |= D_BAR;                                /* (the clock runs) */
    ui.dirty = 0;
    ui.redraw = 0;
    shown_gs = ui.gs;
    for (i = 0; i < 5u; i++)
        if ((m >> i) & 1u)
            strip(i);
}

static void splash(void)                           /* before the audio runs: the title */
{
    uint32_t i;
    for (i = 0; i < 5u; i++)
        strip(i);
    lcd_sync();
}
static void boot_anim(void) { ui.dirty = 1; }      /* (the title is up already) */
