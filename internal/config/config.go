package config

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
)

// Config holds application configuration.
type Config struct {
	DatabasePath string `json:"database_path"`
	Editor       string `json:"editor"`
	Theme        string `json:"theme"`
	DefaultView  string `json:"default_view"`
}

// Default returns default configuration.
func Default() Config {
	home, _ := os.UserHomeDir()
	dbPath := filepath.Join(home, ".local", "share", "vibe", "vibe.db")
	return Config{
		DatabasePath: dbPath,
		Editor:       "",
		Theme:        "default",
		DefaultView:  "tasks",
	}
}

// ConfigPath returns the path to the config file (XDG-style).
func ConfigPath() string {
	if dir := os.Getenv("XDG_CONFIG_HOME"); dir != "" {
		return filepath.Join(dir, "vibe", "config.json")
	}
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "vibe", "config.json")
}

// Load reads config from JSON file, falling back to defaults.
func Load() (Config, error) {
	cfg := Default()
	path := ConfigPath()
	data, err := os.ReadFile(path)
	if err != nil {
		return cfg, nil // use defaults if file missing
	}
	if err := json.Unmarshal(data, &cfg); err != nil {
		return Default(), err
	}
	cfg.DatabasePath = expandPath(cfg.DatabasePath)
	return cfg, nil
}

func expandPath(p string) string {
	if strings.HasPrefix(p, "~/") {
		home, _ := os.UserHomeDir()
		return filepath.Join(home, p[2:])
	}
	return p
}
