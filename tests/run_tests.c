/* run_tests.c - Test runner for vibePDA unit tests */

#include <stdio.h>
#include <stdlib.h>

extern void test_app(void);
extern void test_storage(void);
extern void test_trash_integration(void);
extern void test_config(void);

int main(void) {
    int failed = 0;
    fprintf(stderr, "Running app (UI) tests...\n");
    test_app();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running storage (backend) tests...\n");
    test_storage();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running trash integration tests...\n");
    test_trash_integration();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "Running configuration tests...\n");
    test_config();
    fprintf(stderr, "  OK\n");
    fprintf(stderr, "All tests passed.\n");
    return failed ? EXIT_FAILURE : 0;
}
