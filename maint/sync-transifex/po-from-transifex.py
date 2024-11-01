#!/usr/bin/env python3

import glob
import subprocess
from pathlib import Path

from translation_utils import get_translations, init_sync_dir

RESOURCE_NAME = "mc.pot"

SCRIPT_DIR = Path(__file__).parent
SOURCE_DIR = SCRIPT_DIR.parent.parent
PO_DIR = SOURCE_DIR / "po"


def strip_message_locations(work_dir: Path):
    for po_file in (work_dir / filename for filename in glob.glob("*.po", root_dir=work_dir)):
        po_file.write_text(
            "".join(line for line in po_file.read_text().splitlines(keepends=True) if not line.startswith("#:"))
        )


def copy_translations_to_source_dir(source_dir: Path, target_dir: Path):
    for po_file in (source_dir / filename for filename in glob.glob("*.po", root_dir=source_dir)):
        (target_dir / po_file.name).write_text(po_file.read_text())


def update_linguas(po_dir: Path):
    translations = get_translations(po_dir)
    (po_dir / "LINGUAS").write_text("# List of available translations\n" + "\n".join(translations) + "\n")


sync_dir = init_sync_dir(SCRIPT_DIR, RESOURCE_NAME)

subprocess.run(("tx", "pull", "--all", "--force"), cwd=sync_dir, check=True)

strip_message_locations(sync_dir)

copy_translations_to_source_dir(sync_dir, PO_DIR)

update_linguas(PO_DIR)
