# Test-Driven Development (TDD) — vibePDA

This document describes the TDD approach used in vibePDA and how to apply it when adding or changing features.

---

## What is TDD?

**Test-Driven Development** means writing tests *before* implementation. The cycle is:

1. **Red** — Write a failing test that defines the desired behavior.
2. **Green** — Write the minimal code to make the test pass.
3. **Refactor** — Improve the code while keeping tests green.

---

## Workflow for vibePDA

### 1. Run tests before you start

```bash
make test
```

Ensure all existing tests pass. If they don't, fix them first.

### 2. Write the test first

Add a test in the appropriate file:

| Area | File | Example |
|------|------|---------|
| App state, key handling | `tests/test_app.c` | `app_handle_key` behavior |
| Storage CRUD, migration | `tests/test_storage.c` | `storage_notes_add`, counts |
| Config, data dir | `tests/test_config.c` | `vibe_config_load` |
| Fuzz, boundaries | `tests/test_fuzz.c` | Long strings, NULL, malicious input |
| Trash integration | `tests/test_app.c` (`test_trash_integration`) | Restore, permanent delete |

Use `assert()` for conditions. For clearer failures, use helpers from `test_common.h`:

```c
#include "test_common.h"

/* Assert with message (fails with file:line and message) */
TEST_EQ(storage_notes_count(), 0, "empty storage should have 0 notes");
TEST_STR_EQ(n.title, "Expected", "note title mismatch");
```

### 3. Run the test — it should fail (Red)

```bash
make run_tests && ./run_tests
```

Or run the full suite:

```bash
make test
```

The new test should fail because the behavior is not yet implemented (or is wrong).

### 4. Implement the minimal code (Green)

Implement just enough in `src/` to make the test pass. Avoid over-engineering.

### 5. Run tests again — they should pass

```bash
make test
```

### 6. Refactor

Improve code structure, naming, or performance. Re-run tests after each change to ensure nothing breaks.

---

## Test organization

- **`tests/run_tests.c`** — Test runner. Calls `test_app`, `test_storage`, `test_trash_integration`, `test_config`, `test_fuzz`.
- **`tests/test_common.h`** — Shared helpers: `test_verbose()`, `TEST_EQ`, `TEST_STR_EQ`, etc.
- **`tests/fixture_parks.c`** — Dummy data (Parks and Rec themed) for storage tests.

Tests do **not** start the TUI (`tui_init` is never called). They exercise `app_init`, `app_handle_key`, and the storage API directly, so they run safely in CI and headless environments.

---

## Environment variables

| Variable | Purpose |
|----------|---------|
| `VIBE_TEST_VERBOSE` | `1` (default) or `0` — print each sub-test name |
| `VIBE_TEST_RECORDS` | Number of dummy records to seed (default 100). Use `0` to skip seeding. |
| `VIBE_TEST_SEED` | Random seed for reproducible dummy data (e.g. CI) |

---

## Adding a new feature (TDD example)

**Example: Add a "storage_documents_count()" that returns 0 for empty storage.**

1. **Red** — Add to `test_storage_empty()` in `test_storage.c`:

   ```c
   assert(storage_documents_count() == 0);
   ```

   Run `make test` — fails if the function doesn't exist or returns wrong value.

2. **Green** — Implement `storage_documents_count()` in `storage_file.c` to return 0 when empty.

3. **Refactor** — If the implementation duplicates logic from other count functions, extract a helper.

---

## Guidelines

- **One assertion per logical check** — Makes failures easier to diagnose.
- **Test behavior, not implementation** — Tests should survive refactoring.
- **Keep tests fast** — Use temp dirs, avoid I/O where possible. `VIBE_TEST_RECORDS=0` for quick runs.
- **Isolate tests** — Each test should set up its own state (e.g. `storage_init` with a fresh temp dir).
- **Name tests clearly** — Use `if (test_verbose()) fprintf(stderr, "    descriptive_name\n");` before each logical block.

---

## Quick reference

```bash
# Run all tests
make test

# Run tests quietly (CI mode)
VIBE_TEST_VERBOSE=0 make test

# Run without seeding dummy data (faster)
VIBE_TEST_RECORDS=0 make test

# Reproducible run
VIBE_TEST_SEED=42 VIBE_TEST_RECORDS=50 make test
```

---

## See also

- **docs/TESTING.md** — Full testing documentation (unit tests, manual tests, layout).
- **docs/ARCHITECTURE.md** — Module responsibilities and layering.
