#!/usr/bin/env python3

import subprocess
from pathlib import Path

from translation_utils import init_sync_dir

RESOURCE_NAME = "mc.pot"

SCRIPT_DIR = Path(__file__).parent
SOURCE_DIR = SCRIPT_DIR.parent.parent

sync_dir = init_sync_dir(SCRIPT_DIR, RESOURCE_NAME)

# Copy mc.pot to the working directory
(sync_dir / RESOURCE_NAME).write_text((SOURCE_DIR / "po" / RESOURCE_NAME).read_text())

subprocess.run(("tx", "push", "--source"), cwd=sync_dir, check=True)
