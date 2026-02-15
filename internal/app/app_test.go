package app

import (
	"testing"
)

func TestTruncateToWidth(t *testing.T) {
	tests := []struct {
		s   string
		max int
		want string
	}{
		{"short", 10, "short"},
		{"exactly10!", 10, "exactly10!"},
		{"way too long string", 10, "way too l…"},
		{"日本語テスト", 5, "日本語テ…"},
		{"a", 0, "a"},
		{"", 5, ""},
	}
	for _, tt := range tests {
		got := truncateToWidth(tt.s, tt.max)
		if got != tt.want {
			t.Errorf("truncateToWidth(%q, %d) = %q, want %q", tt.s, tt.max, got, tt.want)
		}
	}
}

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
		{"trash", 4},
		{"Trash", 4},
		{"TRASH", 4},
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
