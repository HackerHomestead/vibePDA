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

- **Go 1.21+** — Many Linux distros ship older Go. Install Go 1.23 via:
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
make build
./vibe
```

Or manually:

```bash
go build -ldflags "-X github.com/you/vibe/internal/app.BuildNumber=$(cat VERSION 2>/dev/null | tr -d '\n' || echo dev)" -o vibe ./cmd/vibe
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

**Event form:** Title → Start (HH:MM) → End (HH:MM) → Notes → Attendees (optional). **Enter** next field, **Ctrl+Tab** Notes→Attendees, **Tab** in Notes = indent. **Ctrl+S** or **Ctrl+Enter** save, **Esc** cancel.

### Tasks (Tab to focus main, then)

| Key       | Action                       |
|-----------|------------------------------|
| `n`       | New task                     |
| `Enter`   | Edit selected task           |
| `space`   | Toggle done                  |
| `Ctrl+↑` / `Ctrl+↓` | Reorder task          |
| `d`       | Delete selected task         |
| `j` / `k` | Move selection               |

### Notes (Tab to focus main, then)

| Key             | Action                |
|-----------------|-----------------------|
| `n`             | New note              |
| `Enter`         | Edit selected note    |
| `Ctrl+S` / `Ctrl+Enter` | Save (in form)        |
| `Esc`           | Cancel (in form)      |
| `d`             | Delete selected note  |
| `j` / `k`       | Move selection        |

First line = title, rest = content. Multiline textarea.

### Contacts (Tab to focus main, then)

| Key             | Action                |
|-----------------|-----------------------|
| `n`             | New contact           |
| `Enter`         | Edit selected contact |
| `Ctrl+S` / `Ctrl+Enter` | Save (in form)        |
| `Esc`           | Cancel (in form)      |
| `d`             | Delete selected contact |
| `j` / `k`       | Move selection        |

Form: Name → Email → Phone → Notes. Enter next field, Shift+Tab previous.

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
