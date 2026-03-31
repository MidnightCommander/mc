Perl syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `tests/syntax/samples/perl.pl`
Legacy reference: `misc/syntax/perl.syntax`
TS query: `misc/syntax-ts/queries-override/perl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[perl]`

Aligned with legacy
-------------------

- Keywords (`sub`) -> `yellow` - MATCH.
- Control flow keywords (`if`, `elsif`, `else`, `unless`, `while`, `until`,
  `for`, `foreach`, `do`, `last`, `next`, `goto`, `use`, `require`, `package`,
  `return`) -> `magenta` - MATCH.
- Logical keyword operators (`and`, `or`) -> `magenta` via `keyword.directive` -
  MATCH.
- `BEGIN`/`END` blocks -> `magenta` - MATCH.
- Builtin functions (`chomp`, `chop`, `defined`, `undef`, `eval`) -> `yellow` -
  MATCH.
- Arithmetic operators (`+`, `-`, `*`, `/`, `**`) -> `yellow` - MATCH.
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`, `<=>`) -> `yellow` -
  MATCH.
- String comparison operators (`eq`, `ne`, `lt`, `gt`, `le`, `ge`, `cmp`) ->
  `yellow` - MATCH.
- Logical operators (`&&`, `||`, `!`) -> `yellow` - MATCH.
- Regex match operators (`=~`, `!~`) -> `yellow` - MATCH.
- Arrow operator (`->`) -> `yellow` - MATCH.
- Assignment operator (`=`) -> `yellow` - MATCH.
- Dot/range operators (`.`, `..`) -> `yellow` - MATCH.
- Semicolons -> `brightmagenta` - MATCH.
- Commas -> `brightcyan` - MATCH.
- Brackets/parens (`(`, `)`, `[`, `]`, `{`, `}`) -> `brightcyan` - MATCH.
- Scalar variables (`$scalar`, `$name`) -> `brightgreen` - MATCH.
- Double-quoted strings -> `green` - MATCH.
- Single-quoted strings -> `green` (TS) vs `brightgreen` (legacy) - MINOR
  MISMATCH (TS uses same `@string` for both).
- Heredoc content -> `green` - MATCH.
- Comments (`#`) -> `brown` - MATCH.
- Regular expressions -> `brightgreen` via `string.special` - MATCH.
- Data section (`__END__`) -> `brown` - MATCH.
- Variable declarations (`my`, `our`, `local`) -> `magenta` - MATCH.
- `redo` keyword -> `magenta` - MATCH.

Intentional improvements over legacy
-------------------------------------

- TS correctly colors `@array` and `%hash` as `brightgreen` via
  `variable.special`, while legacy uses `white` for `@` arrays and `brightcyan`
  for `%` hashes -- TS is more consistent treating all variables uniformly.
- TS colors `@_` as `brightgreen` (regular variable), while legacy uses `red`
  (special variable) -- TS simplifies by not distinguishing special vars.
- TS properly highlights interpolated variables inside double-quoted strings
  (e.g., `$name` in `"Hello, $name!"` shows as `brightgreen` within `green`
  string).
- TS highlights command strings (backticks) as `green` string rather than legacy
  `white on black`.
- TS colors the `=>` fat comma as an operator (`yellow`) consistently, while
  legacy also does `yellow` -- both match but TS catches it structurally.
- TS recognizes `qr/pattern/` as `string.special` (`brightgreen`), matching
  legacy regex coloring.
- TS highlights labels (`DONE:`) with `cyan` via `@label`, while legacy has no
  specific label coloring (it appears as default text with `:` in `brightcyan`).
- TS recognizes `goto DONE` target as `cyan` label.

Known shortcomings
------------------

- TS does not distinguish single-quoted strings (`brightgreen` in legacy) from
  double-quoted strings (`green` in legacy) -- both render as `green`.
- TS does not highlight the shebang line (`#!/usr/bin/perl`) with
  `brightcyan on black` as legacy does -- TS renders it as `brown` (comment).
- TS does not color `print` as `yellow` (function) in all positions -- appears
  uncolored in some contexts where legacy consistently colors it `yellow`.
- TS does not color `die` as `yellow` -- appears uncolored while legacy
  highlights it.
- TS has a minor rendering issue with `%hash` where the `%` sigil shows as
  `yellow` (operator) separately from the variable name, rather than coloring
  the entire `%hash` token as one unit.
- Legacy colors many more builtin functions as `yellow` (e.g., `print`, `die`,
  `open`, `close`, `push`, `pop`, `split`, `join`, etc.) -- TS only covers
  `chomp`, `chop`, `defined`, `undef`, `eval` explicitly.
- TS does not distinguish Perl pragma modules (`strict`, `warnings`, `lib`,
  etc.) with `brightcyan` as legacy does.
