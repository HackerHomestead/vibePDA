package ui

import (
	"github.com/charmbracelet/lipgloss"
)

// 80's-inspired shared styles (amber accent, sharp borders)
var (
	TitleStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(lipgloss.Color(ColorTitleFg)).
			Background(lipgloss.Color(ColorTitleBg)).
			Padding(0, 1)

	SidebarStyle = lipgloss.NewStyle().
			Border(RetroBorder).
			BorderForeground(lipgloss.Color(ColorBorder)).
			Padding(0, 1).
			MarginRight(1)

	SidebarWidth = 18

	MainStyle = lipgloss.NewStyle().
			Border(RetroBorder).
			BorderForeground(lipgloss.Color(ColorBorder)).
			Padding(0, 1)

	SelectedStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color(ColorTitleFg)).
			Background(lipgloss.Color(ColorAccent)).
			Padding(0, 1)

	UnselectedStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color(ColorText)).
			Padding(0, 1)

	HelpStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color(ColorTextDim)).
			Padding(0, 1)

	PlaceholderStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color(ColorTextMuted)).
				Italic(true)

	// EmptyStateStyle is used for "No items" / "No events" etc. in list views.
	EmptyStateStyle = lipgloss.NewStyle().Foreground(lipgloss.Color(ColorTextDim))
)

// EmptyStateView renders a dim empty-state message (e.g. "No events.").
func EmptyStateView(message string) string {
	return EmptyStateStyle.Render(message)
}
