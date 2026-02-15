/* app.c - Main application UI. 1980s-style TUI: menu bar, F-keys, status line. */

#include "app.h"
#include "tui.h"
#include "storage.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *module_names[] = {
    "Notes", "Tasks", "Contacts", "Calendar", "Trash"
};

static int get_item_count(int module) {
    switch (module) {
        case MODULE_NOTES:    return storage_notes_count();
        case MODULE_TASKS:    return storage_tasks_count();
        case MODULE_CONTACTS: return storage_contacts_count();
        case MODULE_CALENDAR: return storage_events_count();
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
    tui_putstr(" 1. Title: ");
    tui_putstr(n->title[0] ? n->title : "(no title)");
    tui_attr_normal();
    for (int i = 10 + (int)strlen(n->title[0] ? n->title : "(no title)"); i < box_width; i++) tui_putchar(' ');
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
    int content_width = box_width - 14;  /* leave room for " 2. Content: " */
    if (content_width < 1) content_width = 1;
    while (line < max_lines) {
        tui_goto(top + 3 + line, box_left);
        tui_putchar('|');
        if (line == 0) tui_putstr(" 2. Content: ");
        else tui_putstr("            ");
        int col = 12;
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
        tui_putstr("            ");
        for (int c = 12; c < box_width - 1; c++) tui_putchar(' ');
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
    } else if (a->current_module == MODULE_TRASH) {
        tui_goto(row, main_col);
        tui_putstr("Trash is empty.");
    }

    if (count == 0 && a->current_module != MODULE_TRASH) {
        tui_goto(row, main_col);
        tui_putstr("(No items. Press F2 or N to add.)");
    }
}

static void draw_status_bar(const AppState *a) {
    int status_row = a->rows - 1;
    tui_goto(status_row, 0);
    tui_attr_reverse();
    tui_putstr(" Up/Down Move  Enter Edit  Tab Switch  N New  D Delete  ? Help ");
    tui_attr_normal();
    int len = 60;
    if (a->message[0]) {
        tui_goto(status_row, len);
        tui_attr_reverse();
        tui_putstr(" ");
        int n = a->cols - len - 2;
        if (n > (int)strlen(a->message)) n = (int)strlen(a->message);
        for (int i = 0; i < n && a->message[i]; i++) tui_putchar(a->message[i]);
        tui_attr_normal();
    }
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

static void draw_content_editor(const AppState *a) {
    int box_width = a->cols - CONTENT_BOX_LEFT * 2 - 2;
    int box_height = CONTENT_BOX_HEIGHT;
    if (box_width < 10) box_width = 10;
    if (box_height > a->rows - CONTENT_BOX_TOP - 2) box_height = a->rows - CONTENT_BOX_TOP - 2;

    /* Top border */
    tui_goto(CONTENT_BOX_TOP, CONTENT_BOX_LEFT);
    tui_putchar('+');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('+');

    /* Title row */
    tui_goto(CONTENT_BOX_TOP + 1, CONTENT_BOX_LEFT);
    tui_putchar('|');
    tui_putstr(" 1. Title: ");
    tui_putstr(a->prompt_data[0][0] ? a->prompt_data[0] : "(no title)");
    for (int i = 10 + (int)strlen(a->prompt_data[0][0] ? a->prompt_data[0] : "(no title)"); i < box_width; i++) tui_putchar(' ');
    tui_putchar('|');

    /* Separator */
    tui_goto(CONTENT_BOX_TOP + 2, CONTENT_BOX_LEFT);
    tui_putchar('|');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('|');

    /* Content header */
    tui_goto(CONTENT_BOX_TOP + 3, CONTENT_BOX_LEFT);
    tui_putchar('|');
    tui_putstr(" 2. Content ");
    for (int i = 11; i < box_width; i++) tui_putchar(' ');
    tui_putchar('|');

    /* Content rows - render line by line */
    const char *p = a->content_edit_buf;
    int line = 0;
    int i = 0;
    int content_rows = box_height - 5;  /* after title, separator, content header, bottom, hint */
    if (content_rows < 1) content_rows = 1;
    while (line < content_rows) {
        tui_goto(CONTENT_BOX_TOP + 4 + line, CONTENT_BOX_LEFT);
        tui_putchar('|');
        int col = 0;
        while (i < a->content_edit_len && col < box_width) {
            char ch = p[i++];
            if (ch == '\n') break;
            tui_putchar(ch >= 32 && ch < 127 ? ch : ' ');
            col++;
        }
        if (i < a->content_edit_len && p[i] == '\n') i++;
        for (; col < box_width; col++) tui_putchar(' ');
        tui_putchar('|');
        line++;
        if (i >= a->content_edit_len) break;
    }
    for (; line < content_rows; line++) {
        tui_goto(CONTENT_BOX_TOP + 4 + line, CONTENT_BOX_LEFT);
        tui_putchar('|');
        for (int c = 0; c < box_width; c++) tui_putchar(' ');
        tui_putchar('|');
    }

    /* Bottom border */
    tui_goto(CONTENT_BOX_TOP + 4 + content_rows, CONTENT_BOX_LEFT);
    tui_putchar('+');
    for (int i = 0; i < box_width; i++) tui_putchar('-');
    tui_putchar('+');

    /* Hint */
    tui_goto(CONTENT_BOX_TOP + 5 + content_rows, CONTENT_BOX_LEFT);
    tui_attr_reverse();
    tui_putstr(" Enter=newline  F2=Save  Esc=Cancel ");
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
    "  Multi-line editor: Enter=newline, F2=Save, Esc=Cancel",
    "",
    "MODULES",
    "  Notes, Tasks, Contacts, Calendar, Trash",
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
        if (key == KEY_ESC) {
            a->content_edit_mode = 0;
            a->prompt_mode = 0;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_F(2)) {
            snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", a->content_edit_buf);
            if (a->prompt_is_edit) {
                int id = get_selected_id(a);
                if (id) {
                    storage_notes_update(id, a->prompt_data[0], a->content_edit_buf);
                    snprintf(a->message, sizeof(a->message), "Note updated.");
                }
            } else {
                int id = storage_notes_add(a->prompt_data[0], a->content_edit_buf);
                snprintf(a->message, sizeof(a->message), "Note %d added.", id);
                a->selected_index = get_item_count(MODULE_NOTES) - 1;
            }
            a->content_edit_mode = 0;
            a->prompt_mode = 0;
            return;
        }
        if (key == KEY_ENTER || key == '\n') {
            if (a->content_edit_len < CONTENT_EDIT_BUF_MAX - 1) {
                a->content_edit_buf[a->content_edit_len++] = '\n';
                a->content_edit_buf[a->content_edit_len] = '\0';
            }
            return;
        }
        if (key == KEY_BACKSPACE || key == 0x08) {
            if (a->content_edit_len > 0) {
                a->content_edit_len--;
                a->content_edit_buf[a->content_edit_len] = '\0';
            }
            return;
        }
        if (key >= 32 && key < 127 && a->content_edit_len < CONTENT_EDIT_BUF_MAX - 1) {
            a->content_edit_buf[a->content_edit_len++] = (char)key;
            a->content_edit_buf[a->content_edit_len] = '\0';
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
        if (key == KEY_ENTER || key == '\n') {
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
        if (key == KEY_ENTER || key == '\n') {
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
