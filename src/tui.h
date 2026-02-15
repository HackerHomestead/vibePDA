/* tui.h - Terminal I/O for vibePDA. Uses curses (ncurses/PDCurses) for 1980s-style TUI. */

#ifndef TUI_H
#define TUI_H

#define TUI_COLS 80
#define TUI_ROWS 24
#ifdef PLATFORM_DOS
# undef TUI_ROWS
# define TUI_ROWS 25
#endif

#ifdef PLATFORM_WASM
/* Stub key codes for WASM (no curses) */
# define KEY_UP      0x4800
# define KEY_DOWN    0x5000
# define KEY_LEFT    0x4b00
# define KEY_RIGHT   0x4d00
# define KEY_HOME    0x4700
# define KEY_END     0x4f00
# define KEY_ENTER   0x0d
# define KEY_ESC     0x1b
# define KEY_TAB     0x09
# define KEY_BACKTAB 0x0f00
# define KEY_BACKSPACE 0x7f
# define KEY_F(n)    (0x3b00 + (n) - 1)
#else
# include <curses.h>
# define KEY_BACKTAB KEY_BTAB
# ifndef KEY_ESC
#  define KEY_ESC 0x1b
# endif
# ifndef KEY_TAB
#  define KEY_TAB 0x09
# endif
#endif

void tui_init(void);
void tui_cleanup(void);
int tui_rows(void);
int tui_cols(void);
void tui_clear(void);
void tui_goto(int row, int col);
void tui_putstr(const char *s);
void tui_putchar(char c);
void tui_refresh(void);

/* Get key: returns curses key code or ASCII */
int tui_getkey(void);

/* Attribute helpers (1980s style: normal, bold, reverse) */
void tui_attr_normal(void);
void tui_attr_bold(void);
void tui_attr_reverse(void);

#endif
