import glob
import re
from pathlib import Path

# Man pages translated through po4a; the names are also the names of the directories in config.d.
MAN_RESOURCES = ["mc.1", "mcedit.1", "mcview.1", "mcdiff.1"]

# Where the translation of a resource is written (relative to the source tree), for every language.
RESOURCE_TARGETS = {
    "mc.hint": "@srcdir@/doc/hints/l10n/mc.hint.{lang}",
    **{resource: f"@srcdir@/doc/man/{{lang}}/{resource}.in" for resource in MAN_RESOURCES},
}


def get_config_file(root_dir: Path, resource: str, name: str) -> Path:
    return root_dir / "config.d" / resource / name


def init_sync_dir(root_dir: Path, resource: str) -> Path:
    tx_dir = root_dir / "var.d" / resource / ".tx"
    tx_dir.mkdir(parents=True, exist_ok=True)
    (tx_dir / "config").write_text(get_config_file(root_dir, resource, "tx.config").read_text())
    return tx_dir.parent


def create_po4a_config(
    sync_dir: Path, script_dir: Path, source_dir: Path, resource: str, languages: list[str] | None = None
) -> Path:
    langs = get_translations(sync_dir)
    if languages is not None:
        langs = [lang for lang in langs if lang in languages]

    config = get_config_file(script_dir, resource, "po4a.cfg").read_text()

    config = config.replace("@resource@", resource)
    config = config.replace("@translations@", " ".join(f"{lang}:var.d/{resource}/{lang}.po" for lang in langs))
    config = config.replace(
        "@resources@", " ".join(f"{lang}:{RESOURCE_TARGETS[resource].format(lang=lang)}" for lang in langs)
    )

    config = config.replace("@srcdir@", str(source_dir))

    config_path = sync_dir / "po4a.cfg"
    config_path.write_text(config)

    return config_path


def get_translations(root_dir: Path) -> list[str]:
    return sorted(Path(filename).name.removesuffix(".po") for filename in glob.glob("*.po", root_dir=root_dir))


def fix_man_page(path: Path) -> None:
    """Undo the side effects of po4a rewrapping a translated man page."""
    text = path.read_text()

    # po4a removes the quotes around arguments that do not contain spaces, but the date of the man page
    # is substituted at build time and does contain spaces in most languages (e.g. "octombrie 2026").
    text = re.sub(r"^(\.TH \S+ \S+) %DATE_OF_MAN_PAGE% ", r'\1 "%DATE_OF_MAN_PAGE%" ', text, flags=re.MULTILINE)

    # po4a wraps long lines, but the hyperlink command \X'tty: link URL' must stay on a single line,
    # otherwise groff complains about an empty X command.
    text = re.sub(r"\\X'[^']*'", lambda match: re.sub(r"\s*\n\s*", " ", match.group(0)), text)

    path.write_text(text)
