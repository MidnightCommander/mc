AWK syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `awk.awk`
Legacy reference: `misc/syntax/awk.syntax`
TS query: `misc/syntax-ts/queries-override/awk-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[awk]`

Aligned with legacy
-------------------

- Comments (`#`): `brown` - MATCH.
- Strings (double-quoted): `green` - MATCH.
- String escape sequences (`\t`, `\n`): `brightgreen` in legacy, not separately
  highlighted in TS (TS renders entire string as `green`) - PARTIAL MATCH.
- `BEGIN`/`END`: legacy uses `red`/`white`, TS uses `brightred` (via
  `@function.special`) - CLOSE MATCH.
- `function` keyword: `brightmagenta` - MATCH (legacy uses `brightmagenta`, TS
  uses `@delimiter.special`).
- Control flow keywords (`if`, `else`, `while`, `for`, `do`, `return`, `break`,
  `continue`, `in`, `exit`, `getline`, `next`, `nextfile`): `white` - MATCH
  (legacy uses `white/26`, TS uses `@keyword.other`).
- `print`/`printf`: `white` - MATCH (legacy uses `white/26`, TS uses
  `@keyword.other`).
- Field references (`$0`, `$1`, `$2`, `$NF`, `$i`): `brightred` - MATCH (legacy
  uses `brightred/18`, TS uses `@variable.builtin`).
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`, `=`): `yellow` - MATCH (legacy
  uses `yellow/24`, TS uses `@operator.word`).
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`): `yellow` - MATCH.
- Assignment operators (`+=`, `-=`, `*=`, `%=`): `yellow` - MATCH.
- Increment/decrement (`++`, `--`): `yellow` - MATCH.
- Regex match (`~`, `!~`): `yellow` - MATCH.
- Comma and semicolon delimiters: legacy uses `white/25` and `lightgray/19`
  respectively, TS uses `brightcyan` for both (via `@delimiter`) - DIFFERENT.

Intentional improvements over legacy
-------------------------------------

- `BEGINFILE`/`ENDFILE` (gawk extensions) are highlighted in `brightred` by TS
  (via `@function.special`). Legacy had no rule for these keywords, leaving them
  as default text.
- `func` (gawk shorthand for `function`) is highlighted in `brightmagenta` by
  TS. Legacy had it as `white`.
- `switch`/`case`/`default` keywords are highlighted in `white` by TS (via
  `@keyword.other`). Legacy had `switch` and `case` as default text (only
  `default` was not specifically listed).
- `delete` is highlighted in `white` by TS. Legacy also had `delete` as `white`.
- Regex literals (`/^#/`, `/pattern/`, `/exclude/`) are highlighted with
  `brightgreen` content inside `yellow` delimiters by TS. Legacy used `red` for
  the entire regex pattern.
- Logical operators (`&&`, `||`, `!`) are highlighted in `yellow` by TS. Legacy
  left `&&` and `||` as default text (they were not in the keyword list).
- Ternary operator (`?`, `:`) is highlighted in `yellow` by TS. Legacy had `?`
  and `:` as `white/25`.
- The `^` and `**` exponentiation operators are highlighted in `yellow` by TS.
  Legacy only matched `^` inconsistently.
- Compound assignment `^=` and `/=` are properly highlighted as `yellow` by TS.
  Legacy split these into separate characters (e.g., `^` then `=`).

Known shortcomings
------------------

- String escape sequences (`\t`, `\n`) and printf format specifiers (`%s`, `%d`,
  `%.2f`) are not separately highlighted by TS within strings. Legacy
  highlighted these in `brightgreen/16` inside strings. The TS grammar does not
  emit separate nodes for escape sequences within AWK strings.
- Builtin variables (`FS`, `OFS`, `NR`, `NF`, `FILENAME`, `ARGC`, `ARGV`,
  `ENVIRON`, etc.) are not highlighted by TS (rendered as default text). Legacy
  highlighted these in `brightblue`. The TS query has no capture for builtin
  variables.
- Builtin functions (`atan2`, `cos`, `sin`, `sqrt`, `gsub`, `index`, `length`,
  `match`, `split`, `sprintf`, `sub`, `substr`, `tolower`, `toupper`, etc.) are
  not highlighted by TS. Legacy highlighted these in `white` with black
  background.
- Array subscript brackets (`[key]`, `[0]`, `["HOME"]`) render as default text
  in TS. Legacy highlighted these in `magenta`.
- Curly braces `{` and `}` and parentheses `(` and `)` are not highlighted by TS
  (default text). Legacy highlighted these in `white/25`.
- Hex constants (e.g., `0xFF`) are not highlighted by TS. Legacy highlighted
  these in `magenta/6`.
- The shebang line `#!/usr/bin/awk -f` is rendered as `brown` (comment) by TS.
  Legacy highlighted it with `yellow`/`magenta` coloring.
- Pipe operator `|` (used in `"date" | getline today`) is not highlighted by TS.
- Number literals (e.g., `0`, `1`, `2`, `10`, `100`) are not highlighted by TS.
  Legacy also left most numbers as default unless they were part of specific
  patterns.
