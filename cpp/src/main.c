/* main.c - vibePDA C port entry (VT102 / DOS console) */

#include "config.h"
#include "app.h"
#include "tui.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    tui_init();

    AppState app;
    app_init(&app, TUI_ROWS, TUI_COLS);

    for (;;) {
        app_draw(&app);
        int key = tui_getkey();
        if (key == KEY_F(12))
            break;
        app_handle_key(&app, key);
    }

    tui_cleanup();
    return 0;
}
