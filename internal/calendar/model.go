package calendar

import (
	"fmt"
	"io"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/ui"
	"github.com/you/vibe/internal/toast"
)

const (
	sidebarWidth       = 20
	listHeight         = 8
	attendeeCardHeight = 5
	attendeeCardWidth  = 34
	attendeeNotesLen   = 40
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
	titleStyle = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color(ui.ColorAccent))
	gridStyle  = lipgloss.NewStyle().BorderStyle(ui.RetroBorder).BorderForeground(lipgloss.Color(ui.ColorBorder))
	dayStyle   = lipgloss.NewStyle().Width(3).Align(lipgloss.Center)
	selectedStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color(ui.ColorTitleFg)).
			Background(lipgloss.Color(ui.ColorAccent)).
			Padding(0, 1)
	unselectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color(ui.ColorText))
	todayStyle      = lipgloss.NewStyle().
			Foreground(lipgloss.Color(ui.ColorTitleFg)).
			Background(lipgloss.Color(ui.ColorTextDim)).
			Padding(0, 1)
	attendeeCardNameStyle  = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color(ui.ColorTitleFg))
	attendeeCardLabelStyle = lipgloss.NewStyle().Foreground(lipgloss.Color(ui.ColorTextDim))
)

// attendeeItem for contacts list in attendees step (used by EventForm).
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

// truncateShort shortens s to n runes with "…" if truncated.
func truncateShort(s string, n int) string {
	runes := []rune(s)
	if len(runes) <= n {
		return s
	}
	return string(runes[:n-1]) + "…"
}

// truncateNotes shortens s to n runes; if truncated, ends with "...more"
func truncateNotes(s string, n int) string {
	runes := []rune(strings.TrimSpace(s))
	if len(runes) == 0 {
		return ""
	}
	if len(runes) <= n {
		return string(runes)
	}
	return string(runes[:n]) + "...more"
}

type attendeeDelegate struct{}

func (d attendeeDelegate) Height() int                             { return attendeeCardHeight }
func (d attendeeDelegate) Spacing() int                            { return 1 }
func (d attendeeDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d attendeeDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	a, ok := item.(attendeeItem)
	if !ok {
		return
	}
	c := a.contact
	selected := index == m.Index()

	// Card content: Name (with ✓), Email, Phone, Notes (truncated with "...more")
	mark := " "
	if a.selected {
		mark = "✓"
	}
	name := c.Name
	if name == "" {
		name = "(no name)"
	}
	lines := []string{
		attendeeCardNameStyle.Render(mark + " " + name),
	}
	if c.Email != "" {
		lines = append(lines, attendeeCardLabelStyle.Render("  ")+truncateShort(c.Email, attendeeCardWidth-4))
	}
	if c.Phone != "" {
		lines = append(lines, attendeeCardLabelStyle.Render("  ")+truncateShort(c.Phone, attendeeCardWidth-4))
	}
	if c.Notes != "" {
		notes := truncateNotes(c.Notes, attendeeNotesLen)
		lines = append(lines, attendeeCardLabelStyle.Render("  ")+notes)
	}
	content := strings.Join(lines, "\n")

	box := lipgloss.NewStyle().Width(attendeeCardWidth + 4).Padding(0, 1)
	if selected {
		box = box.Border(ui.RetroBorder).BorderForeground(lipgloss.Color(ui.ColorAccent)).Background(lipgloss.Color(ui.ColorSelectBg))
	} else {
		box = box.Border(ui.RetroBorder).BorderForeground(lipgloss.Color(ui.ColorBorder))
	}
	io.WriteString(w, box.Render(content))
}

// Model is the calendar view model.
type Model struct {
	repo           *db.CalendarRepo
	contactsRepo   *db.ContactsRepo
	year           int
	month          time.Month
	day            int
	events         []db.Event
	eventList      list.Model
	width          int
	height         int
	err            string
	showForm       bool
	form           EventForm
	showViewEvent  bool
	viewEvent      *db.Event
	viewAttendees  []string // names for read-only view
}

// NewModel creates a new calendar model.
func NewModel(repo *db.CalendarRepo, contactsRepo *db.ContactsRepo, width, height int) Model {
	items := []list.Item{}
	delegate := eventDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = ""
	l.SetShowStatusBar(false)
	l.SetShowPagination(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	form := NewEventForm(contactsRepo, width)

	now := time.Now()
	m := Model{
		repo:         repo,
		contactsRepo: contactsRepo,
		year:         now.Year(),
		month:        now.Month(),
		day:          now.Day(),
		eventList:    l,
		form:         form,
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
	case FormSaveMsg:
		return m.handleFormSave(msg)
	case tea.KeyMsg:
		if m.showViewEvent {
			if msg.String() == "esc" || msg.String() == "escape" || msg.String() == "enter" {
				m.showViewEvent = false
				m.viewEvent = nil
				m.viewAttendees = nil
				return m, nil
			}
			return m, nil
		}
		if m.showForm {
			var cmd tea.Cmd
			m.form, cmd = m.form.Update(msg)
			// Esc closes form (form.Update returns no special msg for cancel)
			if msg.String() == "esc" || msg.String() == "escape" {
				m.showForm = false
				m.form.Blur()
				return m, nil
			}
			return m, cmd
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
			m.syncCalendarToSelectedEvent()
			return m, cmd
		case "k", "up":
			var cmd tea.Cmd
			m.eventList, cmd = m.eventList.Update(msg)
			m.syncCalendarToSelectedEvent()
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
	}

	var cmd tea.Cmd
	m.eventList, cmd = m.eventList.Update(msg)
	return m, cmd
}

func (m *Model) openForm(e *db.Event) {
	m.showForm = true
	if e != nil {
		m.year = e.StartAt.Year()
		m.month = e.StartAt.Month()
		m.day = e.StartAt.Day()
		attendeeIDs, _ := m.repo.ListAttendees(e.ID)
		m.form.OpenForEdit(e, attendeeIDs)
	} else {
		m.form.OpenForNew(m.year, m.month, m.day)
	}
}

func (m Model) handleFormSave(msg FormSaveMsg) (Model, tea.Cmd) {
	yr, mon, d, dateOk := parseDate(msg.Date)
	if !dateOk {
		m.err = "Invalid date (use YYYY-MM-DD)"
		return m, nil
	}
	startT, ok1 := parseTime(msg.Start)
	endT, ok2 := parseTime(msg.End)
	if !ok1 || !ok2 {
		m.err = "Invalid time (use HH:MM)"
		return m, nil
	}
	startAt := time.Date(yr, mon, d, startT/100, startT%100, 0, 0, time.Local)
	endAt := time.Date(yr, mon, d, endT/100, endT%100, 0, 0, time.Local)
	if msg.AllDay {
		startAt = time.Date(yr, mon, d, 0, 0, 0, 0, time.Local)
		endAt = time.Date(yr, mon, d, 23, 59, 59, 0, time.Local)
	}
	e := &db.Event{
		Title:       msg.Title,
		Description: msg.Description,
		StartAt:     startAt,
		EndAt:       endAt,
		AllDay:      msg.AllDay,
	}
	if msg.EditID != 0 {
		e.ID = msg.EditID
		if err := m.repo.Update(e); err != nil {
			m.err = err.Error()
			return m, nil
		}
		if m.contactsRepo != nil {
			_ = m.repo.SetAttendees(e.ID, msg.AttendeeIDs)
		}
	} else {
		if err := m.repo.Create(e); err != nil {
			m.err = err.Error()
			return m, nil
		}
		if m.contactsRepo != nil && len(msg.AttendeeIDs) > 0 {
			_ = m.repo.SetAttendees(e.ID, msg.AttendeeIDs)
		}
	}
	m.showForm = false
	m.form.Blur()
	m.err = ""
	m.showViewEvent = true
	m.viewEvent = e
	m.viewAttendees = nil
	if m.contactsRepo != nil {
		ids, _ := m.repo.ListAttendees(e.ID)
		for _, cid := range ids {
			if c, err := m.contactsRepo.Get(cid); err == nil && c != nil {
				m.viewAttendees = append(m.viewAttendees, c.Name)
			}
		}
	}
	m.refreshEventList()
	toastMsg := "Event saved"
	if msg.EditID != 0 {
		toastMsg = "Event updated"
	}
	return m, tea.Batch(m.loadEvents, func() tea.Msg { return toast.Msg{Text: toastMsg} })
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
		return m, nil
	}
	m.err = ""
	m.refreshEventList()
	return m, tea.Batch(m.loadEvents, func() tea.Msg { return toast.Msg{Text: "Event deleted"} })
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

// syncCalendarToSelectedEvent updates year/month/day to the selected event's date (vice versa: list → calendar).
func (m *Model) syncCalendarToSelectedEvent() {
	item := m.eventList.SelectedItem()
	if item == nil {
		return
	}
	ei, ok := item.(eventItem)
	if !ok {
		return
	}
	m.year = ei.event.StartAt.Year()
	m.month = ei.event.StartAt.Month()
	m.day = ei.event.StartAt.Day()
	m.refreshEventList()
	// Keep selection on the same event in the refreshed list
	items := m.eventList.Items()
	for i := range items {
		if eit, ok := items[i].(eventItem); ok && eit.event.ID == ei.event.ID {
			m.eventList.Select(i)
			break
		}
	}
}

// Count returns the number of events in the current month (for sidebar).
func (m Model) Count() int {
	return len(m.events)
}

// SetSize updates width/height. Single source of truth for layout (app passes main pane size).
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	listH := h - 6
	if listH < 4 {
		listH = 4
	}
	m.eventList.SetSize(w-10, listH)
	m.form.SetSize(w, h)
}

// StatusHint returns context-specific key bindings for the status bar.
func (m Model) StatusHint() string {
	if m.showViewEvent {
		return "Enter/Esc close"
	}
	if m.showForm {
		return m.form.StatusHint()
	}
	return "←/→ month  ,/. day  a all  t today  F5 new  F6 edit  F7 del"
}

// View renders the calendar UI.
func (m Model) View() string {
	var b strings.Builder

	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}

	if m.showViewEvent && m.viewEvent != nil {
		e := m.viewEvent
		b.WriteString(titleStyle.Render(" Event ") + "\n\n")
		b.WriteString("Title: " + e.Title + "\n")
		b.WriteString("Date:  " + e.StartAt.Format("Mon Jan 2, 2006") + "\n")
		if e.AllDay {
			b.WriteString("All day: yes\n")
		} else {
			b.WriteString("Start: " + e.StartAt.Format("15:04") + "  End: " + e.EndAt.Format("15:04") + "\n")
		}
		b.WriteString("Notes:\n")
		if e.Description != "" {
			b.WriteString(e.Description + "\n")
		} else {
			b.WriteString("(none)\n")
		}
		if len(m.viewAttendees) > 0 {
			b.WriteString("\nAttendees: " + strings.Join(m.viewAttendees, ", ") + "\n")
		}
		b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color(ui.ColorTextDim)).Render("Enter or Esc to close") + "\n")
		return b.String()
	}

	if m.showForm {
		if m.form.Err != "" {
			b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.form.Err) + "\n")
		}
		b.WriteString(m.form.View())
		return b.String()
	}

	// Month grid
	monthTitle := fmt.Sprintf(" %s %d ", m.month.String(), m.year)
	b.WriteString(titleStyle.Render(monthTitle) + "\n\n")

	b.WriteString(RenderMonthGrid(m.year, m.month, m.day) + "\n")

	// Events section
	dayLabel := "All events this month"
	if m.day > 0 {
		dayLabel = fmt.Sprintf("Events on %s %d", m.month, m.day)
	}
	b.WriteString(titleStyle.Render(" "+dayLabel+" ") + "\n")
	if len(m.eventList.Items()) == 0 {
		b.WriteString(ui.EmptyStateView("No events.") + "\n")
	} else {
		b.WriteString(m.eventList.View())
	}

	return b.String()
}
