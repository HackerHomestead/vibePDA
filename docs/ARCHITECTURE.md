# vibePDA Architecture

Architecture overview, layering, and refactoring guidelines for maintainability and reuse.

---

## Layer Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Presentation — tui.c/h, ui_box.c/h                                     │
│  Terminal I/O, box-drawing (Unicode/ASCII)                               │
├─────────────────────────────────────────────────────────────────────────┤
│  Application — app.c/h, app_draw.c (optional)                            │
│  State, key handling, layout, module views, content editor, prompts     │
├─────────────────────────────────────────────────────────────────────────┤
│  Data — storage.h, storage_file.c, storage_io.c/h                       │
│  CRUD API, binary file backend, I/O helpers                              │
├─────────────────────────────────────────────────────────────────────────┤
│  Config — vibe_config.c/h                                                │
│  Paths, defaults (XDG, ~/.config/vibe/)                                 │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Module Responsibilities

| Module | Responsibility | Depends On |
|--------|----------------|------------|
| **tui** | Raw terminal I/O (curses), keys, attributes | — |
| **ui_box** | Box-drawing primitives (top, sep, bottom, vertical) | tui |
| **app** | App state, key dispatch, layout orchestration, CRUD flow | tui, ui_box, storage, types |
| **storage** | CRUD API for all entities, trash, migration | types, storage_io |
| **storage_io** | Binary read/write helpers (u32, length-prefixed strings) | — |
| **vibe_config** | Config loading, data dir resolution | — |
| **types** | Data structs (VibeNote, VibeTask, etc.) | — |

---

## Encapsulation Guidelines

1. **Storage API (storage.h)** — Public API only. No internal helpers exposed.
2. **Storage I/O (storage_io.h)** — Internal to storage layer. Used by storage_file.c only.
3. **UI Box (ui_box.h)** — Public. Any UI code may draw boxes.
4. **App (app.h)** — AppState and app_init/app_draw/app_handle_key. Internal helpers stay in app.c.

---

## File Size Targets

| File | Current | Target | Notes |
|------|---------|--------|-------|
| app.c | ~2400 | <1500 | Extract draw helpers, key handlers by mode |
| storage_file.c | ~2400 | <1500 | Extract storage_io; consider per-entity files |
| main.c | ~700 | <500 | CLI parsing could move to separate module |

---

## Refactoring Priorities

1. **Done:** ui_box — Box drawing extracted for reuse (src/ui_box.c, src/ui_box.h).
2. **Done:** storage_io — Binary I/O helpers extracted (src/storage_io.c, src/storage_io.h).
3. **Future:** app_draw.c — All draw_* functions; app.c keeps state and key dispatch.
4. **Future:** storage_notes.c, storage_tasks.c, etc. — Per-entity implementation files.
5. **Future:** app_keys by mode — handle_content_edit_keys(), handle_trash_keys(), etc. as static functions.

---

## Adding New Entities

To add a new entity (e.g. Bookmarks):

1. Add struct to **types.h**.
2. Add CRUD declarations to **storage.h**.
3. Implement in **storage_file.c** (or new storage_bookmarks.c): count, add, list, get, update, delete.
4. Add trash support: *_read_deleted, trash_list_*, restore, permanent_delete, empty.
5. Add to **app.c**: MODULE_*, draw callback, get_item_count, key handlers, prompt steps.

---

## Build

All sources live in `src/`. The Makefile compiles `src/*.c` (excluding tests). New modules: add to SRC in Makefile.
