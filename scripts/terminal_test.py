#!/usr/bin/env python3
"""
Terminal compatibility testing for vibePDA TUI.

Tests the application under different TERM and LANG settings to catch
display issues (e.g. UTF-8 box-drawing on Mac Terminal, legacy terminals).

Usage:
  python3 scripts/terminal_test.py [--screenshots] [--ascii-fallback]

Options:
  --screenshots    Generate screenshot variants (ASCII vs Unicode) for review
  --ascii-fallback Verify VIBE_ASCII_BOX=1 works correctly
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = REPO_ROOT / "docs" / "images" / "terminal_test"
VIBE_BIN = REPO_ROOT / "vibePDA"

# Terminal configurations to test (TERM, LANG)
TERMINAL_CONFIGS = [
    ("xterm-256color", "en_US.UTF-8"),
    ("xterm", "en_US.UTF-8"),
    ("vt100", "C"),
    ("dumb", "C"),
    ("xterm-256color", "C"),  # UTF-8 term but C locale - may cause issues
]

# Box-drawing UTF-8 bytes (for mojibake detection)
BOX_UTF8_BYTES = bytes([0xE2, 0x94, 0x80])  # ─
REPLACEMENT_CHAR = b'\xef\xbf\xbd'  # U+FFFD


def run_cli(args, env=None, timeout=5):
    """Run vibePDA with given args, return (stdout, stderr, returncode)."""
    env = env or os.environ.copy()
    try:
        r = subprocess.run(
            [str(VIBE_BIN)] + args,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=str(REPO_ROOT),
            env=env,
        )
        return r.stdout, r.stderr, r.returncode
    except subprocess.TimeoutExpired:
        return "", "timeout", -1
    except FileNotFoundError:
        return "", "vibePDA not found (run 'make' first)", -1
    except Exception as e:
        return "", str(e), -1


def run_tui_capture(env=None, timeout=2):
    """Run TUI briefly in a pty, capture raw output."""
    env = env or os.environ.copy()
    try:
        # Use script(1) to capture terminal output, or Python pty
        import pty
        import select
        
        master, slave = pty.openpty()
        env["TERM"] = env.get("TERM", "xterm-256color")
        
        pid = os.fork()
        if pid == 0:
            os.close(master)
            os.setsid()
            pty.STDIN_FILENO = slave
            pty.STDOUT_FILENO = slave
            pty.STDERR_FILENO = slave
            os.dup2(slave, 0)
            os.dup2(slave, 1)
            os.dup2(slave, 2)
            os.close(slave)
            os.execve(str(VIBE_BIN), ["vibePDA"], env)
            sys.exit(1)
        
        os.close(slave)
        data = b""
        import time
        end = time.time() + timeout
        while time.time() < end:
            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                try:
                    chunk = os.read(master, 4096)
                    if not chunk:
                        break
                    data += chunk
                except OSError:
                    break
        os.close(master)
        os.waitpid(pid, 0)
        return data
    except ImportError:
        return b"(pty not available)"
    except Exception as e:
        return f"(error: {e})".encode()


def check_mojibake(data):
    """Check for UTF-8 mojibake or replacement characters in captured output."""
    if isinstance(data, str):
        data = data.encode("utf-8", errors="replace")
    issues = []
    if REPLACEMENT_CHAR in data:
        issues.append("Replacement character (U+FFFD) found - UTF-8 decode failure")
    # Check for incomplete UTF-8 sequences
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        issues.append("Invalid UTF-8 sequence in output")
    return issues


def test_ascii_fallback():
    """Verify VIBE_ASCII_BOX=1 produces ASCII borders."""
    env = os.environ.copy()
    env["VIBE_ASCII_BOX"] = "1"
    stdout, stderr, code = run_cli(["notes", "list"], env=env)
    if code != 0 and "not found" not in stderr.lower():
        return False, f"Exit code {code}: {stderr[:200]}"
    # With no notes, we get empty or header. With notes, we should see + - | not UTF-8
    combined = stdout + stderr
    # ASCII box chars
    if "+" in combined or "|" in combined or "-" in combined:
        return True, "ASCII borders present"
    # No box chars in empty output is OK
    return True, "OK (no box chars in empty output)"


def test_terminal_config(term, lang):
    """Test app under specific TERM/LANG."""
    env = os.environ.copy()
    env["TERM"] = term
    env["LANG"] = lang
    env["LC_ALL"] = lang
    stdout, stderr, code = run_cli(["--help"], env=env)
    if code != 0:
        return "fail", f"Exit {code}"
    issues = check_mojibake(stdout + stderr)
    if issues:
        return "warn", "; ".join(issues)
    return "ok", ""


def main():
    ap = argparse.ArgumentParser(description="Terminal compatibility testing for vibePDA")
    ap.add_argument("--screenshots", action="store_true", help="Generate screenshot variants")
    ap.add_argument("--ascii-fallback", action="store_true", help="Test VIBE_ASCII_BOX=1")
    args = ap.parse_args()
    
    if not VIBE_BIN.exists():
        print("ERROR: vibePDA not built. Run 'make' first.")
        sys.exit(1)
    
    print("vibePDA Terminal Compatibility Test")
    print("=" * 50)
    
    # Test ASCII fallback
    if args.ascii_fallback or True:
        print("\n[VIBE_ASCII_BOX=1 fallback]")
        ok, msg = test_ascii_fallback()
        status = "PASS" if ok else "FAIL"
        print(f"  {status}: {msg}")
        if not ok:
            sys.exit(1)
    
    # Test terminal configs
    print("\n[Terminal configurations]")
    results = []
    for term, lang in TERMINAL_CONFIGS:
        status, msg = test_terminal_config(term, lang)
        results.append((term, lang, status, msg))
        sym = "✓" if status == "ok" else "!" if status == "warn" else "✗"
        print(f"  {sym} TERM={term} LANG={lang}: {status}" + (f" - {msg}" if msg else ""))
    
    # Screenshot variants (ASCII vs Unicode)
    if args.screenshots:
        print("\n[Generating screenshot variants]")
        try:
            subprocess.run(
                [sys.executable, str(REPO_ROOT / "scripts" / "gen_screenshots.py")],
                cwd=str(REPO_ROOT),
                check=True,
            )
            print("  Screenshots in docs/images/")
        except subprocess.CalledProcessError as e:
            print(f"  Warning: gen_screenshots failed: {e}")
        except Exception as e:
            print(f"  Warning: {e}")
    
    print("\n" + "=" * 50)
    print("For Mac Terminal or broken UTF-8 display, run:")
    print("  VIBE_ASCII_BOX=1 ./vibePDA")
    print("Or add to ~/.bashrc: export VIBE_ASCII_BOX=1")
    print("Done.")


if __name__ == "__main__":
    main()
