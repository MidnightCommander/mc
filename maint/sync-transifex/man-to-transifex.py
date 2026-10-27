#!/usr/bin/env python3

import subprocess
from pathlib import Path

from translation_utils import MAN_RESOURCES, create_po4a_config, init_sync_dir

SCRIPT_DIR = Path(__file__).parent
SOURCE_DIR = SCRIPT_DIR.parent.parent

for resource in MAN_RESOURCES:
    sync_dir = init_sync_dir(SCRIPT_DIR, resource)

    po4a_config = create_po4a_config(sync_dir, SCRIPT_DIR, SOURCE_DIR, resource)

    subprocess.run(("po4a", str(po4a_config)), cwd=SCRIPT_DIR, check=True)

    subprocess.run(("tx", "push", "--source"), cwd=sync_dir, check=True)
