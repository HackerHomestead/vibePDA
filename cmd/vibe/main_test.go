package main

import (
	"os"
	"os/exec"
	"strings"
	"testing"
)

func TestHelp(t *testing.T) {
	// Build and run with --help
	cmd := exec.Command("go", "run", ".", "--help")
	cmd.Dir = "."
	cmd.Env = append(os.Environ(), "VIBE_DB=") // avoid touching user db
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("run --help: %v\n%s", err, out)
	}
	s := string(out)
	if !strings.Contains(s, "vibePDA") {
		t.Errorf("help output should contain 'vibePDA', got:\n%s", s)
	}
	if !strings.Contains(s, "-h") || !strings.Contains(s, "--help") {
		t.Errorf("help output should mention -h/--help, got:\n%s", s)
	}
}

func TestVersion(t *testing.T) {
	cmd := exec.Command("go", "run", ".", "-v")
	cmd.Dir = "."
	cmd.Env = append(os.Environ(), "VIBE_DB=")
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("run -v: %v\n%s", err, out)
	}
	s := strings.TrimSpace(string(out))
	if !strings.HasPrefix(s, "vibePDA") {
		t.Errorf("version output should start with 'vibePDA', got: %q", s)
	}
}
