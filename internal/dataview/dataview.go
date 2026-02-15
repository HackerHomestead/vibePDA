package dataview

import (
	"strings"
	"time"

	"github.com/charmbracelet/lipgloss"
)

// ColumnDef defines a column: title and width in runes.
type ColumnDef struct {
	Title string
	Width int
}

// RenderTable renders a grid with headers and rows. Each row must have len(cols) cells.
// Cells are truncated to column width.
func RenderTable(cols []ColumnDef, rows [][]string, selectedIndex int) string {
	var b strings.Builder
	headerStyle := lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("245"))
	rowStyle := lipgloss.NewStyle().Foreground(lipgloss.Color("252"))
	selectedStyle := lipgloss.NewStyle().Foreground(lipgloss.Color("15")).Background(lipgloss.Color("62")).Padding(0, 1)

	// Header row
	parts := make([]string, len(cols))
	for i, c := range cols {
		parts[i] = truncate(c.Title, c.Width)
	}
	b.WriteString(headerStyle.Render(strings.Join(parts, " ")) + "\n")

	for rowIdx, row := range rows {
		if len(row) != len(cols) {
			continue
		}
		parts := make([]string, len(cols))
		for i, c := range cols {
			parts[i] = truncate(row[i], c.Width)
		}
		line := strings.Join(parts, " ")
		if rowIdx == selectedIndex {
			line = selectedStyle.Render(line)
		} else {
			line = rowStyle.Render(line)
		}
		b.WriteString(line + "\n")
	}
	return b.String()
}

// truncate shortens s to n runes; appends "…" if truncated.
func truncate(s string, n int) string {
	runes := []rune(s)
	if len(runes) <= n {
		return s
	}
	if n <= 0 {
		return ""
	}
	return string(runes[:n-1]) + "…"
}

// FormatDate formats a time for display in table (short).
func FormatDate(t *time.Time) string {
	if t == nil {
		return ""
	}
	return t.Format("2006-01-02 15:04")
}
