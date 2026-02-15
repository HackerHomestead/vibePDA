# vibePDA

**A terminal personal data assistant** — Notes, Tasks, Contacts, and Calendar in a single TUI, inspired by classic Outlook.

Built with Go and the [Charm](https://github.com/charmbracelet) ecosystem. SQLite backend. Linux-first.

---

## Features

| Module    | Description                 |
|-----------|-----------------------------|
| **Notes** | Scratchpad and quick notes  |
| **Tasks** | To-do list with search, filter, sort |
| **Contacts** | Contact list (multi-column when wide) |
| **Calendar** | Events and appointments |
| **Trash** | Soft-deleted items, restore |

Outlook-style layout: module sidebar (Notes, Tasks, Contacts, Calendar) on the left, main content on the right. No CGO, no ncurses — pure Go.

---

## Requirements

- **Go 1.24.2+** — Many Linux distros ship older Go. Install via:
  ```bash
  ./scripts/install-go.sh
  ```
  Then add to `~/.bashrc`: `export PATH=/usr/local/go/bin:$PATH`
- **Linux** (primary target: Ubuntu)

---

## Installation

### Build from source

```bash
git clone <repo-url>
cd vibe
./configure
make build
./vibePDA
```

Or without configure (uses default Go):

```bash
make build
./vibePDA
```

Optionally install to `~/bin` or `/usr/local/bin`:

```bash
cp vibePDA ~/bin/
```

### Demo data

Build and seed with 100+ Star Wars–themed records per module:

```bash
make build-demo
VIBE_DB=./vibe-demo.db ./vibePDA
```

`make clean` removes the demo database (never touches your real `~/.local/share/vibe/vibe.db`).

---

## Usage

```bash
vibePDA [options]
  -v, --version    Show build/version
  -h, --help       Show help
```

Environment: `VIBE_DB` overrides database path (e.g. `VIBE_DB=./vibe-demo.db` for demo).

### Global (DOS-style function key bar at bottom)

| Key  | Action                    |
|------|---------------------------|
| `F1` | Notes module              |
| `F2` | Tasks module              |
| `F3` | Contacts module           |
| `F4` | Calendar module           |
| `F5` | Trash (sidebar) / New (main) |
| `F6` | Edit selected / Enter     |
| `F7` | Delete selected           |
| `F8` | Focus main pane           |
| `F9` | Focus sidebar             |
| `F10`| Save (in forms)           |
| `F11`| Cancel / Esc (in forms)   |
| `F12`| Quit                      |

`↑` / `↓` or `j` / `k` — move selection. Status bar shows current key bindings.

### Per-module actions (status bar updates context)

**Calendar:** F5 new, F6 edit, F7 delete. `←`/`→` month, `,`/`.` day, `a` all, `t` today. Date and all-day in event form.

**Tasks:** F5 new, F6 edit, F7 delete. `space` toggle done. `/` search, `Shift+F` filter (all/incomplete/complete), `Shift+S` sort (created/due/priority/title). Completed tasks stay in place.

**Notes:** F5 new, F6 edit, F7 delete. Double Enter (empty line) saves. First line = title.

**Contacts:** F5 new, F6 edit, F7 delete. Multi-column card layout when terminal is wide.

**Trash:** View soft-deleted items. `R` restore, `F5` refresh.

---

## Configuration

Optional config file (uses defaults if missing):

- **Path:** `~/.config/vibe/config.json` (or `$XDG_CONFIG_HOME/vibe/config.json`)
- **Format:** JSON

```json
{
  "database_path": "~/.local/share/vibe/vibe.db",
  "editor": "",
  "theme": "default",
  "default_view": "tasks"
}
```

| Field           | Description                           |
|-----------------|---------------------------------------|
| `database_path` | SQLite file path (default: `~/.local/share/vibe/vibe.db`) |
| `editor`        | External editor for long notes (TODO) |
| `theme`         | Reserved for future themes            |
| `default_view`  | Startup module: `notes`, `tasks`, `contacts`, `calendar` |

---

## Project Structure

```
vibe/
├── cmd/
│   ├── vibe/main.go       # Entry point
│   └── vibe-seed/         # Demo data seeder (Star Wars theme)
├── internal/
│   ├── app/               # Bubble Tea model, layout, navigation
│   ├── calendar/          # Calendar view (month grid, events, CRUD)
│   ├── config/            # JSON config loader
│   ├── contacts/          # Contacts view (card layout, multi-column when wide)
│   ├── dataview/          # Reusable grid/table component
│   ├── db/                # SQLite connection, migrations, repos
│   ├── notes/             # Notes view
│   ├── tasks/             # Tasks view (search, filter, sort)
│   ├── toast/             # Transient notifications for CRUD feedback
│   ├── trash/             # Trashcan view (soft-deleted items)
│   └── ui/                # Shared styles, pretty-time helpers
├── Makefile               # build, build-demo, clean, test
├── go.mod
├── PLAN.md                # Architecture and roadmap
└── README.md
```

---

## Tech Stack

| Layer    | Technology                    |
|----------|-------------------------------|
| TUI      | [Bubble Tea](https://github.com/charmbracelet/bubbletea), [Lipgloss](https://github.com/charmbracelet/lipgloss), [Bubbles](https://github.com/charmbracelet/bubbles) |
| Database | [modernc.org/sqlite](https://modernc.org/sqlite) (pure Go) |
| Config   | JSON                          |

---

## Roadmap

See [PLAN.md](PLAN.md) for the full architecture, schema, and implementation phases. Current status:

- [x] TUI shell with Outlook-inspired layout
- [x] SQLite backend and migrations (calendar_events)
- [x] Calendar: month grid, event list, add/edit/delete, time, notes, attendees
- [x] Tasks: list, add/edit/delete, toggle done, reorder
- [x] Notes: list, add/edit/delete, double-Enter saves
- [x] Contacts: card view, multi-column when wide
- [x] Trash: soft-delete, restore
- [x] Tasks: search, filter, sort, pretty date
- [x] Demo: `make build-demo`, `VIBE_DB=./vibe-demo.db ./vibePDA`
- [ ] External editor integration, themes

---

## License

MIT (or as specified in the project)
