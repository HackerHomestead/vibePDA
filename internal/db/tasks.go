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
	UpdatedAt time.Time
	DeletedAt *time.Time
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

// List returns all non-deleted tasks ordered by id (insertion order; completed tasks stay in place).
func (r *TasksRepo) List() ([]Task, error) {
	rows, err := r.db.Query(
		`SELECT id, title, done, due_date, priority, created_at, updated_at, deleted_at FROM tasks WHERE deleted_at IS NULL ORDER BY id`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanTasks(rows)
}

// ListDeleted returns tasks that are in trash, ordered by deleted_at desc.
func (r *TasksRepo) ListDeleted() ([]Task, error) {
	rows, err := r.db.Query(
		`SELECT id, title, done, due_date, priority, created_at, updated_at, deleted_at FROM tasks WHERE deleted_at IS NOT NULL ORDER BY deleted_at DESC`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanTasks(rows)
}

// Get returns a task by ID. Returns nil if the task is deleted (in trash).
func (r *TasksRepo) Get(id int64) (*Task, error) {
	var t Task
	var due, ignored sql.NullString
	err := r.db.QueryRow(
		`SELECT id, title, done, due_date, priority, created_at, updated_at, deleted_at FROM tasks WHERE id = ? AND deleted_at IS NULL`, id,
	).Scan(&t.ID, &t.Title, &t.Done, &due, &t.Priority, &t.CreatedAt, &t.UpdatedAt, &ignored)
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
	t.UpdatedAt = time.Now()
	var due interface{}
	if t.DueDate != nil {
		due = t.DueDate.Format("2006-01-02")
	}
	_, err := r.db.Exec(
		`UPDATE tasks SET title=?, done=?, due_date=?, priority=?, updated_at=? WHERE id=?`,
		t.Title, boolToInt(t.Done), due, t.Priority, t.UpdatedAt, t.ID,
	)
	return err
}

// ToggleDone flips the done state.
func (r *TasksRepo) ToggleDone(id int64) error {
	_, err := r.db.Exec(`UPDATE tasks SET done = 1 - done, updated_at = datetime('now') WHERE id = ?`, id)
	return err
}

// Delete soft-deletes a task (moves to trash).
func (r *TasksRepo) Delete(id int64) error {
	_, err := r.db.Exec(`UPDATE tasks SET deleted_at = datetime('now') WHERE id = ?`, id)
	return err
}

// Restore restores a soft-deleted task.
func (r *TasksRepo) Restore(id int64) error {
	_, err := r.db.Exec(`UPDATE tasks SET deleted_at = NULL WHERE id = ?`, id)
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
		var due, deletedAt sql.NullString
		if err := rows.Scan(&t.ID, &t.Title, &t.Done, &due, &t.Priority, &t.CreatedAt, &t.UpdatedAt, &deletedAt); err != nil {
			return nil, err
		}
		if due.Valid && due.String != "" {
			if parsed, err := time.Parse("2006-01-02", due.String); err == nil {
				t.DueDate = &parsed
			}
		}
		if deletedAt.Valid && deletedAt.String != "" {
			if parsed, err := time.Parse("2006-01-02 15:04:05", deletedAt.String); err == nil {
				t.DeletedAt = &parsed
			}
		}
		tasks = append(tasks, t)
	}
	return tasks, rows.Err()
}
