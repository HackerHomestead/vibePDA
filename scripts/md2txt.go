// md2txt converts Markdown to plain ASCII following Unix documentation
// conventions: 72-character line width, wrapped paragraphs, clear section
// headers, indented code and lists.
//
// Usage: go run scripts/md2txt.go [input.md ...]
// Writes to same path with .txt extension.
package main

import (
	"fmt"
	"os"
	"regexp"
	"strings"
	"unicode"
)

const lineWidth = 72

var (
	reHeader     = regexp.MustCompile(`^#{1,6}\s+`)
	reBold       = regexp.MustCompile(`\*\*([^*]+)\*\*|__([^_]+)__`)
	reItalic     = regexp.MustCompile(`\*([^*]+)\*|_([^_]+)_`)
	reLink       = regexp.MustCompile(`\[([^\]]+)\]\(([^)]+)\)`)
	reCode       = regexp.MustCompile("`([^`]+)`")
	reCodeFence  = regexp.MustCompile("^```")
	reListPrefix = regexp.MustCompile(`^(\s*[-*+]|\s*\d+\.)\s+`)
)

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: go run scripts/md2txt.go <file.md> [file2.md ...]")
		os.Exit(1)
	}
	for _, path := range os.Args[1:] {
		if err := convert(path); err != nil {
			fmt.Fprintf(os.Stderr, "%s: %v\n", path, err)
			os.Exit(1)
		}
	}
}

func convert(path string) error {
	data, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	outPath := strings.TrimSuffix(path, ".md") + ".txt"
	lines := strings.Split(string(data), "\n")
	var out strings.Builder
	inCodeBlock := false
	var paragraph []string
	flushParagraph := func() {
		if len(paragraph) == 0 {
			return
		}
		text := strings.Join(paragraph, " ")
		paragraph = paragraph[:0]
		for _, w := range wrap(text, lineWidth) {
			out.WriteString(w)
			out.WriteByte('\n')
		}
	}

	for i := 0; i < len(lines); i++ {
		line := lines[i]
		trimmed := strings.TrimSpace(line)
		raw := line

		if reCodeFence.MatchString(trimmed) {
			flushParagraph()
			if inCodeBlock {
				inCodeBlock = false
				out.WriteByte('\n')
			} else {
				inCodeBlock = true
			}
			continue
		}

		if inCodeBlock {
			flushParagraph()
			// Indent code block 4 spaces; preserve line as-is up to lineWidth
			if len(trimmed) > 0 {
				indented := "    " + trimmed
				if len(indented) > lineWidth {
					indented = indented[:lineWidth]
				}
				out.WriteString(indented)
			}
			out.WriteByte('\n')
			continue
		}

		if trimmed == "---" || trimmed == "***" || trimmed == "___" {
			flushParagraph()
			out.WriteString(strings.Repeat("-", lineWidth))
			out.WriteByte('\n')
			out.WriteByte('\n')
			continue
		}

		// Header: strip #, output as section title (uppercase), blank after
		if strings.HasPrefix(trimmed, "#") {
			flushParagraph()
			title := reHeader.ReplaceAllString(trimmed, "")
			title = stripMarkdown(title)
			title = strings.TrimSpace(strings.ToUpper(title))
			out.WriteString(title)
			out.WriteByte('\n')
			out.WriteByte('\n')
			continue
		}

		// Table: format as simple two-column or list; skip separator row
		if strings.HasPrefix(trimmed, "|") {
			flushParagraph()
			if isTableSeparator(trimmed) {
				continue
			}
			parts := splitTableRow(raw)
			// First row often header; output each cell wrapped
			rowText := strings.Join(parts, "  ")
			rowText = stripMarkdown(rowText)
			for _, w := range wrap(rowText, lineWidth) {
				out.WriteString(w)
				out.WriteByte('\n')
			}
			continue
		}

		// List item: output "  - " (or same-length) then wrapped body; continuation indented
		if prefix := reListPrefix.FindString(raw); prefix != "" {
			flushParagraph()
			body := strings.TrimSpace(raw[len(prefix):])
			body = stripMarkdown(body)
			lines := wrap(body, lineWidth-4)
			for j, w := range lines {
				if j == 0 {
					out.WriteString("  - ")
				} else {
					out.WriteString("    ")
				}
				out.WriteString(w)
				out.WriteByte('\n')
			}
			continue
		}

		// Blank line: end paragraph
		if trimmed == "" {
			flushParagraph()
			out.WriteByte('\n')
			continue
		}

		// Ordinary paragraph line
		line = stripMarkdown(trimmed)
		if line != "" {
			paragraph = append(paragraph, line)
		}
	}
	flushParagraph()

	text := strings.TrimRight(out.String(), "\n")
	if text != "" && !strings.HasSuffix(text, "\n") {
		text += "\n"
	}
	return os.WriteFile(outPath, []byte(text), 0644)
}

func stripMarkdown(s string) string {
	s = reBold.ReplaceAllString(s, "$1$2")
	s = reItalic.ReplaceAllString(s, "$1$2")
	s = reLink.ReplaceAllString(s, "$1 ($2)")
	s = reCode.ReplaceAllString(s, "$1")
	s = strings.ReplaceAll(s, "&lt;", "<")
	s = strings.ReplaceAll(s, "&gt;", ">")
	return s
}

// wrap breaks text into lines of at most maxWidth runes (content only, no indent).
func wrap(text string, maxWidth int) []string {
	if maxWidth <= 0 {
		maxWidth = lineWidth
	}
	words := fields(text)
	if len(words) == 0 {
		return nil
	}
	var lines []string
	var current string
	runesUsed := 0

	for _, w := range words {
		wlen := len([]rune(w))
		need := wlen
		if runesUsed > 0 {
			need++
		}
		if runesUsed+need > maxWidth && runesUsed > 0 {
			lines = append(lines, current)
			current = w
			runesUsed = wlen
		} else {
			if runesUsed > 0 {
				current += " "
				runesUsed++
			}
			current += w
			runesUsed += wlen
		}
	}
	if current != "" {
		lines = append(lines, current)
	}
	return lines
}

func fields(s string) []string {
	var f []string
	start := -1
	for i, r := range s {
		if unicode.IsSpace(r) {
			if start >= 0 {
				f = append(f, s[start:i])
				start = -1
			}
		} else if start < 0 {
			start = i
		}
	}
	if start >= 0 {
		f = append(f, s[start:])
	}
	return f
}

func isTableSeparator(s string) bool {
	// Remove pipes and outer spaces; row is separator if only dashes, colons, spaces
	s = strings.ReplaceAll(s, "|", " ")
	s = strings.TrimSpace(s)
	if s == "" {
		return false
	}
	for _, r := range s {
		if r != '-' && r != ':' && r != ' ' && r != '\t' {
			return false
		}
	}
	return strings.Contains(s, "-")
}

func splitTableRow(line string) []string {
	line = strings.Trim(line, "| ")
	var parts []string
	for _, s := range strings.Split(line, "|") {
		parts = append(parts, strings.TrimSpace(s))
	}
	return parts
}
