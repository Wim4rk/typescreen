#include "utf8.h"

uint32_t utf8_next(const char **s) {
    unsigned char c = (unsigned char)**s;
    (*s)++;
    if (c < 0x80) return c;

    int extra;
    uint32_t cp;
    if (c < 0xC0)      { return 0xFFFD; }
    else if (c < 0xE0) { cp = c & 0x1F; extra = 1; }
    else if (c < 0xF0) { cp = c & 0x0F; extra = 2; }
    else if (c < 0xF8) { cp = c & 0x07; extra = 3; }
    else               { return 0xFFFD; }

    for (int i = 0; i < extra; i++) {
        unsigned char n = (unsigned char)**s;
        if ((n & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (n & 0x3F);
        (*s)++;
    }
    return cp;
}

int utf8_put(char *dst, uint32_t cp) {
    if (cp < 0x80) {
        dst[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        dst[0] = (char)(0xC0 | (cp >> 6));
        dst[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        dst[0] = (char)(0xE0 | (cp >> 12));
        dst[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        dst[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    dst[0] = (char)(0xF0 | (cp >> 18));
    dst[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    dst[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    dst[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

int utf8_encode(char *dst, size_t size, const uint32_t *cp, size_t n) {
    size_t len = 0;
    for (size_t i = 0; i < n; i++) {
        if (len + 4 >= size) return -1;
        len += utf8_put(dst + len, cp[i]);
    }
    dst[len] = '\0';
    return (int)len;
}
