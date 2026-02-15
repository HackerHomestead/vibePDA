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
    snprintf(buf, sizeof(buf), "%s/notes.txt", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/tasks.txt", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/contacts.txt", path); (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/events.txt", path);   (void)unlink(buf);
    (void)rmdir(path);
#else
    (void)path;
#endif
}

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
        assert(storage_contacts_update(contact_id, "Updated Name", "new@email.com", "555-9999"));
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

    if (strcmp(data_dir, ".") != 0)
        remove_test_dir(data_dir);
}
