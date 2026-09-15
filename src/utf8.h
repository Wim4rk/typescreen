/**
 * @file utf8.h
 * @brief UTF-8 encoding and decoding.
 */
#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>
#include <stddef.h>

/** @brief Decode one codepoint and advance *s. Invalid bytes give U+FFFD. */
uint32_t utf8_next(const char **s);

/** @brief Encode cp into dst (at least 4 bytes). @return The number of bytes. */
int utf8_put(char *dst, uint32_t cp);

/** @brief Encode n codepoints as a NUL-terminated string.
 *  @return The length, or -1 if it does not fit. */
int utf8_encode(char *dst, size_t size, const uint32_t *cp, size_t n);

#endif
