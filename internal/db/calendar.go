package db

import (
	"database/sql"
	"time"
)

// Event represents a calendar event.
type Event struct {
	ID          int64
	Title       string
	Description string
	StartAt     time.Time
	EndAt       time.Time
	AllDay      bool
	Location    string
	CreatedAt   time.Time
	UpdatedAt   time.Time
}

// CalendarRepo provides CRUD for calendar events.
type CalendarRepo struct {
	db *sql.DB
}

// NewCalendarRepo returns a new CalendarRepo.
func NewCalendarRepo(db *sql.DB) *CalendarRepo {
	return &CalendarRepo{db: db}
}

// Create inserts a new event.
func (r *CalendarRepo) Create(e *Event) error {
	res, err := r.db.Exec(
		`INSERT INTO calendar_events (title, description, start_at, end_at, all_day, location)
		 VALUES (?, ?, ?, ?, ?, ?)`,
		e.Title, e.Description, e.StartAt, e.EndAt, boolToInt(e.AllDay), e.Location,
	)
	if err != nil {
		return err
	}
	id, err := res.LastInsertId()
	if err != nil {
		return err
	}
	e.ID = id
	e.CreatedAt = time.Now()
	e.UpdatedAt = e.CreatedAt
	return nil
}

// ListByMonth returns events that overlap the given year and month.
func (r *CalendarRepo) ListByMonth(year int, month int) ([]Event, error) {
	start := time.Date(year, time.Month(month), 1, 0, 0, 0, 0, time.Local)
	end := start.AddDate(0, 1, 0)
	rows, err := r.db.Query(
		`SELECT id, title, description, start_at, end_at, all_day, location, created_at, updated_at
		 FROM calendar_events
		 WHERE start_at < ? AND end_at > ?
		 ORDER BY start_at`,
		end, start,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanEvents(rows)
}

// ListByDay returns events for the given date.
func (r *CalendarRepo) ListByDay(year int, month int, day int) ([]Event, error) {
	start := time.Date(year, time.Month(month), day, 0, 0, 0, 0, time.Local)
	end := start.AddDate(0, 0, 1)
	rows, err := r.db.Query(
		`SELECT id, title, description, start_at, end_at, all_day, location, created_at, updated_at
		 FROM calendar_events
		 WHERE start_at < ? AND end_at > ?
		 ORDER BY start_at`,
		end, start,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanEvents(rows)
}

// Get returns an event by ID.
func (r *CalendarRepo) Get(id int64) (*Event, error) {
	var e Event
	err := r.db.QueryRow(
		`SELECT id, title, description, start_at, end_at, all_day, location, created_at, updated_at
		 FROM calendar_events WHERE id = ?`, id,
	).Scan(
		&e.ID, &e.Title, &e.Description, &e.StartAt, &e.EndAt, &e.AllDay, &e.Location,
		&e.CreatedAt, &e.UpdatedAt,
	)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return &e, nil
}

// Update updates an event.
func (r *CalendarRepo) Update(e *Event) error {
	e.UpdatedAt = time.Now()
	_, err := r.db.Exec(
		`UPDATE calendar_events SET title=?, description=?, start_at=?, end_at=?, all_day=?, location=?, updated_at=?
		 WHERE id=?`,
		e.Title, e.Description, e.StartAt, e.EndAt, boolToInt(e.AllDay), e.Location, e.UpdatedAt, e.ID,
	)
	return err
}

// Delete removes an event.
func (r *CalendarRepo) Delete(id int64) error {
	_, _ = r.db.Exec(`DELETE FROM event_attendees WHERE event_id=?`, id)
	_, err := r.db.Exec(`DELETE FROM calendar_events WHERE id=?`, id)
	return err
}

// ListAttendees returns contact IDs for an event.
func (r *CalendarRepo) ListAttendees(eventID int64) ([]int64, error) {
	rows, err := r.db.Query(`SELECT contact_id FROM event_attendees WHERE event_id=? ORDER BY contact_id`, eventID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var ids []int64
	for rows.Next() {
		var cid int64
		if err := rows.Scan(&cid); err != nil {
			return nil, err
		}
		ids = append(ids, cid)
	}
	return ids, rows.Err()
}

// SetAttendees replaces attendees for an event.
func (r *CalendarRepo) SetAttendees(eventID int64, contactIDs []int64) error {
	if _, err := r.db.Exec(`DELETE FROM event_attendees WHERE event_id=?`, eventID); err != nil {
		return err
	}
	for _, cid := range contactIDs {
		if _, err := r.db.Exec(`INSERT INTO event_attendees (event_id, contact_id) VALUES (?, ?)`, eventID, cid); err != nil {
			return err
		}
	}
	return nil
}

func scanEvents(rows *sql.Rows) ([]Event, error) {
	var events []Event
	for rows.Next() {
		var e Event
		var allDay int
		if err := rows.Scan(
			&e.ID, &e.Title, &e.Description, &e.StartAt, &e.EndAt, &allDay, &e.Location,
			&e.CreatedAt, &e.UpdatedAt,
		); err != nil {
			return nil, err
		}
		e.AllDay = allDay == 1
		events = append(events, e)
	}
	return events, rows.Err()
}

func boolToInt(b bool) int {
	if b {
		return 1
	}
	return 0
}
