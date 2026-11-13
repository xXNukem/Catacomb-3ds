#!/usr/bin/env python3
"""One-line, idempotent warning fix; preserves all other upstream header content."""
from pathlib import Path

header = Path(__file__).resolve().parents[3] / 'src' / 'be_cross.h'
old = 'static bool BE_Cross_isascii(char c)'
new = 'static inline __attribute__((unused)) bool BE_Cross_isascii(char c)'
content = header.read_text(encoding='utf-8')
if new in content:
    print(f'Already fixed: {header}')
elif content.count(old) == 1:
    header.write_text(content.replace(old, new, 1), encoding='utf-8')
    print(f'Fixed one unused-function warning declaration: {header}')
else:
    raise SystemExit(f'Unexpected header version, no changes made: {header}')
