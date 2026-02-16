/* test_config.c - Unit tests for configuration options (--config, --data-dir) */

#include "vibe_config.h"
#include "storage.h"
#include "test_common.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef PLATFORM_LINUX

static void test_config_defaults(void) {
    VibeConfig cfg;
    vibe_config_load(&cfg);
    
    /* Default data dir should be set */
    assert(strlen(cfg.data_dir) > 0);
    
    /* Should contain .local/share/vibe on Linux */
    const char *home = getenv("HOME");
    if (home) {
        assert(strstr(cfg.data_dir, ".local/share/vibe") != NULL || 
               strcmp(cfg.data_dir, ".") == 0);
    }
}

static void test_data_dir_override(void) {
    /* Create a temporary directory */
    char tmpdir[] = "/tmp/vibe_test_XXXXXX";
    assert(mkdtemp(tmpdir) != NULL);
    
    /* Initialize storage with custom directory */
    storage_init(tmpdir);
    
    /* Add a note */
    int id = storage_notes_add("Test Note", "Test Content");
    assert(id > 0);
    
    /* Verify file exists in custom directory */
    char notes_file[512];
    snprintf(notes_file, sizeof(notes_file), "%s/notes.bin", tmpdir);
    FILE *f = fopen(notes_file, "r");
    assert(f != NULL);
    fclose(f);
    
    /* Cleanup */
    unlink(notes_file);
    rmdir(tmpdir);
}

void test_config(void) {
    if (test_verbose()) fprintf(stderr, "    config_defaults\n");
    test_config_defaults();
    if (test_verbose()) fprintf(stderr, "    data_dir_override\n");
    test_data_dir_override();
}

#else
/* Non-Linux platforms: stub */
void test_config(void) {
    /* Configuration tests are Linux-specific */
}
#endif
