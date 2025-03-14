#!/usr/bin/env python3

import re
import subprocess

from pathlib import Path

MC_SOURCE_ROOT = Path(__file__).parent.parent

if not (MC_SOURCE_ROOT / "doc/man/mc.1.in").exists():
    raise FileNotFoundError("cannot read doc/man/mc.1.in")

warnings = []

for manpage in (MC_SOURCE_ROOT / "doc").glob("**/*.1.*"):
    print(manpage)

    for renderer in ("groff", "nroff"):
        warnings.extend(
            subprocess.check_output(
                f"{renderer} -K UTF-8 -Tutf8 -mandoc -ww '{manpage}' | grep 'warning:' || true",
                shell=True,
                stderr=subprocess.STDOUT,
            ).splitlines()
        )

    content = "\n".join(manpage.read_text().splitlines()[1:])  # skip first line

    # https://mandoc.bsd.lv/mdoc/intro/escaping.html
    for pattern in ("\\\\", "\\.", "\\!"):
        if pattern in content:
            warnings.append(f"{manpage}: forbidden pattern '{pattern}'")

    if re.findall(r"\b-\b", content):
        warnings.append(f"{manpage}: unescaped dash ('-')")

    if re.findall(r'[^.]\\"', content):
        warnings.append(f"{manpage}: escaped quote ('\"')")

# Check that English manuals are in ASCII
for manpage in (MC_SOURCE_ROOT / "doc/man").glob("*.1.in"):
    print(manpage)
    warnings.extend(
        subprocess.check_output(
            f"groff -Tascii -winput '{manpage}' | grep 'warning:' || true",
            shell=True,
            stderr=subprocess.STDOUT,
        ).splitlines()
    )

if set(warnings):
    raise AssertionError(f"roff warnings: {set(warnings)}")
