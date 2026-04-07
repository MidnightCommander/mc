Makefile syntax highlighting: TS vs Legacy comparison report
=============================================================

Sample file: `Makefile`
Legacy reference: `misc/syntax/makefile.syntax`
TS query: `misc/syntax-ts/queries-override/make-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[make]`

Aligned with legacy
-------------------

- Comments (`#`): `brown` - MATCH
- Directives (`define`, `endef`, `ifdef`, `ifndef`, `ifeq`, `ifneq`, `else`,
  `endif`, `include`, `override`, `export`, `unexport`, `undefine`, `private`,
  `vpath`): `magenta` - MATCH
- Assignment operator (`=`): `white` - MATCH
- Colon in rules (`:`): `yellow` - MATCH
- Double colon (`::`): `yellow` - MATCH
- Special targets (`.PHONY`, `.SUFFIXES`, `.DEFAULT`, `.PRECIOUS`,
  `.INTERMEDIATE`, `.SECONDARY`, `.DELETE_ON_ERROR`, `.SILENT`,
  `.EXPORT_ALL_VARIABLES`, `.NOTPARALLEL`): `white` - MATCH
- `$(VAR)` variable references: `yellow` (variable name) - MATCH
- Escaped dollar (`$$`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Variable assignment names (`CC`, `CFLAGS`, `LDFLAGS`, etc.): TS colors the LHS
  name as `yellow`. Legacy leaves it as DEFAULT. This clearly distinguishes
  variable definitions from other text.
- Compound assignment operators (`:=`, `::=`, `?=`, `+=`, `!=`): TS colors the
  entire operator as `white`. Legacy only colors the `=` part as `white`,
  leaving the prefix character (`:`, `?`, `+`, `!`) in its default color.
- `-include` and `sinclude` directives: TS colors these as `magenta` like
  `include`. Legacy only recognizes `include` as a directive; `-include` gets
  DEFAULT and `sinclude` gets DEFAULT.
- `override`, `export`, `unexport`, `private`, `undefine`, `vpath` keywords: TS
  colors these as `magenta` directives. Legacy recognizes some but not all of
  these consistently.
- Recipe shell content: TS applies bash injection to `shell_text` nodes,
  providing full shell syntax highlighting in recipes. Commands like `echo` get
  `yellow`, `rm` and `sed` get `cyan`, strings get `green`, and operators get
  `brightcyan`. Legacy leaves recipe content as DEFAULT (no shell awareness).
- Strings in recipes: TS colors `"double quoted"` and `'single quoted'` strings
  as `green` via bash injection. Legacy does not recognize strings inside
  recipes.
- Semicolons and commas: TS colors as `brightcyan` (delimiter). Legacy does not
  distinguish these.
- `$(VAR)` in recipes: TS colors the variable name inside as `yellow` with the
  parentheses getting shell injection colors. Legacy colors the entire `$(VAR)`
  as `yellow` uniformly. TS provides more granularity.
- `${VAR}` references: TS colors the variable name as `yellow`. Legacy colors
  the entire `${VAR}` including braces as `brightgreen`. The legacy distinction
  between `$()` and `${}` is arbitrary; TS treats both uniformly.
- Automatic variables (`$@`, `$^`, `$*`, `$?`): TS colors these as `brightred`
  (function.special) in most contexts. Legacy does not distinguish automatic
  variables from other `$`-prefixed text.
- `ifeq`/`ifneq` arguments: TS colors variable references inside conditionals
  (e.g. `$(CC)` in `ifeq ($(CC),gcc)`) with proper variable highlighting. Legacy
  colors the opening `$(` as `yellow` but doesn't complete the pattern.
- Shell assignment (`!=`): TS colors the variable name as `yellow` and the RHS
  gets bash injection highlighting. Legacy only colors the `=` as `white`.
- Inline recipes (`quick: ; @echo "done"`): TS colors the `;` as `brightcyan`
  and the recipe content gets bash injection. Legacy does not distinguish the
  recipe portion.

Known shortcomings
------------------

- Right side of assignments: legacy colors the entire RHS of a variable
  assignment as `brightcyan`. TS leaves the RHS as DEFAULT (with variable
  references colored individually). The legacy approach is coarse but provides a
  visual distinction; TS relies on individual element coloring instead. This is
  a deliberate design difference.
- `$<` automatic variable in recipes: bash injection interferes with the make
  grammar's `automatic_variable` capture. Bash interprets `<` as a redirect
  operator (`brightcyan`) rather than part of the automatic variable. The `$`
  portion may appear as DEFAULT. This affects `$<` and `$%` specifically; other
  automatic variables (`$@`, `$^`, `$*`, `$?`) work correctly because bash also
  recognizes them.
- Line continuation (`\`): legacy colors the trailing backslash as `yellow`. The
  tree-sitter make grammar absorbs continuation characters into the parent node
  text, so they cannot be captured separately.
- `@substitution@` (autoconf markers): legacy colors `@word@` patterns as
  `brightmagenta` on `black` background. TS does not recognize autoconf
  substitution markers. This is an MC-specific legacy feature not present in
  standard Makefile syntax.
- Recipe tab leader: legacy colors the leading tab character in recipes as
  `lightgray` on `red` background as a visual cue. The tree-sitter grammar does
  not expose the tab as a capturable node.
- `define` block content: both legacy and TS leave the content of
  `define`/`endef` blocks as DEFAULT. Only the `define` and `endef` keywords are
  colored as `magenta`.
