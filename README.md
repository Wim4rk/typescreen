# typescreen

A typewriter solution for e-ink screens.

Keystrokes in, type on the page. Nothing else. No editing in the middle of
the text, no menus. You write forward, and what you wrote is what gets saved.

**Status:** first implementation. Builds and passes host-side tests; not yet
run on a panel.

## Priorities, in order

1. **Screen updates must be fast.** A keystroke should appear as quickly as
   the panel can physically show it. Everything in the design serves this:
   glyphs are stored in the controller's native pixel format so drawing is
   a copy, rotation is applied once at load time and never per keystroke,
   only the cells that changed are sent, and the draw path allocates
   nothing. When a choice trades speed for anything else, speed wins.

2. **Simple to use.** One config file, one glyph table, one keyboard layout.
   No build-time defines to keep in agreement.

3. **As many Latin-script characters as come easily.** Latin-1 and Latin
   Extended-A by default — Western and Central Europe. More ranges are a
   matter of typing them into the generator, not of changing code.

## Quick start

```
sudo apt install libbcm2835-dev
make
sudo ./typescreen
```

Edit `typescreen.conf` first: set `vcom` to the value on your panel's cable
and `layout` to your keyboard. Everything else is in [MANUAL.md](MANUAL.md).

## How it works

```
display.h       the panel: a frame buffer and a way to refresh part of it
  it8951.c      ... implemented for the IT8951 controller
  hal.c         ... over bcm2835 or lgpio, chosen at build time
font.c          a glyph table: codepoint → bitmap, pre-rendered AND
                pre-rotated at load time, in the controller's pixel format
canvas.c        a page in the USER's orientation, mapped to panel
                coordinates. Partial updates for typing, full refresh
                for ghosting.
page.c          a grid of codepoints with word wrap and scrolling
doc.c           everything typed, saved as UTF-8
typewriter.c    keystrokes in, cells out: batching, dead keys, backspace
keyboard.c      evdev keyboard with a loadable layout file
compose.c       accent + letter → accented letter
files.c         the documents directory
config.c        the configuration file
main.c          the reference frontend: event loop, keys, the F3 screen
```

`typewriter.c` and everything below it is the library; `main.c` is one
way to drive it. A different interface — other keys, a status line, a
menu — is a different `main.c`. See *Writing your own frontend* in the
manual, and `make docs` for the API reference.

The controller code is a trimmed rewrite of Waveshare's IT8951 driver,
keeping only what a typewriter needs.

**Any IT8951 panel.** The controller reports its resolution at init, so
panel size is never configured. Columns and rows follow from the panel,
the font's cell size and the margins. Margins are given in cells, so one
configuration works with any font size.

**Fonts are bitmap tables**, generated ahead of time in a browser with
`tools/glyphs.html` from any installed font. A character is a fixed number
of pixels; for panels of a different pixel density you generate a table of
another size. [Old Timey Mono](https://github.com/dse/old-timey-mono-font)
is bundled at 24x43 and 13x21, with gratitude to its designer, Darren Embry.

**Rotation is done in software, at load time — not by the controller.**
The IT8951's own `Rotate` field transforms coordinates on every partial
write, in the hot path, and is unreliable with partial-area loads. Instead
glyphs are rotated once when the table is loaded, and placing one is a
plain copy to a precomputed address. This is settled: the controller's
rotation was tried and abandoned.

**Fast typing is batched.** When keys arrive faster than the panel draws,
cells on the current row are collected and sent as one update — at a
space, after a few characters, or when the queue catches up.

**This is a typewriter, not a reader.** Typographic characters — curly
quotes, dashes, ellipses — are not a goal. Write straight quotes, `--` and `...`; a
document tool downstream can turn them into typography.

**Documents are files in one directory.** The most recently written one
is open when the machine starts; F3 lists the others and lets you start a
new one. That is the whole file system.

Everything that makes a full editor — cursor movement, editing in the
middle, version control, statistics — is deliberately out of scope.

## License

This project's source code is licensed under the [MIT License](LICENSE.md).
`src/hal.c` and `src/it8951.c` are derived from Waveshare's IT8951 driver,
also MIT; its notice is included in LICENSE.md.

The bundled font is licensed under the [SIL Open Font License 1.1](FONT_LICENSE.md).
A generated glyph table is a derivative of the font it was rendered from
and inherits that font's licence.
