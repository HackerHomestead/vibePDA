package tasks

import (
	"fmt"
	"io"
	"sort"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/you/vibe/internal/db"
	"github.com/you/vibe/internal/toast"
	"github.com/you/vibe/internal/ui"
)

const listHeight = 14

// filterShow: 0=all, 1=incomplete, 2=complete
// sortBy: "created" | "due" | "priority" | "title"

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
	parts := []string{}
	if i.task.DueDate != nil {
		parts = append(parts, "Due: "+i.task.DueDate.Format("2006-01-02"))
	}
	ts := i.task.UpdatedAt
	if ts.IsZero() {
		ts = i.task.CreatedAt
	}
	parts = append(parts, ui.PrettyTime(ts))
	return strings.Join(parts, " · ")
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
	line := i.Title()
	if desc := i.Description(); desc != "" {
		line += "  " + lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(desc)
	}
	if index == m.Index() {
		line = selectedStyle.Render("▶ " + line)
	} else {
		line = unselectedStyle.Render("  " + line)
	}
	io.WriteString(w, line+"\n")
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
	repo       *db.TasksRepo
	taskList   list.Model
	width      int
	height     int
	err        string
	showForm   bool
	formEditID int64 // 0=new, else edit
	formInput  textinput.Model
	filterShow int    // 0=all, 1=incomplete, 2=complete
	sortBy     string // "created"|"due"|"priority"|"title"
}

// NewModel creates a new tasks model.
func NewModel(repo *db.TasksRepo, width, height int) Model {
	items := []list.Item{}
	delegate := taskDelegate{}
	l := list.New(items, delegate, width-4, listHeight)
	l.Title = ""
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(true)
	l.SetShowHelp(false)
	l.DisableQuitKeybindings()

	ti := textinput.New()
	ti.Placeholder = "Buy milk"
	ti.Width = 40

	return Model{
		repo:       repo,
		taskList:   l,
		formInput:  ti,
		width:      width,
		height:     height,
		filterShow: 0,
		sortBy:     "created",
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
			m.formEditID = 0
			m.formInput.SetValue("")
			m.formInput.Focus()
			return m, textinput.Blink
		case "enter":
			// Edit selected task
			item := m.taskList.SelectedItem()
			if ti, ok := item.(taskItem); ok {
				m.showForm = true
				m.formEditID = ti.task.ID
				m.formInput.SetValue(ti.task.Title)
				m.formInput.Focus()
				return m, textinput.Blink
			}
			return m, nil
		case "d":
			return m.handleDelete()
		case " ":
			return m.handleToggle()
		case "ctrl+up":
			return m.handleMoveUp()
		case "ctrl+down":
			return m.handleMoveDown()
		case "F": // Shift+f: cycle filter (all/incomplete/complete)
			m.filterShow = (m.filterShow + 1) % 3
			m.refreshList(nil)
			return m, nil
		case "S": // Shift+s: cycle sort
			sorts := []string{"created", "due", "priority", "title"}
			next := 0
			for i, x := range sorts {
				if x == m.sortBy {
					next = (i + 1) % len(sorts)
					break
				}
			}
			m.sortBy = sorts[next]
			m.refreshList(nil)
			return m, nil
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
		listH := msg.Height - 6
		if listH < 4 {
			listH = 4
		}
		m.taskList.SetSize(msg.Width-10, listH)
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
	case "enter", "f2":
		val := strings.TrimSpace(m.formInput.Value())
		if val != "" {
			if m.formEditID != 0 {
				t, _ := m.repo.Get(m.formEditID)
				if t != nil {
					t.Title = val
					if err := m.repo.Update(t); err != nil {
						m.err = err.Error()
					} else {
						m.showForm = false
						m.formInput.Blur()
						m.err = ""
						m.refreshList(nil)
						return m, tea.Batch(m.loadTasks, func() tea.Msg { return toast.Msg{Text: "Task updated"} })
					}
				}
			} else {
				t := &db.Task{Title: val}
				if err := m.repo.Create(t); err != nil {
					m.err = err.Error()
				} else {
					m.showForm = false
					m.formInput.Blur()
					m.err = ""
					m.refreshList(nil)
					return m, tea.Batch(m.loadTasks, func() tea.Msg { return toast.Msg{Text: "Task created"} })
				}
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

func (m Model) handleMoveUp() (Model, tea.Cmd) {
	item := m.taskList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ti, ok := item.(taskItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.MoveUp(ti.task.ID); err != nil {
		m.err = err.Error()
	} else {
		m.err = ""
	}
	m.refreshList(nil)
	return m, m.loadTasks
}

func (m Model) handleMoveDown() (Model, tea.Cmd) {
	item := m.taskList.SelectedItem()
	if item == nil {
		return m, nil
	}
	ti, ok := item.(taskItem)
	if !ok {
		return m, nil
	}
	if err := m.repo.MoveDown(ti.task.ID); err != nil {
		m.err = err.Error()
	} else {
		m.err = ""
	}
	m.refreshList(nil)
	return m, m.loadTasks
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
		return m, nil
	}
	m.err = ""
	m.refreshList(nil)
	return m, tea.Batch(m.loadTasks, func() tea.Msg { return toast.Msg{Text: "Task deleted"} })
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
	// Filter by done status
	var filtered []db.Task
	switch m.filterShow {
	case 1: // incomplete
		for i := range tasks {
			if !tasks[i].Done {
				filtered = append(filtered, tasks[i])
			}
		}
	case 2: // complete
		for i := range tasks {
			if tasks[i].Done {
				filtered = append(filtered, tasks[i])
			}
		}
	default: // all
		filtered = tasks
	}
	// Sort
	sort.Slice(filtered, func(i, j int) bool {
		a, b := &filtered[i], &filtered[j]
		switch m.sortBy {
		case "due":
			if a.DueDate == nil && b.DueDate == nil {
				return a.ID < b.ID
			}
			if a.DueDate == nil {
				return false
			}
			if b.DueDate == nil {
				return true
			}
			return a.DueDate.Before(*b.DueDate)
		case "priority":
			if a.Priority != b.Priority {
				return a.Priority > b.Priority
			}
			return a.ID < b.ID
		case "title":
			if a.Title != b.Title {
				return strings.ToLower(a.Title) < strings.ToLower(b.Title)
			}
			return a.ID < b.ID
		default: // created
			return a.CreatedAt.Before(b.CreatedAt)
		}
	})
	items := make([]list.Item, len(filtered))
	for i := range filtered {
		items[i] = taskItem{task: &filtered[i]}
	}
	m.taskList.SetItems(items)
}

// SetSize updates width/height.
func (m *Model) SetSize(w, h int) {
	m.width = w
	m.height = h
	listH := h - 6
	if listH < 4 {
		listH = 4
	}
	m.taskList.SetSize(w-10, listH)
}

// StatusHint returns context-specific key bindings for the status bar.
func (m Model) StatusHint() string {
	if m.showForm {
		return "Enter next  F10 save  F11 cancel"
	}
	return "/ search  F filter  S sort  F5 new  F6 edit  F7 del"
}

// View renders the tasks UI.
func (m Model) View() string {
	var b strings.Builder

	if m.err != "" {
		b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("9")).Render("Error: "+m.err) + "\n")
	}

	filterLabel := "all"
	if m.filterShow == 1 {
		filterLabel = "incomplete"
	} else if m.filterShow == 2 {
		filterLabel = "complete"
	}

	if m.showForm {
		title := " New Task "
		if m.formEditID != 0 {
			title = " Edit Task "
		}
		b.WriteString(titleStyle.Render(title) + "\n\n")
		b.WriteString("Title: " + m.formInput.View() + "\n")
		return b.String()
	}

	b.WriteString(titleStyle.Render(" Tasks ") + "  ")
	b.WriteString(lipgloss.NewStyle().Foreground(lipgloss.Color("241")).Render(
		fmt.Sprintf("filter: %s | sort: %s", filterLabel, m.sortBy)) + "\n\n")
	b.WriteString(m.taskList.View())

	return b.String()
}
