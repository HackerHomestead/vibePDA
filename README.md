# vibePDA

**A terminal personal data assistant** — Notes, Tasks, Contacts, and Calendar in a single TUI, inspired by classic Outlook.

Built with Go and the [Charm](https://github.com/charmbracelet) ecosystem. SQLite backend. Linux-first.

---

## Features

| Module    | Description                 |
|-----------|-----------------------------|
| **Notes** | Scratchpad and quick notes  |
| **Tasks** | To-do list with status      |
| **Contacts** | Contact list            |
| **Calendar** | Events and appointments |

Outlook-style layout: folder sidebar on the left, main content on the right. No CGO, no ncurses — pure Go.

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
./vibe
```

Or without configure (uses default Go):

```bash
make build
./vibe
```

Optionally install to `~/bin` or `/usr/local/bin`:

```bash
cp vibe ~/bin/
```

---

## Usage

### Global (DOS-style function key bar at bottom)

| Key  | Action                    |
|------|---------------------------|
| `F1` | Notes module              |
| `F2` | Tasks module              |
| `F3` | Contacts module           |
| `F4` | Calendar module           |
| `F5` | New item                  |
| `F6` | Edit selected / Enter     |
| `F7` | Delete selected           |
| `F8` | Focus main pane           |
| `F9` | Focus sidebar             |
| `F10`| Save (in forms)           |
| `F11`| Cancel / Esc (in forms)   |
| `F12`| Quit                      |

`↑` / `↓` or `j` / `k` — move selection within list. Status bar shows current key bindings.

### Per-module actions (status bar updates context)

**Calendar:** F5 new, F6 edit, F7 delete. `←`/`→` month, `,`/`.` day, `a` all, `t` today.

**Tasks:** F5 new, F6 edit, F7 delete. `space` toggle done, `Ctrl+↑`/`Ctrl+↓` reorder.

**Notes / Contacts:** F5 new, F6 edit, F7 delete. First line = title. Form: Enter next field, F10 save, F11 cancel.

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
├── cmd/vibe/main.go       # Entry point
├── internal/
│   ├── app/               # Bubble Tea model, layout, navigation
│   ├── calendar/          # Calendar view (month grid, events, CRUD)
│   ├── config/            # JSON config loader
│   ├── db/                # SQLite connection, migrations, calendar repo
│   └── ui/
│       ├── styles.go      # Lipgloss styles
│       └── views/         # Module views (Notes, Tasks, Contacts placeholders)
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
- [x] Notes: list, add/edit/delete (first line=title, rest=content)
- [x] Contacts: list, add/edit/delete (name, email, phone, notes)
- [ ] External editor integration, themes

---

## License

MIT (or as specified in the project)
