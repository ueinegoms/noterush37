/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Wayne (NOTE RUSH 37), on Vector37 by M. Geisler (makkuro) */
/* NOTE RUSH 37: the screen (in Vector37's draw.c's place), in the web version's look: violet ground, the electric
 * blue card, yellow and white, flat. 240 x 240 in five strips (gfx.c's canvas, one DMA transfer each): the header
 * (NOTE RUSH, the combo), the clock, the card (two strips), its foot. Everything is drawn in screen coordinates;
 * each strip draws the scene and keeps what falls on it. Only the strips that changed go out (game.c: ui.redraw);
 * the clock every frame while it runs, everything while the lightning runs. */
#define C_BG RGB(27, 0, 147)                       /* #1B0093 */
#define UI_LOOP_MS 16u                             /* the main loop's pace (main.c) */
#define C_CARD RGB(28, 61, 247)                    /* #1C3DF7 */
#define C_BOX RGB(34, 18, 106)                     /* #22126A */
#define C_YEL RGB(255, 255, 94)                    /* #FFFF5E */
#define C_OK RGB(0, 255, 136)
#define C_ERR RGB(255, 34, 68)
#define C_MUT RGB(130, 148, 178)                   /* (the web's muted: yellow at 45 % over the card) */
#define C_CLEF RGB(187, 197, 140)
#define C_STAFF RGB(232, 234, 255)

static const uint16_t STRIP_Y[6] = {0, 34, 42, 122, 202, 240};
#define CARD_X 6
#define CARD_Y 42
#define CARD_W 228
#define CARD_H 194
static int32_t strip_y0, strip_y1;

static void text_b(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* bold, as the web */
{
    cv_text(x, y, f, s, c);
    cv_text(x + 1, y, f, s, c);
}
static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    text_b(cx - text_w(f, s) / 2, y, f, s, c);
}
static void text_r(int32_t x1, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    text_b(x1 - text_w(f, s) - 1, y, f, s, c);
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
static uint16_t mix(uint16_t a, uint16_t b, uint32_t k)   /* a at k / 256 over b */
{
    uint32_t r = ((a >> 11) * k + (b >> 11) * (256u - k)) >> 8;
    uint32_t g = (((a >> 5) & 63u) * k + ((b >> 5) & 63u) * (256u - k)) >> 8;
    uint32_t bl = ((a & 31u) * k + (b & 31u) * (256u - k)) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static void button(int32_t y, int32_t h, const char *s, uint16_t bg, uint16_t fg)   /* a web button, across */
{
    cv_rect(CARD_X + 12, y, CARD_W - 24, h, bg);
    text_c(120, y + (h - 16) / 2, &FONT_S, s, fg);
}

/* ------------------------------------------------------------- the staff --- */
/* treble clef, step d: 0 = C4 .. 14 = C6; the lines E4 G4 B4 D5 F5 (steps 2..10), 10 px apart; mid: the B4 line */
#define ST_X0 16
#define ST_X1 224
#define ST_NX 146
static int32_t step_y(int32_t mid, int32_t d) { return mid - (d - 6) * 5; }

/* the clef: a Catmull-Rom curve through these points (pixels from the G4 line, at the clef's x) */
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
        for (t = 0; t < 16u; t++) {                /* t / 16 along the piece: the weights x 8192 */
            int32_t a = (int32_t)t, a2 = a * a, a3 = a2 * a;
            int32_t w0 = -a3 + 32 * a2 - 256 * a, w1 = 3 * a3 - 80 * a2 + 8192, w2 = -3 * a3 + 64 * a2 + 256 * a, w3 = a3 - 16 * a2;
            int32_t x = (w0 * p0[0] + w1 * p1[0] + w2 * p2[0] + w3 * p3[0]) / 8192;
            int32_t y = (w0 * p0[1] + w1 * p1[1] + w2 * p2[1] + w3 * p3[1]) / 8192;
            brush(x0 + x, y0 + y, c);
        }
    }
    ellipse(x0 - 4, y0 + 21, 3, 3, c);             /* the hook's ball */
}
static void note_head(int32_t mid, int32_t d, int32_t x, uint16_t col)
{
    int32_t k, y = step_y(mid, d);
    for (k = 0; k >= d; k -= 2)                    /* ledger lines: C4 below, A5 / C6 above */
        cv_rect(x - 12, step_y(mid, k), 25, 1, C_STAFF);
    for (k = 12; k <= d; k += 2)
        cv_rect(x - 12, step_y(mid, k), 25, 1, C_STAFF);
    ellipse(x, y, 7, 5, col);
    if (d < 6)                                     /* the stem: up on the right below the middle line, else down */
        cv_rect(x + 6, y - 34, 2, 34, col);
    else
        cv_rect(x - 7, y, 2, 35, col);
}
static void staff(int32_t mid)
{
    int32_t k;
    for (k = 2; k <= 10; k += 2)
        cv_rect(ST_X0, step_y(mid, k), ST_X1 - ST_X0, 1, C_STAFF);
    clef(ST_X0 + 18, step_y(mid, 4), C_CLEF);
}
static int32_t step_of(uint32_t midi)              /* the staff step of a white key C4 .. C6 (-1: none) */
{
    static const int8_t ST[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    if (midi < 60u || midi > 84u || ST[midi % 12u] < 0)
        return -1;
    return ST[midi % 12u] + 7 * (int32_t)(midi / 12u - 5u);
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
static void note_label(char *b, uint32_t midi, int octave)   /* "A", "A4", "Eb4" */
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
    text_b(8, 10, &FONT_S, "NOTE RUSH", C_WHITE);
    b[0] = 'x';
    num(b + 1, ui.combo);
    text_r(232, 1, &FONT_L, b, C_YEL);
}
static void clock_bar(void)                        /* as the web's: shown only while a question's time runs */
{
    uint32_t left, f;
    if (ui.gs != GS_ASK || !ui.limit)
        return;
    left = ui.limit - (fm1_ms - ui.ask_t > ui.limit ? ui.limit : fm1_ms - ui.ask_t);
    f = left * 256u / ui.limit;
    cv_rect(0, 35, (int32_t)(240u * f >> 8), 4, f > 128u ? C_OK : f > 51u ? C_YEL : C_ERR);
}
static void card_title(const char *s) { text_c(120, CARD_Y + 8, &FONT_S, s, C_YEL); }
static const char *const PHASE_TITLE[PH_N] = {"FASE 2 - TOQUE A NOTA", "FASE 3 - LEIA A PAUTA",
                                              "FASE 4 - DE OUVIDO"};
static void question(uint16_t col)                 /* the question as asked: the letter, the staff, the ear */
{
    if (ui.phase == PH_NOME)
        text_c(120, 70, &FONT_H, LETTER[ui.cur % 7u], col);
    else if (ui.phase == PH_PAUTA) {
        staff(130);
        note_head(130, ui.cur, ST_NX, col);
    } else {
        speaker(116, 100, C_YEL, ui.tone_off ? C_WHITE : C_YEL);
        button(146, 28, "OUVIR NOTA (PLAY)", C_YEL, C_BOX);
    }
}
static void card_ask(void)
{
    card_title(PHASE_TITLE[ui.phase]);
    if (ui.gs == GS_OK) {
        if (ui.phase == PH_OUVIDO) {
            text_c(120, 70, &FONT_H, LETTER[ui.cur % 7u], C_OK);
        } else
            question(C_OK);
        text_c(120, 212, &FONT_S, "CERTO!", C_OK);
    } else
        question(C_YEL);
}
static void card_over(void)
{
    char b[24], n[8];
    uint32_t m = note_midi(ui.cur);
    card_title(PHASE_TITLE[ui.phase]);
    if (ui.phase == PH_PAUTA) {                    /* the right one, and the one played (where the staff has it) */
        int32_t s = ui.why == WHY_KEY ? step_of(ui.played) : -1;
        staff(130);
        note_head(130, ui.cur, ST_NX - 22, C_OK);
        if (s >= 0)
            note_head(130, s, ST_NX + 26, C_ERR);
    } else
        text_c(120, 70, &FONT_H, LETTER[ui.cur % 7u], C_OK);
    str_cpy(b, ui.why == WHY_TIME ? "TEMPO! - ERA " : "ERROU - ERA ", sizeof b);
    note_label(n, m, ui.phase == PH_PAUTA);
    str_cat(b, n, sizeof b);
    text_c(120, 212, &FONT_S, b, C_ERR);
}
static void card_intro(void)
{
    char b[8];
    uint32_t d = new_note();
    card_title("FASE 1 - MEMORIZE");
    note_label(b, note_midi(d), 1);
    text_c(120, 62, &FONT_XL, b, C_YEL);
    staff(160);
    note_head(160, (int32_t)d, ST_NX, C_YEL);
}
static void score_big(void)
{
    char b[12];
    b[0] = 'x';
    num(b + 1, ui.combo);
    text_c(120, CARD_Y + 8, &FONT_XL, b, C_YEL);
}
static void card_name(void)
{
    static const char *const LBL[4] = {"OCT+", "OCT-", "<", ">"};
    uint32_t i;
    score_big();
    for (i = 0; i < 3u; i++) {                     /* the letters: the one being set plain, the others boxed */
        char s[2] = {ui.name[i], 0};
        int32_t x = 120 - 60 + (int32_t)i * 42;
        if (i != ui.sel)
            cv_rect(x, 104, 36, 40, C_BOX);
        else
            cv_rect(x + 4, 144, 28, 2, C_YEL);
        text_c(x + 18, 108, &FONT_L, s, C_YEL);
    }
    for (i = 0; i < 4u; i++) {                     /* the web's arrows: here the buttons that do it */
        int32_t x = 120 - 94 + (int32_t)i * 48;
        cv_rect(x, 156, 44, 20, C_BOX);
        text_c(x + 22, 158, &FONT_S, LBL[i], C_YEL);
    }
    button(186, 26, "ENVIAR (PLAY)", C_YEL, C_BOX);
    text_c(120, 216, &FONT_S, "PULAR (PATCH)", C_YEL);
}
static void lb_rows(int32_t y, uint32_t hl)
{
    char b[24];
    uint32_t i;
    if (!lb.e[0].score) {
        text_c(120, y + 30, &FONT_S, "NENHUMA PONTUA\xC7\xC3O AINDA", C_MUT);
        return;
    }
    for (i = 0; i < NR_LB && lb.e[i].score; i++) {
        uint16_t c = i == hl ? C_YEL : C_WHITE;
        b[0] = (char)('1' + i), b[1] = '.', b[2] = ' ';
        b[3] = lb.e[i].n[0], b[4] = lb.e[i].n[1], b[5] = lb.e[i].n[2], b[6] = 0;
        text_b(56, y + (int32_t)i * 18, &FONT_S, b, c);
        num(b, lb.e[i].score);
        text_r(184, y + (int32_t)i * 18, &FONT_S, b, c);
    }
}
static void card_board(void)
{
    score_big();
    lb_rows(98, ui.rank);
    button(194, 30, "JOGAR DE NOVO", C_YEL, C_BOX);
}
static void card_setup(void)
{
    uint32_t i;
    card_title("CONFIGURA\xC7\xD5" "ES");
    for (i = 0; i < SET_N; i++) {
        char b[16];
        int32_t y = 70 + (int32_t)i * 28;
        const char *v = b;
        if (i == ui.sel)
            cv_rect(CARD_X + 6, y - 5, CARD_W - 12, 26, C_BOX);
        if (i == SET_TIME)
            v = TIME_NAME[ui.set[i]];
        else if (i == SET_SOUND)
            v = SND_NAME[ui.set[i]];
        else
            num(b, ui.set[i]);
        text_b(CARD_X + 10, y, &FONT_S, SET_NAME[i], C_WHITE);
        text_r(CARD_X + CARD_W - 10, y, &FONT_S, v, C_YEL);
    }
    text_c(120, 186, &FONT_S, "< > ESCOLHE  K1/OCT MUDA", C_MUT);
    text_c(120, 210, &FONT_S, "GLOBE: SALVAR E SAIR", C_YEL);
}
static void title(void)                            /* the web's home: the ranking, the logo, JOGAR */
{
    char b[32];
    cv_rect(CARD_X + 20, 6, CARD_W - 40, 50, C_CARD);
    if (lb.e[0].score) {
        uint32_t i;
        for (i = 0; i < 2u && lb.e[i].score; i++) {
            b[0] = (char)('1' + i), b[1] = '.', b[2] = ' ';
            b[3] = lb.e[i].n[0], b[4] = lb.e[i].n[1], b[5] = lb.e[i].n[2], b[6] = 0;
            text_b(54, 14 + (int32_t)i * 18, &FONT_S, b, C_WHITE);
            num(b, lb.e[i].score);
            text_r(186, 14 + (int32_t)i * 18, &FONT_S, b, C_YEL);
        }
    } else
        text_c(120, 23, &FONT_S, "NENHUMA PONTUA\xC7\xC3O", C_MUT);
    text_b(120 - text_w(&FONT_XL, "NOTE") / 2 + 3, 67, &FONT_XL, "NOTE", C_CARD);   /* the logo: yellow on blue */
    text_c(120, 64, &FONT_XL, "NOTE", C_YEL);
    text_b(120 - text_w(&FONT_XL, "RUSH") / 2 + 3, 113, &FONT_XL, "RUSH", C_CARD);
    text_c(120, 110, &FONT_XL, "RUSH", C_YEL);
    cv_rect(CARD_X + 6, 166, CARD_W - 12, 40, C_CARD);
    text_c(120, 170, &FONT_L, "JOGAR", C_WHITE);
    str_cpy(b, "< ", sizeof b);
    str_cat(b, MODE_NAME[ui.mode], sizeof b);
    str_cat(b, " >  GLOBE: CONFIG", sizeof b);
    text_c(120, 216, &FONT_S, b, C_MUT);
}
static void bolts(void)                            /* the lightning over everything (game.c: lightning) */
{
    uint32_t t = since(bolt.t), b, i, n;
    uint32_t a = t <= 200u ? 256u : t >= NR_OK_MS ? 0u : 256u - (t - 200u) * 256u / (NR_OK_MS - 200u);
    uint16_t core = mix(C_YEL, C_CARD, a * (128u + bolt.k / 2u) >> 8), glow = mix(C_YEL, C_CARD, a * bolt.k >> 9);
    for (b = 0; b < bolt.n; b++) {
        n = t >= 200u ? bolt.np[b] : (bolt.np[b] * t + 199u) / 200u;   /* (it runs out to the combo in 0.2 s) */
        for (i = 1; i < n && i < bolt.np[b]; i++) {
            const int16_t *p = bolt.p[b][i - 1u], *q = bolt.p[b][i];
            if (bolt.k > 96u) {                    /* (a quick answer: wider, a glow) */
                cv_line(p[0] + 1, p[1], q[0] + 1, q[1], glow);
                cv_line(p[0] - 1, p[1], q[0] - 1, q[1], glow);
            }
            cv_line(p[0], p[1], q[0], q[1], core);
        }
    }
}
static void scene(void)
{
    if (ui.gs == GS_TITLE)
        title();
    else {
        header();
        clock_bar();
        cv_rect(CARD_X, CARD_Y, CARD_W, CARD_H, C_CARD);
        switch (ui.gs) {
        case GS_INTRO: card_intro(); break;
        case GS_ASK:
        case GS_OK: card_ask(); break;
        case GS_OVER: card_over(); break;
        case GS_NAME: card_name(); break;
        case GS_BOARD: card_board(); break;
        case GS_SETUP: card_setup(); break;
        }
    }
    if (bolt.t)
        bolts();
    if (ui.msg[0]) {                               /* (the board layer's line, over the card's foot) */
        cv_rect(0, 204, 240, 36, C_BOX);
        text_c(120, 214, &FONT_S, ui.msg, C_WHITE);
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
    if (ui.dirty || ui.gs != shown_gs || bolt.t)
        m = D_ALL;
    if (ui.gs == GS_ASK && ui.limit)
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
