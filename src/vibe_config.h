/* vibe_config.h - Config layer (paths, defaults). Distinct from build config.h. */

#ifndef VIBE_CONFIG_H
#define VIBE_CONFIG_H

#define VIBE_CONFIG_DB_PATH_MAX 256
#define VIBE_CONFIG_EDITOR_PATH_MAX 128
#define VIBE_CONFIG_DATA_DIR_MAX 256

typedef struct {
    char database_path[VIBE_CONFIG_DB_PATH_MAX];
    char data_dir[VIBE_CONFIG_DATA_DIR_MAX]; /* directory for notes.txt, tasks.txt, etc. */
    char editor_path[VIBE_CONFIG_EDITOR_PATH_MAX];
    int default_module; /* 0=Notes, 1=Tasks, 2=Contacts, 3=Calendar */
} VibeConfig;

void vibe_config_load(VibeConfig *out);
void vibe_config_default_db_path(char *buf, int size);
void vibe_config_default_data_dir(char *buf, int size);

#endif
