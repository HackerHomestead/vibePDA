package db

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"time"
)

// ExportVersion is the schema version of the export format.
const ExportVersion = 1

// ExportData is the JSON structure for backup/restore and upgrade.
type ExportData struct {
	Version      int               `json:"version"`
	ExportedAt   time.Time         `json:"exported_at"`
	Notes        []ExportNote      `json:"notes"`
	Tasks        []ExportTask      `json:"tasks"`
	Contacts     []ExportContact   `json:"contacts"`
	Events       []ExportEvent     `json:"events"`
	EventAttendees []ExportAttendee `json:"event_attendees"`
}

type ExportNote struct {
	ID        int64      `json:"id"`
	Title     string     `json:"title"`
	Content   string     `json:"content"`
	CreatedAt time.Time  `json:"created_at"`
	UpdatedAt time.Time  `json:"updated_at"`
	DeletedAt *time.Time `json:"deleted_at,omitempty"`
}

type ExportTask struct {
	ID        int64      `json:"id"`
	Title     string     `json:"title"`
	Done      bool       `json:"done"`
	DueDate   *time.Time `json:"due_date,omitempty"`
	Priority  int        `json:"priority"`
	CreatedAt time.Time  `json:"created_at"`
	UpdatedAt time.Time  `json:"updated_at"`
	DeletedAt *time.Time `json:"deleted_at,omitempty"`
}

type ExportContact struct {
	ID        int64      `json:"id"`
	Name      string     `json:"name"`
	Email     string     `json:"email"`
	Phone     string     `json:"phone"`
	Notes     string     `json:"notes"`
	CreatedAt time.Time  `json:"created_at"`
	DeletedAt *time.Time `json:"deleted_at,omitempty"`
}

type ExportEvent struct {
	ID          int64      `json:"id"`
	Title       string     `json:"title"`
	Description string     `json:"description"`
	StartAt     time.Time  `json:"start_at"`
	EndAt       time.Time  `json:"end_at"`
	AllDay      bool       `json:"all_day"`
	Location    string     `json:"location"`
	CreatedAt   time.Time  `json:"created_at"`
	UpdatedAt   time.Time  `json:"updated_at"`
	DeletedAt   *time.Time `json:"deleted_at,omitempty"`
}

type ExportAttendee struct {
	EventID   int64 `json:"event_id"`
	ContactID int64 `json:"contact_id"`
}

// Export reads all data from the database into a JSON-serializable structure.
func Export(db *sql.DB) (*ExportData, error) {
	notes, err := exportNotes(db)
	if err != nil {
		return nil, fmt.Errorf("export notes: %w", err)
	}
	tasks, err := exportTasks(db)
	if err != nil {
		return nil, fmt.Errorf("export tasks: %w", err)
	}
	contacts, err := exportContacts(db)
	if err != nil {
		return nil, fmt.Errorf("export contacts: %w", err)
	}
	events, err := exportEvents(db)
	if err != nil {
		return nil, fmt.Errorf("export events: %w", err)
	}
	attendees, err := exportAttendees(db)
	if err != nil {
		return nil, fmt.Errorf("export event_attendees: %w", err)
	}
	return &ExportData{
		Version:        ExportVersion,
		ExportedAt:     time.Now(),
		Notes:          notes,
		Tasks:          tasks,
		Contacts:       contacts,
		Events:         events,
		EventAttendees: attendees,
	}, nil
}

func exportNotes(db *sql.DB) ([]ExportNote, error) {
	rows, err := db.Query(`SELECT id, title, content, created_at, updated_at, deleted_at FROM notes ORDER BY id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []ExportNote
	for rows.Next() {
		var n ExportNote
		var deletedAt sql.NullString
		if err := rows.Scan(&n.ID, &n.Title, &n.Content, &n.CreatedAt, &n.UpdatedAt, &deletedAt); err != nil {
			return nil, err
		}
		if deletedAt.Valid && deletedAt.String != "" {
			if t, err := time.Parse("2006-01-02 15:04:05", deletedAt.String); err == nil {
				n.DeletedAt = &t
			}
		}
		out = append(out, n)
	}
	return out, rows.Err()
}

func exportTasks(db *sql.DB) ([]ExportTask, error) {
	rows, err := db.Query(`SELECT id, title, done, due_date, priority, created_at, updated_at, deleted_at FROM tasks ORDER BY id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []ExportTask
	for rows.Next() {
		var t ExportTask
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
		out = append(out, t)
	}
	return out, rows.Err()
}

func exportContacts(db *sql.DB) ([]ExportContact, error) {
	rows, err := db.Query(`SELECT id, name, email, phone, notes, created_at, deleted_at FROM contacts ORDER BY id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []ExportContact
	for rows.Next() {
		var c ExportContact
		var deletedAt sql.NullString
		if err := rows.Scan(&c.ID, &c.Name, &c.Email, &c.Phone, &c.Notes, &c.CreatedAt, &deletedAt); err != nil {
			return nil, err
		}
		if deletedAt.Valid && deletedAt.String != "" {
			if t, err := time.Parse("2006-01-02 15:04:05", deletedAt.String); err == nil {
				c.DeletedAt = &t
			}
		}
		out = append(out, c)
	}
	return out, rows.Err()
}

func exportEvents(db *sql.DB) ([]ExportEvent, error) {
	rows, err := db.Query(`SELECT id, title, description, start_at, end_at, all_day, location, created_at, updated_at, deleted_at FROM calendar_events ORDER BY id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []ExportEvent
	for rows.Next() {
		var e ExportEvent
		var allDay int
		var deletedAt sql.NullString
		if err := rows.Scan(&e.ID, &e.Title, &e.Description, &e.StartAt, &e.EndAt, &allDay, &e.Location, &e.CreatedAt, &e.UpdatedAt, &deletedAt); err != nil {
			return nil, err
		}
		e.AllDay = allDay == 1
		if deletedAt.Valid && deletedAt.String != "" {
			if t, err := time.Parse("2006-01-02 15:04:05", deletedAt.String); err == nil {
				e.DeletedAt = &t
			}
		}
		out = append(out, e)
	}
	return out, rows.Err()
}

func exportAttendees(db *sql.DB) ([]ExportAttendee, error) {
	rows, err := db.Query(`SELECT event_id, contact_id FROM event_attendees ORDER BY event_id, contact_id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []ExportAttendee
	for rows.Next() {
		var a ExportAttendee
		if err := rows.Scan(&a.EventID, &a.ContactID); err != nil {
			return nil, err
		}
		out = append(out, a)
	}
	return out, rows.Err()
}

func fmtTime(t time.Time) string {
	return t.Format("2006-01-02 15:04:05")
}

func fmtTimePtr(t *time.Time) interface{} {
	if t == nil {
		return nil
	}
	return t.Format("2006-01-02 15:04:05")
}

// Import loads export data into the database. IDs are reassigned; event_attendees
// are remapped using the old→new ID mappings. Use after Open() on a fresh schema.
func Import(db *sql.DB, data *ExportData) error {
	contactIDMap := make(map[int64]int64) // old -> new
	eventIDMap := make(map[int64]int64)   // old -> new

	for _, c := range data.Contacts {
		res, err := db.Exec(
			`INSERT INTO contacts (name, email, phone, notes, created_at, deleted_at) VALUES (?, ?, ?, ?, ?, ?)`,
			c.Name, c.Email, c.Phone, c.Notes, fmtTime(c.CreatedAt), fmtTimePtr(c.DeletedAt),
		)
		if err != nil {
			return fmt.Errorf("import contact: %w", err)
		}
		id, _ := res.LastInsertId()
		contactIDMap[c.ID] = id
	}

	for _, n := range data.Notes {
		_, err := db.Exec(
			`INSERT INTO notes (title, content, created_at, updated_at, deleted_at) VALUES (?, ?, ?, ?, ?)`,
			n.Title, n.Content, fmtTime(n.CreatedAt), fmtTime(n.UpdatedAt), fmtTimePtr(n.DeletedAt),
		)
		if err != nil {
			return fmt.Errorf("import note: %w", err)
		}
	}

	for _, t := range data.Tasks {
		var due interface{}
		if t.DueDate != nil {
			due = t.DueDate.Format("2006-01-02")
		}
		res, err := db.Exec(
			`INSERT INTO tasks (title, done, due_date, priority, created_at, updated_at, deleted_at) VALUES (?, ?, ?, ?, ?, ?, ?)`,
			t.Title, boolToInt(t.Done), due, t.Priority, fmtTime(t.CreatedAt), fmtTime(t.UpdatedAt), fmtTimePtr(t.DeletedAt),
		)
		if err != nil {
			return fmt.Errorf("import task: %w", err)
		}
		_ = res
	}

	for _, e := range data.Events {
		res, err := db.Exec(
			`INSERT INTO calendar_events (title, description, start_at, end_at, all_day, location, created_at, updated_at, deleted_at)
			 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
			e.Title, e.Description, e.StartAt, e.EndAt, boolToInt(e.AllDay), e.Location,
			fmtTime(e.CreatedAt), fmtTime(e.UpdatedAt), fmtTimePtr(e.DeletedAt),
		)
		if err != nil {
			return fmt.Errorf("import event: %w", err)
		}
		id, _ := res.LastInsertId()
		eventIDMap[e.ID] = id
	}

	for _, a := range data.EventAttendees {
		newEventID, ok1 := eventIDMap[a.EventID]
		newContactID, ok2 := contactIDMap[a.ContactID]
		if !ok1 || !ok2 {
			continue // skip if event or contact wasn't imported (shouldn't happen with valid export)
		}
		if _, err := db.Exec(`INSERT INTO event_attendees (event_id, contact_id) VALUES (?, ?)`, newEventID, newContactID); err != nil {
			return fmt.Errorf("import event_attendee: %w", err)
		}
	}

	return nil
}

// ExportToJSON serializes ExportData to JSON.
func ExportToJSON(data *ExportData) ([]byte, error) {
	return json.MarshalIndent(data, "", "  ")
}

// ImportFromJSON deserializes ExportData from JSON.
func ImportFromJSON(raw []byte) (*ExportData, error) {
	var data ExportData
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	return &data, nil
}
