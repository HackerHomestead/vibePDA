package contacts

import (
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
)

const (
	listHeight     = 18
	cardHeight     = 6
	cardInnerWidth = 36
	maxNotesLen    = 60
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
func (d contactDelegate) Spacing() int                            { return 1 }
func (d contactDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d contactDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	i, ok := item.(contactItem)
	if !ok {
		return
	}
	c := i.contact
	selected := index == m.Index()

	// Build card content: Name, Email, Phone, Notes
	name := truncate(c.Name, cardInnerWidth-2)
	if name == "" {
		name = "(no name)"
	}
	lines := []string{
		cardNameStyle.Render(name),
	}
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

	box := lipgloss.NewStyle().Width(cardInnerWidth + 4).Padding(0, 1)
	if selected {
		box = box.Border(lipgloss.RoundedBorder()).BorderForeground(lipgloss.Color("62")).Background(lipgloss.Color("236"))
	} else {
		box = box.Border(lipgloss.RoundedBorder()).BorderForeground(lipgloss.Color("240"))
	}
	io.WriteString(w, box.Render(content))
}

var (
	titleStyle     = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("62"))
	cardNameStyle  = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("15"))
	cardLabelStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("241"))
)

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
	l.Title = ""
	l.SetShowStatusBar(false)
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
		m.contactList.SetSize(msg.Width-10, listHeight)
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
	} else {
		m.err = ""
	}
	m.refreshList(nil)
	return m, m.loadContacts
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

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	m.contactList.SetSize(w-10, listHeight)
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
		b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render("Enter: next  Shift+Tab: back  F2: save  Esc: cancel"))
		return b.String()
	}

	b.WriteString(titleStyle.Render(" Contacts ") + "\n\n")
	b.WriteString(m.contactList.View())
	b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(" n new  Enter edit  d delete  j/k select "))

	return b.String()
}
