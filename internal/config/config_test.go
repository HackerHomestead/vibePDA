package config

import (
	"os"
	"path/filepath"
	"testing"
)

func TestDefault(t *testing.T) {
	cfg := Default()
	if cfg.DatabasePath == "" {
		t.Error("Default: DatabasePath should be set")
	}
	if cfg.DefaultView != "tasks" {
		t.Errorf("Default: DefaultView want tasks, got %q", cfg.DefaultView)
	}
	if cfg.Theme != "default" {
		t.Errorf("Default: Theme want default, got %q", cfg.Theme)
	}
}

func TestConfigPath(t *testing.T) {
	path := ConfigPath()
	if path == "" {
		t.Error("ConfigPath: should return non-empty path")
	}
	if filepath.Ext(path) != ".json" {
		t.Errorf("ConfigPath: expected .json extension, got %q", path)
	}
}

func TestLoad_MissingFile(t *testing.T) {
	// Load when file doesn't exist should return defaults
	orig := os.Getenv("XDG_CONFIG_HOME")
	os.Setenv("XDG_CONFIG_HOME", t.TempDir())
	defer os.Setenv("XDG_CONFIG_HOME", orig)

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load (missing file): %v", err)
	}
	if cfg.DefaultView != "tasks" {
		t.Errorf("Load (missing): expected defaults, got DefaultView=%q", cfg.DefaultView)
	}
}

func TestLoad_ValidFile(t *testing.T) {
	dir := t.TempDir()
	os.Setenv("XDG_CONFIG_HOME", dir)
	defer func() {
		os.Unsetenv("XDG_CONFIG_HOME")
	}()

	configDir := filepath.Join(dir, "vibe")
	if err := os.MkdirAll(configDir, 0755); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	path := filepath.Join(configDir, "config.json")
	data := []byte(`{"default_view": "notes", "database_path": "~/data/vibe.db"}`)
	if err := os.WriteFile(path, data, 0644); err != nil {
		t.Fatalf("write config: %v", err)
	}

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if cfg.DefaultView != "notes" {
		t.Errorf("Load: want default_view=notes, got %q", cfg.DefaultView)
	}
	if cfg.DatabasePath == "" {
		t.Error("Load: DatabasePath should be expanded")
	}
}

func TestLoad_InvalidJSON(t *testing.T) {
	dir := t.TempDir()
	os.Setenv("XDG_CONFIG_HOME", dir)
	defer func() {
		os.Unsetenv("XDG_CONFIG_HOME")
	}()

	configDir := filepath.Join(dir, "vibe")
	os.MkdirAll(configDir, 0755)
	path := filepath.Join(configDir, "config.json")
	os.WriteFile(path, []byte(`{invalid json`), 0644)

	_, err := Load()
	if err == nil {
		t.Error("Load (invalid JSON): expected error")
	}
}

func TestExpandPath(t *testing.T) {
	home, _ := os.UserHomeDir()
	tests := []struct {
		in   string
		want string
	}{
		{"~/foo", filepath.Join(home, "foo")},
		{"~/foo/bar", filepath.Join(home, "foo", "bar")},
		{"/absolute", "/absolute"},
	}
	for _, tt := range tests {
		got := expandPath(tt.in)
		if got != tt.want {
			t.Errorf("expandPath(%q) = %q, want %q", tt.in, got, tt.want)
		}
	}
}
