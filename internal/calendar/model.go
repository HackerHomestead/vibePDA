package calendar

import (
	"fmt"
	"io"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textarea"
	"github.com/charmbracelet/bubbles/textinput"
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

// Form steps: 0=title, 1=start, 2=end, 3=notes, 4=attendees
const formSteps = 5

// attendeeItem for contacts list in attendees step.
type attendeeItem struct {
	contact  *db.Contact
	selected bool
}

func (a attendeeItem) Title() string {
	mark := " "
	if a.selected {
		mark = "✓"
	}
	return fmt.Sprintf("%s %s", mark, a.contact.Name)
}
func (a attendeeItem) Description() string { return a.contact.Email }
func (a attendeeItem) FilterValue() string { return a.contact.Name }

type attendeeDelegate struct{}

func (d attendeeDelegate) Height() int                             { return 1 }
func (d attendeeDelegate) Spacing() int                            { return 0 }
func (d attendeeDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d attendeeDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	a, ok := item.(attendeeItem)
	if !ok {
		return
	}
	str := a.Title()
	if index == m.Index() {
		str = selectedStyle.Render("▶ " + str)
	} else {
		str = unselectedStyle.Render("  " + str)
	}
	io.WriteString(w, str)
}

// Model is the calendar view model.
type Model struct {
	repo          *db.CalendarRepo
	contactsRepo  *db.ContactsRepo
	year          int
	month         time.Month
	day           int
	events        []db.Event
	contacts      []db.Contact
	eventList     list.Model
	attendeeList  list.Model
	width         int
	height        int
	err           string
	showForm      bool
	formStep      int
	formTitle     string
	formStart     string
	formEnd       string
	formNotes     string
	formAttendees []int64
	formEditID    int64
	formInput     textinput.Model
	formTextarea  textarea.Model
}

// NewModel creates a new calendar model.
func NewModel(repo *db.CalendarRepo, contactsRepo *db.ContactsRepo, width, height int) Model {
	items := []list.Item{}
	delegate := eventDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = ""
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	ti := textinput.New()
	ti.Placeholder = "Meeting with team"
	ti.Width = 40

	ta := textarea.New()
	ta.Placeholder = "Notes (optional)"
	ta.SetWidth(50)
	ta.SetHeight(4)

	al := list.New([]list.Item{}, attendeeDelegate{}, width-4, 6)
	al.SetShowStatusBar(false)
	al.SetFilteringEnabled(false)
	al.SetShowHelp(false)
	al.DisableQuitKeybindings()

	now := time.Now()
	m := Model{
		repo:         repo,
		contactsRepo: contactsRepo,
		year:         now.Year(),
		month:        now.Month(),
		day:          now.Day(),
		eventList:    l,
		attendeeList: al,
		formInput:    ti,
		formTextarea: ta,
		width:        width,
		height:       height,
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
		case ",":
			m.prevDay()
			m.refreshEventList()
			return m, m.loadEvents
		case ".":
			m.nextDay()
			m.refreshEventList()
			return m, m.loadEvents
		case "a":
			m.day = 0
			m.refreshEventList()
			return m, m.loadEvents
		case "n":
			m.openForm(nil)
			return m, textinput.Blink
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
			// Open edit form for selected event
			item := m.eventList.SelectedItem()
			if ei, ok := item.(eventItem); ok {
				m.openForm(ei.event)
				return m, textinput.Blink
			}
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

func (m *Model) openForm(e *db.Event) {
	m.showForm = true
	m.formEditID = 0
	m.formStep = 0
	m.formNotes = ""
	m.formAttendees = nil
	if e != nil {
		m.formEditID = e.ID
		m.formTitle = e.Title
		m.formStart = e.StartAt.Format("15:04")
		m.formEnd = e.EndAt.Format("15:04")
		m.formNotes = e.Description
		m.year = e.StartAt.Year()
		m.month = e.StartAt.Month()
		m.day = e.StartAt.Day()
		if m.contactsRepo != nil {
			m.formAttendees, _ = m.repo.ListAttendees(e.ID)
		}
	} else {
		m.formTitle = ""
		m.formStart = "09:00"
		m.formEnd = "10:00"
		if m.day == 0 {
			now := time.Now()
			m.year = now.Year()
			m.month = now.Month()
			m.day = now.Day()
		}
	}
	m.formInput.SetValue(m.formTitle)
	m.formInput.Placeholder = "Meeting with team"
	m.formInput.Focus()
	m.formTextarea.SetValue(m.formNotes)
	m.formTextarea.Blur()
	m.refreshAttendeeList()
}

func (m Model) handleFormKey(msg tea.KeyMsg) (Model, tea.Cmd) {
	s := msg.String()

	// F2: save from any step (avoids Ctrl key issues in terminals)
	if s == "f2" {
		return m.formSave()
	}

	// Esc: cancel
	if s == "esc" || s == "escape" {
		m.showForm = false
		m.formInput.Blur()
		m.formTextarea.Blur()
		return m, nil
	}

	// In notes field (step 3): Tab and Enter pass through to textarea
	if m.formStep == 3 {
		var cmd tea.Cmd
		m.formTextarea, cmd = m.formTextarea.Update(msg)
		return m, cmd
	}

	// Tab: in notes (step 3) pass through; else advance. Ctrl+Tab: notes -> attendees.
	switch s {
	case "tab":
		if m.formStep == 3 {
			var cmd tea.Cmd
			m.formTextarea, cmd = m.formTextarea.Update(msg)
			return m, cmd
		}
		return m.formTabForward()
	case "ctrl+tab":
		if m.formStep == 3 && m.contactsRepo != nil {
			m.formNotes = strings.TrimSpace(m.formTextarea.Value())
			m.formStep = 4
			m.formTextarea.Blur()
			m.refreshAttendeeList()
			return m, nil
		}
		return m.formTabForward()
	case "shift+tab":
		return m.formTabBack()
	case "enter":
		return m.formAdvance()
	}

	// Step 4: attendees - space toggles, Enter/Tab saves
	if m.formStep == 4 {
		switch s {
		case " ":
			m.toggleAttendee()
			return m, nil
		case "j", "down":
			var cmd tea.Cmd
			m.attendeeList, cmd = m.attendeeList.Update(msg)
			return m, cmd
		case "k", "up":
			var cmd tea.Cmd
			m.attendeeList, cmd = m.attendeeList.Update(msg)
			return m, cmd
		}
		return m, nil
	}

	// Route to textinput for steps 0, 1, 2
	var cmd tea.Cmd
	m.formInput, cmd = m.formInput.Update(msg)
	return m, cmd
}

func (m Model) formTabForward() (Model, tea.Cmd) {
	if m.formStep == formSteps-1 {
		return m.formSave()
	}
	return m.formAdvance()
}

func (m Model) formTabBack() (Model, tea.Cmd) {
	if m.formStep == 0 {
		m.showForm = false
		m.formInput.Blur()
		m.formTextarea.Blur()
		return m, nil
	}
	m.formStep--
	m.focusFormField()
	return m, nil
}

func (m Model) formAdvance() (Model, tea.Cmd) {
	val := strings.TrimSpace(m.formInput.Value())
	switch m.formStep {
	case 0:
		if val != "" {
			m.formTitle = val
			m.formStep = 1
			m.formInput.SetValue(m.formStart)
			m.formInput.Placeholder = "09:00"
			return m, nil
		}
	case 1:
		if _, ok := parseTime(val); ok {
			m.formStart = val
			m.formStep = 2
			m.formInput.SetValue(m.formEnd)
			m.formInput.Placeholder = "10:00"
			return m, nil
		}
		m.err = "Invalid time (use HH:MM)"
	case 2:
		if _, ok := parseTime(val); ok {
			m.formEnd = val
			m.formStep = 3
			m.formTextarea.SetValue(m.formNotes)
			m.formTextarea.Focus()
			m.formInput.Blur()
			return m, textarea.Blink
		}
		m.err = "Invalid time (use HH:MM)"
	case 3:
		m.formNotes = strings.TrimSpace(m.formTextarea.Value())
		if m.contactsRepo != nil {
			m.formStep = 4
			m.formTextarea.Blur()
			m.refreshAttendeeList()
		} else {
			return m.formSave()
		}
		// Tab from step 3 goes to textarea; Ctrl+Tab advances to step 4
	}
	return m, nil
}

func (m *Model) refreshAttendeeList() {
	if m.contactsRepo == nil {
		return
	}
	contacts, err := m.contactsRepo.List()
	if err != nil {
		return
	}
	m.contacts = contacts
	selected := make(map[int64]bool)
	for _, id := range m.formAttendees {
		selected[id] = true
	}
	items := make([]list.Item, len(contacts))
	for i := range contacts {
		items[i] = attendeeItem{contact: &contacts[i], selected: selected[contacts[i].ID]}
	}
	m.attendeeList.SetItems(items)
}

func (m *Model) toggleAttendee() {
	item := m.attendeeList.SelectedItem()
	if item == nil {
		return
	}
	a, ok := item.(attendeeItem)
	if !ok {
		return
	}
	id := a.contact.ID
	found := false
	for i, fid := range m.formAttendees {
		if fid == id {
			m.formAttendees = append(m.formAttendees[:i], m.formAttendees[i+1:]...)
			found = true
			break
		}
	}
	if !found {
		m.formAttendees = append(m.formAttendees, id)
	}
	m.refreshAttendeeList()
}

func (m *Model) focusFormField() {
	m.formInput.Blur()
	m.formTextarea.Blur()
	switch m.formStep {
	case 4:
		// Attendees list - no focus change needed
		return
	case 0:
		m.formInput.SetValue(m.formTitle)
		m.formInput.Placeholder = "Meeting with team"
		m.formInput.Focus()
	case 1:
		m.formInput.SetValue(m.formStart)
		m.formInput.Placeholder = "09:00"
		m.formInput.Focus()
	case 2:
		m.formInput.SetValue(m.formEnd)
		m.formInput.Placeholder = "10:00"
		m.formInput.Focus()
	case 3:
		m.formTextarea.SetValue(m.formNotes)
		m.formTextarea.Focus()
	}
}

func (m Model) formSave() (Model, tea.Cmd) {
	m.formNotes = strings.TrimSpace(m.formTextarea.Value())
	startT, ok1 := parseTime(strings.TrimSpace(m.formStart))
	endT, ok2 := parseTime(strings.TrimSpace(m.formEnd))
	if !ok1 || !ok2 {
		m.err = "Invalid time (use HH:MM)"
		return m, nil
	}
	yr, mon, d := m.year, m.month, m.day
	e := &db.Event{
		Title:       m.formTitle,
		Description: m.formNotes,
		StartAt:     time.Date(yr, mon, d, startT/100, startT%100, 0, 0, time.Local),
		EndAt:       time.Date(yr, mon, d, endT/100, endT%100, 0, 0, time.Local),
		AllDay:      false,
	}
	if m.formEditID != 0 {
		e.ID = m.formEditID
		if err := m.repo.Update(e); err != nil {
			m.err = err.Error()
			return m, nil
		}
		if m.contactsRepo != nil {
			_ = m.repo.SetAttendees(e.ID, m.formAttendees)
		}
	} else {
		if err := m.repo.Create(e); err != nil {
			m.err = err.Error()
			return m, nil
		}
		if m.contactsRepo != nil && len(m.formAttendees) > 0 {
			_ = m.repo.SetAttendees(e.ID, m.formAttendees)
		}
	}
	m.showForm = false
	m.formInput.Blur()
	m.formTextarea.Blur()
	m.err = ""
	m.refreshEventList()
	return m, m.loadEvents
}

// parseTime parses HH:MM or HHMM, returns hour*100+min and ok.
func parseTime(s string) (int, bool) {
	// Try HH:MM
	re := regexp.MustCompile(`^(\d{1,2}):(\d{2})$`)
	if m := re.FindStringSubmatch(s); len(m) == 3 {
		h, _ := strconv.Atoi(m[1])
		min, _ := strconv.Atoi(m[2])
		if h >= 0 && h <= 23 && min >= 0 && min <= 59 {
			return h*100 + min, true
		}
	}
	// Try HHMM
	if len(s) == 4 {
		h, eh := strconv.Atoi(s[:2])
		min, em := strconv.Atoi(s[2:])
		if eh == nil && em == nil && h >= 0 && h <= 23 && min >= 0 && min <= 59 {
			return h*100 + min, true
		}
	}
	return 0, false
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

func (m *Model) prevDay() {
	last := time.Date(m.year, m.month+1, 0, 0, 0, 0, 0, time.Local).Day()
	if m.day <= 0 {
		m.day = last
		return
	}
	m.day--
	if m.day < 1 {
		m.prevMonth()
		m.day = time.Date(m.year, m.month+1, 0, 0, 0, 0, 0, time.Local).Day()
	}
}

func (m *Model) nextDay() {
	last := time.Date(m.year, m.month+1, 0, 0, 0, 0, 0, time.Local).Day()
	if m.day <= 0 {
		m.day = 1
		return
	}
	m.day++
	if m.day > last {
		m.nextMonth()
		m.day = 1
	}
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
		formTitle := " New Event "
		if m.formEditID != 0 {
			formTitle = " Edit Event "
		}
		b.WriteString(titleStyle.Render(formTitle) + "\n\n")
		b.WriteString("Title: ")
		if m.formStep == 0 {
			b.WriteString(m.formInput.View() + "\n")
		} else {
			b.WriteString(m.formTitle + "\n")
		}
		b.WriteString("Start (HH:MM): ")
		if m.formStep == 1 {
			b.WriteString(m.formInput.View() + "\n")
		} else {
			b.WriteString(m.formStart + "\n")
		}
		b.WriteString("End (HH:MM): ")
		if m.formStep == 2 {
			b.WriteString(m.formInput.View() + "\n")
		} else {
			b.WriteString(m.formEnd + "\n")
		}
		b.WriteString("Notes:\n")
		if m.formStep == 3 {
			b.WriteString(m.formTextarea.View() + "\n")
		} else {
			b.WriteString(m.formNotes)
			if m.formNotes == "" {
				b.WriteString("(empty)")
			}
			b.WriteString("\n")
		}
		if m.formStep == 4 && m.contactsRepo != nil {
			b.WriteString("Attendees (space to toggle):\n")
			b.WriteString(m.attendeeList.View())
		}
		help := "Enter: next  F2: save  Esc: cancel"
		if m.formStep == 3 && m.contactsRepo != nil {
			help = "Tab: indent  Ctrl+Tab: attendees  F2: save  Esc: cancel"
		} else if m.formStep == 4 {
			help = "Space: toggle  F2: save  Esc: cancel"
		}
		b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(help))
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

	b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(" ←/→ month  ,/. prev/next day  a all  t today  n new  d delete  j/k select "))

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
