/* test_common.h - Shared helpers for unit tests (TDD-friendly) */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int test_verbose(void) {
    const char *v = getenv("VIBE_TEST_VERBOSE");
    if (!v || v[0] == '\0') return 1;  /* default: verbose */
    if (v[0] == '0' || v[0] == 'n' || v[0] == 'N') return 0;
    return 1;  /* 1, y, Y, or anything else: verbose */
}

/* TDD assertion macros: fail with file:line and message for easier debugging */
#define TEST_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL %s:%d: %s (got %d, expected %d)\n", __FILE__, __LINE__, (msg), (int)(a), (int)(b)); \
        abort(); \
    } \
} while (0)

#define TEST_STR_EQ(a, b, msg) do { \
    if (strcmp((a), (b)) != 0) { \
        fprintf(stderr, "FAIL %s:%d: %s (got \"%s\", expected \"%s\")\n", __FILE__, __LINE__, (msg), (a), (b)); \
        abort(); \
    } \
} while (0)

#define TEST_TRUE(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
        abort(); \
    } \
} while (0)

#endif
