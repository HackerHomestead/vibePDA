package main

import (
	"fmt"
	"log"
	"os"
	"path/filepath"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/you/vibe/internal/app"
	"github.com/you/vibe/internal/config"
	"github.com/you/vibe/internal/db"
)

const helpText = `vibePDA - A terminal personal data assistant

Usage:
  vibePDA [options]

Options:
  -v, --version    Show build/version and exit
  -h, --help       Show this help and exit

Environment:
  VIBE_DB          Override database path (e.g. ./vibe-demo.db for demo)

Modules: Notes, Tasks, Contacts, Calendar, Trash
Key bindings shown in status bar. See README.md for full documentation.
`

func main() {
	for _, arg := range os.Args[1:] {
		switch arg {
		case "-v", "--version":
			ver := app.BuildNumber
			if ver == "" {
				ver = "dev"
			}
			fmt.Println("vibePDA", ver)
			os.Exit(0)
		case "-h", "-help", "--help":
			fmt.Print(helpText)
			os.Exit(0)
		}
	}
	cfg, err := config.Load()
	if err != nil {
		log.Printf("config load warning: %v (using defaults)", err)
	}

	// Ensure DB directory exists
	dbDir := filepath.Dir(cfg.DatabasePath)
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		log.Fatalf("create db dir: %v", err)
	}

	database, err := db.Open(cfg.DatabasePath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	defer database.Close()

	calendarRepo := db.NewCalendarRepo(database)
	tasksRepo := db.NewTasksRepo(database)
	notesRepo := db.NewNotesRepo(database)
	contactsRepo := db.NewContactsRepo(database)
	m := app.New(cfg.DefaultView, calendarRepo, tasksRepo, notesRepo, contactsRepo)
	p := tea.NewProgram(m, tea.WithAltScreen())

	if _, err := p.Run(); err != nil {
		log.Fatal(err)
	}
}
