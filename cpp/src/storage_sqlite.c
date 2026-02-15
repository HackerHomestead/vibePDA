/* storage_sqlite.c - SQLite storage (Linux, when USE_SQLITE=1) */

#include "config.h"
#include <stdio.h>

#ifdef PLATFORM_LINUX
/* SQLite backend for Linux; link with -lsqlite3 */
void storage_sqlite_init(void) {
    (void)0;
}
#endif
