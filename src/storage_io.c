/* storage_io.c - Binary I/O helpers (little-endian). */

#include "storage_io.h"
#include <string.h>

int storage_io_write_u32(FILE *f, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    b[2] = (unsigned char)((v >> 16) & 0xff);
    b[3] = (unsigned char)((v >> 24) & 0xff);
    return fwrite(b, 1, 4, f) == 4;
}

int storage_io_write_str(FILE *f, const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len > 0x7fffffff) len = 0x7fffffff;
    if (!storage_io_write_u32(f, (uint32_t)len)) return 0;
    if (len > 0 && fwrite(s, 1, len, f) != len) return 0;
    return 1;
}

int storage_io_read_u32(FILE *f, uint32_t *out) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 1;
}

int storage_io_read_str(FILE *f, char *buf, int max) {
    uint32_t len;
    if (!storage_io_read_u32(f, &len)) return 0;
    if (len == 0) { buf[0] = '\0'; return 1; }
    if (len > STORAGE_IO_MAX_STRING_LEN) len = STORAGE_IO_MAX_STRING_LEN;
    if (len >= (uint32_t)max) {
        size_t to_read = (size_t)(max - 1);
        if (fread(buf, 1, to_read, f) != to_read) return 0;
        buf[max - 1] = '\0';
        if (fseek(f, (long)(len - (max - 1)), SEEK_CUR) != 0) return 0;
    } else {
        if (fread(buf, 1, len, f) != len) return 0;
        buf[len] = '\0';
    }
    return 1;
}

int storage_io_skip_str(FILE *f) {
    uint32_t len;
    if (!storage_io_read_u32(f, &len)) return 0;
    if (len > STORAGE_IO_MAX_STRING_LEN) len = STORAGE_IO_MAX_STRING_LEN;
    if (fseek(f, (long)len, SEEK_CUR) != 0) return 0;
    return 1;
}
