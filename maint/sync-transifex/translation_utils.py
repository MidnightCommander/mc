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

    if resource in MAN_RESOURCES:
        prepare_man_master(source_dir / "doc" / "man" / f"{resource}.in", sync_dir / f"{resource}.in")

    config = get_config_file(script_dir, resource, "po4a.cfg").read_text()

    config = config.replace("@resource@", resource)
    config = config.replace("@translations@", " ".join(f"{lang}:var.d/{resource}/{lang}.po" for lang in langs))
    config = config.replace(
        "@resources@", " ".join(f"{lang}:{RESOURCE_TARGETS[resource].format(lang=lang)}" for lang in langs)
    )

    config = config.replace("@man_options@", MAN_PO4A_OPTIONS)
    config = config.replace("@srcdir@", str(source_dir))

    config_path = sync_dir / "po4a.cfg"
    config_path.write_text(config)

    return config_path


def get_translations(root_dir: Path) -> list[str]:
    return sorted(Path(filename).name.removesuffix(".po") for filename in glob.glob("*.po", root_dir=root_dir))


# The built-in help is generated from mc.1 by src/man2hlp, which reads its directives from comments:
#
#   .\"TOPICS "Topics:"          title of the table of contents (translatable, must come first)
#   .\"NODE "Menu Bar"           start of a help node, named by its English title
#   .\"SKIP_SECTION"             do not put the next section into the help
#   .\"DONT_SPLIT"               do not split the next section into nodes
#
#   .\"LINK2"                    link to a node: the line in the middle is the link text
#   menu bar line\&.
#   .\"Menu Bar"
#
# po4a treats comments as notes for the translators and moves them in front of the next paragraph, which breaks
# all of these.  Before extracting the strings, the directives are therefore turned into macros (which po4a keeps
# in place, see MAN_PO4A_OPTIONS) and every link into an inline marker that stays inside its paragraph.  Both are
# turned back into comments after the translated page has been generated.
HELP_DIRECTIVES = {"TOPICS": "MCTOPICS", "NODE": "MCNODE", "SKIP_SECTION": "MCSKIPSECTION", "DONT_SPLIT": "MCDONTSPLIT"}
HELP_DIRECTIVE = re.compile(r'^\.\\"(' + "|".join(HELP_DIRECTIVES) + r')(?: "(.*)"|")$', re.MULTILINE)
HELP_DIRECTIVE_MACRO = re.compile(r"^\.(" + "|".join(HELP_DIRECTIVES.values()) + r')(?: +"?(.*?)"?)? *$', re.MULTILINE)

HELP_LINK = re.compile(r'^\.\\"LINK2"\n(.*)\n\.\\"(.*)"$', re.MULTILINE)
# po4a escapes some characters (e.g. "-" as "\-"), so the marker is made of letters only
HELP_LINK_MARKER = re.compile(r"[ \t]*\n?\\X'mclink:([^']*)'(.*?)\\X'mclink'[ \t]*(\n?)", re.DOTALL)

MAN_PO4A_OPTIONS = (
    "-o untranslated=MCNODE -o noarg=MCSKIPSECTION,MCDONTSPLIT -o translate_joined=MCTOPICS"
)


def fold_help_markup(text: str) -> str:
    def fold_directive(match: re.Match) -> str:
        name, argument = match.groups()
        return f".{HELP_DIRECTIVES[name]}" + ("" if argument is None else f' "{argument}"')

    def fold_link(match: re.Match) -> str:
        label, target = match.groups()
        # A link can be set in italics or bold: ".I link text"
        label = re.sub(r"^\.([IB])\s+(.*)$", r"\\f\1\2\\fP", label)
        return f"\\X'mclink:{target}'{label}\\X'mclink'"

    return HELP_DIRECTIVE.sub(fold_directive, HELP_LINK.sub(fold_link, text))


def unfold_help_markup(text: str) -> str:
    directives = {macro: name for name, macro in HELP_DIRECTIVES.items()}

    def unfold_directive(match: re.Match) -> str:
        macro, argument = match.groups()
        return f'.\\"{directives[macro]}' + ('"' if argument is None else f' "{argument}"')

    def unfold_link(match: re.Match) -> str:
        target, label = (re.sub(r"\s*\n\s*", " ", group).strip() for group in match.groups()[:2])
        target = target.replace("\\-", "-")
        label = re.sub(r"^\\f([IB])(.*)\\f[PR]$", r".\1 \2", label)
        # Text that followed the link on the same line starts a new line now (marked with \0 below)
        rest = "" if match.group(3) else "\0"
        return f'\n.\\"LINK2"\n{label}\n.\\"{target}"\n{rest}'

    text = HELP_DIRECTIVE_MACRO.sub(unfold_directive, text)
    text = HELP_LINK_MARKER.sub(unfold_link, text)

    # ...so it must not be taken for a control line
    return re.sub(r"\0([.'])", r"\\&\1", text).replace("\0", "")


def prepare_man_master(source: Path, destination: Path) -> None:
    """Write the English man page, as given to po4a, to destination."""
    destination.write_text(fold_help_markup(source.read_text()))


def fix_man_page(path: Path) -> None:
    """Undo the side effects of po4a rewrapping a translated man page."""
    text = path.read_text()

    # po4a removes the quotes around arguments that do not contain spaces, but the date of the man page
    # is substituted at build time and does contain spaces in most languages (e.g. "octombrie 2026").
    text = re.sub(r"^(\.TH \S+ \S+) %DATE_OF_MAN_PAGE% ", r'\1 "%DATE_OF_MAN_PAGE%" ', text, flags=re.MULTILINE)

    text = unfold_help_markup(text)

    # po4a turns ".B text" into "\\fBtext\\fP", but src/man2hlp counts the font escapes as text, so the tags of
    # .TP paragraphs become too long for their column.  Lines set entirely in one font are turned back into macros.
    text = re.sub(r"^\\f([BI])([^\\\"\n]*(?:\\[^f\n][^\\\"\n]*)*)\\f[PR]$", r".\1 \2", text, flags=re.MULTILINE)

    # po4a wraps long lines, but the hyperlink command \X'tty: link URL' must stay on a single line,
    # otherwise groff complains about an empty X command.
    text = re.sub(r"\\X'[^']*'", lambda match: re.sub(r"\s*\n\s*", " ", match.group(0)), text)

    path.write_text(text)
