/* test_common.h - Shared helpers for unit tests */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdlib.h>

static inline int test_verbose(void) {
    const char *v = getenv("VIBE_TEST_VERBOSE");
    if (!v || v[0] == '\0') return 1;  /* default: verbose */
    if (v[0] == '0' || v[0] == 'n' || v[0] == 'N') return 0;
    return 1;  /* 1, y, Y, or anything else: verbose */
}

#endif
