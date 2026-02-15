/* app.c - Main application UI. 1980s-style TUI: menu bar, F-keys, status line. */

#include "app.h"
#include "tui.h"
#include "storage.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *module_names[] = {
    "Notes", "Tasks", "Contacts", "Calendar", "Facts", "Trash"
};

static int get_item_count(int module) {
    switch (module) {
        case MODULE_NOTES:    return storage_notes_count();
        case MODULE_TASKS:    return storage_tasks_count();
        case MODULE_CONTACTS: return storage_contacts_count();
        case MODULE_CALENDAR: return storage_events_count();
        case MODULE_FACTS:    return storage_facts_count();
        case MODULE_TRASH:    return storage_trash_count();
        default: return 0;
    }
}

void app_init(AppState *a, int rows, int cols) {
    a->current_module = 0;
    a->focus_sidebar = 1;
    a->selected_index = 0;
    a->item_count = 0;
    a->rows = rows;
    a->cols = cols;
    a->message[0] = '\0';
    a->quit_requested = 0;
    a->show_help = 0;
    a->prompt_mode = 0;
    a->prompt_is_edit = 0;
    a->prompt_label[0] = '\0';
    a->prompt_buf[0] = '\0';
    a->prompt_len = 0;
    a->prompt_step = 0;
    memset(a->prompt_data, 0, sizeof(a->prompt_data));
    a->content_edit_mode = 0;
    a->content_edit_buf[0] = '\0';
    a->content_edit_len = 0;
    a->content_edit_cursor_pos = 0;
    a->content_edit_show_line_numbers = 0;
    a->content_edit_scroll_offset = 0;
}

#define ROW_TITLE   0
#define ROW_MENU    1
#define ROW_CONTENT 2

static void draw_title_bar(const AppState *a) {
    tui_goto(ROW_TITLE, 0);
    tui_attr_reverse();
    tui_putstr(" vibePDA ");
    tui_putstr(" | ");
    tui_putstr(module_names[a->current_module]);
    for (int i = 18 + (int)strlen(module_names[a->current_module]); i < a->cols; i++)
        tui_putchar(' ');
    tui_attr_normal();
}

static void draw_menu_bar(const AppState *a) {
    (void)a;
    tui_goto(ROW_MENU, 0);
    tui_attr_bold();
    tui_putstr(" F1 Help  F2 New  F3 Edit  F4 Delete  F10 Quit ");
    tui_attr_normal();
    for (int i = 44; i < a->cols; i++) tui_putchar(' ');
}

static void draw_sidebar(const AppState *a) {
    int top = ROW_CONTENT;
    int bottom = a->rows - 1;
    for (int r = top; r < bottom; r++) {
        tui_goto(r, 0);
        if (r == top) {
            tui_putstr(" MODULES");
        } else if (r >= top + 1 && r < top + 1 + MODULE_COUNT) {
            int i = r - top - 1;
            if (i == a->current_module && a->focus_sidebar) {
                tui_attr_reverse();
                tui_putstr("> ");
            } else {
                tui_putstr("  ");
            }
            tui_putstr(module_names[i]);
            if (i == a->current_module && a->focus_sidebar)
                tui_attr_normal();
        }
    }
}

typedef struct {
    int id;
    int found;
    int idx;
    int selected;
} FindCtx;

typedef struct {
    int *row;
    int bottom;
    int main_col;
    int main_width;
    int idx;
    int selected;
    int focus_sidebar;
} DrawCtx;

typedef struct {
    VibeNote note;
    int found;
    int idx;
    int want_idx;
} NoteFetchCtx;

static void fetch_note_at_cb(const VibeNote *n, void *v) {
    NoteFetchCtx *c = (NoteFetchCtx *)v;
    if (c->idx == c->want_idx) {
        c->note = *n;
        c->found = 1;
    }
    c->idx++;
}

static void draw_note_card(AppState *a, int main_col, int main_width, int top, int bottom) {
    int count = get_item_count(MODULE_NOTES);
    if (count == 0) return;
    NoteFetchCtx ctx = {{0}, 0, 0, a->selected_index};
    storage_notes_list(fetch_note_at_cb, &ctx);
    if (!ctx.found) return;

    const VibeNote *n = &ctx.note;
    int box_width = main_width;
    int box_left = main_col;
    if (box_width < 10) box_width = 10;

    /* Card top border */
    tui_goto(top, box_left);
    tui_putchar('+');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('+');

    /* Title row */
    tui_goto(top + 1, box_left);
    tui_putchar('|');
    tui_attr_bold();
    tui_putstr(" Title: ");
    tui_putstr(n->title[0] ? n->title : "(no title)");
    tui_attr_normal();
    for (int i = 8 + (int)strlen(n->title[0] ? n->title : "(no title)"); i < box_width; i++) tui_putchar(' ');
    tui_putchar('|');

    /* Separator */
    tui_goto(top + 2, box_left);
    tui_putchar('|');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('|');

    /* Content rows */
    const char *p = n->content;
    int line = 0;
    int i = 0;
    int max_lines = bottom - top - 5;
    if (max_lines < 1) max_lines = 1;
    int content_width = box_width - 2;  /* just borders */
    if (content_width < 1) content_width = 1;
    while (line < max_lines) {
        tui_goto(top + 3 + line, box_left);
        tui_putchar('|');
        int col = 1;
        while (i < (int)strlen(n->content) && col < box_width - 1) {
            char ch = p[i++];
            if (ch == '\n') break;
            tui_putchar(ch >= 32 && ch < 127 ? ch : ' ');
            col++;
        }
        if (i < (int)strlen(n->content) && p[i] == '\n') i++;
        for (; col < box_width - 1; col++) tui_putchar(' ');
        tui_putchar('|');
        line++;
        if (i >= (int)strlen(n->content)) break;
    }
    for (; line < max_lines; line++) {
        tui_goto(top + 3 + line, box_left);
        tui_putchar('|');
        for (int c = 1; c < box_width - 1; c++) tui_putchar(' ');
        tui_putchar('|');
    }

    /* Bottom border */
    tui_goto(top + 3 + max_lines, box_left);
    tui_putchar('+');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('+');

    /* Card index hint */
    tui_goto(top + 4 + max_lines, box_left);
    {
        char buf[64];
        snprintf(buf, sizeof(buf), " Note %d of %d (Up/Down) ", a->selected_index + 1, count);
        tui_attr_reverse();
        tui_putstr(buf);
        tui_attr_normal();
    }
}

static void draw_task_cb(const VibeTask *t, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_reverse();
    char line[256];
    char mark = t->done ? 'x' : ' ';
    snprintf(line, sizeof(line), "[%c] %3d  %.*s", mark, t->id, c->main_width - 12, t->title[0] ? t->title : "(no title)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

static void draw_contact_cb(const VibeContact *c, void *v) {
    DrawCtx *cx = (DrawCtx *)v;
    if (*cx->row >= cx->bottom) return;
    tui_goto(*cx->row, cx->main_col);
    if (cx->idx == cx->selected && !cx->focus_sidebar) tui_attr_reverse();
    char line[256];
    snprintf(line, sizeof(line), "%3d  %.*s", c->id, cx->main_width - 8, c->name[0] ? c->name : "(no name)");
    tui_putstr(line);
    if (cx->idx == cx->selected && !cx->focus_sidebar) tui_attr_normal();
    (*cx->row)++;
    cx->idx++;
}

static void draw_event_cb(const VibeCalendarEvent *e, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_reverse();
    char line[256];
    snprintf(line, sizeof(line), "%3d  %.*s", e->id, c->main_width - 8, e->title[0] ? e->title : "(no title)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

static void draw_fact_cb(const VibeFact *f, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_reverse();
    char line[512];
    int key_len = c->main_width - 20;
    if (key_len < 10) key_len = 10;
    snprintf(line, sizeof(line), "%3d  %.*s = %.*s", f->id, key_len, f->key[0] ? f->key : "(no key)",
             c->main_width - key_len - 10, f->value[0] ? f->value : "(no value)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

static void draw_main(AppState *a) {
    int main_col = 18;
    int main_width = a->cols - main_col - 1;
    int top = ROW_CONTENT;
    int bottom = a->rows - 1;
    if (main_width < 10) main_width = 10;

    int count = get_item_count(a->current_module);
    a->item_count = count;

    if (a->selected_index >= count && count > 0) a->selected_index = count - 1;
    if (a->selected_index < 0) a->selected_index = 0;

    int row = top;
    tui_goto(row, main_col);
    tui_attr_bold();
    tui_putstr(module_names[a->current_module]);
    tui_putstr(" (");
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    tui_putstr(buf);
    tui_putstr(" items)");
    tui_attr_normal();
    row++;

    DrawCtx dctx = {&row, bottom, main_col, main_width, 0, a->selected_index, a->focus_sidebar};

    if (a->current_module == MODULE_NOTES) {
        if (count > 0) {
            draw_note_card(a, main_col, main_width, row, bottom);
        }
    } else if (a->current_module == MODULE_TASKS) {
        storage_tasks_list(draw_task_cb, &dctx);
    } else if (a->current_module == MODULE_CONTACTS) {
        storage_contacts_list(draw_contact_cb, &dctx);
    } else if (a->current_module == MODULE_CALENDAR) {
        storage_events_list(draw_event_cb, &dctx);
    } else if (a->current_module == MODULE_FACTS) {
        storage_facts_list(draw_fact_cb, &dctx);
    } else if (a->current_module == MODULE_TRASH) {
        tui_goto(row, main_col);
        tui_putstr("Trash is empty.");
    }

    if (count == 0 && a->current_module != MODULE_TRASH && a->current_module != MODULE_FACTS) {
        tui_goto(row, main_col);
        tui_putstr("(No items. Press F2 or N to add.)");
    }
}

static void draw_status_bar(const AppState *a) {
    int status_row = a->rows - 1;
    tui_goto(status_row, 0);
    tui_attr_reverse();
    
    /* Build status string: ":Module=>State" */
    char status_buf[64];
    const char *module = module_names[a->current_module];
    const char *state;
    
    if (a->content_edit_mode) {
        state = "ContentEditing";
    } else if (a->prompt_mode) {
        state = a->prompt_is_edit ? "Editing" : "Adding";
    } else {
        state = "View";
    }
    
    snprintf(status_buf, sizeof(status_buf), ":%s=>%s", module, state);
    tui_putstr(status_buf);
    
    /* Fill remaining space, leave room for message on right */
    int status_len = (int)strlen(status_buf);
    int message_start = a->cols - 1;
    if (a->message[0]) {
        int msg_len = (int)strlen(a->message);
        message_start = a->cols - msg_len - 1;
        if (message_start < status_len + 1) message_start = status_len + 1;
    }
    
    for (int i = status_len; i < message_start; i++)
        tui_putchar(' ');
    
    /* Show message on right if present */
    if (a->message[0]) {
        int n = a->cols - message_start - 1;
        if (n > (int)strlen(a->message)) n = (int)strlen(a->message);
        for (int i = 0; i < n && a->message[i]; i++) tui_putchar(a->message[i]);
    }
    
    tui_attr_normal();
}

static void draw_prompt(const AppState *a) {
    int row = a->rows - 1;
    tui_goto(row, 0);
    tui_attr_reverse();
    tui_putstr(a->prompt_label);
    tui_putstr(a->prompt_buf);
    for (int i = (int)strlen(a->prompt_label) + a->prompt_len; i < a->cols; i++)
        tui_putchar(' ');
    tui_attr_normal();
}

#define CONTENT_BOX_TOP    4
#define CONTENT_BOX_LEFT   2
#define CONTENT_BOX_HEIGHT 12
/* content_box_width = cols - 2*LEFT - 2 (for borders) */

/* Calculate which line number cursor is on (0-based) */
static int get_cursor_line_number(const AppState *a) {
    int pos = a->content_edit_cursor_pos;
    if (pos < 0) pos = 0;
    if (pos > a->content_edit_len) pos = a->content_edit_len;
    
    int line = 0;
    for (int i = 0; i < pos && i < a->content_edit_len; i++) {
        if (a->content_edit_buf[i] == '\n') line++;
    }
    return line;
}

/* Count total lines in content */
static int count_content_lines(const AppState *a) {
    int lines = 1;
    for (int i = 0; i < a->content_edit_len; i++) {
        if (a->content_edit_buf[i] == '\n') lines++;
    }
    return lines;
}

/* Ensure cursor is visible by adjusting scroll offset */
static void ensure_cursor_visible(AppState *a, int content_rows) {
    int cursor_line = get_cursor_line_number(a);
    int total_lines = count_content_lines(a);
    
    if (total_lines <= content_rows) {
        /* All content fits on screen, no scrolling needed */
        a->content_edit_scroll_offset = 0;
        return;
    }
    
    /* Ensure cursor is within visible range */
    if (cursor_line < a->content_edit_scroll_offset) {
        /* Cursor is above visible area - scroll up to show it */
        a->content_edit_scroll_offset = cursor_line;
    } else if (cursor_line >= a->content_edit_scroll_offset + content_rows) {
        /* Cursor is below visible area - scroll down to show it */
        a->content_edit_scroll_offset = cursor_line - content_rows + 1;
        if (a->content_edit_scroll_offset < 0) a->content_edit_scroll_offset = 0;
    }
    
    /* Clamp scroll offset to valid range */
    if (a->content_edit_scroll_offset < 0) {
        a->content_edit_scroll_offset = 0;
    }
    int max_scroll = total_lines - content_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (a->content_edit_scroll_offset > max_scroll) {
        a->content_edit_scroll_offset = max_scroll;
    }
}

static void draw_content_editor(AppState *a) {
    int box_width = a->cols - CONTENT_BOX_LEFT * 2 - 2;
    int box_height = CONTENT_BOX_HEIGHT;
    if (box_width < 10) box_width = 10;
    if (box_height > a->rows - CONTENT_BOX_TOP - 2) box_height = a->rows - CONTENT_BOX_TOP - 2;

    int line_num_width = a->content_edit_show_line_numbers ? 5 : 0;  /* "9999 " */
    int text_width = box_width - line_num_width;
    if (text_width < 5) text_width = 5;

    /* Top border */
    tui_goto(CONTENT_BOX_TOP, CONTENT_BOX_LEFT);
    tui_putchar('+');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('+');

    /* Title row */
    tui_goto(CONTENT_BOX_TOP + 1, CONTENT_BOX_LEFT);
    tui_putchar('|');
    tui_putstr(" Title: ");
    tui_putstr(a->prompt_data[0][0] ? a->prompt_data[0] : "(no title)");
    for (int i = 8 + (int)strlen(a->prompt_data[0][0] ? a->prompt_data[0] : "(no title)"); i < box_width; i++) tui_putchar(' ');
    tui_putchar('|');

    /* Separator */
    tui_goto(CONTENT_BOX_TOP + 2, CONTENT_BOX_LEFT);
    tui_putchar('|');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('|');

    /* Content rows - render line by line with cursor and scrolling */
    const char *p = a->content_edit_buf;
    int cursor_pos = a->content_edit_cursor_pos;
    if (cursor_pos < 0) cursor_pos = 0;
    if (cursor_pos > a->content_edit_len) cursor_pos = a->content_edit_len;
    
    int content_rows = box_height - 4;  /* after title, separator, bottom, hint */
    if (content_rows < 1) content_rows = 1;
    
    /* Ensure cursor is visible */
    ensure_cursor_visible(a, content_rows);
    
    /* Find start position for scroll offset */
    int scroll_line = a->content_edit_scroll_offset;
    int i = 0;
    int current_line = 0;
    while (current_line < scroll_line && i < a->content_edit_len) {
        if (p[i] == '\n') current_line++;
        i++;
    }
    
    int cursor_line = -1, cursor_col = -1;
    
    int line = 0;
    int line_num = scroll_line;
    
    while (line < content_rows) {
        tui_goto(CONTENT_BOX_TOP + 3 + line, CONTENT_BOX_LEFT);
        tui_putchar('|');
        
        /* Line number */
        if (a->content_edit_show_line_numbers) {
            char num_buf[16];
            int n = snprintf(num_buf, sizeof(num_buf), "%4d ", line_num + 1);
            if (n > 0 && n < (int)sizeof(num_buf)) tui_putstr(num_buf);
        }
        
        int col = 0;
        int line_start = i;
        while (i < a->content_edit_len && col < text_width) {
            if (i == cursor_pos) {
                cursor_line = line;
                cursor_col = col + line_num_width;
            }
            char ch = p[i];
            if (ch == '\n') {
                i++;
                break;
            }
            tui_putchar(ch >= 32 && ch < 127 ? ch : ' ');
            col++;
            i++;
        }
        if (i == cursor_pos && cursor_line < 0) {
            cursor_line = line;
            cursor_col = col + line_num_width;
        }
        for (; col < text_width; col++) {
            if (i == cursor_pos && cursor_line < 0) {
                cursor_line = line;
                cursor_col = col + line_num_width;
            }
            tui_putchar(' ');
        }
        tui_putchar('|');
        line++;
        line_num++;
        if (i >= a->content_edit_len) {
            if (cursor_pos == a->content_edit_len && cursor_line < 0) {
                cursor_line = line - 1;
                cursor_col = (i - line_start) + line_num_width;
                if (cursor_col >= box_width) cursor_col = box_width - 1;
            }
            break;
        }
    }
    for (; line < content_rows; line++) {
        tui_goto(CONTENT_BOX_TOP + 3 + line, CONTENT_BOX_LEFT);
        tui_putchar('|');
        if (a->content_edit_show_line_numbers) {
            char num_buf[16];
            int n = snprintf(num_buf, sizeof(num_buf), "%4d ", line_num + 1);
            if (n > 0 && n < (int)sizeof(num_buf)) tui_putstr(num_buf);
        }
        for (int c = 0; c < text_width; c++) tui_putchar(' ');
        tui_putchar('|');
        line_num++;
    }
    
    /* Draw cursor */
    if (cursor_line >= 0 && cursor_line < content_rows && cursor_col >= 0 && cursor_col < box_width) {
        tui_goto(CONTENT_BOX_TOP + 3 + cursor_line, CONTENT_BOX_LEFT + 1 + cursor_col);
        tui_attr_reverse();
        char cursor_char = (cursor_pos < a->content_edit_len && a->content_edit_buf[cursor_pos] != '\n') 
            ? a->content_edit_buf[cursor_pos] : ' ';
        tui_putchar(cursor_char);
        tui_attr_normal();
    }

    /* Bottom border */
    tui_goto(CONTENT_BOX_TOP + 3 + content_rows, CONTENT_BOX_LEFT);
    tui_putchar('+');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('+');

    /* Hint */
    tui_goto(CONTENT_BOX_TOP + 4 + content_rows, CONTENT_BOX_LEFT);
    tui_attr_reverse();
    char hint[80];
    const char *ln_status = a->content_edit_show_line_numbers ? "ON" : "OFF";
    snprintf(hint, sizeof(hint), " Enter=Newline  Enter+Enter=Save  Esc=Cancel  F5=Line# %s", ln_status);
    tui_putstr(hint);
    for (int i = (int)strlen(hint); i < box_width; i++) tui_putchar(' ');
    tui_attr_normal();
}

static const char *help_text[] = {
    "vibePDA - Terminal Personal Data Assistant",
    "",
    "NAVIGATION",
    "  Up/Down, j/k   Move selection",
    "  Tab            Switch between sidebar and list",
    "  Enter          Edit selected item",
    "",
    "ACTIONS",
    "  F2 or N        New item",
    "  F3 or E        Edit selected",
    "  F4 or D        Delete selected",
    "  F1 or ?        This help",
    "  F10 or q       Quit",
    "",
    "NOTE CONTENT",
    "  Multi-line editor: Enter=Newline, Enter+Enter=Save, Esc=Cancel",
    "",
    "MODULES",
    "  Notes, Tasks, Contacts, Calendar, Facts, Trash",
    "  Use Up/Down in sidebar to switch.",
    "",
    "Press any key to close",
};
#define HELP_LINES (sizeof(help_text)/sizeof(help_text[0]))

static void draw_help(const AppState *a) {
    int status_row = a->rows - 1;
    for (int r = ROW_CONTENT; r < status_row; r++) {
        tui_goto(r, 0);
        for (int c = 0; c < a->cols; c++) tui_putchar(' ');
    }
    for (size_t i = 0; i < HELP_LINES && (int)(ROW_CONTENT + 1 + i) < status_row; i++) {
        tui_goto(ROW_CONTENT + 1 + (int)i, 2);
        tui_putstr(help_text[i]);
    }
    tui_goto(status_row, 0);
    tui_attr_reverse();
    tui_putstr(" Press any key to close ");
    tui_attr_normal();
}

static int key_char(int key) {
    if (key > 0 && key < 0x100) {
        int c = key & 0xff;
        if (c >= 'A' && c <= 'Z') return c + 32;
        return c;
    }
    return key;
}

static void find_note_cb(const VibeNote *n, void *v) {
    FindCtx *c = (FindCtx *)v;
    if (c->idx == c->selected) { c->id = n->id; c->found = 1; }
    c->idx++;
}
static void find_task_cb(const VibeTask *t, void *v) {
    FindCtx *c = (FindCtx *)v;
    if (c->idx == c->selected) { c->id = t->id; c->found = 1; }
    c->idx++;
}
static void find_contact_cb(const VibeContact *c, void *v) {
    FindCtx *cx = (FindCtx *)v;
    if (cx->idx == cx->selected) { cx->id = c->id; cx->found = 1; }
    cx->idx++;
}
static void find_event_cb(const VibeCalendarEvent *e, void *v) {
    FindCtx *c = (FindCtx *)v;
    if (c->idx == c->selected) { c->id = e->id; c->found = 1; }
    c->idx++;
}
static void find_fact_cb(const VibeFact *f, void *v) {
    FindCtx *c = (FindCtx *)v;
    if (c->idx == c->selected) { c->id = f->id; c->found = 1; }
    c->idx++;
}

static int get_selected_id(const AppState *a) {
    int count = get_item_count(a->current_module);
    if (a->selected_index < 0 || a->selected_index >= count) return 0;
    FindCtx ctx = {0, 0, 0, a->selected_index};
    if (a->current_module == MODULE_NOTES)
        storage_notes_list(find_note_cb, &ctx);
    else if (a->current_module == MODULE_TASKS)
        storage_tasks_list(find_task_cb, &ctx);
    else if (a->current_module == MODULE_CONTACTS)
        storage_contacts_list(find_contact_cb, &ctx);
    else if (a->current_module == MODULE_CALENDAR)
        storage_events_list(find_event_cb, &ctx);
    else if (a->current_module == MODULE_FACTS)
        storage_facts_list(find_fact_cb, &ctx);
    return ctx.found ? ctx.id : 0;
}

static int start_new_prompt(AppState *a) {
    if (a->current_module == MODULE_TRASH) return 0;
    a->prompt_mode = 1;
    a->prompt_is_edit = 0;
    a->prompt_step = 0;
    a->prompt_len = 0;
    a->prompt_buf[0] = '\0';
    memset(a->prompt_data, 0, sizeof(a->prompt_data));
    switch (a->current_module) {
        case MODULE_NOTES:    snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
        case MODULE_TASKS:    snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
        case MODULE_CONTACTS: snprintf(a->prompt_label, sizeof(a->prompt_label), " Name: "); break;
        case MODULE_CALENDAR: snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
        case MODULE_FACTS:    snprintf(a->prompt_label, sizeof(a->prompt_label), " Key: "); break;
        default: a->prompt_mode = 0; return 0;
    }
    return 1;
}

static void finish_new_step(AppState *a) {
    snprintf(a->prompt_data[a->prompt_step], sizeof(a->prompt_data[0]), "%.255s", a->prompt_buf);
    a->prompt_len = 0;
    a->prompt_buf[0] = '\0';

    if (a->current_module == MODULE_NOTES) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            a->content_edit_mode = 1;
            a->content_edit_buf[0] = '\0';
            a->content_edit_len = 0;
            a->content_edit_cursor_pos = 0;
            a->content_edit_scroll_offset = 0;
            return;
        }
        int id = storage_notes_add(a->prompt_data[0], a->content_edit_buf);
        a->prompt_mode = 0;
        a->content_edit_mode = 0;
        snprintf(a->message, sizeof(a->message), "Note %d added.", id);
        a->selected_index = get_item_count(MODULE_NOTES) - 1;
        return;
    }
    if (a->current_module == MODULE_TASKS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Due (YYYY-MM-DD): ");
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Priority (0-3): ");
            return;
        }
        int prio = atoi(a->prompt_buf);
        if (prio < 0 || prio > 3) prio = 0;
        int id = storage_tasks_add(a->prompt_data[0], a->prompt_data[1], prio);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Task %d added.", id);
        a->selected_index = get_item_count(MODULE_TASKS) - 1;
        return;
    }
    if (a->current_module == MODULE_CONTACTS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Email: ");
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Phone: ");
            return;
        }
        int id = storage_contacts_add(a->prompt_data[0], a->prompt_data[1], a->prompt_data[2]);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Contact %d added.", id);
        a->selected_index = get_item_count(MODULE_CONTACTS) - 1;
        return;
    }
    if (a->current_module == MODULE_CALENDAR) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Start: ");
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " End: ");
            return;
        }
        int id = storage_events_add(a->prompt_data[0], NULL, a->prompt_data[1], a->prompt_data[2], 0);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Event %d added.", id);
        a->selected_index = get_item_count(MODULE_CALENDAR) - 1;
        return;
    }
    if (a->current_module == MODULE_FACTS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Value: ");
            return;
        }
        int id = storage_facts_add(a->prompt_data[0], a->prompt_buf);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Fact %d added.", id);
        a->selected_index = get_item_count(MODULE_FACTS) - 1;
        return;
    }
    a->prompt_mode = 0;
}

static int start_edit_prompt(AppState *a) {
    if (a->current_module == MODULE_TRASH) return 0;
    int id = get_selected_id(a);
    if (id == 0) return 0;

    a->prompt_is_edit = 1;
    if (a->current_module == MODULE_NOTES) {
        VibeNote n;
        if (!storage_note_get(id, &n)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", n.title);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", n.content);
        return 1;
    }
    if (a->current_module == MODULE_TASKS) {
        VibeTask t;
        if (!storage_task_get(id, &t)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", t.title);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%s", t.due_date);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%d", t.priority);
        return 1;
    }
    if (a->current_module == MODULE_CONTACTS) {
        VibeContact c;
        if (!storage_contact_get(id, &c)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Name: ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", c.name);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%s", c.email);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%s", c.phone);
        return 1;
    }
    if (a->current_module == MODULE_CALENDAR) {
        VibeCalendarEvent e;
        if (!storage_event_get(id, &e)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", e.title);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%s", e.start_at);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%s", e.end_at);
        return 1;
    }
    if (a->current_module == MODULE_FACTS) {
        VibeFact f;
        if (!storage_fact_get(id, &f)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Key: ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", f.key);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%s", f.value);
        return 1;
    }
    return 0;
}

static void finish_edit_step(AppState *a) {
    int id = get_selected_id(a);
    if (id == 0) { a->prompt_mode = 0; return; }

    snprintf(a->prompt_data[a->prompt_step], sizeof(a->prompt_data[0]), "%.255s", a->prompt_buf);

    if (a->current_module == MODULE_NOTES) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            a->content_edit_mode = 1;
            if (a->prompt_is_edit) {
                VibeNote n;
                if (storage_note_get(id, &n))
                    snprintf(a->content_edit_buf, sizeof(a->content_edit_buf), "%.4095s", n.content);
                else
                    a->content_edit_buf[0] = '\0';
            } else {
                a->content_edit_buf[0] = '\0';
            }
            a->content_edit_len = (int)strlen(a->content_edit_buf);
            a->content_edit_cursor_pos = a->content_edit_len;
            a->content_edit_scroll_offset = 0;
            return;
        }
        storage_notes_update(id, a->prompt_data[0], a->content_edit_buf);
        a->prompt_mode = 0;
        a->content_edit_mode = 0;
        snprintf(a->message, sizeof(a->message), "Note updated.");
        return;
    }
    if (a->current_module == MODULE_TASKS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Due: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[1]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Priority: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[2]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        int prio = atoi(a->prompt_buf);
        storage_tasks_update(id, a->prompt_data[0], a->prompt_data[1], prio, 0);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Task updated.");
        return;
    }
    if (a->current_module == MODULE_CONTACTS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Email: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[1]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Phone: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[2]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        storage_contacts_update(id, a->prompt_data[0], a->prompt_data[1], a->prompt_data[2]);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Contact updated.");
        return;
    }
    if (a->current_module == MODULE_CALENDAR) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Start: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[1]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " End: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[2]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        storage_events_update(id, a->prompt_data[0], NULL, a->prompt_data[1], a->prompt_data[2], 0);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Event updated.");
        return;
    }
    if (a->current_module == MODULE_FACTS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Value: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[1]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        storage_facts_update(id, a->prompt_data[0], a->prompt_buf);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Fact updated.");
        return;
    }
    a->prompt_mode = 0;
}

static int do_delete(AppState *a) {
    if (a->current_module == MODULE_TRASH) return 0;
    int id = get_selected_id(a);
    if (id == 0) return 0;
    switch (a->current_module) {
        case MODULE_NOTES:    storage_notes_delete(id); break;
        case MODULE_TASKS:    storage_tasks_delete(id); break;
        case MODULE_CONTACTS: storage_contacts_delete(id); break;
        case MODULE_CALENDAR: storage_events_delete(id); break;
        case MODULE_FACTS:    storage_facts_delete(id); break;
        default: return 0;
    }
    if (a->selected_index >= get_item_count(a->current_module) && a->selected_index > 0)
        a->selected_index--;
    snprintf(a->message, sizeof(a->message), "Deleted.");
    return 1;
}

void app_handle_key(AppState *a, int key) {
    int c = key_char(key);

    if (a->show_help) {
        a->show_help = 0;
        return;
    }

    if (a->content_edit_mode) {
        int pos = a->content_edit_cursor_pos;
        if (pos < 0) pos = 0;
        if (pos > a->content_edit_len) pos = a->content_edit_len;
        
        if (key == KEY_ESC) {
            a->content_edit_mode = 0;
            a->prompt_mode = 0;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_ENTER || key == '\n' || key == '\r') {
            /* Check if this is the second Enter in a row (two blank lines = save) */
            int is_double_enter = 0;
            if (pos > 0 && a->content_edit_buf[pos - 1] == '\n') {
                /* Cursor is right after a newline, so pressing Enter again = two blank lines */
                is_double_enter = 1;
            } else if (pos == 0 && a->content_edit_len == 0) {
                /* Empty buffer, first Enter = just insert newline */
                is_double_enter = 0;
            }
            
            if (is_double_enter) {
                /* Two blank lines in a row - save the note */
                snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", a->content_edit_buf);
                if (a->prompt_is_edit) {
                    int id = get_selected_id(a);
                    if (id) {
                        if (a->current_module == MODULE_NOTES) {
                            storage_notes_update(id, a->prompt_data[0], a->content_edit_buf);
                            snprintf(a->message, sizeof(a->message), "Note updated.");
                        }
                    }
                } else {
                    if (a->current_module == MODULE_NOTES) {
                        int id = storage_notes_add(a->prompt_data[0], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Note %d added.", id);
                        a->selected_index = get_item_count(MODULE_NOTES) - 1;
                    }
                }
                a->content_edit_mode = 0;
                a->prompt_mode = 0;
                return;
            } else {
                /* Single Enter - insert newline */
                if (a->content_edit_len < CONTENT_EDIT_BUF_MAX - 1) {
                    memmove(a->content_edit_buf + pos + 1, a->content_edit_buf + pos, a->content_edit_len - pos);
                    a->content_edit_buf[pos] = '\n';
                    a->content_edit_len++;
                    a->content_edit_buf[a->content_edit_len] = '\0';
                    a->content_edit_cursor_pos = pos + 1;
                }
                return;
            }
        }
        if (key == KEY_F(5)) {
            a->content_edit_show_line_numbers = !a->content_edit_show_line_numbers;
            return;
        }
#ifdef KEY_PPAGE
        if (key == KEY_PPAGE) {
            int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
            if (content_rows < 1) content_rows = 1;
            int total_lines = count_content_lines(a);
            if (total_lines > content_rows) {
                /* Page up: scroll up by one page */
                a->content_edit_scroll_offset -= content_rows;
                if (a->content_edit_scroll_offset < 0) a->content_edit_scroll_offset = 0;
                /* After scrolling, ensure cursor is still visible (it might have moved off screen) */
                ensure_cursor_visible(a, content_rows);
            }
            return;
        }
#endif
#ifdef KEY_NPAGE
        if (key == KEY_NPAGE) {
            int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
            if (content_rows < 1) content_rows = 1;
            int total_lines = count_content_lines(a);
            if (total_lines > content_rows) {
                /* Page down: scroll down by one page */
                a->content_edit_scroll_offset += content_rows;
                int max_scroll = total_lines - content_rows;
                if (max_scroll < 0) max_scroll = 0;
                if (a->content_edit_scroll_offset > max_scroll) {
                    a->content_edit_scroll_offset = max_scroll;
                }
                /* After scrolling, ensure cursor is still visible */
                ensure_cursor_visible(a, content_rows);
            }
            return;
        }
#endif
        if (key == KEY_LEFT) {
            if (pos > 0) a->content_edit_cursor_pos = pos - 1;
            return;
        }
        if (key == KEY_RIGHT) {
            if (pos < a->content_edit_len) a->content_edit_cursor_pos = pos + 1;
            return;
        }
        if (key == KEY_HOME) {
            /* Move to start of current line */
            while (pos > 0 && a->content_edit_buf[pos - 1] != '\n') pos--;
            a->content_edit_cursor_pos = pos;
            return;
        }
        if (key == KEY_END) {
            /* Move to end of current line */
            while (pos < a->content_edit_len && a->content_edit_buf[pos] != '\n') pos++;
            a->content_edit_cursor_pos = pos;
            return;
        }
        if (key == KEY_UP) {
            /* Move up one line */
            int line_start = pos;
            while (line_start > 0 && a->content_edit_buf[line_start - 1] != '\n') line_start--;
            if (line_start > 0) {
                int prev_line_start = line_start - 1;
                while (prev_line_start > 0 && a->content_edit_buf[prev_line_start - 1] != '\n') prev_line_start--;
                int col = pos - line_start;
                int new_pos = prev_line_start + col;
                while (new_pos < line_start && new_pos < a->content_edit_len && a->content_edit_buf[new_pos] != '\n') new_pos++;
                if (new_pos > line_start) new_pos = line_start;
                a->content_edit_cursor_pos = new_pos;
            } else {
                /* At first line, move to start */
                a->content_edit_cursor_pos = 0;
            }
            /* Immediately ensure cursor is visible */
            int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
            if (content_rows < 1) content_rows = 1;
            ensure_cursor_visible(a, content_rows);
            return;
        }
        if (key == KEY_DOWN) {
            /* Move down one line */
            int line_start = pos;
            while (line_start > 0 && a->content_edit_buf[line_start - 1] != '\n') line_start--;
            int line_end = pos;
            while (line_end < a->content_edit_len && a->content_edit_buf[line_end] != '\n') line_end++;
            if (line_end < a->content_edit_len) {
                int col = pos - line_start;
                int next_line_start = line_end + 1;
                int next_line_end = next_line_start;
                while (next_line_end < a->content_edit_len && a->content_edit_buf[next_line_end] != '\n') next_line_end++;
                int new_pos = next_line_start + col;
                if (new_pos > next_line_end) new_pos = next_line_end;
                a->content_edit_cursor_pos = new_pos;
            } else {
                /* At last line, move to end */
                a->content_edit_cursor_pos = a->content_edit_len;
            }
            /* Immediately ensure cursor is visible */
            int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
            if (content_rows < 1) content_rows = 1;
            ensure_cursor_visible(a, content_rows);
            return;
        }
        if (key == KEY_ENTER || key == '\n' || key == '\r') {
            /* Check if this is the second Enter in a row (two blank lines = save) */
            int is_double_enter = 0;
            if (pos > 0 && a->content_edit_buf[pos - 1] == '\n') {
                /* Cursor is right after a newline, so pressing Enter again = two blank lines */
                is_double_enter = 1;
            } else if (pos == 0 && a->content_edit_len == 0) {
                /* Empty buffer, first Enter = just insert newline */
                is_double_enter = 0;
            }
            
            if (is_double_enter) {
                /* Two blank lines in a row - save the note */
                snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", a->content_edit_buf);
                if (a->prompt_is_edit) {
                    int id = get_selected_id(a);
                    if (id) {
                        if (a->current_module == MODULE_NOTES) {
                            storage_notes_update(id, a->prompt_data[0], a->content_edit_buf);
                            snprintf(a->message, sizeof(a->message), "Note updated.");
                        }
                    }
                } else {
                    if (a->current_module == MODULE_NOTES) {
                        int id = storage_notes_add(a->prompt_data[0], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Note %d added.", id);
                        a->selected_index = get_item_count(MODULE_NOTES) - 1;
                    }
                }
                a->content_edit_mode = 0;
                a->prompt_mode = 0;
                return;
            } else {
                /* Single Enter - insert newline */
                if (a->content_edit_len < CONTENT_EDIT_BUF_MAX - 1) {
                    memmove(a->content_edit_buf + pos + 1, a->content_edit_buf + pos, a->content_edit_len - pos);
                    a->content_edit_buf[pos] = '\n';
                    a->content_edit_len++;
                    a->content_edit_buf[a->content_edit_len] = '\0';
                    a->content_edit_cursor_pos = pos + 1;
                    /* Ensure cursor stays visible after inserting newline */
                    int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
                    if (content_rows < 1) content_rows = 1;
                    ensure_cursor_visible(a, content_rows);
                }
                return;
            }
        }
        if (key == KEY_BACKSPACE || key == 0x08) {
            if (pos > 0) {
                memmove(a->content_edit_buf + pos - 1, a->content_edit_buf + pos, a->content_edit_len - pos);
                a->content_edit_len--;
                a->content_edit_buf[a->content_edit_len] = '\0';
                a->content_edit_cursor_pos = pos - 1;
                /* Ensure cursor stays visible after deletion */
                int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
                if (content_rows < 1) content_rows = 1;
                ensure_cursor_visible(a, content_rows);
            }
            return;
        }
        if (key >= 32 && key < 127 && a->content_edit_len < CONTENT_EDIT_BUF_MAX - 1) {
            memmove(a->content_edit_buf + pos + 1, a->content_edit_buf + pos, a->content_edit_len - pos);
            a->content_edit_buf[pos] = (char)key;
            a->content_edit_len++;
            a->content_edit_buf[a->content_edit_len] = '\0';
            a->content_edit_cursor_pos = pos + 1;
            /* Ensure cursor stays visible after insertion (especially when typing at bottom) */
            int content_rows = (a->rows - CONTENT_BOX_TOP - 4);
            if (content_rows < 1) content_rows = 1;
            ensure_cursor_visible(a, content_rows);
            return;
        }
        return;
    }

    if (a->prompt_mode) {
        if (key == KEY_ESC) {
            a->prompt_mode = 0;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_ENTER || key == '\n' || key == '\r') {
            if (a->prompt_is_edit)
                finish_edit_step(a);
            else
                finish_new_step(a);
            return;
        }
        if (key == KEY_BACKSPACE || key == 0x08) {
            if (a->prompt_len > 0) {
                a->prompt_len--;
                a->prompt_buf[a->prompt_len] = '\0';
            }
            return;
        }
        if (key >= 32 && key < 127 && a->prompt_len < PROMPT_BUF_LEN - 1) {
            a->prompt_buf[a->prompt_len++] = (char)key;
            a->prompt_buf[a->prompt_len] = '\0';
            return;
        }
        return;
    }

    if (key == KEY_F(10) || c == 'q') {
        a->quit_requested = 1;
        return;
    }
    if (key == KEY_F(1) || c == '?') {
        a->show_help = 1;
        return;
    }
    if (key == KEY_F(2) || c == 'n') {
        start_new_prompt(a);
        return;
    }
    if (key == KEY_F(3) || c == 'e') {
        start_edit_prompt(a);
        return;
    }
    if (key == KEY_F(4) || c == 'd') {
        do_delete(a);
        return;
    }

    if (a->focus_sidebar) {
        if (key == KEY_UP || key == 'k') {
            if (a->current_module > 0) {
                a->current_module--;
                a->selected_index = 0;
                if (a->current_module == MODULE_NOTES && get_item_count(MODULE_NOTES) > 0)
                    a->focus_sidebar = 0;
            }
            return;
        }
        if (key == KEY_DOWN || key == 'j') {
            if (a->current_module < MODULE_COUNT - 1) {
                a->current_module++;
                a->selected_index = 0;
                if (a->current_module == MODULE_NOTES && get_item_count(MODULE_NOTES) > 0)
                    a->focus_sidebar = 0;
            }
            return;
        }
        if (key == KEY_TAB) {
            a->focus_sidebar = 0;
            return;
        }
    } else {
        if (key == KEY_BACKTAB || (key == KEY_LEFT && a->cols > 0)) {
            a->focus_sidebar = 1;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_UP || key == 'k') {
            if (a->selected_index > 0) a->selected_index--;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_DOWN || key == 'j') {
            if (a->selected_index < get_item_count(a->current_module) - 1) a->selected_index++;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_ENTER || key == '\n' || key == '\r') {
            start_edit_prompt(a);
            return;
        }
        a->message[0] = '\0';
    }
}

void app_draw(AppState *a) {
    tui_clear();
    if (a->show_help) {
        draw_title_bar(a);
        draw_menu_bar(a);
        draw_help(a);
        tui_refresh();
        return;
    }
    draw_title_bar(a);
    draw_menu_bar(a);
    if (a->content_edit_mode) {
        draw_content_editor(a);
        draw_status_bar(a);
    } else {
        draw_sidebar(a);
        draw_main(a);
        if (a->prompt_mode)
            draw_prompt(a);
        else
            draw_status_bar(a);
    }
    tui_refresh();
}
