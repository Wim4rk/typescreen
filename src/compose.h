/**
 * @file compose.h
 * @brief Dead-key composition: accent + letter -> accented letter.
 *
 * Covers Latin-1 Supplement and Latin Extended-A. The accent is its
 * spacing form (U+00B4 acute, U+0060 grave, U+005E circumflex, U+00A8
 * diaeresis, U+007E tilde, U+02DA ring, U+02C7 caron, U+00B8 cedilla,
 * U+02DD double acute, U+02D8 breve, U+02DB ogonek, U+00AF macron,
 * U+02D9 dot above, U+002F stroke).
 */
#ifndef COMPOSE_H
#define COMPOSE_H

#include <stdint.h>

/** @return The composed codepoint, or 0 when the pair does not combine. */
uint32_t compose(uint32_t accent, uint32_t letter);

#endif
