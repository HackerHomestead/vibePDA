package trash

import (
	"fmt"
	"io"
	"sort"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/dataview"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/toast"
)

const listHeight = 14

// TrashItem is one row in the trash (any type).
type TrashItem struct {
	Type      string     // "Note", "Task", "Contact", "Event"
	ID        int64
	DisplayTitle string   // title or name for display
	CreatedAt *time.Time
	DeletedAt *time.Time
}

func (i TrashItem) Title() string       { return i.DisplayTitle }
func (i TrashItem) Description() string { return i.Type + " | " + dataview.FormatDate(i.DeletedAt) }
func (i TrashItem) FilterValue() string { return i.Type + " " + i.DisplayTitle }

var (
	titleStyle   = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("62"))
	headerStyle  = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("245"))
	rowStyle    = lipgloss.NewStyle().Foreground(lipgloss.Color("252"))
	selectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("15")).Background(lipgloss.Color("62")).Padding(0, 1)
)

type trashDelegate struct{}

func (d trashDelegate) Height() int                             { return 1 }
func (d trashDelegate) Spacing() int                            { return 0 }
func (d trashDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d trashDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	t, ok := item.(TrashItem)
	if !ok {
		return
	}
	created := ""
	if t.CreatedAt != nil {
		created = t.CreatedAt.Format("2006-01-02 15:04")
	}
	deleted := ""
	if t.DeletedAt != nil {
		deleted = t.DeletedAt.Format("2006-01-02 15:04")
	}
	title := t.DisplayTitle
	if title == "" {
		title = "(no title)"
	}
	line := padTruncate(t.Type, 8) + " " + padTruncate(fmt.Sprintf("%d", t.ID), 6) + " " + padTruncate(title, 32) + " " + padTruncate(created, 16) + " " + padTruncate(deleted, 16)
	if index == m.Index() {
		line = selectedStyle.Render(line)
	} else {
		line = rowStyle.Render(line)
	}
	io.WriteString(w, line+"\n")
}

func padTruncate(s string, w int) string {
	runes := []rune(s)
	if len(runes) > w {
		return string(runes[:w-1]) + "…"
	}
	return s + strings.Repeat(" ", w-len(runes))
}

// Model is the trashcan view model.
type Model struct {
	calendarRepo *db.CalendarRepo
	tasksRepo    *db.TasksRepo
	notesRepo    *db.NotesRepo
	contactsRepo *db.ContactsRepo
	items        []TrashItem
	list         list.Model
	width        int
	height       int
	err          string
}

// NewModel creates a trash model.
func NewModel(calendarRepo *db.CalendarRepo, tasksRepo *db.TasksRepo, notesRepo *db.NotesRepo, contactsRepo *db.ContactsRepo, width, height int) Model {
	delegate := trashDelegate{}
	l := list.New([]list.Item{}, delegate, width-4, listHeight)
	l.Title = ""
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()
	return Model{
		calendarRepo: calendarRepo,
		tasksRepo:    tasksRepo,
		notesRepo:    notesRepo,
		contactsRepo: contactsRepo,
		list:         l,
		width:        width,
		height:       height,
	}
}

type trashLoadedMsg struct{ items []TrashItem }

func (m Model) loadTrash() tea.Msg {
	var items []TrashItem
	if m.notesRepo != nil {
		notes, _ := m.notesRepo.ListDeleted()
		for i := range notes {
			n := &notes[i]
			title := n.Title
			if title == "" && len(n.Content) > 0 {
				title = strings.TrimSpace(n.Content)
				if len(title) > 32 {
					title = title[:32] + "…"
				}
			}
			items = append(items, TrashItem{Type: "Note", ID: n.ID, DisplayTitle: title, CreatedAt: &n.CreatedAt, DeletedAt: n.DeletedAt})
		}
	}
	if m.tasksRepo != nil {
		tasks, _ := m.tasksRepo.ListDeleted()
		for i := range tasks {
			t := &tasks[i]
			items = append(items, TrashItem{Type: "Task", ID: t.ID, DisplayTitle: t.Title, CreatedAt: &t.CreatedAt, DeletedAt: t.DeletedAt})
		}
	}
	if m.contactsRepo != nil {
		contacts, _ := m.contactsRepo.ListDeleted()
		for i := range contacts {
			c := &contacts[i]
			title := c.Name
			if title == "" {
				title = c.Email
			}
			items = append(items, TrashItem{Type: "Contact", ID: c.ID, DisplayTitle: title, CreatedAt: &c.CreatedAt, DeletedAt: c.DeletedAt})
		}
	}
	if m.calendarRepo != nil {
		events, _ := m.calendarRepo.ListDeleted()
		for i := range events {
			e := &events[i]
			items = append(items, TrashItem{Type: "Event", ID: e.ID, DisplayTitle: e.Title, CreatedAt: &e.CreatedAt, DeletedAt: e.DeletedAt})
		}
	}
	sort.Slice(items, func(i, j int) bool {
		a, b := items[i].DeletedAt, items[j].DeletedAt
		if a == nil {
			return false
		}
		if b == nil {
			return true
		}
		return a.After(*b)
	})
	return trashLoadedMsg{items: items}
}

// Init loads trash items.
func (m Model) Init() tea.Cmd {
	return m.loadTrash
}

// Update handles messages.
func (m Model) Update(msg tea.Msg) (Model, tea.Cmd) {
	switch msg := msg.(type) {
	case trashLoadedMsg:
		m.items = msg.items
		litems := make([]list.Item, len(msg.items))
		for i := range msg.items {
			litems[i] = msg.items[i]
		}
		m.list.SetItems(litems)
		return m, nil
	case tea.KeyMsg:
		s := msg.String()
		if s == "r" || s == "R" {
			// Restore selected
			item := m.list.SelectedItem()
			if item == nil {
				return m, nil
			}
			t, ok := item.(TrashItem)
			if !ok {
				return m, nil
			}
			var err error
			switch t.Type {
			case "Note":
				err = m.notesRepo.Restore(t.ID)
			case "Task":
				err = m.tasksRepo.Restore(t.ID)
			case "Contact":
				err = m.contactsRepo.Restore(t.ID)
			case "Event":
				err = m.calendarRepo.Restore(t.ID)
			}
			if err != nil {
				m.err = err.Error()
				return m, nil
			}
			m.err = ""
			return m, tea.Batch(m.loadTrash, func() tea.Msg { return toast.Msg{Text: "Restored"} })
		}
		if s == "j" || s == "down" || s == "k" || s == "up" {
			var cmd tea.Cmd
			m.list, cmd = m.list.Update(msg)
			return m, cmd
		}
		return m, nil
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		m.list.SetSize(msg.Width-10, listHeight)
		return m, nil
	}
	var cmd tea.Cmd
	m.list, cmd = m.list.Update(msg)
	return m, cmd
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	m.list.SetSize(w-10, listHeight)
}

// StatusHint returns key bindings for the status bar.
func (m Model) StatusHint() string {
	return "j/k move  R restore  F5 refresh"
}

// View renders the trash UI (reusable dataview grid).
func (m Model) View() string {
	var b strings.Builder
	b.WriteString(titleStyle.Render(" Trash ") + "\n\n")
	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}
	// Header row
	b.WriteString(headerStyle.Render("Type     ID    Title                            Created          Deleted") + "\n")
	b.WriteString(m.list.View())
	return b.String()
}
