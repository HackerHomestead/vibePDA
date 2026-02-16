#!/usr/bin/env python3
"""
vibePDA Terminal Configuration — detect display, ask user, set settings.

Shows Unicode vs ASCII box-drawing samples, asks what the user sees, then
writes VIBE_ASCII_BOX and other settings to ~/.config/vibe/vibe.env.
Optionally adds a source line to the user's shell config.

Usage:
  python3 scripts/configure_terminal.py [--non-interactive] [--force-ascii] [--force-unicode]

Options:
  --non-interactive  Skip prompts; use auto-detection only (TERM, LANG)
  --force-ascii      Set VIBE_ASCII_BOX=1 without asking
  --force-unicode    Unset VIBE_ASCII_BOX (use Unicode) without asking
  --no-shell-config  Do not modify ~/.bashrc, ~/.zshrc, etc.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

# Unicode box-drawing (U+2500 block)
BOX_TOP_L = "\u250c"   # ┌
BOX_TOP_R = "\u2510"   # ┐
BOX_BOT_L = "\u2514"   # └
BOX_BOT_R = "\u2518"   # ┘
BOX_H = "\u2500"       # ─
BOX_V = "\u2502"       # │
BOX_LT = "\u251c"      # ├
BOX_RT = "\u2524"      # ┤
BOX_BT = "\u2534"      # ┴

CONFIG_DIR = Path.home() / ".config" / "vibe"
ENV_FILE = CONFIG_DIR / "vibe.env"
VIBE_BIN_NAME = "vibePDA"


def detect_environment():
    """Return dict of TERM, LANG, LC_*, locale output."""
    info = {
        "TERM": os.environ.get("TERM", "(unset)"),
        "LANG": os.environ.get("LANG", "(unset)"),
        "LC_ALL": os.environ.get("LC_ALL", "(unset)"),
        "locale": None,
    }
    try:
        r = subprocess.run(
            ["locale"], capture_output=True, text=True, timeout=2
        )
        if r.returncode == 0:
            info["locale"] = r.stdout.strip()[:200]
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass
    return info


def draw_sample(use_ascii: bool) -> str:
    """Draw a small sample box (Unicode or ASCII)."""
    if use_ascii:
        tl, tr, bl, br = "+", "+", "+", "+"
        h, v = "-", "|"
        lt, rt, bt = "+", "+", "+"
    else:
        tl, tr, bl, br = BOX_TOP_L, BOX_TOP_R, BOX_BOT_L, BOX_BOT_R
        h, v = BOX_H, BOX_V
        lt, rt, bt = BOX_LT, BOX_RT, BOX_BT

    lines = ["Sample Card", "Content here"]
    w = max(len(l) for l in lines) + 2
    w = max(w, 12)
    top = tl + h * (w - 2) + tr
    mid = v + " " * (w - 2) + v
    sep = lt + h * (w - 2) + rt
    bot = bl + h * (w - 2) + br

    out = [top]
    for i, line in enumerate(lines):
        out.append(v + " " + line.ljust(w - 4) + " " + v)
        if i < len(lines) - 1:
            out.append(sep)
    out.append(bot)
    return "\n".join(out)


def prompt_user() -> bool:
    """
    Show samples, ask user what they see. Returns True if ASCII fallback needed.
    """
    print()
    print("=" * 60)
    print("  vibePDA — Terminal Display Configuration")
    print("=" * 60)
    print()
    print("Below are two sample boxes. Please look at BOTH and answer.")
    print()

    print("--- SAMPLE A (Unicode box-drawing) ---")
    print(draw_sample(use_ascii=False))
    print()
    print("   Expected: clean corners ┌ ┐ └ ┘ and lines ─ │")
    print("   If you see: ? � ~T~B or other garbled chars, Unicode is broken.")
    print()

    print("--- SAMPLE B (ASCII fallback) ---")
    print(draw_sample(use_ascii=True))
    print()
    print("   Expected: + - |")
    print()

    while True:
        print("Which looks correct in YOUR terminal?")
        print("  1) Sample A (Unicode) looks good  — use Unicode")
        print("  2) Sample B (ASCII) looks good    — use ASCII fallback")
        print("  3) Both look good                  — use Unicode (preferred)")
        print("  4) Both look bad / unsure         — use ASCII fallback (safer)")
        try:
            choice = input("Enter 1, 2, 3, or 4: ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return True  # default to ASCII on interrupt
        if choice in ("1", "2", "3", "4"):
            break
        print("  Please enter 1, 2, 3, or 4.")
    print()

    # 1=Unicode good, 2=ASCII good, 3=both (use Unicode), 4=unsure (use ASCII)
    return choice in ("2", "4")


def auto_detect() -> bool:
    """
    Heuristic: LANG=C or missing UTF-8 suggests ASCII fallback.
    Returns True if ASCII fallback recommended.
    """
    lang = os.environ.get("LANG", "")
    lc_all = os.environ.get("LC_ALL", "")
    term = os.environ.get("TERM", "")

    # C or POSIX locale usually means no UTF-8
    if "C" in lang or "POSIX" in lang or "C" in lc_all or "POSIX" in lc_all:
        if "UTF-8" not in lang and "UTF-8" not in lc_all:
            return True

    # Legacy TERM types
    if term.lower() in ("dumb", "vt100", "vt102", "cons25"):
        return True

    return False


def write_config(use_ascii: bool, no_shell_config: bool) -> None:
    """Write vibe.env and optionally update shell config."""
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)

    if use_ascii:
        content = "# vibePDA terminal config (ASCII box fallback)\nexport VIBE_ASCII_BOX=1\n"
    else:
        content = "# vibePDA terminal config (Unicode box-drawing)\nexport VIBE_ASCII_BOX=0\n"

    ENV_FILE.write_text(content, encoding="utf-8")
    print(f"Wrote: {ENV_FILE}")

    if no_shell_config:
        print()
        print("To apply in new shells, add to your ~/.bashrc or ~/.zshrc:")
        print(f"  [ -f {ENV_FILE} ] && . {ENV_FILE}")
        return

    # Find shell config files
    shell = os.environ.get("SHELL", "")
    rc_files = []
    if "zsh" in shell:
        rc_files = [
            Path.home() / ".zshrc",
            Path.home() / ".zshenv",
        ]
    elif "bash" in shell or not shell:
        rc_files = [
            Path.home() / ".bashrc",
            Path.home() / ".profile",
        ]
    rc_files = [p for p in rc_files if p.exists()]

    source_line = f'[ -f "{ENV_FILE}" ] && . "{ENV_FILE}"  # vibePDA'
    marker = "# vibePDA"

    updated = []
    for rc in rc_files:
        try:
            text = rc.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if marker in text or f"vibe.env" in text:
            continue  # already configured
        new_text = text.rstrip() + "\n\n" + source_line + "\n"
        try:
            rc.write_text(new_text, encoding="utf-8")
            updated.append(rc)
        except OSError:
            pass

    if updated:
        print(f"Added source line to: {', '.join(str(p) for p in updated)}")
        print("Run 'source ~/.bashrc' (or your shell rc) or open a new terminal.")
    else:
        print()
        print("To apply in new shells, add to your ~/.bashrc or ~/.zshrc:")
        print(f"  [ -f {ENV_FILE} ] && . {ENV_FILE}")


def main():
    ap = argparse.ArgumentParser(
        description="Configure vibePDA terminal display (Unicode vs ASCII box-drawing)"
    )
    ap.add_argument(
        "--non-interactive",
        action="store_true",
        help="Skip prompts; use auto-detection only",
    )
    ap.add_argument(
        "--force-ascii",
        action="store_true",
        help="Set VIBE_ASCII_BOX=1 without asking",
    )
    ap.add_argument(
        "--force-unicode",
        action="store_true",
        help="Use Unicode (unset VIBE_ASCII_BOX) without asking",
    )
    ap.add_argument(
        "--no-shell-config",
        action="store_true",
        help="Do not modify ~/.bashrc, ~/.zshrc, etc.",
    )
    args = ap.parse_args()

    use_ascii = None

    if args.force_ascii:
        use_ascii = True
    elif args.force_unicode:
        use_ascii = False
    elif args.non_interactive:
        use_ascii = auto_detect()
        info = detect_environment()
        print("Auto-detection (non-interactive):")
        print(f"  TERM={info['TERM']}  LANG={info['LANG']}")
        print(f"  Recommendation: {'ASCII fallback' if use_ascii else 'Unicode'}")
    else:
        info = detect_environment()
        print("Current environment:")
        print(f"  TERM={info['TERM']}  LANG={info['LANG']}")
        use_ascii = prompt_user()

    write_config(use_ascii, args.no_shell_config)

    if use_ascii:
        print()
        print("Configured: ASCII box-drawing (+ - |)")
    else:
        print()
        print("Configured: Unicode box-drawing (┌ ─ ┐ │ ├ ┤ └ ┘)")
    print()
    print("vibePDA loads ~/.config/vibe/vibe.env automatically — no shell restart needed.")


if __name__ == "__main__":
    main()
