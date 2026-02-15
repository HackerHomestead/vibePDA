// md2txt converts Markdown to plain ASCII text (strip formatting, keep structure).
// Usage: go run scripts/md2txt.go [input.md ...]
// Writes to same path with .txt extension.
package main

import (
	"bufio"
	"fmt"
	"os"
	"regexp"
	"strings"
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

var (
	reHeader  = regexp.MustCompile(`^#{1,6}\s+`)
	reBold    = regexp.MustCompile(`\*\*([^*]+)\*\*|__([^_]+)__`)
	reItalic  = regexp.MustCompile(`\*([^*]+)\*|_([^_]+)_`)
	reLink    = regexp.MustCompile(`\[([^\]]+)\]\(([^)]+)\)`)
	reCode    = regexp.MustCompile("`([^`]+)`")
	reCodeFence = regexp.MustCompile("^```")
)

func convert(path string) error {
	data, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	outPath := strings.TrimSuffix(path, ".md") + ".txt"
	inCodeBlock := false
	var out strings.Builder
	sc := bufio.NewScanner(strings.NewReader(string(data)))
	for sc.Scan() {
		line := sc.Text()
		trimmed := strings.TrimSpace(line)
		// Code fence: ``` or ```lang
		if reCodeFence.MatchString(trimmed) {
			if inCodeBlock {
				inCodeBlock = false
				out.WriteString("\n")
			} else {
				inCodeBlock = true
			}
			continue
		}
		if inCodeBlock {
			out.WriteString(line)
			out.WriteByte('\n')
			continue
		}
		// Horizontal rule
		if trimmed == "---" || trimmed == "***" || trimmed == "___" {
			out.WriteString("--------------------------------------------------------------------------------\n")
			continue
		}
		// Header: strip # and keep text
		if strings.HasPrefix(trimmed, "#") {
			line = reHeader.ReplaceAllString(line, "")
		}
		// Table: simplify (strip | and join with spaces); skip separator row (|---|---|)
		if strings.HasPrefix(trimmed, "|") {
			if isTableSeparator(trimmed) {
				continue
			}
			parts := splitTableRow(line)
			line = strings.Join(parts, "  ")
		}
		line = reBold.ReplaceAllString(line, "$1$2")
		line = reItalic.ReplaceAllString(line, "$1$2")
		line = reLink.ReplaceAllString(line, "$1 ($2)")
		line = reCode.ReplaceAllString(line, "$1")
		line = strings.ReplaceAll(line, "&lt;", "<")
		line = strings.ReplaceAll(line, "&gt;", ">")
		out.WriteString(strings.TrimSpace(line))
		if line != "" {
			out.WriteByte('\n')
		}
	}
	if err := sc.Err(); err != nil {
		return err
	}
	return os.WriteFile(outPath, []byte(strings.TrimRight(out.String(), "\n")+"\n"), 0644)
}

func isTableSeparator(s string) bool {
	s = strings.Trim(s, "| ")
	return strings.Trim(s, "-: \t") == ""
}

func splitTableRow(line string) []string {
	line = strings.Trim(line, "| ")
	var parts []string
	for _, s := range strings.Split(line, "|") {
		parts = append(parts, strings.TrimSpace(s))
	}
	return parts
}
