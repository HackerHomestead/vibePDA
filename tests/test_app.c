/* test_app.c - Unit tests for app state and key handling (1980s TUI) */

#include "app.h"
#include "storage.h"
#include "test_common.h"
#include "tui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include <stdlib.h>
#endif

void test_app(void) {
    AppState a;

    if (test_verbose()) fprintf(stderr, "    app_init\n");
    app_init(&a, 24, 80);
    assert(a.current_module == 0);
    assert(a.focus_sidebar == 1);
    assert(a.quit_requested == 0);
    assert(a.show_help == 0);
    assert(a.prompt_mode == 0);

    if (test_verbose()) fprintf(stderr, "    module_switch\n");
    /* Module switch: Up/Down in sidebar */
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_TASKS);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_CONTACTS);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_CALENDAR);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_FACTS);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_FINANCES);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_DOCUMENTS);
    app_handle_key(&a, KEY_DOWN);
    assert(a.current_module == MODULE_TRASH);
    app_handle_key(&a, KEY_UP);
    assert(a.current_module == MODULE_DOCUMENTS);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    app_handle_key(&a, KEY_UP);
    assert(a.current_module == MODULE_NOTES);

    if (test_verbose()) fprintf(stderr, "    focus_tab\n");
    /* Focus: Tab switches to main pane */
    app_handle_key(&a, KEY_TAB);
    assert(a.focus_sidebar == 0);
    app_handle_key(&a, KEY_BACKTAB);
    assert(a.focus_sidebar == 1);

    if (test_verbose()) fprintf(stderr, "    quit\n");
    /* Quit */
    app_init(&a, 24, 80);
    app_handle_key(&a, 'q');
    assert(a.quit_requested == 1);

    if (test_verbose()) fprintf(stderr, "    help\n");
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

    if (test_verbose()) fprintf(stderr, "    f2_new_prompt\n");
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

    if (test_verbose()) fprintf(stderr, "    content_editor\n");
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

    if (test_verbose()) fprintf(stderr, "    trash_no_new\n");
    /* Trash module: F2/N should not start prompt */
    app_init(&a, 24, 80);
    /* Navigate to Trash module */
    for (int i = 0; i < MODULE_TRASH; i++) {
        app_handle_key(&a, KEY_DOWN);
    }
    assert(a.current_module == MODULE_TRASH);
    app_handle_key(&a, 'n'); /* Try to start new prompt */
    assert(a.prompt_mode == 0); /* Should not start prompt in Trash */
    app_handle_key(&a, KEY_F(2)); /* Try F2 */
    assert(a.prompt_mode == 0); /* Should not start prompt in Trash */
}

/* Integration test for trash functionality with storage */
void test_trash_integration(void) {
    if (test_verbose()) fprintf(stderr, "    trash_integration (create, list, restore, permanent delete, empty)\n");
    char data_dir[256];
#ifdef PLATFORM_LINUX
    snprintf(data_dir, sizeof(data_dir), "/tmp/vibe_test_trash_XXXXXX");
    if (!mkdtemp(data_dir)) {
        fprintf(stderr, "test_trash_integration: mkdtemp failed, using .\n");
        snprintf(data_dir, sizeof(data_dir), ".");
    }
#else
    snprintf(data_dir, sizeof(data_dir), ".");
#endif

    storage_init(data_dir);
    
    /* Create some items and delete them */
    int note_id = storage_notes_add("Test Note", "Test content");
    assert(note_id > 0);
    int task_id = storage_tasks_add("Test Task", "2026-12-31", 1);
    assert(task_id > 0);
    int fact_id = storage_facts_add("test_key", "test_value");
    assert(fact_id > 0);
    
    /* Delete them to put in trash */
    assert(storage_notes_delete(note_id));
    assert(storage_tasks_delete(task_id));
    assert(storage_facts_delete(fact_id));
    
    /* Verify trash count */
    assert(storage_trash_count() >= 3);
    
    /* Test trash listing */
    typedef struct {
        int count;
        int found_note;
        int found_task;
        int found_fact;
    } TrashTestCtx;
    
    TrashTestCtx trash_ctx = {0, 0, 0, 0};
    
    void count_trash(int entity_type, int id, const char *title, void *ctx) {
        TrashTestCtx *c = (TrashTestCtx *)ctx;
        c->count++;
        if (entity_type == 0 && id == note_id) c->found_note = 1;
        if (entity_type == 1 && id == task_id) c->found_task = 1;
        if (entity_type == 4 && id == fact_id) c->found_fact = 1;
        (void)title;
    }
    
    storage_trash_list(count_trash, &trash_ctx);
    assert(trash_ctx.count >= 3);
    assert(trash_ctx.found_note == 1);
    assert(trash_ctx.found_task == 1);
    assert(trash_ctx.found_fact == 1);
    
    /* Test restore */
    int trash_before_restore = storage_trash_count();
    assert(storage_restore(0, note_id)); /* Restore note */
    assert(storage_trash_count() == trash_before_restore - 1);
    
    /* Verify restored note can be retrieved */
    VibeNote n;
    assert(storage_note_get(note_id, &n));
    assert(n.id == note_id);
    assert(strcmp(n.title, "Test Note") == 0);
    
    /* Delete it again */
    assert(storage_notes_delete(note_id));
    
    /* Test permanent delete */
    int trash_before_permanent = storage_trash_count();
    assert(storage_permanent_delete(4, fact_id)); /* Permanently delete fact */
    assert(storage_trash_count() == trash_before_permanent - 1);
    
    /* Verify it's gone from trash */
    int found_fact = 0;
    void check_fact(int entity_type, int id, const char *title, void *ctx) {
        if (entity_type == 4 && id == fact_id) {
            *(int *)ctx = 1;
        }
        (void)title;
    }
    storage_trash_list(check_fact, &found_fact);
    assert(found_fact == 0);
    
    /* Test empty trash */
    int trash_before_empty = storage_trash_count();
    if (trash_before_empty > 0) {
        int deleted = storage_empty_trash();
        assert(deleted >= 0);
        assert(storage_trash_count() == 0);
    }
    
    /* Cleanup */
#ifdef PLATFORM_LINUX
    if (strcmp(data_dir, ".") != 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/notes.bin", data_dir);    (void)unlink(buf);
        snprintf(buf, sizeof(buf), "%s/tasks.bin", data_dir);    (void)unlink(buf);
        snprintf(buf, sizeof(buf), "%s/contacts.bin", data_dir); (void)unlink(buf);
        snprintf(buf, sizeof(buf), "%s/events.bin", data_dir);   (void)unlink(buf);
        snprintf(buf, sizeof(buf), "%s/facts.bin", data_dir);    (void)unlink(buf);
        (void)rmdir(data_dir);
    }
#endif
}
