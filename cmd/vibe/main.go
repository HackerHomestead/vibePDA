package main

import (
	"log"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/you/vibe/internal/app"
	"github.com/you/vibe/internal/config"
)

func main() {
	cfg, err := config.Load()
	if err != nil {
		log.Printf("config load warning: %v (using defaults)", err)
	}

	m := app.New(cfg.DefaultView)
	p := tea.NewProgram(m, tea.WithAltScreen())

	if _, err := p.Run(); err != nil {
		log.Fatal(err)
	}
}
