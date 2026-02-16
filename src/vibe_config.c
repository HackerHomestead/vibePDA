/* vibe_config.c - Config layer (paths, defaults).
 *
 * XDG-style: ~/.local/share/vibe for data on Linux.
 * VIBE_DATA env overrides data dir on DOS. Database path is legacy.
 * ~/.config/vibe/vibe.env: optional env file (from configure_terminal.py).
 */
#include "vibe_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif

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

/* Load ~/.config/vibe/vibe.env and apply export KEY=VAL via setenv.
 * Only sets vars not already in environment (env takes precedence). */
void vibe_config_load_env_file(void) {
#ifdef PLATFORM_LINUX
    const char *home = getenv("HOME");
    if (!home || !home[0]) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/vibe/vibe.env", home);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "export ", 7) == 0) p += 7;
        else if (*p == '#' || *p == '\n' || *p == '\0') continue;
        char *eq = strchr(p, '=');
        if (!eq || eq == p) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        size_t n = strlen(val);
        while (n > 0 && (val[n - 1] == '\n' || val[n - 1] == '\r')) val[--n] = '\0';
        if (getenv(key) != NULL) continue; /* env overrides file */
        setenv(key, val, 0);
    }
    fclose(f);
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
