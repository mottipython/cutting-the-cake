/*
 * cutting-the-cake: screensaver of Calca 1 from "Anathem".
 *
 * Erasmas has the boy Dath cut a cake into square servings one spatula wide. Four
 * are easy. A square twice as long each way gives sixteen, not eight, and three
 * each way gives nine. So how do you cut exactly eight? It is the old problem of
 * doubling the square, and the answer is to cut corner to corner.
 *
 *   cutting-the-cake            full screen on every display; any key or mouse movement quits
 *   cutting-the-cake --window   in a window; Esc or Q quits
 *   CuttingTheCake.scr /s       Windows screensaver conventions
 */
#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The pictures (drawn with Gemini, prepared by tools/make_sprites.py) are built
 * into the program as PNG and JPEG files and decoded at start-up. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS /* else Windows builds need libwinpthread-1.dll */
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__APPLE__)
/* Mach-O: its own section names, and C names carry a leading underscore */
#define ART_SECTION ".section __DATA,__const\n"
#define ART_SYM "_art_"
#elif defined(_WIN32)
#define ART_SECTION ".section .rdata,\"dr\"\n"
#define ART_SYM "art_"
#else
#define ART_SECTION ".section .rodata\n"
#define ART_SYM "art_"
#endif
#define ART(name, file) \
    __asm__(ART_SECTION ".globl " ART_SYM #name "\n" ART_SYM #name ":\n.incbin \"assets/art/" file "\"\n" \
            ".globl " ART_SYM #name "_end\n" ART_SYM #name "_end:\n.text\n"); \
    extern const unsigned char art_##name[], art_##name##_end[];
ART(table, "table.jpg") ART(cake, "cake.jpg") ART(spatula, "spatula.png") ART(banner, "banner.png")
ART(erasmas, "erasmas.png") ART(dath, "dath.png") ART(dath_hmm, "dath_hmm.png") ART(dath_yay, "dath_yay.png")
ART(arsibalt, "arsibalt.png") ART(arsibalt_yay, "arsibalt_yay.png")

enum { SP_TABLE, SP_CAKE, SP_SPATULA, SP_BANNER, SP_ERASMAS, SP_DATH, SP_DATH_HMM, SP_DATH_YAY,
       SP_ARSIBALT, SP_ARSIBALT_YAY, NSPRITE };
static struct { const unsigned char *file, *end; int w, h; unsigned char *px; } sprite[NSPRITE] = {
    {art_table, art_table_end}, {art_cake, art_cake_end}, {art_spatula, art_spatula_end}, {art_banner, art_banner_end},
    {art_erasmas, art_erasmas_end}, {art_dath, art_dath_end}, {art_dath_hmm, art_dath_hmm_end},
    {art_dath_yay, art_dath_yay_end}, {art_arsibalt, art_arsibalt_end}, {art_arsibalt_yay, art_arsibalt_yay_end},
};
static SDL_Texture **cur_tex; /* the pictures, for the renderer being drawn to */

static void load_sprites(void)
{
    for (int i = 0; i < NSPRITE; i++) {
        int n;
        sprite[i].px = stbi_load_from_memory(sprite[i].file, (int)(sprite[i].end - sprite[i].file), &sprite[i].w, &sprite[i].h, &n, 4);
        if (!sprite[i].px) { sprite[i].w = sprite[i].h = 1; }
    }
}
static void make_textures(SDL_Renderer *r, SDL_Texture **tex)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    for (int i = 0; i < NSPRITE; i++) {
        tex[i] = NULL;
        if (!sprite[i].px) continue;
        tex[i] = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, sprite[i].w, sprite[i].h);
        if (!tex[i]) continue;
        SDL_UpdateTexture(tex[i], NULL, sprite[i].px, sprite[i].w * 4);
        SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
    }
}

/* ---- options ---- */
static int opt_window = 0;
static double opt_speed = 1.0;
static uint64_t opt_seed = 0;
static const char *opt_snapshot = NULL;
static double opt_time = 30;
static int opt_w = 1920, opt_h = 1200;

/* ---- colours ---- */
typedef struct { Uint8 r, g, b; } Rgb;
static const Rgb BG = {24, 18, 14}, INK = {46, 32, 22}, CHORD = {130, 44, 30};
static const Rgb PAN = {58, 54, 52}, PAN_RIM = {156, 150, 142}, CUT = {74, 42, 24}, CRUMB = {250, 232, 190};
static const Rgb GOLD = {255, 200, 60}, PAPER = {253, 246, 226};
/* each new cake has a different icing */
static const Rgb ICING[5] = {{255, 250, 236}, {255, 214, 220}, {236, 244, 206}, {255, 236, 170}, {214, 232, 250}};

/* ---- geometry buffer ---- */
static SDL_Vertex *vb;
static int *ib;
static int nv, ni, vcap, icap;

static void vb_need(int v, int i)
{
    if (nv + v > vcap) { vcap = (nv + v) * 2; vb = realloc(vb, sizeof *vb * vcap); }
    if (ni + i > icap) { icap = (ni + i) * 2; ib = realloc(ib, sizeof *ib * icap); }
}
static inline SDL_Color col(Rgb c, double a)
{
    SDL_Color o = {c.r, c.g, c.b, (Uint8)(a < 0 ? 0 : a > 1 ? 255 : a * 255)};
    return o;
}
static inline void vert(double x, double y, SDL_Color c)
{
    vb[nv].position.x = (float)x; vb[nv].position.y = (float)y;
    vb[nv].color = c; vb[nv].tex_coord.x = vb[nv].tex_coord.y = 0;
    nv++;
}
static void quad4(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3,
                  SDL_Color c0, SDL_Color c1, SDL_Color c2, SDL_Color c3)
{
    vb_need(4, 6);
    int b = nv;
    vert(x0, y0, c0); vert(x1, y1, c1); vert(x2, y2, c2); vert(x3, y3, c3);
    ib[ni++] = b; ib[ni++] = b + 1; ib[ni++] = b + 2; ib[ni++] = b; ib[ni++] = b + 2; ib[ni++] = b + 3;
}
static void quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, SDL_Color c)
{
    quad4(x0, y0, x1, y1, x2, y2, x3, y3, c, c, c, c);
}
static void rect(double x, double y, double w, double h, SDL_Color c) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c); }
static void disc(double cx, double cy, double r, SDL_Color centre, SDL_Color rim)
{
    int n = r < 4 ? 10 : r < 30 ? 28 : 56;
    vb_need(n + 1, 3 * n);
    int b = nv;
    vert(cx, cy, centre);
    for (int i = 0; i < n; i++) vert(cx + r * cos(i * 2 * M_PI / n), cy + r * sin(i * 2 * M_PI / n), rim);
    for (int i = 0; i < n; i++) { ib[ni++] = b; ib[ni++] = b + 1 + i; ib[ni++] = b + 1 + (i + 1) % n; }
}
/* a stroke with round ends */
static void line(double x0, double y0, double x1, double y1, double w, SDL_Color c)
{
    double dx = x1 - x0, dy = y1 - y0, l = hypot(dx, dy);
    if (l > 1e-9) {
        double nx = -dy / l * w / 2, ny = dx / l * w / 2;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
    }
    if (w > 2.5) { disc(x0, y0, w / 2, c, c); disc(x1, y1, w / 2, c, c); }
}
static void ring(double cx, double cy, double r, double w, SDL_Color c)
{
    int n = r < 30 ? 32 : 64;
    vb_need(2 * n, 6 * n);
    int b = nv;
    for (int i = 0; i < n; i++) {
        double a = i * 2 * M_PI / n;
        vert(cx + (r + w / 2) * cos(a), cy + (r + w / 2) * sin(a), c);
        vert(cx + (r - w / 2) * cos(a), cy + (r - w / 2) * sin(a), c);
    }
    for (int i = 0; i < n; i++) {
        int a = b + 2 * i, d = b + 2 * ((i + 1) % n);
        ib[ni++] = a; ib[ni++] = a + 1; ib[ni++] = d; ib[ni++] = a + 1; ib[ni++] = d + 1; ib[ni++] = d;
    }
}
static void flush(SDL_Renderer *r)
{
    if (ni) SDL_RenderGeometry(r, NULL, vb, nv, ib, ni);
    nv = ni = 0;
}

static SDL_Renderer *ren; /* where the current frame is being drawn */

/* a picture stretched over a rectangle; sx0..sx1 picks a slice of it across (0..1) */
static void pic_part(int which, double x, double y, double w, double h, double sx0, double sx1, double tilt, int flip)
{
    flush(ren);
    if (!cur_tex || !cur_tex[which]) return;
    SDL_Rect src = {(int)(sx0 * sprite[which].w), 0, (int)((sx1 - sx0) * sprite[which].w), sprite[which].h};
    SDL_FRect dst = {(float)x, (float)y, (float)w, (float)h};
    SDL_FPoint pivot = {(float)(w / 2), (float)h};
    SDL_RenderCopyExF(ren, cur_tex[which], &src, &dst, tilt, &pivot, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
/* a picture `tall` high, standing with the point (ax, ay) of it (0..1 across and down) at (x, y) */
static void pic(int which, double x, double y, double tall, double ax, double ay)
{
    double w = tall * sprite[which].w / sprite[which].h;
    pic_part(which, x - w * ax, y - tall * ay, w, tall, 0, 1, 0, 0);
}

/* ---- lettering: pen strokes on a 4 x 6 grid, drawn with a round nib ---- */
static const char *GLYPH[] = {
    "A06 02 20 42 46|04 44", "B00 06 36 45 44 33 03|33 42 41 30 00", "C41 30 10 01 05 16 36 45",
    "D00 06 26 44 42 20 00", "E40 00 06 46|03 33", "F40 00 06|03 33", "G41 30 10 01 05 16 36 45 43 23",
    "H00 06|40 46|03 43", "I20 26|10 30|16 36", "J40 45 36 16 05", "K00 06|40 03 46", "L00 06 46",
    "M06 00 23 40 46", "N06 00 46 40", "O10 30 41 45 36 16 05 01 10", "P06 00 30 41 42 33 03",
    "Q10 30 41 45 36 16 05 01 10|24 46", "R06 00 30 41 42 33 03|23 46", "S41 30 10 01 02 13 33 44 45 36 16 05",
    "T00 40|20 26", "U00 05 16 36 45 40", "V00 26 40", "W00 16 23 36 40", "X00 46|40 06", "Y00 23 40|23 26",
    "Z00 40 06 46", "010 30 41 45 36 16 05 01 10", "111 20 26|16 36", "201 10 30 41 42 06 46",
    "301 10 30 41 42 33 23|33 44 45 36 16 05", "436 30 04 44", "540 00 03 33 44 45 36 16 05",
    "630 10 01 05 16 36 45 44 33 03", "700 40 16", "813 02 01 10 30 41 42 33 13 04 05 16 36 45 44 33",
    "943 13 02 01 10 30 41 45 36 16", ",25 26 17", ".26 26", ":22 22|25 25", "'20 21", "?01 10 30 41 42 24|26 26",
    "-13 33", "!20 24|26 26",
};
static const char *glyph(char ch)
{
    for (unsigned i = 0; i < sizeof GLYPH / sizeof *GLYPH; i++)
        if (GLYPH[i][0] == ch) return GLYPH[i] + 1;
    return NULL;
}
static double text_width(const char *str, double s) { return strlen(str) * 6.2 * s - 2.2 * s; }
/* text centred on cx, top at y; s is one grid step, so letters are 6s tall */
static void text(const char *str, double cx, double y, double s, double weight, SDL_Color c)
{
    double x = cx - text_width(str, s) / 2;
    for (; *str; str++, x += 6.2 * s) {
        const char *g = glyph(*str);
        if (!g) continue;
        int have = 0; double lx = 0, ly = 0;
        for (; *g; g++) {
            if (*g == '|') { have = 0; continue; }
            if (*g == ' ') continue;
            double gx = x + (g[0] - '0') * s, gy = y + (g[1] - '0') * s;
            g++;
            if (have) line(lx, ly, gx, gy, s * weight, c);
            disc(gx, gy, s * weight * (have && lx == gx && ly == gy ? 0.62 : 0.5), c, c); /* the nib is round */
            lx = gx; ly = gy; have = 1;
        }
    }
}
/* ---- the lesson ----
 *
 * Calca 1. Erasmas has Dath cut a cake into square servings one spatula wide. Four
 * are easy (two by two). Asked for a square twice as long each way, Dath expects
 * eight and gets sixteen; three each way gives nine. So how do you cut eight?
 * Spin the cake to a clean corner, mark four squares of four servings each, cut
 * each one corner to corner, and the tilted square in the middle is exactly eight.
 *
 * Everything is timed from the start of the round, in seconds.
 */
#define ROUND 84.0
static double T, anim_t;
static int flavour;
static uint64_t seed_state;

static uint64_t next_rand(void)
{
    seed_state ^= seed_state << 13; seed_state ^= seed_state >> 7; seed_state ^= seed_state << 17;
    return seed_state;
}
static void new_round(void) { T = 0; flavour = (int)(next_rand() % 5); }
static void update(double dt)
{
    anim_t += dt;
    T += dt * opt_speed;
    if (T > ROUND) new_round();
}
/* how far along we are between two moments, 0..1 */
static double seg(double a, double b) { return T <= a ? 0 : T >= b ? 1 : (T - a) / (b - a); }
static double ease(double u) { return u * u * (3 - 2 * u); }

/* cuts and marks, in spatula-widths from the corner being worked on */
typedef struct { int corner; double x0, y0, x1, y1, t0, t1; } Stroke;
static const Stroke CUTS[] = {
    /* first corner: two by two */
    {0, 2, 0, 2, 2, 5.0, 6.0}, {0, 0, 2, 2, 2, 6.0, 7.0}, {0, 1, 0, 1, 2, 7.0, 7.8}, {0, 0, 1, 2, 1, 7.8, 8.6},
    /* four by four */
    {0, 4, 0, 4, 4, 16.0, 17.0}, {0, 0, 4, 4, 4, 17.0, 18.0}, {0, 2, 2, 2, 4, 18.0, 18.6}, {0, 2, 2, 4, 2, 18.6, 19.2},
    {0, 1, 2, 1, 4, 19.2, 19.8}, {0, 2, 1, 4, 1, 19.8, 20.4}, {0, 3, 0, 3, 4, 20.4, 21.2}, {0, 0, 3, 4, 3, 21.2, 22.0},
    /* second corner: the four diagonals, then the two cuts that make eight */
    {1, 2, 0, 4, 2, 52.0, 53.4}, {1, 4, 2, 2, 4, 54.0, 55.0}, {1, 2, 4, 0, 2, 55.0, 56.0}, {1, 0, 2, 2, 0, 56.0, 57.0},
    {1, 2, 0, 2, 4, 67.0, 68.4}, {1, 0, 2, 4, 2, 68.4, 69.8},
};
static const Stroke MARKS[] = {
    {1, 0, 0, 2, 0, 39.5, 40.1}, {1, 2, 0, 2, 2, 40.1, 40.7}, {1, 2, 2, 0, 2, 40.7, 41.3}, {1, 0, 2, 0, 0, 41.3, 41.9},
    {1, 2, 0, 4, 0, 43.0, 43.5}, {1, 4, 0, 4, 4, 43.5, 44.3}, {1, 4, 4, 0, 4, 44.3, 45.1}, {1, 0, 4, 0, 2, 45.1, 45.6},
    {1, 2, 2, 2, 4, 45.6, 46.1}, {1, 2, 2, 4, 2, 46.1, 46.6},
};
#define NCUTS (int)(sizeof CUTS / sizeof *CUTS)
#define NMARKS (int)(sizeof MARKS / sizeof *MARKS)

/* who says what, and when */
enum { ERASMAS, DATH, ARSIBALT };
typedef struct { double t0, t1; int who; const char *text; } Line;
static const Line LINES[] = {
    {1.5, 4.6, ERASMAS, "EACH SERVING A SQUARE,|ONE SPATULA WIDE."},
    {8.8, 10.4, ERASMAS, "HOW MANY SERVINGS?"},
    {10.6, 12.2, DATH, "FOUR."},
    {12.4, 15.6, ERASMAS, "NOW TWICE AS LONG|EACH WAY. HOW MANY?"},
    {13.6, 15.8, DATH, "TWO TIMES FOUR|IS EIGHT."},
    {24.6, 27.4, DATH, "SIXTEEN.|NOT EIGHT!"},
    {27.6, 29.6, ERASMAS, "THEN HOW DO WE|CUT EIGHT?"},
    {29.2, 30.6, DATH, "THREE EACH WAY?"},
    {32.2, 34.0, DATH, "NO. THAT IS NINE."},
    {34.2, 37.6, ERASMAS, "NOW YOU KNOW THAT|YOU DO NOT KNOW."},
    {38.2, 42.0, ERASMAS, "MARK A SQUARE BIG ENOUGH|FOR FOUR SERVINGS."},
    {42.4, 46.6, ERASMAS, "AND THREE MORE|JUST LIKE IT."},
    {47.0, 49.0, DATH, "SIXTEEN AGAIN."},
    {49.2, 52.6, ARSIBALT, "CUT ONE IN HALF. AND MIND|MY HALF IS NO SMALLER."},
    {53.6, 56.8, ERASMAS, "GOOD. NOW THE|OTHER THREE."},
    {57.4, 60.6, ERASMAS, "LOOK AT IT THIS WAY.|WHAT IS IN THE MIDDLE?"},
    {60.8, 62.2, DATH, "A SQUARE."},
    {62.4, 64.6, ERASMAS, "FOUR TRIANGLES,|TWO SERVINGS EACH."},
    {64.8, 67.0, DATH, "EIGHT!"},
    {70.6, 74.0, ARSIBALT, "AT LAST. CAKE."},
    {74.2, 79.0, ERASMAS, "NOT ROUGHLY EIGHT.|EXACTLY EIGHT."},
};
#define NLINES (int)(sizeof LINES / sizeof *LINES)

/* ---- drawing ---- */
#define PAN_N 9.0      /* the pan is nine spatula-widths square */
static double unit, S, fcx, fcy, th_pan, th_view;

/* a point of the cake, measured from the corner being worked on, to the screen.
 * corner 0 is the first corner; corner 1 is the opposite one, reached by spinning
 * the pan half a turn. The view can also be turned about the middle of the work. */
static void tr(int corner, double x, double y, double *ox, double *oy)
{
    if (corner) { x = PAN_N - x; y = PAN_N - y; }
    double c = cos(th_pan), s = sin(th_pan), px = x - PAN_N / 2, py = y - PAN_N / 2;
    double qx = c * px - s * py + PAN_N / 2 - 2, qy = s * px + c * py + PAN_N / 2 - 2;
    double c2 = cos(th_view), s2 = sin(th_view);
    *ox = fcx + (c2 * qx - s2 * qy) * S;
    *oy = fcy - (s2 * qx + c2 * qy) * S;
}
static void wline(int corner, double x0, double y0, double x1, double y1, double w, SDL_Color c)
{
    double ax, ay, bx, by;
    tr(corner, x0, y0, &ax, &ay); tr(corner, x1, y1, &bx, &by);
    line(ax, ay, bx, by, w * S, c);
}
/* a convex piece of the cake's top, moved (dx, dy) on screen. Drawn as a fan about
 * its middle: two bare triangles making a rectangle are mis-drawn by some renderers. */
static void cake_piece(int corner, const double *x, const double *y, int n, double dx, double dy, double alpha, double shade)
{
    flush(ren);
    vb_need(n + 1, 3 * n);
    Rgb ic = ICING[flavour];
    SDL_Color c = {(Uint8)(ic.r * shade), (Uint8)(ic.g * shade), (Uint8)(ic.b * shade), (Uint8)(alpha * 255)};
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) { mx += x[i] / n; my += y[i] / n; }
    for (int i = -1; i < n; i++) {
        double wx = i < 0 ? mx : x[i], wy = i < 0 ? my : y[i], sx, sy;
        double px = corner ? PAN_N - wx : wx, py = corner ? PAN_N - wy : wy;
        tr(corner, wx, wy, &sx, &sy);
        vert(sx + dx, sy + dy, c);
        vb[nv - 1].tex_coord.x = (float)(px / PAN_N); vb[nv - 1].tex_coord.y = (float)(1 - py / PAN_N);
    }
    for (int i = 0; i < n; i++) { ib[ni++] = 0; ib[ni++] = 1 + i; ib[ni++] = 1 + (i + 1) % n; }
    SDL_RenderGeometry(ren, cur_tex ? cur_tex[SP_CAKE] : NULL, vb, nv, ib, ni);
    nv = ni = 0;
}
static void wpoly(int corner, const double *x, const double *y, int n, SDL_Color c)
{
    vb_need(n, 3 * (n - 2));
    int b = nv;
    for (int i = 0; i < n; i++) { double sx, sy; tr(corner, x[i], y[i], &sx, &sy); vert(sx, sy, c); }
    for (int i = 1; i < n - 1; i++) { ib[ni++] = b; ib[ni++] = b + i; ib[ni++] = b + i + 1; }
}
static void outline(int corner, const double *x, const double *y, int n, double w, SDL_Color c)
{
    for (int i = 0; i < n; i++) wline(corner, x[i], y[i], x[(i + 1) % n], y[(i + 1) % n], w, c);
}
/* a number written on the icing, popping up as it is counted */
static void count_at(int corner, double x, double y, int n, double pop, double size)
{
    if (pop <= 0) return;
    char b[8];
    double sx, sy, k = pop >= 1 ? 1 : 0.3 + 0.9 * pop, s = size * S * k;
    snprintf(b, sizeof b, "%d", n);
    double ts = n > 9 ? s * 0.68 : s; /* two figures are written smaller to fit */
    tr(corner, x, y, &sx, &sy);
    disc(sx, sy, s * 4.6, col(PAPER, 0.9), col(PAPER, 0.9));
    ring(sx, sy, s * 4.6, s * 0.5, col(INK, 0.9));
    text(b, sx, sy - ts * 3, ts, 1.1, col(INK, 1));
}

/* the eight triangular servings of the tilted square, by number */
static void serving(int i, double *x, double *y)
{
    static const double V[4][2] = {{2, 0}, {4, 2}, {2, 4}, {0, 2}};
    int a = i / 2, b = (a + 1) % 4;
    double mx = (V[a][0] + V[b][0]) / 2, my = (V[a][1] + V[b][1]) / 2;
    x[0] = 2; y[0] = 2;
    if (i % 2 == 0) { x[1] = V[a][0]; y[1] = V[a][1]; x[2] = mx; y[2] = my; }
    else { x[1] = mx; y[1] = my; x[2] = V[b][0]; y[2] = V[b][1]; }
}

static void draw_cake(int w, int h)
{
    static const double SQ[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    double x[8], y[8];
    /* the pan and the cake in it */
    double rim = 0.42, lip = 0.16;
    for (int i = 0; i < 4; i++) { x[i] = -rim + SQ[i][0] * (PAN_N + 2 * rim); y[i] = -rim + SQ[i][1] * (PAN_N + 2 * rim); }
    for (int i = 0; i < 4; i++) { double sx, sy; tr(0, x[i], y[i], &sx, &sy); x[4 + i] = sx; y[4 + i] = sy; }
    quad(x[4] + S * 0.25, y[4] + S * 0.3, x[5] + S * 0.25, y[5] + S * 0.3, x[6] + S * 0.25, y[6] + S * 0.3, x[7] + S * 0.25, y[7] + S * 0.3, col(INK, 0.35));
    wpoly(0, x, y, 4, col(PAN_RIM, 1));
    for (int i = 0; i < 4; i++) { x[i] = -lip + SQ[i][0] * (PAN_N + 2 * lip); y[i] = -lip + SQ[i][1] * (PAN_N + 2 * lip); }
    wpoly(0, x, y, 4, col(PAN, 1));
    /* the cake's top, laid down a spatula-width square at a time and only where it
     * shows: very large triangles running far off screen upset some renderers */
    for (int i = 0; i < (int)PAN_N; i++)
        for (int j = 0; j < (int)PAN_N; j++) {
            int seen = 0;
            for (int q = 0; q < 4; q++) {
                double sx, sy;
                x[q] = i + SQ[q][0]; y[q] = j + SQ[q][1];
                tr(0, x[q], y[q], &sx, &sy);
                if (sx > -S && sx < w + S && sy > -S && sy < h + S) seen = 1;
            }
            if (seen) cake_piece(0, x, y, 4, 0, 0, 1, 1);
        }

    /* servings that have been eaten leave bare pan; ones on their way out fly left */
    for (int i = 0; i < 8; i++) {
        double go = seg(71.0 + i * 0.55, 72.4 + i * 0.55);
        if (go <= 0) continue;
        serving(i, x, y);
        wpoly(1, x, y, 3, col(PAN, 1));
        flush(ren);
        if (go < 1) cake_piece(1, x, y, 3, -ease(go) * w * 0.34, -sin(go * M_PI) * h * 0.08 + ease(go) * h * 0.1, 1 - go * go, 1);
    }

    /* marks drawn on the icing */
    for (int i = 0; i < NMARKS; i++) {
        const Stroke *m = &MARKS[i];
        double p = seg(m->t0, m->t1);
        if (p <= 0) continue;
        double L = hypot(m->x1 - m->x0, m->y1 - m->y0) * p, ux = (m->x1 - m->x0) / (L / p), uy = (m->y1 - m->y0) / (L / p);
        for (double q = 0; q < L; q += 0.3)
            wline(m->corner, m->x0 + ux * q, m->y0 + uy * q, m->x0 + ux * fmin(q + 0.16, L), m->y0 + uy * fmin(q + 0.16, L), 0.07, col(CHORD, 0.9));
    }
    /* things pointed out: the three-by-three, the square to halve, the tilted square */
    double glow = 0.5 + 0.2 * sin(anim_t * 5), p;
    if ((p = seg(29.4, 30.2) * (1 - seg(33.6, 34.2))) > 0) {
        for (int i = 0; i < 4; i++) { x[i] = SQ[i][0] * 3; y[i] = SQ[i][1] * 3; }
        wpoly(0, x, y, 4, col(GOLD, 0.3 * glow * p)); outline(0, x, y, 4, 0.1, col(GOLD, p));
    }
    if ((p = seg(49.2, 49.8) * (1 - seg(53.4, 54.0))) > 0) {
        for (int i = 0; i < 4; i++) { x[i] = 2 + SQ[i][0] * 2; y[i] = SQ[i][1] * 2; }
        wpoly(1, x, y, 4, col(GOLD, 0.35 * glow * p)); outline(1, x, y, 4, 0.1, col(GOLD, p));
    }
    if ((p = seg(59.4, 60.2) * (1 - seg(70.4, 71.0))) > 0) {
        x[0] = 2; y[0] = 0; x[1] = 4; y[1] = 2; x[2] = 2; y[2] = 4; x[3] = 0; y[3] = 2;
        wpoly(1, x, y, 4, col(GOLD, 0.3 * glow * p)); outline(1, x, y, 4, 0.1, col(GOLD, p));
    }
    /* cuts: a dark line with a crumbly edge, and the spatula at the end of the newest */
    double tipx = 0, tipy = 0; int cutting = 0;
    for (int i = 0; i < NCUTS; i++) {
        const Stroke *c = &CUTS[i];
        p = ease(seg(c->t0, c->t1));
        if (p <= 0) continue;
        double ex = c->x0 + (c->x1 - c->x0) * p, ey = c->y0 + (c->y1 - c->y0) * p;
        wline(c->corner, c->x0, c->y0, ex, ey, 0.13, col(CRUMB, 0.9));
        wline(c->corner, c->x0, c->y0, ex, ey, 0.06, col(CUT, 1));
        if (p < 1) { tr(c->corner, ex, ey, &tipx, &tipy); cutting = 1; }
    }
    /* counting */
    for (int i = 0; i < 4; i++) /* one to four */
        count_at(0, 0.5 + i % 2, 0.5 + i / 2, i + 1, seg(8.8 + i * 0.35, 9.0 + i * 0.35) * (1 - seg(15.4, 15.8)), 0.06);
    for (int i = 0; i < 16; i++) /* one to sixteen */
        count_at(0, 0.5 + i % 4, 0.5 + i / 4, i + 1, seg(22.2 + i * 0.14, 22.35 + i * 0.14) * (1 - seg(28.6, 29.0)), 0.06);
    for (int i = 0; i < 9; i++) /* one to nine */
        count_at(0, 0.5 + i % 3, 0.5 + i / 3, i + 1, seg(30.4 + i * 0.18, 30.55 + i * 0.18) * (1 - seg(33.8, 34.2)), 0.06);
    for (int i = 0; i < 4; i++) /* four servings in each marked square */
        count_at(1, 1 + 2 * (i % 2), 1 + 2 * (i / 2), 4, seg(47.0 + i * 0.3, 47.2 + i * 0.3) * (1 - seg(49.0, 49.4)), 0.085);
    for (int i = 0; i < 4; i++) { /* two in each triangle of the tilted square */
        static const double C[4][2] = {{2.67, 1.33}, {2.67, 2.67}, {1.33, 2.67}, {1.33, 1.33}};
        count_at(1, C[i][0], C[i][1], 2, seg(62.4 + i * 0.35, 62.6 + i * 0.35) * (1 - seg(66.6, 67.0)), 0.085);
    }
    for (int i = 0; i < 8; i++) { /* the eight servings */
        serving(i, x, y);
        count_at(1, (x[0] + x[1] + x[2]) / 3, (y[0] + y[1] + y[2]) / 3, i + 1, seg(69.9 + i * 0.12, 70.0 + i * 0.12) * (1 - seg(70.8 + i * 0.55, 71.0 + i * 0.55)), 0.06);
    }
    flush(ren);
    if (cutting) { /* blade on the cut, handle towards the cook */
        double tall = S * 3.6, sw = tall * sprite[SP_SPATULA].w / sprite[SP_SPATULA].h;
        SDL_FRect dst = {(float)(tipx - sw / 2), (float)(tipy - tall * 0.12), (float)sw, (float)tall};
        SDL_FPoint pivot = {(float)(sw / 2), (float)(tall * 0.12)};
        if (cur_tex && cur_tex[SP_SPATULA]) SDL_RenderCopyExF(ren, cur_tex[SP_SPATULA], NULL, &dst, 48 + sin(anim_t * 14) * 3, &pivot, SDL_FLIP_NONE);
    }
}

/* a speech bubble above a speaker; '|' in the text starts a second line */
static void bubble(double x, double y, const char *str, double pop)
{
    char a[80], b[80] = "";
    const char *bar = strchr(str, '|');
    snprintf(a, sizeof a, "%.*s", bar ? (int)(bar - str) : (int)strlen(str), str);
    if (bar) snprintf(b, sizeof b, "%s", bar + 1);
    double s = unit * 0.3 * (pop >= 1 ? 1 : 0.5 + 0.5 * pop), lines = b[0] ? 2 : 1;
    double bw = fmax(text_width(a, s), text_width(b, s)) + s * 14, bh = s * (lines * 8.6 + 6);
    double edge = unit * 1.5;
    if (x - bw / 2 < edge) x = edge + bw / 2; /* keep it on the screen */
    double x0 = x - bw / 2, y0 = y - bh, r = s * 3.2;
    for (int pass = 0; pass < 2; pass++) { /* ink edge, then paper */
        double g = pass ? 0 : s * 0.75;
        SDL_Color c = col(pass ? PAPER : INK, 1);
        rect(x0 + r - g, y0 - g, bw - 2 * r + 2 * g, bh + 2 * g, c);
        rect(x0 - g, y0 + r - g, bw + 2 * g, bh - 2 * r + 2 * g, c);
        disc(x0 + r, y0 + r, r + g, c, c); disc(x0 + bw - r, y0 + r, r + g, c, c);
        disc(x0 + r, y0 + bh - r, r + g, c, c); disc(x0 + bw - r, y0 + bh - r, r + g, c, c);
        quad(x - s * 3 - g, y0 + bh - 1, x + s * 3 + g, y0 + bh - 1, x - s * 1, y0 + bh + s * 5 + g * 1.6, x - s * 1, y0 + bh + s * 5 + g * 1.6, c);
    }
    text(a, x, y0 + s * 3.6, s, 1.0, col(INK, 1));
    if (b[0]) text(b, x, y0 + s * 12.2, s, 1.0, col(INK, 1));
}

/* the caption, on a scroll across the bottom */
static void draw_caption(int w, int h, const char *l1, const char *l2)
{
    double s1 = unit * 0.5, s2 = unit * 0.34;
    double widest = fmax(text_width(l1, s1), text_width(l2, s2)), room = w * 0.8 - unit * 8;
    if (widest > room) { s1 *= room / widest; s2 *= room / widest; }
    double bw = fmax(text_width(l1, s1), text_width(l2, s2)) + unit * 13, bh = unit * 12;
    double bx = w / 2.0 - bw / 2, by = h - unit * 2 - bh;
    double cap = 0.135, cw = bh * 1.15 * sprite[SP_BANNER].w * cap / sprite[SP_BANNER].h;
    rect(bx + cw * 0.6 + unit * 0.5, by + bh * 0.16 + unit * 0.6, bw - cw * 1.2, bh * 0.72, col(INK, 0.3));
    pic_part(SP_BANNER, bx + cw - 1, by, bw - 2 * cw + 2, bh, cap, 1 - cap, 0, 0);
    pic_part(SP_BANNER, bx, by, cw, bh, 0, cap, 0, 0);
    pic_part(SP_BANNER, bx + bw - cw, by, cw, bh, 1 - cap, 1, 0, 0);
    text(l1, w / 2.0, by + bh * 0.24, s1, 0.95, col(INK, 1));
    text(l2, w / 2.0, by + bh * 0.24 + s1 * 6 + unit * 1.3, s2, 0.9, col(CHORD, 1));
}

static void draw_scene(SDL_Renderer *r, int w, int h)
{
    ren = r;
    unit = (w < h ? w : h) * 0.0125;
    S = h * 0.108;
    fcx = w * 0.64; fcy = h * 0.43;
    th_pan = M_PI * ease(seg(35.0, 38.6));            /* spin to a clean corner */
    th_view = -M_PI / 4 * ease(seg(57.4, 60.0));      /* "look at it this way" */
    SDL_SetRenderDrawColor(r, BG.r, BG.g, BG.b, 255);
    SDL_RenderClear(r);
    pic_part(SP_TABLE, 0, 0, w, h, 0, 1, 0, 0);
    draw_cake(w, h);

    /* the three of them, down the left of the table */
    int dath = T > 22.4 && T < 37 ? SP_DATH_HMM : T > 64.6 ? SP_DATH_YAY : SP_DATH;
    int ars = T > 70.4 ? SP_ARSIBALT_YAY : SP_ARSIBALT;
    int who[3] = {SP_ERASMAS, dath, ars};
    double base = h * 0.80, px[3] = {w * 0.215, w * 0.355, w * 0.08}, tall[3];
    for (int i = 0; i < 3; i++) tall[i] = h * 0.43 * sprite[who[i]].h / sprite[SP_ERASMAS].h; /* all drawn to one scale */
    for (int k = 2; k >= 0; k--) {
        int i = k == 2 ? 2 : k == 1 ? 0 : 1; /* Arsibalt at the back, Dath in front */
        double bob = sin(anim_t * 1.6 + i * 2) * unit * 0.25;
        disc(px[i], base, tall[i] * 0.3, col(INK, 0.3), col(INK, 0));
        pic(who[i], px[i], base + bob, tall[i], 0.5, 1);
    }
    for (int i = 0; i < NLINES; i++) {
        const Line *l = &LINES[i];
        if (T < l->t0 || T > l->t1) continue;
        double pop = fmin((T - l->t0) / 0.2, 1) * fmin((l->t1 - T) / 0.15, 1);
        bubble(px[l->who] + unit * 2, base - h * 0.43 * sprite[l->who == DATH ? SP_DATH : l->who == ERASMAS ? SP_ERASMAS : SP_ARSIBALT].h / sprite[SP_ERASMAS].h - unit * 2.5, l->text, pop);
    }
    const char *l1 = "CUTTING THE CAKE", *l2 = "HOW DO YOU CUT EIGHT EQUAL SERVINGS?";
    if (T > 8.6) { l1 = "TWO EACH WAY"; l2 = "FOUR SERVINGS"; }
    if (T > 15.8) { l1 = "FOUR EACH WAY"; l2 = "TWICE AS LONG IS NOT TWICE AS MUCH"; }
    if (T > 24.4) { l1 = "FOUR EACH WAY"; l2 = "SIXTEEN SERVINGS"; }
    if (T > 29.2) { l1 = "THREE EACH WAY"; l2 = T > 32 ? "NINE SERVINGS" : "?"; }
    if (T > 34.6) { l1 = "A CLEAN CORNER"; l2 = "SPIN THE CAKE ROUND"; }
    if (T > 39.0) { l1 = "FOUR SQUARES"; l2 = "FOUR SERVINGS IN EACH"; }
    if (T > 49.2) { l1 = "CORNER TO CORNER"; l2 = "ONE CUT HALVES A SQUARE EXACTLY"; }
    if (T > 57.4) { l1 = "THE SQUARE IN THE MIDDLE"; l2 = "FOUR HALVES OF FOUR"; }
    if (T > 64.6) { l1 = "EIGHT SERVINGS"; l2 = "A SQUARE WITH TWICE THE CAKE OF THE FIRST"; }
    if (T > 74.2) { l1 = "EXACTLY EIGHT"; l2 = "NOT ROUGHLY. EXACTLY."; }
    draw_caption(w, h, l1, l2);
    flush(r);
    double fade = fmin(seg(0, 1.4), 1 - seg(ROUND - 1.6, ROUND - 0.2));
    if (fade < 1) { rect(0, 0, w, h, col(BG, 1 - fade)); flush(r); }
}

typedef struct { SDL_Window *win; SDL_Renderer *ren; SDL_Texture *scene, *tex[NSPRITE]; int w, h; } View;

static void size_view(View *v)
{
    int ow, oh;
    SDL_RendererInfo info;
    SDL_GetRendererOutputSize(v->ren, &ow, &oh);
    if (ow < 1 || oh < 1) return;
    int ss = 2; /* drawn at twice the size and scaled down, for smooth edges */
    if (SDL_GetRendererInfo(v->ren, &info) == 0) {
        if (!(info.flags & SDL_RENDERER_TARGETTEXTURE) || (info.flags & SDL_RENDERER_SOFTWARE)) ss = 0;
        else if (info.max_texture_width && (ow * 2 > info.max_texture_width || oh * 2 > info.max_texture_height)) ss = 0;
    }
    if (v->w == ow * (ss ? ss : 1) && v->h == oh * (ss ? ss : 1) && (v->scene || !ss)) return;
    if (v->scene) SDL_DestroyTexture(v->scene);
    v->scene = NULL; v->w = ow; v->h = oh;
    if (!ss) return;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    v->scene = SDL_CreateTexture(v->ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ow * ss, oh * ss);
    if (v->scene) { v->w = ow * ss; v->h = oh * ss; }
}
static void render(View *v)
{
    if (v->scene) SDL_SetRenderTarget(v->ren, v->scene);
    cur_tex = v->tex;
    draw_scene(v->ren, v->w, v->h);
    if (v->scene) { SDL_SetRenderTarget(v->ren, NULL); SDL_RenderCopy(v->ren, v->scene, NULL, NULL); }
    SDL_RenderPresent(v->ren);
}
static int make_view(View *v, int display)
{
    Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI;
    int x = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    if (opt_window) flags |= SDL_WINDOW_RESIZABLE;
    else flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
    memset(v, 0, sizeof *v);
    v->win = SDL_CreateWindow("Cutting the Cake", x, x, 1200, 750, flags);
    if (!v->win) return 0;
    v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!v->ren) v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_SOFTWARE);
    if (!v->ren) return 0;
    SDL_SetRenderDrawBlendMode(v->ren, SDL_BLENDMODE_BLEND);
    make_textures(v->ren, v->tex);
    return 1;
}

static void start_show(void)
{
    seed_state = opt_seed ? opt_seed * 0x9E3779B97F4A7C15ULL : ((uint64_t)time(NULL) << 20) ^ SDL_GetPerformanceCounter();
    if (!seed_state) seed_state = 1;
    for (int i = 0; i < 6; i++) next_rand();
    anim_t = 0;
    new_round();
}

static int snapshot(void)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, opt_w, opt_h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!r) { fprintf(stderr, "cutting-the-cake: %s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Texture *tex[NSPRITE];
    make_textures(r, tex);
    cur_tex = tex;
    start_show();
    for (double t = 0; t < opt_time; t += 1.0 / 60) update(1.0 / 60);
    draw_scene(r, opt_w, opt_h);
    SDL_RenderPresent(r);
    if (SDL_SaveBMP(surf, opt_snapshot) != 0) { fprintf(stderr, "cutting-the-cake: %s\n", SDL_GetError()); return 1; }
    return 0;
}

static void usage(void)
{
    static const char text[] =
        "Cutting the Cake screensaver\n\n"
        "  --window        run in a window (Esc or Q quits)\n"
        "  --speed X       animation speed, 1 = normal\n"
        "  --seed N        repeatable sequence\n"
        "  --snapshot FILE.bmp [--time SECONDS] [--size WxH]\n"
        "                  write a picture of the screensaver after SECONDS and exit\n";
    if (getenv("SDL_VIDEODRIVER") && !strcmp(getenv("SDL_VIDEODRIVER"), "dummy")) {
        fputs(text, stderr);
        return;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Cutting the Cake", text, NULL);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((a[0] == '/' || a[0] == '-') && (a[1] == 's' || a[1] == 'S') && !a[2]) continue;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'p' || a[1] == 'P') && (!a[2] || a[2] == ':')) return 0;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'c' || a[1] == 'C') && (!a[2] || a[2] == ':')) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Cutting the Cake", "Cutting the Cake screensaver has no settings.", NULL);
            return 0;
        }
        else if (!strcmp(a, "--window")) opt_window = 1;
        else if (!strcmp(a, "--speed") && i + 1 < argc) opt_speed = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--seed") && i + 1 < argc) opt_seed = SDL_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--snapshot") && i + 1 < argc) opt_snapshot = argv[++i];
        else if (!strcmp(a, "--time") && i + 1 < argc) {
            opt_time = SDL_atof(argv[++i]);
            if (opt_time < 0.0) opt_time = 0.0;
            if (opt_time > 86400.0) opt_time = 86400.0;
        }
        else if (!strcmp(a, "--size") && i + 1 < argc) {
            const char *x = SDL_strchr(argv[++i], 'x');
            opt_w = SDL_atoi(argv[i]); opt_h = x ? SDL_atoi(x + 1) : 0;
            if (opt_w < 64 || opt_h < 64 || opt_w > 8192 || opt_h > 8192) { usage(); return 1; }
        }
        else { usage(); return 1; }
    }
    if (opt_speed < 0.05) opt_speed = 0.05;
    load_sprites();

    if (opt_snapshot) return snapshot();
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "cutting-the-cake: %s\n", SDL_GetError()); return 1; }

    View views[8];
    int nviews = 0, ndisp = opt_window ? 1 : SDL_GetNumVideoDisplays();
    if (ndisp < 1) ndisp = 1;
    for (int d = 0; d < ndisp && nviews < 8; d++)
        if (make_view(&views[nviews], d)) nviews++;
    if (!nviews) { fprintf(stderr, "cutting-the-cake: %s\n", SDL_GetError()); return 1; }
    if (!opt_window) SDL_ShowCursor(SDL_DISABLE);
    start_show();

    Uint64 freqc = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter(), start = last;
    double moved = 0;
    int running = 1;
    while (running) {
        SDL_Event e;
        double since = (double)(SDL_GetPerformanceCounter() - start) / freqc;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                if (!opt_window || e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q) running = 0;
            } else if (!opt_window && since > 1.0) {
                if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL || e.type == SDL_FINGERDOWN) running = 0;
                else if (e.type == SDL_MOUSEMOTION) {
                    moved += abs(e.motion.xrel) + abs(e.motion.yrel);
                    if (moved > 24) running = 0;
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / freqc;
        last = now;
        if (dt > 0.1) dt = 0.1;
        update(dt);
        for (int i = 0; i < nviews; i++) { size_view(&views[i]); render(&views[i]); }
        SDL_Delay(views[0].scene ? 1 : 12);
    }
    SDL_Quit();
    return 0;
}
