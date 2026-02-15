/* vibe_config.c - Config layer (paths, defaults) */

#include "vibe_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void vibe_config_default_db_path(char *buf, int size) {
    if (!buf || size <= 0) return;
    buf[0] = '\0';
#ifdef PLATFORM_LINUX
    {
        const char *home = getenv("HOME");
        if (home)
            snprintf(buf, size, "%s/.local/share/vibe/vibe.db", home);
        else
            snprintf(buf, size, "vibe.db");
    }
#else
    snprintf(buf, size, "vibe.db");
#endif
}

void vibe_config_default_data_dir(char *buf, int size) {
    if (!buf || size <= 0) return;
    buf[0] = '\0';
#ifdef PLATFORM_LINUX
    {
        const char *home = getenv("HOME");
        if (home)
            snprintf(buf, size, "%s/.local/share/vibe", home);
        else
            snprintf(buf, size, ".");
    }
#else
    {
        const char *d = getenv("VIBE_DATA");
        if (d && d[0])
            snprintf(buf, size, "%s", d);
        else
            snprintf(buf, size, ".");
    }
#endif
}

void vibe_config_load(VibeConfig *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    vibe_config_default_db_path(out->database_path, sizeof(out->database_path));
    vibe_config_default_data_dir(out->data_dir, sizeof(out->data_dir));
    out->editor_path[0] = '\0';
    out->default_module = 0;
}

void vibe_config_print(const VibeConfig *cfg) {
    if (!cfg) return;
    printf("vibePDA Configuration:\n");
    printf("  Data directory: %s\n", cfg->data_dir[0] ? cfg->data_dir : "(default)");
    printf("  Database path: %s\n", cfg->database_path[0] ? cfg->database_path : "(not used)");
    printf("  Editor path: %s\n", cfg->editor_path[0] ? cfg->editor_path : "(not set)");
    printf("  Default module: %d\n", cfg->default_module);
#ifdef PLATFORM_LINUX
    {
        const char *home = getenv("HOME");
        const char *data_env = getenv("VIBE_DATA");
        printf("\nEnvironment:\n");
        printf("  HOME: %s\n", home ? home : "(not set)");
        printf("  VIBE_DATA: %s\n", data_env ? data_env : "(not set)");
    }
#endif
}
