#!/usr/bin/env python3

import argparse
import subprocess
import sys
from pathlib import Path

from translation_utils import MAN_RESOURCES, create_po4a_config, fix_man_page, init_sync_dir

SCRIPT_DIR = Path(__file__).parent
SOURCE_DIR = SCRIPT_DIR.parent.parent

parser = argparse.ArgumentParser(
    description="Pull the translations of the man pages from Transifex and generate doc/man/LANG/*.1.in."
)
# Languages must be named explicitly: man pages of the other languages are maintained by hand for now,
# and must not be overwritten by the (still empty) translations from Transifex.
parser.add_argument("languages", nargs="+", metavar="LANG", help="languages to update, e.g. ro")
args = parser.parse_args()

for resource in MAN_RESOURCES:
    sync_dir = init_sync_dir(SCRIPT_DIR, resource)

    subprocess.run(("tx", "pull", "--force", "-l", ",".join(args.languages)), cwd=sync_dir, check=True)

    po4a_config = create_po4a_config(sync_dir, SCRIPT_DIR, SOURCE_DIR, resource, args.languages)

    subprocess.run(("po4a", str(po4a_config)), cwd=SCRIPT_DIR, check=True)

    for lang in args.languages:
        man_page = SOURCE_DIR / "doc" / "man" / lang / f"{resource}.in"
        if man_page.exists():
            fix_man_page(man_page)

for lang in args.languages:
    if not (SOURCE_DIR / "doc" / "man" / lang / "Makefile.am").exists():
        print(
            f"warning: doc/man/{lang}/Makefile.am does not exist; add it, and add {lang} to ALL_DOC_LINGUAS "
            "in configure.ac, to AC_CONFIG_FILES in configure.ac and to DIST_SUBDIRS in doc/man/Makefile.am",
            file=sys.stderr,
        )
