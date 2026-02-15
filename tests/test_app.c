/* test_app.c - Unit tests for app state and key handling (1980s TUI) */

#include "app.h"
#include "tui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_app(void) {
    AppState a;

    app_init(&a, 24, 80);
    assert(a.current_module == 0);
    assert(a.focus_sidebar == 1);
    assert(a.quit_requested == 0);
    assert(a.show_help == 0);
    assert(a.prompt_mode == 0);

    /* Module switch: Up/Down in sidebar */
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_TASKS);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_CONTACTS);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_CALENDAR);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_TRASH);
    app_handle_key(&a, KEY_UP);
    assert(a.current_module == MODULE_CALENDAR);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    assert(a.current_module == MODULE_NOTES);

    /* Focus: Tab switches to main pane */
    app_handle_key(&a, KEY_TAB);
    assert(a.focus_sidebar == 0);
    app_handle_key(&a, KEY_BACKTAB);
    assert(a.focus_sidebar == 1);

    /* Quit */
    app_init(&a, 24, 80);
    app_handle_key(&a, 'q');
    assert(a.quit_requested == 1);

    /* F1 / ? shows help */
    app_init(&a, 24, 80);
    app_handle_key(&a, '?');
    assert(a.show_help == 1);
    app_handle_key(&a, 'x'); /* any key closes help */
    assert(a.show_help == 0);

    app_init(&a, 24, 80);
    app_handle_key(&a, KEY_F(1));
    assert(a.show_help == 1);
    app_handle_key(&a, ' ');
    assert(a.show_help == 0);

    /* F2 / N starts new prompt (when not in Trash) */
    app_init(&a, 24, 80);
    app_handle_key(&a, 'n');
    assert(a.prompt_mode == 1);
    assert(a.prompt_is_edit == 0);
    app_handle_key(&a, KEY_ESC);
    assert(a.prompt_mode == 0);

    /* F10 quits */
    app_init(&a, 24, 80);
    app_handle_key(&a, KEY_F(10));
    assert(a.quit_requested == 1);

    /* Content editor: F2 in Notes, enter title, Enter advances to content_edit_mode */
    app_init(&a, 24, 80);
    app_handle_key(&a, 'n');
    assert(a.prompt_mode == 1);
    app_handle_key(&a, 'T');
    app_handle_key(&a, 'e');
    app_handle_key(&a, 's');
    app_handle_key(&a, 't');
    app_handle_key(&a, '\n');
    assert(a.content_edit_mode == 1);
    app_handle_key(&a, KEY_ESC);
    assert(a.content_edit_mode == 0);
    assert(a.prompt_mode == 0);
}
