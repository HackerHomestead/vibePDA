VIBEPDA


A terminal personal data assistant — Notes, Tasks, Contacts, and
Calendar in a single TUI, inspired by classic Outlook.

Built with Go and the Charm (https://github.com/charmbracelet)
ecosystem. SQLite backend. Linux-first.

------------------------------------------------------------------------


FEATURES


Module Description
Notes Scratchpad and quick notes
Tasks To-do list with search, filter, sort
Contacts Contact list (multi-column when wide)
Calendar Events and appointments
Trash Soft-deleted items, restore

Outlook-style layout: module sidebar (Notes, Tasks, Contacts, Calendar,
Trash) on the left with record counts, main content on the right. 80's
terminal aesthetic (phosphor green, sharp borders). No CGO, no ncurses —
pure Go.

------------------------------------------------------------------------


REQUIREMENTS


  - Go 1.24.2+ — Many Linux distros ship older Go. Install via:
    ./scripts/install-go.sh

Then add to ~/.bashrc: export PATH=/usr/local/go/bin:$PATH
  - Linux (primary target: Ubuntu)

------------------------------------------------------------------------


INSTALLATION


BUILD FROM SOURCE


    git clone <repo-url>
    cd vibe
    ./configure
    make build
    ./vibePDA


Or without configure (uses default Go):

    make build
    ./vibePDA


Optionally install to ~/bin or /usr/local/bin:

    cp vibePDA ~/bin/


REDISTRIBUTABLE PACKAGE


Create a versioned tarball with the binary and documentation:

    make build-package


This produces dist/vibePDA-<version>.tar.gz containing the vibePDA
binary, README.md/README.txt, CHANGELOG.md/CHANGELOG.txt,
PLAN.md/PLAN.txt, VERSION, docs/ (with TESTING.md/TESTING.txt), and
demo/README.md/demo/README.txt. Every markdown doc has an ASCII .txt
equivalent; run make docs-txt to regenerate them.

DEMO DATA


Build and seed with 100+ Parks and Rec–themed records per module (data
in demo/):

    make build-demo
    VIBE_DB=./vibe-demo.db ./vibePDA


make clean removes the demo database (never touches your real
~/.local/share/vibe/vibe.db).

------------------------------------------------------------------------


USAGE


    vibePDA [options]
    -v, --version    Show build/version
    -h, --help       Show help
    -r, --regenerate Wipe database and recreate with current schema
    -u, --upgrade    Export, upgrade schema, re-import (preserves user d
    -e, --export F   Export data to JSON file
    -i, --import F   Import data from JSON file


Environment: VIBEDB overrides database path (e.g. VIBEDB=./vibe-demo.db
for demo).

The sidebar shows record counts next to each module (e.g. "Notes (42)",
"Tasks (15)").

GLOBAL (DOS-STYLE FUNCTION KEY BAR AT BOTTOM)


Key Action
F1 Notes module
F2 Tasks module
F3 Contacts module
F4 Calendar module
F5 Trash (sidebar) / New (main)
F6 Edit selected / Enter
F7 Delete selected
F8 Focus main pane
F9 Focus sidebar
F10 Save (in forms)
F11 Cancel / Esc (in forms)
F12 Quit

↑ / ↓ or j / k — move selection. Status bar shows current key bindings.

PER-MODULE ACTIONS (STATUS BAR UPDATES CONTEXT)


Calendar: F5 new, F6 edit, F7 delete. ←/→ month, ,/. day, a all, t
today. Date and all-day in event form.

Tasks: F5 new, F6 edit, F7 delete. space toggle done. / search, Shift+F
filter (all/incomplete/complete), Shift+S sort
(created/due/priority/title). Completed tasks stay in place.

Notes: F5 new, F6 edit, F7 delete. Double Enter (empty line) saves.
First line = title.

Contacts: F5 new, F6 edit, F7 delete. Multi-column card layout when
terminal is wide.

Trash: View soft-deleted items. R restore, F5 refresh.

------------------------------------------------------------------------


CONFIGURATION


Optional config file (uses defaults if missing):

  - Path: ~/.config/vibe/config.json (or
    $XDGCONFIGHOME/vibe/config.json)
  - Format: JSON

    {
    "database_path": "~/.local/share/vibe/vibe.db",
    "editor": "",
    "theme": "default",
    "default_view": "tasks"
    }


Field Description
database_path SQLite file path (default: ~/.local/share/vibe/vibe.db)
editor External editor for long notes (TODO)
theme Reserved for future themes
default_view Startup module: notes, tasks, contacts, calendar

------------------------------------------------------------------------


PROJECT STRUCTURE


    vibe/
    ├── cmd/
    │   ├── vibe/main.go       # Entry point
    │   └── vibe-seed/         # Demo data seeder (loads from de
    ├── internal/
    │   ├── app/               # Bubble Tea model, layout, navig
    │   ├── calendar/          # Calendar view (month grid, even
    │   ├── config/            # JSON config loader
    │   ├── contacts/          # Contacts view (card layout, mul
    │   ├── dataview/          # Reusable grid/table component
    │   ├── db/                # SQLite connection, migrations, 
    │   ├── notes/             # Notes view
    │   ├── tasks/             # Tasks view (search, filter, sor
    │   ├── toast/             # Transient notifications for CRU
    │   ├── trash/             # Trashcan view (soft-deleted ite
    │   └── ui/                # Shared styles, pretty-time help
    ├── Makefile               # build, build-package, build-demo,
    ├── scripts/
    │   ├── install-go.sh      # Install Go 1.24.2+
    │   └── md2txt.go          # Generate .txt ASCII from .md (m
    ├── go.mod
    ├── PLAN.md                # Architecture and roadmap
    └── README.md


------------------------------------------------------------------------


TECH STACK


Layer Technology
TUI Bubble Tea (https://github.com/charmbracelet/bubbletea), Lipgloss
(https://github.com/charmbracelet/lipgloss), Bubbles
(https://github.com/charmbracelet/bubbles)
Database modernc.org/sqlite (https://modernc.org/sqlite) (pure Go)
Config JSON

------------------------------------------------------------------------


ROADMAP


See PLAN.md (PLAN.md) for the full architecture, schema, and
implementation phases. Current status:

  - [x] TUI shell with Outlook-inspired layout
  - [x] SQLite backend and migrations (calendar_events)
  - [x] Calendar: month grid, event list, add/edit/delete, time, notes,
    attendees
  - [x] Tasks: list, add/edit/delete, toggle done, reorder
  - [x] Notes: list, add/edit/delete, double-Enter saves
  - [x] Contacts: card view, multi-column when wide
  - [x] Trash: soft-delete, restore
  - [x] Tasks: search, filter, sort, pretty date
  - [x] Demo: make build-demo, VIBE_DB=./vibe-demo.db ./vibePDA
  - [x] Redistributable package: make build-package (or make all); ASCII
    .txt for all markdown docs
  - [ ] External editor integration, themes

------------------------------------------------------------------------


LICENSE


MIT (or as specified in the project)
