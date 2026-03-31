Diff syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `diff.diff`
Legacy reference: `misc/syntax/diff.syntax`
TS query: `misc/syntax-ts/queries-override/diff-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[diff]`

Aligned with legacy
-------------------

- Comment lines (`# ...`): `brightcyan` (TS `tag`) - MATCH
- `diff` command lines: `brightred` (TS `function.special`) - MATCH (legacy uses
  `white` on `red` background, TS uses `brightred` foreground; both use red
  emphasis)
- `---`/`+++` filenames: `brightmagenta` (TS `markup.heading`) - MATCH
- `@@` location headers: `brightcyan` (TS `delimiter`) - MATCH
- Addition lines (`+`): `brightgreen` (TS `markup.addition`) - MATCH
- Deletion lines (`-`): `brightred` (TS `markup.deletion`) - MATCH
- Context lines (` ` prefix): `yellow` (TS `keyword`) - MATCH (legacy uses
  `yellow` as default context)

Intentional improvements over legacy
-------------------------------------

- TS separately identifies `similarity`, `index`, and `mode` lines with `brown`
  (`comment` capture), giving them distinct muted color; legacy colors `index`
  and `similarity` lines as generic `yellow` default context.
- TS highlights `file_change` nodes (`new file mode`, `deleted file mode`,
  `rename from/to`) as `brightmagenta` (`markup.heading`), clearly
  distinguishing them from plain context; legacy treats some of these as default
  `yellow`.
- TS parses filenames within `---`/`+++` lines granularly, coloring the
  `---`/`+++` prefix separately from the filename path; legacy colors the entire
  line as `brightmagenta`.
- `mode` lines (e.g., `mode 100644`) are colored as `yellow` (`keyword`) in TS
  when not standalone, providing distinct emphasis.

Known shortcomings
------------------

- Legacy colors the leading space of context lines as `black` on `white`
  background (visible whitespace indicator); TS has no equivalent per-character
  background highlighting.
- Legacy context lines starting with whitespace get `lightgray` color for the
  text content; TS colors the entire context line uniformly as `yellow`.
- The `diff` command line prefix text (`diff --git`) in TS is split between
  `brightred` (`function.special`) and `brightmagenta` (`markup.heading` for
  filenames); legacy colors the entire line as `white` on `red` background.
