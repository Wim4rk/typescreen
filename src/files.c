#include "files.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    char name[FILES_NAME_MAX];
    time_t mtime;
} entry_t;

static int by_name(const void *a, const void *b) {
    return strcmp(((const entry_t *)a)->name, ((const entry_t *)b)->name);
}

/* Regular files, dotfiles and .tmp excluded. Returns the count. */
static int scan(entry_t *entries, int max, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return -1;

    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        size_t len = strlen(e->d_name);
        if (e->d_name[0] == '.' || len >= FILES_NAME_MAX) continue;
        if (len > 4 && strcmp(e->d_name + len - 4, ".tmp") == 0) continue;

        char path[1024];
        struct stat st;
        if (files_path(path, sizeof(path), dir, e->d_name) != 0) continue;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        strcpy(entries[n].name, e->d_name);
        entries[n].mtime = st.st_mtime;
        n++;
    }
    closedir(d);
    return n;
}

static entry_t entries[1024];

int files_list(file_list_t *l, const char *dir) {
    l->count = 0;
    int n = scan(entries, 1024, dir);
    if (n < 0) return -1;

    qsort(entries, (size_t)n, sizeof(entry_t), by_name);
    for (int i = 0; i < n && i < FILES_MAX; i++) {
        strcpy(l->name[i], entries[i].name);
        l->count++;
    }
    return 0;
}

int files_newest(char *dst, size_t size, const char *dir) {
    dst[0] = '\0';
    int n = scan(entries, 1024, dir);
    if (n < 0) return -1;

    int best = -1;
    for (int i = 0; i < n; i++) {
        if (best < 0 || entries[i].mtime > entries[best].mtime) best = i;
    }
    if (best >= 0) snprintf(dst, size, "%s", entries[best].name);
    return 0;
}

int files_valid_name(const char *name) {
    return name[0] != '\0' && name[0] != '.' && strchr(name, '/') == NULL &&
           strlen(name) < FILES_NAME_MAX;
}

void files_date_name(char *dst, size_t size) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(dst, size, "%Y-%m-%d.txt", &tm);
}

int files_path(char *dst, size_t size, const char *dir, const char *name) {
    return snprintf(dst, size, "%s/%s", dir, name) < (int)size ? 0 : -1;
}
