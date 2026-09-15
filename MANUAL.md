# typescreen manual

## Hardware

- A Raspberry Pi. Any model; a Pi Zero is enough.
- An e-paper panel with a Waveshare IT8951 driver board (6", 7.8", 9.7",
  10.3" or 13.3"). The board connects to the Pi's SPI header:

  | IT8951 | Pi (BCM) |
  |--------|----------|
  | VCC    | 5V       |
  | GND    | GND      |
  | MISO   | 9        |
  | MOSI   | 10       |
  | SCK    | 11       |
  | CS     | 8        |
  | RST    | 17       |
  | HRDY   | 24       |

  Set the board's switch to SPI mode.
- A USB keyboard.

Enable SPI once: `sudo raspi-config` → Interface Options → SPI.

## Building

```
sudo apt install build-essential libbcm2835-dev
make
```

On a Pi 5 the bcm2835 library does not work; use lgpio instead:

```
sudo apt install liblgpio-dev
make BACKEND=LGPIO
```

`make test` runs the host-side tests (no hardware needed).

## Configuration

Copy `typescreen.conf` and edit it. `typescreen` reads `typescreen.conf`
in the current directory, or the file given as its first argument.

```
sudo ./typescreen                 # uses ./typescreen.conf
sudo ./typescreen my.conf
```

| Key             | Meaning |
|-----------------|---------|
| `vcom`          | The VCOM voltage printed on the panel's flex cable, e.g. `-2.14`. Every panel has its own; a wrong value gives a washed-out or smeared image. |
| `rotation`      | `0`, `90`, `180` or `270` — how many degrees clockwise the page is turned on the panel. If text comes out upside down, add 180. |
| `font`          | Path to a glyph table (see *Fonts*). |
| `margin_left`, `margin_right` | In columns. `3` means three characters' width. |
| `margin_top`, `margin_bottom` | In rows. `1` means one character's height. |
| `line_spacing`  | Extra space between rows as a fraction of the character height. `1` is a blank line between rows, `0.5` half of one. |
| `keep_rows`     | When the cursor runs off the bottom, the page scrolls and this many rows of context remain at the top. |
| `keyboard`      | The evdev device, usually `/dev/input/event0`. See *Troubleshooting* to find yours. |
| `layout`        | Path to a keyboard layout file (see *Keyboard layouts*). |
| `directory`     | Where documents live. `~` is your home directory. |

Margins and spacing are in cells rather than pixels so that the same
configuration works with any font size: change the font and the margins
stay proportional.

The number of columns and rows is worked out at startup from the panel
size (reported by the controller), the font's cell size and the margins.
It is printed on startup:

```
IT8951: panel 1448x1072, firmware ..., VCOM -2.14 V
font: fonts/OldTimeyMono-24x43.bin, 319 glyphs, cell 24x43
canvas: 1448x1072 page, 54 columns x 11 rows, margins 72/72/43/43 px, rotation 0
```

## Fonts

A font is a *glyph table*: a bitmap of every character, rendered once at a
fixed pixel size. The program does no font rendering itself, which keeps
drawing a plain memory copy.

A character is always the same number of pixels, so its physical size
depends on the panel's pixel density:

| Panel  | Pixels      | Pixels/inch | 24x43 table gives | Table for ~3.5 mm text |
|--------|-------------|-------------|-------------------|------------------------|
| 6"     | 1448 x 1072 | 300         | 3.6 mm            | 24x43 (bundled)        |
| 7.8"   | 1872 x 1404 | 300         | 3.6 mm            | 24x43 (bundled)        |
| 9.7"   | 1200 x 825  | 150         | 7.3 mm            | 13x21 (bundled)        |
| 10.3"  | 1872 x 1404 | 227         | 4.8 mm            | 18x32                  |
| 13.3"  | 1600 x 1200 | 150         | 7.3 mm            | 13x21 (bundled)        |

Two tables of Old Timey Mono are bundled: `fonts/OldTimeyMono-24x43.bin`
for the 300 ppi panels and `fonts/OldTimeyMono-13x21.bin` for the 150 ppi
ones. For other panels, or for larger or smaller text, generate a table.

### Generating a glyph table

1. Install the font on your computer. The bundled table was made from
   [Old Timey Mono](https://github.com/dse/old-timey-mono-font) (OFL).
   Any monospace font works; typewriter faces under free licences include
   *Special Elite* (Apache 2.0) and *Courier Prime* (OFL).

2. Open `tools/glyphs.html` in a browser.

3. Enter the font family name exactly as installed.

4. Set the **cell size**. Keep roughly the proportions of the bundled
   tables (width ≈ 0.56 × height, rounded up at small sizes so wide glyphs
   like `æ` are not clipped):

   | Cell     | Font size | Baseline (Y offset) |
   |----------|-----------|---------------------|
   | 13 x 21  | 18        | 17                  |
   | 18 x 32  | 27        | 25                  |
   | 24 x 43  | 36        | 34                  |
   | 32 x 58  | 48        | 46                  |

   Font size ≈ 0.84 × cell height; baseline ≈ 0.79 × cell height. Other
   fonts need other values.

5. Check the preview: the widest glyphs (`W`, `Å`) must fit the cell, the
   descenders (`g`) must not be cut off, and accents must show. Adjust font
   size and offsets until they do.

6. Leave the codepoint ranges at the default (`0x20-0x7E, 0xA0-0xFF,
   0x100-0x17F`: ASCII, Latin-1, Latin Extended-A) unless you need more.
   Anything the font can draw can go in.

7. Output format *Binary table*, click Generate. Rename the download to
   `Name-WxH.bin`, put it in `fonts/`, and point `font =` at it.

The licence of a table is the licence of the font it was rendered from.

## Keyboard layouts

A layout file maps physical keys to characters. One line per key:

```
# key         plain    shift    altgr    altgr+shift
A             a        A
SEMICOLON     ö        Ö        ø        Ø
EQUAL         dead:´   dead:`
SPACE         space
```

- The key is its Linux name without `KEY_` (`A`, `1`, `SEMICOLON`,
  `102ND`, ...) or its number. `src/keyboard.c` lists the names.
- Each of the four columns is a single character, `U+XXXX`, `space` or
  `none`. Missing columns fall back to the nearest one to the left.
- `dead:` before an accent makes it a dead key: it prints the accent, and
  the next letter replaces it with the accented letter. `dead:´` then `e`
  gives `é`. Accent followed by space leaves the accent alone.
- Enter, Backspace, Esc, Shift, Ctrl, Caps Lock, AltGr (right Alt) and
  F1-F12 are fixed and not listed.

`layouts/se.conf` (Swedish) and `layouts/us.conf` (US, with accents on
AltGr) are included. Copy one and edit.

## Using it

Type. What you type appears on the panel and goes into the document.

| Key       | Action |
|-----------|--------|
| Enter     | New line (and save) |
| Backspace | Remove the last character |
| F2        | Save |
| F3        | Open another document, or start a new one |
| F5        | Redraw the whole page slowly and cleanly. Use it when ghosts of old text build up. |
| F10       | Save and quit |

When you pause for a moment, a `_` appears where the next character will
land, as on a typewriter. It disappears with the next key.

### Documents

Documents are plain text files in `directory`. At startup the most
recently written one is opened, with the page showing its end — as if the
sheet of paper is still in the machine. If the directory is empty, a new
document named after today's date (`2026-09-15.txt`) is started.

F3 shows the documents, sorted by name and numbered:

```
Open a document: type its number, or a new name.
Enter to open, Esc to go back.

 1  2026-09-15.txt
 2  chapter-three.txt
 3  notes.txt

> _
```

Type a number and Enter to open that document. Type a name and Enter to
create a new one (or open it, if it exists). Esc or F3 goes back to where
you were. The current document is saved before the list is shown.

Only as many documents as fit on the page are listed. Files whose names
start with a dot are not shown.

The page shows the last rows of the document. When you reach the bottom,
the page clears and the last `keep_rows` rows move to the top so you keep
your context.

Words wrap at spaces and hyphens. A line is never broken mid-word unless
the word is wider than the page.

The document is saved on every Enter, on F2, F3 and F10, and when the
program is stopped with Ctrl-C or `systemctl stop`. Saving writes a temporary file
and renames it, so a power cut during a save cannot leave a half-written
file. Pull the plug mid-line and you lose at most that line.

## Running as a service

To start at boot, create `/etc/systemd/system/typescreen.service`:

```
[Unit]
Description=typescreen
After=local-fs.target

[Service]
WorkingDirectory=/home/pi/typescreen
ExecStart=/home/pi/typescreen/typescreen
Restart=on-failure
KillSignal=SIGINT
TimeoutStopSec=10

[Install]
WantedBy=multi-user.target
```

```
sudo systemctl enable --now typescreen
sudo systemctl stop typescreen      # saves and exits
```

`KillSignal=SIGINT` matters: it lets the program save before exiting.

The bcm2835 backend needs root (it maps `/dev/mem`), so the service runs
as root. Set `directory` to an absolute path in that case — for root, `~`
is `/root`. With `BACKEND=LGPIO` the
program can run as a user in the `gpio`, `spi` and `input` groups.

## Troubleshooting

**Nothing on the panel, or "no valid device info".** Check the wiring and
that the board's switch is on SPI. Check that SPI is enabled
(`ls /dev/spidev*`).

**Text is faint, grey or smeared.** `vcom` is wrong. Use the value on the
panel's cable.

**Text is upside down or sideways.** Change `rotation`.

**Ghosts of old text remain.** Normal for e-paper in fast mode. Press F5.

**"keyboard: cannot open /dev/input/event0".** Find the right device:

```
cat /proc/bus/input/devices
```

Look for your keyboard's name and its `Handlers=... eventN` line; set
`keyboard = /dev/input/eventN`. The number can change if you plug in other
USB devices; `/dev/input/by-id/usb-...-event-kbd` is a stable alternative.

**Wrong characters for some keys.** Your keyboard's physical layout does
not match the layout file. Copy the closest one in `layouts/` and fix the
keys that differ.

**A box instead of a character.** The glyph table has no bitmap for that
codepoint. Generate a table with a wider range.

**Words wrap in the wrong place after backspacing across a line.** This
should not happen; the page is rebuilt from the document whenever the
cursor moves back to a previous line. If it does, report it with the text
that triggered it.

## Writing your own frontend

`main.c` is one way to drive the typewriter: an event loop, four function
keys and the F3 screen. If you want other keys, a status line, a menu or
a different way to choose documents, write your own `main.c` and keep the
rest. `make docs` builds the API reference (Doxygen) in `docs/html/`.

The pieces, bottom up:

| Header         | What it gives you |
|----------------|-------------------|
| `display.h`    | The panel: `display_open`, `display_blit(pixels, x, y, w, h, mode)`, `display_wait`. Panel coordinates, 8-bit pixels. This is also the layer to replace for another controller. |
| `font.h`       | `font_load(path, rotation)`, `font_glyph(cp)`. |
| `canvas.h`     | Layout and drawing in the *user's* orientation: `canvas_draw_cells`, `canvas_draw_rows`, `canvas_draw_page`, and `canvas_draw_pixels` for anything that is not text. |
| `page.h`       | A grid of codepoints with a cursor and word wrap. Make as many as you like — a menu is a page. |
| `doc.h`        | The text, and UTF-8 files. |
| `typewriter.h` | The typing itself: `typewriter_type`, `typewriter_dead`, `typewriter_newline`, `typewriter_backspace`, `typewriter_flush`, `typewriter_layout`, `typewriter_relayout`, and the prompt: `typewriter_show_prompt`, `typewriter_hide_prompt`. |
| `keyboard.h`   | evdev events to key presses, with a layout file. |
| `files.h`, `config.h` | The documents directory and the configuration file, if you want the same ones. |

A minimal frontend:

```c
display_t dev;      display_open(&dev, 2140);
font_t font;        font_load(&font, "fonts/OldTimeyMono-24x43.bin", 0);
canvas_t canvas;    canvas_init(&canvas, &dev, &font, 0, 3, 3, 1, 1, 1);
page_t page;        page_init(&page, canvas.cols, canvas.rows, 3);
doc_t doc;          doc_init(&doc);
typewriter_t tw;    typewriter_init(&tw, &canvas, &page, &doc);

canvas_draw_page(&canvas, &page, DISPLAY_CLEAN);

for (;;) {
    /* wait for a key; on a 900 ms timeout: typewriter_show_prompt(&tw) */
    keypress_t k = /* keyboard_translate() on the next evdev event */;
    switch (k.kind) {
    case KP_CHAR:      typewriter_type(&tw, k.cp, /* more queued? */ false); break;
    case KP_DEAD:      typewriter_dead(&tw, k.cp); break;
    case KP_NEWLINE:   typewriter_newline(&tw); break;
    case KP_BACKSPACE: typewriter_backspace(&tw); break;
    default: break;
    }
}
```

Rules that keep it fast and consistent:

- Call `typewriter_flush` before you draw anything else on the page, and
  before saving on exit. Cells may be held back while keys are queued.
- The prompt is yours to time: call `typewriter_show_prompt` when the
  typist has paused (the reference frontend waits 900 ms). Typing removes
  it by itself; call `typewriter_hide_prompt` before drawing elsewhere.
- If you change the document behind the typewriter's back (open a file,
  delete a paragraph), call `typewriter_layout` and draw the page.
- To show a screen of your own, draw it into a `page_t` with `page_put`
  and `canvas_draw_page`, or draw pixels with `canvas_draw_pixels`. When
  you return to the text, call `typewriter_layout` and draw the page
  again with `DISPLAY_CLEAN`.
- Fast refreshes of different areas may overlap; call `display_wait`
  before a `DISPLAY_CLEAN` or `DISPLAY_INIT` refresh.
- The typewriter owns nothing. You allocate the canvas, page and
  document, and you may swap the document for another one.

For another e-paper controller, implement `display.h` in a new file and
build with it instead of `it8951.c` and `hal.c`. Nothing above it needs
to change.
