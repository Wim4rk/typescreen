#include "doc.h"
#include "utf8.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void doc_init(doc_t *d) {
    memset(d, 0, sizeof(*d));
}

void doc_free(doc_t *d) {
    free(d->cp);
    memset(d, 0, sizeof(*d));
}

int doc_push(doc_t *d, uint32_t cp) {
    if (d->len == d->cap) {
        size_t cap = d->cap ? d->cap * 2 : 4096;
        uint32_t *n = realloc(d->cp, cap * sizeof(uint32_t));
        if (!n) return -1;
        d->cp = n;
        d->cap = cap;
    }
    d->cp[d->len++] = cp;
    return 0;
}

void doc_pop(doc_t *d) {
    if (d->len > 0) d->len--;
}

int doc_load(doc_t *d, const char *path) {
    d->len = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) return errno == ENOENT ? 0 : -1;

    /* Read the whole file, then decode. \r is dropped. */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) { fclose(fp); return -1; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return -1; }
    size_t got = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    buf[got] = '\0';

    const char *s = buf;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (cp == '\r') continue;
        if (doc_push(d, cp) != 0) { free(buf); return -1; }
    }
    free(buf);
    return 0;
}

int doc_save(const doc_t *d, const char *path) {
    char tmp[1024];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return -1;

    FILE *fp = fopen(tmp, "wb");
    if (!fp) return -1;

    char enc[4];
    for (size_t i = 0; i < d->len; i++) fwrite(enc, 1, (size_t)utf8_put(enc, d->cp[i]), fp);
    if (d->len > 0 && d->cp[d->len - 1] != '\n') fputc('\n', fp);

    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0 || fclose(fp) != 0) {
        unlink(tmp);
        return -1;
    }
    return rename(tmp, path);
}
