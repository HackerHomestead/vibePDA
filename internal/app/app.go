package app

import (
	"io"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/calendar"
	"github.com/you/vibe/internal/contacts"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/notes"
	"github.com/you/vibe/internal/tasks"
	"github.com/you/vibe/internal/toast"
	"github.com/you/vibe/internal/trash"
)

// Module identifiers (sidebar list: Notes, Tasks, Contacts, Calendar, Trash)
const (
	ModuleNotes = iota
	ModuleTasks
	ModuleContacts
	ModuleCalendar
	ModuleTrash
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

	// DOS-style status bar (blue background, white text)
	statusBarStyle = lipgloss.NewStyle().
			Background(lipgloss.Color("17")).
			Foreground(lipgloss.Color("15")).
			Padding(0, 1)
)

// BuildNumber is set at build time via -ldflags, reads from VERSION file (e.g. 0.3.0)
var BuildNumber string

// Focus pane: sidebar or main content
const (
	FocusSidebar = iota
	FocusMain
)

// clearToastMsg is sent after a delay to hide the toaster.
type clearToastMsg struct{}

// Model is the Bubble Tea application model (Outlook-inspired layout).
type Model struct {
	sidebar   list.Model
	calendar  calendar.Model
	tasks     tasks.Model
	notes     notes.Model
	contacts  contacts.Model
	trash     trash.Model
	focusPane int // FocusSidebar or FocusMain
	width     int
	height    int
	toast     string // transient message (e.g. "Event deleted")
}

// New creates a new application model.
func New(defaultView string, calendarRepo *db.CalendarRepo, tasksRepo *db.TasksRepo, notesRepo *db.NotesRepo, contactsRepo *db.ContactsRepo) Model {
	items := []list.Item{
		moduleItem{title: "Notes", id: ModuleNotes},
		moduleItem{title: "Tasks", id: ModuleTasks},
		moduleItem{title: "Contacts", id: ModuleContacts},
		moduleItem{title: "Calendar", id: ModuleCalendar},
		moduleItem{title: "Trash", id: ModuleTrash},
	}

	delegate := moduleDelegate{}
	l := list.New(items, delegate, sidebarWidth, listHeight)
	l.Title = " Modules"
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
	tr := trash.NewModel(calendarRepo, tasksRepo, notesRepo, contactsRepo, mainW, mainH)

	return Model{
		sidebar:   l,
		calendar:  cal,
		tasks:     tsk,
		notes:     nts,
		contacts:  con,
		trash:     tr,
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
	case "trash":
		return 4
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
	case ModuleTrash:
		return m.trash.Init()
	}
	return nil
}

// Update handles messages.
func (m Model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	// Toaster: show message and schedule clear
	switch v := msg.(type) {
	case toast.Msg:
		m.toast = v.Text
		return m, tea.Tick(3*time.Second, func(time.Time) tea.Msg { return clearToastMsg{} })
	case clearToastMsg:
		m.toast = ""
		return m, nil
	}

	routingMsg := msg
	switch kmsg := msg.(type) {
	case tea.KeyMsg:
		s := kmsg.String()
		switch s {
		case "ctrl+c", "q", "f12":
			return m, tea.Quit
		case "tab", "f8":
			m.focusPane = FocusMain
			return m, nil
		case "shift+tab", "f9":
			m.focusPane = FocusSidebar
			return m, nil
		case "f1":
			m.sidebar.Select(ModuleNotes)
			return m, m.notes.Init()
		case "f2":
			// F2: Tasks module when sidebar; Save when main+form
			if m.focusPane == FocusSidebar {
				m.sidebar.Select(ModuleTasks)
				return m, m.tasks.Init()
			}
			// Main pane: pass through to module (form save handled there)
		case "f3":
			m.sidebar.Select(ModuleContacts)
			return m, m.contacts.Init()
		case "f4":
			m.sidebar.Select(ModuleCalendar)
			return m, m.calendar.Init()
		case "f5":
			if m.focusPane == FocusSidebar {
				m.sidebar.Select(ModuleTrash)
				return m, m.trash.Init()
			}
			// Main pane: F5 = New (or R refresh when in Trash) — fall through to map
			fallthrough
		case "f6", "f7", "f10", "f11":
			// Route F-keys to main pane when focused; translate to action keys
			if m.focusPane == FocusMain {
				mapped := map[string]tea.KeyMsg{
					"f5":  {Type: tea.KeyRunes, Runes: []rune{'n'}},
					"f6":  {Type: tea.KeyEnter},
					"f7":  {Type: tea.KeyRunes, Runes: []rune{'d'}},
					"f10": {Type: tea.KeyF2},
					"f11": {Type: tea.KeyEscape},
				}
				if m.sidebar.Index() == ModuleTrash {
					mapped["f5"] = tea.KeyMsg{Type: tea.KeyRunes, Runes: []rune{'r'}} // refresh trash
				}
				if fake, ok := mapped[s]; ok {
					routingMsg = fake
				} else {
					return m, nil
				}
			} else if s != "f5" {
				return m, nil
			}
		default:
			// Continue to normal routing
		}
	case tea.WindowSizeMsg:
		ws := msg.(tea.WindowSizeMsg)
		m.width = ws.Width
		m.height = ws.Height
		m.sidebar.SetSize(ws.Width/4, ws.Height-4)
		mainW := m.width - sidebarWidth - 10
		mainH := m.height - 6
		m.calendar.SetSize(mainW, mainH)
		m.tasks.SetSize(mainW, mainH)
		m.notes.SetSize(mainW, mainH)
		m.contacts.SetSize(mainW, mainH)
		m.trash.SetSize(mainW, mainH)
		return m, nil
	}

	idx := m.sidebar.Index()

	// Route async load msgs (notesMsg, tasksMsg, etc.) to modules regardless of focus.
	// When Init() runs, the Cmd returns a msg that must reach the module's Update;
	// otherwise records never populate until the user Tabs (which triggers routing).
	// Only pass non-KeyMsg so key routing remains correct.
	if _, isKey := routingMsg.(tea.KeyMsg); !isKey {
		var loadCmd tea.Cmd
		m.calendar, loadCmd = m.calendar.Update(routingMsg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.tasks, loadCmd = m.tasks.Update(routingMsg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.notes, loadCmd = m.notes.Update(routingMsg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.contacts, loadCmd = m.contacts.Update(routingMsg)
		if loadCmd != nil {
			return m, loadCmd
		}
		m.trash, loadCmd = m.trash.Update(routingMsg)
		if loadCmd != nil {
			return m, loadCmd
		}
	}

	// Main pane focused: route keys to modules
	if m.focusPane == FocusMain {
		if keyMsg, ok := routingMsg.(tea.KeyMsg); ok && keyMsg.String() == "shift+tab" {
			m.focusPane = FocusSidebar
			return m, nil
		}
		if idx == ModuleCalendar {
			var cmd tea.Cmd
			m.calendar, cmd = m.calendar.Update(routingMsg)
			return m, cmd
		}
		if idx == ModuleTasks {
			var cmd tea.Cmd
			m.tasks, cmd = m.tasks.Update(routingMsg)
			return m, cmd
		}
		if idx == ModuleNotes {
			var cmd tea.Cmd
			m.notes, cmd = m.notes.Update(routingMsg)
			return m, cmd
		}
		if idx == ModuleContacts {
			var cmd tea.Cmd
			m.contacts, cmd = m.contacts.Update(routingMsg)
			return m, cmd
		}
		if idx == ModuleTrash {
			var cmd tea.Cmd
			m.trash, cmd = m.trash.Update(routingMsg)
			return m, cmd
		}
	}

	// Sidebar focused or other module: route to sidebar
	if keyMsg, ok := routingMsg.(tea.KeyMsg); ok && keyMsg.String() == "tab" {
		m.focusPane = FocusMain
		return m, nil
	}

	var cmd tea.Cmd
	m.sidebar, cmd = m.sidebar.Update(routingMsg)

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
	case ModuleTrash:
		cmd = tea.Batch(cmd, m.trash.Init())
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

	// DOS-style status bar at bottom (blue background, white text)
	statusBar := statusBarStr(m)
	statusBar = statusBarStyle.Width(m.width).Render(statusBar)

	out := title + "\n" + body + "\n"
	if m.toast != "" {
		toasterStyle := lipgloss.NewStyle().
			Background(lipgloss.Color("62")).
			Foreground(lipgloss.Color("15")).
			Padding(0, 2)
		toaster := toasterStyle.Render(" " + m.toast + " ")
		out += lipgloss.Place(m.width, 1, lipgloss.Center, lipgloss.Left, toaster) + "\n"
	}
	return out + statusBar
}

// statusBarStr returns the DOS-style status bar content (dynamic by active module).
func statusBarStr(m Model) string {
	var hint string
	switch m.sidebar.Index() {
	case ModuleNotes:
		hint = m.notes.StatusHint()
	case ModuleTasks:
		hint = m.tasks.StatusHint()
	case ModuleContacts:
		hint = m.contacts.StatusHint()
	case ModuleCalendar:
		hint = m.calendar.StatusHint()
	case ModuleTrash:
		hint = m.trash.StatusHint()
	default:
		hint = m.tasks.StatusHint()
	}
	return " F1 Notes  F2 Tasks  F3 Contacts  F4 Calendar  F5 Trash  |  " + hint + "  |  F8 Main  F9 Side  |  F12 Quit "
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
	case ModuleTrash:
		return m.trash.View()
	default:
		return m.tasks.View()
	}
}
