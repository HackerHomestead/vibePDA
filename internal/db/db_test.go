package db

import (
	"database/sql"
	"path/filepath"
	"testing"
	"time"
)

func setupDB(t *testing.T) *sql.DB {
	t.Helper()
	// Use temp file for SQLite (in-memory has some quirks with modernc.org/sqlite)
	tmp := filepath.Join(t.TempDir(), "test.db")
	database, err := Open(tmp)
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	t.Cleanup(func() { database.Close() })
	return database
}

func TestNotesRepo(t *testing.T) {
	database := setupDB(t)
	repo := NewNotesRepo(database)

	n := &Note{Title: "Test Note", Content: "Body content"}
	if err := repo.Create(n); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if n.ID == 0 {
		t.Error("expected ID to be set")
	}

	list, err := repo.List()
	if err != nil {
		t.Fatalf("List: %v", err)
	}
	if len(list) != 1 || list[0].Title != "Test Note" {
		t.Errorf("List: got %v", list)
	}

	got, err := repo.Get(n.ID)
	if err != nil || got == nil || got.Title != "Test Note" {
		t.Errorf("Get: err=%v got=%v", err, got)
	}

	n.Title = "Updated"
	if err := repo.Update(n); err != nil {
		t.Fatalf("Update: %v", err)
	}
	got, _ = repo.Get(n.ID)
	if got.Title != "Updated" {
		t.Errorf("Update: got %q", got.Title)
	}

	if err := repo.Delete(n.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	got, _ = repo.Get(n.ID)
	if got != nil {
		t.Error("Delete: expected nil after delete")
	}
}

func TestTasksRepo(t *testing.T) {
	database := setupDB(t)
	repo := NewTasksRepo(database)

	task := &Task{Title: "Buy milk", Done: false}
	if err := repo.Create(task); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if task.ID == 0 {
		t.Error("expected ID to be set")
	}

	list, err := repo.List()
	if err != nil {
		t.Fatalf("List: %v", err)
	}
	if len(list) != 1 || list[0].Title != "Buy milk" || list[0].Done {
		t.Errorf("List: got %v", list)
	}

	if err := repo.ToggleDone(task.ID); err != nil {
		t.Fatalf("ToggleDone: %v", err)
	}
	got, _ := repo.Get(task.ID)
	if !got.Done {
		t.Error("ToggleDone: expected Done=true")
	}

	if err := repo.Delete(task.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	list, _ = repo.List()
	if len(list) != 0 {
		t.Errorf("List after delete: got %d items", len(list))
	}
}

func TestContactsRepo(t *testing.T) {
	database := setupDB(t)
	repo := NewContactsRepo(database)

	c := &Contact{Name: "Alice", Email: "alice@example.com", Phone: "555-1234"}
	if err := repo.Create(c); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if c.ID == 0 {
		t.Error("expected ID to be set")
	}

	list, err := repo.List()
	if err != nil {
		t.Fatalf("List: %v", err)
	}
	if len(list) != 1 || list[0].Name != "Alice" {
		t.Errorf("List: got %v", list)
	}

	c.Name = "Alice Smith"
	if err := repo.Update(c); err != nil {
		t.Fatalf("Update: %v", err)
	}
	got, _ := repo.Get(c.ID)
	if got.Name != "Alice Smith" {
		t.Errorf("Update: got Name %q", got.Name)
	}

	if err := repo.Delete(c.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	got, _ = repo.Get(c.ID)
	if got != nil {
		t.Error("Delete: expected nil after delete")
	}
}

func TestCalendarRepo(t *testing.T) {
	database := setupDB(t)
	repo := NewCalendarRepo(database)
	now := time.Now()
	start := time.Date(now.Year(), now.Month(), 15, 9, 0, 0, 0, time.Local)
	end := start.Add(time.Hour)

	e := &Event{Title: "Meeting", Description: "Team sync", StartAt: start, EndAt: end}
	if err := repo.Create(e); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if e.ID == 0 {
		t.Error("expected ID to be set")
	}

	events, err := repo.ListByMonth(now.Year(), int(now.Month()))
	if err != nil {
		t.Fatalf("ListByMonth: %v", err)
	}
	if len(events) != 1 || events[0].Title != "Meeting" {
		t.Errorf("ListByMonth: got %v", events)
	}

	dayEvents, err := repo.ListByDay(now.Year(), int(now.Month()), 15)
	if err != nil {
		t.Fatalf("ListByDay: %v", err)
	}
	if len(dayEvents) != 1 {
		t.Errorf("ListByDay: got %d events", len(dayEvents))
	}

	if err := repo.Delete(e.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	got, _ := repo.Get(e.ID)
	if got != nil {
		t.Error("Delete: expected nil after delete")
	}
}

func TestOpen(t *testing.T) {
	tmp := filepath.Join(t.TempDir(), "open.db")
	database, err := Open(tmp)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	database.Close()
}
