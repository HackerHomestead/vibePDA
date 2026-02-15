package contacts

import (
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/ui"
	"github.com/you/vibe/internal/toast"
)

const (
	listHeight      = 18
	cardHeight      = 6
	cardInnerWidth  = 36
	cardOuterWidth  = cardInnerWidth + 4
	cardGap         = 2
	maxNotesLen     = 60
	minWidth2Cols   = 2*cardOuterWidth + cardGap + 10 // ~90
	minWidth3Cols   = 3*cardOuterWidth + 2*cardGap + 10
	minWidth4Cols   = 4*cardOuterWidth + 3*cardGap + 10
)

// truncate shortens s to at most n runes, adding "…" if truncated.
func truncate(s string, n int) string {
	runes := []rune(s)
	if len(runes) <= n {
		return s
	}
	return string(runes[:n-1]) + "…"
}

// contactItem implements list.Item.
type contactItem struct {
	contact *db.Contact
}

func (i contactItem) Title() string       { return i.contact.Name }
func (i contactItem) Description() string { return i.contact.Email }
func (i contactItem) FilterValue() string { return i.contact.Name }

type contactDelegate struct{}

func (d contactDelegate) Height() int                             { return cardHeight }
func (d contactDelegate) Spacing() int                            { return 0 }
func (d contactDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d contactDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	i, ok := item.(contactItem)
	if !ok {
		return
	}
	io.WriteString(w, renderContactCard(i.contact, index == m.Index()))
}

var (
	titleStyle     = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color(ui.ColorAccent))
	cardNameStyle  = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color(ui.ColorTitleFg))
	cardLabelStyle = lipgloss.NewStyle().Foreground(lipgloss.Color(ui.ColorTextDim))
)

// renderContactCard returns a single contact card as a string.
func renderContactCard(c *db.Contact, selected bool) string {
	name := truncate(c.Name, cardInnerWidth-2)
	if name == "" {
		name = "(no name)"
	}
	lines := []string{cardNameStyle.Render(name)}
	if c.Email != "" {
		lines = append(lines, cardLabelStyle.Render("Email:")+" "+truncate(c.Email, cardInnerWidth-10))
	}
	if c.Phone != "" {
		lines = append(lines, cardLabelStyle.Render("Phone:")+" "+truncate(c.Phone, cardInnerWidth-10))
	}
	if c.Notes != "" {
		lines = append(lines, cardLabelStyle.Render("Notes:")+" "+truncate(c.Notes, maxNotesLen))
	}
	content := strings.Join(lines, "\n")
	box := lipgloss.NewStyle().Width(cardOuterWidth).Padding(0, 1)
	if selected {
		box = box.Border(ui.RetroBorder).BorderForeground(lipgloss.Color(ui.ColorAccent)).Background(lipgloss.Color(ui.ColorSelectBg))
	} else {
		box = box.Border(ui.RetroBorder).BorderForeground(lipgloss.Color(ui.ColorBorder))
	}
	return box.Render(content)
}

// Form steps: 0=name, 1=email, 2=phone, 3=notes
const contactFormSteps = 4

// Model is the contacts view model.
type Model struct {
	repo        *db.ContactsRepo
	contactList list.Model
	width       int
	height      int
	err         string
	showForm    bool
	formStep    int
	formEditID  int64
	formName    string
	formEmail   string
	formPhone   string
	formNotes   string
	formInput   textinput.Model
}

// NewModel creates a new contacts model.
func NewModel(repo *db.ContactsRepo, width, height int) Model {
	items := []list.Item{}
	delegate := contactDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = " Contacts "
	l.SetShowTitle(true)
	l.Styles.Title = titleStyle
	l.SetShowStatusBar(false)
	l.SetShowPagination(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	ti := textinput.New()
	ti.Width = 40

	return Model{
		repo:        repo,
		contactList: l,
		formInput:   ti,
		width:       width,
		height:      height,
	}
}

// Init loads contacts.
func (m Model) Init() tea.Cmd {
	return m.loadContacts
}

func (m Model) loadContacts() tea.Msg {
	contacts, err := m.repo.List()
	if err != nil {
		return errMsg{err: err}
	}
	return contactsMsg{contacts: contacts}
}

type errMsg struct{ err error }
type contactsMsg struct{ contacts []db.Contact }

// Update handles messages.
func (m Model) Update(msg tea.Msg) (Model, tea.Cmd) {
	switch msg := msg.(type) {
	case contactsMsg:
		m.refreshList(msg.contacts)
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
			m.openForm(nil)
			return m, textinput.Blink
		case "enter":
			item := m.contactList.SelectedItem()
			if ci, ok := item.(contactItem); ok {
				m.openForm(ci.contact)
				return m, textinput.Blink
			}
			return m, nil
		case "d":
			return m.handleDelete()
		case "j", "down":
			var cmd tea.Cmd
			m.contactList, cmd = m.contactList.Update(msg)
			return m, cmd
		case "k", "up":
			var cmd tea.Cmd
			m.contactList, cmd = m.contactList.Update(msg)
			return m, cmd
		}
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		availH := msg.Height - 4
		if availH < 4 {
			availH = 4
		}
		m.contactList.SetSize(msg.Width-10, availH)
		return m, nil
	}

	var cmd tea.Cmd
	m.contactList, cmd = m.contactList.Update(msg)
	return m, cmd
}

func (m *Model) openForm(c *db.Contact) {
	m.showForm = true
	m.formEditID = 0
	m.formStep = 0
	if c != nil {
		m.formEditID = c.ID
		m.formName = c.Name
		m.formEmail = c.Email
		m.formPhone = c.Phone
		m.formNotes = c.Notes
	} else {
		m.formName = ""
		m.formEmail = ""
		m.formPhone = ""
		m.formNotes = ""
	}
	m.formInput.SetValue(m.formName)
	m.formInput.Placeholder = "Full name"
	m.formInput.Focus()
}

func (m Model) handleFormKey(msg tea.KeyMsg) (Model, tea.Cmd) {
	s := msg.String()
	if s == "f2" {
		return m.formSave()
	}
	if s == "esc" || s == "escape" {
		m.showForm = false
		m.formInput.Blur()
		return m, nil
	}
	if s == "enter" {
		val := strings.TrimSpace(m.formInput.Value())
		switch m.formStep {
		case 0:
			if val != "" {
				m.formName = val
				m.formStep = 1
				m.formInput.SetValue(m.formEmail)
				m.formInput.Placeholder = "Email"
				return m, nil
			}
		case 1:
			m.formEmail = val
			m.formStep = 2
			m.formInput.SetValue(m.formPhone)
			m.formInput.Placeholder = "Phone"
			return m, nil
		case 2:
			m.formPhone = val
			m.formStep = 3
			m.formInput.SetValue(m.formNotes)
			m.formInput.Placeholder = "Notes"
			return m, nil
		case 3:
			m.formNotes = val
			return m.formSave()
		}
		return m, nil
	}
	if s == "shift+tab" && m.formStep > 0 {
		m.formStep--
		switch m.formStep {
		case 0:
			m.formInput.SetValue(m.formName)
			m.formInput.Placeholder = "Full name"
		case 1:
			m.formInput.SetValue(m.formEmail)
			m.formInput.Placeholder = "Email"
		case 2:
			m.formInput.SetValue(m.formPhone)
			m.formInput.Placeholder = "Phone"
		case 3:
			m.formInput.SetValue(m.formNotes)
			m.formInput.Placeholder = "Notes"
		}
		m.formInput.Focus()
		return m, nil
	}

	var cmd tea.Cmd
	m.formInput, cmd = m.formInput.Update(msg)
	return m, cmd
}

func (m Model) formSave() (Model, tea.Cmd) {
	val := strings.TrimSpace(m.formInput.Value())
	switch m.formStep {
	case 0:
		m.formName = val
	case 1:
		m.formEmail = val
	case 2:
		m.formPhone = val
	case 3:
		m.formNotes = val
	}
	if m.formName == "" {
		return m, nil
	}
	if m.formEditID != 0 {
		c, _ := m.repo.Get(m.formEditID)
		if c != nil {
			c.Name = m.formName
			c.Email = m.formEmail
			c.Phone = m.formPhone
			c.Notes = m.formNotes
			if err := m.repo.Update(c); err != nil {
				m.err = err.Error()
			} else {
				m.showForm = false
				m.formInput.Blur()
				m.err = ""
				m.refreshList(nil)
				return m, tea.Batch(m.loadContacts, func() tea.Msg { return toast.Msg{Text: "Contact updated"} })
			}
		}
	} else {
		c := &db.Contact{Name: m.formName, Email: m.formEmail, Phone: m.formPhone, Notes: m.formNotes}
		if err := m.repo.Create(c); err != nil {
			m.err = err.Error()
		} else {
			m.showForm = false
			m.formInput.Blur()
			m.err = ""
			m.refreshList(nil)
			return m, tea.Batch(m.loadContacts, func() tea.Msg { return toast.Msg{Text: "Contact created"} })
		}
	}
	m.refreshList(nil)
	return m, m.loadContacts
}

func (m Model) handleDelete() (Model, tea.Cmd) {
	item := m.contactList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ci, ok := item.(contactItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.Delete(ci.contact.ID); err != nil {
		m.err = err.Error()
		return m, nil
	}
	m.err = ""
	m.refreshList(nil)
	return m, tea.Batch(m.loadContacts, func() tea.Msg { return toast.Msg{Text: "Contact deleted"} })
}

func (m *Model) refreshList(contacts []db.Contact) {
	if contacts == nil {
		var err error
		contacts, err = m.repo.List()
		if err != nil {
			return
		}
	}
	items := make([]list.Item, len(contacts))
	for i := range contacts {
		items[i] = contactItem{contact: &contacts[i]}
	}
	m.contactList.SetItems(items)
}

// Count returns the number of contacts in the list.
func (m Model) Count() int {
	return len(m.contactList.Items())
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	// Use available height minus title (2) and help (1); min 4 lines visible
	availH := h - 4
	if availH < 4 {
		availH = 4
	}
	m.contactList.SetSize(w-10, availH)
}

// StatusHint returns context-specific key bindings for the status bar.
func (m Model) StatusHint() string {
	if m.showForm {
		return "Enter next  Tab next  F10 save  F11 cancel"
	}
	return "F5 new  F6 edit  F7 del"
}

// View renders the contacts UI.
func (m Model) View() string {
	var b strings.Builder

	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}

	if m.showForm {
		title := " New Contact "
		if m.formEditID != 0 {
			title = " Edit Contact "
		}
		b.WriteString(titleStyle.Render(title) + "\n\n")
		labels := []string{"Name:", "Email:", "Phone:", "Notes:"}
		values := []string{m.formName, m.formEmail, m.formPhone, m.formNotes}
		for i := 0; i < 4; i++ {
			if m.formStep == i {
				b.WriteString(labels[i] + " " + m.formInput.View() + "\n")
			} else {
				b.WriteString(labels[i] + " " + values[i] + "\n")
			}
		}
		return b.String()
	}

	// Multi-column layout when terminal is wide enough (list has built-in title)
	if m.width >= minWidth2Cols {
		b.WriteString(titleStyle.Render(" Contacts ") + "\n\n")
		b.WriteString(m.renderGrid())
	} else {
		b.WriteString(m.contactList.View())
	}

	return b.String()
}

// renderGrid lays out contact cards in multiple columns (2–4 based on width).
func (m Model) renderGrid() string {
	items := m.contactList.Items()
	if len(items) == 0 {
		return ""
	}
	sel := m.contactList.Index()

	cols := 2
	if m.width >= minWidth4Cols {
		cols = 4
	} else if m.width >= minWidth3Cols {
		cols = 3
	}

	gap := strings.Repeat(" ", cardGap)
	var rows []string
	for i := 0; i < len(items); i += cols {
		var parts []string
		for j := 0; j < cols && i+j < len(items); j++ {
			if j > 0 {
				parts = append(parts, gap)
			}
			idx := i + j
			ci, ok := items[idx].(contactItem)
			if !ok {
				continue
			}
			parts = append(parts, renderContactCard(ci.contact, idx == sel))
		}
		rows = append(rows, lipgloss.JoinHorizontal(lipgloss.Top, parts...))
	}
	return lipgloss.JoinVertical(lipgloss.Left, rows...)
}
