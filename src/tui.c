/* tui.c - Terminal I/O using curses (ncurses on Linux, PDCurses on DOS).
 *
 * Thin wrapper: init/cleanup, goto, putstr/putchar, attributes, getkey.
 * 1980s-style: 80x24, bold/reverse, F-keys. WASM stub does nothing.
 */
#include "tui.h"
#include <string.h>

#ifndef PLATFORM_WASM

#include <curses.h>

void tui_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nonl();  /* Enter returns \n for better portability */
    intrflush(stdscr, FALSE);
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

int tui_getkey(void) {
    return -1;
}

#endif /* PLATFORM_WASM */
