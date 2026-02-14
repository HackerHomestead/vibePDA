package calendar

import (
	"fmt"
	"io"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
)

const (
	sidebarWidth = 20
	listHeight   = 8
)

// eventItem implements list.Item for the event list.
type eventItem struct {
	event *db.Event
}

func (i eventItem) Title() string {
	t := i.event.Title
	if i.event.AllDay {
		return "  " + t
	}
	return fmt.Sprintf("%s %s", i.event.StartAt.Format("15:04"), t)
}
func (i eventItem) Description() string { return "" }
func (i eventItem) FilterValue() string { return i.event.Title }

// eventDelegate renders event list items.
type eventDelegate struct{}

func (d eventDelegate) Height() int                             { return 1 }
func (d eventDelegate) Spacing() int                            { return 0 }
func (d eventDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d eventDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	i, ok := item.(eventItem)
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
	titleStyle = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("62"))
	gridStyle  = lipgloss.NewStyle().BorderStyle(lipgloss.NormalBorder()).BorderForeground(lipgloss.Color("240"))
	dayStyle   = lipgloss.NewStyle().Width(3).Align(lipgloss.Center)
	selectedStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("15")).
			Background(lipgloss.Color("62")).
			Padding(0, 1)
	unselectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("252"))
	todayStyle      = lipgloss.NewStyle().
			Foreground(lipgloss.Color("15")).
			Background(lipgloss.Color("241")).
			Padding(0, 1)
)

// Model is the calendar view model.
type Model struct {
	repo       *db.CalendarRepo
	year       int
	month      time.Month
	day        int // 0 = no day selected
	events     []db.Event
	eventList  list.Model
	width      int
	height     int
	err        string
	showForm   bool   // true when adding/editing
	formTitle  string // for new event
}

// NewModel creates a new calendar model.
func NewModel(repo *db.CalendarRepo, width, height int) Model {
	items := []list.Item{}
	delegate := eventDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = ""
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	now := time.Now()
	m := Model{
		repo:      repo,
		year:      now.Year(),
		month:     now.Month(),
		day:       now.Day(),
		eventList: l,
		width:     width,
		height:    height,
	}
	return m
}

// Init loads initial events.
func (m Model) Init() tea.Cmd {
	return m.loadEvents
}

// loadEventsCmd returns a command to load events for the current month.
func (m Model) loadEvents() tea.Msg {
	events, err := m.repo.ListByMonth(m.year, int(m.month))
	if err != nil {
		return errMsg{err: err}
	}
	return eventsMsg{events: events}
}

type errMsg struct{ err error }
type eventsMsg struct{ events []db.Event }

// Update handles messages.
func (m Model) Update(msg tea.Msg) (Model, tea.Cmd) {
	switch msg := msg.(type) {
	case eventsMsg:
		m.events = msg.events
		m.refreshEventList()
		return m, nil
	case errMsg:
		m.err = msg.err.Error()
		return m, nil
	case tea.KeyMsg:
		if m.showForm {
			return m.handleFormKey(msg)
		}
		switch msg.String() {
		case "h", "left":
			m.prevMonth()
			return m, m.loadEvents
		case "l", "right":
			m.nextMonth()
			return m, m.loadEvents
		case "t":
			now := time.Now()
			m.year = now.Year()
			m.month = now.Month()
			m.day = now.Day()
			m.refreshEventList()
			return m, m.loadEvents
		case "n":
			m.showForm = true
			m.formTitle = ""
			return m, nil
		case "d":
			return m.handleDelete()
		case "j", "down":
			var cmd tea.Cmd
			m.eventList, cmd = m.eventList.Update(msg)
			return m, cmd
		case "k", "up":
			var cmd tea.Cmd
			m.eventList, cmd = m.eventList.Update(msg)
			return m, cmd
		case "enter":
			// Could open edit form - skip for now
			return m, nil
		}
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		m.eventList.SetSize(msg.Width-10, listHeight)
		return m, nil
	}

	var cmd tea.Cmd
	m.eventList, cmd = m.eventList.Update(msg)
	return m, cmd
}

func (m Model) handleFormKey(msg tea.KeyMsg) (Model, tea.Cmd) {
	switch msg.String() {
	case "esc", "ctrl+c":
		m.showForm = false
		return m, nil
	case "enter":
		if strings.TrimSpace(m.formTitle) != "" {
			e := &db.Event{
				Title:   strings.TrimSpace(m.formTitle),
				StartAt: time.Date(m.year, m.month, m.day, 9, 0, 0, 0, time.Local),
				EndAt:   time.Date(m.year, m.month, m.day, 10, 0, 0, 0, time.Local),
				AllDay:  false,
			}
			if m.day == 0 {
				now := time.Now()
				e.StartAt = time.Date(now.Year(), now.Month(), now.Day(), 9, 0, 0, 0, time.Local)
				e.EndAt = e.StartAt.Add(time.Hour)
			}
			if err := m.repo.Create(e); err != nil {
				m.err = err.Error()
			} else {
				m.showForm = false
				m.err = ""
			}
			m.refreshEventList()
			return m, m.loadEvents
		}
		return m, nil
	default:
		// Append printable runes
		if msg.Type == tea.KeyRunes && len(msg.Runes) > 0 {
			m.formTitle += string(msg.Runes)
			return m, nil
		}
		if msg.String() == "backspace" && len(m.formTitle) > 0 {
			m.formTitle = m.formTitle[:len(m.formTitle)-1]
			return m, nil
		}
		return m, nil
	}
}

func (m Model) handleDelete() (Model, tea.Cmd) {
	item := m.eventList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ei, ok := item.(eventItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.Delete(ei.event.ID); err != nil {
		m.err = err.Error()
	} else {
		m.err = ""
	}
	m.refreshEventList()
	return m, m.loadEvents
}

func (m *Model) prevMonth() {
	if m.month == 1 {
		m.year--
		m.month = 12
	} else {
		m.month--
	}
	m.day = 0
}

func (m *Model) nextMonth() {
	if m.month == 12 {
		m.year++
		m.month = 1
	} else {
		m.month++
	}
	m.day = 0
}

func (m *Model) refreshEventList() {
	var dayEvents []db.Event
	if m.day > 0 {
		dayEvents, _ = m.repo.ListByDay(m.year, int(m.month), m.day)
	} else {
		dayEvents = m.events
	}
	items := make([]list.Item, len(dayEvents))
	for i := range dayEvents {
		items[i] = eventItem{event: &dayEvents[i]}
	}
	m.eventList.SetItems(items)
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	m.eventList.SetSize(w-10, listHeight)
}

// View renders the calendar UI.
func (m Model) View() string {
	var b strings.Builder

	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}

	if m.showForm {
		b.WriteString(titleStyle.Render(" New Event ") + "\n")
		b.WriteString("Title: " + m.formTitle + "▌\n")
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render("Enter: save  Esc: cancel"))
		return b.String()
	}

	// Month grid
	monthTitle := fmt.Sprintf(" %s %d ", m.month.String(), m.year)
	b.WriteString(titleStyle.Render(monthTitle) + "\n\n")

	grid := m.renderMonthGrid()
	b.WriteString(grid)
	b.WriteString("\n")

	// Events section
	dayLabel := "All events this month"
	if m.day > 0 {
		dayLabel = fmt.Sprintf("Events on %s %d", m.month, m.day)
	}
	b.WriteString(titleStyle.Render(" "+dayLabel+" ") + "\n")
	b.WriteString(m.eventList.View())

	b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(" ←/→ month  t today  n new  d delete  j/k select "))

	return b.String()
}

func (m Model) renderMonthGrid() string {
	first := time.Date(m.year, m.month, 1, 0, 0, 0, 0, time.Local)
	last := first.AddDate(0, 1, -1)
	startWeekday := int(first.Weekday()) // 0=Sun
	daysInMonth := last.Day()

	// Header: Sun Sat ...
	weekdays := []string{"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}
	header := ""
	for _, w := range weekdays {
		header += dayStyle.Render(w) + " "
	}
	header = lipgloss.NewStyle().Foreground(lipgloss.Color("245")).Render(header) + "\n"

	// Pad start
	rows := ""
	col := 0
	for i := 0; i < startWeekday; i++ {
		rows += dayStyle.Render("") + " "
		col++
	}

	now := time.Now()
	for d := 1; d <= daysInMonth; d++ {
		s := fmt.Sprintf("%2d", d)
		cell := dayStyle.Render(s)
		isToday := now.Year() == m.year && now.Month() == m.month && now.Day() == d
		isSelected := m.day == d
		if isToday {
			cell = todayStyle.Render(s)
		} else if isSelected {
			cell = selectedStyle.Render(s)
		}
		rows += cell + " "
		col++
		if col%7 == 0 {
			rows += "\n"
		}
	}

	return header + rows
}
