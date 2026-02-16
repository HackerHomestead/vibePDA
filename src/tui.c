/* tui.c - Terminal I/O using curses (ncurses on Linux, PDCurses on DOS).
 *
 * Thin wrapper: init/cleanup, goto, putstr/putchar, attributes, getkey.
 * 1980s-style: 80x24, bold/reverse, F-keys. Color scheme matches README screenshots.
 * WASM stub does nothing.
 */
#include "tui.h"
#include <string.h>

#ifndef PLATFORM_WASM

/* Use ncursesw for proper UTF-8 multi-byte support (box-drawing, etc.).
 * Regular ncurses treats each byte as a char, causing mojibake. */
#include <ncursesw/curses.h>

/* Color pairs (matches scripts/gen_screenshots.py palette) */
#define PAIR_TITLE_BAR        1  /* white on blue */
#define PAIR_MENU_BAR         2  /* yellow on black */
#define PAIR_SIDEBAR_SELECTED 3  /* white on blue */
#define PAIR_CARD_TITLE       4  /* cyan on black */
#define PAIR_STATUS_BAR       5  /* white on black (muted bar) */
#define PAIR_CONTENT_SELECTED 6  /* white on blue */

static int tui_has_colors;

void tui_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nonl();  /* Enter returns \n for better portability */
    intrflush(stdscr, FALSE);

    tui_has_colors = 0;
    if (has_colors()) {
        start_color();
        init_pair(PAIR_TITLE_BAR, COLOR_WHITE, COLOR_BLUE);
        init_pair(PAIR_MENU_BAR, COLOR_YELLOW, COLOR_BLACK);
        init_pair(PAIR_SIDEBAR_SELECTED, COLOR_WHITE, COLOR_BLUE);
        init_pair(PAIR_CARD_TITLE, COLOR_CYAN, COLOR_BLACK);
        init_pair(PAIR_STATUS_BAR, COLOR_WHITE, COLOR_BLACK);
        init_pair(PAIR_CONTENT_SELECTED, COLOR_WHITE, COLOR_BLUE);
        tui_has_colors = 1;
    }
}

int tui_rows(void) {
    return LINES;
}

int tui_cols(void) {
    return COLS;
}

void tui_cleanup(void) {
    attrset(A_NORMAL);
    endwin();
}

void tui_clear(void) {
    erase();
}

void tui_goto(int row, int col) {
    move(row, col);
}

void tui_putstr(const char *s) {
    if (!s) return;
    addstr(s);
}

void tui_putchar(char c) {
    addch((chtype)(unsigned char)c);
}

void tui_refresh(void) {
    refresh();
}

void tui_attr_normal(void) {
    attrset(A_NORMAL);
}

void tui_attr_bold(void) {
    attron(A_BOLD);
}

void tui_attr_reverse(void) {
    attron(A_REVERSE);
}

/* Color scheme (matches README screenshots). Fallback to bold/reverse when no colors. */
void tui_attr_title_bar(void) {
    if (tui_has_colors)
        attrset(COLOR_PAIR(PAIR_TITLE_BAR));
    else
        attron(A_REVERSE);
}

void tui_attr_menu_bar(void) {
    if (tui_has_colors)
        attrset(COLOR_PAIR(PAIR_MENU_BAR));
    else
        attron(A_BOLD);
}

void tui_attr_sidebar_selected(void) {
    if (tui_has_colors)
        attrset(COLOR_PAIR(PAIR_SIDEBAR_SELECTED));
    else
        attron(A_REVERSE);
}

void tui_attr_card_title(void) {
    if (tui_has_colors)
        attrset(COLOR_PAIR(PAIR_CARD_TITLE));
    else
        attron(A_BOLD);
}

void tui_attr_status_bar(void) {
    if (tui_has_colors)
        attrset(COLOR_PAIR(PAIR_STATUS_BAR));
    else
        attron(A_REVERSE);
}

void tui_attr_content_selected(void) {
    if (tui_has_colors)
        attrset(COLOR_PAIR(PAIR_CONTENT_SELECTED));
    else
        attron(A_REVERSE);
}

void tui_attr_dim(void) {
    attron(A_DIM);
}

int tui_getkey(void) {
    int c = getch();
    if (c == ERR) return -1;
    return c;
}

#else /* PLATFORM_WASM */

int tui_rows(void) { return TUI_ROWS; }
int tui_cols(void) { return TUI_COLS; }

void tui_init(void) {
    (void)0;
}

void tui_cleanup(void) {
    (void)0;
}

void tui_clear(void) {
    (void)0;
}

void tui_goto(int row, int col) {
    (void)row;
    (void)col;
}

void tui_putstr(const char *s) {
    (void)s;
}

void tui_putchar(char c) {
    (void)c;
}

void tui_refresh(void) {
    (void)0;
}

void tui_attr_normal(void) {
    (void)0;
}

void tui_attr_bold(void) {
    (void)0;
}

void tui_attr_reverse(void) {
    (void)0;
}

void tui_attr_title_bar(void) { (void)0; }
void tui_attr_menu_bar(void) { (void)0; }
void tui_attr_sidebar_selected(void) { (void)0; }
void tui_attr_card_title(void) { (void)0; }
void tui_attr_status_bar(void) { (void)0; }
void tui_attr_content_selected(void) { (void)0; }
void tui_attr_dim(void) { (void)0; }

int tui_getkey(void) {
    return -1;
}

#endif /* PLATFORM_WASM */
