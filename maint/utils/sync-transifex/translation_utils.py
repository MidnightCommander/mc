import glob
from pathlib import Path


def get_config_file(root_dir: Path, resource: str, name: str) -> Path:
    return root_dir / "config.d" / resource / name


def init_sync_dir(root_dir: Path, resource: str) -> Path:
    tx_dir = root_dir / "var.d" / resource / ".tx"
    tx_dir.mkdir(parents=True, exist_ok=True)
    (tx_dir / "config").write_text(get_config_file(root_dir, resource, "tx.config").read_text())
    return tx_dir.parent


def create_po4a_config(sync_dir: Path, script_dir: Path, source_dir: Path, resource: str) -> Path:
    langs = get_translations(sync_dir)

    config = get_config_file(script_dir, resource, "po4a.cfg").read_text()

    config = config.replace("@translations@", " ".join(f"{lang}:var.d/$master/{lang}.po" for lang in langs))
    config = config.replace("@resources@", " ".join(f"{lang}:@srcdir@/doc/hints/l10n/mc.hint.{lang}" for lang in langs))

    config = config.replace("@srcdir@", str(source_dir))

    config_path = sync_dir / "po4a.cfg"
    config_path.write_text(config)

    return config_path


def get_translations(root_dir: Path) -> list[str]:
    return sorted(Path(filename).name.removesuffix(".po") for filename in glob.glob("*.po", root_dir=root_dir))
