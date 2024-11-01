#!/usr/bin/env python3

import subprocess
from pathlib import Path

from translation_utils import create_po4a_config, init_sync_dir

RESOURCE_NAME = "mc.hint"

SCRIPT_DIR = Path(__file__).parent
SOURCE_DIR = SCRIPT_DIR.parent.parent

sync_dir = init_sync_dir(SCRIPT_DIR, RESOURCE_NAME)

po4a_config = create_po4a_config(sync_dir, SCRIPT_DIR, SOURCE_DIR, RESOURCE_NAME)

subprocess.run(("po4a", str(po4a_config)), cwd=SCRIPT_DIR, check=True)

subprocess.run(("tx", "push", "--source"), cwd=sync_dir, check=True)
