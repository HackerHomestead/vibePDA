/* storage.h - Storage backend API (file-based). Data layer.
 *
 * All entities use soft-delete (deleted_at). Trash lists deleted items;
 * restore clears deleted_at; permanent_delete removes records entirely.
 * entity_type for trash/restore: 0=note, 1=task, 2=contact, 3=event, 4=fact.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include "types.h"

/* Initialize storage. Pass data directory (e.g. ~/.local/share/vibe).
 * Call once at startup. Migrates .txt to .bin on Linux if needed. */
void storage_init(const char *data_dir);

/* Counts. Exclude soft-deleted unless noted. */
int storage_notes_count(void);
int storage_tasks_count(void);
int storage_contacts_count(void);
int storage_events_count(void);
int storage_facts_count(void);
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
int storage_contacts_add(const char *name, const char *email, const char *phone, const char *notes);
void storage_contacts_list(void (*cb)(const VibeContact *, void *), void *ctx);
int storage_contact_get(int id, VibeContact *out);
int storage_contacts_update(int id, const char *name, const char *email, const char *phone, const char *notes);
int storage_contacts_delete(int id);

/* Calendar events CRUD */
int storage_events_add(const char *title, const char *desc, const char *start_at, const char *end_at, int all_day);
void storage_events_list(void (*cb)(const VibeCalendarEvent *, void *), void *ctx);
int storage_event_get(int id, VibeCalendarEvent *out);
int storage_events_update(int id, const char *title, const char *desc, const char *start_at, const char *end_at, int all_day);
int storage_events_delete(int id);

/* Facts CRUD */
int storage_facts_add(const char *key, const char *value);
void storage_facts_list(void (*cb)(const VibeFact *, void *), void *ctx);
int storage_fact_get(int id, VibeFact *out);
int storage_facts_update(int id, const char *key, const char *value);
int storage_facts_delete(int id);

/* Finances CRUD */
int storage_finances_add(const char *date, const char *description, double amount, const char *category, const char *account, const char *notes);
void storage_finances_list(void (*cb)(const VibeFinanceEntry *, void *), void *ctx);
int storage_finance_get(int id, VibeFinanceEntry *out);
int storage_finances_update(int id, const char *date, const char *description, double amount, const char *category, const char *account, const char *notes);
int storage_finances_delete(int id);
int storage_finances_count(void);

/* Documents CRUD */
int storage_documents_add(const char *title, const char *template_name, const char *content);
void storage_documents_list(void (*cb)(const VibeDocument *, void *), void *ctx);
int storage_document_get(int id, VibeDocument *out);
int storage_documents_update(int id, const char *title, const char *template_name, const char *content);
int storage_documents_delete(int id);
int storage_documents_count(void);

/* Trash: list deleted items; restore by entity type (0=note,1=task,2=contact,3=event,4=fact,5=finance,6=document) and id */
void storage_trash_list(void (*cb)(int entity_type, int id, const char *title, void *), void *ctx);
int storage_restore(int entity_type, int id);
/* Permanent delete: actually remove records from storage (cannot be undone) */
int storage_permanent_delete(int entity_type, int id);
int storage_empty_trash(void);

/* Search/Filter: filter list results by query (case-insensitive substring match) */
/* Pass NULL or empty string to disable filtering */
void storage_set_search_filter(const char *query);

/* Notes search */
void storage_notes_list_filtered(void (*cb)(const VibeNote *, void *), void *ctx, const char *query);
/* Tasks search */
void storage_tasks_list_filtered(void (*cb)(const VibeTask *, void *), void *ctx, const char *query);
/* Contacts search */
void storage_contacts_list_filtered(void (*cb)(const VibeContact *, void *), void *ctx, const char *query);
/* Events search */
void storage_events_list_filtered(void (*cb)(const VibeCalendarEvent *, void *), void *ctx, const char *query);
/* Facts search */
void storage_facts_list_filtered(void (*cb)(const VibeFact *, void *), void *ctx, const char *query);
/* Finances search */
void storage_finances_list_filtered(void (*cb)(const VibeFinanceEntry *, void *), void *ctx, const char *query);
/* Documents search */
void storage_documents_list_filtered(void (*cb)(const VibeDocument *, void *), void *ctx, const char *query);

#endif
