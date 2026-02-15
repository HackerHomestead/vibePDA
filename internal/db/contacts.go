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

// List returns all contacts ordered by name.
func (r *ContactsRepo) List() ([]Contact, error) {
	rows, err := r.db.Query(
		`SELECT id, name, email, phone, notes, created_at FROM contacts ORDER BY name`,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanContacts(rows)
}

// Get returns a contact by ID.
func (r *ContactsRepo) Get(id int64) (*Contact, error) {
	var c Contact
	err := r.db.QueryRow(
		`SELECT id, name, email, phone, notes, created_at FROM contacts WHERE id = ?`, id,
	).Scan(&c.ID, &c.Name, &c.Email, &c.Phone, &c.Notes, &c.CreatedAt)
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

// Delete removes a contact.
func (r *ContactsRepo) Delete(id int64) error {
	_, err := r.db.Exec(`DELETE FROM contacts WHERE id=?`, id)
	return err
}

func scanContacts(rows *sql.Rows) ([]Contact, error) {
	var contacts []Contact
	for rows.Next() {
		var c Contact
		if err := rows.Scan(&c.ID, &c.Name, &c.Email, &c.Phone, &c.Notes, &c.CreatedAt); err != nil {
			return nil, err
		}
		contacts = append(contacts, c)
	}
	return contacts, rows.Err()
}
