#!/usr/bin/env python3
"""Reject literal foreground colors in *.syntax files; only scope tokens are allowed.

A token is an upper-case name such as KEYWORD_CONTROL or STRING_QUOTED_DOUBLE.
Backgrounds may stay literal (see CLAUDE.md), so a literal color is only an error
when it is the foreground color of a keyword or context rule, or the value of a define.
The special foregrounds "default" and "base" are allowed, for highlights that only set a
background (e.g. "keyword linestart \\t default red" marks a tab).

A rule directly under a "# TODO scope" comment (up to the next blank line) is an
accepted exception. Pass --strict to reject those too.

A file containing the line "# check-tokens: literal colors by design" is skipped
entirely, even with --strict; use it only where the literal colors are the content
(e.g. a file that shows each color name in its own color).

Usage: check-tokens.py [--strict] [FILE ...]   (default: all *.syntax here)
Exit status: 0 when clean, 1 when violations were found.
"""

import glob
import os
import re
import sys

COLORS = {
    'black', 'blue', 'brown', 'cyan', 'gray', 'green', 'lightgray', 'magenta',
    'orange', 'red', 'white', 'yellow', 'brightblue', 'brightcyan', 'brightgreen',
    'brightmagenta', 'brightred', 'brightwhite', 'brightyellow',
}
FLAGS = {'whole', 'wholeleft', 'wholeright', 'linestart', 'exclusive'}
TOKEN = re.compile(r'^[A-Z][A-Z0-9_]*$')
BY_DESIGN = '# check-tokens: literal colors by design'
# "default" (terminal default) and "base" (mc's main colors) carry no color of their own;
# they are the foreground of background-only highlights such as "default red"
SPECIAL = {'default', 'base'}
HEX = re.compile(r'^#[0-9a-fA-F]{6}$')


def is_literal(word):
    # "yellow/24" is a legacy 256-color form; the part before "/" is the color
    return word.split('/')[0] in COLORS or bool(HEX.match(word))


def foreground(line):
    """Return the foreground color (literal or token) of a rule, or None.

    Positional, as MC reads them:
      keyword <pattern> <fg> [bg]
      context default [fg] [bg]
      context <start> <end> <fg> [bg]
      define <name> <fg> [bg]
    A rule without a color word in that position has no color.
    """
    words = [w for w in line.split() if w not in FLAGS]
    if not words:
        return None
    kind = words[0]
    if kind == 'keyword' and len(words) >= 3:
        return words[2]
    if kind == 'context' and len(words) >= 2 and words[1] == 'default':
        return words[2] if len(words) >= 3 else None  # "context default" has no delimiters
    if kind == 'context' and len(words) >= 4:
        return words[3]
    if kind == 'define' and len(words) >= 3:
        return words[2]
    return None


NAME = re.compile(r'^[A-Za-z_][A-Za-z0-9_/#]*$')


def check(path, strict):
    """Return (line, message) pairs for one file."""
    with open(path, encoding='latin-1') as fh:
        lines = fh.read().split('\n')
    if any(line.strip() == BY_DESIGN for line in lines):
        return []

    # First pass: which lines are exempt (under a "# TODO scope" comment, up to a blank
    # line) and which define names exist with which value.
    exempt_line = [False] * len(lines)
    defines = {}
    exempt = False
    for number, raw in enumerate(lines):
        stripped = raw.strip()
        if not stripped:
            exempt = False
        elif stripped.startswith('#'):
            if 'TODO scope' in stripped and not strict:
                exempt = True
        else:
            exempt_line[number] = exempt
            words = stripped.split()
            if words[0] == 'define' and len(words) >= 3:
                defines[words[1]] = (words[2], exempt)

    problems = []
    for number, raw in enumerate(lines):
        stripped = raw.strip()
        if not stripped or stripped.startswith('#'):
            continue
        fg = foreground(stripped)
        if fg is None or not NAME.match(fg) and not is_literal(fg) and fg not in defines:
            continue  # pattern text with spaces or punctuation, not a color position
        exempt = exempt_line[number]
        if fg in SPECIAL:
            continue
        if fg in defines:
            value, value_exempt = defines[fg]
            if is_literal(value) and not (value_exempt and not strict):
                problems.append((number + 1, f'uses define "{fg}" = literal "{value}"'))
            elif not is_literal(value) and not TOKEN.match(value):
                problems.append((number + 1, f'define "{fg}" = bad token "{value}"'))
        elif is_literal(fg):
            if not exempt:
                problems.append((number + 1, f'literal color "{fg}" (use a scope token)'))
        elif not TOKEN.match(fg):
            if not exempt:
                problems.append((number + 1, f'bad token "{fg}" (tokens are [A-Z0-9_]+)'))
    return problems


def main(argv):
    strict = '--strict' in argv
    files = [a for a in argv if a != '--strict'] or sorted(glob.glob(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '*.syntax')))
    total = 0
    for path in files:
        for number, message in check(path, strict):
            print(f'{os.path.basename(path)}:{number}: {message}')
            total += 1
    if total:
        print(f'{total} violation(s)')
        return 1
    print('ok: only scope tokens in foreground colors')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
