/* app.h - Application state and screens */

#ifndef APP_H
#define APP_H

#define MODULE_NOTES    0
#define MODULE_TASKS    1
#define MODULE_CONTACTS 2
#define MODULE_CALENDAR 3
#define MODULE_TRASH    4
#define MODULE_COUNT    5

typedef struct {
    int current_module;
    int focus_sidebar; /* 1 = sidebar, 0 = main */
    int rows;
    int cols;
} AppState;

void app_init(AppState *a, int rows, int cols);
void app_handle_key(AppState *a, int key);
void app_draw(const AppState *a);

#endif
