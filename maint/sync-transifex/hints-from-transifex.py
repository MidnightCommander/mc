#!/usr/bin/env python3

import glob
import subprocess
from pathlib import Path
from textwrap import wrap

from translation_utils import create_po4a_config, init_sync_dir

RESOURCE_NAME = "mc.hint"

SCRIPT_DIR = Path(__file__).parent
SOURCE_DIR = SCRIPT_DIR.parent.parent


def unwrap_paragraphs():
    hint_files = glob.glob(str(SOURCE_DIR / "doc" / "hints" / "l10n" / "mc.hint.*"))
    for hint_file in map(Path, hint_files):
        lines = hint_file.read_text().split("\n\n")
        hint_file.write_text("\n\n".join("".join(wrap(line, width=1024)) for line in lines) + "\n")


sync_dir = init_sync_dir(SCRIPT_DIR, RESOURCE_NAME)

subprocess.run(("tx", "pull", "--all", "--force"), cwd=sync_dir, check=True)

po4a_config = create_po4a_config(sync_dir, SCRIPT_DIR, SOURCE_DIR, RESOURCE_NAME)

subprocess.run(("po4a", str(po4a_config)), cwd=SCRIPT_DIR, check=True)

unwrap_paragraphs()
