#!/usr/bin/env python3
"""
Generate PNG screenshots and animated GIF of vibePDA TUI for documentation.
Renders terminal-style UI without requiring a display.
"""

import os
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Requires Pillow: pip install Pillow")
    sys.exit(1)

# Output directory
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
OUT_DIR = REPO_ROOT / "docs" / "images"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Terminal dimensions (80x25 classic)
COLS = 80
ROWS = 25
CELL_W = 10
CELL_H = 18
PADDING = 4
BORDER = 2

# Colors (approximate terminal)
BG = (0, 0, 0)
FG = (170, 170, 170)
TITLE_BG = (0, 0, 128)
TITLE_FG = (255, 255, 255)
MENU_FG = (255, 255, 0)
SIDEBAR_SEL_BG = (0, 0, 128)
SIDEBAR_SEL_FG = (255, 255, 255)
CARD_TITLE = (100, 200, 255)
STATUS_BG = (50, 50, 50)
STATUS_FG = (200, 200, 200)

MODULES = ["Notes", "Tasks", "Contacts", "Calendar", "Facts", "Finances", "Documents", "Trash"]


def find_mono_font():
    """Find a suitable monospace font."""
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return None


def create_image(width, height):
    """Create image with terminal-like dimensions."""
    img_w = width * CELL_W + PADDING * 2 + BORDER * 2
    img_h = height * CELL_H + PADDING * 2 + BORDER * 2
    return Image.new("RGB", (img_w, img_h), color=(30, 30, 30))


def draw_frame(draw, font, font_bold, lines, start_row=0):
    """Draw text lines onto the image draw context."""
    for r, line in enumerate(lines):
        row = start_row + r
        if row >= ROWS:
            break
        y = PADDING + BORDER + row * CELL_H
        draw.text((PADDING + BORDER, y), line, font=font, fill=FG)


def render_notes_view(module_idx=0):
    """Render Notes module view."""
    img = create_image(COLS, ROWS)
    draw = ImageDraw.Draw(img)
    font_path = find_mono_font()
    font = ImageFont.truetype(font_path, 14) if font_path else ImageFont.load_default()
    font_bold = ImageFont.truetype(font_path, 14) if font_path else font

    # Title bar
    title = f" vibePDA  | {MODULES[module_idx]} "
    draw.rectangle(
        [(0, 0), (img.width, CELL_H + PADDING)],
        fill=TITLE_BG,
    )
    draw.text((PADDING + BORDER, PADDING), title, font=font, fill=TITLE_FG)

    # Menu bar
    menu = " F1 Help | F2 New | F3 Edit | F4 Delete | F5 Search | F10 Quit "
    draw.rectangle(
        [(0, CELL_H + PADDING), (img.width, 2 * CELL_H + PADDING)],
        fill=(40, 40, 40),
    )
    draw.text((PADDING + BORDER, CELL_H + PADDING), menu, font=font, fill=MENU_FG)

    # Sidebar
    content_top = 2 * CELL_H + PADDING
    for i, mod in enumerate(MODULES):
        y = content_top + (i + 1) * CELL_H
        prefix = "> " if i == module_idx else "  "
        text = prefix + mod
        if i == module_idx:
            draw.rectangle(
                [(0, y - 2), (18 * CELL_W // 2, y + CELL_H - 2)],
                fill=SIDEBAR_SEL_BG,
            )
            draw.text((PADDING + BORDER, y), text, font=font, fill=SIDEBAR_SEL_FG)
        else:
            draw.text((PADDING + BORDER, y), text, font=font, fill=FG)

    # Main content - note cards
    main_col = 20
    cards = [
        ("1  Grocery list", "Milk, eggs, bread, coffee"),
        ("2  Project ideas", "CLI tool for backups, TUI dashboard"),
        ("3  Meeting notes", "Discuss Q1 goals, budget review"),
    ]
    for i, (title, content) in enumerate(cards):
        y = content_top + (i + 1) * CELL_H
        draw.text((PADDING + BORDER + main_col * CELL_W // 2, y), title, font=font, fill=CARD_TITLE)
        draw.text((PADDING + BORDER + main_col * CELL_W // 2, y + CELL_H), content[:50], font=font, fill=FG)

    # Status bar
    status_y = (ROWS - 1) * CELL_H + PADDING
    draw.rectangle(
        [(0, status_y), (img.width, img.height)],
        fill=STATUS_BG,
    )
    draw.text((PADDING + BORDER, status_y), " :Notes=>View ", font=font, fill=STATUS_FG)

    return img


def render_tasks_view():
    """Render Tasks module view."""
    img = create_image(COLS, ROWS)
    draw = ImageDraw.Draw(img)
    font_path = find_mono_font()
    font = ImageFont.truetype(font_path, 14) if font_path else ImageFont.load_default()

    # Title bar
    title = " vibePDA  | Tasks "
    draw.rectangle([(0, 0), (img.width, CELL_H + PADDING)], fill=TITLE_BG)
    draw.text((PADDING + BORDER, PADDING), title, font=font, fill=TITLE_FG)

    # Menu bar
    menu = " F1 Help | F2 New | F3 Edit | F4 Delete | F5 Search | F10 Quit "
    draw.rectangle(
        [(0, CELL_H + PADDING), (img.width, 2 * CELL_H + PADDING)],
        fill=(40, 40, 40),
    )
    draw.text((PADDING + BORDER, CELL_H + PADDING), menu, font=font, fill=MENU_FG)

    # Sidebar - Tasks selected
    content_top = 2 * CELL_H + PADDING
    for i, mod in enumerate(MODULES):
        y = content_top + (i + 1) * CELL_H
        prefix = "> " if i == 1 else "  "
        text = prefix + mod
        if i == 1:
            draw.rectangle(
                [(0, y - 2), (18 * CELL_W // 2, y + CELL_H - 2)],
                fill=SIDEBAR_SEL_BG,
            )
            draw.text((PADDING + BORDER, y), text, font=font, fill=SIDEBAR_SEL_FG)
        else:
            draw.text((PADDING + BORDER, y), text, font=font, fill=FG)

    # Task list
    tasks = [
        "[ ] Buy groceries",
        "[X] Finish report",
        "[ ] Call dentist",
        "[ ] Review PRs",
    ]
    for i, t in enumerate(tasks):
        y = content_top + (i + 1) * CELL_H
        draw.text((PADDING + BORDER + 10 * CELL_W, y), t, font=font, fill=FG)

    # Status bar
    status_y = (ROWS - 1) * CELL_H + PADDING
    draw.rectangle([(0, status_y), (img.width, img.height)], fill=STATUS_BG)
    draw.text((PADDING + BORDER, status_y), " :Tasks=>View ", font=font, fill=STATUS_FG)

    return img


def render_help_view():
    """Render Help overlay."""
    img = create_image(COLS, ROWS)
    draw = ImageDraw.Draw(img)
    font_path = find_mono_font()
    font = ImageFont.truetype(font_path, 14) if font_path else ImageFont.load_default()

    # Dimmed background
    draw.rectangle([(0, 0), (img.width, img.height)], fill=(20, 20, 20))

    help_lines = [
        "vibePDA - Terminal Personal Data Assistant",
        "",
        "NAVIGATION",
        "  Up/Down, j/k   Move selection",
        "  Tab            Switch between sidebar and list",
        "  Enter          Edit selected item",
        "",
        "ACTIONS",
        "  F2 or N        New item",
        "  F3 or E        Edit selected",
        "  F5 or /        Search/Filter items",
        "  F10 or q       Quit",
        "",
        "Press any key to close",
    ]
    content_top = 2 * CELL_H + PADDING
    for i, line in enumerate(help_lines):
        y = content_top + i * CELL_H
        draw.text((PADDING + BORDER + 4, y), line, font=font, fill=FG)

    return img


def render_search_view():
    """Render Search/Filter mode."""
    img = create_image(COLS, ROWS)
    draw = ImageDraw.Draw(img)
    font_path = find_mono_font()
    font = ImageFont.truetype(font_path, 14) if font_path else ImageFont.load_default()

    # Title bar
    title = " vibePDA  | Notes "
    draw.rectangle([(0, 0), (img.width, CELL_H + PADDING)], fill=TITLE_BG)
    draw.text((PADDING + BORDER, PADDING), title, font=font, fill=TITLE_FG)

    # Menu bar
    menu = " F1 Help | F2 New | F3 Edit | F4 Delete | F5 Search | F10 Quit "
    draw.rectangle(
        [(0, CELL_H + PADDING), (img.width, 2 * CELL_H + PADDING)],
        fill=(40, 40, 40),
    )
    draw.text((PADDING + BORDER, CELL_H + PADDING), menu, font=font, fill=MENU_FG)

    # Sidebar
    content_top = 2 * CELL_H + PADDING
    for i, mod in enumerate(MODULES):
        y = content_top + (i + 1) * CELL_H
        prefix = "> " if i == 0 else "  "
        text = prefix + mod
        if i == 0:
            draw.rectangle(
                [(0, y - 2), (18 * CELL_W // 2, y + CELL_H - 2)],
                fill=SIDEBAR_SEL_BG,
            )
            draw.text((PADDING + BORDER, y), text, font=font, fill=SIDEBAR_SEL_FG)
        else:
            draw.text((PADDING + BORDER, y), text, font=font, fill=FG)

    # Search prompt in status bar
    status_y = (ROWS - 1) * CELL_H + PADDING
    draw.rectangle([(0, status_y), (img.width, img.height)], fill=(60, 40, 0))
    draw.text((PADDING + BORDER, status_y), " Search: grocery_ ", font=font, fill=(255, 255, 200))

    # Filtered result
    draw.text((PADDING + BORDER + 10 * CELL_W, content_top + CELL_H), "1  Grocery list", font=font, fill=CARD_TITLE)
    draw.text((PADDING + BORDER + 10 * CELL_W, content_top + 2 * CELL_H), "Milk, eggs, bread, coffee", font=font, fill=FG)

    return img


def main():
    print("Generating screenshots...")
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # Generate PNGs
    screens = [
        ("notes.png", render_notes_view(0), "Notes module"),
        ("tasks.png", render_tasks_view(), "Tasks module"),
        ("help.png", render_help_view(), "Help overlay"),
        ("search.png", render_search_view(), "Search/Filter mode"),
    ]
    for name, img, desc in screens:
        path = OUT_DIR / name
        img.save(path, "PNG")
        print(f"  Saved {path} ({desc})")

    # Animated GIF - cycle through views
    frames = []
    for i in range(4):
        frames.append(render_notes_view(0))
    frames.append(render_tasks_view())
    frames.append(render_search_view())
    frames.append(render_help_view())
    for i in range(3):
        frames.append(render_notes_view(0))

    # Resize for reasonable GIF size (2x scale for readability)
    scale = 2
    w, h = frames[0].size
    frames_scaled = [f.resize((w * scale, h * scale), Image.NEAREST) for f in frames]

    gif_path = OUT_DIR / "vibePDA-demo.gif"
    frames_scaled[0].save(
        gif_path,
        save_all=True,
        append_images=frames_scaled[1:],
        duration=800,
        loop=0,
    )
    print(f"  Saved {gif_path} (animated demo)")

    print("Done.")


if __name__ == "__main__":
    main()
