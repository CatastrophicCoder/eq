#!/usr/bin/env python3
"""Checks that every relative link and image in the README, docs/*.md and docs/**/*.html exists.

Usage: python3 tools/check_docs.py   (from the repository root; exits 1 on a broken reference)
"""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
files = [root / "README.md", *sorted((root / "docs").glob("*.md")), *sorted((root / "docs").rglob("*.html"))]
pattern = re.compile(r'(?:\]\(|src="|href="|src=\\")([^)"#\s\\]+)')
broken = []

for f in files:
    for target in pattern.findall(f.read_text(encoding="utf-8")):
        if re.match(r"^(https?:|mailto:|data:)", target) or "${" in target or "+" in target:
            continue
        if not (f.parent / target).exists():
            broken.append(f"{f.relative_to(root)}: {target}")

print("\n".join(broken) if broken else f"All relative links resolve ({len(files)} files checked).")
sys.exit(1 if broken else 0)
