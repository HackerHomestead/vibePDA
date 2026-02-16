/* storage_io.h - Binary I/O helpers for storage layer (internal).
 *
 * Little-endian uint32_t and length-prefixed strings.
 * Used by storage_file.c only.
 */

#ifndef STORAGE_IO_H
#define STORAGE_IO_H

#include <stdint.h>
#include <stdio.h>

#define STORAGE_IO_MAX_STRING_LEN (16 * 1024 * 1024)  /* 16MB cap */

int storage_io_write_u32(FILE *f, uint32_t v);
int storage_io_write_str(FILE *f, const char *s);
int storage_io_read_u32(FILE *f, uint32_t *out);
int storage_io_read_str(FILE *f, char *buf, int max);
int storage_io_skip_str(FILE *f);

#endif
