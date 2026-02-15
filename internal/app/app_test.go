package app

import (
	"testing"
)

func TestIndexForView(t *testing.T) {
	tests := []struct {
		name string
		want int
	}{
		{"notes", 0},
		{"Notes", 0},
		{"NOTES", 0},
		{"tasks", 1},
		{"Tasks", 1},
		{"contacts", 2},
		{"Contacts", 2},
		{"calendar", 3},
		{"Calendar", 3},
		{"", 1},
		{"invalid", 1},
		{"foo", 1},
	}
	for _, tt := range tests {
		got := indexForView(tt.name)
		if got != tt.want {
			t.Errorf("indexForView(%q) = %d, want %d", tt.name, got, tt.want)
		}
	}
}
