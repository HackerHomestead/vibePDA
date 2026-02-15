package db

import (
	"database/sql"
	"time"
)

// Contact represents a contact.
type Contact struct {
	ID        int64
	Name      string
	Email     string
	Phone     string
	Notes     string
	CreatedAt time.Time
	DeletedAt *time.Time
}

// ContactsRepo provides CRUD for contacts.
type ContactsRepo struct {
	db *sql.DB
}

// NewContactsRepo returns a new ContactsRepo.
func NewContactsRepo(db *sql.DB) *ContactsRepo {
	return &ContactsRepo{db: db}
}

// Create inserts a new contact.
func (r *ContactsRepo) Create(c *Contact) error {
	res, err := r.db.Exec(
		`INSERT INTO contacts (name, email, phone, notes) VALUES (?, ?, ?, ?)`,
		c.Name, c.Email, c.Phone, c.Notes,
	)
	if err != nil {
		return err
	}
	id, err := res.LastInsertId()
	if err != nil {
		return err
	}
	c.ID = id
	c.CreatedAt = time.Now()
	return nil
}

// List returns all non-deleted contacts ordered by name.
func (r *ContactsRepo) List() ([]Contact, error) {
	rows, err := r.db.Query(
		`SELECT id, name, email, phone, notes, created_at, deleted_at FROM contacts WHERE deleted_at IS NULL ORDER BY name`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanContacts(rows)
}

// ListDeleted returns contacts that are in trash, ordered by deleted_at desc.
func (r *ContactsRepo) ListDeleted() ([]Contact, error) {
	rows, err := r.db.Query(
		`SELECT id, name, email, phone, notes, created_at, deleted_at FROM contacts WHERE deleted_at IS NOT NULL ORDER BY deleted_at DESC`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanContacts(rows)
}

// Get returns a contact by ID. Returns nil if the contact is deleted (in trash).
func (r *ContactsRepo) Get(id int64) (*Contact, error) {
	var c Contact
	var ignored sql.NullString
	err := r.db.QueryRow(
		`SELECT id, name, email, phone, notes, created_at, deleted_at FROM contacts WHERE id = ? AND deleted_at IS NULL`, id,
	).Scan(&c.ID, &c.Name, &c.Email, &c.Phone, &c.Notes, &c.CreatedAt, &ignored)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return &c, nil
}

// Update updates a contact.
func (r *ContactsRepo) Update(c *Contact) error {
	_, err := r.db.Exec(
		`UPDATE contacts SET name=?, email=?, phone=?, notes=? WHERE id=?`,
		c.Name, c.Email, c.Phone, c.Notes, c.ID,
	)
	return err
}

// Delete soft-deletes a contact (moves to trash).
func (r *ContactsRepo) Delete(id int64) error {
	_, err := r.db.Exec(`UPDATE contacts SET deleted_at = datetime('now') WHERE id = ?`, id)
	return err
}

// Restore restores a soft-deleted contact.
func (r *ContactsRepo) Restore(id int64) error {
	_, err := r.db.Exec(`UPDATE contacts SET deleted_at = NULL WHERE id = ?`, id)
	return err
}

func scanContacts(rows *sql.Rows) ([]Contact, error) {
	var contacts []Contact
	for rows.Next() {
		var c Contact
		var deletedAt sql.NullString
		if err := rows.Scan(&c.ID, &c.Name, &c.Email, &c.Phone, &c.Notes, &c.CreatedAt, &deletedAt); err != nil {
			return nil, err
		}
		if deletedAt.Valid && deletedAt.String != "" {
			if t, err := time.Parse("2006-01-02 15:04:05", deletedAt.String); err == nil {
				c.DeletedAt = &t
			}
		}
		contacts = append(contacts, c)
	}
	return contacts, rows.Err()
}
