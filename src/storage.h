/* storage.h - Storage backend API (file-based). Data layer. */

#ifndef STORAGE_H
#define STORAGE_H

#include "types.h"

/* Initialize storage; pass data directory (e.g. ~/.local/share/vibe). Call once at startup. */
void storage_init(const char *data_dir);

/* Counts. Exclude soft-deleted unless noted. */
int storage_notes_count(void);
int storage_tasks_count(void);
int storage_contacts_count(void);
int storage_events_count(void);
int storage_trash_count(void);

/* Notes CRUD */
int storage_notes_add(const char *title, const char *content);
void storage_notes_list(void (*cb)(const VibeNote *, void *), void *ctx);
int storage_note_get(int id, VibeNote *out);
int storage_notes_update(int id, const char *title, const char *content);
int storage_notes_delete(int id);

/* Tasks CRUD */
int storage_tasks_add(const char *title, const char *due_date, int priority);
void storage_tasks_list(void (*cb)(const VibeTask *, void *), void *ctx);
int storage_task_get(int id, VibeTask *out);
int storage_tasks_update(int id, const char *title, const char *due_date, int priority, int done);
int storage_tasks_delete(int id);

/* Contacts CRUD */
int storage_contacts_add(const char *name, const char *email, const char *phone);
void storage_contacts_list(void (*cb)(const VibeContact *, void *), void *ctx);
int storage_contact_get(int id, VibeContact *out);
int storage_contacts_update(int id, const char *name, const char *email, const char *phone);
int storage_contacts_delete(int id);

/* Calendar events CRUD */
int storage_events_add(const char *title, const char *desc, const char *start_at, const char *end_at, int all_day);
void storage_events_list(void (*cb)(const VibeCalendarEvent *, void *), void *ctx);
int storage_event_get(int id, VibeCalendarEvent *out);
int storage_events_update(int id, const char *title, const char *desc, const char *start_at, const char *end_at, int all_day);
int storage_events_delete(int id);

/* Trash: list deleted items; restore by entity type (0=note,1=task,2=contact,3=event) and id */
void storage_trash_list(void (*cb)(int entity_type, int id, const char *title, void *), void *ctx);
int storage_restore(int entity_type, int id);

#endif
