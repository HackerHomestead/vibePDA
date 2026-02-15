# Code Review — vibePDA (Senior Developer Perspective)

**Date:** 2026-02-15  
**Scope:** Full codebase (C port, alpha)

---

## Summary

The codebase is well-structured for an alpha-stage project. The storage layer, TUI, and app shell are cleanly separated. The following improvements were applied and recommendations documented.

---

## Changes Applied

### 1. Storage Layer (`src/storage_file.c`)

- **File header**: Added binary format documentation (little-endian, length-prefixed strings).
- **Entity type constants**: Introduced `ENTITY_NOTE`, `ENTITY_TASK`, etc. to replace magic numbers in `storage_restore` and `storage_permanent_delete`.
- **Migration byte order**: Migration from `.txt` to `.bin` now uses `write_u32()` for all 4-byte writes, ensuring little-endian consistency.
- **copy_str safety**: Added null/dst and `max <= 0` guards to avoid undefined behavior.

### 2. API Documentation

- **storage.h**: Clarified entity_type values and soft-delete semantics.
- **types.h**: Documented field size limits and soft-delete convention.

---

## Recommendations for Future Work

### 1. Byte Order Consistency (Portability)

The storage layer uses `write_u32`/`read_u32` for string lengths and in migration. Many direct `fread`/`fwrite` calls for 4-byte integers (id, done, priority, all_day) still use native byte order. On x86/ARM (little-endian) this matches the format; on big-endian systems it would be wrong.

**Recommendation**: Replace all `fread(&id, 1, 4, f)` with `read_u32(f, &id)` and all `fwrite(&id, 1, 4, f)` with `write_u32(f, id)` for full portability.

### 2. Nested Functions (GCC Extension)

`storage_note_get`, `storage_task_get`, etc. use nested callback functions (e.g. `void find_one(...)`). This is a GNU C extension, not standard C11.

**Recommendation**: For strict C11 portability, use static helper functions with a context struct passed via `void *ctx`.

### 3. Error Handling

- `fopen` failures are handled but not logged.
- `mkdir` in `ensure_data_dir` ignores `stat`/`mkdir` errors.

**Recommendation**: Add optional logging (e.g. via `stderr` or a pluggable log callback) for storage initialization and migration failures.

### 4. Path Length Safety

`data_path` uses `snprintf` with fixed buffers. Very long `data_dir` paths could truncate.

**Recommendation**: Validate `strlen(data_dir) + strlen(name) + 8 < DATA_DIR_MAX` or use dynamic allocation for paths.

### 5. Test Coverage

- Storage tests cover CRUD and trash; filtered list tests exist.
- No explicit tests for migration, corrupted files, or very long strings.

**Recommendation**: Add tests for migration, truncated reads, and edge cases (empty files, malformed records).

---

## Architecture Notes

- **Layering**: main → app → tui, with storage as a separate data layer. Clean separation.
- **State**: AppState holds all UI state; no global UI variables beyond module names.
- **Storage**: File-per-entity, append-only with rewrite for updates. Suitable for small datasets; consider indexing for large-scale use.

---

## Checklist (Google-Style)

| Item | Status |
|------|--------|
| Clear module boundaries | ✓ |
| Consistent naming | ✓ |
| No magic numbers (entity types) | ✓ (applied) |
| Defensive null checks | ✓ (copy_str) |
| Resource cleanup (FILE*) | ✓ |
| Documentation in headers | ✓ (improved) |
| Byte order portability | Partial (migration fixed) |
| Standard C only | Partial (nested functions) |
