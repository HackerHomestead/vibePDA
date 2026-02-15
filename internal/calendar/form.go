package calendar

import (
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textarea"
	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/you/vibe/internal/db"
)

const formSteps = 7

// FormSaveMsg is sent when the user saves the event form. Calendar handles it and performs repo Create/Update.
type FormSaveMsg struct {
	Title       string
	Description string
	Date        string // YYYY-MM-DD
	Start       string
	End         string
	AllDay      bool
	EditID      int64
	AttendeeIDs []int64
}

// EventForm is the add/edit event form (multi-step: title, date, start, end, all-day, notes, attendees).
type EventForm struct {
	Step         int
	Title        string
	Date         string
	Start        string
	End          string
	AllDay       bool
	Notes        string
	AttendeeIDs  []int64
	EditID       int64
	Input        textinput.Model
	Textarea     textarea.Model
	AttendeeList list.Model
	contactsRepo *db.ContactsRepo
	contacts     []db.Contact
	Err          string
}

// NewEventForm creates a new event form. Width is used for list sizing.
func NewEventForm(contactsRepo *db.ContactsRepo, width int) EventForm {
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
	al.SetFilteringEnabled(true)
	al.SetShowHelp(false)
	al.DisableQuitKeybindings()

	return EventForm{
		Input:        ti,
		Textarea:     ta,
		AttendeeList: al,
		contactsRepo: contactsRepo,
	}
}

// OpenForNew initializes the form for a new event on the given date.
func (f *EventForm) OpenForNew(year int, month time.Month, day int) {
	f.Step = 0
	f.EditID = 0
	f.Title = ""
	f.Notes = ""
	f.AttendeeIDs = nil
	if day == 0 {
		now := time.Now()
		year, month, day = now.Year(), now.Month(), now.Day()
	}
	f.Date = time.Date(year, month, day, 0, 0, 0, 0, time.Local).Format("2006-01-02")
	f.Start = "09:00"
	f.End = "10:00"
	f.AllDay = false
	f.Input.SetValue(f.Title)
	f.Input.Placeholder = "Meeting with team"
	f.Input.Focus()
	f.Textarea.SetValue(f.Notes)
	f.Textarea.Blur()
	f.refreshAttendeeList()
}

// OpenForEdit initializes the form for editing an existing event.
func (f *EventForm) OpenForEdit(e *db.Event, attendeeIDs []int64) {
	f.Step = 0
	f.EditID = e.ID
	f.Title = e.Title
	f.Date = e.StartAt.Format("2006-01-02")
	f.Start = e.StartAt.Format("15:04")
	f.End = e.EndAt.Format("15:04")
	f.AllDay = e.AllDay
	f.Notes = e.Description
	f.AttendeeIDs = attendeeIDs
	f.Input.SetValue(f.Title)
	f.Input.Placeholder = "Meeting with team"
	f.Input.Focus()
	f.Textarea.SetValue(f.Notes)
	f.Textarea.Blur()
	f.refreshAttendeeList()
}

// Update handles key and other messages. Returns a Cmd that may produce FormSaveMsg on save.
func (f EventForm) Update(msg tea.Msg) (EventForm, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		s := msg.String()

		if s == "f2" {
			return f.saveRequest()
		}
		if s == "esc" || s == "escape" {
			return f, nil // caller closes form
		}

		if f.Step == 4 {
			if s == " " {
				f.AllDay = !f.AllDay
				return f, nil
			}
			if s == "enter" {
				return f.advance()
			}
		}

		if f.Step == 5 {
			if s == "enter" {
				if strings.HasSuffix(f.Textarea.Value(), "\n") {
					return f.saveRequest()
				}
			}
			var cmd tea.Cmd
			f.Textarea, cmd = f.Textarea.Update(msg)
			return f, cmd
		}

		switch s {
		case "tab":
			if f.Step == 5 {
				var cmd tea.Cmd
				f.Textarea, cmd = f.Textarea.Update(msg)
				return f, cmd
			}
			return f.tabForward()
		case "ctrl+tab":
			if f.Step == 5 && f.contactsRepo != nil {
				f.Notes = strings.TrimSpace(f.Textarea.Value())
				f.Step = 6
				f.Textarea.Blur()
				f.refreshAttendeeList()
				return f, nil
			}
			return f.tabForward()
		case "shift+tab":
			return f.tabBack()
		case "enter":
			return f.advance()
		}

		if f.Step == 6 {
			switch s {
			case " ":
				f.toggleAttendee()
				return f, nil
			case "enter", "tab":
				return f.saveRequest()
			case "j", "down", "k", "up":
				var cmd tea.Cmd
				f.AttendeeList, cmd = f.AttendeeList.Update(msg)
				return f, cmd
			}
			var cmd tea.Cmd
			f.AttendeeList, cmd = f.AttendeeList.Update(msg)
			return f, cmd
		}

		var cmd tea.Cmd
		f.Input, cmd = f.Input.Update(msg)
		return f, cmd
	}
	return f, nil
}

func (f EventForm) tabForward() (EventForm, tea.Cmd) {
	if f.Step == formSteps-1 {
		return f.saveRequest()
	}
	return f.advance()
}

func (f EventForm) tabBack() (EventForm, tea.Cmd) {
	if f.Step == 0 {
		return f, nil // caller closes form
	}
	f.Step--
	f.focusField()
	return f, nil
}

func (f *EventForm) focusField() {
	f.Input.Blur()
	f.Textarea.Blur()
	switch f.Step {
	case 6, 4:
		return
	case 0:
		f.Input.SetValue(f.Title)
		f.Input.Placeholder = "Meeting with team"
		f.Input.Focus()
	case 1:
		f.Input.SetValue(f.Date)
		f.Input.Placeholder = "2006-01-02"
		f.Input.Focus()
	case 2:
		f.Input.SetValue(f.Start)
		f.Input.Placeholder = "09:00"
		f.Input.Focus()
	case 3:
		f.Input.SetValue(f.End)
		f.Input.Placeholder = "10:00"
		f.Input.Focus()
	case 5:
		f.Textarea.SetValue(f.Notes)
		f.Textarea.Focus()
	}
}

func (f EventForm) advance() (EventForm, tea.Cmd) {
	val := strings.TrimSpace(f.Input.Value())
	switch f.Step {
	case 0:
		if val != "" {
			f.Title = val
			f.Step = 1
			f.Input.SetValue(f.Date)
			f.Input.Placeholder = "2006-01-02"
			return f, nil
		}
	case 1:
		if _, _, _, ok := parseDate(val); ok {
			f.Date = val
			f.Step = 2
			f.Input.SetValue(f.Start)
			f.Input.Placeholder = "09:00"
			return f, nil
		}
		f.Err = "Invalid date (use YYYY-MM-DD)"
	case 2:
		if _, ok := parseTime(val); ok {
			f.Start = val
			f.Step = 3
			f.Input.SetValue(f.End)
			f.Input.Placeholder = "10:00"
			return f, nil
		}
		f.Err = "Invalid time (use HH:MM)"
	case 3:
		if _, ok := parseTime(val); ok {
			f.End = val
			f.Step = 4
			return f, nil
		}
		f.Err = "Invalid time (use HH:MM)"
	case 4:
		f.Step = 5
		f.Textarea.SetValue(f.Notes)
		f.Textarea.Focus()
		f.Input.Blur()
		return f, textarea.Blink
	case 5:
		f.Notes = strings.TrimSpace(f.Textarea.Value())
		if f.contactsRepo != nil {
			f.Step = 6
			f.Textarea.Blur()
			f.refreshAttendeeList()
		} else {
			return f.saveRequest()
		}
	}
	return f, nil
}

func (f EventForm) saveRequest() (EventForm, tea.Cmd) {
	f.Err = ""
	f.Notes = strings.TrimSpace(f.Textarea.Value())
	if _, _, _, ok := parseDate(strings.TrimSpace(f.Date)); !ok {
		f.Err = "Invalid date (use YYYY-MM-DD)"
		return f, nil
	}
	if _, ok := parseTime(strings.TrimSpace(f.Start)); !ok {
		f.Err = "Invalid time (use HH:MM)"
		return f, nil
	}
	if _, ok := parseTime(strings.TrimSpace(f.End)); !ok {
		f.Err = "Invalid time (use HH:MM)"
		return f, nil
	}
	payload := FormSaveMsg{
		Title:       f.Title,
		Description: f.Notes,
		Date:        strings.TrimSpace(f.Date),
		Start:       strings.TrimSpace(f.Start),
		End:         strings.TrimSpace(f.End),
		AllDay:      f.AllDay,
		EditID:      f.EditID,
		AttendeeIDs: append([]int64(nil), f.AttendeeIDs...),
	}
	return f, func() tea.Msg { return payload }
}

func (f *EventForm) refreshAttendeeList() {
	if f.contactsRepo == nil {
		return
	}
	contacts, err := f.contactsRepo.List()
	if err != nil {
		return
	}
	f.contacts = contacts
	selected := make(map[int64]bool)
	for _, id := range f.AttendeeIDs {
		selected[id] = true
	}
	items := make([]list.Item, len(contacts))
	for i := range contacts {
		items[i] = attendeeItem{contact: &contacts[i], selected: selected[contacts[i].ID]}
	}
	f.AttendeeList.SetItems(items)
}

func (f *EventForm) toggleAttendee() {
	item := f.AttendeeList.SelectedItem()
	if item == nil {
		return
	}
	a, ok := item.(attendeeItem)
	if !ok {
		return
	}
	id := a.contact.ID
	for i, fid := range f.AttendeeIDs {
		if fid == id {
			f.AttendeeIDs = append(f.AttendeeIDs[:i], f.AttendeeIDs[i+1:]...)
			f.refreshAttendeeList()
			return
		}
	}
	f.AttendeeIDs = append(f.AttendeeIDs, id)
	f.refreshAttendeeList()
}

// View renders the form UI.
func (f EventForm) View() string {
	var b strings.Builder
	formTitle := " New Event "
	if f.EditID != 0 {
		formTitle = " Edit Event "
	}
	b.WriteString(titleStyle.Render(formTitle) + "\n\n")
	b.WriteString("Title: ")
	if f.Step == 0 {
		b.WriteString(f.Input.View() + "\n")
	} else {
		b.WriteString(f.Title + "\n")
	}
	b.WriteString("Date (YYYY-MM-DD): ")
	if f.Step == 1 {
		b.WriteString(f.Input.View() + "\n")
	} else {
		b.WriteString(f.Date + "\n")
	}
	b.WriteString("Start (HH:MM): ")
	if f.Step == 2 {
		b.WriteString(f.Input.View() + "\n")
	} else {
		b.WriteString(f.Start + "\n")
	}
	b.WriteString("End (HH:MM): ")
	if f.Step == 3 {
		b.WriteString(f.Input.View() + "\n")
	} else {
		b.WriteString(f.End + "\n")
	}
	allDayMark := " "
	if f.AllDay {
		allDayMark = "x"
	}
	b.WriteString("All day: [" + allDayMark + "] (space to toggle)")
	if f.Step == 4 {
		b.WriteString("  ←")
	}
	b.WriteString("\n")
	b.WriteString("Notes:\n")
	if f.Step == 5 {
		b.WriteString(f.Textarea.View() + "\n")
	} else {
		b.WriteString(f.Notes)
		if f.Notes == "" {
			b.WriteString("(empty)")
		}
		b.WriteString("\n")
	}
	if f.Step == 6 && f.contactsRepo != nil {
		b.WriteString("\nAttendees (type to filter, space to toggle):\n")
		b.WriteString(f.AttendeeList.View())
	}
	return b.String()
}

// StatusHint returns the form step-specific hint.
func (f EventForm) StatusHint() string {
	switch f.Step {
	case 0:
		return "Enter next  F10 save  F11 cancel"
	case 1, 2, 3:
		return "Enter next  F10 save  F11 cancel"
	case 4:
		return "Space toggle all-day  Enter next  F10 save  F11 cancel"
	case 5:
		if f.contactsRepo != nil {
			return "Tab indent  Ctrl+Tab attendees  Enter² save  F10 save  F11 cancel"
		}
		return "Enter² save  F10 save  F11 cancel"
	case 6:
		return "Type filter  Space toggle  Enter save  F10 save  F11 cancel"
	}
	return ""
}

// SetSize updates the form's list dimensions (e.g. when main pane is resized).
func (f *EventForm) SetSize(w, h int) {
	if w > 4 {
		f.AttendeeList.SetSize(w-4, 14)
	}
}

// Blur blurs form inputs (call when closing form).
func (f *EventForm) Blur() {
	f.Input.Blur()
	f.Textarea.Blur()
}

func parseTime(s string) (int, bool) {
	re := regexp.MustCompile(`^(\d{1,2}):(\d{2})$`)
	if m := re.FindStringSubmatch(s); len(m) == 3 {
		h, _ := strconv.Atoi(m[1])
		min, _ := strconv.Atoi(m[2])
		if h >= 0 && h <= 23 && min >= 0 && min <= 59 {
			return h*100 + min, true
		}
	}
	if len(s) == 4 {
		h, eh := strconv.Atoi(s[:2])
		min, em := strconv.Atoi(s[2:])
		if eh == nil && em == nil && h >= 0 && h <= 23 && min >= 0 && min <= 59 {
			return h*100 + min, true
		}
	}
	return 0, false
}

func parseDate(s string) (year int, month time.Month, day int, ok bool) {
	t, err := time.Parse("2006-01-02", s)
	if err != nil {
		return 0, 0, 0, false
	}
	return t.Year(), t.Month(), t.Day(), true
}

