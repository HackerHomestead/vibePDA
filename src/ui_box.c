/* ui_box.c - Box-drawing primitives (Unicode or ASCII). */

#include "ui_box.h"
#include "tui.h"
#include <ctype.h>
#include <stdlib.h>

static int use_ascii_box = -1;

static void init_box_style(void) {
    if (use_ascii_box >= 0) return;
    const char *e = getenv("VIBE_ASCII_BOX");
    use_ascii_box = (e && (e[0] == '1' || (e[0] != '0' && tolower((unsigned char)e[0]) == 'y'))) ? 1 : 0;
}

void ui_box_top(int row, int col, int w) {
    init_box_style();
    tui_goto(row, col);
    if (use_ascii_box) {
        tui_putchar('+');
        for (int i = 0; i < w; i++) tui_putchar('-');
        tui_putchar('+');
    } else {
        tui_putstr("\xe2\x94\x8c");
        for (int i = 0; i < w; i++) tui_putstr("\xe2\x94\x80");
        tui_putstr("\xe2\x94\x90");
    }
}

void ui_box_sep(int row, int col, int w) {
    init_box_style();
    tui_goto(row, col);
    if (use_ascii_box) {
        tui_putchar('|');
        for (int i = 0; i < w; i++) tui_putchar('-');
        tui_putchar('|');
    } else {
        tui_putstr("\xe2\x94\x9c");
        for (int i = 0; i < w; i++) tui_putstr("\xe2\x94\x80");
        tui_putstr("\xe2\x94\xa4");
    }
}

void ui_box_bottom(int row, int col, int w) {
    init_box_style();
    tui_goto(row, col);
    if (use_ascii_box) {
        tui_putchar('+');
        for (int i = 0; i < w; i++) tui_putchar('-');
        tui_putchar('+');
    } else {
        tui_putstr("\xe2\x94\x94");
        for (int i = 0; i < w; i++) tui_putstr("\xe2\x94\x80");
        tui_putstr("\xe2\x94\x98");
    }
}

void ui_box_v(void) {
    init_box_style();
    if (use_ascii_box)
        tui_putchar('|');
    else
        tui_putstr("\xe2\x94\x82");
}
