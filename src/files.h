/**
 * @file files.h
 * @brief The documents directory: plain files, listed by name.
 */
#ifndef FILES_H
#define FILES_H

#include <stddef.h>

#define FILES_MAX 64
#define FILES_NAME_MAX 256

/** A directory listing. */
typedef struct {
    int count;
    char name[FILES_MAX][FILES_NAME_MAX];   /**< Sorted by name. */
} file_list_t;

/**
 * @brief List the regular files in dir, sorted by name.
 *
 * Names starting with '.' and ending in ".tmp" are skipped. At most
 * FILES_MAX entries are returned.
 * @return 0 on success, -1 if the directory cannot be read.
 */
int files_list(file_list_t *l, const char *dir);

/**
 * @brief The most recently modified file in dir.
 * @param dst  Receives the name, or "" if the directory is empty.
 * @param size Size of dst.
 * @param dir  The directory.
 * @return 0 on success, -1 if the directory cannot be read.
 */
int files_newest(char *dst, size_t size, const char *dir);

/** @brief True if name is acceptable as a document name: non-empty,
 *         no '/', not starting with '.'. */
int files_valid_name(const char *name);

/** @brief Today's date as "YYYY-MM-DD.txt". */
void files_date_name(char *dst, size_t size);

/** @brief Join dir and name. @return 0, or -1 if it does not fit. */
int files_path(char *dst, size_t size, const char *dir, const char *name);

#endif
