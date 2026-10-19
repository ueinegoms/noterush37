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
static void sbolts(uint32_t strip);

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
/* treble clef, step d: -7 = C3 .. 0 = C4 .. 14 = C6; the lines E4 G4 B4 D5 F5 (steps 2..10), 10 px apart; mid: the B4 line */
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
    for (k = 0; k >= d; k -= 2)                    /* ledger lines: C4 .. D3 below, A5 / C6 above */
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
static int32_t step_of(uint32_t midi, int32_t *d)  /* the staff step of a white key C3 .. C6 (0: none) */
{
    static const int8_t ST[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    if (midi < 48u || midi > 84u || ST[midi % 12u] < 0)
        return 0;
    *d = ST[midi % 12u] + 7 * ((int32_t)(midi / 12u) - 5);
    return 1;
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
    sbolts(D_HEAD);                                /* (a combo running: its lightning, under the words) */
    text_b(8, 10, &FONT_S, ui.msg[0] ? ui.msg : "NOTE RUSH", ui.msg[0] ? C_YEL : C_WHITE);   /* (or a knob's value) */
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
        staff(125);
        note_head(125, note_step(ui.cur), ST_NX, col);
    } else {
        speaker(116, 100, C_YEL, ui.tone_off ? C_WHITE : C_YEL);
        button(146, 28, "OUVIR NOTA (PAD)", C_YEL, C_BOX);
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
        int32_t s;
        staff(125);
        note_head(125, note_step(ui.cur), ST_NX - 22, C_OK);
        if (ui.why == WHY_KEY && step_of(ui.played, &s))
            note_head(125, s, ST_NX + 26, C_ERR);
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
    staff(150);
    note_head(150, note_step(d), ST_NX, C_YEL);
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
    sbolts(D_MID);                                 /* (the score's lightning, under it) */
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
    button(192, 30, "ENVIAR (PLAY)", C_YEL, C_BOX);
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
    button(194, 30, "IN\xCD" "CIO (PLAY)", C_YEL, C_BOX);
}
/* the VETOR (PARA): the four corners (each its brightness, as set), the figure, its point (where the brightness is
 * taken) and the mod wheel's bolt: its trail from the figure out to the point */
#define VX0 59
#define VY0 75
#define VW 122
static int32_t vx(int32_t q) { return VX0 + (((q + 32768) * (VW - 1)) >> 16); }
static int32_t vy(int32_t q) { return VY0 + (((q + 32768) * (VW - 1)) >> 16); }
static void card_vector(void)
{
    char b[8];
    int32_t i, x, y, px = 0, py = 0, bx = vp.x - vb.x, by = vp.y - vb.y;
    uint32_t c;
    opar_t o;
    card_title("VETOR LISSAJOUS: O BRILHO");
    memset(&o, 0, sizeof o);
    o.wx = vec.ph * 256, o.wy = vec.fx * 256, o.mo = vec.fy * 256;
    for (i = 0; i <= 128; i++) {                   /* the figure, 128 pieces round */
        fam_xy(SH_LISSA, &o, (uint32_t)i << 25, &x, &y);
        x = vx(x * 7 / 8), y = vy(y * 7 / 8);
        if (i)
            cv_line(px, py, x, y, C_STAFF);
        px = x, py = y;
    }
    for (c = 0; c < NC; c++) {                     /* the corners: 1 2 above, 3 4 below; their brightness beside */
        int32_t cx = c & 1u ? VX0 + VW - 1 : VX0, cy = c & 2u ? VY0 + VW - 1 : VY0, v = vec.cut[c];
        ellipse(cx, cy, 8, 8, C_YEL);
        b[0] = (char)('1' + c), b[1] = 0;
        text_c(cx, cy - 8, &FONT_S, b, C_BOX);
        b[0] = v < 0 ? '-' : '+';
        num(b + 1, (uint32_t)(v < 0 ? -v : v));
        if (c & 1u)
            text_b(cx + 11, cy - 8, &FONT_S, b, C_YEL);
        else
            text_r(cx - 10, cy - 8, &FONT_S, b, C_YEL);
    }
    px = vx(bx), py = vy(by);                      /* the bolt's trail out from the figure */
    for (i = 0; i < (int32_t)vb.tn; i++) {
        x = vx(clamp(bx + vb.tr[i][0], -32767, 32767)), y = vy(clamp(by + vb.tr[i][1], -32767, 32767));
        cv_line(px, py, x, y, C_YEL);
        px = x, py = y;
    }
    ellipse(vx(vp.x), vy(vp.y), 4, 4, C_YEL);
    text_c(120, 208, &FONT_S, "K1-3 FORMA K4-7 CANTO K8 VEL", C_MUT);
    text_c(120, 222, &FONT_S, "PARA OU PLAY: SAIR", C_YEL);
}
/* the PATCH screen: the sound's eight knobs as they sit on the keyboard, K1 .. K8 left to right, a column each (a dial
 * and its value); over each pair what it does, drawn in 1 px lines: the filter's curve (BRILHO, RESSONANCIA), the
 * envelope (ATAQUE, RELEASE), the wave driven (DRIVE), the room's tail (SALA), the echoes (ECO TEMPO, ECO REPETE).
 * The knob turned last lit */
#define SP_Y0 70                                   /* the drawings: rows 70 .. 128 */
#define SP_H 58
#define SK_Y 154                                   /* the dials' centre */
static int32_t kcol(uint32_t k) { return 8 + (int32_t)k * 28; }   /* knob k's column, 28 wide */
static void circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* 1 px round, midpoint */
{
    int32_t x = r, y = 0, e = 1 - r;
    while (x >= y) {
        cv_pset(cx + x, cy + y, c), cv_pset(cx - x, cy + y, c), cv_pset(cx + x, cy - y, c), cv_pset(cx - x, cy - y, c);
        cv_pset(cx + y, cy + x, c), cv_pset(cx - y, cy + x, c), cv_pset(cx + y, cy - x, c), cv_pset(cx - y, cy - x, c);
        y++;
        if (e < 0)
            e += 2 * y + 1;
        else
            x--, e += 2 * (y - x) + 1;
    }
}
static void sp_filter(int32_t x0, int32_t w, uint16_t c)   /* flat, a bump at the cutoff (resonance), then down */
{
    int32_t cut = base_cut[0], res = pt.c[0][CP_RES], fc = cut * (w - 1) / 127, x, px = 0, py = 0, bot = SP_Y0 + SP_H;
    for (x = 0; x < w; x++) {
        int32_t d = x - fc, lv = SP_H * 3 / 5, bump = res * (SP_H / 3) / 127, k = d < 0 ? -d : d, y;
        if (k < 7)
            lv += bump * (7 - k) / 7;
        if (d > 0)
            lv -= d * 3;
        y = bot - clamp(lv, 0, SP_H);
        if (x)
            cv_line(x0 + px, py, x0 + x, y, c);
        px = x, py = y;
    }
}
static void sp_env(int32_t x0, int32_t w, uint16_t c)      /* attack, decay, the sustain held, release */
{
    int32_t bot = SP_Y0 + SP_H, top = SP_Y0 + 2, sus = bot - pt.g[G_AS] * (SP_H - 4) / 127;
    int32_t a = 2 + pt.g[G_AA] * 15 / 127, d = 2 + pt.g[G_AD] * 9 / 127, r = 2 + pt.g[G_AR] * 15 / 127;
    int32_t h = clamp(w - 1 - a - d - r, 4, 20), x1 = x0 + a, x2 = x1 + d, x3 = x2 + h, x4 = x3 + r;
    cv_line(x0, bot, x1, top, c);
    cv_line(x1, top, x2, sus, c);
    cv_line(x2, sus, x3, sus, c);
    cv_line(x3, sus, x4, bot, c);
}
static void sp_drive(int32_t x0, int32_t w, uint16_t c)    /* a sine, squared off the more it is driven */
{
    int32_t x, px = 0, py = 0, g = 256 + pt.g[G_DRIVE] * 12, mid = SP_Y0 + SP_H / 2;
    for (x = 0; x < w; x++) {
        int32_t v = clamp((sinq((uint32_t)x * (0xFFFFFFFFu / (uint32_t)(w - 1))) * g) >> 8, -32767, 32767);
        int32_t y = mid - v * (SP_H / 2 - 4) / 32767;
        if (x)
            cv_line(x0 + px, py, x0 + x, y, c);
        px = x, py = y;
    }
}
static void sp_room(int32_t x0, int32_t w, uint16_t c)     /* the tail: ticks dying away, longer the bigger the room */
{
    int32_t n = 3 + pt.g[G_RVSIZE] * (w / 2 - 3) / 127, i, bot = SP_Y0 + SP_H;
    for (i = 0; i < n; i++) {
        int32_t h = (SP_H - 6) * (n - i) / n, j = (int32_t)((uint32_t)(i * 2654435761u) >> 29);   /* (scattered) */
        cv_line(x0 + i * 2, bot, x0 + i * 2, bot - h + j * h / 16, c);
    }
}
static void sp_echo(int32_t x0, int32_t w, uint16_t c)     /* the note, then its echoes: apart by ECO TEMPO, each */
{                                                          /* softer by ECO REPETE */
    int32_t gap = 4 + pt.g[G_DLTIME] * 14 / 127, h = SP_H - 4, x, bot = SP_Y0 + SP_H;
    for (x = x0 + 1; x < x0 + w && h > 1; x += gap) {
        cv_line(x, bot, x, bot - h, c);
        cv_line(x - 1, bot - h, x + 1, bot - h, c);
        h = h * (20 + pt.g[G_DLFB]) / 160;
    }
}
static void card_sound(void)
{
    static const char *const LBL[8] = {"BRI", "RES", "ATQ", "REL", "DRV", "SAL", "ECO", "REP"};
    char b[24];
    uint32_t k, lit = ui.kturn && since(ui.kturn_t) < 2500u ? ui.kturn : 0u;
    str_cpy(b, "SOM: ", sizeof b);
    str_cat(b, SND_NAME[ui.set[SET_SOUND]], sizeof b);
    card_title(b);
    sp_filter(kcol(0), 54, lit == 1u || lit == 2u ? C_YEL : C_STAFF);
    sp_env(kcol(2), 54, lit == 3u || lit == 4u ? C_YEL : C_STAFF);
    sp_drive(kcol(4), 26, lit == 5u ? C_YEL : C_STAFF);
    sp_room(kcol(5) + 1, 26, lit == 6u ? C_YEL : C_STAFF);
    sp_echo(kcol(6), 54, lit == 7u || lit == 8u ? C_YEL : C_STAFF);
    for (k = 0; k < 8u; k++) {                     /* the dials: -135 .. +135 degrees */
        const sctl_t *c = &KNOB[k];
        int32_t v = c->corner ? (c->id == CP_CUT ? base_cut[0] : pt.c[0][c->id]) : pt.g[c->id], lo = c->corner ? CDESC[c->id].min : GDESC[c->id].min;
        int32_t hi = c->corner ? CDESC[c->id].max : GDESC[c->id].max, cx = kcol(k) + 14;
        uint32_t ph = (uint32_t)(((int64_t)(v - lo) * 270 / (hi - lo) - 135) * 11930465);   /* (degrees x 2^32 / 360) */
        uint16_t col = lit == k + 1u ? C_YEL : C_STAFF;
        circle(cx, SK_Y, 11, col);
        cv_line(cx, SK_Y, cx + (sinq(ph) * 10 >> 15), SK_Y - (cosq(ph) * 10 >> 15), col);
        text_c(cx, 170, &FONT_S, LBL[k], lit == k + 1u ? C_YEL : C_MUT);
        num(b, (uint32_t)(v < 0 ? -v : v));
        text_c(cx, 186, &FONT_S, b, lit == k + 1u ? C_YEL : C_WHITE);
    }
    text_c(120, 204, &FONT_S, "FADERS: VOL REV DLY CHO", C_MUT);
    text_c(120, 220, &FONT_S, "GLOBE: TUDO DE F\xC1" "BRICA", C_YEL);
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
    text_c(120, 210, &FONT_S, "GLOBE OU PLAY: SALVAR", C_YEL);
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
    text_b(64, 170, &FONT_L, "JOGAR", C_WHITE);
    text_b(150, 178, &FONT_S, "(PLAY)", C_YEL);
    if (ui.msg[0])                                 /* (a knob's value, or the board layer's line) */
        text_c(120, 216, &FONT_S, ui.msg, C_YEL);
    else {
        str_cpy(b, "< ", sizeof b);
        str_cat(b, MODE_NAME[ui.mode], sizeof b);
        str_cat(b, " >  GLOBE: CONFIG", sizeof b);
        text_c(120, 209, &FONT_S, b, C_MUT);
        text_c(120, 224, &FONT_S, "PATCH: SOM  PARA: VETOR", C_MUT);
    }
}
static void bolts(void)                            /* the lightning over everything (game.c: lightning): 1 px of */
{                                                  /* plain yellow, jagged; it runs up in 0.2 s, then draws back */
    uint32_t t = since(bolt.t), b, i;                /* into the combo */
    for (b = 0; b < bolt.n; b++) {
        uint32_t np = bolt.np[b], n = t >= 200u ? np : (np * t + 199u) / 200u;
        uint32_t i0 = t <= 200u ? 0u : (np - 1u) * (t - 200u) / (NR_OK_MS - 200u);
        for (i = i0 + 1u; i < n; i++)
            cv_line(bolt.p[b][i - 1u][0], bolt.p[b][i - 1u][1], bolt.p[b][i][0], bolt.p[b][i][1], C_YEL);
    }
}
static void sbolts(uint32_t strip)                 /* the band bolts of a strip (game.c: band_bolt): they run */
{                                                  /* out to the right edge, then draw back into it */
    uint32_t i, k, b;
    for (i = 0; i < SB_N; i++) {
        uint32_t t, n, i0;
        if (!sb[i].t || sb[i].strip != strip)
            continue;
        t = since(sb[i].t);
        n = t >= SB_RUN ? sb[i].n : (sb[i].n * t + SB_RUN - 1u) / SB_RUN;
        i0 = t <= SB_LIFE - 130u ? 0u : (sb[i].n - 1u) * (t - (SB_LIFE - 130u)) / 130u;
        for (k = i0 + 1u; k < n; k++)
            cv_line(sb[i].p[k - 1u][0], sb[i].p[k - 1u][1], sb[i].p[k][0], sb[i].p[k][1], C_YEL);
        for (b = 0; b < sb[i].nb; b++)
            if (sb[i].at[b] < n && sb[i].at[b] >= i0) {
                const int16_t *a = sb[i].p[sb[i].at[b]];
                uint32_t j;
                for (j = 0; j < sb[i].bl[b]; j++, a = sb[i].b[b][j - 1u])
                    cv_line(a[0], a[1], sb[i].b[b][j][0], sb[i].b[b][j][1], C_YEL);
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
        case GS_VECTOR: card_vector(); break;
        case GS_SOUND: card_sound(); break;
        }
    }
    if (bolt.t)
        bolts();
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
