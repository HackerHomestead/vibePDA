# vibePDA

**A terminal personal data assistant** — Notes, Tasks, Contacts, and Calendar in a single TUI, inspired by classic Outlook.

**Status:** Alpha. Core TUI, storage, and CRUD are in place; polish and full feature parity are ongoing.

**C** implementation with three build targets: **1) Linux**, **2) FreeDOS** (DJGPP), **3) WebAssembly** (Emscripten). Uses **curses** (ncurses on Linux, PDCurses on DOS). Terminal: VT102 minimum; on Linux, modern terminal standards (e.g. SGR, 256 colors) may be used. File-based storage on all platforms (no SQLite).

---

## Features

| Module    | Description                 |
|-----------|-----------------------------|
| **Notes** | Scratchpad and quick notes  |
| **Tasks** | To-do list                  |
| **Contacts** | Contact list            |
| **Calendar** | Events and appointments |
| **Facts** | Key-value pairs (e.g. SSN, passwords) |
| **Trash** | Soft-deleted items, restore |

- **1980s-style TUI**: Menu bar (F1–F4, F10), status line. Uses curses (ncurses/PDCurses).
- **Note cards**: Notes display as cards (Title + Content); Up/Down navigate between them.
- **Multi-line content editor**: Note body uses a bordered text area with cursor, line numbers (F5), and scrolling (Page Up/Down). Enter=newline, Enter+Enter=save, Esc=cancel.
- **Hotkeys**: F2/N new, F3/E edit, F4/D delete, F1/? help, F10/q quit; Up/Down navigate; Tab switch pane.

---

## Requirements

- **Linux**: GCC, GNU Make, **ncurses** (`libncurses-dev`). File-based storage only (no SQLite).
- **Linux 32-bit**: `gcc-multilib`, `libc6-dev-i386`, ncurses for `TARGET=linux-ia32`.
- **FreeDOS**: DJGPP cross-compiler (`i586-pc-msdosdjgpp-gcc`), **PDCurses** for `TARGET=dos`.
- **WebAssembly**: Emscripten (`emcc`) for `TARGET=webasm` (produces `vibePDA.js` + `vibePDA.wasm`).

---

## Command-line options

- **`--help`**, **`-h`** — Print usage and exit.
- **`--version`**, **`-v`** — Print version and exit.
- **Unknown arguments** — Print "unknown argument" to stderr, show help, and exit with code 1.

### One-shot CLI (Linux and DOS)

Run a single CRUD operation and exit (no TUI):

- **notes** — `add <title> [content]` | `list` | `show <id>` | `edit <id> <title> [content]` | `delete <id>`
- **tasks** — `add <title> [due_date] [priority]` | `list` | `show <id>` | `delete <id>`
- **contacts** — `add <name> [email] [phone]` | `list` | `delete <id>`
- **calendar** — `add <title> [start] [end] [all_day]` | `list` | `delete <id>`
- **trash** — `list` | `restore <type> <id>` (type: note, task, contact, event)

Example: `./vibePDA notes add "My title" "Content"` prints the new note id; `./vibePDA notes list` prints tab-separated id, title, content.

### Interactive command mode (REPL)

Like a classic GW-BASIC session: prompt, type commands, see output, scroll back. Use **`-cmd`**, **`--cmd`**, or **`--mode command`**:

```bash
./vibePDA -cmd
vibePDA 0.7.3 — interactive command mode (type 'help' or 'quit')
vibe> notes list
1	My note	Content
vibe> tasks add "Todo"
1
vibe> quit
```

Same commands as one-shot; type `help` for a short list and `quit` (or `exit`, `q`) to exit. Supports `"quoted args"` for multi-word arguments.

---

## Build

**GNU Make** (direct):

```bash
# 1) Linux (default, ncurses, file-based storage)
make
./vibePDA --help
./vibePDA --version
./vibePDA
```

- **Clean rebuild**: `make clean all` or `make rebuild` — removes all binaries and object files, then rebuilds.
- 32-bit Linux: `make TARGET=linux-ia32`  
- **2) FreeDOS**: `make TARGET=dos` (produces `vibePDA.exe`, PDCurses)  
- **3) WebAssembly**: `make TARGET=webasm` (produces `vibePDA.js` and `vibePDA.wasm`; requires Emscripten)

**GNU Autotools** (recommended for packaging):

```bash
autoreconf -fi
./configure --help
./configure --version
./configure
make
make install
```

---

## Tests

```bash
make test
# or
make && ./run_tests
```

Runs unit tests for app (UI state / key handling) and storage (backend).

---

## Install

```bash
make install
# Optional: make install DESTDIR=/tmp/stage PREFIX=/usr/local
```

---

## Project layout

```
vibe/
├── Makefile           # GNU Make; TARGET=linux|linux-ia32|dos|webasm
├── config.h.in       # Config template
├── configure.ac      # Autoconf (GNU)
├── Makefile.am       # Automake (GNU)
├── src/
│   ├── main.c              # Entry point; CLI args; TUI loop
│   ├── app.c, app.h        # App shell: state, F-keys, note cards, content editor
│   ├── tui.c, tui.h        # Terminal I/O via curses (ncurses/PDCurses)
│   ├── vibe_config.c, vibe_config.h  # Config layer (paths, defaults)
│   ├── types.h             # Data types (Note, Task, Contact, Event)
│   ├── storage.h           # Storage API
│   └── storage_file.c      # File-based backend (all platforms)
├── tests/              # Unit tests (app + storage)
├── README.md, CHANGELOG.md, PLAN.md, VERSION
└── docs/
    └── TESTING.md
```

---

## License

MIT (or as specified in the project)
