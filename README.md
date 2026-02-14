# Vibe — Console Personal Data Assistant

A TUI (terminal user interface) personal data assistant written in Go, using the [Charm](https://github.com/charmbracelet) ecosystem.

## Requirements

- **Go 1.21+** — Ubuntu's default Go may be older. Install via:
  ```bash
  sudo snap install go --classic
  ```
- **Linux** (primary target: Ubuntu)

No CGO, ncurses, or other system dependencies required.

## Quick Start

```bash
cd vibe
go mod tidy
go build -o vibe ./cmd/vibe
./vibe
```

## Tech Stack

| Layer     | Technology              |
|-----------|-------------------------|
| TUI       | Bubble Tea + Lipgloss + Bubbles |
| Database  | SQLite (modernc.org/sqlite — pure Go) |
| Config    | JSON (~/.config/vibe/config.json) |

## Planning

**MVP modules:** Notes, Tasks, Contacts, Calendar.

See [PLAN.md](PLAN.md) for full architecture, features, and implementation phases.
