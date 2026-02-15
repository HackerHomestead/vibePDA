# vibePDA — Console Personal Data Assistant

## Overview

A terminal-based (TUI) personal data assistant written in Go, targeting Linux (Ubuntu). Uses the Charm ecosystem for a polished terminal UI, SQLite for portable data storage, and JSON for configuration.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    TUI Layer (Bubble Tea)                    │
│  ┌─────────────┐ ┌─────────────┐ ┌───────────────────────┐  │
│  │   Main      │ │   Views     │ │   Bubbles Components  │  │
│  │   App Loop  │ │  (Lipgloss) │ │ List, Input, Table…   │  │
│  └─────────────┘ └─────────────┘ └───────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    Data Layer                                │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  Repository / DAO  →  SQLite (modernc.org/sqlite)   │    │
│  └─────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│                    Config Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  Config Loader  →  JSON (~/.config/vibe/config.json) │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Tech Stack

| Layer      | Choice                    | Rationale                                              |
|-----------|---------------------------|--------------------------------------------------------|
| TUI       | **Bubble Tea**            | Functional TUI framework, well-documented, widely used |
| Styling   | **Lipgloss**              | Declarative styling for borders, colors, layout        |
| Components| **Bubbles**               | List, TextInput, Viewport, Table, Help, etc.           |
| Database  | **modernc.org/sqlite**    | Pure Go, no CGO; easier build & portability on Linux   |
| Config    | **JSON**                  | Standard, portable, human-editable                     |

No ncurses or CGO required — keeps builds simple and cross-compilation friendly.

---

## Personal Data Assistant Features (MVP)

| Module   | Description                    | Data Model                          |
|----------|--------------------------------|-------------------------------------|
| **Notes**| Quick notes / scratchpad      | id, title, content, created_at, tags |
| **Tasks**| Todo / task list with status  | id, title, done, due_date, priority |
| **Contacts** | Simple contact list       | id, name, email, phone, notes       |
| **Calendar** | Events & appointments   | id, title, description, start, end, all_day |

Future: reminders, journal entries, recurring events.

**Sidebar / "Modules"**: The left sidebar shows four fixed modules (Notes, Tasks, Contacts, Calendar). The label "Folders" was removed — these are modules, not file/folder hierarchies. Future MVP: optional user-created folders or categories (e.g. Work/Personal) for organizing notes/tasks within a module.

---

## Project Structure

```
vibe/
├── cmd/vibe/main.go             # Entry point, opens DB, creates repo
├── internal/
│   ├── app/
│   │   └── app.go               # Bubble Tea model, sidebar, calendar integration
│   ├── calendar/
│   │   └── model.go             # Calendar view: month grid, event list, add/delete
│   ├── config/
│   │   └── config.go            # JSON config loader
│   ├── db/
│   │   ├── db.go                # SQLite connection, schema migrations
│   │   └── calendar.go          # CalendarRepo CRUD
│   └── ui/
│       ├── styles.go            # Lipgloss style definitions
│       └── views/               # Placeholder views (notes, tasks, contacts)
│           └── views.go
├── go.mod
├── go.sum
├── PLAN.md
└── README.md
```

---

## Configuration (JSON)

**Location:** `~/.config/vibe/config.json` (XDG_CONFIG_HOME fallback to `~/.config`)

```json
{
  "database_path": "~/.local/share/vibe/vibe.db",
  "editor": "",
  "theme": "default",
  "default_view": "tasks"
}
```

- `database_path`: SQLite file; `~` expands to home dir (default: `~/.local/share/vibe/vibe.db`)
- `editor`: Optional external editor for long notes (vim, nano, etc.)
- `theme`: Reserved for future themes
- `default_view`: Which module to show on startup (`notes`, `tasks`, `contacts`, or `calendar`)

---

## Dependencies (go.mod)

```go
module github.com/you/vibe

go 1.21

require (
    github.com/charmbracelet/bubbletea v0.26.0
    github.com/charmbracelet/bubbles v0.20.0
    github.com/charmbracelet/lipgloss v0.11.0
    modernc.org/sqlite v1.31.0
)
```

**Ubuntu system dependencies:** None. Pure Go + SQLite; no CGO or ncurses.

---

## Key Implementation Notes

### Bubble Tea Flow

1. **Model**: Holds app state (current view, selected item, data cache).
2. **Update**: Handles key events, navigation, CRUD actions.
3. **View**: Renders via Lipgloss and Bubbles components.
4. **Init**: Loads config and DB, returns initial Cmd.

### Views

- **Main menu**: Bubbles List to switch between Notes / Tasks / Contacts / Calendar.
- **Notes**: Bubbles List + Viewport for content preview; TextInput for new/edit.
- **Tasks**: Bubbles Table or List with checkboxes.
- **Contacts**: Bubbles Table for name, email, phone.
- **Calendar**: Month grid (Lipgloss layout) + List of upcoming events; TextInput for new/edit event.

### Navigation

- `Tab` / `Shift+Tab`: Switch views or panels.
- `j/k` or `↑/↓`: List/table navigation (vim-style optional).
- `Enter`: Open/edit item.
- `n`: New item.
- `d`: Delete (with confirmation).
- `q` or `Esc`: Quit or back.
- **Calendar**: `h/l` or `←/→`: Previous/next month; `t`: Today.

### Database Schema (SQLite)

```sql
-- Notes
CREATE TABLE notes (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    content TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Tasks
CREATE TABLE tasks (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    done INTEGER DEFAULT 0,
    due_date DATE,
    priority INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Contacts
CREATE TABLE contacts (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    email TEXT,
    phone TEXT,
    notes TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Calendar events
CREATE TABLE calendar_events (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    description TEXT,
    start_at DATETIME NOT NULL,
    end_at DATETIME NOT NULL,
    all_day INTEGER DEFAULT 0,
    location TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

---

## Build & Run (Linux)

**Requirement:** Go 1.21+ (for Charm libs and modernc/sqlite). On Ubuntu:

```bash
# Option A: snap (latest Go)
sudo snap install go --classic

# Option B: official installer
# https://go.dev/dl/
```

Then:

```bash
# Clone / cd into project
cd vibe

# Fetch deps
go mod tidy

# Build
go build -o vibe ./cmd/vibe

# Run
./vibe
```

Optional: install to `~/bin` or `/usr/local/bin`.

---

## Suggested Phases

1. **Phase 1**: Project scaffolding, config loading, DB init with migrations. ✅
2. **Phase 2**: Bubble Tea skeleton, main menu, single view (e.g. Tasks). ✅
3. **Phase 3**: CRUD for Tasks, Notes, Contacts, and Calendar. (Calendar ✅; Notes, Tasks, Contacts pending)
4. **Phase 4**: Polish: keybindings, help text, error handling. (partial)
5. **Phase 5**: Optional: external editor, themes, backup/export.

---

## References

- [Bubble Tea](https://github.com/charmbracelet/bubbletea) — TUI framework
- [Lipgloss](https://github.com/charmbracelet/lipgloss) — Styling
- [Bubbles](https://github.com/charmbracelet/bubbles) — UI components
- [Charm GitHub](https://github.com/charmbracelet)
- [modernc.org/sqlite](https://modernc.org/sqlite) — Pure Go SQLite
