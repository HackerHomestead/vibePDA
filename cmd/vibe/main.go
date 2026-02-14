package main

import (
	"log"
	"os"
	"path/filepath"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/you/vibe/internal/app"
	"github.com/you/vibe/internal/config"
	"github.com/you/vibe/internal/db"
)

func main() {
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
	m := app.New(cfg.DefaultView, calendarRepo, tasksRepo)
	p := tea.NewProgram(m, tea.WithAltScreen())

	if _, err := p.Run(); err != nil {
		log.Fatal(err)
	}
}
