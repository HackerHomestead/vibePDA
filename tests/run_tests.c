/* run_tests.c - Test runner for vibePDA unit tests */

#include <stdio.h>
#include <stdlib.h>

extern void test_app(void);
extern void test_storage(void);
extern void test_trash_integration(void);
extern void test_config(void);
extern void test_fuzz(void);

static int verbose(void) {
    const char *v = getenv("VIBE_TEST_VERBOSE");
    return !v || v[0] == '\0' || v[0] == '1' || v[0] == 'y' || v[0] == 'Y';
}

int main(void) {
    int failed = 0;
    int v = verbose();
    fprintf(stderr, "Running app (UI) tests...\n");
    if (v) fprintf(stderr, "  app_init, module switch, focus, quit, help, F2/N, content editor, trash\n");
    test_app();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running storage (backend) tests...\n");
    if (v) fprintf(stderr, "  empty, migration, corruption, CRUD, trash, search\n");
    test_storage();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running trash integration tests...\n");
    if (v) fprintf(stderr, "  create/delete, trash list, restore, permanent delete, empty\n");
    test_trash_integration();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running configuration tests...\n");
    if (v) fprintf(stderr, "  defaults, data-dir override\n");
    test_config();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running fuzz/sanity tests...\n");
    if (v) fprintf(stderr, "  long strings, empty/NULL, special chars, numeric boundaries, nonexistent ID, filter, malicious length\n");
    test_fuzz();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "All tests passed.\n");
    return failed ? EXIT_FAILURE : 0;
}
