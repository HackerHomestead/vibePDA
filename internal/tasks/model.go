package tasks

import (
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
)

const listHeight = 14

// taskItem implements list.Item.
type taskItem struct {
	task *db.Task
}

func (i taskItem) Title() string {
	prefix := "[ ] "
	if i.task.Done {
		prefix = "[x] "
	}
	return prefix + i.task.Title
}
func (i taskItem) Description() string {
	if i.task.DueDate != nil {
		return "Due: " + i.task.DueDate.Format("2006-01-02")
	}
	return ""
}
func (i taskItem) FilterValue() string { return i.task.Title }

// taskDelegate renders task list items.
type taskDelegate struct{}

func (d taskDelegate) Height() int                             { return 1 }
func (d taskDelegate) Spacing() int                            { return 0 }
func (d taskDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d taskDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	i, ok := item.(taskItem)
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
	selectedStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("15")).
			Background(lipgloss.Color("62")).
			Padding(0, 1)
	unselectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("252"))
)

// Model is the tasks view model.
type Model struct {
	repo      *db.TasksRepo
	taskList  list.Model
	width     int
	height    int
	err       string
	showForm  bool
	formInput textinput.Model
}

// NewModel creates a new tasks model.
func NewModel(repo *db.TasksRepo, width, height int) Model {
	items := []list.Item{}
	delegate := taskDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = ""
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	ti := textinput.New()
	ti.Placeholder = "Buy milk"
	ti.Width = 40

	return Model{
		repo:      repo,
		taskList:  l,
		formInput: ti,
		width:     width,
		height:    height,
	}
}

// Init loads tasks.
func (m Model) Init() tea.Cmd {
	return m.loadTasks
}

func (m Model) loadTasks() tea.Msg {
	tasks, err := m.repo.List()
	if err != nil {
		return errMsg{err: err}
	}
	return tasksMsg{tasks: tasks}
}

type errMsg struct{ err error }
type tasksMsg struct{ tasks []db.Task }

// Update handles messages.
func (m Model) Update(msg tea.Msg) (Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tasksMsg:
		m.refreshList(msg.tasks)
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
			m.showForm = true
			m.formInput.SetValue("")
			m.formInput.Focus()
			return m, textinput.Blink
		case "d":
			return m.handleDelete()
		case " ", "enter":
			return m.handleToggle()
		case "j", "down":
			var cmd tea.Cmd
			m.taskList, cmd = m.taskList.Update(msg)
			return m, cmd
		case "k", "up":
			var cmd tea.Cmd
			m.taskList, cmd = m.taskList.Update(msg)
			return m, cmd
		}
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		m.taskList.SetSize(msg.Width-10, listHeight)
		return m, nil
	}

	var cmd tea.Cmd
	m.taskList, cmd = m.taskList.Update(msg)
	return m, cmd
}

func (m Model) handleFormKey(msg tea.KeyMsg) (Model, tea.Cmd) {
	switch msg.String() {
	case "esc":
		m.showForm = false
		m.formInput.Blur()
		return m, nil
	case "enter":
		val := strings.TrimSpace(m.formInput.Value())
		if val != "" {
			t := &db.Task{Title: val}
			if err := m.repo.Create(t); err != nil {
				m.err = err.Error()
			} else {
				m.showForm = false
				m.formInput.Blur()
				m.err = ""
			}
			m.refreshList(nil)
			return m, m.loadTasks
		}
		return m, nil
	default:
		var cmd tea.Cmd
		m.formInput, cmd = m.formInput.Update(msg)
		return m, cmd
	}
}

func (m Model) handleDelete() (Model, tea.Cmd) {
	item := m.taskList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ti, ok := item.(taskItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.Delete(ti.task.ID); err != nil {
		m.err = err.Error()
	} else {
		m.err = ""
	}
	m.refreshList(nil)
	return m, m.loadTasks
}

func (m Model) handleToggle() (Model, tea.Cmd) {
	item := m.taskList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ti, ok := item.(taskItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.ToggleDone(ti.task.ID); err != nil {
		m.err = err.Error()
	} else {
		m.err = ""
	}
	m.refreshList(nil)
	return m, m.loadTasks
}

func (m *Model) refreshList(tasks []db.Task) {
	if tasks == nil {
		var err error
		tasks, err = m.repo.List()
		if err != nil {
			return
		}
	}
	items := make([]list.Item, len(tasks))
	for i := range tasks {
		items[i] = taskItem{task: &tasks[i]}
	}
	m.taskList.SetItems(items)
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	m.taskList.SetSize(w-10, listHeight)
}

// View renders the tasks UI.
func (m Model) View() string {
	var b strings.Builder

	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}

	if m.showForm {
		b.WriteString(titleStyle.Render(" New Task ") + "\n\n")
		b.WriteString("Title: " + m.formInput.View() + "\n")
		b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render("Enter: save  Esc: cancel"))
		return b.String()
	}

	b.WriteString(titleStyle.Render(" Tasks ") + "\n\n")
	b.WriteString(m.taskList.View())
	b.WriteString("\n" + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(" n new  space/enter toggle  d delete  j/k select "))

	return b.String()
}
