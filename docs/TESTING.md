# Testing vibePDA (C)

Unit and manual testing for the C build. **Status:** Alpha.

---

## Unit tests

### Build and run

```bash
make test
```

This builds `run_tests` and runs it. Exit code 0 means all tests passed.

### What is tested

- **App (UI/UX)**  
  - `app_init()` sets initial state (module 0, sidebar focus, no menu, no prompt, no content editor, no search).  
  - `app_handle_key()`: module switch (Up/Down in sidebar), focus (Tab/Shift+Tab), quit (q, F10), help (F1, ?), CRUD hotkeys (F2/N new, F3/E edit, F4/D delete), and search (F5/).  
  - Content editor: F2 in Notes → enter title → Enter advances to `content_edit_mode`; Esc cancels and clears `content_edit_mode`.
  - Search mode: F5 starts search, Enter applies filter, F5 again clears filter.
  - Trash module: F2/N disabled, restore (R), checkbox toggle (Space), select all/none (A/U), delete (X).

- **Storage (backend)**  
  - `storage_init()` and counts: init in a temp dir (uses `.bin` binary files), counts return 0 when empty. Then (unless disabled) the test seeds **dummy data** and asserts counts. See **Dummy data** below.
  - **Migration** (Linux only): Creates `.txt` files in TSV format, runs `storage_init`, verifies migration to `.bin` and that migrated data is readable.
  - **Corruption/edge cases** (Linux only): Empty files, truncated records, malformed data, minimal valid records — verifies no crashes and graceful handling.
  - Trash functionality: soft-delete, trash listing, restore, permanent delete, empty trash.
  - Search/Filter: Filtered list functions for all modules with case-insensitive substring matching.

- **CLI (integration)**  
  - `./vibePDA --foo` prints "unknown argument" to stderr and exits with code 1.

Tests do **not** start the TUI or terminal (no `tui_init`). They only call `app_init`, `app_handle_key`, and storage API so they are safe to run in CI or headless environments.

### Dummy data (Parks and Rec themed)

Storage tests can seed the backend with fake notes, tasks, contacts, and calendar events inspired by *Parks and Recreation*. The data is **configurable** and **random** (by default).

- **Record count** — Set `VIBE_TEST_RECORDS` to the number of records to create (split roughly evenly across notes, tasks, contacts, events). Default: **100**. Set to **0** to skip seeding and only run the empty-storage checks.
- **Reproducibility** — Set `VIBE_TEST_SEED` to an integer to fix the random seed (e.g. for CI). If unset, the seed is based on the current time.

Examples:

```bash
make test
VIBE_TEST_RECORDS=50 make test
VIBE_TEST_RECORDS=0 make test
VIBE_TEST_SEED=42 VIBE_TEST_RECORDS=100 make test
```

---

## Layout and terminal size

- **Minimum**: 80×24 (VT102 baseline). Layout assumes at least this; smaller terminals may clip content.
- **Manual check**: Resize to 80×24, 120×30, etc. Confirm menu bar, sidebar, and main pane render; note cards show Title + Content; content editor uses a bordered text area.

---

## Interaction (1980s-style TUI)

- **F-keys**: F1 Help, F2 New, F3 Edit, F4 Delete, F5 Search, F10 Quit. Shortcuts: n/t/c/a/x switch module; q quit; ? help; / search.
- **Navigation**: Up/Down in sidebar switch modules; Up/Down in main pane move selection; Tab/Shift+Tab switch focus between sidebar and main.
- **Search/Filter**: F5 or / starts search; type query to filter in real-time; Enter applies filter; F5 again clears filter.
- **Note cards**: Notes display as cards (Title + Content). Select a note to view its content in the main pane.
- **Content editor**: When adding/editing a note body, a bordered multi-line text area appears with cursor, line numbers (F5), and scrolling (Page Up/Down). Enter inserts newline; Enter+Enter (two blank lines) saves; Esc cancels and exits content editor. Arrow keys move cursor; cursor automatically scrolls into view.
- **Trash management**: Checkbox selection (Space), select all (A), unselect all (U), restore (R), permanent delete (X with confirmation).

Manual test: run `./vibePDA`, press F2 (New), type a title, Enter, type content, press Enter twice to save. Press Esc to cancel if needed.

---

## Running a subset of tests

The test runner is a single binary. To add filters or separate app vs storage tests, extend `tests/test_app.c` and `tests/test_storage.c` (or the runner) with optional arguments or environment variables; for now, `make test` runs the full suite. Use `VIBE_TEST_RECORDS=0` to run only the empty-storage assertions without dummy data.
