# Changelog

All notable changes to vibePDA are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [0.8.0-alpha] - 2026-02-15

**Alpha release.** Core TUI, storage, and CRUD in place.

### Added

- **1980s-style TUI**: Curses (ncurses on Linux, PDCurses on DOS). Menu bar (F1–F4, F10), status line, F-keys for actions.
- **Note cards**: Notes display as Title + Content; Up/Down navigate between them.
- **Multi-line content editor**: Note body uses bordered text area with cursor, line numbers (F5), and scrolling (Page Up/Down). Enter=newline, Enter+Enter=save, Esc=cancel. Arrow keys move cursor responsively.
- **FACTS module**: New module for key-value pairs (e.g. "Andrew SSN = 455-56-2022"). Full CRUD support with `facts add <key> <value>`, `list`, `edit`, `delete` commands.
- **TUI test framework**: `make test_tui` builds automated UI tests (`tests/test_tui.c`) that simulate key presses and verify UI behavior.
- **Unit tests**: `make test` runs app (UI state, key handling, content editor) and storage tests. CLI integration: `--foo` prints "unknown argument", exits 1.
- **Make targets**: `make clean all` and `make rebuild` remove all binaries and object files before rebuilding. Linux targets use ncurses by default.
- **Display size flags**: `--display small` (80x25), `--display auto` (dynamic terminal size), `--display custom COLxROW` (e.g. `--display custom 120x30`).
- **REPL improvements**: Prompt changed from `"vibe> "` to `"> "`. Added `list` command to show all records (Notes, Tasks, Contacts, Calendar, Facts) in formatted tables.
- **Status bar**: Shows current UI context (e.g. `:Notes=>View`, `:Notes=>ContentEditing`, `:Facts=>Adding`).

### Changed

- **Content editor save**: Changed from F2 to Enter+Enter (two blank lines) for saving notes. More intuitive for multiline editing.
- **Cursor responsiveness**: Cursor now immediately moves to next line when pressing Up/Down arrow keys, without waiting for typing.
- **Pagination**: Improved scrolling for large notes (100+ lines). Page Up/Down scrolls one page at a time. Cursor automatically scrolls into view when moving.
- **Documentation**: README, PLAN, docs/TESTING updated for alpha status, curses TUI, note cards, content editor, FACTS module, make clean/rebuild.
- **Unknown arguments**: Print "unknown argument" to stderr, show help, exit 1.

---

## [0.7.1] - 2026-02-15

### Added

- **build-package**: Make target produces a redistributable `dist/vibePDA-<version>.tar.gz` with the binary and all documentation. `make all` (or `make`) now runs build-package.
- **ASCII .txt for all markdown**: Every markdown doc (README, CHANGELOG, PLAN, docs/TESTING, demo/README) has an equivalent `.txt` plain-text file. `make docs-txt` regenerates them from the `.md` sources via `scripts/md2txt.go`. The redistributable package includes both formats.

### Fixed

- **Calendar empty state**: When there are no events for the selected day, the calendar now shows "No events." in app styling instead of the list default ("No items." with a blue title bar).

---

## [0.7.0] - 2026-02-15

### Added

- **Sidebar record counts**: Each module shows count next to title, e.g. "Notes (42)", "Tasks (15)".
- **CLI `--regenerate`**: Wipe database and recreate with current schema, then exit.
- **CLI `--upgrade`**: Export data, upgrade schema, re-import (preserves user data).
- **CLI `--export` / `-e`**: Export data to JSON file.
- **CLI `--import` / `-i`**: Import data from JSON file.
- **80's terminal theme**: Phosphor green accent, sharp single/double borders, DOS-style status bar.
- **External demo data**: `demo/` dir (contacts.txt, notes.txt, tasks.txt, events.txt, theme.json) — Parks and Rec themed.

### Changed

- **Sidebar width**: Narrower (~16 cols), scales to widest title + count.
- **Tasks list**: Fixed double-spacing between items; reserved height for filter line to prevent overflow.
- **Pagination**: Disabled on all lists to prevent layout overflow.
- **Status bar**: Truncated when long hints would wrap; prevents layout jump on module switch.
- **vibe-seed**: Loads from external files; detects DB lock and shows helpful error when another instance is running.

### Fixed

- Tasks list running off screen with many entries.
- Title bar disappearing on some modules (Tasks, Notes, Contacts use list built-in title).
- Sidebar and main pane vertical height mismatch.
- build-demo disk I/O error when vibePDA has db open — clearer error message.

---

## [0.6.0] - 2026-02-15

### Added

- **Trash**: Soft-delete for all modules; deleted items move to Trash. F5 to open Trash, R to restore.
- **Build demo**: `make build-demo` builds and seeds 100+ records per module from `demo/` (Parks and Rec theme). `make clean` removes demo db (never touches user db).
- **CLI options**: `-v`/`--version` and `-h`/`--help`. `VIBE_DB` env var overrides database path.
- **Toaster notifications**: Transient "Saved", "Deleted", "Restored" messages on CRUD operations.
- **Tasks**: Search (`/`), filter (`Shift+F` all/incomplete/complete), sort (`Shift+S` created/due/priority/title). Pretty date ("Today 6pm"). Completed tasks stay in place.
- **Notes**: Double Enter (empty line) saves, same as calendar event notes.
- **Calendar**: Date field (YYYY-MM-DD) and all-day checkbox in event form.
- **Contacts**: Multi-column card layout when terminal is wide (2–4 columns).
- **Binary name**: Output is `vibePDA` (Makefile, docs).

### Changed

- **Soft delete**: Delete marks records as deleted; restore from Trash. `deleted_at` column added to all tables.
- **Tasks**: Order by id (no resort of completed). `updated_at` column for pretty date display.
- **UI**: Dynamic height for lists; reduced spacing in grid/list views.

---

## [0.5.0] - 2026-02-15

### Added

- **Function key navigation**: F1–F12 for all primary actions
  - F1 Notes, F2 Tasks, F3 Contacts, F4 Calendar
  - F5 New, F6 Edit, F7 Delete
  - F8 Focus main, F9 Focus sidebar
  - F10 Save (in forms), F11 Cancel (in forms)
  - F12 Quit
- **DOS-style status bar**: Fixed bottom bar showing key bindings (blue background, white text)

### Changed

- Navigation: Tab/Shift+Tab retained; ↑/↓ for sidebar list movement
- Status bar replaces dynamic help bar

---

## [0.4.1] - 2026-02-15

### Fixed

- Tests: remove unused `encoding/json` import in config_test.go
- Build: run `go mod tidy` to sync dependencies

---

## [0.4.0] - 2026-02-14

### Added

- **Tests**: `go test ./...` for db, config, and app packages
  - NotesRepo, TasksRepo, ContactsRepo, CalendarRepo CRUD
  - Config load, expandPath, Default
  - indexForView (sidebar module routing)
- **configure script**: Checks Go 1.24.2+, generates config.mk
- **make check**: Runs test suite (alias for make test)
- **make clean**: Removes built binary
- **scripts/install-go.sh**: Installs Go 1.24.2 to /usr/local/go
- **Calendar day navigation**: `,` prev day, `.` next day, `a` all events
- **Contacts card view**: Records shown as cards with name, email, phone, notes
- **Pane focus hint**: Thick border on focused pane (sidebar vs main)

### Changed

- **Project name**: Vibe → vibePDA (UI, README, CHANGELOG, PLAN)
- **Save key**: F2 (replaces Ctrl+S/Ctrl+Enter for terminal compatibility)
- **Records on startup**: Main pane shows records without pressing Tab
- **Build environment**: Go 1.24.2+ required; Makefile uses config.mk from configure

### Fixed

- Records not visible when switching sidebar modules until Tab pressed
- CHANGELOG dates corrected to 2026
- go.mod Go version (1.24 for charmbracelet/bubbles compatibility)

---

## [0.3.0] - 2026-02-14

### Added

- **Notes module**: Full CRUD (first line = title, rest = content, textarea)
- **Contacts module**: Full CRUD (name, email, phone, notes)
- **Calendar attendees**: Add contacts as event attendees (form step 4)
- **Tasks edit**: Enter on selected task opens edit form
- **Tasks reorder**: Ctrl+Up / Ctrl+Down to move tasks
- **Tasks module**: Full CRUD for to-do items
  - Add, delete, and toggle done status
  - SQLite `tasks` table with title, done, due_date, priority
  - Keybindings: `n` new, `space`/`Enter` toggle, `d` delete, `j`/`k` navigate

- **Calendar event edit**: Enter on selected event opens edit form

- **Calendar notes**: Multiline notes/description field

- **Calendar form**: Ctrl+S / Ctrl+Enter save, Esc cancel; Tab in Notes = indent; Ctrl+Tab = Notes→Attendees
  - Uses Bubbles textarea component
  - Stored in `description` column

- **Pane navigation**: Tab / Shift+Tab to switch focus between sidebar and main content

- **Calendar event time input**: Add/edit events with explicit start and end times
  - Form steps: Title → Start (HH:MM) → End (HH:MM) → Notes
  - Tab / Enter advance fields, Shift+Tab go back

- **Build number**: Version from VERSION file in title bar when built with `-ldflags`
  - Makefile target: `make build`

### Changed

- Calendar event form now uses Bubbles TextInput (fixes space character in titles)

- Help bar updates dynamically based on focused module and pane

### Fixed

- Space character now works when typing calendar event titles

---

## [0.2.0] - 2026-02-14

### Added

- Calendar module with SQLite backend
- Month grid view with event list
- Add and delete calendar events
- Config: `database_path` default `~/.local/share/vibe/vibe.db`

---

## [0.1.0] - 2026-02-14

### Added

- Initial TUI with Outlook-inspired layout
- Sidebar: Notes, Tasks, Contacts, Calendar modules
- Bubble Tea + Lipgloss + Bubbles stack
- JSON config at `~/.config/vibe/config.json`
