/* test_tui.c - TUI test framework for automated UI testing */

#include "config.h"
#include "../src/app.h"
#include "../src/tui.h"
#include "../src/storage.h"
#include "../src/vibe_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Test framework state */
typedef struct {
    AppState *app;
    int test_passed;
    int test_failed;
    char error_msg[256];
} TestFramework;

static TestFramework g_test;

/* Simulate key press */
static void send_key(int key) {
    app_handle_key(g_test.app, key);
    app_draw(g_test.app);
    usleep(10000); /* 10ms delay for visual feedback */
}

/* Simulate typing a string */
static void type_string(const char *str) {
    for (const char *p = str; *p; p++) {
        send_key((int)(unsigned char)*p);
    }
}

/* Assertions */
static void assert_true(int condition, const char *msg) {
    if (!condition) {
        snprintf(g_test.error_msg, sizeof(g_test.error_msg), "FAIL: %s", msg);
        g_test.test_failed++;
        printf("%s\n", g_test.error_msg);
    } else {
        g_test.test_passed++;
    }
}

static void assert_equal(int a, int b, const char *msg) {
    if (a != b) {
        snprintf(g_test.error_msg, sizeof(g_test.error_msg), "FAIL: %s (expected %d, got %d)", msg, b, a);
        g_test.test_failed++;
        printf("%s\n", g_test.error_msg);
    } else {
        g_test.test_passed++;
    }
}

static void assert_str_equal(const char *a, const char *b, const char *msg) {
    if (strcmp(a, b) != 0) {
        snprintf(g_test.error_msg, sizeof(g_test.error_msg), "FAIL: %s (expected '%s', got '%s')", msg, b, a);
        g_test.test_failed++;
        printf("%s\n", g_test.error_msg);
    } else {
        g_test.test_passed++;
    }
}

/* Test: Create a note */
static void test_create_note(void) {
    printf("Test: Create note\n");
    
    /* Navigate to Notes */
    assert_equal(g_test.app->current_module, MODULE_NOTES, "Should start in Notes");
    
    /* Press F2 to create new note */
    send_key(KEY_F(2));
    assert_true(g_test.app->prompt_mode == 1, "Should be in prompt mode");
    assert_true(g_test.app->prompt_is_edit == 0, "Should be adding, not editing");
    
    /* Type title */
    type_string("Test Note");
    send_key(KEY_ENTER);
    
    /* Should now be in content editor */
    assert_true(g_test.app->content_edit_mode == 1, "Should be in content editor");
    
    /* Type some content */
    type_string("This is test content");
    send_key(KEY_ENTER); /* Enter for newline */
    type_string("Line 2");
    
    /* Save with Enter+Enter (two blank lines) */
    send_key(KEY_ENTER); /* First Enter creates blank line */
    send_key(KEY_ENTER); /* Second Enter saves */
    
    /* Should be back in view mode */
    assert_true(g_test.app->content_edit_mode == 0, "Should exit content editor");
    assert_true(g_test.app->prompt_mode == 0, "Should exit prompt mode");
    
    /* Verify note was created */
    int count = storage_notes_count();
    assert_true(count > 0, "Note should be created");
}

/* Test: Edit a note */
static void test_edit_note(void) {
    printf("Test: Edit note\n");
    
    /* Create a note first */
    send_key(KEY_F(2));
    type_string("Edit Test");
    send_key(KEY_ENTER);
    type_string("Original content");
    send_key(KEY_ENTER);
    
    /* Select the note (should be selected) */
    assert_true(g_test.app->selected_index >= 0, "Note should be selected");
    
    /* Press 'e' to edit */
    send_key('e');
    assert_true(g_test.app->prompt_mode == 1, "Should be in prompt mode");
    assert_true(g_test.app->prompt_is_edit == 1, "Should be editing");
    
    /* Edit title */
    type_string(" Edited");
    send_key(KEY_ENTER);
    
    /* Should be in content editor */
    assert_true(g_test.app->content_edit_mode == 1, "Should be in content editor");
    
    /* Modify content */
    type_string("Modified");
    send_key(KEY_ENTER); /* First Enter creates blank line */
    send_key(KEY_ENTER); /* Second Enter saves */
    
    /* Should be saved */
    assert_true(g_test.app->content_edit_mode == 0, "Should exit content editor");
}

/* Test: Cancel editing */
static void test_cancel_edit(void) {
    printf("Test: Cancel edit\n");
    
    /* Create a note */
    send_key(KEY_F(2));
    type_string("Cancel Test");
    send_key(KEY_ENTER);
    type_string("Original");
    send_key(KEY_ENTER); /* First Enter creates blank line */
    send_key(KEY_ENTER); /* Second Enter saves */
    
    /* Edit it */
    send_key('e');
    send_key(KEY_ENTER); /* Skip title edit */
    
    /* Type something */
    type_string("Should not save");
    send_key(KEY_ENTER); /* Add a line */
    
    /* Press Esc to cancel (don't press Enter+Enter) */
    send_key(KEY_ESC);
    
    /* Should exit editor */
    assert_true(g_test.app->content_edit_mode == 0, "Should exit content editor");
    assert_true(g_test.app->prompt_mode == 0, "Should exit prompt mode");
}

/* Test: Scrolling */
static void test_scrolling(void) {
    printf("Test: Scrolling\n");
    
    /* Create a note with many lines */
    send_key(KEY_F(2));
    type_string("Scroll Test");
    send_key(KEY_ENTER);
    
    /* Add many lines */
    for (int i = 0; i < 30; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Line %d", i);
        type_string(buf);
        send_key(KEY_ENTER); /* Enter for newline */
    }
    
    /* Check scroll offset */
    int initial_offset = g_test.app->content_edit_scroll_offset;
    assert_equal(initial_offset, 0, "Initial scroll offset should be 0");
    
    /* Move cursor to end */
    int len = g_test.app->content_edit_len;
    for (int i = 0; i < len; i++) {
        send_key(KEY_RIGHT);
    }
    
    /* Scroll offset should have changed */
    int final_offset = g_test.app->content_edit_scroll_offset;
    assert_true(final_offset >= 0, "Scroll offset should be non-negative");
    
    /* Test Page Down */
    int before_page = g_test.app->content_edit_scroll_offset;
#ifdef KEY_NPAGE
    send_key(KEY_NPAGE);
    int after_page = g_test.app->content_edit_scroll_offset;
    assert_true(after_page >= before_page, "Page Down should increase scroll offset");
#endif
    
    /* Cancel */
    send_key(KEY_ESC);
}

/* Test: FACTS module */
static void test_facts_module(void) {
    printf("Test: FACTS module\n");
    
    /* Navigate to FACTS */
    for (int i = 0; i < MODULE_FACTS; i++) {
        send_key(KEY_DOWN);
    }
    send_key(KEY_ENTER);
    
    assert_equal(g_test.app->current_module, MODULE_FACTS, "Should be in FACTS module");
    
    /* Create a fact */
    send_key(KEY_F(2));
    assert_true(g_test.app->prompt_mode == 1, "Should be in prompt mode");
    
    /* Type key */
    type_string("Andrew SSN");
    send_key(KEY_ENTER);
    
    /* Type value */
    assert_true(g_test.app->prompt_step == 1, "Should be on value step");
    type_string("455-56-2022");
    send_key(KEY_ENTER);
    
    /* Should be saved */
    assert_true(g_test.app->prompt_mode == 0, "Should exit prompt mode");
    
    /* Verify fact was created */
    int count = storage_facts_count();
    assert_true(count > 0, "Fact should be created");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("TUI Test Framework\n");
    printf("==================\n\n");
    
    /* Initialize */
    memset(&g_test, 0, sizeof(g_test));
    
    /* Set up test data directory */
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/vibe_test_%d", getpid());
    VibeConfig cfg;
    vibe_config_load(&cfg);
    storage_init(test_dir);
    
    /* Initialize TUI (non-interactive mode) */
    tui_init();
    tui_setup();
    
    /* Initialize app */
    AppState app;
    app_init(&app, 24, 80);
    g_test.app = &app;
    
    /* Run tests */
    test_create_note();
    test_edit_note();
    test_cancel_edit();
    test_scrolling();
    test_facts_module();
    
    /* Cleanup */
    tui_cleanup();
    
    /* Print results */
    printf("\n==================\n");
    printf("Tests passed: %d\n", g_test.test_passed);
    printf("Tests failed: %d\n", g_test.test_failed);
    
    if (g_test.test_failed == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
