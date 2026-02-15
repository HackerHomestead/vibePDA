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
	DeletedAt *time.Time
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

// List returns all non-deleted notes ordered by updated_at desc.
func (r *NotesRepo) List() ([]Note, error) {
	rows, err := r.db.Query(
		`SELECT id, title, content, created_at, updated_at, deleted_at FROM notes WHERE deleted_at IS NULL ORDER BY updated_at DESC`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanNotes(rows)
}

// ListDeleted returns notes that are in trash, ordered by deleted_at desc.
func (r *NotesRepo) ListDeleted() ([]Note, error) {
	rows, err := r.db.Query(
		`SELECT id, title, content, created_at, updated_at, deleted_at FROM notes WHERE deleted_at IS NOT NULL ORDER BY deleted_at DESC`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanNotes(rows)
}

// Get returns a note by ID. Returns nil if the note is deleted (in trash).
func (r *NotesRepo) Get(id int64) (*Note, error) {
	var n Note
	var ignored sql.NullString
	err := r.db.QueryRow(
		`SELECT id, title, content, created_at, updated_at, deleted_at FROM notes WHERE id = ? AND deleted_at IS NULL`, id,
	).Scan(&n.ID, &n.Title, &n.Content, &n.CreatedAt, &n.UpdatedAt, &ignored)
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

// Delete soft-deletes a note (moves to trash).
func (r *NotesRepo) Delete(id int64) error {
	_, err := r.db.Exec(`UPDATE notes SET deleted_at = datetime('now') WHERE id = ?`, id)
	return err
}

// Restore restores a soft-deleted note.
func (r *NotesRepo) Restore(id int64) error {
	_, err := r.db.Exec(`UPDATE notes SET deleted_at = NULL WHERE id = ?`, id)
	return err
}

func scanNotes(rows *sql.Rows) ([]Note, error) {
	var notes []Note
	for rows.Next() {
		var n Note
		var deletedAt sql.NullString
		if err := rows.Scan(&n.ID, &n.Title, &n.Content, &n.CreatedAt, &n.UpdatedAt, &deletedAt); err != nil {
			return nil, err
		}
		if deletedAt.Valid && deletedAt.String != "" {
			if t, err := time.Parse("2006-01-02 15:04:05", deletedAt.String); err == nil {
				n.DeletedAt = &t
			}
		}
		notes = append(notes, n)
	}
	return notes, rows.Err()
}
