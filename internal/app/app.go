package app

import (
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/ui/views"
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

	mainStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(lipgloss.Color("240")).
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

// Model is the Bubble Tea application model (Outlook-inspired layout).
type Model struct {
	sidebar list.Model
	width   int
	height  int
}

// New creates a new application model.
func New(defaultView string) Model {
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

	// Select default view
	idx := indexForView(defaultView)
	l.Select(idx)

	return Model{
		sidebar: l,
		width:   80,
		height:  24,
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
		return 1 // tasks
	}
}

// Init runs on program start.
func (m Model) Init() tea.Cmd {
	return nil
}

// Update handles messages.
func (m Model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c", "q":
			return m, tea.Quit
		}
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		m.sidebar.SetSize(msg.Width/4, msg.Height-4)
		return m, nil
	}

	var cmd tea.Cmd
	m.sidebar, cmd = m.sidebar.Update(msg)
	return m, cmd
}

// View renders the UI.
func (m Model) View() string {
	// Outlook-style: title bar, then sidebar + main area, then help
	title := titleBarStyle.Render(" Vibe — Personal Data Assistant ")
	title = lipgloss.Place(m.width, 1, lipgloss.Left, lipgloss.Top, title)

	// Sidebar + main content
	sidebarView := sidebarStyle.Width(sidebarWidth + 4).Height(m.height - 6).Render(m.sidebar.View())

	mainContent := m.mainContent()
	mainWidth := m.width - sidebarWidth - 10
	mainView := mainStyle.Width(mainWidth).Height(m.height - 6).Render(mainContent)

	body := lipgloss.JoinHorizontal(lipgloss.Top, sidebarView, mainView)

	// Help bar
	help := helpStyle.Render(" ↑/↓ Navigate  Enter Select module  q Quit ")

	return title + "\n" + body + "\n" + help
}

func (m Model) mainContent() string {
	idx := m.sidebar.Index()
	switch idx {
	case ModuleNotes:
		return views.NotesView()
	case ModuleTasks:
		return views.TasksView()
	case ModuleContacts:
		return views.ContactsView()
	case ModuleCalendar:
		return views.CalendarView()
	default:
		return views.TasksView()
	}
}
