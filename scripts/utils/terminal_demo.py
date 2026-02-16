#!/usr/bin/env python3
"""
Terminal visual reference demo — tests UTF-8, Unicode, and ASCII display.

Run this in your terminal to see how it renders box-drawing and other elements.
Use as a reference when debugging display issues (e.g. Mac Terminal, legacy TTYs).

Usage:
  python3 scripts/utils/terminal_demo.py
"""

# Unicode box-drawing (U+2500 block)
BOX_TOP_L = "\u250c"   # ┌
BOX_TOP_R = "\u2510"   # ┐
BOX_BOT_L = "\u2514"   # └
BOX_BOT_R = "\u2518"   # ┘
BOX_H = "\u2500"       # ─
BOX_V = "\u2502"       # │
BOX_T = "\u252c"       # ┬
BOX_LT = "\u251c"      # ├
BOX_RT = "\u2524"      # ┤
BOX_BT = "\u2534"      # ┴

# ASCII equivalents
ASCII_TL, ASCII_TR = "+", "+"
ASCII_BL, ASCII_BR = "+", "+"
ASCII_H, ASCII_V = "-", "|"
ASCII_T, ASCII_LT, ASCII_RT, ASCII_BT = "+", "+", "+", "+"


def draw_box(lines, use_ascii=False):
    """Draw a bordered box around text lines."""
    tl, tr, bl, br = (ASCII_TL, ASCII_TR, ASCII_BL, ASCII_BR) if use_ascii else (BOX_TOP_L, BOX_TOP_R, BOX_BOT_L, BOX_BOT_R)
    h, v = (ASCII_H, ASCII_V) if use_ascii else (BOX_H, BOX_V)
    lt, rt, bt = (ASCII_LT, ASCII_RT, ASCII_BT) if use_ascii else (BOX_LT, BOX_RT, BOX_BT)

    width = max(len(line) for line in lines) if lines else 0
    width = max(width, 4)
    top = tl + h * (width - 2) + tr
    bot = bl + h * (width - 2) + br
    sep = lt + h * (width - 2) + rt

    out = [top]
    for i, line in enumerate(lines):
        padded = line[: width - 2].ljust(width - 2)
        out.append(v + padded + v)
        if i < len(lines) - 1:
            out.append(sep)
    out.append(bot)
    return "\n".join(out)


def main():
    print("=" * 60)
    print("  vibePDA — Terminal Visual Reference Demo")
    print("=" * 60)
    print()

    # 1. Unicode box-drawing
    print("1. UNICODE BOX-DRAWING (U+2500 block)")
    print("   If you see replacement chars (? or �), your terminal may not support UTF-8.")
    print()
    lines = ["Title: Sample Card", "Content line 1", "Content line 2"]
    print(draw_box(lines, use_ascii=False))
    print()

    # 2. ASCII box-drawing
    print("2. ASCII BOX-DRAWING (+ - |)")
    print("   Fallback for terminals with broken UTF-8 (e.g. VIBE_ASCII_BOX=1).")
    print()
    print(draw_box(lines, use_ascii=True))
    print()

    # 3. Character reference table
    print("3. CHARACTER REFERENCE")
    print("-" * 40)
    refs = [
        ("Box corners", f"{BOX_TOP_L} {BOX_TOP_R} {BOX_BOT_L} {BOX_BOT_R}", "┌ ┐ └ ┘"),
        ("Box lines", f"{BOX_H} {BOX_V}", "─ │"),
        ("Box tees", f"{BOX_LT} {BOX_RT} {BOX_T} {BOX_BT}", "├ ┤ ┬ ┴"),
        ("ASCII", "+ - |", "+ - |"),
        ("Arrows", "← → ↑ ↓", "← → ↑ ↓"),
        ("Bullets", "• ◦ ▪ ▫", "• ◦ ▪ ▫"),
        ("Check/X", "✓ ✗ ☑ ☐", "✓ ✗ ☑ ☐"),
    ]
    for name, sample, expected in refs:
        print(f"   {name:12} {sample:20} (expected: {expected})")
    print("-" * 40)
    print()

    # 4. ASCII table (printable 32-126)
    print("4. ASCII TABLE (printable 32-126)")
    print("-" * 60)
    print("  dec  hex  char  |  dec  hex  char  |  dec  hex  char  |  dec  hex  char")
    print("-" * 60)
    for row_start in range(32, 127, 4):
        parts = []
        for i in range(4):
            n = row_start + i
            if n <= 126:
                c = chr(n)
                disp = repr(c)[1:-1] if c in '"\'' else c
                parts.append(f"  {n:3d}  {n:02X}   {disp:^4} ")
            else:
                parts.append(" " * 14)
        print(" |".join(parts))
    print("-" * 60)
    print()

    # 5. UTF-8 table (box-drawing and common symbols)
    print("5. UTF-8 TABLE (box-drawing U+2500, arrows, bullets, check)")
    print("-" * 60)
    utf8_refs = [
        (0x2500, "─", "BOX DRAWINGS LIGHT HORIZONTAL"),
        (0x2502, "│", "BOX DRAWINGS LIGHT VERTICAL"),
        (0x250C, "┌", "BOX DRAWINGS LIGHT DOWN AND RIGHT"),
        (0x2510, "┐", "BOX DRAWINGS LIGHT DOWN AND LEFT"),
        (0x2514, "└", "BOX DRAWINGS LIGHT UP AND RIGHT"),
        (0x2518, "┘", "BOX DRAWINGS LIGHT UP AND LEFT"),
        (0x251C, "├", "BOX DRAWINGS LIGHT VERTICAL AND RIGHT"),
        (0x2524, "┤", "BOX DRAWINGS LIGHT VERTICAL AND LEFT"),
        (0x252C, "┬", "BOX DRAWINGS LIGHT DOWN AND HORIZONTAL"),
        (0x2534, "┴", "BOX DRAWINGS LIGHT UP AND HORIZONTAL"),
        (0x2190, "←", "LEFTWARDS ARROW"),
        (0x2192, "→", "RIGHTWARDS ARROW"),
        (0x2191, "↑", "UPWARDS ARROW"),
        (0x2193, "↓", "DOWNWARDS ARROW"),
        (0x2022, "•", "BULLET"),
        (0x25E6, "◦", "WHITE BULLET"),
        (0x2713, "✓", "CHECK MARK"),
        (0x2717, "✗", "BALLOT X"),
    ]
    for cp, ch, name in utf8_refs:
        print(f"  U+{cp:04X}  {ch}  {name}")
    print("-" * 60)
    print()

    # 6. Full card example (Unicode)
    print("6. FULL CARD EXAMPLE (Unicode)")
    card = [
        "Contact: Jane Doe",
        "Email: jane@example.com",
        "Phone: +1-555-1234",
    ]
    print(draw_box(card, use_ascii=False))
    print()

    # 7. Full card example (ASCII)
    print("7. FULL CARD EXAMPLE (ASCII fallback)")
    print(draw_box(card, use_ascii=True))
    print()

    print("=" * 60)
    print("  If Unicode (1, 3, 5, 6) looks wrong, use: VIBE_ASCII_BOX=1 ./vibePDA")
    print("=" * 60)


if __name__ == "__main__":
    main()
