# vibePDA — Main branch plan (C/C++)

Terminal personal data assistant. **Status:** Alpha. **Implementation:** C (C++ optional). **Build targets:** 1) Linux, 2) FreeDOS (DJGPP), 3) WebAssembly (Emscripten). **TUI:** curses (ncurses on Linux, PDCurses on DOS). **Terminal:** VT102 minimum; on Linux, modern terminal standards are allowed. GNU Make and Autotools.

---

## Architecture (layers)

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Presentation — TUI (tui.c/h)                                           │
│  curses (ncurses on Linux, PDCurses on DOS); VT102 minimum              │
├─────────────────────────────────────────────────────────────────────────┤
│  Application — App shell (app.c/h)                                      │
│  Menu bar, sidebar, main pane; F-keys; note cards; content editor        │
├─────────────────────────────────────────────────────────────────────────┤
│  Data — Storage API (storage.h, storage_file.c) — file-based only       │
│  CRUD per entity: notes, tasks, contacts, calendar_events; soft-delete   │
├─────────────────────────────────────────────────────────────────────────┤
│  Config (vibe_config) — Paths, defaults (XDG, ~/.config/vibe/)           │
└─────────────────────────────────────────────────────────────────────────┘
```

- **TUI**: curses (ncurses on Linux, PDCurses on DOS); VT102 minimum baseline. Color scheme (blue, gray, yellow, black) matches README screenshots; ncurses color pairs for title bar, menu bar, sidebar selection, status bar, card titles; fallback to bold/reverse when terminal lacks color support.
- **App**: Holds state (current module, focus, prompt mode, content editor); draws layout; F-keys for actions; note cards (Title + Content); multi-line content editor with cursor, line numbers (F5), scrolling (Page Up/Down); Enter=newline, Enter+Enter=save, Esc=cancel.
- **Storage**: Single API (storage.h). File-based only (storage_file.c) on all platforms. Binary `.bin` format (length-prefixed strings) so tabs and newlines are safe in data. All entities use soft-delete where applicable; Trash is a view over deleted items. Supports Notes, Tasks, Contacts, Calendar Events, and Facts (key-value pairs).
- **Config**: Database path, optional editor path, default view; future: JSON or key=value.

---

## Tech stack (C/C++)

| Layer    | Choice                 | Notes                          |
|----------|------------------------|---------------------------------|
| Language | C11 (C++ optional)     | std=c11; no C++ required yet   |
| TUI      | ncurses (Linux), PDCurses (DOS) | tui.c/h; VT102 min baseline |
| Storage  | File-based only        | storage.h + storage_file.c      |
| Build    | GNU Make, Autoconf, Automake | TARGET=linux\|dos\|webasm |
| Config   | XDG, key=value or JSON | config.c/h                      |

---

## Data model (schema)

Entities map to C structs in **types.h** and to binary flat files (notes.bin, tasks.bin, contacts.bin, events.bin, facts.bin).

| Entity          | Purpose        | Main fields (types.h)                          |
|-----------------|----------------|-------------------------------------------------|
| **Note**        | Scratchpad     | id, title, content, created_at, deleted_at     |
| **Task**        | Todo           | id, title, done, due_date, priority, created_at, deleted_at |
| **Contact**     | Contact list   | id, name, email, phone, notes, created_at, deleted_at |
| **CalendarEvent** | Appointments | id, title, description, start_at, end_at, all_day, deleted_at |
| **Fact**        | Key-value pairs | id, key, value, created_at, deleted_at |
| **FinanceEntry** | General ledger | id, date, description, amount, category, account, notes, created_at, deleted_at |
| **Document**    | Templated forms | id, title, template_name, content, created_at, deleted_at |
| **Trash**       | View only      | Restore = clear deleted_at; Permanent delete with confirmation |

All user-facing entities support **soft-delete** (deleted_at). Trash lists items where deleted_at IS NOT NULL; restore clears deleted_at. Permanent delete removes items completely.

---

## Modules (views)

| Module   | Description           | Data / view                          |
|----------|------------------------|--------------------------------------|
| Notes    | Scratchpad, quick notes| Note list; New/Edit/Delete; Search (title/content) |
| Tasks    | Todo list              | Task list; done flag; New/Edit/Delete; Search (title) |
| Contacts | Contact list           | Business card view (name, email, phone, notes); New/Edit/Delete; Search (name/email/phone) |
| Calendar | Events & appointments  | Month grid + event list; New/Edit/Delete; Search (title) |
| Facts    | Key-value pairs        | Fact list (key = value); New/Edit/Delete; Search (key/value) |
| Finances | General ledger         | Finance entry list; Search (description/category/account) |
| Documents | Templated forms        | Document list; Search (title/template/content) |
| Trash    | Soft-deleted items     | List deleted items; Restore; Checkbox selection; Permanent delete |

Sidebar lists module names only (no record counters).

---

## Project structure (C/C++)

```
vibe/
├── src/
│   ├── main.c              # Entry; parse_args (--help, --version); TUI loop
│   ├── app.c, app.h        # App shell: state, F-keys, note cards, content editor
│   ├── tui.c, tui.h        # Presentation: curses (ncurses/PDCurses), keys, attributes
│   ├── ui_box.c, ui_box.h  # Box-drawing primitives (Unicode/ASCII)
│   ├── vibe_config.c, vibe_config.h  # Config layer: paths, defaults
│   ├── types.h             # Data types: Note, Task, Contact, CalendarEvent, Fact
│   ├── storage.h           # Storage API: init, *_count, *_list, *_add, *_update, *_delete, *_restore
│   ├── storage_io.c, storage_io.h  # Binary I/O helpers (internal to storage)
│   └── storage_file.c      # File-based backend (all platforms)
├── tests/
│   ├── run_tests.c
│   ├── test_app.c
│   ├── test_storage.c
│   └── test_fuzz.c      # Fuzz/sanity and security regression tests
├── Makefile                # TARGET=linux|dos|webasm; all, clean, rebuild, test, install
├── config.h.in, configure.ac, Makefile.am
├── VERSION, README.md, CHANGELOG.md
└── docs/
    ├── CODE_REVIEW.md
    ├── INFOSEC_REVIEW.md
    └── TESTING.md
```

Optional future: split views into **src/notes.c**, **src/tasks.c**, etc., each with draw_* and handle_key_* for that module; app.c would delegate to them. For the main-branch refactor, app.c remains the single view layer and uses storage API for (when implemented) list/add/edit/delete.

---

## Interaction

- **F-keys**: F1 Help, F2 New, F3 Edit, F4 Delete, F10 Quit. Shortcuts: n/t/c/a/x switch module; q quit; ? help.
- **Navigation**: Up/Down in sidebar switch modules; Up/Down in main pane move selection; Tab/Shift+Tab switch focus between sidebar and main.
- **Note cards**: Notes display as cards (Title + Content). Select a note to view its content.
- **Content editor**: Multi-line text area for note body with cursor, line numbers (F5), and scrolling (Page Up/Down). Enter=newline; Enter+Enter=save (two blank lines); Esc=Cancel. Arrow keys move cursor responsively.
- **CLI**: `--help`, `--version`. Unknown arguments print "unknown argument" to stderr, show help, exit 1.

---

## Build targets

| Target   | make / make all        | Output            |
|----------|------------------------|-------------------|
| Linux    | make                  | vibePDA           |
| FreeDOS  | make TARGET=dos        | vibePDA.exe       |
| WebAssembly | make TARGET=webasm  | vibePDA.js, .wasm |

Autotools: `autoreconf -fi && ./configure && make && make install`. CLI: `--help`, `--version`.

---

## Phases (main branch, C/C++)

1. **Scaffolding** — Makefile, config.h, TUI, build targets. ✅  
2. **Layout** — Menu bar, sidebar, main pane, F-keys. ✅  
3. **Config layer** — config.c/h: db path, defaults (XDG). ✅  
4. **Data layer** — types.h; storage.h API (counts, then list/add/update/delete/restore); storage_file only (stubs ✅)  
5. **Views** — List/detail and CRUD in app; note cards; content editor. ✅  
6. **Polish** — Resize handling for note cards, toasts, export/import, backup.

---

## References

- VT102 minimum; modern Linux terminal (ECMA-48, SGR, etc.)  
- DJGPP (FreeDOS)  
- Emscripten (WebAssembly)


