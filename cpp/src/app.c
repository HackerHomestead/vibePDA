/* app.c - Main application UI (VT102/DOS compatible layout) */

#include "app.h"
#include "tui.h"
#include <stdio.h>
#include <string.h>

static const char *module_names[] = {
    "Notes", "Tasks", "Contacts", "Calendar", "Trash"
};

void app_init(AppState *a, int rows, int cols) {
    a->current_module = 0;
    a->focus_sidebar = 1;
    a->rows = rows;
    a->cols = cols;
}

static void draw_title_bar(void) {
    tui_attr_reverse();
    tui_putstr(" vibePDA ");
    tui_attr_normal();
    tui_goto(0, 9);
    /* Pad to end of line if needed */
    tui_putstr("\n");
}

static void draw_sidebar(const AppState *a) {
    int r;
    for (r = 1; r < a->rows - 2; r++) {
        tui_goto(r, 0);
        if (r == 1) {
            tui_putstr(" Modules");
        } else if (r >= 2 && r < 2 + MODULE_COUNT) {
            int i = r - 2;
            if (i == a->current_module && a->focus_sidebar) {
                tui_attr_reverse();
                tui_putstr("> ");
            } else {
                tui_putstr("  ");
            }
            tui_putstr(module_names[i]);
            tui_putstr(" (0)");
            if (i == a->current_module && a->focus_sidebar)
                tui_attr_normal();
        }
    }
}

static void draw_main(const AppState *a) {
    int r;
    int main_col = 18;
    int main_width = a->cols - main_col - 2;
    if (main_width < 10) main_width = 10;

    for (r = 1; r < a->rows - 2; r++) {
        tui_goto(r, main_col);
        if (r == 1) {
            tui_putstr(" ");
            tui_putstr(module_names[a->current_module]);
            tui_putstr(" ");
        } else if (r == 3) {
            tui_putstr("No items.");
        }
    }
}

static void draw_status(const AppState *a) {
    int row = a->rows - 1;
    tui_goto(row, 0);
    tui_attr_reverse();
    tui_putstr(" F1 Notes  F2 Tasks  F3 Contacts  F4 Calendar  F5 Trash ");
    tui_putstr(" |  F8 Main  F9 Side  |  F12 Quit ");
    tui_attr_normal();
}

void app_handle_key(AppState *a, int key) {
    if (key == KEY_F(12))
        return; /* caller checks for quit */

    if (key == KEY_F(8)) {
        a->focus_sidebar = 0;
        return;
    }
    if (key == KEY_F(9)) {
        a->focus_sidebar = 1;
        return;
    }

    if (a->focus_sidebar) {
        if (key == KEY_F(1)) { a->current_module = MODULE_NOTES; return; }
        if (key == KEY_F(2)) { a->current_module = MODULE_TASKS; return; }
        if (key == KEY_F(3)) { a->current_module = MODULE_CONTACTS; return; }
        if (key == KEY_F(4)) { a->current_module = MODULE_CALENDAR; return; }
        if (key == KEY_F(5)) { a->current_module = MODULE_TRASH; return; }
        if (key == KEY_UP || key == 'k') {
            if (a->current_module > 0) a->current_module--;
            return;
        }
        if (key == KEY_DOWN || key == 'j') {
            if (a->current_module < MODULE_COUNT - 1) a->current_module++;
            return;
        }
        if (key == KEY_TAB) {
            a->focus_sidebar = 0;
            return;
        }
    } else {
        if (key == 0x0f00) /* Shift+Tab */ {
            a->focus_sidebar = 1;
            return;
        }
    }
}

void app_draw(const AppState *a) {
    tui_clear();
    draw_title_bar();
    draw_sidebar(a);
    draw_main(a);
    draw_status(a);
    tui_refresh();
}
