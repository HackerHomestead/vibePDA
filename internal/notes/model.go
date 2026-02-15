package notes

import (
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textarea"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/ui"
	"github.com/you/vibe/internal/toast"
)

const listHeight = 14

// noteItem implements list.Item.
type noteItem struct {
	note *db.Note
}

func (i noteItem) Title() string       { return i.note.Title }
func (i noteItem) Description() string { return i.note.Content }
func (i noteItem) FilterValue() string { return i.note.Title }

type noteDelegate struct{}

func (d noteDelegate) Height() int                             { return 1 }
func (d noteDelegate) Spacing() int                            { return 0 }
func (d noteDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d noteDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	i, ok := item.(noteItem)
	if !ok {
		return
	}
	str := i.Title()
	if index == m.Index() {
		str = selectedStyle.Render("▶ " + str)
	} else {
		str = unselectedStyle.Render("  " + str)
	}
	io.WriteString(w, str)
}

var (
	titleStyle = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color(ui.ColorAccent))
	selectedStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color(ui.ColorTitleFg)).
			Background(lipgloss.Color(ui.ColorAccent)).
			Padding(0, 1)
	unselectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color(ui.ColorText))
)

// Model is the notes view model.
type Model struct {
	repo         *db.NotesRepo
	noteList     list.Model
	width        int
	height       int
	err          string
	showForm     bool
	formEditID   int64
	formTextarea textarea.Model
}

// NewModel creates a new notes model.
func NewModel(repo *db.NotesRepo, width, height int) Model {
	items := []list.Item{}
	delegate := noteDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = " Notes "
	l.SetShowTitle(true)
	l.Styles.Title = titleStyle
	l.SetShowStatusBar(false)
	l.SetShowPagination(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	ta := textarea.New()
	ta.Placeholder = "Note content..."
	ta.SetWidth(50)
	ta.SetHeight(8)

	return Model{
		repo:         repo,
		noteList:     l,
		formTextarea: ta,
		width:        width,
		height:       height,
	}
}

// Init loads notes.
func (m Model) Init() tea.Cmd {
	return m.loadNotes
}

func (m Model) loadNotes() tea.Msg {
	notes, err := m.repo.List()
	if err != nil {
		return errMsg{err: err}
	}
	return notesMsg{notes: notes}
}

type errMsg struct{ err error }
type notesMsg struct{ notes []db.Note }

// Update handles messages.
func (m Model) Update(msg tea.Msg) (Model, tea.Cmd) {
	switch msg := msg.(type) {
	case notesMsg:
		m.refreshList(msg.notes)
		return m, nil
	case errMsg:
		m.err = msg.err.Error()
		return m, nil
	case tea.KeyMsg:
		if m.showForm {
			return m.handleFormKey(msg)
		}
		switch msg.String() {
		case "n":
			m.showForm = true
			m.formEditID = 0
			m.formTextarea.SetValue("")
			m.formTextarea.Focus()
			return m, textarea.Blink
		case "enter":
			item := m.noteList.SelectedItem()
			if ni, ok := item.(noteItem); ok {
				m.showForm = true
				m.formEditID = ni.note.ID
				val := ni.note.Title
				if ni.note.Content != "" {
					val += "\n" + ni.note.Content
				}
				m.formTextarea.SetValue(val)
				m.formTextarea.Focus()
				return m, textarea.Blink
			}
			return m, nil
		case "d":
			return m.handleDelete()
		case "j", "down":
			var cmd tea.Cmd
			m.noteList, cmd = m.noteList.Update(msg)
			return m, cmd
		case "k", "up":
			var cmd tea.Cmd
			m.noteList, cmd = m.noteList.Update(msg)
			return m, cmd
		}
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		listH := msg.Height - 6
		if listH < 4 {
			listH = 4
		}
		m.noteList.SetSize(msg.Width-10, listH)
		return m, nil
	}

	var cmd tea.Cmd
	m.noteList, cmd = m.noteList.Update(msg)
	return m, cmd
}

func (m Model) handleFormKey(msg tea.KeyMsg) (Model, tea.Cmd) {
	s := msg.String()
	if s == "f2" {
		return m.formSave()
	}
	if s == "esc" || s == "escape" {
		m.showForm = false
		m.formTextarea.Blur()
		return m, nil
	}
	// Double Enter (empty line) saves, same as calendar event notes
	if s == "enter" {
		val := m.formTextarea.Value()
		if strings.HasSuffix(val, "\n") {
			return m.formSave()
		}
	}
	var cmd tea.Cmd
	m.formTextarea, cmd = m.formTextarea.Update(msg)
	return m, cmd
}

func (m Model) formSave() (Model, tea.Cmd) {
	val := strings.TrimSpace(m.formTextarea.Value())
	if val == "" {
		return m, nil
	}
	lines := strings.SplitN(val, "\n", 2)
	title := lines[0]
	content := ""
	if len(lines) > 1 {
		content = strings.TrimLeft(lines[1], "\n")
	}
	if m.formEditID != 0 {
		n, _ := m.repo.Get(m.formEditID)
		if n != nil {
			n.Title = title
			n.Content = content
			if err := m.repo.Update(n); err != nil {
				m.err = err.Error()
			} else {
				m.showForm = false
				m.formTextarea.Blur()
				m.err = ""
				m.refreshList(nil)
				return m, tea.Batch(m.loadNotes, func() tea.Msg { return toast.Msg{Text: "Note updated"} })
			}
		}
	} else {
		n := &db.Note{Title: title, Content: content}
		if err := m.repo.Create(n); err != nil {
			m.err = err.Error()
		} else {
			m.showForm = false
			m.formTextarea.Blur()
			m.err = ""
			m.refreshList(nil)
			return m, tea.Batch(m.loadNotes, func() tea.Msg { return toast.Msg{Text: "Note created"} })
		}
	}
	m.refreshList(nil)
	return m, m.loadNotes
}

func (m Model) handleDelete() (Model, tea.Cmd) {
	item := m.noteList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ni, ok := item.(noteItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.Delete(ni.note.ID); err != nil {
		m.err = err.Error()
		return m, nil
	}
	m.err = ""
	m.refreshList(nil)
	return m, tea.Batch(m.loadNotes, func() tea.Msg { return toast.Msg{Text: "Note deleted"} })
}

func (m *Model) refreshList(notes []db.Note) {
	if notes == nil {
		var err error
		notes, err = m.repo.List()
		if err != nil {
			return
		}
	}
	items := make([]list.Item, len(notes))
	for i := range notes {
		items[i] = noteItem{note: &notes[i]}
	}
	m.noteList.SetItems(items)
}

// Count returns the number of notes in the list.
func (m Model) Count() int {
	return len(m.noteList.Items())
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	listH := h - 6
	if listH < 4 {
		listH = 4
	}
	m.noteList.SetSize(w-10, listH)
}

// StatusHint returns context-specific key bindings for the status bar.
func (m Model) StatusHint() string {
	if m.showForm {
		return "Enter² save  F10 save  F11 cancel"
	}
	return "F5 new  F6 edit  F7 del"
}

// View renders the notes UI.
func (m Model) View() string {
	var b strings.Builder

	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}

	if m.showForm {
		title := " New Note "
		if m.formEditID != 0 {
			title = " Edit Note "
		}
		b.WriteString(titleStyle.Render(title) + "\n\n")
		b.WriteString("First line = title, rest = content:\n")
		b.WriteString(m.formTextarea.View())
		return b.String()
	}

	b.WriteString(m.noteList.View())

	return b.String()
}
