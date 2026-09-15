#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "page.h"
#include "font.h"
#include "canvas.h"
#include "compose.h"
#include "doc.h"
#include "keyboard.h"
#include "files.h"
#include "utf8.h"
#include "config.h"
#include "typewriter.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>

/* Host-side tests: no panel, no keyboard. Run with `make test`. */

/* stub controller: record blits */
static int last_x, last_y, last_w, last_h, last_mode, blits;
static uint8_t lastbuf[1448*1072];
void display_blit(const display_t *d, const uint8_t *px, int x, int y, int w, int h, display_mode mode) {
    (void)d; last_x=x; last_y=y; last_w=w; last_h=h; last_mode=mode; blits++;
    memcpy(lastbuf, px, (size_t)w*h);
}
void display_wait(void) {}

static void type(page_t *p, const char *s) { for (; *s; s++) page_put(p, (uint32_t)*s); }
static void dump(page_t *p) {
    for (int r = 0; r < p->rows; r++) { printf("|"); for (int c = 0; c < p->cols; c++) { uint32_t v = page_cell(p,r,c); putchar(v ? (char)v : '.'); } printf("|\n"); }
    printf("cursor %d,%d\n", p->cur_row, p->cur_col);
}

static void test_files_utf8_config(void) {
    /* utf8 round trip */
    const char *txt = "aé€😀";
    const char *p = txt;
    uint32_t cps[4];
    for (int i = 0; i < 4; i++) cps[i] = utf8_next(&p);
    assert(cps[0]=='a' && cps[1]==0xE9 && cps[2]==0x20AC && cps[3]==0x1F600 && *p == 0);
    char back[32];
    assert(utf8_encode(back, sizeof(back), cps, 4) == (int)strlen(txt) && strcmp(back, txt) == 0);
    p = "\xff\x41"; assert(utf8_next(&p) == 0xFFFD && utf8_next(&p) == 'A');

    /* files: by name, dotfiles and .tmp skipped; newest by mtime */
    system("rm -rf tests/dir && mkdir -p tests/dir tests/dir/sub && touch tests/dir/.hidden tests/dir/x.txt.tmp");
    FILE *f;
    f = fopen("tests/dir/old.txt", "w"); fputs("o", f); fclose(f);
    f = fopen("tests/dir/new.txt", "w"); fputs("n", f); fclose(f);
    struct utimbuf ut = { 1000000, 1000000 }; utime("tests/dir/old.txt", &ut);
    ut.modtime = ut.actime = 2000000; utime("tests/dir/new.txt", &ut);
    file_list_t l;
    assert(files_list(&l, "tests/dir") == 0);
    assert(l.count == 2 && strcmp(l.name[0], "new.txt") == 0 && strcmp(l.name[1], "old.txt") == 0);
    char newest[FILES_NAME_MAX];
    assert(files_newest(newest, sizeof(newest), "tests/dir") == 0 && strcmp(newest, "new.txt") == 0);
    assert(files_newest(newest, sizeof(newest), "tests/dir/sub") == 0 && newest[0] == 0);
    assert(files_newest(newest, sizeof(newest), "tests/nope") != 0);
    assert(files_valid_name("brev.txt") && !files_valid_name("") && !files_valid_name(".x") && !files_valid_name("a/b"));
    char dn[32]; files_date_name(dn, sizeof(dn)); assert(strlen(dn) == 14 && dn[4] == '-');
    system("rm -rf tests/dir");

    /* config: ~ expansion, directory key, vcom in volts */
    f = fopen("tests/c.conf", "w"); fputs("vcom=-2.14\ndirectory = ~/docs/\n", f); fclose(f);
    config_t c; config_defaults(&c);
    assert(config_load(&c, "tests/c.conf") == 0);
    char want[600]; snprintf(want, sizeof(want), "%s/docs", getenv("HOME"));
    assert(strcmp(c.directory, want) == 0 && c.vcom_mv == 2140 && c.line_spacing == 1);
    unlink("tests/c.conf");
}

static void test_typewriter(void) {
    display_t dev = { 1448, 1072, 0 };
    font_t f;
    assert(font_load(&f, "fonts/OldTimeyMono-24x43.bin", 0) == 0);
    canvas_t c;
    assert(canvas_init(&c, &dev, &f, 0, 3, 3, 1, 1, 1) == 0);   /* 54 cols */
    page_t p; page_init(&p, c.cols, c.rows, 3);
    doc_t d; doc_init(&d);
    typewriter_t tw; typewriter_init(&tw, &c, &p, &d);

    /* a burst: nothing sent until flush */
    int before = blits;
    typewriter_type(&tw, 'a', true);
    typewriter_type(&tw, 'b', true);
    assert(blits == before && tw.span_row == 0 && tw.span_col0 == 0 && tw.span_col1 == 1);
    typewriter_type(&tw, 'c', false);
    assert(blits == before + 1 && last_w == 3 * 24 && tw.span_row == -1);

    /* dead key: accent shown, then replaced */
    typewriter_dead(&tw, 0xB4);
    assert(page_cell(&p, 0, 3) == 0xB4 && d.len == 4);
    typewriter_type(&tw, 'e', false);
    assert(page_cell(&p, 0, 3) == 0xE9 && d.len == 4 && d.cp[3] == 0xE9 && p.cur_col == 4);
    /* accent + space: accent stays */
    typewriter_dead(&tw, 0x60);
    typewriter_type(&tw, ' ', false);
    assert(page_cell(&p, 0, 4) == 0x60 && d.len == 5 && p.cur_col == 5);

    /* backspace across a line boundary rebuilds from the document */
    typewriter_newline(&tw);
    assert(p.cur_row == 1 && p.cur_col == 0 && d.cp[5] == '\n');
    before = blits;
    typewriter_backspace(&tw);
    assert(d.len == 5 && p.cur_row == 0 && p.cur_col == 5);
    assert(blits == before);   /* the page did not change visibly */
    typewriter_backspace(&tw);
    assert(d.len == 4 && p.cur_col == 4 && page_cell(&p, 0, 4) == 0 && blits == before + 1);

    /* prompt: shown in the cursor cell after a pause, cell stays empty */
    doc_free(&d); doc_init(&d); typewriter_layout(&tw);
    typewriter_type(&tw, 'q', false);
    before = blits;
    typewriter_show_prompt(&tw);
    assert(blits == before + 1 && tw.prompt_visible && tw.prompt_col == 1);
    assert(page_cell(&p, 0, 1) == 0);
    assert(memcmp(lastbuf, font_glyph(&f, '_'), 24 * 43) == 0);
    typewriter_show_prompt(&tw);
    assert(blits == before + 1);   /* already shown */
    typewriter_type(&tw, 'r', false);   /* erased, then the character drawn */
    assert(blits == before + 3 && !tw.prompt_visible && page_cell(&p, 0, 1) == 'r');
    typewriter_hide_prompt(&tw);
    assert(blits == before + 3);   /* nothing to hide */

    /* wrap then backspace the moved word back up */
    doc_free(&d); doc_init(&d); typewriter_layout(&tw);
    for (int i = 0; i < 50; i++) typewriter_type(&tw, 'x', false);
    typewriter_type(&tw, ' ', false);
    for (int i = 0; i < 5; i++) typewriter_type(&tw, 'y', false);   /* wraps: yyyy y -> row 1 */
    assert(p.cur_row == 1 && p.cur_col == 5 && page_cell(&p, 1, 0) == 'y');
    for (int i = 0; i < 6; i++) typewriter_backspace(&tw);
    assert(p.cur_row == 0 && p.cur_col == 50 && d.len == 50 && page_cell(&p, 1, 0) == 0);

    doc_free(&d); page_free(&p); canvas_free(&c); font_free(&f);
}

static void test_doc_and_keyboard(void) {
    doc_t d; doc_init(&d);
    const char *s = "Hej världen! œ Ÿ ăĕ\nrad två";
    for (const char *p = s; *p; ) { int n; unsigned char c=*p; int extra = c<0x80?0:c<0xE0?1:c<0xF0?2:3; uint32_t cp = extra==0?c:extra==1?(c&0x1F):extra==2?(c&0x0F):(c&7); p++; for(n=0;n<extra;n++,p++) cp=(cp<<6)|((unsigned char)*p&0x3F); doc_push(&d,cp);} 
    assert(doc_save(&d, "tests/doc.txt") == 0);
    doc_t e; doc_init(&e); assert(doc_load(&e, "tests/doc.txt") == 0);
    assert(e.len == d.len + 1 && e.cp[e.len-1] == '\n');
    assert(memcmp(e.cp, d.cp, d.len*4) == 0);
    FILE *f = fopen("tests/doc.txt","rb"); char buf[200]; size_t n = fread(buf,1,199,f); buf[n]=0; fclose(f);
    assert(strcmp(buf, "Hej världen! œ Ÿ ăĕ\nrad två\n") == 0);
    doc_t m; doc_init(&m); assert(doc_load(&m, "tests/nope.txt") == 0 && m.len == 0);

    layout_t l; assert(keyboard_load_layout(&l, "layouts/se.conf") == 0);
    assert(l.cp[KEY_A][0]=='a' && l.cp[KEY_A][1]=='A' && l.cp[KEY_LEFTBRACE][0]==0xE5 && l.cp[KEY_LEFTBRACE][1]==0xC5);
    assert(l.cp[KEY_EQUAL][0]==0xB4 && l.dead[KEY_EQUAL][0] && l.cp[KEY_EQUAL][1]==0x60 && l.dead[KEY_EQUAL][1]);
    assert(l.cp[KEY_SEMICOLON][2]==0xF8 && l.cp[KEY_SEMICOLON][3]==0xD8 && l.cp[KEY_E][2]==0x20AC);
    assert(l.cp[KEY_SPACE][0]==' ');
    assert(keyboard_load_layout(&l, "layouts/us.conf") == 0);
    assert(l.cp[KEY_APOSTROPHE][2]==0xB4 && l.dead[KEY_APOSTROPHE][2] && l.cp[KEY_COMMA][2]==0xE7);

    /* translate: shift, caps, altgr fallback */
    struct input_event ev; memset(&ev,0,sizeof ev); ev.type = EV_KEY;
    keypress_t k;
    ev.code = KEY_A; ev.value = 1; k = keyboard_translate(&l,&ev); assert(k.kind==KP_CHAR && k.cp=='a');
    ev.code = KEY_LEFTSHIFT; ev.value=1; keyboard_translate(&l,&ev);
    ev.code = KEY_A; k = keyboard_translate(&l,&ev); assert(k.cp=='A');
    ev.code = KEY_LEFTSHIFT; ev.value=0; keyboard_translate(&l,&ev);
    ev.code = KEY_CAPSLOCK; ev.value=1; keyboard_translate(&l,&ev);
    ev.code = KEY_A; k = keyboard_translate(&l,&ev); assert(k.cp=='A');
    ev.code = KEY_1; k = keyboard_translate(&l,&ev); assert(k.cp=='1');   /* caps does not touch digits */
    ev.code = KEY_CAPSLOCK; keyboard_translate(&l,&ev);
    ev.code = KEY_RIGHTALT; ev.value=1; keyboard_translate(&l,&ev);
    ev.code = KEY_B; k = keyboard_translate(&l,&ev); assert(k.cp=='b');   /* no altgr level: falls back */
    ev.code = KEY_APOSTROPHE; k = keyboard_translate(&l,&ev); assert(k.kind==KP_DEAD && k.cp==0xB4);
    ev.code = KEY_RIGHTALT; ev.value=0; keyboard_translate(&l,&ev);
    ev.code = KEY_A; ev.value=0; k = keyboard_translate(&l,&ev); assert(k.kind==KP_NONE);
    ev.code = KEY_F2; ev.value=1; k = keyboard_translate(&l,&ev); assert(k.kind==KP_FUNCTION && k.fn==2);
    ev.code = KEY_F12; k = keyboard_translate(&l,&ev); assert(k.fn==12);
    ev.code = KEY_ENTER; k = keyboard_translate(&l,&ev); assert(k.kind==KP_NEWLINE);
    ev.code = KEY_BACKSPACE; k = keyboard_translate(&l,&ev); assert(k.kind==KP_BACKSPACE);
    puts("doc + keyboard tests passed");
    }

int main(void) {
    /* --- page --- */
    page_t p; page_init(&p, 10, 4, 1);
    type(&p, "hello world foo");
    dump(&p);
    assert(p.cur_row == 1 && p.cur_col == 9);
    assert(page_cell(&p,0,0)=='h' && page_cell(&p,0,5)==' ' && page_cell(&p,0,6)==0 && page_cell(&p,1,0)=='w');
    /* after wrap the space is swallowed if at column 0 */
    page_clear(&p);
    type(&p, "abcdefghij klm");   /* exactly fills the row: hard break */
    dump(&p);
    assert(p.cur_row == 1 && p.cur_col == 3 && page_cell(&p,1,0)=='k');
    /* scroll */
    page_clear(&p);
    type(&p, "a\nb\nc\nd\ne");
    dump(&p);
    assert(p.cur_row == 1 && page_cell(&p,0,0)=='d' && page_cell(&p,1,0)=='e');
    /* wrap into scroll carries the word */
    page_clear(&p);
    type(&p, "1\n2\n3\nabc defghij");
    dump(&p);
    assert(page_cell(&p,0,0)=='a' && page_cell(&p,1,0)=='d' && p.cur_row==1 && p.cur_col==7);
    /* backspace */
    page_change_t ch = page_backspace(&p);
    assert(ch.kind == PAGE_CELL && p.cur_col == 6 && page_cell(&p,1,6)==0);
    page_free(&p);

    /* --- compose --- */
    assert(compose(0xB4, 'e') == 0xE9);
    assert(compose(0xA8, 'A') == 0xC4);
    assert(compose(0x2C7, 'c') == 0x10D);
    assert(compose(0xB4, 'x') == 0);
    assert(compose(0x2DA, 'a') == 0xE5);

    /* --- font, all rotations --- */
    font_t f0, f90, f180, f270;
    assert(font_load(&f0, "fonts/OldTimeyMono-24x43.bin", 0) == 0);
    assert(font_load(&f90, "fonts/OldTimeyMono-24x43.bin", 90) == 0);
    assert(font_load(&f180, "fonts/OldTimeyMono-24x43.bin", 180) == 0);
    assert(font_load(&f270, "fonts/OldTimeyMono-24x43.bin", 270) == 0);
    assert(f0.glyph_w == 24 && f90.glyph_w == 43 && f90.glyph_h == 24);
    const uint8_t *a0 = font_glyph(&f0, 'A'), *a180 = font_glyph(&f180, 'A');
    const uint8_t *a90 = font_glyph(&f90, 'A'), *a270 = font_glyph(&f270, 'A');
    int n = 24*43, black = 0;
    for (int i = 0; i < n; i++) { assert(a0[i] == a180[n-1-i]); if (a0[i]==0) black++; }
    assert(black > 50);
    /* 90: (x,y) -> (h-1-y, x) in a 43-wide bitmap; 270: (y, w-1-x) */
    for (int y = 0; y < 43; y++) for (int x = 0; x < 24; x++) {
        assert(a90[x*43 + (43-1-y)] == a0[y*24+x]);
        assert(a270[(24-1-x)*43 + y] == a0[y*24+x]);
    }
    assert(font_glyph(&f0, ' ') == NULL);
    assert(font_glyph(&f0, 0x20AC) == f0.missing);   /* euro not in table */
    assert(font_glyph(&f0, 0x17F) != f0.missing && font_glyph(&f0, 0x100) != f0.missing);
    assert(font_glyph(&f0, 0xE5) != f0.missing);

    /* --- canvas, rotation 180 must reproduce the original geometry --- */
    display_t dev = { 1448, 1072, 0 };
    canvas_t c;
    assert(canvas_init(&c, &dev, &f180, 180, 68/24.0, 68/24.0, 44/43.0, 68/43.0, 21/43.0) == 0);
    assert(c.cols == 54 && c.rows == 15);
    page_init(&p, c.cols, c.rows, 3);
    type(&p, "Ab");
    canvas_draw_cells(&c, &p, 0, 1, 1);   /* cell (0,1) */
    printf("blit %d,%d %dx%d\n", last_x, last_y, last_w, last_h);
    assert(last_x == 1448 - 68 - 2*24 && last_y == 1072 - 44 - 64 + 21 && last_w == 24 && last_h == 43);
    /* the buffer must equal the rotated glyph for 'b' */
    assert(memcmp(lastbuf, font_glyph(&f180, 'b'), 24*43) == 0);
    canvas_draw_rows(&c, &p, 0, 1);
    printf("rows blit %d,%d %dx%d\n", last_x, last_y, last_w, last_h);
    assert(last_x == 0 && last_w == 1448 && last_h == 128 && last_y == 1072 - 44 - 128);
    canvas_draw_page(&c, &p, DISPLAY_CLEAN);
    assert(last_x == 0 && last_y == 0 && last_w == 1448 && last_h == 1072 && last_mode == DISPLAY_CLEAN);
    /* glyph 'A' lands at cell (0,0): panel x = 1448-68-24 .. , y = 1072-44-64+21 */
    {
        int px = 1448-68-24, py = 1072-44-64+21;
        const uint8_t *g = font_glyph(&f180, 'A');
        for (int y = 0; y < 43; y++) assert(memcmp(lastbuf + (py+y)*1448 + px, g + y*24, 24) == 0);
    }
    canvas_free(&c);

    /* rotation 90: page is 1072 wide, 1448 high */
    assert(canvas_init(&c, &dev, &f90, 90, 2.5, 2.5, 1, 1, 0.5) == 0);
    printf("rot90: %d cols x %d rows\n", c.cols, c.rows);
    assert(c.page_w == 1072 && c.cols == (1072-120)/24 && c.rows == (1448-86)/65);
    page_free(&p); page_init(&p, c.cols, c.rows, 3);
    type(&p, "A");
    canvas_draw_cells(&c, &p, 0, 0, 0);
    /* user rect (60,43,24,43) -> panel x = 1448-(43+43)=1362, y = 60, w=43, h=24 */
    printf("rot90 blit %d,%d %dx%d\n", last_x, last_y, last_w, last_h);
    assert(last_x == 1362 && last_y == 60 && last_w == 43 && last_h == 24);
    assert(memcmp(lastbuf, font_glyph(&f90, 'A'), 24*43) == 0);
    canvas_free(&c);

    /* the small table on a 13.3" panel, same configuration */
    font_t small;
    assert(font_load(&small, "fonts/OldTimeyMono-13x21.bin", 0) == 0);
    assert(small.cell_w == 13 && small.cell_h == 21 && small.count == 319);
    assert(font_glyph(&small, 0x153) != small.missing);
    display_t big = { 1600, 1200, 0 };
    canvas_free(&c);
    assert(canvas_init(&c, &big, &small, 0, 3, 3, 1, 1, 0.5) == 0);
    printf("13.3\" with 13x21: %d cols x %d rows\n", c.cols, c.rows);
    assert(c.margin_left == 39 && c.margin_top == 21 && c.row_h == 21 + 11);
    assert(c.cols == (1600 - 78) / 13 && c.rows == (1200 - 42) / 32);
    font_free(&small);
    canvas_free(&c);

    /* rotation 0 */
    assert(canvas_init(&c, &dev, &f0, 0, 68/24.0, 68/24.0, 44/43.0, 68/43.0, 21/43.0) == 0);
    page_free(&p); page_init(&p, c.cols, c.rows, 3);
    type(&p, "AB");
    canvas_draw_cells(&c, &p, 0, 0, 1);
    assert(last_x == 68 && last_y == 44 && last_w == 48 && last_h == 43);
    for (int y = 0; y < 43; y++) {
        assert(memcmp(lastbuf + y*48, font_glyph(&f0,'A') + y*24, 24) == 0);
        assert(memcmp(lastbuf + y*48 + 24, font_glyph(&f0,'B') + y*24, 24) == 0);
    }
    test_doc_and_keyboard();
    test_files_utf8_config();
    test_typewriter();
    printf("all tests passed (%d blits)\n", blits);
    return 0;
}
