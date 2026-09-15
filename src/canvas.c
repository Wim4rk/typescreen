#include "canvas.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int x, y, w, h; } rect_t;

/* User-orientation rectangle to panel rectangle. Same mapping as font.c. */
static rect_t to_panel(const canvas_t *c, rect_t u) {
    int pw = c->dev->width, ph = c->dev->height;
    rect_t p;
    switch (c->rotation) {
    case 90:  p.x = pw - (u.y + u.h); p.y = u.x;             p.w = u.h; p.h = u.w; break;
    case 180: p.x = pw - (u.x + u.w); p.y = ph - (u.y + u.h); p.w = u.w; p.h = u.h; break;
    case 270: p.x = u.y;             p.y = ph - (u.x + u.w); p.w = u.h; p.h = u.w; break;
    default:  p = u; break;
    }
    return p;
}

static rect_t cell_rect(const canvas_t *c, int row, int col) {
    rect_t r = {
        c->margin_left + col * c->font->cell_w,
        c->margin_top + row * c->row_h,
        c->font->cell_w,
        c->font->cell_h,
    };
    return r;
}

int canvas_init(canvas_t *c, const display_t *dev, const font_t *font, int rotation,
                double margin_left_cells, double margin_right_cells,
                double margin_top_cells, double margin_bottom_cells,
                double line_spacing_cells) {
    memset(c, 0, sizeof(*c));
    c->dev = dev;
    c->font = font;
    c->rotation = rotation;

    if (rotation == 90 || rotation == 270) {
        c->page_w = dev->height;
        c->page_h = dev->width;
    } else {
        c->page_w = dev->width;
        c->page_h = dev->height;
    }

    int margin_right = (int)lround(margin_right_cells * font->cell_w);
    int margin_bottom = (int)lround(margin_bottom_cells * font->cell_h);
    c->margin_left = (int)lround(margin_left_cells * font->cell_w);
    c->margin_top = (int)lround(margin_top_cells * font->cell_h);
    c->row_h = font->cell_h + (int)lround(line_spacing_cells * font->cell_h);
    c->cols = (c->page_w - c->margin_left - margin_right) / font->cell_w;
    c->rows = (c->page_h - c->margin_top - margin_bottom) / c->row_h;
    if (c->cols < 1 || c->rows < 1) {
        fprintf(stderr, "canvas: margins leave no room for text\n");
        return -1;
    }

    c->buf = malloc((size_t)dev->width * dev->height);
    if (!c->buf) return -1;

    printf("canvas: %dx%d page, %d columns x %d rows, margins %d/%d/%d/%d px, rotation %d\n",
           c->page_w, c->page_h, c->cols, c->rows,
           c->margin_left, margin_right, c->margin_top, margin_bottom, rotation);
    return 0;
}

void canvas_free(canvas_t *c) {
    free(c->buf);
    memset(c, 0, sizeof(*c));
}

/* Render the cells of rows row0..row1, columns col0..col1 into the user
 * rectangle `area`, then send it to the panel. */
static void draw(canvas_t *c, const page_t *p, rect_t area,
                 int row0, int row1, int col0, int col1, display_mode mode) {
    rect_t out = to_panel(c, area);
    memset(c->buf, 0xF0, (size_t)out.w * out.h);

    int gw = c->font->glyph_w, gh = c->font->glyph_h;

    for (int row = row0; row <= row1; row++) {
        for (int col = col0; col <= col1; col++) {
            const uint8_t *glyph = font_glyph(c->font, page_cell(p, row, col));
            if (!glyph) continue;

            rect_t g = to_panel(c, cell_rect(c, row, col));
            uint8_t *dst = c->buf + (size_t)(g.y - out.y) * out.w + (g.x - out.x);
            for (int y = 0; y < gh; y++) {
                memcpy(dst + (size_t)y * out.w, glyph + (size_t)y * gw, gw);
            }
        }
    }

    display_blit(c->dev, c->buf, out.x, out.y, out.w, out.h, mode);
}

void canvas_draw_cells(canvas_t *c, const page_t *p, int row, int col0, int col1) {
    if (col0 > col1) return;
    rect_t a = cell_rect(c, row, col0);
    a.w = (col1 - col0 + 1) * c->font->cell_w;
    draw(c, p, a, row, row, col0, col1, DISPLAY_FAST);
}

void canvas_draw_rows(canvas_t *c, const page_t *p, int row0, int row1) {
    if (row0 < 0) row0 = 0;
    if (row1 >= c->rows) row1 = c->rows - 1;
    if (row0 > row1) return;

    /* Full page width, including the line spacing below each row. */
    rect_t a = { 0, c->margin_top + row0 * c->row_h, c->page_w, (row1 - row0 + 1) * c->row_h };
    if (a.y + a.h > c->page_h) a.h = c->page_h - a.y;
    draw(c, p, a, row0, row1, 0, c->cols - 1, DISPLAY_FAST);
}

void canvas_draw_page(canvas_t *c, const page_t *p, display_mode mode) {
    rect_t a = { 0, 0, c->page_w, c->page_h };
    draw(c, p, a, 0, c->rows - 1, 0, c->cols - 1, mode);
}

void canvas_draw_pixels(canvas_t *c, const uint8_t *pixels,
                        int x, int y, int w, int h, display_mode mode) {
    rect_t u = { x, y, w, h };
    rect_t out = to_panel(c, u);

    /* Same point mapping as font.c, one pixel at a time. */
    for (int uy = 0; uy < h; uy++) {
        for (int ux = 0; ux < w; ux++) {
            int px, py;
            switch (c->rotation) {
            case 90:  px = h - 1 - uy; py = ux;         break;
            case 180: px = w - 1 - ux; py = h - 1 - uy; break;
            case 270: px = uy;         py = w - 1 - ux; break;
            default:  px = ux;         py = uy;         break;
            }
            c->buf[(size_t)py * out.w + px] = pixels[(size_t)uy * w + ux];
        }
    }

    display_blit(c->dev, c->buf, out.x, out.y, out.w, out.h, mode);
}
