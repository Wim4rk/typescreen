/**
 * @file doc.h
 * @brief The document: everything typed, as codepoints. Files are UTF-8.
 */
#ifndef DOC_H
#define DOC_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t *cp;
    size_t len, cap;
} doc_t;

void doc_init(doc_t *d);
void doc_free(doc_t *d);

/** @return 0, or -1 on allocation failure. */
int  doc_push(doc_t *d, uint32_t cp);
void doc_pop(doc_t *d);

/**
 * @brief Replace the contents with a file's. Invalid UTF-8 becomes
 *        U+FFFD; carriage returns are dropped.
 * @return 0 on success (a missing file gives an empty document), -1 on
 *         a read error.
 */
int doc_load(doc_t *d, const char *path);

/**
 * @brief Write the document as UTF-8 with a final newline.
 *
 * Writes to path.tmp, syncs, then renames over path, so a power cut
 * never leaves a half-written file.
 * @return 0, or -1 with errno set.
 */
int doc_save(const doc_t *d, const char *path);

#endif
