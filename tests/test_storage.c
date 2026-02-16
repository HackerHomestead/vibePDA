/* test_storage.c - Unit tests for storage backend */

#include "fixture_parks.h"
#include "storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif

static void test_storage_empty(const char *data_dir) {
    storage_init(data_dir);
    assert(storage_notes_count() == 0);
    assert(storage_tasks_count() == 0);
    assert(storage_contacts_count() == 0);
    assert(storage_events_count() == 0);
    assert(storage_trash_count() == 0);
}

static void remove_test_dir(const char *path) {
#ifdef PLATFORM_LINUX
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/notes.bin", path);     (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/notes.txt", path);     (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/tasks.bin", path);     (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/tasks.txt", path);     (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/contacts.bin", path); (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/contacts.txt", path); (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/events.bin", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/events.txt", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/facts.bin", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/facts.txt", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/finances.bin", path); (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/documents.bin", path); (void)unlink(buf);
    (void)rmdir(path);
#else
    (void)path;
#endif
}

#ifdef PLATFORM_LINUX
/* Migration test: create .txt files, run storage_init, verify migration to .bin */
static void test_migration(const char *data_dir) {
    char path_txt[512], path_bin[512];
    FILE *f;

    /* Create notes.txt in TSV format: id\ttitle\tcontent\tcreated_at\tdeleted_at */
    snprintf(path_txt, sizeof(path_txt), "%s/notes.txt", data_dir);
    snprintf(path_bin, sizeof(path_bin), "%s/notes.bin", data_dir);
    (void)unlink(path_bin); /* Ensure .bin does not exist */
    f = fopen(path_txt, "w");
    assert(f);
    fprintf(f, "1\tMigrated Note\tMigrated content\t2026-01-01 12:00:00\t\n");
    fprintf(f, "2\tSecond Note\tMore content\t2026-01-02 12:00:00\t\n");
    fclose(f);

    storage_init(data_dir);

    /* Verify .bin exists and .txt was removed */
    assert(access(path_bin, F_OK) == 0);
    assert(access(path_txt, F_OK) != 0);

    /* Verify migrated data is readable */
    assert(storage_notes_count() == 2);
    {
        int count = 0;
        void count_notes(const VibeNote *n, void *ctx) {
            (void)n;
            (*(int *)ctx)++;
        }
        storage_notes_list(count_notes, &count);
        assert(count == 2);
    }
    {
        VibeNote n;
        assert(storage_note_get(1, &n));
        assert(strcmp(n.title, "Migrated Note") == 0);
        assert(strcmp(n.content, "Migrated content") == 0);
        assert(storage_note_get(2, &n));
        assert(strcmp(n.title, "Second Note") == 0);
    }

    /* Create facts.txt and verify migration */
    snprintf(path_txt, sizeof(path_txt), "%s/facts.txt", data_dir);
    snprintf(path_bin, sizeof(path_bin), "%s/facts.bin", data_dir);
    (void)unlink(path_bin);
    f = fopen(path_txt, "w");
    assert(f);
    fprintf(f, "1\tmigrated_key\tmigrated_value\t2026-01-01 12:00:00\t\n");
    fclose(f);

    storage_init(data_dir); /* Re-init to trigger facts migration */

    assert(access(path_bin, F_OK) == 0);
    assert(access(path_txt, F_OK) != 0);
    assert(storage_facts_count() == 1);
    {
        VibeFact fact;
        assert(storage_fact_get(1, &fact));
        assert(strcmp(fact.key, "migrated_key") == 0);
        assert(strcmp(fact.value, "migrated_value") == 0);
    }
}

/* Corruption/edge case tests: empty file, truncated record, malformed data */
static void test_corruption_and_edge_cases(const char *data_dir) {
    char path[512];
    FILE *f;

    /* Empty file: should return 0 count and not crash */
    snprintf(path, sizeof(path), "%s/notes.bin", data_dir);
    (void)unlink(path);
    f = fopen(path, "wb");
    assert(f);
    fclose(f);

    storage_init(data_dir);
    assert(storage_notes_count() == 0);
    {
        int count = 0;
        void count_notes(const VibeNote *n, void *ctx) { (void)n; (*(int *)ctx)++; }
        storage_notes_list(count_notes, &count);
        assert(count == 0);
    }

    /* Truncated record: write only id (4 bytes), no strings - read should stop gracefully */
    f = fopen(path, "wb");
    assert(f);
    { unsigned char id4[] = { 1, 0, 0, 0 }; fwrite(id4, 1, 4, f); }
    fclose(f);

    storage_init(data_dir);
    /* Should not crash; count may be 0 (incomplete record skipped) */
    (void)storage_notes_count();
    {
        int count = 0;
        void count_notes(const VibeNote *n, void *ctx) { (void)n; (*(int *)ctx)++; }
        storage_notes_list(count_notes, &count);
        assert(count == 0); /* Incomplete record not yielded */
    }

    /* Malformed: zero-length string for title (valid), then truncated - should not crash */
    f = fopen(path, "wb");
    assert(f);
    { unsigned char id4[] = { 1, 0, 0, 0 }; fwrite(id4, 1, 4, f); }
    { unsigned char len0[] = { 0, 0, 0, 0 }; fwrite(len0, 1, 4, f); } /* empty title */
    { unsigned char len5[] = { 5, 0, 0, 0 }; fwrite(len5, 1, 4, f); }
    fwrite("hello", 1, 5, f); /* content "hello" */
    /* Omit created_at, deleted_at - truncated */
    fclose(f);

    storage_init(data_dir);
    (void)storage_notes_count(); /* Should not crash */
    {
        int count = 0;
        void count_notes(const VibeNote *n, void *ctx) { (void)n; (*(int *)ctx)++; }
        storage_notes_list(count_notes, &count);
        assert(count == 0); /* Incomplete record */
    }

    /* Valid minimal record: id + empty title + empty content + empty created + empty deleted */
    f = fopen(path, "wb");
    assert(f);
    { unsigned char id4[] = { 1, 0, 0, 0 }; fwrite(id4, 1, 4, f); }
    { unsigned char len0[] = { 0, 0, 0, 0 }; fwrite(len0, 1, 4, f); }
    { unsigned char len0b[] = { 0, 0, 0, 0 }; fwrite(len0b, 1, 4, f); }
    { unsigned char len0c[] = { 0, 0, 0, 0 }; fwrite(len0c, 1, 4, f); }
    { unsigned char len0d[] = { 0, 0, 0, 0 }; fwrite(len0d, 1, 4, f); }
    fclose(f);

    storage_init(data_dir);
    assert(storage_notes_count() == 1);
    {
        VibeNote n;
        assert(storage_note_get(1, &n));
        assert(n.id == 1);
        assert(n.title[0] == '\0');
        assert(n.content[0] == '\0');
    }
}
#endif

void test_storage(void) {
    int want_records = 100;
    const char *env = getenv("VIBE_TEST_RECORDS");
    if (env && *env)
        want_records = atoi(env);

    char data_dir[256];
#ifdef PLATFORM_LINUX
    snprintf(data_dir, sizeof(data_dir), "/tmp/vibe_test_XXXXXX");
    if (!mkdtemp(data_dir)) {
        fprintf(stderr, "test_storage: mkdtemp failed, using .\n");
        snprintf(data_dir, sizeof(data_dir), ".");
    }
#else
    snprintf(data_dir, sizeof(data_dir), ".");
#endif

    /* Empty storage */
    test_storage_empty(data_dir);

#ifdef PLATFORM_LINUX
    /* Migration and corruption tests (use separate temp dir) */
    {
        char migrate_dir[256];
        snprintf(migrate_dir, sizeof(migrate_dir), "/tmp/vibe_migrate_XXXXXX");
        if (mkdtemp(migrate_dir)) {
            test_migration(migrate_dir);
            remove_test_dir(migrate_dir);
        }
        snprintf(migrate_dir, sizeof(migrate_dir), "/tmp/vibe_corrupt_XXXXXX");
        if (mkdtemp(migrate_dir)) {
            test_corruption_and_edge_cases(migrate_dir);
            remove_test_dir(migrate_dir);
        }
    }
#endif

    if (want_records <= 0) {
        if (strcmp(data_dir, ".") != 0)
            remove_test_dir(data_dir);
        return;
    }

    /* Seed Parks and Rec themed dummy data (configurable, random). */
    fixture_seed_parks(data_dir, want_records);

    int notes = storage_notes_count();
    int tasks = storage_tasks_count();
    int contacts = storage_contacts_count();
    int events = storage_events_count();
    assert(notes + tasks + contacts + events == want_records);
    assert(notes > 0 && tasks > 0 && contacts > 0 && events > 0);

    /* Tasks: get, update, delete */
    {
        int task_id = 0;
        void capture_first_task(const VibeTask *t, void *ctx) {
            *(int *)ctx = t->id;
        }
        storage_tasks_list(capture_first_task, &task_id);
        assert(task_id > 0);
        VibeTask t;
        assert(storage_task_get(task_id, &t));
        assert(t.id == task_id);
        assert(storage_tasks_update(task_id, "Updated task", "2026-01-01", 2, 0));
        assert(storage_task_get(task_id, &t));
        assert(strcmp(t.title, "Updated task") == 0);
        assert(storage_tasks_delete(task_id));
        assert(!storage_task_get(task_id, &t));
        assert(storage_tasks_count() == tasks - 1);
    }

    /* Contacts: list, get, update, delete */
    {
        int contact_id = 0;
        void capture_first_contact(const VibeContact *c, void *ctx) {
            *(int *)ctx = c->id;
        }
        storage_contacts_list(capture_first_contact, &contact_id);
        assert(contact_id > 0);
        VibeContact c;
        assert(storage_contact_get(contact_id, &c));
        assert(c.id == contact_id);
        assert(storage_contacts_update(contact_id, "Updated Name", "new@email.com", "555-9999", ""));
        assert(storage_contact_get(contact_id, &c));
        assert(strcmp(c.name, "Updated Name") == 0);
        assert(storage_contacts_delete(contact_id));
        assert(!storage_contact_get(contact_id, &c));
        assert(storage_contacts_count() == contacts - 1);
    }

    /* Events: list, get, update, delete */
    {
        int event_id = 0;
        void capture_first_event(const VibeCalendarEvent *e, void *ctx) {
            *(int *)ctx = e->id;
        }
        storage_events_list(capture_first_event, &event_id);
        assert(event_id > 0);
        VibeCalendarEvent e;
        assert(storage_event_get(event_id, &e));
        assert(e.id == event_id);
        assert(storage_events_update(event_id, "Updated event", NULL, "2026-02-01 10:00", "2026-02-01 11:00", 0));
        assert(storage_event_get(event_id, &e));
        assert(strcmp(e.title, "Updated event") == 0);
        assert(storage_events_delete(event_id));
        assert(!storage_event_get(event_id, &e));
        assert(storage_events_count() == events - 1);
    }

    /* Trash: test soft-delete and trash listing */
    {
        int initial_trash_count = storage_trash_count();
        assert(initial_trash_count >= 3); /* Should have the 3 deleted items from above */
        
        /* Test trash_list callback */
        typedef struct {
            int count;
            int found_task;
            int found_contact;
            int found_event;
        } TrashCountCtx;
        
        TrashCountCtx trash_ctx = {0, 0, 0, 0};
        
        void count_trash_items(int entity_type, int id, const char *title, void *ctx) {
            TrashCountCtx *c = (TrashCountCtx *)ctx;
            c->count++;
            if (entity_type == 1) c->found_task = 1;
            if (entity_type == 2) c->found_contact = 1;
            if (entity_type == 3) c->found_event = 1;
            (void)id; (void)title;
        }
        
        storage_trash_list(count_trash_items, &trash_ctx);
        assert(trash_ctx.count == initial_trash_count);
        assert(trash_ctx.found_task == 1);
        assert(trash_ctx.found_contact == 1);
        assert(trash_ctx.found_event == 1);
    }

    /* Trash: test restore */
    {
        /* Find a deleted task to restore */
        int deleted_task_id = 0;
        void find_deleted_task(int entity_type, int id, const char *title, void *ctx) {
            if (entity_type == 1 && deleted_task_id == 0) {
                *(int *)ctx = id;
            }
            (void)title;
        }
        storage_trash_list(find_deleted_task, &deleted_task_id);
        
        if (deleted_task_id > 0) {
            int trash_before = storage_trash_count();
            assert(storage_restore(1, deleted_task_id)); /* Restore task */
            assert(storage_trash_count() == trash_before - 1);
            
            /* Verify it's restored (can be retrieved normally) */
            VibeTask t;
            assert(storage_task_get(deleted_task_id, &t));
            assert(t.id == deleted_task_id);
            
            /* Delete it again for cleanup */
            storage_tasks_delete(deleted_task_id);
        }
    }

    /* Trash: test permanent delete */
    {
        /* Create a fact, delete it, then permanently delete it */
        int fact_id = storage_facts_add("test_key", "test_value");
        assert(fact_id > 0);
        assert(storage_facts_delete(fact_id));
        
        int trash_before = storage_trash_count();
        assert(storage_permanent_delete(4, fact_id)); /* Permanently delete fact */
        assert(storage_trash_count() == trash_before - 1);
        
        /* Verify it's gone from trash */
        int found_in_trash = 0;
        void check_trash(int entity_type, int id, const char *title, void *ctx) {
            if (entity_type == 4 && id == fact_id) {
                *(int *)ctx = 1;
            }
            (void)title;
        }
        storage_trash_list(check_trash, &found_in_trash);
        assert(found_in_trash == 0);
    }

    /* Trash: test empty_trash */
    {
        /* Delete a few more items to have trash */
        int note_id = 0;
        void capture_first_note(const VibeNote *n, void *ctx) {
            if (*(int *)ctx == 0) *(int *)ctx = n->id;
        }
        storage_notes_list(capture_first_note, &note_id);
        if (note_id > 0) {
            storage_notes_delete(note_id);
        }
        
        int trash_before = storage_trash_count();
        if (trash_before > 0) {
            int deleted_count = storage_empty_trash();
            assert(deleted_count >= 0);
            assert(storage_trash_count() == 0);
        }
    }

    if (strcmp(data_dir, ".") != 0)
        remove_test_dir(data_dir);
}
