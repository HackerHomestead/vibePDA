package views

import (
	"github.com/charmbracelet/lipgloss"
)

// Placeholder content for each module (Outlook-style "folder" views)
var PlaceholderStyle = lipgloss.NewStyle().
	Foreground(lipgloss.Color("245")).
	Italic(true)

// NotesView returns placeholder content for Notes module.
func NotesView() string {
	return PlaceholderStyle.Render(`
  Notes
  ────────────────────────────────────────────────────────────
  Your notes and scratchpad.

  No notes yet. Press 'n' to create a new note (coming soon).
`)
}

// TasksView returns placeholder content for Tasks module.
func TasksView() string {
	return PlaceholderStyle.Render(`
  Tasks
  ────────────────────────────────────────────────────────────
  Your to-do list.

  No tasks yet. Press 'n' to add a task (coming soon).
`)
}

// ContactsView returns placeholder content for Contacts module.
func ContactsView() string {
	return PlaceholderStyle.Render(`
  Contacts
  ────────────────────────────────────────────────────────────
  Your contact list.

  No contacts yet. Press 'n' to add a contact (coming soon).
`)
}

// CalendarView returns placeholder content (unused when calendar model is active).
func CalendarView() string {
	return PlaceholderStyle.Render(`
  Calendar
  ────────────────────────────────────────────────────────────
  Events and appointments.
`)
}
