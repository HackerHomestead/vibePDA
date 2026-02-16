/* test_fuzz.c - Sanity/fuzz tests for storage and input handling.
 *
 * Tests boundary conditions: very long strings, empty strings, special chars,
 * negative/zero IDs, numeric overflow, etc. Designed to expose potential
 * buffer overflows, truncation bugs, and crashes.
 */

#include "storage.h"
#include "types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif

static char *make_long_string(int len, char fill) {
    char *s = malloc(len + 1);
    if (!s) return NULL;
    memset(s, fill, len);
    s[len] = '\0';
    return s;
}

static void test_long_strings(const char *data_dir) {
    storage_init(data_dir);

    /* At max length - should truncate via copy_str, not crash */
    char *title_max = make_long_string(VIBE_TITLE_MAX - 1, 'A');
    char *content_max = make_long_string(VIBE_CONTENT_MAX - 1, 'B');
    assert(title_max && content_max);

    int id = storage_notes_add(title_max, content_max);
    assert(id > 0);
    if (id > 0) {
        VibeNote n;
        assert(storage_note_get(id, &n));
        assert(n.title[VIBE_TITLE_MAX - 1] == '\0');
        assert(n.content[VIBE_CONTENT_MAX - 1] == '\0');
        storage_notes_delete(id);
    }
    free(title_max);
    free(content_max);

    /* Over max - should truncate via copy_str */
    char *title_over = make_long_string(VIBE_TITLE_MAX + 100, 'X');
    char *content_over = make_long_string(VIBE_CONTENT_MAX + 500, 'Y');
    assert(title_over && content_over);

    id = storage_notes_add(title_over, content_over);
    assert(id > 0);
    if (id > 0) {
        VibeNote n;
        assert(storage_note_get(id, &n));
        assert(strlen(n.title) < VIBE_TITLE_MAX);
        assert(strlen(n.content) < VIBE_CONTENT_MAX);
        storage_notes_delete(id);
    }
    free(title_over);
    free(content_over);

    /* Facts: key and value at max */
    char *key_max = make_long_string(VIBE_TITLE_MAX - 1, 'K');
    char *val_max = make_long_string(VIBE_CONTENT_MAX - 1, 'V');
    assert(key_max && val_max);
    id = storage_facts_add(key_max, val_max);
    assert(id > 0);
    if (id > 0) {
        VibeFact f;
        assert(storage_fact_get(id, &f));
        storage_facts_delete(id);
    }
    free(key_max);
    free(val_max);
}

static void test_empty_and_null_strings(const char *data_dir) {
    storage_init(data_dir);

    /* NULL and empty - should not crash */
    int id = storage_notes_add(NULL, NULL);
    assert(id > 0);
    if (id > 0) {
        VibeNote n;
        assert(storage_note_get(id, &n));
        assert(n.title[0] == '\0');
        assert(n.content[0] == '\0');
        storage_notes_delete(id);
    }

    id = storage_notes_add("", "");
    assert(id > 0);
    storage_notes_delete(id);

    id = storage_tasks_add("", NULL, 0);
    assert(id > 0);
    storage_tasks_delete(id);

    id = storage_tasks_add("title", "", 0);
    assert(id > 0);
    storage_tasks_delete(id);

    id = storage_facts_add("", "");
    assert(id > 0);
    storage_facts_delete(id);

    id = storage_contacts_add("", NULL, NULL, NULL);
    assert(id > 0);
    storage_contacts_delete(id);
}

static void test_special_characters(const char *data_dir) {
    storage_init(data_dir);

    /* Tabs and newlines - storage format supports them */
    const char *with_tabs = "title\twith\ttabs";
    const char *with_newlines = "line1\nline2\nline3";
    int id = storage_notes_add(with_tabs, with_newlines);
    assert(id > 0);
    if (id > 0) {
        VibeNote n;
        assert(storage_note_get(id, &n));
        assert(strstr(n.title, "\t") != NULL);
        assert(strstr(n.content, "\n") != NULL);
        storage_notes_delete(id);
    }

    /* High ASCII / potential UTF-8 */
    const char *utf8 = "caf\xc3\xa9";  /* cafe with acute e */
    id = storage_notes_add(utf8, utf8);
    assert(id > 0);
    if (id > 0) storage_notes_delete(id);
}

static void test_numeric_boundaries(const char *data_dir) {
    storage_init(data_dir);

    /* Priority 0, negative, large - storage uses int */
    int id = storage_tasks_add("p0", "2026-01-01", 0);
    assert(id > 0);
    if (id > 0) {
        VibeTask t;
        assert(storage_task_get(id, &t));
        assert(t.priority == 0);
        storage_tasks_delete(id);
    }

    id = storage_tasks_add("p3", "2026-01-01", 3);
    assert(id > 0);
    if (id > 0) {
        VibeTask t;
        assert(storage_task_get(id, &t));
        assert(t.priority == 3);
        storage_tasks_delete(id);
    }

    /* Finance amount: zero, negative, very large */
    id = storage_finances_add("2026-01-01", "zero", 0.0, "cat", "acc", "");
    assert(id > 0);
    if (id > 0) {
        VibeFinanceEntry fe;
        assert(storage_finance_get(id, &fe));
        assert(fe.amount == 0.0);
        storage_finances_delete(id);
    }

    id = storage_finances_add("2026-01-01", "negative", -999999.99, "cat", "acc", "");
    assert(id > 0);
    if (id > 0) {
        VibeFinanceEntry fe;
        assert(storage_finance_get(id, &fe));
        assert(fe.amount < 0);
        storage_finances_delete(id);
    }

    id = storage_finances_add("2026-01-01", "large", 1e20, "cat", "acc", "");
    assert(id > 0);
    if (id > 0) storage_finances_delete(id);
}

static void test_get_nonexistent_id(const char *data_dir) {
    storage_init(data_dir);

    VibeNote n = {0};
    VibeTask t = {0};
    VibeContact c = {0};
    VibeFact f = {0};

    assert(!storage_note_get(0, &n));
    assert(!storage_note_get(-1, &n));
    assert(!storage_note_get(999999, &n));
    assert(!storage_note_get(999999, NULL));  /* NULL out - should not crash */

    assert(!storage_task_get(0, &t));
    assert(!storage_contact_get(-1, &c));
    assert(!storage_fact_get(999999, &f));
}

static void test_filter_empty_and_long_query(const char *data_dir) {
    storage_init(data_dir);
    int id = storage_notes_add("test", "content");
    assert(id > 0);

    int count = 0;
    void count_cb(const VibeNote *n, void *ctx) { (void)n; (*(int *)ctx)++; }
    storage_notes_list_filtered(count_cb, &count, NULL);
    assert(count == 1);
    count = 0;
    storage_notes_list_filtered(count_cb, &count, "");
    assert(count == 1);

    /* Very long query - should not crash */
    char *long_q = make_long_string(1000, 'x');
    assert(long_q);
    count = 0;
    storage_notes_list_filtered(count_cb, &count, long_q);
    assert(count == 0);
    free(long_q);

    storage_notes_delete(id);
}

#ifdef PLATFORM_LINUX
/* Regression: malicious/corrupt length prefix (0xFFFFFFFF) must not cause overflow */
static void test_malicious_length_prefix(const char *data_dir) {
    storage_init(data_dir);

    char path[512];
    snprintf(path, sizeof(path), "%s/notes.bin", data_dir);
    FILE *f = fopen(path, "wb");
    assert(f);
    /* Write id=1, then length=0xFFFFFFFF for title - read_str should cap and not overflow */
    { unsigned char id4[] = { 1, 0, 0, 0 }; fwrite(id4, 1, 4, f); }
    { unsigned char len4[] = { 0xff, 0xff, 0xff, 0xff }; fwrite(len4, 1, 4, f); }
    fclose(f);

    storage_init(data_dir);
    /* Should not crash; may return 0 or 1 depending on how much we skip */
    (void)storage_notes_count();
    {
        int count = 0;
        void count_cb(const VibeNote *n, void *ctx) { (void)n; (*(int *)ctx)++; }
        storage_notes_list(count_cb, &count);
        assert(count >= 0 && count <= 1);  /* Graceful handling */
    }
}
#endif

static void remove_test_dir(const char *path) {
#ifdef PLATFORM_LINUX
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/notes.bin", path);     (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/tasks.bin", path);     (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/contacts.bin", path); (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/events.bin", path);   (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/facts.bin", path);    (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/finances.bin", path); (void)unlink(buf);
    snprintf(buf, sizeof(buf), "%s/documents.bin", path); (void)unlink(buf);
    (void)rmdir(path);
#else
    (void)path;
#endif
}

void test_fuzz(void) {
    char data_dir[256];
#ifdef PLATFORM_LINUX
    snprintf(data_dir, sizeof(data_dir), "/tmp/vibe_fuzz_XXXXXX");
    if (!mkdtemp(data_dir)) {
        fprintf(stderr, "test_fuzz: mkdtemp failed, using .\n");
        snprintf(data_dir, sizeof(data_dir), ".");
    }
#else
    snprintf(data_dir, sizeof(data_dir), ".");
#endif

    test_long_strings(data_dir);
    test_empty_and_null_strings(data_dir);
    test_special_characters(data_dir);
    test_numeric_boundaries(data_dir);
    test_get_nonexistent_id(data_dir);
    test_filter_empty_and_long_query(data_dir);

#ifdef PLATFORM_LINUX
    test_malicious_length_prefix(data_dir);
#endif

    if (strcmp(data_dir, ".") != 0)
        remove_test_dir(data_dir);
}
