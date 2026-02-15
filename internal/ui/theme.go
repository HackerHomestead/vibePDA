// Package ui theme: 80's terminal aesthetic for modern terminals.
//
// Palette: Amber/gold accent (CRT phosphor), sharp single-line borders (no rounded),
// high contrast. Works at 80x24 and scales to HD. All colors are xterm 256 for
// broad terminal support.
package ui

import (
	"github.com/charmbracelet/lipgloss"
)

// 80's-inspired xterm 256 palette
const (
	// Accent: phosphor green (CRT terminal, high contrast with white)
	ColorAccent = "10"
	// Accent dim (borders when focused)
	ColorAccentDim = "28"
	// Status bar: deep blue (DOS/Norton Commander style)
	ColorStatusBg = "17"
	// Title bar and selection: inverted (white text on accent bg)
	ColorTitleFg = "15"
	ColorTitleBg = "10"
	// Borders: gray (unfocused)
	ColorBorder = "240"
	// Text
	ColorText      = "252"
	ColorTextDim   = "241"
	ColorTextMuted = "245"
	// Selection background
	ColorSelectBg = "236"
	// Error
	ColorError = "9"
)

// Border styles: single-line (retro), double when focused (classic 80s)
var (
	RetroBorder       = lipgloss.NormalBorder()
	RetroBorderFocused = lipgloss.DoubleBorder()
)
