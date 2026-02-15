package main

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"

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
  -r, --regenerate Wipe database and recreate with current schema, then exit
  -u, --upgrade    Export data, upgrade schema, re-import (preserves user data)
  -e, --export     Export to JSON file, then exit
  -i, --import     Import from JSON file, then exit

Environment:
  VIBE_DB          Override database path (e.g. ./vibe-demo.db for demo)

Modules: Notes, Tasks, Contacts, Calendar, Trash
Key bindings shown in status bar. See README.md for full documentation.
`

func main() {
	var exportPath, importPath string
	for i := 1; i < len(os.Args); i++ {
		arg := os.Args[i]
		switch arg {
		case "-r", "--regenerate":
			runRegenerate()
			return
		case "-u", "--upgrade":
			runUpgrade()
			return
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
		case "-e", "--export":
			if i+1 < len(os.Args) && !strings.HasPrefix(os.Args[i+1], "-") {
				exportPath = os.Args[i+1]
				i++
			}
		case "-i", "--import":
			if i+1 < len(os.Args) && !strings.HasPrefix(os.Args[i+1], "-") {
				importPath = os.Args[i+1]
				i++
			}
		default:
			if strings.HasPrefix(arg, "--export=") {
				exportPath = strings.TrimPrefix(arg, "--export=")
			} else if strings.HasPrefix(arg, "-e=") {
				exportPath = strings.TrimPrefix(arg, "-e=")
			} else if strings.HasPrefix(arg, "--import=") {
				importPath = strings.TrimPrefix(arg, "--import=")
			} else if strings.HasPrefix(arg, "-i=") {
				importPath = strings.TrimPrefix(arg, "-i=")
			}
		}
	}
	if exportPath != "" {
		runExport(exportPath)
		return
	}
	if importPath != "" {
		runImport(importPath)
		return
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

func runRegenerate() {
	cfg, err := config.Load()
	if err != nil {
		log.Printf("config load warning: %v (using defaults)", err)
	}
	dbPath := cfg.DatabasePath

	if err := os.Remove(dbPath); err != nil && !os.IsNotExist(err) {
		log.Fatalf("remove database: %v", err)
	}

	dbDir := filepath.Dir(dbPath)
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		log.Fatalf("create db dir: %v", err)
	}

	database, err := db.Open(dbPath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	database.Close()

	fmt.Printf("Database regenerated: %s\n", dbPath)
	fmt.Println("Run without --regenerate to start.")
}

func runUpgrade() {
	cfg, err := config.Load()
	if err != nil {
		log.Printf("config load warning: %v (using defaults)", err)
	}
	dbPath := cfg.DatabasePath

	// Open existing db and export
	database, err := db.Open(dbPath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	data, err := db.Export(database)
	database.Close()
	if err != nil {
		log.Fatalf("export data: %v", err)
	}
	count := len(data.Notes) + len(data.Tasks) + len(data.Contacts) + len(data.Events)
	log.Printf("exported %d records", count)

	// Remove and recreate
	if err := os.Remove(dbPath); err != nil && !os.IsNotExist(err) {
		log.Fatalf("remove database: %v", err)
	}
	dbDir := filepath.Dir(dbPath)
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		log.Fatalf("create db dir: %v", err)
	}
	database, err = db.Open(dbPath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	if err := db.Import(database, data); err != nil {
		database.Close()
		log.Fatalf("import data: %v", err)
	}
	database.Close()

	fmt.Printf("Database upgraded: %s (%d records preserved)\n", dbPath, count)
	fmt.Println("Run without --upgrade to start.")
}

func runExport(path string) {
	cfg, err := config.Load()
	if err != nil {
		log.Printf("config load warning: %v (using defaults)", err)
	}
	database, err := db.Open(cfg.DatabasePath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	defer database.Close()
	data, err := db.Export(database)
	if err != nil {
		log.Fatalf("export: %v", err)
	}
	raw, err := db.ExportToJSON(data)
	if err != nil {
		log.Fatalf("encode json: %v", err)
	}
	if err := os.WriteFile(path, raw, 0644); err != nil {
		log.Fatalf("write %s: %v", path, err)
	}
	fmt.Printf("Exported to %s\n", path)
}

func runImport(path string) {
	cfg, err := config.Load()
	if err != nil {
		log.Printf("config load warning: %v (using defaults)", err)
	}
	raw, err := os.ReadFile(path)
	if err != nil {
		log.Fatalf("read %s: %v", path, err)
	}
	data, err := db.ImportFromJSON(raw)
	if err != nil {
		log.Fatalf("decode json: %v", err)
	}
	// Replace database with imported data (regenerate then import)
	dbPath := cfg.DatabasePath
	if err := os.Remove(dbPath); err != nil && !os.IsNotExist(err) {
		log.Fatalf("remove database: %v", err)
	}
	dbDir := filepath.Dir(dbPath)
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		log.Fatalf("create db dir: %v", err)
	}
	database, err := db.Open(dbPath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	defer database.Close()
	if err := db.Import(database, data); err != nil {
		log.Fatalf("import: %v", err)
	}
	count := len(data.Notes) + len(data.Tasks) + len(data.Contacts) + len(data.Events)
	fmt.Printf("Imported %d records from %s\n", count, path)
}
