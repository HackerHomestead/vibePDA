/* tui.c - VT102 and DOS console I/O */

#include "tui.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_LINUX
# include <termios.h>
# include <unistd.h>
# include <sys/select.h>
static struct termios saved_termios;
static int tui_fd = 0;
#endif

#ifdef PLATFORM_DOS
# include <conio.h>
# include <bios.h>
#endif

static void vt102_cursor(int row, int col) {
    printf("\033[%d;%dH", row + 1, col + 1);
}

static void vt102_clear_screen(void) {
    printf("\033[2J");
}

static void vt102_attr(int n) {
    printf("\033[%dm", n);
}

void tui_init(void) {
#ifdef PLATFORM_LINUX
    tui_fd = STDIN_FILENO;
    if (isatty(tui_fd)) {
        struct termios t;
        tcgetattr(tui_fd, &saved_termios);
        t = saved_termios;
        t.c_lflag &= ~(ICANON | ECHO);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(tui_fd, TCSANOW, &t);
    }
#endif
    tui_clear();
}

void tui_cleanup(void) {
#ifdef PLATFORM_LINUX
    tui_attr_normal();
    if (isatty(tui_fd))
        tcsetattr(tui_fd, TCSANOW, &saved_termios);
#endif
}

void tui_clear(void) {
    vt102_clear_screen();
    vt102_cursor(0, 0);
}

void tui_goto(int row, int col) {
    vt102_cursor(row, col);
}

void tui_putstr(const char *s) {
    if (!s) return;
    fputs(s, stdout);
}

void tui_putchar(char c) {
    putchar(c);
}

void tui_refresh(void) {
    fflush(stdout);
}

void tui_attr_normal(void)  { vt102_attr(0); }
void tui_attr_bold(void)    { vt102_attr(1); }
void tui_attr_reverse(void) { vt102_attr(7); }

#ifdef PLATFORM_LINUX
static int read_key_linux(void) {
    char buf[8];
    int n = read(tui_fd, buf, sizeof(buf));
    if (n <= 0) return -1;
    if (n == 1) return (unsigned char)buf[0];
    /* ESC [ A = Up, B = Down, C = Right, D = Left */
    if (n >= 3 && buf[0] == 0x1b && buf[1] == '[') {
        if (buf[2] == 'A') return KEY_UP;
        if (buf[2] == 'B') return KEY_DOWN;
        if (buf[2] == 'C') return KEY_RIGHT;
        if (buf[2] == 'D') return KEY_LEFT;
    }
    return (unsigned char)buf[0];
}
#endif

#ifdef PLATFORM_DOS
static int read_key_dos(void) {
    int c = _bios_keybrd(_KEYBRD_READ);
    if (c & 0xff00)
        return c;
    return c & 0xff;
}
#endif

int tui_getkey(void) {
#ifdef PLATFORM_DOS
    return read_key_dos();
#else
    return read_key_linux();
#endif
}
