package ui

import (
	"time"
)

// PrettyTime formats t as "Today 6pm", "Yesterday 4:30am", "Mon Jan 2 3pm", etc.
func PrettyTime(t time.Time) string {
	now := time.Now()
	today := time.Date(now.Year(), now.Month(), now.Day(), 0, 0, 0, 0, now.Location())
	yesterday := today.AddDate(0, 0, -1)
	tt := time.Date(t.Year(), t.Month(), t.Day(), 0, 0, 0, 0, t.Location())

	if tt.Equal(today) {
		return "Today " + t.Format("3:04PM")
	}
	if tt.Equal(yesterday) {
		return "Yesterday " + t.Format("3:04AM")
	}
	if now.Year() == t.Year() {
		return t.Format("Mon Jan 2 3:04PM")
	}
	return t.Format("Jan 2, 2006 3:04PM")
}
