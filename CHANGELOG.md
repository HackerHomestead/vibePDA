# Changelog

All notable changes to Vibe are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Added

- **Tasks module**: Full CRUD for to-do items
  - Add, delete, and toggle done status
  - SQLite `tasks` table with title, done, due_date, priority
  - Keybindings: `n` new, `space`/`Enter` toggle, `d` delete, `j`/`k` navigate

- **Calendar event edit**: Press `Enter` on selected event to open edit form

- **Calendar notes**: Multiline notes/description field for events
  - Uses Bubbles textarea component
  - Stored in `description` column

- **Pane navigation**: Tab / Shift+Tab to switch focus between sidebar and main content

- **Calendar event time input**: Add/edit events with explicit start and end times
  - Form steps: Title → Start (HH:MM) → End (HH:MM) → Notes
  - Tab / Enter advance fields, Shift+Tab go back

- **Build number**: Epoch timestamp in title bar when built with `-ldflags`
  - Makefile target: `make build`

### Changed

- Calendar event form now uses Bubbles TextInput (fixes space character in titles)

- Help bar updates dynamically based on focused module and pane

### Fixed

- Space character now works when typing calendar event titles

---

## [0.2.0] - 2025-02-14

### Added

- Calendar module with SQLite backend
- Month grid view with event list
- Add and delete calendar events
- Config: `database_path` default `~/.local/share/vibe/vibe.db`

---

## [0.1.0] - 2025-02-14

### Added

- Initial TUI with Outlook-inspired layout
- Sidebar: Notes, Tasks, Contacts, Calendar modules
- Bubble Tea + Lipgloss + Bubbles stack
- JSON config at `~/.config/vibe/config.json`
