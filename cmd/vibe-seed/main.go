// vibe-seed populates a database with demo data from external files.
// Used by "make build-demo". Loads contacts, notes, tasks, events from demo/.
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/you/vibe/internal/db"
)

const countPerType = 100

type themeConfig struct {
	Name             string `json:"name"`
	EmailDomain      string `json:"email_domain"`
	NoteContent      string `json:"note_content"`
	EventDescription string `json:"event_description"`
}

func main() {
	demoDir := flag.String("demo-dir", "demo", "directory containing contacts.txt, notes.txt, tasks.txt, events.txt, theme.json")
	dbPath := flag.String("db", "vibe-demo.db", "path to demo database (created in current dir)")
	flag.Parse()

	abs, err := filepath.Abs(*dbPath)
	if err != nil {
		log.Fatalf("resolve path: %v", err)
	}
	cwd, err := os.Getwd()
	if err != nil {
		log.Fatalf("getwd: %v", err)
	}
	cwdAbs, _ := filepath.Abs(cwd)
	if filepath.Dir(abs) != cwdAbs {
		log.Fatalf("refusing to use db outside current directory (would not be removed by make clean): %s", abs)
	}

	// Remove db and any SQLite WAL/shm files (left behind if previous run used WAL mode)
	_ = os.Remove(abs)
	_ = os.Remove(abs + "-wal")
	_ = os.Remove(abs + "-shm")
	dbDir := filepath.Dir(abs)
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		log.Fatalf("create db dir: %v", err)
	}

	contacts, err := loadLines(filepath.Join(*demoDir, "contacts.txt"))
	if err != nil {
		log.Fatalf("load contacts: %v", err)
	}
	notes, err := loadLines(filepath.Join(*demoDir, "notes.txt"))
	if err != nil {
		log.Fatalf("load notes: %v", err)
	}
	tasks, err := loadLines(filepath.Join(*demoDir, "tasks.txt"))
	if err != nil {
		log.Fatalf("load tasks: %v", err)
	}
	events, err := loadLines(filepath.Join(*demoDir, "events.txt"))
	if err != nil {
		log.Fatalf("load events: %v", err)
	}
	theme, err := loadTheme(filepath.Join(*demoDir, "theme.json"))
	if err != nil {
		log.Fatalf("load theme: %v", err)
	}
	if theme.EmailDomain == "" {
		theme.EmailDomain = "pawnee.in.gov"
	}
	if theme.NoteContent == "" {
		theme.NoteContent = "Demo content. Treat yo self."
	}
	if theme.EventDescription == "" {
		theme.EventDescription = "Demo event. Everything is cccccool."
	}

	database, err := db.Open(abs)
	if err != nil {
		if isDBLockedOrIOErr(err) {
			log.Fatalf("Cannot open database: %v\n\n  The database may be in use by another vibePDA instance.\n  Close all running instances and try again.", err)
		}
		log.Fatalf("open db: %v", err)
	}
	defer database.Close()

	calendarRepo := db.NewCalendarRepo(database)
	tasksRepo := db.NewTasksRepo(database)
	notesRepo := db.NewNotesRepo(database)
	contactsRepo := db.NewContactsRepo(database)

	now := time.Now()
	baseDate := time.Date(now.Year(), now.Month(), 1, 0, 0, 0, 0, time.Local)

	for i := 0; i < countPerType; i++ {
		name := pick(contacts, i)
		c := &db.Contact{
			Name:  name,
			Email: fmt.Sprintf("%s@%s", toEmailPart(name), theme.EmailDomain),
			Phone: fmt.Sprintf("555-%04d", 1000+i%9000),
			Notes: "Demo contact",
		}
		if err := contactsRepo.Create(c); err != nil {
			log.Fatalf("create contact: %v", err)
		}
	}
	log.Printf("created %d contacts", countPerType)

	for i := 0; i < countPerType; i++ {
		title := pick(notes, i)
		n := &db.Note{
			Title:   title,
			Content: theme.NoteContent,
		}
		if err := notesRepo.Create(n); err != nil {
			log.Fatalf("create note: %v", err)
		}
	}
	log.Printf("created %d notes", countPerType)

	for i := 0; i < countPerType; i++ {
		title := pick(tasks, i)
		due := baseDate.AddDate(0, 0, i%60)
		t := &db.Task{
			Title:    title,
			Done:     i%5 == 0,
			DueDate:  &due,
			Priority: i % 5,
		}
		if err := tasksRepo.Create(t); err != nil {
			log.Fatalf("create task: %v", err)
		}
	}
	log.Printf("created %d tasks", countPerType)

	for i := 0; i < countPerType; i++ {
		title := pick(events, i)
		day := 1 + (i * 3) % 28
		month := int(baseDate.Month()) + (i/30)%3
		if month > 12 {
			month -= 12
		}
		year := baseDate.Year()
		if month < int(baseDate.Month()) {
			year++
		}
		start := time.Date(year, time.Month(month), day, 9+(i%8), (i*7)%60, 0, 0, time.Local)
		end := start.Add(time.Duration(1+i%3) * time.Hour)
		e := &db.Event{
			Title:       title,
			Description: theme.EventDescription,
			StartAt:     start,
			EndAt:       end,
			AllDay:      i%7 == 0,
		}
		if err := calendarRepo.Create(e); err != nil {
			log.Fatalf("create event: %v", err)
		}
	}
	log.Printf("created %d events", countPerType)

	log.Printf("demo database ready: %s (%s theme)", abs, theme.Name)
}

func loadLines(path string) ([]string, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	var lines []string
	s := bufio.NewScanner(f)
	for s.Scan() {
		t := strings.TrimSpace(s.Text())
		if t != "" {
			lines = append(lines, t)
		}
	}
	if err := s.Err(); err != nil {
		return nil, err
	}
	if len(lines) == 0 {
		return nil, fmt.Errorf("no entries in %s", path)
	}
	return lines, nil
}

func loadTheme(path string) (*themeConfig, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return &themeConfig{}, nil // optional; use defaults
	}
	var t themeConfig
	if err := json.Unmarshal(data, &t); err != nil {
		return nil, err
	}
	return &t, nil
}

func pick(list []string, index int) string {
	s := list[index%len(list)]
	if index >= len(list) {
		return fmt.Sprintf("%s (%d)", s, index)
	}
	return s
}

// isDBLockedOrIOErr reports whether err is a SQLite disk I/O or database-locked error.
func isDBLockedOrIOErr(err error) bool {
	if err == nil {
		return false
	}
	s := strings.ToLower(err.Error())
	return strings.Contains(s, "disk i/o") ||
		strings.Contains(s, "database is locked") ||
		strings.Contains(s, "database locked") ||
		strings.Contains(s, "5898") ||
		strings.Contains(s, "sqlite_busy") ||
		strings.Contains(s, "io error")
}

func toEmailPart(name string) string {
	var b []byte
	for _, r := range name {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') {
			b = append(b, byte(r))
		} else if r == ' ' {
			b = append(b, '.')
		}
	}
	if len(b) == 0 {
		return "contact"
	}
	return strings.ToLower(string(b))
}
