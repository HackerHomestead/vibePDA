/* storage_file.c - Binary file-based storage (Linux, DOS). One file per entity type.
 *
 * BINARY FORMAT (little-endian):
 *   - All multi-byte integers: uint32_t, little-endian
 *   - Strings: 4-byte length (uint32_t) + N bytes UTF-8 data (no NUL)
 *   - Supports tabs and newlines in data; no sanitization
 *
 * FILES: notes.bin, tasks.bin, contacts.bin, events.bin, facts.bin
 * MIGRATION: On Linux, .txt (TSV) files are migrated to .bin on first run.
 */

#include "config.h"
#include "storage.h"
#include "types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef PLATFORM_LINUX
# include <sys/stat.h>
# include <unistd.h>
#endif

#define DATA_DIR_MAX 256
#define LINE_MAX 8192
#define EXT ".bin"

/* Entity type codes for trash/restore (must match MODULE_* in app.h) */
#define ENTITY_NOTE     0
#define ENTITY_TASK     1
#define ENTITY_CONTACT  2
#define ENTITY_EVENT    3
#define ENTITY_FACT     4
#define ENTITY_FINANCE  5
#define ENTITY_DOCUMENT 6

static char s_data_dir[DATA_DIR_MAX];
static int s_data_dir_set;

static void timestamp(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm)
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
    else
        snprintf(buf, size, "%ld", (long)t);
}

/* Copy string into dst, truncating to max-1 chars. No sanitization (tabs/newlines OK). */
static void copy_str(char *dst, const char *src, int max) {
    if (!dst || max <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    int n = 0;
    while (src[n] && n < max - 1) { dst[n] = src[n]; n++; }
    dst[n] = '\0';
}

/* Case-insensitive substring match */
static int str_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 1;
    const char *h = haystack;
    const char *n = needle;
    while (*h) {
        const char *h_start = h;
        const char *n_start = n;
        while (*h && *n &&
               ((*h >= 'A' && *h <= 'Z' ? *h + 32 : *h) ==
                (*n >= 'A' && *n <= 'Z' ? *n + 32 : *n))) {
            h++; n++;
        }
        if (!*n) return 1;
        h = h_start + 1;
        n = n_start;
    }
    return 0;
}

/* Binary I/O helpers - little-endian */
static int write_u32(FILE *f, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    b[2] = (unsigned char)((v >> 16) & 0xff);
    b[3] = (unsigned char)((v >> 24) & 0xff);
    return fwrite(b, 1, 4, f) == 4;
}
static int write_str(FILE *f, const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len > 0x7fffffff) len = 0x7fffffff;
    if (!write_u32(f, (uint32_t)len)) return 0;
    if (len > 0 && fwrite(s, 1, len, f) != len) return 0;
    return 1;
}
static int read_u32(FILE *f, uint32_t *out) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 1;
}
static int read_str(FILE *f, char *buf, int max) {
    uint32_t len;
    if (!read_u32(f, &len)) return 0;
    if (len == 0) { buf[0] = '\0'; return 1; }
    if ((int)len >= max) {
        if (fread(buf, 1, max - 1, f) != (size_t)(max - 1)) return 0;
        buf[max - 1] = '\0';
        if (fseek(f, (long)(len - (max - 1)), SEEK_CUR) != 0) return 0;
    } else {
        if (fread(buf, 1, len, f) != len) return 0;
        buf[len] = '\0';
    }
    return 1;
}
static int skip_str(FILE *f) {
    uint32_t len;
    if (!read_u32(f, &len)) return 0;
    if (fseek(f, (long)len, SEEK_CUR) != 0) return 0;
    return 1;
}

#ifdef PLATFORM_LINUX
static void migrate_if_needed(const char *base);
#endif

void storage_init(const char *data_dir) {
    s_data_dir_set = 0;
    if (data_dir) {
        snprintf(s_data_dir, sizeof(s_data_dir), "%s", data_dir);
        s_data_dir_set = 1;
    }
#ifdef PLATFORM_LINUX
    migrate_if_needed("notes");
    migrate_if_needed("tasks");
    migrate_if_needed("contacts");
    migrate_if_needed("events");
    migrate_if_needed("facts");
#endif
}

static void data_path(char *buf, int size, const char *name) {
    if (s_data_dir_set)
        snprintf(buf, size, "%s/%s", s_data_dir, name);
    else
        snprintf(buf, size, "%s", name);
}

static void ensure_data_dir(void) {
#ifdef PLATFORM_LINUX
    if (s_data_dir_set) {
        struct stat st;
        if (stat(s_data_dir, &st) != 0)
            mkdir(s_data_dir, 0755);
    }
#endif
}

/* Migration: convert .txt (TSV) to .bin if .txt exists and .bin does not */
#ifdef PLATFORM_LINUX
static int parse_tab_field(char **p, char *buf, int max) {
    char *start = *p;
    char *q = strchr(start, '\t');
    if (q) {
        size_t len = (size_t)(q - start);
        if (len >= (size_t)max) len = max - 1;
        memcpy(buf, start, len);
        buf[len] = '\0';
        *p = q + 1;
    } else {
        q = strchr(start, '\n');
        if (q) {
            size_t len = (size_t)(q - start);
            if (len >= (size_t)max) len = max - 1;
            memcpy(buf, start, len);
            buf[len] = '\0';
            *p = q + 1;
        } else {
            snprintf(buf, max, "%s", start);
            *p = start + strlen(start);
        }
    }
    return 1;
}

static void migrate_notes_txt_to_bin(const char *path_txt, const char *path_bin) {
    FILE *in = fopen(path_txt, "r");
    if (!in) return;
    FILE *out = fopen(path_bin, "wb");
    if (!out) { fclose(in); return; }
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        char id_buf[32], title[VIBE_TITLE_MAX], content[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        parse_tab_field(&p, id_buf, sizeof(id_buf));
        parse_tab_field(&p, title, sizeof(title));
        parse_tab_field(&p, content, sizeof(content));
        parse_tab_field(&p, created, sizeof(created));
        parse_tab_field(&p, deleted, sizeof(deleted));
        int id = atoi(id_buf);
        if (id <= 0) continue;
        write_u32(out, (uint32_t)id);
        write_str(out, title);
        write_str(out, content);
        write_str(out, created);
        write_str(out, deleted[0] ? deleted : "");
    }
    fclose(in);
    fclose(out);
    unlink(path_txt);
}

static void migrate_tasks_txt_to_bin(const char *path_txt, const char *path_bin) {
    FILE *in = fopen(path_txt, "r");
    if (!in) return;
    FILE *out = fopen(path_bin, "wb");
    if (!out) { fclose(in); return; }
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        char id_buf[32], title[VIBE_TITLE_MAX], done_buf[8], due[VIBE_DATETIME_MAX], prio_buf[16], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        parse_tab_field(&p, id_buf, sizeof(id_buf));
        parse_tab_field(&p, title, sizeof(title));
        parse_tab_field(&p, done_buf, sizeof(done_buf));
        parse_tab_field(&p, due, sizeof(due));
        parse_tab_field(&p, prio_buf, sizeof(prio_buf));
        parse_tab_field(&p, created, sizeof(created));
        parse_tab_field(&p, deleted, sizeof(deleted));
        int id = atoi(id_buf);
        if (id <= 0) continue;
        write_u32(out, (uint32_t)id);
        write_str(out, title);
        write_u32(out, (uint32_t)atoi(done_buf));
        write_str(out, due);
        write_u32(out, (uint32_t)atoi(prio_buf));
        write_str(out, created);
        write_str(out, deleted[0] ? deleted : "");
    }
    fclose(in);
    fclose(out);
    unlink(path_txt);
}

static void migrate_contacts_txt_to_bin(const char *path_txt, const char *path_bin) {
    FILE *in = fopen(path_txt, "r");
    if (!in) return;
    FILE *out = fopen(path_bin, "wb");
    if (!out) { fclose(in); return; }
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        char id_buf[32], name[VIBE_NAME_MAX], email[VIBE_EMAIL_MAX], phone[VIBE_PHONE_MAX], notes[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        parse_tab_field(&p, id_buf, sizeof(id_buf));
        parse_tab_field(&p, name, sizeof(name));
        parse_tab_field(&p, email, sizeof(email));
        parse_tab_field(&p, phone, sizeof(phone));
        parse_tab_field(&p, notes, sizeof(notes));
        parse_tab_field(&p, created, sizeof(created));
        parse_tab_field(&p, deleted, sizeof(deleted));
        int id = atoi(id_buf);
        if (id <= 0) continue;
        write_u32(out, (uint32_t)id);
        write_str(out, name);
        write_str(out, email);
        write_str(out, phone);
        write_str(out, notes);
        write_str(out, created);
        write_str(out, deleted[0] ? deleted : "");
    }
    fclose(in);
    fclose(out);
    unlink(path_txt);
}

static void migrate_events_txt_to_bin(const char *path_txt, const char *path_bin) {
    FILE *in = fopen(path_txt, "r");
    if (!in) return;
    FILE *out = fopen(path_bin, "wb");
    if (!out) { fclose(in); return; }
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        char id_buf[32], title[VIBE_TITLE_MAX], desc[VIBE_CONTENT_MAX], start[VIBE_DATETIME_MAX], end[VIBE_DATETIME_MAX], all_buf[8], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        parse_tab_field(&p, id_buf, sizeof(id_buf));
        parse_tab_field(&p, title, sizeof(title));
        parse_tab_field(&p, desc, sizeof(desc));
        parse_tab_field(&p, start, sizeof(start));
        parse_tab_field(&p, end, sizeof(end));
        parse_tab_field(&p, all_buf, sizeof(all_buf));
        parse_tab_field(&p, created, sizeof(created));
        parse_tab_field(&p, deleted, sizeof(deleted));
        int id = atoi(id_buf);
        if (id <= 0) continue;
        write_u32(out, (uint32_t)id);
        write_str(out, title);
        write_str(out, desc);
        write_str(out, start);
        write_str(out, end);
        write_u32(out, (uint32_t)atoi(all_buf));
        write_str(out, created);
        write_str(out, deleted[0] ? deleted : "");
    }
    fclose(in);
    fclose(out);
    unlink(path_txt);
}

static void migrate_facts_txt_to_bin(const char *path_txt, const char *path_bin) {
    FILE *in = fopen(path_txt, "r");
    if (!in) return;
    FILE *out = fopen(path_bin, "wb");
    if (!out) { fclose(in); return; }
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        char id_buf[32], key[VIBE_TITLE_MAX], value[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        parse_tab_field(&p, id_buf, sizeof(id_buf));
        parse_tab_field(&p, key, sizeof(key));
        parse_tab_field(&p, value, sizeof(value));
        parse_tab_field(&p, created, sizeof(created));
        parse_tab_field(&p, deleted, sizeof(deleted));
        int id = atoi(id_buf);
        if (id <= 0) continue;
        write_u32(out, (uint32_t)id);
        write_str(out, key);
        write_str(out, value);
        write_str(out, created);
        write_str(out, deleted[0] ? deleted : "");
    }
    fclose(in);
    fclose(out);
    unlink(path_txt);
}

static void migrate_if_needed(const char *base) {
    char path_txt[DATA_DIR_MAX + 64];
    char path_bin[DATA_DIR_MAX + 64];
    data_path(path_txt, sizeof(path_txt), base);
    data_path(path_bin, sizeof(path_bin), base);
    strcat(path_txt, ".txt");
    strcat(path_bin, ".bin");
    if (access(path_txt, F_OK) != 0) return;
    if (access(path_bin, F_OK) == 0) return;
    if (strcmp(base, "notes") == 0) migrate_notes_txt_to_bin(path_txt, path_bin);
    else if (strcmp(base, "tasks") == 0) migrate_tasks_txt_to_bin(path_txt, path_bin);
    else if (strcmp(base, "contacts") == 0) migrate_contacts_txt_to_bin(path_txt, path_bin);
    else if (strcmp(base, "events") == 0) migrate_events_txt_to_bin(path_txt, path_bin);
    else if (strcmp(base, "facts") == 0) migrate_facts_txt_to_bin(path_txt, path_bin);
}
#endif

static int next_id(const char *base) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), base);
    strcat(path, EXT);
    FILE *f = fopen(path, "rb");
    int max = 0;
    if (f) {
        /* Scan for max id - we need to read records. For notes: id is first 4 bytes. */
        for (;;) {
            uint32_t id;
            if (fread(&id, 1, 4, f) != 4) break;
            if ((int)id > max) max = (int)id;
            /* Skip rest of record - depends on entity type. Use a generic skip. */
            if (strstr(base, "notes") != NULL) {
                if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
            } else if (strstr(base, "tasks") != NULL) {
                if (!skip_str(f)) break;
                if (fseek(f, 4, SEEK_CUR) != 0) break;  /* done */
                if (!skip_str(f)) break;
                if (fseek(f, 4, SEEK_CUR) != 0) break;  /* priority */
                if (!skip_str(f) || !skip_str(f)) break;
            } else if (strstr(base, "contacts") != NULL) {
                if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
            } else if (strstr(base, "events") != NULL) {
                if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f) || fseek(f, 4, SEEK_CUR) != 0) break;
                if (!skip_str(f) || !skip_str(f)) break;
            } else if (strstr(base, "facts") != NULL) {
                if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
            } else break;
        }
        fclose(f);
    }
    return max + 1;
}

/* --- Notes --- */
int storage_notes_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes" EXT);
    FILE *f = fopen(path, "rb");
    int n = 0;
    if (f) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, f) != 4) break;
            if (!skip_str(f) || !skip_str(f) || !skip_str(f)) break;
            uint32_t del_len;
            if (!read_u32(f, &del_len)) break;
            if (del_len == 0) n++;
            else if (fseek(f, (long)del_len, SEEK_CUR) != 0) break;
        }
        fclose(f);
    }
    return n;
}

int storage_notes_add(const char *title, const char *content) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes" EXT);
    int id = next_id("notes");
    char t[VIBE_TITLE_MAX], c[VIBE_CONTENT_MAX];
    copy_str(t, title ? title : "", VIBE_TITLE_MAX);
    copy_str(c, content ? content : "", VIBE_CONTENT_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    uint32_t u32 = (uint32_t)id;
    fwrite(&u32, 1, 4, f);
    write_str(f, t);
    write_str(f, c);
    write_str(f, ts);
    write_str(f, "");
    fclose(f);
    return id;
}

void storage_notes_list(void (*cb)(const VibeNote *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeNote n = {0};
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        n.id = (int)id;
        if (!read_str(f, n.title, sizeof(n.title))) break;
        if (!read_str(f, n.content, sizeof(n.content))) break;
        if (!read_str(f, n.created_at, sizeof(n.created_at))) break;
        if (!read_str(f, n.deleted_at, sizeof(n.deleted_at))) break;
        if (!n.deleted_at[0]) cb(&n, ctx);
    }
    fclose(f);
}

int storage_note_get(int id, VibeNote *out) {
    int found = 0;
    void find_one(const VibeNote *n, void *ctx) {
        (void)ctx;
        if (n->id == id && out) { *out = *n; found = 1; }
    }
    storage_notes_list(find_one, NULL);
    return found;
}

static int notes_rewrite(int skip_id, int set_deleted, const char *new_title, const char *new_content) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes" EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int ok = 0;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, in) != 4) break;
        char title[VIBE_TITLE_MAX], content[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        if (!read_str(in, title, sizeof(title))) break;
        if (!read_str(in, content, sizeof(content))) break;
        if (!read_str(in, created, sizeof(created))) break;
        if (!read_str(in, deleted, sizeof(deleted))) break;
        if ((int)id == skip_id) {
            if (set_deleted) {
                timestamp(deleted, sizeof(deleted));
                fwrite(&id, 1, 4, out);
                write_str(out, title);
                write_str(out, content);
                write_str(out, created);
                write_str(out, deleted);
            } else if (new_title) {
                fwrite(&id, 1, 4, out);
                write_str(out, new_title);
                write_str(out, new_content ? new_content : "");
                write_str(out, created);
                write_str(out, "");
            }
            ok = 1;
        } else {
            fwrite(&id, 1, 4, out);
            write_str(out, title);
            write_str(out, content);
            write_str(out, created);
            write_str(out, deleted);
        }
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_notes_update(int id, const char *title, const char *content) {
    char t[VIBE_TITLE_MAX], c[VIBE_CONTENT_MAX];
    copy_str(t, title ? title : "", VIBE_TITLE_MAX);
    copy_str(c, content ? content : "", VIBE_CONTENT_MAX);
    return notes_rewrite(id, 0, t, c);
}

int storage_notes_delete(int id) {
    return notes_rewrite(id, 1, NULL, NULL);
}

/* --- Tasks --- */
int storage_tasks_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks" EXT);
    FILE *f = fopen(path, "rb");
    int n = 0;
    if (f) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, f) != 4) break;
            if (!skip_str(f)) break;
            if (fseek(f, 4, SEEK_CUR) != 0) break; /* done */
            if (!skip_str(f)) break;
            if (fseek(f, 4, SEEK_CUR) != 0) break;
            if (!skip_str(f)) break;
            uint32_t del_len;
            if (!read_u32(f, &del_len)) break;
            if (del_len == 0) n++;
            else if (fseek(f, (long)del_len, SEEK_CUR) != 0) break;
        }
        fclose(f);
    }
    return n;
}

int storage_tasks_add(const char *title, const char *due_date, int priority) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks" EXT);
    int id = next_id("tasks");
    char t[VIBE_TITLE_MAX];
    copy_str(t, title ? title : "", VIBE_TITLE_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    uint32_t u32 = (uint32_t)id;
    fwrite(&u32, 1, 4, f);
    write_str(f, t);
    u32 = 0; fwrite(&u32, 1, 4, f);
    write_str(f, due_date ? due_date : "");
    u32 = (uint32_t)priority; fwrite(&u32, 1, 4, f);
    write_str(f, ts);
    write_str(f, "");
    fclose(f);
    return id;
}

void storage_tasks_list(void (*cb)(const VibeTask *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeTask t = {0};
    while (1) {
        uint32_t id, done, prio;
        if (fread(&id, 1, 4, f) != 4) break;
        t.id = (int)id;
        if (!read_str(f, t.title, sizeof(t.title))) break;
        if (fread(&done, 1, 4, f) != 4) break;
        t.done = (int)done;
        if (!read_str(f, t.due_date, sizeof(t.due_date))) break;
        if (fread(&prio, 1, 4, f) != 4) break;
        t.priority = (int)prio;
        if (!read_str(f, t.created_at, sizeof(t.created_at))) break;
        if (!read_str(f, t.deleted_at, sizeof(t.deleted_at))) break;
        if (!t.deleted_at[0]) cb(&t, ctx);
    }
    fclose(f);
}

int storage_task_get(int id, VibeTask *out) {
    int found = 0;
    void find_one(const VibeTask *t, void *ctx) {
        (void)ctx;
        if (t->id == id && out) { *out = *t; found = 1; }
    }
    storage_tasks_list(find_one, NULL);
    return found;
}

static int tasks_rewrite(int skip_id, int set_done, int set_deleted,
    const char *new_title, const char *new_due, int new_prio) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks" EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int ok = 0;
    while (1) {
        uint32_t id, done, prio;
        if (fread(&id, 1, 4, in) != 4) break;
        char title[VIBE_TITLE_MAX], due[VIBE_DATETIME_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        if (!read_str(in, title, sizeof(title))) break;
        if (fread(&done, 1, 4, in) != 4) break;
        if (!read_str(in, due, sizeof(due))) break;
        if (fread(&prio, 1, 4, in) != 4) break;
        if (!read_str(in, created, sizeof(created))) break;
        if (!read_str(in, deleted, sizeof(deleted))) break;
        if ((int)id == skip_id) {
            if (set_deleted) {
                timestamp(deleted, sizeof(deleted));
                fwrite(&id, 1, 4, out);
                write_str(out, title);
                uint32_t d = (uint32_t)done; fwrite(&d, 1, 4, out);
                write_str(out, due);
                fwrite(&prio, 1, 4, out);
                write_str(out, created);
                write_str(out, deleted);
            } else if (new_title) {
                fwrite(&id, 1, 4, out);
                write_str(out, new_title);
                uint32_t d = (uint32_t)set_done; fwrite(&d, 1, 4, out);
                write_str(out, new_due ? new_due : "");
                uint32_t p = (uint32_t)new_prio; fwrite(&p, 1, 4, out);
                write_str(out, created);
                write_str(out, "");
            }
            ok = 1;
        } else {
            fwrite(&id, 1, 4, out);
            write_str(out, title);
            fwrite(&done, 1, 4, out);
            write_str(out, due);
            fwrite(&prio, 1, 4, out);
            write_str(out, created);
            write_str(out, deleted);
        }
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_tasks_update(int id, const char *title, const char *due_date, int priority, int done) {
    VibeTask t;
    if (!storage_task_get(id, &t)) return 0;
    char ti[VIBE_TITLE_MAX], du[VIBE_DATETIME_MAX];
    copy_str(ti, title ? title : "", VIBE_TITLE_MAX);
    copy_str(du, due_date ? due_date : "", VIBE_DATETIME_MAX);
    return tasks_rewrite(id, done, 0, ti, du, priority);
}

int storage_tasks_delete(int id) {
    return tasks_rewrite(id, 0, 1, NULL, NULL, 0);
}

/* --- Contacts --- */
int storage_contacts_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts" EXT);
    FILE *f = fopen(path, "rb");
    int n = 0;
    if (f) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, f) != 4) break;
            for (int i = 0; i < 5; i++) if (!skip_str(f)) goto contacts_count_done; /* name, email, phone, notes, created */
            uint32_t del_len;
            if (!read_u32(f, &del_len)) break;
            if (del_len == 0) n++;
            else if (fseek(f, (long)del_len, SEEK_CUR) != 0) break;
        }
contacts_count_done:
        fclose(f);
    }
    return n;
}

int storage_contacts_add(const char *name, const char *email, const char *phone) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts" EXT);
    int id = next_id("contacts");
    char n[VIBE_NAME_MAX], e[VIBE_EMAIL_MAX], p[VIBE_PHONE_MAX];
    copy_str(n, name ? name : "", VIBE_NAME_MAX);
    copy_str(e, email ? email : "", VIBE_EMAIL_MAX);
    copy_str(p, phone ? phone : "", VIBE_PHONE_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    uint32_t u32 = (uint32_t)id;
    fwrite(&u32, 1, 4, f);
    write_str(f, n);
    write_str(f, e);
    write_str(f, p);
    write_str(f, "");
    write_str(f, ts);
    write_str(f, "");
    fclose(f);
    return id;
}

void storage_contacts_list(void (*cb)(const VibeContact *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeContact c = {0};
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        c.id = (int)id;
        if (!read_str(f, c.name, sizeof(c.name))) break;
        if (!read_str(f, c.email, sizeof(c.email))) break;
        if (!read_str(f, c.phone, sizeof(c.phone))) break;
        if (!skip_str(f)) break; /* notes */
        if (!read_str(f, c.created_at, sizeof(c.created_at))) break;
        if (!read_str(f, c.deleted_at, sizeof(c.deleted_at))) break;
        if (!c.deleted_at[0]) cb(&c, ctx);
    }
    fclose(f);
}

int storage_contact_get(int id, VibeContact *out) {
    int found = 0;
    void find_one(const VibeContact *c, void *ctx) {
        (void)ctx;
        if (c->id == id && out) { *out = *c; found = 1; }
    }
    storage_contacts_list(find_one, NULL);
    return found;
}

static int contacts_rewrite(int skip_id, int set_deleted,
    const char *new_name, const char *new_email, const char *new_phone) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts" EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int ok = 0;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, in) != 4) break;
        char name[VIBE_NAME_MAX], email[VIBE_EMAIL_MAX], phone[VIBE_PHONE_MAX], notes[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        if (!read_str(in, name, sizeof(name))) break;
        if (!read_str(in, email, sizeof(email))) break;
        if (!read_str(in, phone, sizeof(phone))) break;
        if (!read_str(in, notes, sizeof(notes))) break;
        if (!read_str(in, created, sizeof(created))) break;
        if (!read_str(in, deleted, sizeof(deleted))) break;
        if ((int)id == skip_id) {
            if (set_deleted) {
                timestamp(deleted, sizeof(deleted));
                fwrite(&id, 1, 4, out);
                write_str(out, name);
                write_str(out, email);
                write_str(out, phone);
                write_str(out, notes);
                write_str(out, created);
                write_str(out, deleted);
            } else if (new_name) {
                fwrite(&id, 1, 4, out);
                write_str(out, new_name);
                write_str(out, new_email ? new_email : "");
                write_str(out, new_phone ? new_phone : "");
                write_str(out, notes);
                write_str(out, created);
                write_str(out, "");
            }
            ok = 1;
        } else {
            fwrite(&id, 1, 4, out);
            write_str(out, name);
            write_str(out, email);
            write_str(out, phone);
            write_str(out, notes);
            write_str(out, created);
            write_str(out, deleted);
        }
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_contacts_update(int id, const char *name, const char *email, const char *phone) {
    char n[VIBE_NAME_MAX], e[VIBE_EMAIL_MAX], p[VIBE_PHONE_MAX];
    copy_str(n, name ? name : "", VIBE_NAME_MAX);
    copy_str(e, email ? email : "", VIBE_EMAIL_MAX);
    copy_str(p, phone ? phone : "", VIBE_PHONE_MAX);
    return contacts_rewrite(id, 0, n, e, p);
}

int storage_contacts_delete(int id) {
    return contacts_rewrite(id, 1, NULL, NULL, NULL);
}

/* --- Events --- */
int storage_events_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events" EXT);
    FILE *f = fopen(path, "rb");
    int n = 0;
    if (f) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, f) != 4) break;
            if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
            if (fseek(f, 4, SEEK_CUR) != 0) break;
            if (!skip_str(f)) break;
            uint32_t del_len;
            if (!read_u32(f, &del_len)) break;
            if (del_len == 0) n++;
            else if (fseek(f, (long)del_len, SEEK_CUR) != 0) break;
        }
        fclose(f);
    }
    return n;
}

int storage_events_add(const char *title, const char *desc, const char *start_at, const char *end_at, int all_day) {
    (void)desc;
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events" EXT);
    int id = next_id("events");
    char t[VIBE_TITLE_MAX];
    copy_str(t, title ? title : "", VIBE_TITLE_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    uint32_t u32 = (uint32_t)id;
    fwrite(&u32, 1, 4, f);
    write_str(f, t);
    write_str(f, "");
    write_str(f, start_at ? start_at : "");
    write_str(f, end_at ? end_at : "");
    u32 = (uint32_t)all_day; fwrite(&u32, 1, 4, f);
    write_str(f, ts);
    write_str(f, "");
    fclose(f);
    return id;
}

void storage_events_list(void (*cb)(const VibeCalendarEvent *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeCalendarEvent e = {0};
    while (1) {
        uint32_t id, all_day;
        if (fread(&id, 1, 4, f) != 4) break;
        e.id = (int)id;
        if (!read_str(f, e.title, sizeof(e.title))) break;
        if (!read_str(f, e.description, sizeof(e.description))) break;
        if (!read_str(f, e.start_at, sizeof(e.start_at))) break;
        if (!read_str(f, e.end_at, sizeof(e.end_at))) break;
        if (fread(&all_day, 1, 4, f) != 4) break;
        e.all_day = (int)all_day;
        if (!read_str(f, e.created_at, sizeof(e.created_at))) break;
        if (!read_str(f, e.deleted_at, sizeof(e.deleted_at))) break;
        if (!e.deleted_at[0]) cb(&e, ctx);
    }
    fclose(f);
}

int storage_event_get(int id, VibeCalendarEvent *out) {
    int found = 0;
    void find_one(const VibeCalendarEvent *e, void *ctx) {
        (void)ctx;
        if (e->id == id && out) { *out = *e; found = 1; }
    }
    storage_events_list(find_one, NULL);
    return found;
}

static int events_rewrite(int skip_id, int set_deleted,
    const char *new_title, const char *new_desc, const char *new_start, const char *new_end, int new_all_day) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events" EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int ok = 0;
    while (1) {
        uint32_t id, all_day;
        if (fread(&id, 1, 4, in) != 4) break;
        char title[VIBE_TITLE_MAX], desc[VIBE_CONTENT_MAX], start[VIBE_DATETIME_MAX], end[VIBE_DATETIME_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        if (!read_str(in, title, sizeof(title))) break;
        if (!read_str(in, desc, sizeof(desc))) break;
        if (!read_str(in, start, sizeof(start))) break;
        if (!read_str(in, end, sizeof(end))) break;
        if (fread(&all_day, 1, 4, in) != 4) break;
        if (!read_str(in, created, sizeof(created))) break;
        if (!read_str(in, deleted, sizeof(deleted))) break;
        if ((int)id == skip_id) {
            if (set_deleted) {
                timestamp(deleted, sizeof(deleted));
                fwrite(&id, 1, 4, out);
                write_str(out, title);
                write_str(out, desc);
                write_str(out, start);
                write_str(out, end);
                uint32_t a = (uint32_t)all_day; fwrite(&a, 1, 4, out);
                write_str(out, created);
                write_str(out, deleted);
            } else if (new_title) {
                fwrite(&id, 1, 4, out);
                write_str(out, new_title);
                write_str(out, new_desc ? new_desc : "");
                write_str(out, new_start ? new_start : "");
                write_str(out, new_end ? new_end : "");
                uint32_t a = (uint32_t)new_all_day; fwrite(&a, 1, 4, out);
                write_str(out, created);
                write_str(out, "");
            }
            ok = 1;
        } else {
            fwrite(&id, 1, 4, out);
            write_str(out, title);
            write_str(out, desc);
            write_str(out, start);
            write_str(out, end);
            fwrite(&all_day, 1, 4, out);
            write_str(out, created);
            write_str(out, deleted);
        }
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_events_update(int id, const char *title, const char *desc, const char *start_at, const char *end_at, int all_day) {
    char t[VIBE_TITLE_MAX], d[VIBE_CONTENT_MAX], s[VIBE_DATETIME_MAX], e[VIBE_DATETIME_MAX];
    copy_str(t, title ? title : "", VIBE_TITLE_MAX);
    copy_str(d, desc ? desc : "", VIBE_CONTENT_MAX);
    copy_str(s, start_at ? start_at : "", VIBE_DATETIME_MAX);
    copy_str(e, end_at ? end_at : "", VIBE_DATETIME_MAX);
    return events_rewrite(id, 0, t, d, s, e, all_day);
}

int storage_events_delete(int id) {
    return events_rewrite(id, 1, NULL, NULL, NULL, NULL, 0);
}

/* --- Facts --- */
int storage_facts_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts" EXT);
    FILE *f = fopen(path, "rb");
    int n = 0;
    if (f) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, f) != 4) break;
            if (!skip_str(f) || !skip_str(f) || !skip_str(f)) break;
            uint32_t del_len;
            if (!read_u32(f, &del_len)) break;
            if (del_len == 0) n++;
            else if (fseek(f, (long)del_len, SEEK_CUR) != 0) break;
        }
        fclose(f);
    }
    return n;
}

int storage_facts_add(const char *key, const char *value) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts" EXT);
    int id = next_id("facts");
    char k[VIBE_TITLE_MAX];
    copy_str(k, key ? key : "", VIBE_TITLE_MAX);
    char v[VIBE_CONTENT_MAX];
    copy_str(v, value ? value : "", VIBE_CONTENT_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    uint32_t u32 = (uint32_t)id;
    fwrite(&u32, 1, 4, f);
    write_str(f, k);
    write_str(f, v);
    write_str(f, ts);
    write_str(f, "");
    fclose(f);
    return id;
}

void storage_facts_list(void (*cb)(const VibeFact *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeFact fact = {0};
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        fact.id = (int)id;
        if (!read_str(f, fact.key, sizeof(fact.key))) break;
        if (!read_str(f, fact.value, sizeof(fact.value))) break;
        if (!read_str(f, fact.created_at, sizeof(fact.created_at))) break;
        if (!read_str(f, fact.deleted_at, sizeof(fact.deleted_at))) break;
        if (!fact.deleted_at[0]) cb(&fact, ctx);
    }
    fclose(f);
}

int storage_fact_get(int id, VibeFact *out) {
    int found = 0;
    void find_one(const VibeFact *f, void *ctx) {
        (void)ctx;
        if (f->id == id && out) { *out = *f; found = 1; }
    }
    storage_facts_list(find_one, NULL);
    return found;
}

static int facts_rewrite(int skip_id, int set_deleted,
    const char *new_key, const char *new_value) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts" EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int ok = 0;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, in) != 4) break;
        char key[VIBE_TITLE_MAX], value[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
        if (!read_str(in, key, sizeof(key))) break;
        if (!read_str(in, value, sizeof(value))) break;
        if (!read_str(in, created, sizeof(created))) break;
        if (!read_str(in, deleted, sizeof(deleted))) break;
        if ((int)id == skip_id) {
            if (set_deleted) {
                timestamp(deleted, sizeof(deleted));
                fwrite(&id, 1, 4, out);
                write_str(out, key);
                write_str(out, value);
                write_str(out, created);
                write_str(out, deleted);
            } else if (new_key) {
                fwrite(&id, 1, 4, out);
                write_str(out, new_key);
                write_str(out, new_value ? new_value : "");
                write_str(out, created);
                write_str(out, "");
            }
            ok = 1;
        } else {
            fwrite(&id, 1, 4, out);
            write_str(out, key);
            write_str(out, value);
            write_str(out, created);
            write_str(out, deleted);
        }
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_facts_update(int id, const char *key, const char *value) {
    char k[VIBE_TITLE_MAX], v[VIBE_CONTENT_MAX];
    copy_str(k, key ? key : "", VIBE_TITLE_MAX);
    copy_str(v, value ? value : "", VIBE_CONTENT_MAX);
    return facts_rewrite(id, 0, k, v);
}

int storage_facts_delete(int id) {
    return facts_rewrite(id, 1, NULL, NULL);
}

/* --- Trash --- */
static int count_deleted_in_file(const char *base, int (*read_and_check)(FILE*, int*)) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), base);
    strcat(path, EXT);
    FILE *f = fopen(path, "rb");
    int n = 0;
    if (f) {
        int has_deleted;
        while (read_and_check(f, &has_deleted)) {
            if (has_deleted) n++;
        }
        fclose(f);
    }
    return n;
}

static int notes_read_deleted(FILE *f, int *has_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, f) != 4) return 0;
    if (!skip_str(f) || !skip_str(f) || !skip_str(f)) return 0;
    uint32_t del_len;
    if (!read_u32(f, &del_len)) return 0;
    *has_deleted = (del_len > 0);
    if (del_len > 0 && fseek(f, (long)del_len, SEEK_CUR) != 0) return 0;
    return 1;
}

static int tasks_read_deleted(FILE *f, int *has_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, f) != 4) return 0;
    if (!skip_str(f)) return 0;
    if (fseek(f, 4, SEEK_CUR) != 0) return 0;
    if (!skip_str(f)) return 0;
    if (fseek(f, 4, SEEK_CUR) != 0) return 0;
    if (!skip_str(f)) return 0;
    uint32_t del_len;
    if (!read_u32(f, &del_len)) return 0;
    *has_deleted = (del_len > 0);
    if (del_len > 0 && fseek(f, (long)del_len, SEEK_CUR) != 0) return 0;
    return 1;
}

static int contacts_read_deleted(FILE *f, int *has_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, f) != 4) return 0;
    for (int i = 0; i < 5; i++) if (!skip_str(f)) return 0;
    uint32_t del_len;
    if (!read_u32(f, &del_len)) return 0;
    *has_deleted = (del_len > 0);
    if (del_len > 0 && fseek(f, (long)del_len, SEEK_CUR) != 0) return 0;
    return 1;
}

static int events_read_deleted(FILE *f, int *has_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, f) != 4) return 0;
    if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) return 0;
    if (fseek(f, 4, SEEK_CUR) != 0) return 0;
    if (!skip_str(f)) return 0;
    uint32_t del_len;
    if (!read_u32(f, &del_len)) return 0;
    *has_deleted = (del_len > 0);
    if (del_len > 0 && fseek(f, (long)del_len, SEEK_CUR) != 0) return 0;
    return 1;
}

static int facts_read_deleted(FILE *f, int *has_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, f) != 4) return 0;
    if (!skip_str(f) || !skip_str(f) || !skip_str(f)) return 0;
    uint32_t del_len;
    if (!read_u32(f, &del_len)) return 0;
    *has_deleted = (del_len > 0);
    if (del_len > 0 && fseek(f, (long)del_len, SEEK_CUR) != 0) return 0;
    return 1;
}

int storage_trash_count(void) {
    return count_deleted_in_file("notes", notes_read_deleted) +
           count_deleted_in_file("tasks", tasks_read_deleted) +
           count_deleted_in_file("contacts", contacts_read_deleted) +
           count_deleted_in_file("events", events_read_deleted) +
           count_deleted_in_file("facts", facts_read_deleted);
}

/* Trash list: iterate each file, emit deleted items with title */
static void trash_list_notes(void (*cb)(int, int, const char*, void*), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        char title[VIBE_TITLE_MAX];
        if (!read_str(f, title, sizeof(title))) break;
        if (!skip_str(f) || !skip_str(f)) break;
        uint32_t del_len;
        if (!read_u32(f, &del_len)) break;
        if (del_len > 0) cb(0, (int)id, title, ctx);
        else if (fseek(f, 0, SEEK_CUR) != 0) break;
    }
    fclose(f);
}

static void trash_list_tasks(void (*cb)(int, int, const char*, void*), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        char title[VIBE_TITLE_MAX];
        if (!read_str(f, title, sizeof(title))) break;
        if (fseek(f, 4, SEEK_CUR) != 0) break;  /* done */
        if (!skip_str(f)) break;  /* due */
        if (fseek(f, 4, SEEK_CUR) != 0) break;  /* priority */
        if (!skip_str(f)) break;  /* created */
        uint32_t del_len;
        if (!read_u32(f, &del_len)) break;
        if (del_len > 0) cb(1, (int)id, title, ctx);
        else if (fseek(f, 0, SEEK_CUR) != 0) break;
    }
    fclose(f);
}

static void trash_list_contacts(void (*cb)(int, int, const char*, void*), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        char name[VIBE_NAME_MAX];
        if (!read_str(f, name, sizeof(name))) break;
        if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
        uint32_t del_len;
        if (!read_u32(f, &del_len)) break;
        if (del_len > 0) cb(2, (int)id, name, ctx);
    }
    fclose(f);
}

static void trash_list_events(void (*cb)(int, int, const char*, void*), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        char title[VIBE_TITLE_MAX];
        if (!read_str(f, title, sizeof(title))) break;
        if (!skip_str(f) || !skip_str(f) || !skip_str(f)) break;
        if (fseek(f, 4, SEEK_CUR) != 0) break;
        if (!skip_str(f)) break;
        uint32_t del_len;
        if (!read_u32(f, &del_len)) break;
        if (del_len > 0) cb(3, (int)id, title, ctx);
    }
    fclose(f);
}

static void trash_list_facts(void (*cb)(int, int, const char*, void*), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        char key[VIBE_TITLE_MAX];
        if (!read_str(f, key, sizeof(key))) break;
        if (!skip_str(f) || !skip_str(f)) break;
        uint32_t del_len;
        if (!read_u32(f, &del_len)) break;
        if (del_len > 0) cb(4, (int)id, key, ctx);
    }
    fclose(f);
}

void storage_trash_list(void (*cb)(int entity_type, int id, const char *title, void *), void *ctx) {
    if (!cb) return;
    trash_list_notes(cb, ctx);
    trash_list_tasks(cb, ctx);
    trash_list_contacts(cb, ctx);
    trash_list_events(cb, ctx);
    trash_list_facts(cb, ctx);
}

/* Restore: clear deleted_at for entity */
int storage_restore(int entity_type, int id) {
    switch (entity_type) {
        case ENTITY_NOTE: {
            char path[DATA_DIR_MAX + 64];
            data_path(path, sizeof(path), "notes" EXT);
            FILE *f = fopen(path, "rb");
            if (!f) return 0;
            while (1) {
                uint32_t rid;
                if (fread(&rid, 1, 4, f) != 4) break;
                if ((int)rid != id) {
                    if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
                    continue;
                }
                char title[VIBE_TITLE_MAX], content[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX];
                if (!read_str(f, title, sizeof(title))) { fclose(f); return 0; }
                if (!read_str(f, content, sizeof(content))) { fclose(f); return 0; }
                if (!read_str(f, created, sizeof(created))) { fclose(f); return 0; }
                skip_str(f);
                fclose(f);
                return notes_rewrite(id, 0, title, content);
            }
            fclose(f);
            return 0;
        }
        case ENTITY_TASK: {
            char path[DATA_DIR_MAX + 64];
            data_path(path, sizeof(path), "tasks" EXT);
            FILE *f = fopen(path, "rb");
            if (!f) return 0;
            while (1) {
                uint32_t rid, done, prio;
                if (fread(&rid, 1, 4, f) != 4) break;
                if ((int)rid != id) {
                    if (!skip_str(f)) break;
                    if (fseek(f, 4, SEEK_CUR) != 0) break;
                    if (!skip_str(f)) break;
                    if (fseek(f, 4, SEEK_CUR) != 0) break;
                    if (!skip_str(f) || !skip_str(f)) break;
                    continue;
                }
                char title[VIBE_TITLE_MAX], due[VIBE_DATETIME_MAX];
                if (!read_str(f, title, sizeof(title))) { fclose(f); return 0; }
                if (fread(&done, 1, 4, f) != 4) { fclose(f); return 0; }
                if (!read_str(f, due, sizeof(due))) { fclose(f); return 0; }
                if (fread(&prio, 1, 4, f) != 4) { fclose(f); return 0; }
                skip_str(f); skip_str(f);
                fclose(f);
                return tasks_rewrite(id, (int)done, 0, title, due, (int)prio);
            }
            fclose(f);
            return 0;
        }
        case ENTITY_CONTACT: {
            char path[DATA_DIR_MAX + 64];
            data_path(path, sizeof(path), "contacts" EXT);
            FILE *f = fopen(path, "rb");
            if (!f) return 0;
            while (1) {
                uint32_t rid;
                if (fread(&rid, 1, 4, f) != 4) break;
                if ((int)rid != id) {
                    for (int i = 0; i < 6; i++) if (!skip_str(f)) goto contacts_restore_done;
                    continue;
                }
                char name[VIBE_NAME_MAX], email[VIBE_EMAIL_MAX], phone[VIBE_PHONE_MAX];
                if (!read_str(f, name, sizeof(name))) { fclose(f); return 0; }
                if (!read_str(f, email, sizeof(email))) { fclose(f); return 0; }
                if (!read_str(f, phone, sizeof(phone))) { fclose(f); return 0; }
                skip_str(f); skip_str(f); skip_str(f);
                fclose(f);
                return contacts_rewrite(id, 0, name, email, phone);
            }
contacts_restore_done:
            fclose(f);
            return 0;
        }
        case ENTITY_EVENT: {
            char path[DATA_DIR_MAX + 64];
            data_path(path, sizeof(path), "events" EXT);
            FILE *f = fopen(path, "rb");
            if (!f) return 0;
            while (1) {
                uint32_t rid, all_day;
                if (fread(&rid, 1, 4, f) != 4) break;
                if ((int)rid != id) {
                    if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
                    if (fseek(f, 4, SEEK_CUR) != 0) break;
                    if (!skip_str(f) || !skip_str(f)) break;
                    continue;
                }
                char title[VIBE_TITLE_MAX], desc[VIBE_CONTENT_MAX], start[VIBE_DATETIME_MAX], end[VIBE_DATETIME_MAX];
                if (!read_str(f, title, sizeof(title))) { fclose(f); return 0; }
                if (!read_str(f, desc, sizeof(desc))) { fclose(f); return 0; }
                if (!read_str(f, start, sizeof(start))) { fclose(f); return 0; }
                if (!read_str(f, end, sizeof(end))) { fclose(f); return 0; }
                if (fread(&all_day, 1, 4, f) != 4) { fclose(f); return 0; }
                skip_str(f); skip_str(f);
                fclose(f);
                return events_rewrite(id, 0, title, desc, start, end, (int)all_day);
            }
            fclose(f);
            return 0;
        }
        case ENTITY_FACT: {
            char path[DATA_DIR_MAX + 64];
            data_path(path, sizeof(path), "facts" EXT);
            FILE *f = fopen(path, "rb");
            if (!f) return 0;
            while (1) {
                uint32_t rid;
                if (fread(&rid, 1, 4, f) != 4) break;
                if ((int)rid != id) {
                    if (!skip_str(f) || !skip_str(f) || !skip_str(f) || !skip_str(f)) break;
                    continue;
                }
                char key[VIBE_TITLE_MAX], value[VIBE_CONTENT_MAX];
                if (!read_str(f, key, sizeof(key))) { fclose(f); return 0; }
                if (!read_str(f, value, sizeof(value))) { fclose(f); return 0; }
                skip_str(f); skip_str(f);
                fclose(f);
                return facts_rewrite(id, 0, key, value);
            }
            fclose(f);
            return 0;
        }
        case ENTITY_FINANCE:
        case ENTITY_DOCUMENT: return 0;
        default: return 0;
    }
}

/* Finances, Documents: stubs */
int storage_finances_count(void) { return 0; }
int storage_finances_add(const char *date, const char *description, double amount, const char *category, const char *account, const char *notes) {
    (void)date; (void)description; (void)amount; (void)category; (void)account; (void)notes;
    return 0;
}
void storage_finances_list(void (*cb)(const VibeFinanceEntry *, void *), void *ctx) { (void)cb; (void)ctx; }
int storage_finance_get(int id, VibeFinanceEntry *out) { (void)id; (void)out; return 0; }
int storage_finances_update(int id, const char *date, const char *description, double amount, const char *category, const char *account, const char *notes) {
    (void)id; (void)date; (void)description; (void)amount; (void)category; (void)account; (void)notes;
    return 0;
}
int storage_finances_delete(int id) { (void)id; return 0; }

int storage_documents_count(void) { return 0; }
int storage_documents_add(const char *title, const char *template_name, const char *content) {
    (void)title; (void)template_name; (void)content;
    return 0;
}
void storage_documents_list(void (*cb)(const VibeDocument *, void *), void *ctx) { (void)cb; (void)ctx; }
int storage_document_get(int id, VibeDocument *out) { (void)id; (void)out; return 0; }
int storage_documents_update(int id, const char *title, const char *template_name, const char *content) {
    (void)id; (void)title; (void)template_name; (void)content;
    return 0;
}
int storage_documents_delete(int id) { (void)id; return 0; }

/* Permanent delete: remove record from file */
static int permanent_delete_rewrite(const char *base, int skip_id) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), base);
    strcat(path, EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int found = 0;
    /* Copy all records except skip_id - use generic record copy by type */
    if (strcmp(base, "notes") == 0) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, in) != 4) break;
            char title[VIBE_TITLE_MAX], content[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
            if (!read_str(in, title, sizeof(title))) break;
            if (!read_str(in, content, sizeof(content))) break;
            if (!read_str(in, created, sizeof(created))) break;
            if (!read_str(in, deleted, sizeof(deleted))) break;
            if ((int)id == skip_id) { found = 1; continue; }
            fwrite(&id, 1, 4, out);
            write_str(out, title);
            write_str(out, content);
            write_str(out, created);
            write_str(out, deleted);
        }
    } else if (strcmp(base, "tasks") == 0) {
        while (1) {
            uint32_t id, done, prio;
            if (fread(&id, 1, 4, in) != 4) break;
            char title[VIBE_TITLE_MAX], due[VIBE_DATETIME_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
            if (!read_str(in, title, sizeof(title))) break;
            if (fread(&done, 1, 4, in) != 4) break;
            if (!read_str(in, due, sizeof(due))) break;
            if (fread(&prio, 1, 4, in) != 4) break;
            if (!read_str(in, created, sizeof(created))) break;
            if (!read_str(in, deleted, sizeof(deleted))) break;
            if ((int)id == skip_id) { found = 1; continue; }
            fwrite(&id, 1, 4, out);
            write_str(out, title);
            fwrite(&done, 1, 4, out);
            write_str(out, due);
            fwrite(&prio, 1, 4, out);
            write_str(out, created);
            write_str(out, deleted);
        }
    } else if (strcmp(base, "contacts") == 0) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, in) != 4) break;
            char name[VIBE_NAME_MAX], email[VIBE_EMAIL_MAX], phone[VIBE_PHONE_MAX], notes[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
            if (!read_str(in, name, sizeof(name))) break;
            if (!read_str(in, email, sizeof(email))) break;
            if (!read_str(in, phone, sizeof(phone))) break;
            if (!read_str(in, notes, sizeof(notes))) break;
            if (!read_str(in, created, sizeof(created))) break;
            if (!read_str(in, deleted, sizeof(deleted))) break;
            if ((int)id == skip_id) { found = 1; continue; }
            fwrite(&id, 1, 4, out);
            write_str(out, name);
            write_str(out, email);
            write_str(out, phone);
            write_str(out, notes);
            write_str(out, created);
            write_str(out, deleted);
        }
    } else if (strcmp(base, "events") == 0) {
        while (1) {
            uint32_t id, all_day;
            if (fread(&id, 1, 4, in) != 4) break;
            char title[VIBE_TITLE_MAX], desc[VIBE_CONTENT_MAX], start[VIBE_DATETIME_MAX], end[VIBE_DATETIME_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
            if (!read_str(in, title, sizeof(title))) break;
            if (!read_str(in, desc, sizeof(desc))) break;
            if (!read_str(in, start, sizeof(start))) break;
            if (!read_str(in, end, sizeof(end))) break;
            if (fread(&all_day, 1, 4, in) != 4) break;
            if (!read_str(in, created, sizeof(created))) break;
            if (!read_str(in, deleted, sizeof(deleted))) break;
            if ((int)id == skip_id) { found = 1; continue; }
            fwrite(&id, 1, 4, out);
            write_str(out, title);
            write_str(out, desc);
            write_str(out, start);
            write_str(out, end);
            fwrite(&all_day, 1, 4, out);
            write_str(out, created);
            write_str(out, deleted);
        }
    } else if (strcmp(base, "facts") == 0) {
        while (1) {
            uint32_t id;
            if (fread(&id, 1, 4, in) != 4) break;
            char key[VIBE_TITLE_MAX], value[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
            if (!read_str(in, key, sizeof(key))) break;
            if (!read_str(in, value, sizeof(value))) break;
            if (!read_str(in, created, sizeof(created))) break;
            if (!read_str(in, deleted, sizeof(deleted))) break;
            if ((int)id == skip_id) { found = 1; continue; }
            fwrite(&id, 1, 4, out);
            write_str(out, key);
            write_str(out, value);
            write_str(out, created);
            write_str(out, deleted);
        }
    }
    fclose(in);
    fclose(out);
    if (found) { remove(path); rename(tmp, path); return 1; }
    remove(tmp);
    return 0;
}

int storage_permanent_delete(int entity_type, int id) {
    switch (entity_type) {
        case ENTITY_NOTE:     return permanent_delete_rewrite("notes", id);
        case ENTITY_TASK:     return permanent_delete_rewrite("tasks", id);
        case ENTITY_CONTACT:  return permanent_delete_rewrite("contacts", id);
        case ENTITY_EVENT:    return permanent_delete_rewrite("events", id);
        case ENTITY_FACT:     return permanent_delete_rewrite("facts", id);
        case ENTITY_FINANCE:  return permanent_delete_rewrite("finances", id);
        case ENTITY_DOCUMENT: return permanent_delete_rewrite("documents", id);
        default: return 0;
    }
}

/* Empty trash: rewrite each file omitting deleted records */
static int empty_trash_file(const char *base, int (*read_record)(FILE*, FILE*, int*)) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), base);
    strcat(path, EXT);
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return 0; }
    int count = 0;
    int is_deleted;
    while (read_record(in, out, &is_deleted)) {
        if (is_deleted) count++;
    }
    fclose(in);
    fclose(out);
    remove(path);
    rename(tmp, path);
    return count;
}

static int notes_empty_record(FILE *in, FILE *out, int *is_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, in) != 4) return 0;
    char title[VIBE_TITLE_MAX], content[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
    if (!read_str(in, title, sizeof(title))) return 0;
    if (!read_str(in, content, sizeof(content))) return 0;
    if (!read_str(in, created, sizeof(created))) return 0;
    if (!read_str(in, deleted, sizeof(deleted))) return 0;
    *is_deleted = (deleted[0] != '\0');
    if (!*is_deleted) {
        fwrite(&id, 1, 4, out);
        write_str(out, title);
        write_str(out, content);
        write_str(out, created);
        write_str(out, "");
    }
    return 1;
}

static int tasks_empty_record(FILE *in, FILE *out, int *is_deleted) {
    uint32_t id, done, prio;
    if (fread(&id, 1, 4, in) != 4) return 0;
    char title[VIBE_TITLE_MAX], due[VIBE_DATETIME_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
    if (!read_str(in, title, sizeof(title))) return 0;
    if (fread(&done, 1, 4, in) != 4) return 0;
    if (!read_str(in, due, sizeof(due))) return 0;
    if (fread(&prio, 1, 4, in) != 4) return 0;
    if (!read_str(in, created, sizeof(created))) return 0;
    if (!read_str(in, deleted, sizeof(deleted))) return 0;
    *is_deleted = (deleted[0] != '\0');
    if (!*is_deleted) {
        fwrite(&id, 1, 4, out);
        write_str(out, title);
        fwrite(&done, 1, 4, out);
        write_str(out, due);
        fwrite(&prio, 1, 4, out);
        write_str(out, created);
        write_str(out, "");
    }
    return 1;
}

static int contacts_empty_record(FILE *in, FILE *out, int *is_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, in) != 4) return 0;
    char name[VIBE_NAME_MAX], email[VIBE_EMAIL_MAX], phone[VIBE_PHONE_MAX], notes[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
    if (!read_str(in, name, sizeof(name))) return 0;
    if (!read_str(in, email, sizeof(email))) return 0;
    if (!read_str(in, phone, sizeof(phone))) return 0;
    if (!read_str(in, notes, sizeof(notes))) return 0;
    if (!read_str(in, created, sizeof(created))) return 0;
    if (!read_str(in, deleted, sizeof(deleted))) return 0;
    *is_deleted = (deleted[0] != '\0');
    if (!*is_deleted) {
        fwrite(&id, 1, 4, out);
        write_str(out, name);
        write_str(out, email);
        write_str(out, phone);
        write_str(out, notes);
        write_str(out, created);
        write_str(out, "");
    }
    return 1;
}

static int events_empty_record(FILE *in, FILE *out, int *is_deleted) {
    uint32_t id, all_day;
    if (fread(&id, 1, 4, in) != 4) return 0;
    char title[VIBE_TITLE_MAX], desc[VIBE_CONTENT_MAX], start[VIBE_DATETIME_MAX], end[VIBE_DATETIME_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
    if (!read_str(in, title, sizeof(title))) return 0;
    if (!read_str(in, desc, sizeof(desc))) return 0;
    if (!read_str(in, start, sizeof(start))) return 0;
    if (!read_str(in, end, sizeof(end))) return 0;
    if (fread(&all_day, 1, 4, in) != 4) return 0;
    if (!read_str(in, created, sizeof(created))) return 0;
    if (!read_str(in, deleted, sizeof(deleted))) return 0;
    *is_deleted = (deleted[0] != '\0');
    if (!*is_deleted) {
        fwrite(&id, 1, 4, out);
        write_str(out, title);
        write_str(out, desc);
        write_str(out, start);
        write_str(out, end);
        fwrite(&all_day, 1, 4, out);
        write_str(out, created);
        write_str(out, "");
    }
    return 1;
}

static int facts_empty_record(FILE *in, FILE *out, int *is_deleted) {
    uint32_t id;
    if (fread(&id, 1, 4, in) != 4) return 0;
    char key[VIBE_TITLE_MAX], value[VIBE_CONTENT_MAX], created[VIBE_DATETIME_MAX], deleted[VIBE_DATETIME_MAX];
    if (!read_str(in, key, sizeof(key))) return 0;
    if (!read_str(in, value, sizeof(value))) return 0;
    if (!read_str(in, created, sizeof(created))) return 0;
    if (!read_str(in, deleted, sizeof(deleted))) return 0;
    *is_deleted = (deleted[0] != '\0');
    if (!*is_deleted) {
        fwrite(&id, 1, 4, out);
        write_str(out, key);
        write_str(out, value);
        write_str(out, created);
        write_str(out, "");
    }
    return 1;
}

int storage_empty_trash(void) {
    int count = 0;
    count += empty_trash_file("notes", notes_empty_record);
    count += empty_trash_file("tasks", tasks_empty_record);
    count += empty_trash_file("contacts", contacts_empty_record);
    count += empty_trash_file("events", events_empty_record);
    count += empty_trash_file("facts", facts_empty_record);
    return count;
}

void storage_set_search_filter(const char *query) { (void)query; }

/* Filtered list: same as list but filter by query */
void storage_notes_list_filtered(void (*cb)(const VibeNote *, void *), void *ctx, const char *query) {
    if (!query || !*query) { storage_notes_list(cb, ctx); return; }
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeNote n = {0};
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        n.id = (int)id;
        if (!read_str(f, n.title, sizeof(n.title))) break;
        if (!read_str(f, n.content, sizeof(n.content))) break;
        if (!read_str(f, n.created_at, sizeof(n.created_at))) break;
        if (!read_str(f, n.deleted_at, sizeof(n.deleted_at))) break;
        if (!n.deleted_at[0] && (str_contains_ci(n.title, query) || str_contains_ci(n.content, query)))
            cb(&n, ctx);
    }
    fclose(f);
}

void storage_tasks_list_filtered(void (*cb)(const VibeTask *, void *), void *ctx, const char *query) {
    if (!query || !*query) { storage_tasks_list(cb, ctx); return; }
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeTask t = {0};
    while (1) {
        uint32_t id, done, prio;
        if (fread(&id, 1, 4, f) != 4) break;
        t.id = (int)id;
        if (!read_str(f, t.title, sizeof(t.title))) break;
        if (fread(&done, 1, 4, f) != 4) break;
        t.done = (int)done;
        if (!read_str(f, t.due_date, sizeof(t.due_date))) break;
        if (fread(&prio, 1, 4, f) != 4) break;
        t.priority = (int)prio;
        if (!read_str(f, t.created_at, sizeof(t.created_at))) break;
        if (!read_str(f, t.deleted_at, sizeof(t.deleted_at))) break;
        if (!t.deleted_at[0] && str_contains_ci(t.title, query)) cb(&t, ctx);
    }
    fclose(f);
}

void storage_contacts_list_filtered(void (*cb)(const VibeContact *, void *), void *ctx, const char *query) {
    if (!query || !*query) { storage_contacts_list(cb, ctx); return; }
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeContact c = {0};
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        c.id = (int)id;
        if (!read_str(f, c.name, sizeof(c.name))) break;
        if (!read_str(f, c.email, sizeof(c.email))) break;
        if (!read_str(f, c.phone, sizeof(c.phone))) break;
        if (!skip_str(f)) break;
        if (!read_str(f, c.created_at, sizeof(c.created_at))) break;
        if (!read_str(f, c.deleted_at, sizeof(c.deleted_at))) break;
        if (!c.deleted_at[0] && (str_contains_ci(c.name, query) || str_contains_ci(c.email, query) || str_contains_ci(c.phone, query)))
            cb(&c, ctx);
    }
    fclose(f);
}

void storage_events_list_filtered(void (*cb)(const VibeCalendarEvent *, void *), void *ctx, const char *query) {
    if (!query || !*query) { storage_events_list(cb, ctx); return; }
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeCalendarEvent e = {0};
    while (1) {
        uint32_t id, all_day;
        if (fread(&id, 1, 4, f) != 4) break;
        e.id = (int)id;
        if (!read_str(f, e.title, sizeof(e.title))) break;
        if (!read_str(f, e.description, sizeof(e.description))) break;
        if (!read_str(f, e.start_at, sizeof(e.start_at))) break;
        if (!read_str(f, e.end_at, sizeof(e.end_at))) break;
        if (fread(&all_day, 1, 4, f) != 4) break;
        e.all_day = (int)all_day;
        if (!read_str(f, e.created_at, sizeof(e.created_at))) break;
        if (!read_str(f, e.deleted_at, sizeof(e.deleted_at))) break;
        if (!e.deleted_at[0] && str_contains_ci(e.title, query)) cb(&e, ctx);
    }
    fclose(f);
}

void storage_facts_list_filtered(void (*cb)(const VibeFact *, void *), void *ctx, const char *query) {
    if (!query || !*query) { storage_facts_list(cb, ctx); return; }
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts" EXT);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    VibeFact fact = {0};
    while (1) {
        uint32_t id;
        if (fread(&id, 1, 4, f) != 4) break;
        fact.id = (int)id;
        if (!read_str(f, fact.key, sizeof(fact.key))) break;
        if (!read_str(f, fact.value, sizeof(fact.value))) break;
        if (!read_str(f, fact.created_at, sizeof(fact.created_at))) break;
        if (!read_str(f, fact.deleted_at, sizeof(fact.deleted_at))) break;
        if (!fact.deleted_at[0] && (str_contains_ci(fact.key, query) || str_contains_ci(fact.value, query)))
            cb(&fact, ctx);
    }
    fclose(f);
}

void storage_finances_list_filtered(void (*cb)(const VibeFinanceEntry *, void *), void *ctx, const char *query) {
    (void)cb; (void)ctx; (void)query;
}

void storage_documents_list_filtered(void (*cb)(const VibeDocument *, void *), void *ctx, const char *query) {
    (void)cb; (void)ctx; (void)query;
}
