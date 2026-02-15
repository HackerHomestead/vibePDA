# vibePDA User Manual

**Version 0.8.0-alpha**

A comprehensive guide to using vibePDA, your terminal personal data assistant.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Navigation](#navigation)
4. [Modules](#modules)
5. [Search and Filter](#search-and-filter)
6. [Trash Management](#trash-management)
7. [Keyboard Shortcuts](#keyboard-shortcuts)
8. [Command-Line Interface](#command-line-interface)
9. [Tips and Tricks](#tips-and-tricks)
10. [Troubleshooting](#troubleshooting)

---

## Introduction

vibePDA is a terminal-based personal data assistant inspired by classic 1980s software. It provides a unified interface for managing notes, tasks, contacts, calendar events, facts, finances, and documents—all from your terminal.

### Key Features

- **1980s-style TUI**: Clean, retro interface with function key navigation
- **Multiple modules**: Notes, Tasks, Contacts, Calendar, Facts, Finances, Documents, and Trash
- **Search/Filter**: Quickly find items across all modules
- **Soft delete**: Items go to Trash before permanent deletion
- **File-based storage**: No database required, simple text files
- **Cross-platform**: Linux, FreeDOS, and WebAssembly support

---

## Getting Started

### Installation

```bash
# Build from source
make
./vibePDA
```

### First Launch

When you first launch vibePDA, you'll see:

- **Sidebar (left)**: List of modules (Notes, Tasks, Contacts, Calendar, Facts, Finances, Documents, Trash)
- **Main pane (right)**: Content for the selected module
- **Menu bar (top)**: Function key shortcuts
- **Status bar (bottom)**: Current module and state

### Basic Navigation

- **Up/Down** or **j/k**: Navigate items in sidebar or main pane
- **Tab**: Switch focus between sidebar and main pane
- **Enter**: Edit selected item (when focus is in main pane)
- **q** or **F10**: Quit

---

## Navigation

### Sidebar Navigation

The sidebar shows all available modules:

```
 MODULES
  Notes
  Tasks
  Contacts
  Calendar
  Facts
  Finances
  Documents
  Trash
```

- Use **Up/Down** or **j/k** to move between modules
- The selected module is highlighted with reverse video (`> ` prefix)
- Each module shows its item count: `Notes (5)`

### Main Pane Navigation

- **Up/Down** or **j/k**: Navigate through items in the current module
- **Tab**: Move focus from sidebar to main pane
- **Shift+Tab** or **Left Arrow**: Move focus back to sidebar
- **Enter**: Edit the selected item

### Focus States

- **Sidebar focus**: Module selection highlighted, Up/Down changes module
- **Main pane focus**: Item selection highlighted, Up/Down navigates items

---

## Modules

### Notes

**Purpose**: Scratchpad and quick notes with multi-line content.

**Features**:
- Display as cards showing title and content preview
- Multi-line content editor with cursor and line numbers
- Full-text search (searches title and content)

**Creating a Note**:
1. Press **F2** or **N**
2. Enter title, press Enter
3. Enter content in the multi-line editor
4. Press **Enter+Enter** (two blank lines) to save
5. Press **Esc** to cancel

**Editing a Note**:
1. Select note in list
2. Press **F3** or **E**, or press **Enter**
3. Edit title, press Enter
4. Edit content in multi-line editor
5. Press **Enter+Enter** to save

**Deleting a Note**:
1. Select note
2. Press **F4** or **D**
3. Note moves to Trash (soft delete)

**Content Editor Controls**:
- **Enter**: Insert newline
- **Enter+Enter**: Save and exit
- **Esc**: Cancel editing
- **F5**: Toggle line numbers
- **Page Up/Down**: Scroll content
- **Arrow keys**: Move cursor

### Tasks

**Purpose**: To-do list with priorities and due dates.

**Features**:
- Checkbox display (`[x]` for done, `[ ]` for pending)
- Priority levels (0-3)
- Due date tracking
- Search by title

**Creating a Task**:
1. Press **F2** or **N**
2. Enter title, press Enter
3. Enter due date (YYYY-MM-DD), press Enter
4. Enter priority (0-3), press Enter
5. Task is created

**Completing a Task**:
- Tasks show `[x]` when done (managed through edit)

**Editing a Task**:
1. Select task
2. Press **F3** or **E**
3. Edit fields step by step
4. Press Enter after each field

**Deleting a Task**:
1. Select task
2. Press **F4** or **D**
3. Task moves to Trash

### Contacts

**Purpose**: Contact list with name, email, phone.

**Features**:
- Name, email, phone fields
- Search by name, email, or phone
- Simple list display

**Creating a Contact**:
1. Press **F2** or **N**
2. Enter name, press Enter
3. Enter email, press Enter
4. Enter phone, press Enter
5. Contact is created

**Editing a Contact**:
1. Select contact
2. Press **F3** or **E**
3. Edit fields step by step

**Deleting a Contact**:
1. Select contact
2. Press **F4** or **D**
3. Contact moves to Trash

### Calendar

**Purpose**: Events and appointments.

**Features**:
- Title, start time, end time, all-day flag
- Search by title
- Simple list display

**Creating an Event**:
1. Press **F2** or **N**
2. Enter title, press Enter
3. Enter start time, press Enter
4. Enter end time, press Enter
5. Event is created

**Editing an Event**:
1. Select event
2. Press **F3** or **E**
3. Edit fields step by step

**Deleting an Event**:
1. Select event
2. Press **F4** or **D**
3. Event moves to Trash

### Facts

**Purpose**: Key-value pairs for storing information like passwords, SSNs, etc.

**Features**:
- Key-value format (e.g., "Andrew SSN = 455-56-2022")
- Search by key or value
- Simple list display

**Creating a Fact**:
1. Press **F2** or **N**
2. Enter key, press Enter
3. Enter value, press Enter
4. Fact is created

**Editing a Fact**:
1. Select fact
2. Press **F3** or **E**
3. Edit key, press Enter
4. Edit value, press Enter

**Deleting a Fact**:
1. Select fact
2. Press **F4** or **D**
3. Fact moves to Trash

### Finances

**Purpose**: General ledger for tracking financial transactions.

**Status**: Stub module (not yet implemented)

**Planned Features**:
- Date, description, amount, category, account
- Transaction tracking
- Financial reporting

### Documents

**Purpose**: Templated forms for document management.

**Status**: Stub module (not yet implemented)

**Planned Features**:
- Template-based documents
- Form filling
- Document storage

### Trash

**Purpose**: View and manage soft-deleted items.

**Features**:
- Lists all deleted items from all modules
- Checkbox selection for batch operations
- Restore deleted items
- Permanent delete with confirmation

**Viewing Trash**:
- Navigate to Trash module in sidebar
- See all deleted items with their type and title
- Items show as `[Note]`, `[Task]`, `[Contact]`, etc.

**Restoring Items**:
1. Select item in Trash
2. Press **R**
3. Item is restored to its original module

**Selecting Items**:
- **Space**: Toggle checkbox for selected item
- **A**: Select all items
- **U**: Unselect all items

**Permanently Deleting Selected Items**:
1. Select items using checkboxes (Space to toggle)
2. Press **X**
3. Confirm with **Y** or **Enter**
4. Cancel with **N** or **Esc**

**Trash Module Controls**:
- **R**: Restore selected item
- **Space**: Toggle checkbox
- **A**: Select all
- **U**: Unselect all
- **X**: Delete selected (with confirmation)
- **Y**: Confirm deletion
- **N** or **Esc**: Cancel deletion

---

## Search and Filter

### Starting a Search

1. Navigate to any module (except Trash)
2. Press **F5** or **/**
3. Status bar changes to show "Search: " prompt
4. Type your search query
5. Results filter in real-time as you type

### Search Behavior

- **Case-insensitive**: Searches match regardless of case
- **Substring matching**: Finds items containing the search term
- **Real-time filtering**: Results update as you type
- **Filter persistence**: Filter remains active after pressing Enter

### Applying a Filter

1. Type your search query
2. Press **Enter** to apply the filter
3. Filtered list remains visible
4. You can perform actions (edit, delete) on filtered items
5. Status bar shows "Filtered" state
6. Module header shows `[Filter: query]`

### Clearing a Filter

- Press **F5** again (when filter is active)
- Or press **Esc** while in search input mode

### Search Fields by Module

| Module    | Search Fields                    |
|-----------|----------------------------------|
| Notes     | Title, Content                   |
| Tasks     | Title                             |
| Contacts  | Name, Email, Phone                |
| Calendar  | Title                             |
| Facts     | Key, Value                        |
| Finances  | (Not implemented yet)            |
| Documents | (Not implemented yet)            |

### Visual Indicators

- **Search input mode**: Status bar shows "Search: " with blinking cursor
- **Filter active**: Module header shows `[Filter: query]`
- **Status bar state**: Shows "Searching" or "Filtered"

---

## Trash Management

### Understanding Soft Delete

When you delete an item (F4 or D), it's not immediately removed. Instead:
1. Item is marked as deleted (`deleted_at` timestamp set)
2. Item disappears from its module
3. Item appears in Trash module
4. Item can be restored at any time

### Viewing Trash

1. Navigate to Trash module in sidebar
2. See all deleted items with:
   - Checkbox `[ ]` or `[X]` (selected)
   - Type indicator `[Note]`, `[Task]`, etc.
   - ID number
   - Title/name

### Restoring Items

**Single Item**:
1. Select item in Trash
2. Press **R**
3. Item is restored to its original module

### Permanent Deletion

**Selected Items**:
1. Use **Space** to toggle checkboxes for items to delete
2. Use **A** to select all, **U** to unselect all
3. Press **X** to delete selected items
4. Confirmation prompt appears: "PERMANENTLY DELETE X SELECTED ITEM(S)?"
5. Press **Y** or **Enter** to confirm
6. Press **N** or **Esc** to cancel

**Important**: Permanent deletion cannot be undone!

### Trash Interface

The Trash module has a special menu bar:
```
F1 Help | R Restore | Space Toggle | A All | U None | X Delete | F10 Quit
```

---

## Keyboard Shortcuts

### Global Shortcuts

| Key          | Action                    |
|--------------|---------------------------|
| **F1** or **?** | Show help                |
| **F2** or **N** | New item                |
| **F3** or **E** | Edit selected            |
| **F4** or **D** | Delete selected          |
| **F5** or **/** | Search/Filter            |
| **F10** or **q** | Quit                     |
| **Tab**      | Switch focus (sidebar ↔ main) |
| **Shift+Tab** | Switch to sidebar        |
| **Up/Down**  | Navigate items           |
| **j/k**      | Navigate items           |
| **Enter**    | Edit selected item       |
| **Esc**      | Cancel current operation |

### Navigation

| Key          | Action                    |
|--------------|---------------------------|
| **Up** or **k** | Move up                |
| **Down** or **j** | Move down              |
| **Tab**      | Switch between sidebar and main pane |
| **Left Arrow** | Switch to sidebar      |

### Search Mode

| Key          | Action                    |
|--------------|---------------------------|
| **F5** or **/** | Start search            |
| **Enter**    | Apply filter (keep active) |
| **Esc**      | Cancel search, clear filter |
| **F5** (when filtered) | Clear filter |
| **Backspace** | Delete character        |
| **Any character** | Add to search query |

### Trash Module

| Key          | Action                    |
|--------------|---------------------------|
| **R**        | Restore selected item     |
| **Space**    | Toggle checkbox           |
| **A**        | Select all items          |
| **U**        | Unselect all items        |
| **X**        | Delete selected (with confirmation) |
| **Y** or **Enter** | Confirm deletion |
| **N** or **Esc** | Cancel deletion |

### Content Editor (Notes)

| Key          | Action                    |
|--------------|---------------------------|
| **Enter**    | Insert newline            |
| **Enter+Enter** | Save and exit         |
| **Esc**      | Cancel editing            |
| **F5**       | Toggle line numbers        |
| **Page Up**  | Scroll up                 |
| **Page Down** | Scroll down              |
| **Arrow keys** | Move cursor            |
| **Backspace** | Delete character        |

---

## Command-Line Interface

### One-Shot Commands

Run a single operation and exit:

```bash
# Notes
./vibePDA notes add "Title" "Content"
./vibePDA notes list
./vibePDA notes show <id>
./vibePDA notes edit <id> "New Title" "New Content"
./vibePDA notes delete <id>

# Tasks
./vibePDA tasks add "Title" "2026-12-31" 1
./vibePDA tasks list
./vibePDA tasks show <id>
./vibePDA tasks delete <id>

# Contacts
./vibePDA contacts add "Name" "email@example.com" "555-1234"
./vibePDA contacts list
./vibePDA contacts show <id>
./vibePDA contacts delete <id>

# Calendar
./vibePDA calendar add "Event" "2026-02-15 10:00" "2026-02-15 11:00" 0
./vibePDA calendar list
./vibePDA calendar delete <id>

# Facts
./vibePDA facts add "key" "value"
./vibePDA facts list
./vibePDA facts show <id>
./vibePDA facts edit <id> "new_key" "new_value"
./vibePDA facts delete <id>

# Trash
./vibePDA trash list
./vibePDA trash restore <type> <id>
```

### Command Options

```bash
./vibePDA --help          # Show help and exit
./vibePDA --version       # Show version and exit
./vibePDA --config        # Print current configuration and exit
./vibePDA --data-dir DIR  # Override default data directory
./vibePDA --display MODE  # Set display size (small|auto|custom COLxROW)
```

**Configuration Options:**

- **`--config`**: Displays the current configuration including:
  - Data directory (default or overridden)
  - Database path
  - Editor path
  - Default module
  - Environment variables (HOME, VIBE_DATA on Linux)

- **`--data-dir DIR`**: Overrides the default data directory. All data files (notes.txt, tasks.txt, contacts.txt, etc.) will be stored in the specified directory. Useful for:
  - Using a different location for data files
  - Testing with isolated data
  - Portable installations (e.g., USB drives)
  
  Example:
  ```bash
  ./vibePDA --data-dir /tmp/my_vibe_data notes add "Test" "Content"
  ./vibePDA --data-dir /tmp/my_vibe_data --config
  ```

- **`--display MODE`**: Controls the terminal display size:
  - `small`: Fixed 80x25 terminal size
  - `auto`: Uses current terminal size (default)
  - `custom COLxROW`: Specify custom dimensions (e.g., `--display custom 120x30`)

---

## Tips and Tricks

### Efficient Workflow

1. **Use search for quick access**: Press F5, type a few characters, press Enter
2. **Keyboard navigation**: Learn j/k for navigation (faster than arrow keys)
3. **Tab switching**: Use Tab to quickly switch between sidebar and content
4. **Filter persistence**: Apply a filter, then perform batch operations on filtered items

### Notes Module

- Use Enter+Enter to save (two blank lines) - faster than Esc then save
- F5 toggles line numbers for easier navigation in long notes
- Content editor automatically scrolls to keep cursor visible

### Tasks Module

- Use priorities (0-3) to organize tasks
- Due dates help with time management
- Completed tasks stay in list (can be filtered out)

### Contacts Module

- Search works across name, email, and phone
- Useful for finding contacts by any identifier

### Trash Module

- Regularly check Trash and restore items you need
- Use select all (A) + delete (X) to clean up old items
- Remember: permanent delete cannot be undone!

### Search Tips

- Short queries work best (e.g., "meet" finds "meeting")
- Case doesn't matter ("JOHN" finds "John")
- Search persists after Enter - great for batch operations
- Press F5 again to clear filter when done

---

## Troubleshooting

### Common Issues

**Problem**: Can't see items after switching modules
- **Solution**: Press Tab to switch focus to main pane

**Problem**: Search filter won't clear
- **Solution**: Press F5 when filter is active (not in search input mode)

**Problem**: Can't delete items in Trash
- **Solution**: Make sure you've selected items with Space, then press X

**Problem**: Cursor not visible in search/prompt
- **Solution**: The blinking cursor (`_`) should appear at the end of input. If not visible, check terminal compatibility.

**Problem**: Items not showing after restore
- **Solution**: Navigate away from Trash and back, or refresh the module

### Terminal Compatibility

vibePDA requires:
- VT102 minimum terminal support
- Curses library (ncurses on Linux)
- Terminal size: Minimum 80x25 recommended

### Data Location

Data files are stored in:
- Linux: `~/.local/share/vibe/` or current directory
- Files: `notes.txt`, `tasks.txt`, `contacts.txt`, `events.txt`, `facts.txt`

### Getting Help

- Press **F1** or **?** in the application for help
- Check `README.md` for build and installation
- Review `CHANGELOG.md` for recent changes

---

## Advanced Usage

### Batch Operations

1. Apply a search filter
2. Navigate through filtered results
3. Perform operations (edit, delete) on multiple items
4. Clear filter when done

### Trash Cleanup Workflow

1. Navigate to Trash
2. Review deleted items
3. Restore items you need (R)
4. Select items to permanently delete (Space)
5. Press X to delete selected
6. Confirm with Y

### Multi-Module Workflow

1. Create items in appropriate modules
2. Use search to find items across modules
3. Delete items when done (they go to Trash)
4. Periodically clean Trash

---

## Module-Specific Details

### Notes

- **Content Editor**: Full-featured multi-line editor
- **Line Numbers**: Toggle with F5
- **Scrolling**: Automatic when cursor moves
- **Save**: Enter+Enter (two blank lines)
- **Cancel**: Esc

### Tasks

- **Priority**: 0 (low) to 3 (high)
- **Due Date**: Format YYYY-MM-DD
- **Done Status**: Managed through edit

### Contacts

- **Fields**: Name (required), Email (optional), Phone (optional)
- **Search**: Searches all three fields

### Calendar

- **Time Format**: YYYY-MM-DD HH:MM
- **All-Day**: Flag for all-day events
- **Search**: Searches event titles

### Facts

- **Format**: Key = Value
- **Use Cases**: Passwords, SSNs, license numbers, etc.
- **Search**: Searches both key and value

---

## Best Practices

1. **Regular Backups**: Copy data files from `~/.local/share/vibe/`
2. **Use Search**: Faster than scrolling through long lists
3. **Clean Trash**: Periodically review and permanently delete old items
4. **Organize Tasks**: Use priorities and due dates effectively
5. **Consistent Naming**: Use consistent titles/names for easier searching

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────┐
│                    vibePDA Quick Reference              │
├─────────────────────────────────────────────────────────┤
│ Navigation                                              │
│   Up/Down, j/k    Navigate items                       │
│   Tab             Switch sidebar ↔ main                 │
│   Enter           Edit selected                         │
│                                                          │
│ Actions                                                  │
│   F2/N            New item                              │
│   F3/E            Edit selected                         │
│   F4/D            Delete (to Trash)                     │
│   F5/             Search/Filter                         │
│   F1/?            Help                                  │
│   F10/q           Quit                                  │
│                                                          │
│ Search                                                  │
│   F5/             Start search                          │
│   Enter           Apply filter                          │
│   F5 (filtered)   Clear filter                          │
│   Esc             Cancel search                          │
│                                                          │
│ Trash                                                    │
│   R               Restore selected                      │
│   Space           Toggle checkbox                        │
│   A               Select all                            │
│   U               Unselect all                          │
│   X               Delete selected                       │
│   Y               Confirm deletion                       │
│   N/Esc           Cancel deletion                       │
└─────────────────────────────────────────────────────────┘
```

---

**End of User Manual**

For technical details, see `README.md` and `PLAN.md`.
For development information, see `docs/TESTING.md`.
