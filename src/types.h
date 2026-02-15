/* types.h - Data types for vibePDA entities.
 *
 * All structs use fixed-size buffers. Soft-delete via deleted_at (non-empty = deleted).
 */

#ifndef VIBE_TYPES_H
#define VIBE_TYPES_H

/* Field size limits (bytes, including NUL) */
#define VIBE_TITLE_MAX 256
#define VIBE_CONTENT_MAX 4096
#define VIBE_NAME_MAX 128
#define VIBE_EMAIL_MAX 128
#define VIBE_PHONE_MAX 64
#define VIBE_DATETIME_MAX 32

typedef struct {
    int id;
    char title[VIBE_TITLE_MAX];
    char content[VIBE_CONTENT_MAX];
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeNote;

typedef struct {
    int id;
    char title[VIBE_TITLE_MAX];
    int done;
    char due_date[VIBE_DATETIME_MAX];
    int priority;
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeTask;

typedef struct {
    int id;
    char name[VIBE_NAME_MAX];
    char email[VIBE_EMAIL_MAX];
    char phone[VIBE_PHONE_MAX];
    char notes[VIBE_CONTENT_MAX];
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeContact;

typedef struct {
    int id;
    char title[VIBE_TITLE_MAX];
    char description[VIBE_CONTENT_MAX];
    char start_at[VIBE_DATETIME_MAX];
    char end_at[VIBE_DATETIME_MAX];
    int all_day;
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeCalendarEvent;

typedef struct {
    int id;
    char key[VIBE_TITLE_MAX];
    char value[VIBE_CONTENT_MAX];
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeFact;

typedef struct {
    int id;
    char date[VIBE_DATETIME_MAX];
    char description[VIBE_TITLE_MAX];
    double amount;
    char category[VIBE_TITLE_MAX];
    char account[VIBE_TITLE_MAX];
    char notes[VIBE_CONTENT_MAX];
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeFinanceEntry;

typedef struct {
    int id;
    char title[VIBE_TITLE_MAX];
    char template_name[VIBE_TITLE_MAX];
    char content[VIBE_CONTENT_MAX];
    char created_at[VIBE_DATETIME_MAX];
    char deleted_at[VIBE_DATETIME_MAX];
} VibeDocument;

#endif
