package app

import (
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/calendar"
	"github.com/you/vibe/internal/contacts"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/notes"
	"github.com/you/vibe/internal/tasks"
)

// Module identifiers (Outlook-style folder list)
const (
	ModuleNotes = iota
	ModuleTasks
	ModuleContacts
	ModuleCalendar
)

const (
	sidebarWidth = 20
	listHeight   = 12
)

// moduleItem implements list.Item for the sidebar.
type moduleItem struct {
	title string
	id    int
}

func (i moduleItem) Title() string       { return i.title }
func (i moduleItem) Description() string { return "" }
func (i moduleItem) FilterValue() string { return i.title }

// moduleDelegate renders sidebar list items.
type moduleDelegate struct{}

func (d moduleDelegate) Height() int                               { return 1 }
func (d moduleDelegate) Spacing() int                              { return 0 }
func (d moduleDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd   { return nil }
func (d moduleDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	i, ok := item.(moduleItem)
	if !ok {
		return
	}
	str := "  " + i.title
	if index == m.Index() {
		str = "▶ " + i.title
		str = selectedItemStyle.Render(str)
	} else {
		str = unselectedItemStyle.Render(str)
	}
	io.WriteString(w, str)
}

var (
	titleBarStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(lipgloss.Color("15")).
			Background(lipgloss.Color("62")).
			Padding(0, 1)

	sidebarStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(lipgloss.Color("240")).
			Padding(0, 1).
			MarginRight(1)

	sidebarStyleFocused = lipgloss.NewStyle().
				Border(lipgloss.ThickBorder()).
				BorderForeground(lipgloss.Color("62")).
				Padding(0, 1).
				MarginRight(1)

	mainStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(lipgloss.Color("240")).
			Padding(0, 1)

	mainStyleFocused = lipgloss.NewStyle().
				Border(lipgloss.ThickBorder()).
				BorderForeground(lipgloss.Color("62")).
				Padding(0, 1)

	selectedItemStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color("15")).
				Background(lipgloss.Color("62")).
				Padding(0, 1)

	unselectedItemStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color("252")).
				Padding(0, 1)

	helpStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("241")).
			Padding(0, 1)
)

// BuildNumber is set at build time via -ldflags, reads from VERSION file (e.g. 0.3.0)
var BuildNumber string

// Focus pane: sidebar or main content
const (
	FocusSidebar = iota
	FocusMain
)

// Model is the Bubble Tea application model (Outlook-inspired layout).
type Model struct {
	sidebar   list.Model
	calendar  calendar.Model
	tasks     tasks.Model
	notes     notes.Model
	contacts  contacts.Model
	focusPane int // FocusSidebar or FocusMain
	width     int
	height    int
}

// New creates a new application model.
func New(defaultView string, calendarRepo *db.CalendarRepo, tasksRepo *db.TasksRepo, notesRepo *db.NotesRepo, contactsRepo *db.ContactsRepo) Model {
	items := []list.Item{
		moduleItem{title: "Notes", id: ModuleNotes},
		moduleItem{title: "Tasks", id: ModuleTasks},
		moduleItem{title: "Contacts", id: ModuleContacts},
		moduleItem{title: "Calendar", id: ModuleCalendar},
	}

	delegate := moduleDelegate{}
	l := list.New(items, delegate, sidebarWidth, listHeight)
	l.Title = " Folders"
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	idx := indexForView(defaultView)
	l.Select(idx)

	mainW, mainH := 60, 20
	cal := calendar.NewModel(calendarRepo, contactsRepo, mainW, mainH)
	tsk := tasks.NewModel(tasksRepo, mainW, mainH)
	nts := notes.NewModel(notesRepo, mainW, mainH)
	con := contacts.NewModel(contactsRepo, mainW, mainH)

	return Model{
		sidebar:   l,
		calendar:  cal,
		tasks:     tsk,
		notes:     nts,
		contacts:  con,
		focusPane: FocusSidebar,
		width:     80,
		height:    24,
	}
}

func indexForView(name string) int {
	switch strings.ToLower(name) {
	case "notes":
		return 0
	case "tasks":
		return 1
	case "contacts":
		return 2
	case "calendar":
		return 3
	default:
		return 1
	}
}

// Init runs on program start.
func (m Model) Init() tea.Cmd {
	switch m.sidebar.Index() {
	case ModuleCalendar:
		return m.calendar.Init()
	case ModuleTasks:
		return m.tasks.Init()
	case ModuleNotes:
		return m.notes.Init()
	case ModuleContacts:
		return m.contacts.Init()
	}
	return nil
}

// Update handles messages.
func (m Model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c", "q":
			return m, tea.Quit
		case "tab":
			m.focusPane = FocusMain
			return m, nil
		case "shift+tab":
			m.focusPane = FocusSidebar
			return m, nil
		}
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		m.sidebar.SetSize(msg.Width/4, msg.Height-4)
		mainW := m.width - sidebarWidth - 10
		mainH := m.height - 6
		m.calendar.SetSize(mainW, mainH)
		m.tasks.SetSize(mainW, mainH)
		m.notes.SetSize(mainW, mainH)
		m.contacts.SetSize(mainW, mainH)
		return m, nil
	}

	idx := m.sidebar.Index()

	// Route async load msgs (notesMsg, tasksMsg, etc.) to modules regardless of focus.
	// When Init() runs, the Cmd returns a msg that must reach the module's Update;
	// otherwise records never populate until the user Tabs (which triggers routing).
	// Only pass non-KeyMsg so key routing remains correct.
	if _, isKey := msg.(tea.KeyMsg); !isKey {
		var loadCmd tea.Cmd
		m.calendar, loadCmd = m.calendar.Update(msg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.tasks, loadCmd = m.tasks.Update(msg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.notes, loadCmd = m.notes.Update(msg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.contacts, loadCmd = m.contacts.Update(msg)
		if loadCmd != nil {
			return m, loadCmd
		}
	}

	// Main pane focused: route keys to modules
	if m.focusPane == FocusMain {
		if keyMsg, ok := msg.(tea.KeyMsg); ok && keyMsg.String() == "shift+tab" {
			m.focusPane = FocusSidebar
			return m, nil
		}
		if idx == ModuleCalendar {
			var cmd tea.Cmd
			m.calendar, cmd = m.calendar.Update(msg)
			return m, cmd
		}
		if idx == ModuleTasks {
			var cmd tea.Cmd
			m.tasks, cmd = m.tasks.Update(msg)
			return m, cmd
		}
		if idx == ModuleNotes {
			var cmd tea.Cmd
			m.notes, cmd = m.notes.Update(msg)
			return m, cmd
		}
		if idx == ModuleContacts {
			var cmd tea.Cmd
			m.contacts, cmd = m.contacts.Update(msg)
			return m, cmd
		}
	}

	// Sidebar focused or other module: route to sidebar
	if keyMsg, ok := msg.(tea.KeyMsg); ok && keyMsg.String() == "tab" {
		m.focusPane = FocusMain
		return m, nil
	}

	var cmd tea.Cmd
	m.sidebar, cmd = m.sidebar.Update(msg)

	// Init module when it becomes selected
	switch m.sidebar.Index() {
	case ModuleCalendar:
		cmd = tea.Batch(cmd, m.calendar.Init())
	case ModuleTasks:
		cmd = tea.Batch(cmd, m.tasks.Init())
	case ModuleNotes:
		cmd = tea.Batch(cmd, m.notes.Init())
	case ModuleContacts:
		cmd = tea.Batch(cmd, m.contacts.Init())
	}

	return m, cmd
}

// View renders the UI.
func (m Model) View() string {
	titleStr := " vibePDA "
	if BuildNumber != "" {
		titleStr = " vibePDA (build " + BuildNumber + ") "
	}
	title := titleBarStyle.Render(titleStr)
	title = lipgloss.Place(m.width, 1, lipgloss.Left, lipgloss.Top, title)

	sidebarBox := sidebarStyle
	if m.focusPane == FocusSidebar {
		sidebarBox = sidebarStyleFocused
	}
	sidebarView := sidebarBox.Width(sidebarWidth + 4).Height(m.height - 6).Render(m.sidebar.View())

	mainContent := m.mainContent()
	mainWidth := m.width - sidebarWidth - 10
	mainBox := mainStyle
	if m.focusPane == FocusMain {
		mainBox = mainStyleFocused
	}
	mainView := mainBox.Width(mainWidth).Height(m.height - 6).Render(mainContent)

	body := lipgloss.JoinHorizontal(lipgloss.Top, sidebarView, mainView)

	help := " Tab: main  ↑/↓ sidebar  q Quit "
	idx := m.sidebar.Index()
	if m.focusPane == FocusMain {
		switch idx {
		case ModuleCalendar:
			help = " Shift+Tab: sidebar  ←/→ month  ,/. day  a all  t today  n new  Enter edit  F2 save  Esc cancel  d delete  q Quit "
		case ModuleTasks:
			help = " Shift+Tab: sidebar  n new  Enter edit  space toggle  Ctrl+↑/↓ reorder  d delete  q Quit "
		case ModuleNotes:
			help = " Shift+Tab: sidebar  n new  Enter edit  F2 save  Esc cancel  d delete  j/k select  q Quit "
		case ModuleContacts:
			help = " Shift+Tab: sidebar  n new  Enter edit  F2 save  Esc cancel  d delete  j/k select  q Quit "
		default:
			help = " Shift+Tab: sidebar  q Quit "
		}
	} else {
		help = " Tab: main  ↑/↓ modules  q Quit "
	}
	helpBar := helpStyle.Render(help)

	return title + "\n" + body + "\n" + helpBar
}

func (m Model) mainContent() string {
	switch m.sidebar.Index() {
	case ModuleNotes:
		return m.notes.View()
	case ModuleTasks:
		return m.tasks.View()
	case ModuleContacts:
		return m.contacts.View()
	case ModuleCalendar:
		return m.calendar.View()
	default:
		return m.tasks.View()
	}
}
