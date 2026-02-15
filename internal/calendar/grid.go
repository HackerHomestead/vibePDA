package calendar

import (
	"fmt"
	"time"

	"github.com/charmbracelet/lipgloss"
)

// RenderMonthGrid renders a 7-column month grid for the given year/month
// with today and the selected day highlighted. Uses package-level dayStyle,
// todayStyle, and selectedStyle.
func RenderMonthGrid(year int, month time.Month, day int) string {
	first := time.Date(year, month, 1, 0, 0, 0, 0, time.Local)
	last := first.AddDate(0, 1, -1)
	startWeekday := int(first.Weekday()) // 0=Sun
	daysInMonth := last.Day()

	weekdays := []string{"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}
	header := ""
	for _, w := range weekdays {
		header += dayStyle.Render(w) + " "
	}
	header = lipgloss.NewStyle().Foreground(lipgloss.Color("245")).Render(header) + "\n"

	rows := ""
	col := 0
	for i := 0; i < startWeekday; i++ {
		rows += dayStyle.Render("") + " "
		col++
	}

	now := time.Now()
	for d := 1; d <= daysInMonth; d++ {
		s := fmt.Sprintf("%2d", d)
		cell := dayStyle.Render(s)
		isToday := now.Year() == year && now.Month() == month && now.Day() == d
		isSelected := day == d
		if isToday {
			cell = todayStyle.Render(s)
		} else if isSelected {
			cell = selectedStyle.Render(s)
		}
		rows += cell + " "
		col++
		if col%7 == 0 {
			rows += "\n"
		}
	}

	return header + rows
}
