/* storage_file.c - File-based storage (Linux, DOS). One file per entity type. */

#include "config.h"
#include "storage.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef PLATFORM_LINUX
# include <sys/stat.h>
#endif

#define DATA_DIR_MAX 256
#define LINE_MAX 8192

static char s_data_dir[DATA_DIR_MAX];
static int s_data_dir_set;

static void sanitize(char *dst, const char *src, int max) {
    int j = 0;
    if (!src) { dst[0] = '\0'; return; }
    for (; src[j] && j < max - 1; j++) {
        char c = src[j];
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        dst[j] = c;
    }
    dst[j] = '\0';
}

static void timestamp(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm)
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
    else
        snprintf(buf, size, "%ld", (long)t);
}

void storage_init(const char *data_dir) {
    s_data_dir_set = 0;
    if (data_dir) {
        snprintf(s_data_dir, sizeof(s_data_dir), "%s", data_dir);
        s_data_dir_set = 1;
    }
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

static int next_id(const char *filename) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), filename);
    FILE *f = fopen(path, "r");
    int max = 0;
    if (f) {
        char line[LINE_MAX];
        while (fgets(line, sizeof(line), f)) {
            int id = 0;
            sscanf(line, "%d", &id);
            if (id > max) max = id;
        }
        fclose(f);
    }
    return max + 1;
}

/* Notes: id\ttitle\tcontent\tcreated_at\tdeleted_at (empty = not deleted) */
int storage_notes_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes.txt");
    FILE *f = fopen(path, "r");
    int n = 0;
    if (f) {
        char line[LINE_MAX];
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            int tabs = 0;
            while (*p) { if (*p == '\t') tabs++; p++; }
            if (tabs < 4) continue;
            p--;
            while (p > line && (*p == '\n' || *p == '\r')) p--;
            if (p > line && *p == '\t') n++;
        }
        fclose(f);
    }
    return n;
}

int storage_notes_add(const char *title, const char *content) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes.txt");
    int id = next_id("notes.txt");
    char t[VIBE_TITLE_MAX], c[VIBE_CONTENT_MAX];
    sanitize(t, title ? title : "", VIBE_TITLE_MAX);
    sanitize(c, content ? content : "", VIBE_CONTENT_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    fprintf(f, "%d\t%s\t%s\t%s\t\n", id, t, c, ts);
    fclose(f);
    return id;
}

void storage_notes_list(void (*cb)(const VibeNote *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "notes.txt");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        VibeNote n = {0};
        char *p = line;
        char *q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; n.id = atoi(p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(n.title, sizeof(n.title), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(n.content, sizeof(n.content), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(n.created_at, sizeof(n.created_at), "%s", p); *q = '\t'; p = q + 1;
        if (p[0] != '\t' && p[0] != '\n' && p[0] != '\0') continue;
        cb(&n, ctx);
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
    data_path(path, sizeof(path), "notes.txt");
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "r");
    if (!in) return 0;
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return 0; }
    char line[LINE_MAX];
    int ok = 0;
    while (fgets(line, sizeof(line), in)) {
        int id = atoi(line);
        char *rest = strchr(line, '\t');
        if (!rest) { fputs(line, out); continue; }
        if (id == skip_id) {
            if (set_deleted) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                char *end = strchr(line, '\n');
                if (end) *end = '\0';
                end = line + strlen(line);
                while (end > line && (end[-1] == '\t' || end[-1] == ' ')) end--;
                *end = '\0';
                fprintf(out, "%s\t%s\n", line, ts);
            } else if (new_title != NULL) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                fprintf(out, "%d\t%s\t%s\t%s\t\n", id, new_title, new_content ? new_content : "", ts);
            }
            ok = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_notes_update(int id, const char *title, const char *content) {
    char t[VIBE_TITLE_MAX], c[VIBE_CONTENT_MAX];
    sanitize(t, title ? title : "", VIBE_TITLE_MAX);
    sanitize(c, content ? content : "", VIBE_CONTENT_MAX);
    return notes_rewrite(id, 0, t, c);
}

int storage_notes_delete(int id) {
    return notes_rewrite(id, 1, NULL, NULL);
}

/* Tasks */
int storage_tasks_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks.txt");
    FILE *f = fopen(path, "r");
    int n = 0;
    if (f) {
        char line[LINE_MAX];
        while (fgets(line, sizeof(line), f)) {
            int id;
            if (sscanf(line, "%d", &id) != 1) continue;
            char *last = line + strlen(line) - 1;
            while (last > line && (*last == '\n' || *last == '\r')) last--;
            if (last > line && *last == '\t') n++;
        }
        fclose(f);
    }
    return n;
}

int storage_tasks_add(const char *title, const char *due_date, int priority) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks.txt");
    int id = next_id("tasks.txt");
    char t[VIBE_TITLE_MAX];
    sanitize(t, title ? title : "", VIBE_TITLE_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    fprintf(f, "%d\t%s\t0\t%s\t%d\t%s\t\n", id, t, due_date ? due_date : "", priority, ts);
    fclose(f);
    return id;
}

void storage_tasks_list(void (*cb)(const VibeTask *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "tasks.txt");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        VibeTask t = {0};
        int done;
        if (sscanf(line, "%d\t%255[^\t]\t%d\t%31[^\t]\t%d\t%31[^\t]",
                   &t.id, t.title, &done, t.due_date, &t.priority, t.created_at) >= 5) {
            t.done = done;
            char *p = strchr(line, '\t');
            for (int i = 0; i < 5 && p; i++) p = strchr(p + 1, '\t');
            if (p && (p[1] == '\t' || p[1] == '\n' || !p[1])) cb(&t, ctx);
        }
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
    data_path(path, sizeof(path), "tasks.txt");
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "r");
    if (!in) return 0;
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return 0; }
    char line[LINE_MAX];
    int ok = 0;
    while (fgets(line, sizeof(line), in)) {
        int id;
        if (sscanf(line, "%d", &id) != 1) { fputs(line, out); continue; }
        if (id != skip_id) { fputs(line, out); continue; }
        char *rest = strchr(line, '\t');
        if (!rest) { fputs(line, out); continue; }
        if (set_deleted) {
            char ts[VIBE_DATETIME_MAX];
            timestamp(ts, sizeof(ts));
            char *end = strchr(line, '\n');
            if (end) *end = '\0';
            end = line + strlen(line);
            while (end > line && (end[-1] == '\t' || end[-1] == ' ')) end--;
            *end = '\0';
            fprintf(out, "%s\t%s\n", line, ts);
        } else if (new_title != NULL) {
            char ts[VIBE_DATETIME_MAX];
            timestamp(ts, sizeof(ts));
            fprintf(out, "%d\t%s\t%d\t%s\t%d\t%s\t\n", skip_id, new_title, set_done, new_due ? new_due : "", new_prio, ts);
        }
        ok = 1;
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
    sanitize(ti, title ? title : "", VIBE_TITLE_MAX);
    sanitize(du, due_date ? due_date : "", VIBE_DATETIME_MAX);
    return tasks_rewrite(id, done, 0, ti, du, priority);
}

int storage_tasks_delete(int id) {
    return tasks_rewrite(id, 0, 1, NULL, NULL, 0);
}

/* Contacts: id\tname\temail\tphone\t\tcreated_at (trailing tab = not deleted) */
int storage_contacts_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts.txt");
    FILE *f = fopen(path, "r");
    int n = 0;
    if (f) {
        char line[LINE_MAX];
        while (fgets(line, sizeof(line), f)) {
            int id;
            if (sscanf(line, "%d", &id) != 1) continue;
            char *last = line + strlen(line) - 1;
            while (last > line && (*last == '\n' || *last == '\r')) last--;
            if (last > line && *last == '\t') n++;
        }
        fclose(f);
    }
    return n;
}
int storage_contacts_add(const char *name, const char *email, const char *phone) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts.txt");
    int id = next_id("contacts.txt");
    char n[VIBE_NAME_MAX], e[VIBE_EMAIL_MAX], p[VIBE_PHONE_MAX];
    sanitize(n, name ? name : "", VIBE_NAME_MAX);
    sanitize(e, email ? email : "", VIBE_EMAIL_MAX);
    sanitize(p, phone ? phone : "", VIBE_PHONE_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    fprintf(f, "%d\t%s\t%s\t%s\t\t%s\t\n", id, n, e, p, ts);
    fclose(f);
    return id;
}
void storage_contacts_list(void (*cb)(const VibeContact *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "contacts.txt");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        VibeContact c = {0};
        char *p = line;
        char *q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; c.id = atoi(p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(c.name, sizeof(c.name), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(c.email, sizeof(c.email), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(c.phone, sizeof(c.phone), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        p = q + 1; /* skip notes field */
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(c.created_at, sizeof(c.created_at), "%s", p); *q = '\t'; p = q + 1;
        if (p[0] != '\t' && p[0] != '\n' && p[0] != '\0') continue;
        cb(&c, ctx);
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
    data_path(path, sizeof(path), "contacts.txt");
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "r");
    if (!in) return 0;
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return 0; }
    char line[LINE_MAX];
    int ok = 0;
    while (fgets(line, sizeof(line), in)) {
        int id = atoi(line);
        char *rest = strchr(line, '\t');
        if (!rest) { fputs(line, out); continue; }
        if (id == skip_id) {
            if (set_deleted) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                char *end = strchr(line, '\n');
                if (end) *end = '\0';
                end = line + strlen(line);
                while (end > line && (end[-1] == '\t' || end[-1] == ' ')) end--;
                *end = '\0';
                fprintf(out, "%s\t%s\n", line, ts);
            } else if (new_name != NULL) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                fprintf(out, "%d\t%s\t%s\t%s\t\t%s\t\n", id, new_name, new_email ? new_email : "", new_phone ? new_phone : "", ts);
            }
            ok = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_contacts_update(int id, const char *name, const char *email, const char *phone) {
    char n[VIBE_NAME_MAX], e[VIBE_EMAIL_MAX], p[VIBE_PHONE_MAX];
    sanitize(n, name ? name : "", VIBE_NAME_MAX);
    sanitize(e, email ? email : "", VIBE_EMAIL_MAX);
    sanitize(p, phone ? phone : "", VIBE_PHONE_MAX);
    return contacts_rewrite(id, 0, n, e, p);
}

int storage_contacts_delete(int id) {
    return contacts_rewrite(id, 1, NULL, NULL, NULL);
}

/* Events: id\ttitle\tdesc\tstart_at\tend_at\tall_day\tcreated_at (trailing tab = not deleted) */
int storage_events_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events.txt");
    FILE *f = fopen(path, "r");
    int n = 0;
    if (f) {
        char line[LINE_MAX];
        while (fgets(line, sizeof(line), f)) {
            int id;
            if (sscanf(line, "%d", &id) != 1) continue;
            char *last = line + strlen(line) - 1;
            while (last > line && (*last == '\n' || *last == '\r')) last--;
            if (last > line && *last == '\t') n++;
        }
        fclose(f);
    }
    return n;
}
int storage_events_add(const char *title, const char *desc, const char *start_at, const char *end_at, int all_day) {
    (void)desc;
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events.txt");
    int id = next_id("events.txt");
    char t[VIBE_TITLE_MAX];
    sanitize(t, title ? title : "", VIBE_TITLE_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    fprintf(f, "%d\t%s\t\t%s\t%s\t%d\t%s\t\n", id, t, start_at ? start_at : "", end_at ? end_at : "", all_day, ts);
    fclose(f);
    return id;
}
void storage_events_list(void (*cb)(const VibeCalendarEvent *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "events.txt");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        VibeCalendarEvent e = {0};
        char *p = line;
        char *q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; e.id = atoi(p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(e.title, sizeof(e.title), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        p = q + 1; /* skip desc */
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(e.start_at, sizeof(e.start_at), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(e.end_at, sizeof(e.end_at), "%s", p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; e.all_day = atoi(p); *q = '\t'; p = q + 1;
        q = strchr(p, '\t');
        if (!q) continue;
        *q = '\0'; snprintf(e.created_at, sizeof(e.created_at), "%s", p); *q = '\t'; p = q + 1;
        if (p[0] != '\t' && p[0] != '\n' && p[0] != '\0') continue;
        cb(&e, ctx);
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
    data_path(path, sizeof(path), "events.txt");
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "r");
    if (!in) return 0;
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return 0; }
    char line[LINE_MAX];
    int ok = 0;
    while (fgets(line, sizeof(line), in)) {
        int id = atoi(line);
        char *rest = strchr(line, '\t');
        if (!rest) { fputs(line, out); continue; }
        if (id == skip_id) {
            if (set_deleted) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                char *end = strchr(line, '\n');
                if (end) *end = '\0';
                end = line + strlen(line);
                while (end > line && (end[-1] == '\t' || end[-1] == ' ')) end--;
                *end = '\0';
                fprintf(out, "%s\t%s\n", line, ts);
            } else if (new_title != NULL) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                fprintf(out, "%d\t%s\t%s\t%s\t%s\t%d\t%s\t\n", id, new_title, new_desc ? new_desc : "",
                    new_start ? new_start : "", new_end ? new_end : "", new_all_day, ts);
            }
            ok = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_events_update(int id, const char *title, const char *desc, const char *start_at, const char *end_at, int all_day) {
    char t[VIBE_TITLE_MAX], d[VIBE_CONTENT_MAX], s[VIBE_DATETIME_MAX], e[VIBE_DATETIME_MAX];
    sanitize(t, title ? title : "", VIBE_TITLE_MAX);
    sanitize(d, desc ? desc : "", VIBE_CONTENT_MAX);
    sanitize(s, start_at ? start_at : "", VIBE_DATETIME_MAX);
    sanitize(e, end_at ? end_at : "", VIBE_DATETIME_MAX);
    return events_rewrite(id, 0, t, d, s, e, all_day);
}

int storage_events_delete(int id) {
    return events_rewrite(id, 1, NULL, NULL, NULL, NULL, 0);
}

/* Facts: id\tkey\tvalue\tcreated_at\tdeleted_at (empty = not deleted) */
int storage_facts_count(void) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts.txt");
    FILE *f = fopen(path, "r");
    int n = 0;
    if (f) {
        char line[LINE_MAX];
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            int tabs = 0;
            while (*p) { if (*p == '\t') tabs++; p++; }
            if (tabs < 4) continue;
            p--;
            while (p > line && (*p == '\n' || *p == '\r')) p--;
            if (p > line && *p == '\t') n++;
        }
        fclose(f);
    }
    return n;
}

int storage_facts_add(const char *key, const char *value) {
    ensure_data_dir();
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts.txt");
    int id = next_id("facts.txt");
    char k[VIBE_TITLE_MAX];
    sanitize(k, key ? key : "", VIBE_TITLE_MAX);
    char v[VIBE_CONTENT_MAX];
    sanitize(v, value ? value : "", VIBE_CONTENT_MAX);
    char ts[VIBE_DATETIME_MAX];
    timestamp(ts, sizeof(ts));
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    fprintf(f, "%d\t%s\t%s\t%s\t\n", id, k, v, ts);
    fclose(f);
    return id;
}

void storage_facts_list(void (*cb)(const VibeFact *, void *), void *ctx) {
    char path[DATA_DIR_MAX + 64];
    data_path(path, sizeof(path), "facts.txt");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        VibeFact fact = {0};
        char *p = line;
        if (sscanf(p, "%d", &fact.id) != 1) continue;
        p = strchr(p, '\t');
        if (!p) continue;
        p++;
        char *key_end = strchr(p, '\t');
        if (!key_end) continue;
        *key_end = '\0';
        snprintf(fact.key, sizeof(fact.key), "%s", p);
        p = key_end + 1;
        char *val_end = strchr(p, '\t');
        if (!val_end) continue;
        *val_end = '\0';
        snprintf(fact.value, sizeof(fact.value), "%s", p);
        p = val_end + 1;
        char *created_end = strchr(p, '\t');
        if (!created_end) continue;
        *created_end = '\0';
        snprintf(fact.created_at, sizeof(fact.created_at), "%s", p);
        p = created_end + 1;
        char *deleted_end = strchr(p, '\n');
        if (deleted_end) *deleted_end = '\0';
        if (p[0] == '\0' || p[0] == '\t') {
            fact.deleted_at[0] = '\0';
            if (cb) cb(&fact, ctx);
        } else {
            snprintf(fact.deleted_at, sizeof(fact.deleted_at), "%s", p);
        }
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
    data_path(path, sizeof(path), "facts.txt");
    char tmp[DATA_DIR_MAX + 72];
    snprintf(tmp, sizeof(tmp), "%s.$$$", path);
    FILE *in = fopen(path, "r");
    if (!in) return 0;
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return 0; }
    char line[LINE_MAX];
    int ok = 0;
    while (fgets(line, sizeof(line), in)) {
        int id = atoi(line);
        char *rest = strchr(line, '\t');
        if (!rest) { fputs(line, out); continue; }
        if (id == skip_id) {
            if (set_deleted) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                char *end = strchr(line, '\n');
                if (end) *end = '\0';
                end = line + strlen(line);
                while (end > line && (end[-1] == '\t' || end[-1] == ' ')) end--;
                *end = '\0';
                fprintf(out, "%s\t%s\n", line, ts);
            } else if (new_key != NULL) {
                char ts[VIBE_DATETIME_MAX];
                timestamp(ts, sizeof(ts));
                fprintf(out, "%d\t%s\t%s\t%s\t\n", id, new_key, new_value ? new_value : "", ts);
            }
            ok = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (ok) { remove(path); rename(tmp, path); }
    else remove(tmp);
    return ok;
}

int storage_facts_update(int id, const char *key, const char *value) {
    char k[VIBE_TITLE_MAX], v[VIBE_CONTENT_MAX];
    sanitize(k, key ? key : "", VIBE_TITLE_MAX);
    sanitize(v, value ? value : "", VIBE_CONTENT_MAX);
    return facts_rewrite(id, 0, k, v);
}

int storage_facts_delete(int id) {
    return facts_rewrite(id, 1, NULL, NULL);
}

int storage_trash_count(void) { return 0; }
void storage_trash_list(void (*cb)(int entity_type, int id, const char *title, void *), void *ctx) { (void)cb;(void)ctx; }
int storage_restore(int entity_type, int id) { (void)entity_type;(void)id; return 0; }
