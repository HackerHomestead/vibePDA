/* app.h - Application state and screens (1980s-style TUI) */

#ifndef APP_H
#define APP_H

#define MODULE_NOTES    0
#define MODULE_TASKS    1
#define MODULE_CONTACTS 2
#define MODULE_CALENDAR 3
#define MODULE_FACTS    4
#define MODULE_TRASH    5
#define MODULE_COUNT    6

#define APP_MESSAGE_LEN 80
#define COMMAND_BUF_LEN 256
#define PROMPT_BUF_LEN  512
#define CONTENT_EDIT_BUF_MAX 4096

typedef struct {
    int current_module;
    int focus_sidebar;   /* 1 = sidebar, 0 = main */
    int selected_index;
    int item_count;      /* cached count for current module */
    int rows;
    int cols;
    char message[APP_MESSAGE_LEN];
    int quit_requested;
    int show_help;
    /* Prompt mode: collect user input for New/Edit */
    int prompt_mode;
    int prompt_is_edit;  /* 1=editing, 0=adding */
    char prompt_label[32];
    char prompt_buf[PROMPT_BUF_LEN];
    int prompt_len;
    int prompt_step;     /* 0=first field, 1=second, etc. */
    char prompt_data[4][256];  /* collected field values */
    /* Content editor: bordered multi-line text area for note body */
    int content_edit_mode;
    char content_edit_buf[CONTENT_EDIT_BUF_MAX];
    int content_edit_len;
    int content_edit_cursor_pos;  /* cursor position in buffer */
    int content_edit_show_line_numbers;  /* 1=show line numbers */
    int content_edit_scroll_offset;  /* line number to start displaying from */
} AppState;

void app_init(AppState *a, int rows, int cols);
void app_handle_key(AppState *a, int key);
void app_draw(AppState *a);

#endif
