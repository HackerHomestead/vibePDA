/* app.c - Main application UI.
 *
 * 1980s-style TUI: menu bar, sidebar, main pane, F-keys, status line.
 * Handles CRUD, content editor, search/filter, trash. Draws note cards, lists.
 */
#include "app.h"
#include "tui.h"
#include "ui_box.h"
#include "storage.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *module_names[] = {
    "Notes", "Tasks", "Contacts", "Calendar", "Facts", "Finances", "Documents", "Trash"
};

/* Trash item storage for restore functionality */
#define MAX_TRASH_ITEMS 1000
static struct {
    int entity_type;
    int id;
} trash_items[MAX_TRASH_ITEMS];
static int trash_items_count = 0;
static int trash_selected[MAX_TRASH_ITEMS]; /* Checkbox state: 1=selected, 0=not selected */
static int trash_confirm_mode = 0; /* 1=showing confirmation prompt, 0=normal */

static int count_callback(void *ctx) {
    int *count = (int *)ctx;
    (*count)++;
    return 0;
}

static int get_item_count_with_filter(int module, const char *search_query) {
    int count = 0;
    if (search_query && search_query[0]) {
        /* Count filtered results */
        switch (module) {
            case MODULE_NOTES: {
                void count_note(const VibeNote *n, void *ctx) { (void)n; count_callback(ctx); }
                storage_notes_list_filtered(count_note, &count, search_query);
                return count;
            }
            case MODULE_TASKS: {
                void count_task(const VibeTask *t, void *ctx) { (void)t; count_callback(ctx); }
                storage_tasks_list_filtered(count_task, &count, search_query);
                return count;
            }
            case MODULE_CONTACTS: {
                void count_contact(const VibeContact *c, void *ctx) { (void)c; count_callback(ctx); }
                storage_contacts_list_filtered(count_contact, &count, search_query);
                return count;
            }
            case MODULE_CALENDAR: {
                void count_event(const VibeCalendarEvent *e, void *ctx) { (void)e; count_callback(ctx); }
                storage_events_list_filtered(count_event, &count, search_query);
                return count;
            }
            case MODULE_FACTS: {
                void count_fact(const VibeFact *f, void *ctx) { (void)f; count_callback(ctx); }
                storage_facts_list_filtered(count_fact, &count, search_query);
                return count;
            }
            case MODULE_FINANCES: {
                void count_finance(const VibeFinanceEntry *fe, void *ctx) { (void)fe; count_callback(ctx); }
                storage_finances_list_filtered(count_finance, &count, search_query);
                return count;
            }
            case MODULE_DOCUMENTS: {
                void count_doc(const VibeDocument *d, void *ctx) { (void)d; count_callback(ctx); }
                storage_documents_list_filtered(count_doc, &count, search_query);
                return count;
            }
            default: break;
        }
    }
    /* No filter or trash - use normal count */
    switch (module) {
        case MODULE_NOTES:    return storage_notes_count();
        case MODULE_TASKS:    return storage_tasks_count();
        case MODULE_CONTACTS: return storage_contacts_count();
        case MODULE_CALENDAR: return storage_events_count();
        case MODULE_FACTS:    return storage_facts_count();
        case MODULE_FINANCES: return storage_finances_count();
        case MODULE_DOCUMENTS: return storage_documents_count();
        case MODULE_TRASH:    return storage_trash_count();
        default: return 0;
    }
}

static int get_item_count(int module) {
    return get_item_count_with_filter(module, NULL);
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
    a->content_edit_field = 0;
    a->search_mode = 0;
    a->search_query[0] = '\0';
    a->search_len = 0;
}

#define ROW_TITLE   0
#define ROW_MENU    1
#define ROW_CONTENT 2

static void draw_title_bar(const AppState *a) {
    tui_goto(ROW_TITLE, 0);
    tui_attr_title_bar();
    tui_putstr(" vibePDA ");
    tui_putstr(" | ");
    tui_putstr(module_names[a->current_module]);
    for (int i = 18 + (int)strlen(module_names[a->current_module]); i < a->cols; i++)
        tui_putchar(' ');
    tui_attr_normal();
}

static void draw_menu_bar(const AppState *a) {
    tui_goto(ROW_MENU, 0);
    tui_attr_menu_bar();
    if (a->current_module == MODULE_TRASH) {
        tui_putstr(" F1 Help | R Restore | Space Toggle | A All | U None | X Delete | F10 Quit ");
    } else {
        tui_putstr(" F1 Help | F2 New | F3 Edit | F4 Delete | F5 Search | F10 Quit ");
    }
    tui_attr_normal();
    int len = a->current_module == MODULE_TRASH ? 68 : 58;
    for (int i = len; i < a->cols; i++) tui_putchar(' ');
}

static void draw_sidebar(const AppState *a) {
    int top = ROW_CONTENT;
    int bottom = a->rows - 1;
    int sidebar_width = 18; /* Fixed sidebar width */
    for (int r = top; r < bottom; r++) {
        tui_goto(r, 0);
        if (r == top) {
            tui_putstr(" MODULES");
            /* Clear rest of line */
            for (int i = 8; i < sidebar_width; i++) tui_putchar(' ');
        } else if (r >= top + 1 && r < top + 1 + MODULE_COUNT) {
            int i = r - top - 1;
            int is_selected = (i == a->current_module && a->focus_sidebar);
            if (is_selected) {
                tui_attr_sidebar_selected();
                tui_putstr("> ");
            } else {
                tui_putstr("  ");
            }
            tui_putstr(module_names[i]);
            if (is_selected)
                tui_attr_normal();
            /* Clear rest of line */
            int len = 2 + (int)strlen(module_names[i]);
            for (int j = len; j < sidebar_width; j++) tui_putchar(' ');
        } else {
            /* Clear entire line for rows beyond modules */
            for (int i = 0; i < sidebar_width; i++) tui_putchar(' ');
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
    int scroll_offset;  /* skip this many items before drawing (for list scroll) */
} DrawCtx;

typedef struct {
    VibeNote note;
    int found;
    int idx;
    int want_idx;
} NoteFetchCtx;

typedef struct {
    VibeContact contact;
    int found;
    int idx;
    int want_idx;
} ContactFetchCtx;

static void fetch_note_at_cb(const VibeNote *n, void *v) {
    NoteFetchCtx *c = (NoteFetchCtx *)v;
    if (c->idx == c->want_idx) {
        c->note = *n;
        c->found = 1;
    }
    c->idx++;
}

static void draw_note_card(AppState *a, int main_col, int main_width, int top, int bottom) {
    const char *filter_query = a->search_query[0] ? a->search_query : NULL;
    int count = get_item_count_with_filter(MODULE_NOTES, filter_query);
    if (count == 0) return;
    NoteFetchCtx ctx = {{0}, 0, 0, a->selected_index};
    if (filter_query) {
        storage_notes_list_filtered(fetch_note_at_cb, &ctx, filter_query);
    } else {
        storage_notes_list(fetch_note_at_cb, &ctx);
    }
    if (!ctx.found) return;

    const VibeNote *n = &ctx.note;
    int box_width = main_width;
    int box_left = main_col;
    if (box_width < 10) box_width = 10;

    /* Card top border */
    ui_box_top(top, box_left, box_width);

    /* Title row */
    tui_goto(top + 1, box_left);
    ui_box_v();
    tui_attr_card_title();
    tui_putstr(" Title: ");
    tui_attr_normal();
    tui_putstr(n->title[0] ? n->title : "(no title)");
    for (int i = 8 + (int)strlen(n->title[0] ? n->title : "(no title)"); i < box_width; i++) tui_putchar(' ');
    ui_box_v();

    /* Separator */
    ui_box_sep(top + 2, box_left, box_width);

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
        ui_box_v();
        int col = 1;
        while (i < (int)strlen(n->content) && col < box_width - 1) {
            char ch = p[i++];
            if (ch == '\n') break;
            tui_putchar(ch >= 32 && ch < 127 ? ch : ' ');
            col++;
        }
        if (i < (int)strlen(n->content) && p[i] == '\n') i++;
        for (; col < box_width - 1; col++) tui_putchar(' ');
        ui_box_v();
        line++;
        if (i >= (int)strlen(n->content)) break;
    }
    for (; line < max_lines; line++) {
        tui_goto(top + 3 + line, box_left);
        ui_box_v();
        for (int c = 1; c < box_width - 1; c++) tui_putchar(' ');
        ui_box_v();
    }

    /* Bottom border */
    ui_box_bottom(top + 3 + max_lines, box_left, box_width);

    /* Card index hint */
    tui_goto(top + 4 + max_lines, box_left);
    {
        char buf[64];
        snprintf(buf, sizeof(buf), " Note %d of %d (Up/Down) ", a->selected_index + 1, count);
        tui_attr_dim();
        tui_putstr(buf);
        tui_attr_normal();
    }
}

static void draw_task_cb(const VibeTask *t, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (c->idx < c->scroll_offset) { c->idx++; return; }
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_content_selected();
    char line[256];
    char mark = t->done ? 'x' : ' ';
    snprintf(line, sizeof(line), "[%c] %3d  %.*s", mark, t->id, c->main_width - 12, t->title[0] ? t->title : "(no title)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

#define CONTACT_CARD_MIN_WIDTH 28
#define CONTACT_CARD_HEIGHT 8
#define CONTACT_CARD_GAP 1

static void fetch_contact_at_cb(const VibeContact *c, void *v) {
    ContactFetchCtx *ctx = (ContactFetchCtx *)v;
    if (ctx->idx == ctx->want_idx) {
        ctx->contact = *c;
        ctx->found = 1;
    }
    ctx->idx++;
}

/* Draw a single compact contact card at (box_left, top) with given width. */
static void draw_single_contact_card(const VibeContact *c, int box_left, int top, int box_width, int is_selected) {
    if (box_width < 10) box_width = 10;
    int inner = box_width - 2;
    if (inner < 1) inner = 1;
    if (is_selected) tui_attr_content_selected();
    ui_box_top(top, box_left, box_width);
    tui_goto(top + 1, box_left);
    ui_box_v();
    tui_attr_bold();
    const char *name = c->name[0] ? c->name : "(no name)";
    int name_len = (int)strlen(name);
    for (int i = 0; i < inner; i++) tui_putchar(i < name_len ? name[i] : ' ');
    tui_attr_normal();
    if (is_selected) tui_attr_content_selected();
    ui_box_v();
    if (is_selected) tui_attr_normal();
    ui_box_sep(top + 2, box_left, box_width);
    tui_goto(top + 3, box_left);
    ui_box_v();
    if (is_selected) tui_attr_content_selected();
    tui_putstr(" ");
    const char *email = c->email[0] ? c->email : "-";
    int email_len = (int)strlen(email);
    for (int i = 0; i < inner - 1; i++) tui_putchar(i < email_len ? email[i] : ' ');
    if (is_selected) tui_attr_normal();
    ui_box_v();
    tui_goto(top + 4, box_left);
    ui_box_v();
    if (is_selected) tui_attr_content_selected();
    tui_putstr(" ");
    const char *phone = c->phone[0] ? c->phone : "-";
    int phone_len = (int)strlen(phone);
    for (int i = 0; i < inner - 1; i++) tui_putchar(i < phone_len ? phone[i] : ' ');
    if (is_selected) tui_attr_normal();
    ui_box_v();
    /* Notes row (truncated, first line only) */
    tui_goto(top + 5, box_left);
    ui_box_v();
    if (is_selected) tui_attr_content_selected();
    tui_putstr(" ");
    const char *notes = c->notes[0] ? c->notes : "";
    for (int i = 0; i < inner - 1; i++) {
        char ch = (i < (int)strlen(notes) && notes[i] != '\n') ? (notes[i] >= 32 ? notes[i] : ' ') : ' ';
        tui_putchar(ch);
    }
    if (is_selected) tui_attr_normal();
    ui_box_v();
    ui_box_bottom(top + 6, box_left, box_width);
}

static void draw_contact_card(AppState *a, int main_col, int main_width, int top, int bottom) {
    const char *filter_query = a->search_query[0] ? a->search_query : NULL;
    int count = get_item_count_with_filter(MODULE_CONTACTS, filter_query);
    if (count == 0) return;

    int avail_height = bottom - top - 2;  /* leave room for footer */
    if (avail_height < CONTACT_CARD_HEIGHT) avail_height = CONTACT_CARD_HEIGHT;

    /* Grid layout: how many columns fit? */
    int num_cols = 1;
    int card_width = main_width;
    if (main_width >= (CONTACT_CARD_MIN_WIDTH + CONTACT_CARD_GAP) * 2) {
        num_cols = (main_width + CONTACT_CARD_GAP) / (CONTACT_CARD_MIN_WIDTH + CONTACT_CARD_GAP);
        if (num_cols < 1) num_cols = 1;
        card_width = (main_width - (num_cols - 1) * CONTACT_CARD_GAP) / num_cols;
        if (card_width < CONTACT_CARD_MIN_WIDTH) {
            num_cols = main_width / (CONTACT_CARD_MIN_WIDTH + CONTACT_CARD_GAP);
            if (num_cols < 1) num_cols = 1;
            card_width = (main_width - (num_cols - 1) * CONTACT_CARD_GAP) / num_cols;
        }
    }
    int num_rows = avail_height / (CONTACT_CARD_HEIGHT + CONTACT_CARD_GAP);
    if (num_rows < 1) num_rows = 1;
    int cards_per_screen = num_cols * num_rows;

    /* Scroll so selected card is visible */
    int start_index = a->selected_index;
    if (cards_per_screen < count && a->selected_index >= cards_per_screen) {
        start_index = a->selected_index - (a->selected_index % num_cols) - (num_rows - 1) * num_cols;
        if (start_index < 0) start_index = 0;
    }
    int end_index = start_index + cards_per_screen;
    if (end_index > count) end_index = count;

    /* Draw each visible card */
    for (int i = start_index; i < end_index; i++) {
        ContactFetchCtx ctx = {{0}, 0, 0, i};
        if (filter_query) {
            storage_contacts_list_filtered(fetch_contact_at_cb, &ctx, filter_query);
        } else {
            storage_contacts_list(fetch_contact_at_cb, &ctx);
        }
        if (!ctx.found) continue;
        int grid_idx = i - start_index;
        int row = grid_idx / num_cols;
        int col = grid_idx % num_cols;
        int card_left = main_col + col * (card_width + CONTACT_CARD_GAP);
        int card_top = top + row * (CONTACT_CARD_HEIGHT + CONTACT_CARD_GAP);
        int is_selected = (i == a->selected_index && !a->focus_sidebar);
        draw_single_contact_card(&ctx.contact, card_left, card_top, card_width, is_selected);
    }

    /* Footer: Contact N of M (Up/Down/Left/Right) */
    int footer_row = top + num_rows * (CONTACT_CARD_HEIGHT + CONTACT_CARD_GAP);
    if (footer_row < bottom) {
        tui_goto(footer_row, main_col);
        char buf[80];
        const char *nav = (num_cols > 1) ? "Up/Down/Left/Right" : "Up/Down";
        snprintf(buf, sizeof(buf), " Contact %d of %d (%s) ", a->selected_index + 1, count, nav);
        tui_attr_dim();
        tui_putstr(buf);
        tui_attr_normal();
    }
}

static void draw_event_cb(const VibeCalendarEvent *e, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (c->idx < c->scroll_offset) { c->idx++; return; }
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_content_selected();
    char line[256];
    snprintf(line, sizeof(line), "%3d  %.*s", e->id, c->main_width - 8, e->title[0] ? e->title : "(no title)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

static void draw_fact_cb(const VibeFact *f, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (c->idx < c->scroll_offset) { c->idx++; return; }
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_content_selected();
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

static void draw_finance_cb(const VibeFinanceEntry *fe, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (c->idx < c->scroll_offset) { c->idx++; return; }
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_content_selected();
    char line[256];
    snprintf(line, sizeof(line), "%3d  %s  $%.2f  %.*s", fe->id, fe->date,
             fe->amount, c->main_width - 25, fe->description[0] ? fe->description : "(no description)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

static void draw_document_cb(const VibeDocument *d, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    if (c->idx < c->scroll_offset) { c->idx++; return; }
    if (*c->row >= c->bottom) return;
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_content_selected();
    char line[256];
    int max_title = (int)sizeof(line) - 92;  /* leave room for "%3d  [%.80s] " */
    if (max_title > c->main_width - 20) max_title = c->main_width - 20;
    if (max_title < 1) max_title = 1;
    snprintf(line, sizeof(line), "%3d  [%.80s] %.*s", d->id, d->template_name[0] ? d->template_name : "default",
             max_title, d->title[0] ? d->title : "(no title)");
    tui_putstr(line);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_normal();
    (*c->row)++;
    c->idx++;
}

static void draw_trash_cb(int entity_type, int id, const char *title, void *v) {
    DrawCtx *c = (DrawCtx *)v;
    /* Always store every trash item for restore/delete - needed even when scrolled off-screen */
    if (c->idx < MAX_TRASH_ITEMS) {
        trash_items[c->idx].entity_type = entity_type;
        trash_items[c->idx].id = id;
    }
    if (c->idx < c->scroll_offset) { c->idx++; return; }
    if (*c->row >= c->bottom) return;
    
    tui_goto(*c->row, c->main_col);
    if (c->idx == c->selected && !c->focus_sidebar) tui_attr_content_selected();
    
    /* Show checkbox */
    int checked = (c->idx < MAX_TRASH_ITEMS && trash_selected[c->idx]) ? 1 : 0;
    tui_putchar('[');
    if (checked) tui_putchar('X');
    else tui_putchar(' ');
    tui_putchar(']');
    tui_putchar(' ');
    
    const char *type_names[] = {"Note", "Task", "Contact", "Event", "Fact", "Finance", "Document"};
    char line[256];
    snprintf(line, sizeof(line), "[%s] %3d  %.*s", 
             entity_type >= 0 && entity_type < 7 ? type_names[entity_type] : "?",
             id, c->main_width - 25, title ? title : "(no title)");
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

    const char *filter_query = a->search_query[0] ? a->search_query : NULL;
    int count = get_item_count_with_filter(a->current_module, filter_query);
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
    if (a->search_query[0]) {
        tui_putstr(" [Filter: ");
        tui_putstr(a->search_query);
        tui_putstr("]");
    }
    tui_attr_normal();
    row++;

    /* List scroll: when more items than fit, skip items so selection stays visible */
    int content_bottom = bottom;
    if (a->current_module == MODULE_TRASH && trash_confirm_mode)
        content_bottom = bottom - 4;  /* reserve rows for confirmation prompt */
    int visible_rows = content_bottom - row;
    if (visible_rows < 1) visible_rows = 1;
    int list_scroll = 0;
    if (count > visible_rows && a->selected_index >= visible_rows)
        list_scroll = a->selected_index - visible_rows + 1;
    if (list_scroll > count - visible_rows) list_scroll = count - visible_rows;
    if (list_scroll < 0) list_scroll = 0;

    DrawCtx dctx = {&row, content_bottom, main_col, main_width, 0, a->selected_index, a->focus_sidebar, list_scroll};

    if (a->current_module == MODULE_NOTES) {
        if (count > 0) {
            /* Note card drawing handles filtering internally */
            draw_note_card(a, main_col, main_width, row, bottom);
        }
    } else if (a->current_module == MODULE_TASKS) {
        if (filter_query) {
            storage_tasks_list_filtered(draw_task_cb, &dctx, filter_query);
        } else {
            storage_tasks_list(draw_task_cb, &dctx);
        }
    } else if (a->current_module == MODULE_CONTACTS) {
        if (count > 0) {
            draw_contact_card(a, main_col, main_width, row, bottom);
        }
    } else if (a->current_module == MODULE_CALENDAR) {
        if (filter_query) {
            storage_events_list_filtered(draw_event_cb, &dctx, filter_query);
        } else {
            storage_events_list(draw_event_cb, &dctx);
        }
    } else if (a->current_module == MODULE_FACTS) {
        if (filter_query) {
            storage_facts_list_filtered(draw_fact_cb, &dctx, filter_query);
        } else {
            storage_facts_list(draw_fact_cb, &dctx);
        }
    } else if (a->current_module == MODULE_FINANCES) {
        if (filter_query) {
            storage_finances_list_filtered(draw_finance_cb, &dctx, filter_query);
        } else {
            storage_finances_list(draw_finance_cb, &dctx);
        }
    } else if (a->current_module == MODULE_DOCUMENTS) {
        if (filter_query) {
            storage_documents_list_filtered(draw_document_cb, &dctx, filter_query);
        } else {
            storage_documents_list(draw_document_cb, &dctx);
        }
    } else if (a->current_module == MODULE_TRASH) {
        if (count > 0) {
            /* Don't reset selections - preserve checkbox state */
            trash_items_count = 0;
            storage_trash_list(draw_trash_cb, &dctx);
            trash_items_count = dctx.idx;
            
            /* Show confirmation prompt if in confirm mode */
            if (trash_confirm_mode) {
                row = bottom - 3;
                tui_goto(row, main_col);
                tui_attr_bold();
                int selected_count = 0;
                for (int i = 0; i < trash_items_count; i++) {
                    if (trash_selected[i]) selected_count++;
                }
                tui_putstr("PERMANENTLY DELETE ");
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", selected_count);
                tui_putstr(buf);
                tui_putstr(" SELECTED ITEM(S)?");
                tui_attr_normal();
                row++;
                tui_goto(row, main_col);
                tui_putstr("Press Y to confirm, N or Esc to cancel");
            }
        } else {
            tui_goto(row, main_col);
            tui_putstr("Trash is empty.");
            trash_items_count = 0;
            trash_confirm_mode = 0;
        }
    }

    if (count == 0 && a->current_module != MODULE_TRASH && a->current_module != MODULE_FACTS && 
        a->current_module != MODULE_FINANCES && a->current_module != MODULE_DOCUMENTS) {
        tui_goto(row, main_col);
        tui_putstr("(No items. Press F2 or N to add.)");
    }
}

static void draw_status_bar(const AppState *a) {
    int status_row = a->rows - 1;
    tui_goto(status_row, 0);
    tui_attr_status_bar();
    
    /* Build status string: ":Module=>State" */
    char status_buf[64];
    const char *module = module_names[a->current_module];
    const char *state;
    
    if (a->content_edit_mode) {
        state = "ContentEditing";
    } else if (a->prompt_mode) {
        state = a->prompt_is_edit ? "Editing" : "Adding";
    } else if (a->search_mode) {
        state = "Searching";
    } else if (a->search_query[0]) {
        state = "Filtered";
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
    tui_attr_status_bar();
    tui_putstr(a->prompt_label);
    tui_putstr(a->prompt_buf);
    /* Show blinking cursor - toggle based on a simple counter */
    static int cursor_frame = 0;
    cursor_frame++;
    if ((cursor_frame / 10) % 2 == 0) {
        /* Show cursor */
        tui_attr_normal();
        tui_attr_reverse();
        tui_putchar('_');
        tui_attr_normal();
        tui_attr_reverse();
    } else {
        /* Hide cursor (blink off) */
        tui_putchar(' ');
    }
    for (int i = (int)strlen(a->prompt_label) + a->prompt_len + 1; i < a->cols; i++)
        tui_putchar(' ');
    tui_attr_normal();
}

static void draw_search_prompt(const AppState *a) {
    int row = a->rows - 1;
    tui_goto(row, 0);
    tui_attr_status_bar();
    tui_putstr(" Search: ");
    tui_putstr(a->search_query);
    /* Show blinking cursor - toggle based on a simple counter */
    static int search_cursor_frame = 0;
    search_cursor_frame++;
    if ((search_cursor_frame / 10) % 2 == 0) {
        /* Show cursor */
        tui_attr_normal();
        tui_attr_reverse();
        tui_putchar('_');
        tui_attr_normal();
        tui_attr_reverse();
    } else {
        /* Hide cursor (blink off) */
        tui_putchar(' ');
    }
    for (int i = 9 + a->search_len + 1; i < a->cols; i++)
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

#define CONTACT_FORM_TOP    4
#define CONTACT_FORM_LEFT   2
#define CONTACT_FORM_HEIGHT 14

/* Contact form editor: Name, Email, Phone, Notes in a box (like notes content editor) */
static void draw_contact_form_editor(AppState *a) {
    int box_width = a->cols - CONTACT_FORM_LEFT * 2 - 2;
    int box_height = CONTACT_FORM_HEIGHT;
    if (box_width < 20) box_width = 20;
    if (box_height > a->rows - CONTACT_FORM_TOP - 2) box_height = a->rows - CONTACT_FORM_TOP - 2;

    int inner = box_width - 2;
    if (inner < 1) inner = 1;

    ui_box_top(CONTACT_FORM_TOP, CONTACT_FORM_LEFT, box_width);

    /* Name row */
    tui_goto(CONTACT_FORM_TOP + 1, CONTACT_FORM_LEFT);
    ui_box_v();
    tui_attr_card_title();
    tui_putstr(" Name:  ");
    tui_attr_normal();
    const char *name_val = (a->content_edit_field == 0) ? a->prompt_buf : a->prompt_data[0];
    int name_len = (int)strlen(name_val);
    for (int i = 0; i < inner - 7; i++) {
        if (a->content_edit_field == 0 && i == a->prompt_len) tui_attr_reverse();
        tui_putchar(i < name_len ? (name_val[i] >= 32 && name_val[i] < 127 ? name_val[i] : ' ') : ' ');
        if (a->content_edit_field == 0 && i == a->prompt_len) tui_attr_normal();
    }
    ui_box_v();

    /* Email row */
    tui_goto(CONTACT_FORM_TOP + 2, CONTACT_FORM_LEFT);
    ui_box_v();
    tui_attr_card_title();
    tui_putstr(" Email: ");
    tui_attr_normal();
    const char *email_val = (a->content_edit_field == 1) ? a->prompt_buf : a->prompt_data[1];
    int email_len = (int)strlen(email_val);
    for (int i = 0; i < inner - 7; i++) {
        if (a->content_edit_field == 1 && i == a->prompt_len) tui_attr_reverse();
        tui_putchar(i < email_len ? (email_val[i] >= 32 && email_val[i] < 127 ? email_val[i] : ' ') : ' ');
        if (a->content_edit_field == 1 && i == a->prompt_len) tui_attr_normal();
    }
    ui_box_v();

    /* Phone row */
    tui_goto(CONTACT_FORM_TOP + 3, CONTACT_FORM_LEFT);
    ui_box_v();
    tui_attr_card_title();
    tui_putstr(" Phone: ");
    tui_attr_normal();
    const char *phone_val = (a->content_edit_field == 2) ? a->prompt_buf : a->prompt_data[2];
    int phone_len = (int)strlen(phone_val);
    for (int i = 0; i < inner - 7; i++) {
        if (a->content_edit_field == 2 && i == a->prompt_len) tui_attr_reverse();
        tui_putchar(i < phone_len ? (phone_val[i] >= 32 && phone_val[i] < 127 ? phone_val[i] : ' ') : ' ');
        if (a->content_edit_field == 2 && i == a->prompt_len) tui_attr_normal();
    }
    ui_box_v();

    ui_box_sep(CONTACT_FORM_TOP + 4, CONTACT_FORM_LEFT, box_width);

    /* Notes content area */
    const char *p = a->content_edit_buf;
    int content_rows = box_height - 7;
    if (content_rows < 1) content_rows = 1;
    ensure_cursor_visible(a, content_rows);
    int scroll_line = a->content_edit_scroll_offset;
    int i = 0, current_line = 0;
    while (current_line < scroll_line && i < a->content_edit_len) {
        if (p[i] == '\n') current_line++;
        i++;
    }
    int line = 0;
    while (line < content_rows) {
        tui_goto(CONTACT_FORM_TOP + 5 + line, CONTACT_FORM_LEFT);
        ui_box_v();
        tui_putstr(" ");
        int col = 0;
        while (i < a->content_edit_len && col < inner - 1) {
            if (a->content_edit_field == 3 && i == a->content_edit_cursor_pos) tui_attr_reverse();
            char ch = p[i];
            if (ch == '\n') { i++; break; }
            tui_putchar(ch >= 32 && ch < 127 ? ch : ' ');
            if (a->content_edit_field == 3 && i == a->content_edit_cursor_pos) tui_attr_normal();
            col++; i++;
        }
        if (a->content_edit_field == 3 && i == a->content_edit_cursor_pos && col < inner - 1) tui_attr_reverse();
        for (; col < inner - 1; col++) tui_putchar(' ');
        if (a->content_edit_field == 3 && i == a->content_edit_cursor_pos) tui_attr_normal();
        ui_box_v();
        line++;
        if (i >= a->content_edit_len) break;
    }
    for (; line < content_rows; line++) {
        tui_goto(CONTACT_FORM_TOP + 5 + line, CONTACT_FORM_LEFT);
        ui_box_v();
        for (int c = 0; c < inner; c++) tui_putchar(' ');
        ui_box_v();
    }

    ui_box_bottom(CONTACT_FORM_TOP + 5 + content_rows, CONTACT_FORM_LEFT, box_width);

    tui_goto(CONTACT_FORM_TOP + 6 + content_rows, CONTACT_FORM_LEFT);
    tui_attr_dim();
    tui_putstr(" Tab=Next  Enter+Enter=Save  Esc=Cancel ");
    tui_attr_normal();
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
    ui_box_top(CONTENT_BOX_TOP, CONTENT_BOX_LEFT, box_width);

    /* Title row */
    tui_goto(CONTENT_BOX_TOP + 1, CONTENT_BOX_LEFT);
    ui_box_v();
    tui_attr_card_title();
    tui_putstr(" Title: ");
    tui_attr_normal();
    tui_putstr(a->prompt_data[0][0] ? a->prompt_data[0] : "(no title)");
    for (int i = 8 + (int)strlen(a->prompt_data[0][0] ? a->prompt_data[0] : "(no title)"); i < box_width; i++) tui_putchar(' ');
    ui_box_v();

    /* Separator */
    ui_box_sep(CONTENT_BOX_TOP + 2, CONTENT_BOX_LEFT, box_width);

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
        ui_box_v();
        
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
        ui_box_v();
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
        ui_box_v();
        if (a->content_edit_show_line_numbers) {
            char num_buf[16];
            int n = snprintf(num_buf, sizeof(num_buf), "%4d ", line_num + 1);
            if (n > 0 && n < (int)sizeof(num_buf)) tui_putstr(num_buf);
        }
        for (int c = 0; c < text_width; c++) tui_putchar(' ');
        ui_box_v();
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
    ui_box_bottom(CONTENT_BOX_TOP + 3 + content_rows, CONTENT_BOX_LEFT, box_width);

    /* Hint */
    tui_goto(CONTENT_BOX_TOP + 4 + content_rows, CONTENT_BOX_LEFT);
    tui_attr_dim();
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
    "  Left/Right     Move in grid (Contacts)",
    "  Tab            Switch between sidebar and list",
    "  Enter          Edit selected item",
    "",
    "ACTIONS",
    "  F2 or N        New item",
    "  F3 or E        Edit selected",
    "  F4 or D        Delete selected",
    "  F5 or /        Search/Filter items",
    "  F1 or ?        This help",
    "  F10 or q       Quit",
    "",
    "NOTE CONTENT",
    "  Multi-line editor: Enter=Newline, Enter+Enter=Save, Esc=Cancel",
    "",
    "CONTACT FORM",
    "  Tab/Enter=Next field  Enter+Enter in Notes=Save  Esc=Cancel",
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
    tui_attr_status_bar();
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
static void find_finance_cb(const VibeFinanceEntry *fe, void *v) {
    FindCtx *c = (FindCtx *)v;
    if (c->idx == c->selected) { c->id = fe->id; c->found = 1; }
    c->idx++;
}
static void find_document_cb(const VibeDocument *d, void *v) {
    FindCtx *c = (FindCtx *)v;
    if (c->idx == c->selected) { c->id = d->id; c->found = 1; }
    c->idx++;
}

static int get_selected_id(const AppState *a) {
    const char *fq = a->search_query[0] ? a->search_query : NULL;
    int count = get_item_count_with_filter(a->current_module, fq);
    if (a->selected_index < 0 || a->selected_index >= count) return 0;
    FindCtx ctx = {0, 0, 0, a->selected_index};
    if (a->current_module == MODULE_NOTES) {
        if (fq) storage_notes_list_filtered(find_note_cb, &ctx, fq);
        else storage_notes_list(find_note_cb, &ctx);
    } else if (a->current_module == MODULE_TASKS) {
        if (fq) storage_tasks_list_filtered(find_task_cb, &ctx, fq);
        else storage_tasks_list(find_task_cb, &ctx);
    } else if (a->current_module == MODULE_CONTACTS) {
        if (fq) storage_contacts_list_filtered(find_contact_cb, &ctx, fq);
        else storage_contacts_list(find_contact_cb, &ctx);
    } else if (a->current_module == MODULE_CALENDAR) {
        if (fq) storage_events_list_filtered(find_event_cb, &ctx, fq);
        else storage_events_list(find_event_cb, &ctx);
    } else if (a->current_module == MODULE_FACTS) {
        if (fq) storage_facts_list_filtered(find_fact_cb, &ctx, fq);
        else storage_facts_list(find_fact_cb, &ctx);
    } else if (a->current_module == MODULE_FINANCES) {
        if (fq) storage_finances_list_filtered(find_finance_cb, &ctx, fq);
        else storage_finances_list(find_finance_cb, &ctx);
    } else if (a->current_module == MODULE_DOCUMENTS) {
        if (fq) storage_documents_list_filtered(find_document_cb, &ctx, fq);
        else storage_documents_list(find_document_cb, &ctx);
    }
    return ctx.found ? ctx.id : 0;
}

static int start_new_prompt(AppState *a) {
    if (a->current_module == MODULE_TRASH) return 0;
    if (a->current_module == MODULE_CONTACTS) {
        a->content_edit_mode = 1;
        a->prompt_is_edit = 0;
        a->content_edit_field = 0;
        memset(a->prompt_data, 0, sizeof(a->prompt_data));
        a->prompt_buf[0] = '\0';
        a->prompt_len = 0;
        a->content_edit_buf[0] = '\0';
        a->content_edit_len = 0;
        a->content_edit_cursor_pos = 0;
        a->content_edit_scroll_offset = 0;
        return 1;
    }
    a->prompt_mode = 1;
    a->prompt_is_edit = 0;
    a->prompt_step = 0;
    a->prompt_len = 0;
    a->prompt_buf[0] = '\0';
    memset(a->prompt_data, 0, sizeof(a->prompt_data));
    switch (a->current_module) {
        case MODULE_NOTES:    snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
        case MODULE_TASKS:    snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
        case MODULE_CONTACTS: /* handled above */ break;
        case MODULE_CALENDAR: snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
        case MODULE_FACTS:    snprintf(a->prompt_label, sizeof(a->prompt_label), " Key: "); break;
        case MODULE_FINANCES: snprintf(a->prompt_label, sizeof(a->prompt_label), " Date (YYYY-MM-DD): "); break;
        case MODULE_DOCUMENTS: snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: "); break;
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
        /* Contact add uses content_edit_mode form */
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
        int id = storage_facts_add(a->prompt_data[0], a->prompt_data[1]);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Fact %d added.", id);
        a->selected_index = get_item_count(MODULE_FACTS) - 1;
        return;
    }
    if (a->current_module == MODULE_FINANCES) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Description: ");
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Amount: ");
            return;
        }
        if (a->prompt_step == 2) {
            a->prompt_step = 3;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Category: ");
            return;
        }
        if (a->prompt_step == 3) {
            a->prompt_step = 4;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Account: ");
            return;
        }
        double amount = atof(a->prompt_data[2]);
        int id = storage_finances_add(a->prompt_data[0], a->prompt_data[1], amount, a->prompt_data[3], a->prompt_data[4], "");
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Finance entry %d added.", id);
        a->selected_index = get_item_count(MODULE_FINANCES) - 1;
        return;
    }
    if (a->current_module == MODULE_DOCUMENTS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Template: ");
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            a->content_edit_mode = 1;
            a->content_edit_buf[0] = '\0';
            a->content_edit_len = 0;
            a->content_edit_cursor_pos = 0;
            a->content_edit_scroll_offset = 0;
            return;
        }
        int id = storage_documents_add(a->prompt_data[0], a->prompt_data[1], a->content_edit_buf);
        a->prompt_mode = 0;
        a->content_edit_mode = 0;
        snprintf(a->message, sizeof(a->message), "Document %d added.", id);
        a->selected_index = get_item_count(MODULE_DOCUMENTS) - 1;
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
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", t.due_date);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%d", t.priority);
        snprintf(a->prompt_data[3], sizeof(a->prompt_data[3]), "%d", t.done); /* Store done status */
        return 1;
    }
    if (a->current_module == MODULE_CONTACTS) {
        VibeContact c;
        if (!storage_contact_get(id, &c)) return 0;
        a->content_edit_mode = 1;
        a->prompt_is_edit = 1;
        a->content_edit_field = 0;
        snprintf(a->prompt_data[0], sizeof(a->prompt_data[0]), "%.255s", c.name);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", c.email);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%.255s", c.phone);
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", c.name);
        a->prompt_len = (int)strlen(c.name);
        snprintf(a->content_edit_buf, sizeof(a->content_edit_buf), "%.4095s", c.notes);
        a->content_edit_len = (int)strlen(a->content_edit_buf);
        a->content_edit_cursor_pos = a->content_edit_len;
        a->content_edit_scroll_offset = 0;
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
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", e.start_at);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%.255s", e.end_at);
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
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", f.value);
        return 1;
    }
    if (a->current_module == MODULE_FINANCES) {
        VibeFinanceEntry fe;
        if (!storage_finance_get(id, &fe)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Date (YYYY-MM-DD): ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", fe.date);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", fe.description);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%.2f", fe.amount);
        snprintf(a->prompt_data[3], sizeof(a->prompt_data[3]), "%.255s", fe.category);
        snprintf(a->prompt_data[4], sizeof(a->prompt_data[4]), "%.255s", fe.account);
        return 1;
    }
    if (a->current_module == MODULE_DOCUMENTS) {
        VibeDocument d;
        if (!storage_document_get(id, &d)) return 0;
        a->prompt_mode = 1;
        a->prompt_step = 0;
        snprintf(a->prompt_label, sizeof(a->prompt_label), " Title: ");
        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", d.title);
        a->prompt_len = (int)strlen(a->prompt_buf);
        snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", d.template_name);
        snprintf(a->prompt_data[2], sizeof(a->prompt_data[2]), "%.255s", d.content);
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
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Priority (0-3): ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[2]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 2) {
            a->prompt_step = 3;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Done (0=no, 1=yes): ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[3]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        int prio = atoi(a->prompt_data[2]);
        if (prio < 0 || prio > 3) prio = 0;
        int done = atoi(a->prompt_buf);
        if (done != 0 && done != 1) done = 0;
        storage_tasks_update(id, a->prompt_data[0], a->prompt_data[1], prio, done);
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Task updated.");
        return;
    }
    if (a->current_module == MODULE_CONTACTS) {
        /* Contact edit uses content_edit_mode form */
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
    if (a->current_module == MODULE_FINANCES) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Description: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[1]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Amount: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[2]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 2) {
            a->prompt_step = 3;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Category: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[3]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 3) {
            a->prompt_step = 4;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Account: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[4]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        double amount = atof(a->prompt_data[2]);
        storage_finances_update(id, a->prompt_data[0], a->prompt_data[1], amount, a->prompt_data[3], a->prompt_data[4], "");
        a->prompt_mode = 0;
        snprintf(a->message, sizeof(a->message), "Finance entry updated.");
        return;
    }
    if (a->current_module == MODULE_DOCUMENTS) {
        if (a->prompt_step == 0) {
            a->prompt_step = 1;
            snprintf(a->prompt_label, sizeof(a->prompt_label), " Template: ");
            snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[1]);
            a->prompt_len = (int)strlen(a->prompt_buf);
            return;
        }
        if (a->prompt_step == 1) {
            a->prompt_step = 2;
            a->content_edit_mode = 1;
            if (a->prompt_is_edit) {
                VibeDocument d;
                if (storage_document_get(id, &d))
                    snprintf(a->content_edit_buf, sizeof(a->content_edit_buf), "%.4095s", d.content);
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
        storage_documents_update(id, a->prompt_data[0], a->prompt_data[1], a->content_edit_buf);
        a->prompt_mode = 0;
        a->content_edit_mode = 0;
        snprintf(a->message, sizeof(a->message), "Document updated.");
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
        case MODULE_FINANCES: storage_finances_delete(id); break;
        case MODULE_DOCUMENTS: storage_documents_delete(id); break;
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
        /* Contact form: Name, Email, Phone, Notes in a box */
        if (a->current_module == MODULE_CONTACTS) {
            if (key == KEY_ESC) {
                a->content_edit_mode = 0;
                a->prompt_mode = 0;
                a->message[0] = '\0';
                return;
            }
            if (key == KEY_TAB) {
                /* Save current field, move to next */
                if (a->content_edit_field <= 2) {
                    snprintf(a->prompt_data[a->content_edit_field], sizeof(a->prompt_data[0]), "%.255s", a->prompt_buf);
                    a->content_edit_field++;
                    if (a->content_edit_field <= 2) {
                        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[a->content_edit_field]);
                        a->prompt_len = (int)strlen(a->prompt_buf);
                    }
                }
                return;
            }
            if (key == KEY_BACKTAB) {
                if (a->content_edit_field > 0) {
                    if (a->content_edit_field <= 2)
                        snprintf(a->prompt_data[a->content_edit_field], sizeof(a->prompt_data[0]), "%.255s", a->prompt_buf);
                    a->content_edit_field--;
                    if (a->content_edit_field <= 2) {
                        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[a->content_edit_field]);
                        a->prompt_len = (int)strlen(a->prompt_buf);
                    }
                }
                return;
            }
            if (a->content_edit_field <= 2) {
                /* Single-line field editing */
                if (key == KEY_ENTER || key == '\n' || key == '\r') {
                    /* Enter = move to next field (like Tab) */
                    snprintf(a->prompt_data[a->content_edit_field], sizeof(a->prompt_data[0]), "%.255s", a->prompt_buf);
                    a->content_edit_field++;
                    if (a->content_edit_field <= 2) {
                        snprintf(a->prompt_buf, sizeof(a->prompt_buf), "%s", a->prompt_data[a->content_edit_field]);
                        a->prompt_len = (int)strlen(a->prompt_buf);
                    }
                    return;
                }
                if (key == KEY_BACKSPACE || key == 0x08) {
                    if (a->prompt_len > 0) {
                        int plen = (int)strlen(a->prompt_buf);
                        memmove(a->prompt_buf + a->prompt_len - 1, a->prompt_buf + a->prompt_len, plen - a->prompt_len + 1);
                        a->prompt_len--;
                    }
                    return;
                }
                if (key == KEY_LEFT && a->prompt_len > 0) { a->prompt_len--; return; }
                if (key == KEY_RIGHT && a->prompt_len < (int)strlen(a->prompt_buf)) { a->prompt_len++; return; }
                if (key >= 32 && key < 127 && (int)strlen(a->prompt_buf) < 255) {
                    int plen = (int)strlen(a->prompt_buf);
                    memmove(a->prompt_buf + a->prompt_len + 1, a->prompt_buf + a->prompt_len, plen - a->prompt_len + 1);
                    a->prompt_buf[a->prompt_len++] = (char)key;
                    a->prompt_buf[a->prompt_len] = '\0';
                    return;
                }
                return;
            }
            /* content_edit_field == 3: Notes multiline */
            if (key == KEY_ENTER || key == '\n' || key == '\r') {
                int pos = a->content_edit_cursor_pos;
                if (pos < 0) pos = 0;
                if (pos > a->content_edit_len) pos = a->content_edit_len;
                int is_double_enter = (pos > 0 && a->content_edit_buf[pos - 1] == '\n');
                if (is_double_enter) {
                    /* Save contact */
                    int id = get_selected_id(a);
                    if (a->prompt_is_edit && id) {
                        storage_contacts_update(id, a->prompt_data[0], a->prompt_data[1], a->prompt_data[2], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Contact updated.");
                    } else {
                        id = storage_contacts_add(a->prompt_data[0], a->prompt_data[1], a->prompt_data[2], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Contact %d added.", id);
                        a->selected_index = get_item_count(MODULE_CONTACTS) - 1;
                    }
                    a->content_edit_mode = 0;
                    a->prompt_mode = 0;
                    return;
                }
                if (a->content_edit_len < CONTENT_EDIT_BUF_MAX - 1) {
                    memmove(a->content_edit_buf + pos + 1, a->content_edit_buf + pos, a->content_edit_len - pos);
                    a->content_edit_buf[pos] = '\n';
                    a->content_edit_len++;
                    a->content_edit_buf[a->content_edit_len] = '\0';
                    a->content_edit_cursor_pos = pos + 1;
                    int content_rows = CONTACT_FORM_HEIGHT - 7;
                    if (content_rows < 1) content_rows = 1;
                    ensure_cursor_visible(a, content_rows);
                }
                return;
            }
            /* Notes field: content editor keys */
            {
                int pos = a->content_edit_cursor_pos;
                if (pos < 0) pos = 0;
                if (pos > a->content_edit_len) pos = a->content_edit_len;
                int content_rows = CONTACT_FORM_HEIGHT - 7;
                if (content_rows < 1) content_rows = 1;
#ifdef KEY_PPAGE
                if (key == KEY_PPAGE) {
                    a->content_edit_scroll_offset -= content_rows;
                    if (a->content_edit_scroll_offset < 0) a->content_edit_scroll_offset = 0;
                    ensure_cursor_visible(a, content_rows);
                    return;
                }
#endif
#ifdef KEY_NPAGE
                if (key == KEY_NPAGE) {
                    a->content_edit_scroll_offset += content_rows;
                    int max_scroll = count_content_lines(a) - content_rows;
                    if (max_scroll < 0) max_scroll = 0;
                    if (a->content_edit_scroll_offset > max_scroll) a->content_edit_scroll_offset = max_scroll;
                    ensure_cursor_visible(a, content_rows);
                    return;
                }
#endif
                if (key == KEY_LEFT) { if (pos > 0) a->content_edit_cursor_pos = pos - 1; return; }
                if (key == KEY_RIGHT) { if (pos < a->content_edit_len) a->content_edit_cursor_pos = pos + 1; return; }
                if (key == KEY_HOME) {
                    while (pos > 0 && a->content_edit_buf[pos - 1] != '\n') pos--;
                    a->content_edit_cursor_pos = pos;
                    return;
                }
                if (key == KEY_END) {
                    while (pos < a->content_edit_len && a->content_edit_buf[pos] != '\n') pos++;
                    a->content_edit_cursor_pos = pos;
                    return;
                }
                if (key == KEY_UP) {
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
                        a->content_edit_cursor_pos = 0;
                    }
                    ensure_cursor_visible(a, content_rows);
                    return;
                }
                if (key == KEY_DOWN) {
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
                        a->content_edit_cursor_pos = a->content_edit_len;
                    }
                    ensure_cursor_visible(a, content_rows);
                    return;
                }
                if (key == KEY_BACKSPACE || key == 0x08) {
                    if (pos > 0) {
                        memmove(a->content_edit_buf + pos - 1, a->content_edit_buf + pos, a->content_edit_len - pos);
                        a->content_edit_len--;
                        a->content_edit_buf[a->content_edit_len] = '\0';
                        a->content_edit_cursor_pos = pos - 1;
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
                    ensure_cursor_visible(a, content_rows);
                    return;
                }
            }
            return;
        } else {
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
                /* Two blank lines in a row - save the note or document */
                snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", a->content_edit_buf);
                if (a->prompt_is_edit) {
                    int id = get_selected_id(a);
                    if (id) {
                        if (a->current_module == MODULE_NOTES) {
                            storage_notes_update(id, a->prompt_data[0], a->content_edit_buf);
                            snprintf(a->message, sizeof(a->message), "Note updated.");
                        } else if (a->current_module == MODULE_DOCUMENTS) {
                            storage_documents_update(id, a->prompt_data[0], a->prompt_data[1], a->content_edit_buf);
                            snprintf(a->message, sizeof(a->message), "Document updated.");
                        }
                    }
                } else {
                    if (a->current_module == MODULE_NOTES) {
                        int id = storage_notes_add(a->prompt_data[0], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Note %d added.", id);
                        a->selected_index = get_item_count(MODULE_NOTES) - 1;
                    } else if (a->current_module == MODULE_DOCUMENTS) {
                        int id = storage_documents_add(a->prompt_data[0], a->prompt_data[1], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Document %d added.", id);
                        a->selected_index = get_item_count(MODULE_DOCUMENTS) - 1;
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
                /* Two blank lines in a row - save the note or document */
                snprintf(a->prompt_data[1], sizeof(a->prompt_data[1]), "%.255s", a->content_edit_buf);
                if (a->prompt_is_edit) {
                    int id = get_selected_id(a);
                    if (id) {
                        if (a->current_module == MODULE_NOTES) {
                            storage_notes_update(id, a->prompt_data[0], a->content_edit_buf);
                            snprintf(a->message, sizeof(a->message), "Note updated.");
                        } else if (a->current_module == MODULE_DOCUMENTS) {
                            storage_documents_update(id, a->prompt_data[0], a->prompt_data[1], a->content_edit_buf);
                            snprintf(a->message, sizeof(a->message), "Document updated.");
                        }
                    }
                } else {
                    if (a->current_module == MODULE_NOTES) {
                        int id = storage_notes_add(a->prompt_data[0], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Note %d added.", id);
                        a->selected_index = get_item_count(MODULE_NOTES) - 1;
                    } else if (a->current_module == MODULE_DOCUMENTS) {
                        int id = storage_documents_add(a->prompt_data[0], a->prompt_data[1], a->content_edit_buf);
                        snprintf(a->message, sizeof(a->message), "Document %d added.", id);
                        a->selected_index = get_item_count(MODULE_DOCUMENTS) - 1;
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
    }

    if (a->search_mode) {
        if (key == KEY_ESC) {
            /* Cancel search - clear filter */
            a->search_mode = 0;
            a->search_query[0] = '\0';
            a->search_len = 0;
            a->selected_index = 0;
            a->message[0] = '\0';
            return;
        }
        if (key == KEY_ENTER || key == '\n' || key == '\r') {
            /* Apply search filter - exit search input but keep filter active */
            a->search_mode = 0;
            a->selected_index = 0;
            if (a->search_query[0]) {
                snprintf(a->message, sizeof(a->message), "Filter active. Press F5 to clear.");
            } else {
                snprintf(a->message, sizeof(a->message), "Filter cleared.");
            }
            return;
        }
        if (key == KEY_BACKSPACE || key == 0x08) {
            if (a->search_len > 0) {
                a->search_len--;
                a->search_query[a->search_len] = '\0';
                a->selected_index = 0; /* Reset selection when search changes */
            }
            return;
        }
        if (key >= 32 && key < 127 && a->search_len < 255) {
            a->search_query[a->search_len++] = (char)key;
            a->search_query[a->search_len] = '\0';
            a->selected_index = 0; /* Reset selection when search changes */
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
    if (key == KEY_F(5) || c == '/') {
        /* Toggle search mode or clear filter */
        if (a->current_module != MODULE_TRASH) {
            if (a->search_query[0] && !a->search_mode) {
                /* Filter is active but not in search input mode - clear it */
                a->search_query[0] = '\0';
                a->search_len = 0;
                a->selected_index = 0;
                snprintf(a->message, sizeof(a->message), "Filter cleared.");
            } else {
                /* Start search mode */
                a->search_mode = 1;
                a->selected_index = 0;
                /* Keep existing query if there is one, user can edit it */
            }
        }
        return;
    }
    /* Trash module special handlers */
    if (a->current_module == MODULE_TRASH && !a->focus_sidebar) {
        /* Handle confirmation mode */
        if (trash_confirm_mode) {
            if (c == 'y' || c == 'Y' || key == KEY_ENTER || key == '\n' || key == '\r') {
                /* Confirm deletion */
                int deleted = 0;
                /* Delete selected items */
                for (int i = trash_items_count - 1; i >= 0; i--) {
                    if (trash_selected[i]) {
                        if (storage_permanent_delete(trash_items[i].entity_type, trash_items[i].id)) {
                            deleted++;
                        }
                    }
                }
                snprintf(a->message, sizeof(a->message), "Deleted %d item(s) permanently.", deleted);
                trash_confirm_mode = 0;
                memset(trash_selected, 0, sizeof(trash_selected));
                return;
            } else if (c == 'n' || c == 'N' || key == KEY_ESC) {
                /* Cancel */
                trash_confirm_mode = 0;
                snprintf(a->message, sizeof(a->message), "Cancelled.");
                return;
            }
            return; /* Ignore other keys in confirm mode */
        }
        
        /* Normal trash mode handlers */
        if ((c == 'r' || c == 'R') && !trash_confirm_mode) {
            if (a->selected_index >= 0 && a->selected_index < trash_items_count) {
                int entity_type = trash_items[a->selected_index].entity_type;
                int id = trash_items[a->selected_index].id;
                if (storage_restore(entity_type, id)) {
                    snprintf(a->message, sizeof(a->message), "Restored.");
                    if (a->selected_index >= get_item_count(MODULE_TRASH) && a->selected_index > 0)
                        a->selected_index--;
                } else {
                    snprintf(a->message, sizeof(a->message), "Restore failed.");
                }
            }
            return;
        }
        
        if (key == ' ' && !trash_confirm_mode) {
            /* Toggle checkbox */
            if (a->selected_index >= 0 && a->selected_index < trash_items_count) {
                trash_selected[a->selected_index] = !trash_selected[a->selected_index];
            }
            return;
        }
        
        if ((c == 'a' || c == 'A') && !trash_confirm_mode) {
            /* Select all */
            for (int i = 0; i < trash_items_count; i++) {
                trash_selected[i] = 1;
            }
            snprintf(a->message, sizeof(a->message), "All items selected.");
            return;
        }
        
        if ((c == 'u' || c == 'U') && !trash_confirm_mode) {
            /* Unselect all */
            memset(trash_selected, 0, sizeof(trash_selected));
            snprintf(a->message, sizeof(a->message), "All items unselected.");
            return;
        }
        
        if ((c == 'x' || c == 'X') && !trash_confirm_mode) {
            /* Delete selected - show confirmation */
            int selected_count = 0;
            for (int i = 0; i < trash_items_count; i++) {
                if (trash_selected[i]) selected_count++;
            }
            if (selected_count > 0) {
                trash_confirm_mode = 1;
            } else {
                snprintf(a->message, sizeof(a->message), "No items selected.");
            }
            return;
        }
        
    }

    if (a->focus_sidebar) {
        if (key == KEY_UP || key == 'k') {
            if (a->current_module > 0) {
                a->current_module--;
                a->selected_index = 0;
                if (a->current_module == MODULE_TRASH) {
                    memset(trash_selected, 0, sizeof(trash_selected));
                    trash_confirm_mode = 0;
                }
                if (a->current_module == MODULE_NOTES && get_item_count(MODULE_NOTES) > 0)
                    a->focus_sidebar = 0;
            }
            return;
        }
        if (key == KEY_DOWN || key == 'j') {
            if (a->current_module < MODULE_COUNT - 1) {
                a->current_module++;
                a->selected_index = 0;
                if (a->current_module == MODULE_TRASH) {
                    memset(trash_selected, 0, sizeof(trash_selected));
                    trash_confirm_mode = 0;
                }
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
        /* Contacts: Left/Right move within grid before potentially going to sidebar */
        if (a->current_module == MODULE_CONTACTS) {
            const char *fq = a->search_query[0] ? a->search_query : NULL;
            int count = get_item_count_with_filter(MODULE_CONTACTS, fq);
            if (key == KEY_LEFT && a->selected_index > 0) {
                a->selected_index--;
                a->message[0] = '\0';
                return;
            }
            if (key == KEY_RIGHT && a->selected_index < count - 1) {
                a->selected_index++;
                a->message[0] = '\0';
                return;
            }
        }
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
        if (key == ' ' && a->current_module == MODULE_TASKS && !a->prompt_mode && !a->search_mode && !a->content_edit_mode) {
            /* Toggle task completion */
            int id = get_selected_id(a);
            if (id > 0) {
                VibeTask t;
                if (storage_task_get(id, &t)) {
                    int new_done = !t.done;
                    storage_tasks_update(id, t.title, t.due_date, t.priority, new_done);
                    snprintf(a->message, sizeof(a->message), new_done ? "Task marked complete." : "Task marked incomplete.");
                }
            }
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
        if (a->current_module == MODULE_CONTACTS)
            draw_contact_form_editor(a);
        else
            draw_content_editor(a);
        draw_status_bar(a);
    } else {
        draw_sidebar(a);
        draw_main(a);
        if (a->search_mode) {
            draw_search_prompt(a);
        } else if (a->prompt_mode) {
            draw_prompt(a);
        } else {
            draw_status_bar(a);
        }
    }
    tui_refresh();
}
