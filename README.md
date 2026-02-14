# Vibe

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

- **Go 1.21+** — Many Linux distros ship older Go. Install latest via:
  ```bash
  sudo snap install go --classic
  ```
- **Linux** (primary target: Ubuntu)

---

## Installation

### Build from source

```bash
git clone <repo-url>
cd vibe
go mod tidy
go build -ldflags "-X github.com/you/vibe/internal/app.BuildNumber=$(date +%s)" -o vibe ./cmd/vibe
./vibe
```

Or use the Makefile:

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

### Global

| Key            | Action                          |
|----------------|---------------------------------|
| `Tab`          | Focus main pane (from sidebar)  |
| `Shift+Tab`    | Focus sidebar (from main pane)  |
| `↑` / `↓`      | Navigate sidebar modules        |
| `Enter`        | Select module                   |
| `q` / `Ctrl+C` | Quit                            |

### Calendar (Tab to focus main, then)

| Key        | Action                                    |
|------------|-------------------------------------------|
| `←` / `→` or `h` / `l` | Previous/next month              |
| `t`        | Jump to today                             |
| `n`        | New event                                 |
| `Enter`    | Edit selected event                       |
| `d`        | Delete selected event                     |
| `j` / `k`  | Move event selection                      |

**Event form:** Title → Start (HH:MM) → End (HH:MM) → Notes (multiline). **Tab** / **Enter** next field, **Shift+Tab** previous, **Esc** cancel. Notes supports multiline input.

### Tasks (Tab to focus main, then)

| Key       | Action                |
|-----------|-----------------------|
| `n`       | New task              |
| `space` / `Enter` | Toggle done        |
| `d`       | Delete selected task  |
| `j` / `k` | Move selection        |

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
- [x] Calendar: month grid, event list, add/delete, time input
- [x] Tasks: list, add/delete, toggle done
- [ ] CRUD for Notes, Contacts
- [ ] External editor integration, themes

---

## License

MIT (or as specified in the project)
