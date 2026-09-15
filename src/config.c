#include "config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_defaults(config_t *c) {
    memset(c, 0, sizeof(*c));
    c->vcom_mv = 0;
    c->rotation = 0;
    c->margin_left = c->margin_right = 3;
    c->margin_top = c->margin_bottom = 1;
    c->line_spacing = 1;
    c->keep_rows = 3;
    strcpy(c->font, "fonts/OldTimeyMono-24x43.bin");
    strcpy(c->keyboard, "/dev/input/event0");
    strcpy(c->layout, "layouts/us.conf");
    strcpy(c->directory, "~");
}

static void set_str(char *dst, size_t size, const char *v) {
    snprintf(dst, size, "%s", v);
}

/* Accepts the value as printed on the panel's cable ("-2.14", volts) or
 * as millivolts ("2140"). Sign is ignored. */
static int parse_vcom(const char *v) {
    double x = fabs(strtod(v, NULL));
    if (x == 0) return 0;
    if (x < 100) x *= 1000;
    return (int)lround(x);
}

int config_load(config_t *c, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "config: cannot open %s\n", path);
        return -1;
    }

    char line[1024];
    int lineno = 0, errors = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *key = strtok(line, " \t\r\n=");
        if (!key) continue;
        char *val = strtok(NULL, "\r\n");
        if (!val) {
            fprintf(stderr, "config: %s:%d: %s has no value\n", path, lineno, key);
            errors++;
            continue;
        }
        while (*val == ' ' || *val == '\t' || *val == '=') val++;
        char *end = val + strlen(val);
        while (end > val && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

        double d = strtod(val, NULL);
        if      (!strcmp(key, "vcom"))          c->vcom_mv = parse_vcom(val);
        else if (!strcmp(key, "rotation"))      c->rotation = (int)d;
        else if (!strcmp(key, "margin_left"))   c->margin_left = d;
        else if (!strcmp(key, "margin_right"))  c->margin_right = d;
        else if (!strcmp(key, "margin_top"))    c->margin_top = d;
        else if (!strcmp(key, "margin_bottom")) c->margin_bottom = d;
        else if (!strcmp(key, "line_spacing"))  c->line_spacing = d;
        else if (!strcmp(key, "keep_rows"))     c->keep_rows = (int)d;
        else if (!strcmp(key, "font"))          set_str(c->font, sizeof(c->font), val);
        else if (!strcmp(key, "keyboard"))      set_str(c->keyboard, sizeof(c->keyboard), val);
        else if (!strcmp(key, "layout"))        set_str(c->layout, sizeof(c->layout), val);
        else if (!strcmp(key, "directory"))     set_str(c->directory, sizeof(c->directory), val);
        else {
            fprintf(stderr, "config: %s:%d: unknown key %s\n", path, lineno, key);
            errors++;
        }
    }
    fclose(fp);

    if (c->vcom_mv <= 0) {
        fprintf(stderr, "config: vcom must be set (the value printed on the panel's cable, e.g. -2.14)\n");
        errors++;
    }
    if (c->rotation != 0 && c->rotation != 90 && c->rotation != 180 && c->rotation != 270) {
        fprintf(stderr, "config: rotation must be 0, 90, 180 or 270\n");
        errors++;
    }
    if (c->margin_left < 0 || c->margin_right < 0 || c->margin_top < 0 ||
        c->margin_bottom < 0 || c->line_spacing < 0) {
        fprintf(stderr, "config: margins and line_spacing cannot be negative\n");
        errors++;
    }
    if (c->keep_rows < 0) c->keep_rows = 0;

    if (c->directory[0] == '~' && (c->directory[1] == '\0' || c->directory[1] == '/')) {
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        char expanded[512];
        if (snprintf(expanded, sizeof(expanded), "%s%s", home, c->directory + 1) >= (int)sizeof(expanded)) {
            fprintf(stderr, "config: directory path too long\n");
            errors++;
        }
        memcpy(c->directory, expanded, sizeof(expanded));
    }
    size_t n = strlen(c->directory);
    while (n > 1 && c->directory[n - 1] == '/') c->directory[--n] = '\0';
    return errors ? -1 : 0;
}
