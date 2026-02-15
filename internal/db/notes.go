package db

import (
	"database/sql"
	"time"
)

// Note represents a note.
type Note struct {
	ID        int64
	Title     string
	Content   string
	CreatedAt time.Time
	UpdatedAt time.Time
}

// NotesRepo provides CRUD for notes.
type NotesRepo struct {
	db *sql.DB
}

// NewNotesRepo returns a new NotesRepo.
func NewNotesRepo(db *sql.DB) *NotesRepo {
	return &NotesRepo{db: db}
}

// Create inserts a new note.
func (r *NotesRepo) Create(n *Note) error {
	res, err := r.db.Exec(
		`INSERT INTO notes (title, content) VALUES (?, ?)`,
		n.Title, n.Content,
	)
	if err != nil {
		return err
	}
	id, err := res.LastInsertId()
	if err != nil {
		return err
	}
	n.ID = id
	n.CreatedAt = time.Now()
	n.UpdatedAt = n.CreatedAt
	return nil
}

// List returns all notes ordered by updated_at desc.
func (r *NotesRepo) List() ([]Note, error) {
	rows, err := r.db.Query(
		`SELECT id, title, content, created_at, updated_at FROM notes ORDER BY updated_at DESC`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanNotes(rows)
}

// Get returns a note by ID.
func (r *NotesRepo) Get(id int64) (*Note, error) {
	var n Note
	err := r.db.QueryRow(
		`SELECT id, title, content, created_at, updated_at FROM notes WHERE id = ?`, id,
	).Scan(&n.ID, &n.Title, &n.Content, &n.CreatedAt, &n.UpdatedAt)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return &n, nil
}

// Update updates a note.
func (r *NotesRepo) Update(n *Note) error {
	n.UpdatedAt = time.Now()
	_, err := r.db.Exec(
		`UPDATE notes SET title=?, content=?, updated_at=? WHERE id=?`,
		n.Title, n.Content, n.UpdatedAt, n.ID,
	)
	return err
}

// Delete removes a note.
func (r *NotesRepo) Delete(id int64) error {
	_, err := r.db.Exec(`DELETE FROM notes WHERE id=?`, id)
	return err
}

func scanNotes(rows *sql.Rows) ([]Note, error) {
	var notes []Note
	for rows.Next() {
		var n Note
		if err := rows.Scan(&n.ID, &n.Title, &n.Content, &n.CreatedAt, &n.UpdatedAt); err != nil {
			return nil, err
		}
		notes = append(notes, n)
	}
	return notes, rows.Err()
}
