package db

import (
	"database/sql"
	"time"
)

// Task represents a to-do item.
type Task struct {
	ID        int64
	Title     string
	Done      bool
	DueDate   *time.Time
	Priority  int
	CreatedAt time.Time
}

// TasksRepo provides CRUD for tasks.
type TasksRepo struct {
	db *sql.DB
}

// NewTasksRepo returns a new TasksRepo.
func NewTasksRepo(db *sql.DB) *TasksRepo {
	return &TasksRepo{db: db}
}

// Create inserts a new task.
func (r *TasksRepo) Create(t *Task) error {
	var due interface{}
	if t.DueDate != nil {
		due = t.DueDate.Format("2006-01-02")
	}
	res, err := r.db.Exec(
		`INSERT INTO tasks (title, done, due_date, priority) VALUES (?, ?, ?, ?)`,
		t.Title, boolToInt(t.Done), due, t.Priority,
	)
	if err != nil {
		return err
	}
	id, err := res.LastInsertId()
	if err != nil {
		return err
	}
	t.ID = id
	t.CreatedAt = time.Now()
	return nil
}

// List returns all tasks ordered by done, priority desc, created_at.
func (r *TasksRepo) List() ([]Task, error) {
	rows, err := r.db.Query(
		`SELECT id, title, done, due_date, priority, created_at FROM tasks ORDER BY done, priority DESC, created_at`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanTasks(rows)
}

// Get returns a task by ID.
func (r *TasksRepo) Get(id int64) (*Task, error) {
	var t Task
	var due sql.NullString
	err := r.db.QueryRow(
		`SELECT id, title, done, due_date, priority, created_at FROM tasks WHERE id = ?`, id,
	).Scan(&t.ID, &t.Title, &t.Done, &due, &t.Priority, &t.CreatedAt)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	if due.Valid && due.String != "" {
		if parsed, err := time.Parse("2006-01-02", due.String); err == nil {
			t.DueDate = &parsed
		}
	}
	return &t, nil
}

// Update updates a task.
func (r *TasksRepo) Update(t *Task) error {
	var due interface{}
	if t.DueDate != nil {
		due = t.DueDate.Format("2006-01-02")
	}
	_, err := r.db.Exec(
		`UPDATE tasks SET title=?, done=?, due_date=?, priority=? WHERE id=?`,
		t.Title, boolToInt(t.Done), due, t.Priority, t.ID,
	)
	return err
}

// ToggleDone flips the done state.
func (r *TasksRepo) ToggleDone(id int64) error {
	_, err := r.db.Exec(`UPDATE tasks SET done = 1 - done WHERE id = ?`, id)
	return err
}

// Delete removes a task.
func (r *TasksRepo) Delete(id int64) error {
	_, err := r.db.Exec(`DELETE FROM tasks WHERE id = ?`, id)
	return err
}

// MoveUp swaps priority with the task above in the list.
func (r *TasksRepo) MoveUp(id int64) error {
	tasks, err := r.List()
	if err != nil || len(tasks) < 2 {
		return err
	}
	var idx int = -1
	for i, t := range tasks {
		if t.ID == id {
			idx = i
			break
		}
	}
	if idx <= 0 {
		return nil
	}
	above := tasks[idx-1]
	task := tasks[idx]
	task.Priority, above.Priority = above.Priority, task.Priority
	_ = r.Update(&task)
	return r.Update(&above)
}

// MoveDown swaps priority with the task below in the list.
func (r *TasksRepo) MoveDown(id int64) error {
	tasks, err := r.List()
	if err != nil || len(tasks) < 2 {
		return err
	}
	var idx int = -1
	for i, t := range tasks {
		if t.ID == id {
			idx = i
			break
		}
	}
	if idx < 0 || idx >= len(tasks)-1 {
		return nil
	}
	below := tasks[idx+1]
	task := tasks[idx]
	task.Priority, below.Priority = below.Priority, task.Priority
	_ = r.Update(&task)
	return r.Update(&below)
}

func scanTasks(rows *sql.Rows) ([]Task, error) {
	var tasks []Task
	for rows.Next() {
		var t Task
		var due sql.NullString
		if err := rows.Scan(&t.ID, &t.Title, &t.Done, &due, &t.Priority, &t.CreatedAt); err != nil {
			return nil, err
		}
		if due.Valid && due.String != "" {
			if parsed, err := time.Parse("2006-01-02", due.String); err == nil {
				t.DueDate = &parsed
			}
		}
		tasks = append(tasks, t)
	}
	return tasks, rows.Err()
}
