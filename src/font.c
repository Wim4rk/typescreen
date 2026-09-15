#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | ((uint32_t)rd16(p + 2) << 16); }

/* Same mapping as canvas.c: the user's page is rotated clockwise by
 * `rotation` degrees on the panel. */
static void rotate(uint8_t *dst, const uint8_t *src, int w, int h, int rotation) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int px, py;
            switch (rotation) {
            case 90:  px = h - 1 - y; py = x;         break;
            case 180: px = w - 1 - x; py = h - 1 - y; break;
            case 270: px = y;         py = w - 1 - x; break;
            default:  px = x;         py = y;         break;
            }
            int pw = (rotation == 90 || rotation == 270) ? h : w;
            dst[py * pw + px] = src[y * w + x];
        }
    }
}

int font_load(font_t *f, const char *path, int rotation) {
    memset(f, 0, sizeof(*f));

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "font: cannot open %s\n", path);
        return -1;
    }

    uint8_t hdr[16];
    if (fread(hdr, 1, 16, fp) != 16 || memcmp(hdr, "TSGL", 4) != 0 || rd16(hdr + 4) != 1) {
        fprintf(stderr, "font: %s is not a TSGL v1 table\n", path);
        fclose(fp);
        return -1;
    }

    f->cell_w = rd16(hdr + 6);
    f->cell_h = rd16(hdr + 8);
    f->count = rd32(hdr + 12);
    if (f->cell_w == 0 || f->cell_h == 0 || f->count == 0) {
        fprintf(stderr, "font: %s has an empty header\n", path);
        fclose(fp);
        return -1;
    }

    if (rotation == 90 || rotation == 270) {
        f->glyph_w = f->cell_h;
        f->glyph_h = f->cell_w;
    } else {
        f->glyph_w = f->cell_w;
        f->glyph_h = f->cell_h;
    }

    size_t size = (size_t)f->cell_w * f->cell_h;
    f->codepoints = malloc(f->count * sizeof(uint32_t));
    f->bitmaps = malloc(f->count * size);
    f->missing = malloc(size);
    uint8_t *tmp = malloc(size);
    if (!f->codepoints || !f->bitmaps || !f->missing || !tmp) {
        fclose(fp);
        free(tmp);
        font_free(f);
        return -1;
    }

    for (uint32_t i = 0; i < f->count; i++) {
        uint8_t cp[4];
        if (fread(cp, 1, 4, fp) != 4 || fread(tmp, 1, size, fp) != size) {
            fprintf(stderr, "font: %s is truncated\n", path);
            fclose(fp);
            free(tmp);
            font_free(f);
            return -1;
        }
        f->codepoints[i] = rd32(cp);
        if (i > 0 && f->codepoints[i] <= f->codepoints[i - 1]) {
            fprintf(stderr, "font: %s is not sorted by codepoint\n", path);
            fclose(fp);
            free(tmp);
            font_free(f);
            return -1;
        }
        rotate(f->bitmaps + i * size, tmp, f->cell_w, f->cell_h, rotation);
    }
    fclose(fp);

    /* Missing glyph: a hollow box. */
    memset(tmp, 0xF0, size);
    for (int y = 0; y < f->cell_h; y++) {
        for (int x = 0; x < f->cell_w; x++) {
            if (x < 2 || y < 2 || x >= f->cell_w - 2 || y >= f->cell_h - 2)
                tmp[y * f->cell_w + x] = 0x00;
        }
    }
    rotate(f->missing, tmp, f->cell_w, f->cell_h, rotation);
    free(tmp);

    printf("font: %s, %u glyphs, cell %dx%d\n", path, f->count, f->cell_w, f->cell_h);
    return 0;
}

void font_free(font_t *f) {
    free(f->codepoints);
    free(f->bitmaps);
    free(f->missing);
    memset(f, 0, sizeof(*f));
}

const uint8_t *font_glyph(const font_t *f, uint32_t cp) {
    if (cp <= 0x20 || cp == 0xA0) return NULL;

    uint32_t lo = 0, hi = f->count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (f->codepoints[mid] < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < f->count && f->codepoints[lo] == cp)
        return f->bitmaps + (size_t)lo * f->cell_w * f->cell_h;
    return f->missing;
}
