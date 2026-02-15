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

// Form steps: 0=title, 1=date, 2=start, 3=end, 4=all-day, 5=notes, 6=attendees
const formSteps = 7

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
	err             string
	showForm        bool
	showViewEvent   bool
	viewEvent       *db.Event
	viewAttendees   []string // names for read-only view
	formStep        int
	formTitle     string
	formDate      string // YYYY-MM-DD
	formStart     string
	formEnd       string
	formAllDay    bool
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
	l.SetShowPagination(false)
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

	al := list.New([]list.Item{}, attendeeDelegate{}, width-4, 14)
	al.SetShowStatusBar(false)
	al.SetShowPagination(false)
	al.SetFilteringEnabled(true) // Type to filter/search contacts
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
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		listH := msg.Height - 6
		if listH < 4 {
			listH = 4
		}
		m.eventList.SetSize(msg.Width-10, listH)
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
		m.formDate = e.StartAt.Format("2006-01-02")
		m.formStart = e.StartAt.Format("15:04")
		m.formEnd = e.EndAt.Format("15:04")
		m.formAllDay = e.AllDay
		m.formNotes = e.Description
		m.year = e.StartAt.Year()
		m.month = e.StartAt.Month()
		m.day = e.StartAt.Day()
		if m.contactsRepo != nil {
			m.formAttendees, _ = m.repo.ListAttendees(e.ID)
		}
	} else {
		m.formTitle = ""
		if m.day == 0 {
			now := time.Now()
			m.year = now.Year()
			m.month = now.Month()
			m.day = now.Day()
		}
		m.formDate = time.Date(m.year, m.month, m.day, 0, 0, 0, 0, time.Local).Format("2006-01-02")
		m.formStart = "09:00"
		m.formEnd = "10:00"
		m.formAllDay = false
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

	// Step 4: all-day - space toggles, Enter advances to notes
	if m.formStep == 4 {
		if s == " " {
			m.formAllDay = !m.formAllDay
			return m, nil
		}
		if s == "enter" {
			return m.formAdvance()
		}
	}

	// In notes field (step 5): Tab passes through; double Enter saves
	if m.formStep == 5 {
		if s == "enter" {
			val := m.formTextarea.Value()
			if strings.HasSuffix(val, "\n") {
				// Double Enter (empty line) = save
				return m.formSave()
			}
		}
		var cmd tea.Cmd
		m.formTextarea, cmd = m.formTextarea.Update(msg)
		return m, cmd
	}

	// Tab: in notes (step 5) pass through; else advance. Ctrl+Tab: notes -> attendees.
	switch s {
	case "tab":
		if m.formStep == 5 {
			var cmd tea.Cmd
			m.formTextarea, cmd = m.formTextarea.Update(msg)
			return m, cmd
		}
		return m.formTabForward()
	case "ctrl+tab":
		if m.formStep == 5 && m.contactsRepo != nil {
			m.formNotes = strings.TrimSpace(m.formTextarea.Value())
			m.formStep = 6
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

	// Step 6: attendees - space toggle, Enter/Tab save, type to filter, j/k navigate
	if m.formStep == 6 {
		switch s {
		case " ":
			m.toggleAttendee()
			return m, nil
		case "enter", "tab":
			return m.formSave()
		case "j", "down", "k", "up":
			var cmd tea.Cmd
			m.attendeeList, cmd = m.attendeeList.Update(msg)
			return m, cmd
		}
		// Pass typing keys to list for filtering (autocomplete/search contacts)
		var cmd tea.Cmd
		m.attendeeList, cmd = m.attendeeList.Update(msg)
		return m, cmd
	}

	// Route to textinput for steps 0, 1, 2, 3 (title, date, start, end)
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
			m.formInput.SetValue(m.formDate)
			m.formInput.Placeholder = "2006-01-02"
			return m, nil
		}
	case 1:
		if yr, mon, d, ok := parseDate(val); ok {
			m.formDate = val
			m.year, m.month, m.day = yr, mon, d
			m.formStep = 2
			m.formInput.SetValue(m.formStart)
			m.formInput.Placeholder = "09:00"
			return m, nil
		}
		m.err = "Invalid date (use YYYY-MM-DD)"
	case 2:
		if _, ok := parseTime(val); ok {
			m.formStart = val
			m.formStep = 3
			m.formInput.SetValue(m.formEnd)
			m.formInput.Placeholder = "10:00"
			return m, nil
		}
		m.err = "Invalid time (use HH:MM)"
	case 3:
		if _, ok := parseTime(val); ok {
			m.formEnd = val
			m.formStep = 4
			// Step 4 is all-day (no input field); Enter will advance from handleFormKey
			return m, nil
		}
		m.err = "Invalid time (use HH:MM)"
	case 4:
		// All-day step: Enter advances to notes
		m.formStep = 5
		m.formTextarea.SetValue(m.formNotes)
		m.formTextarea.Focus()
		m.formInput.Blur()
		return m, textarea.Blink
	case 5:
		m.formNotes = strings.TrimSpace(m.formTextarea.Value())
		if m.contactsRepo != nil {
			m.formStep = 6
			m.formTextarea.Blur()
			m.refreshAttendeeList()
		} else {
			return m.formSave()
		}
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
	case 6:
		// Attendees list - no focus change needed
		return
	case 4:
		// All-day step - no input field
		return
	case 0:
		m.formInput.SetValue(m.formTitle)
		m.formInput.Placeholder = "Meeting with team"
		m.formInput.Focus()
	case 1:
		m.formInput.SetValue(m.formDate)
		m.formInput.Placeholder = "2006-01-02"
		m.formInput.Focus()
	case 2:
		m.formInput.SetValue(m.formStart)
		m.formInput.Placeholder = "09:00"
		m.formInput.Focus()
	case 3:
		m.formInput.SetValue(m.formEnd)
		m.formInput.Placeholder = "10:00"
		m.formInput.Focus()
	case 5:
		m.formTextarea.SetValue(m.formNotes)
		m.formTextarea.Focus()
	}
}

func (m Model) formSave() (Model, tea.Cmd) {
	m.formNotes = strings.TrimSpace(m.formTextarea.Value())
	yr, mon, d, dateOk := parseDate(strings.TrimSpace(m.formDate))
	if !dateOk {
		m.err = "Invalid date (use YYYY-MM-DD)"
		return m, nil
	}
	startT, ok1 := parseTime(strings.TrimSpace(m.formStart))
	endT, ok2 := parseTime(strings.TrimSpace(m.formEnd))
	if !ok1 || !ok2 {
		m.err = "Invalid time (use HH:MM)"
		return m, nil
	}
	startAt := time.Date(yr, mon, d, startT/100, startT%100, 0, 0, time.Local)
	endAt := time.Date(yr, mon, d, endT/100, endT%100, 0, 0, time.Local)
	if m.formAllDay {
		startAt = time.Date(yr, mon, d, 0, 0, 0, 0, time.Local)
		endAt = time.Date(yr, mon, d, 23, 59, 59, 0, time.Local)
	}
	e := &db.Event{
		Title:       m.formTitle,
		Description: m.formNotes,
		StartAt:     startAt,
		EndAt:       endAt,
		AllDay:      m.formAllDay,
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
	msg := "Event saved"
	if m.formEditID != 0 {
		msg = "Event updated"
	}
	return m, tea.Batch(m.loadEvents, func() tea.Msg { return toast.Msg{Text: msg} })
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

// parseDate parses YYYY-MM-DD, returns year, month, day and ok.
func parseDate(s string) (year int, month time.Month, day int, ok bool) {
	t, err := time.Parse("2006-01-02", s)
	if err != nil {
		return 0, 0, 0, false
	}
	return t.Year(), t.Month(), t.Day(), true
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

// Count returns the number of events in the event list.
func (m Model) Count() int {
	return len(m.eventList.Items())
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	listH := h - 6
	if listH < 4 {
		listH = 4
	}
	m.eventList.SetSize(w-10, listH)
}

// StatusHint returns context-specific key bindings for the status bar.
func (m Model) StatusHint() string {
	if m.showViewEvent {
		return "Enter/Esc close"
	}
	if m.showForm {
		switch m.formStep {
		case 0:
			return "Enter next  F10 save  F11 cancel"
		case 1, 2, 3:
			return "Enter next  F10 save  F11 cancel"
		case 4:
			return "Space toggle all-day  Enter next  F10 save  F11 cancel"
		case 5:
			if m.contactsRepo != nil {
				return "Tab indent  Ctrl+Tab attendees  Enter² save  F10 save  F11 cancel"
			}
			return "Enter² save  F10 save  F11 cancel"
		case 6:
			return "Type filter  Space toggle  Enter save  F10 save  F11 cancel"
		}
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
		b.WriteString("Date (YYYY-MM-DD): ")
		if m.formStep == 1 {
			b.WriteString(m.formInput.View() + "\n")
		} else {
			b.WriteString(m.formDate + "\n")
		}
		b.WriteString("Start (HH:MM): ")
		if m.formStep == 2 {
			b.WriteString(m.formInput.View() + "\n")
		} else {
			b.WriteString(m.formStart + "\n")
		}
		b.WriteString("End (HH:MM): ")
		if m.formStep == 3 {
			b.WriteString(m.formInput.View() + "\n")
		} else {
			b.WriteString(m.formEnd + "\n")
		}
		allDayMark := " "
		if m.formAllDay {
			allDayMark = "x"
		}
		b.WriteString("All day: [" + allDayMark + "] (space to toggle)")
		if m.formStep == 4 {
			b.WriteString("  ←")
		}
		b.WriteString("\n")
		b.WriteString("Notes:\n")
		if m.formStep == 5 {
			b.WriteString(m.formTextarea.View() + "\n")
		} else {
			b.WriteString(m.formNotes)
			if m.formNotes == "" {
				b.WriteString("(empty)")
			}
			b.WriteString("\n")
		}
		if m.formStep == 6 && m.contactsRepo != nil {
			b.WriteString("\nAttendees (type to filter, space to toggle):\n")
			b.WriteString(m.attendeeList.View())
		}
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
