Bash syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `bash.sh`
Legacy reference: `misc/syntax/sh.syntax`
TS query: `misc/syntax-ts/queries-override/bash-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[bash]`

Aligned with legacy
-------------------

- Language keywords (`if`, `then`, `else`, `fi`, `for`, `do`, `done`, etc.):
  `yellow` - MATCH
- Shell builtins (`echo`, `printf`, `set`, `shift`, `break`, etc.): `yellow` -
  MATCH
  NOTE: Legacy lists these as keywords. TS captures them via `#any-of?`
  predicate on `command_name` nodes, matching the same `yellow` color.
- External commands (`cat`, `ls`, `grep`, etc.): `cyan` - MATCH
- Security commands (`gpg`, `ssh`, `scp`, `openssl`, `md5sum`): `red` - MATCH
- Function definitions (`function name`, `name()`): `brightmagenta` - MATCH
- Function keyword: `brightmagenta` - MATCH
- Function parens `()`: `brightmagenta` - MATCH
- Comments: `brown` - MATCH
- Strings (double-quoted, single-quoted): `green` - MATCH
- Variable expansions (`$VAR`, `${VAR}`): `brightgreen` - MATCH
- Special variables (`$?`, `$#`, `$@`, `$*`, `$$`, `$!`, `$_`): `brightred` -
  MATCH
  Including the `$` sign (whole `$?` is red).
- Positional parameters (`$0`-`$9`): `brightred` - MATCH
  Including the `$` sign.
- `;;` in case statements: `brightred` - MATCH
- `;` semicolons: `brightcyan` - MATCH
- `{ }` braces in compound statements: `brightcyan` - MATCH
- Heredoc body: `green` - MATCH
- Backtick characters (`` ` ``): `brightred` - MATCH

Intentional improvements over legacy
-------------------------------------

- Shebang (`#!/bin/bash`): TS colors the entire line as `brightcyan` via a
  `(#match? "^#!")` predicate on comments. Legacy cannot distinguish shebangs
  from regular comments due to the `context # \n` rule taking precedence over
  the keyword pattern.
- Backtick content: legacy colors everything inside backticks as
  `brightred`/default. TS colors backtick delimiters as `brightred` but lets
  the content inside get normal command coloring (command names `cyan`, keywords
  `yellow`, etc.), matching standalone command behavior.
- Test operators (`-f`, `-d`, `-eq`, `-ne`, etc.): TS colors these as
  `brightcyan` (operator) in both `[ ]` and `[[ ]]` contexts. Legacy does not
  color them. This improves readability of test expressions.
- `$()` content: TS only colors the `$(` delimiter as `brightgreen`. The
  commands inside get normal coloring. Legacy colors the entire `$()` including
  content as `brightgreen`.
- `{ }` in `${VAR}`: TS correctly colors the closing `}` as `brightgreen`
  (part of the expansion). Earlier versions incorrectly colored it as
  `brightcyan` (delimiter).
- Square brackets `[ ]` `[[ ]]`: TS leaves these as DEFAULT (white), matching
  legacy behavior. The brackets are not colored.

Known shortcomings
------------------

- Format specifiers in `echo`/`printf` strings are not colored. Tree-sitter
  does not parse `printf` format strings inside bash string nodes.
- Heredoc content is colored as `green` (string) uniformly. Legacy has more
  nuanced heredoc handling with variable expansion coloring inside heredocs. TS
  does not currently apply injection for heredoc content.
- The `#any-of?` predicate for shell builtins relies on MC's predicate
  evaluator. The builtin list is hardcoded in the query file. New builtins
  require manual additions.
