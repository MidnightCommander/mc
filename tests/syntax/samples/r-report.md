R syntax highlighting: TS vs Legacy comparison report
=====================================================

Sample file: `tests/syntax/samples/r.r`
Legacy reference: `misc/syntax/r.syntax`
TS query: `misc/syntax-ts/queries-override/r-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[r]`

Aligned with legacy
-------------------

- Comments (`# ...`): `brown` via `@comment` - MATCH
- Assignment operator `<-`: `brightred` via `@function.special` - MATCH
- Global assignment `<<-`: `brightred` via `@function.special` - MATCH
- Right assignment `->`: `brightred` via `@function.special` - MATCH
- Right global assignment `->>`: `brightred` via `@function.special` - MATCH
- `function` keyword: `yellow` via `@keyword` - MATCH
- Control flow `if`, `for`, `while`: `brightmagenta` via `@keyword.control` -
  MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, `^`): `yellow` via `@operator.word`
  - MATCH
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`): `yellow` via
  `@operator.word` - MATCH
- Logical operators (`&`, `&&`, `|`, `||`, `!`): `yellow` via `@operator.word` -
  MATCH
- Strings (`"hello"`, `"alpha"`, etc.): `brightgreen` via `@string.special` -
  MATCH
- Colon operator (`:`): `yellow` via `@operator.word` - MATCH
- Dollar sign (`$`): `yellow` via `@operator.word` - MATCH
- Tilde (`~`): `yellow` via `@operator.word` - MATCH
- Pipe operator (`|>`): `yellow` via `@operator.word` - MATCH
- Namespace operators (`::`, `:::`): `yellow` via `@operator.word` - MATCH
- Function calls (`library`, `c`, `seq`, `rep`, `data.frame`, `paste`, `mean`,
  `sum`, `sqrt`, `plot`, `hist`, `boxplot`, `apply`, `sapply`, `tapply`,
  `lapply`, `lm`, `summary`, `factor`, `levels`, `list`, `names`, `str`,
  `read.table`, `write.table`, `grep`, `gsub`, `abs`, `log`, `round`, `max`,
  `min`, `range`, `length`, `cor`, `t.test`, `matrix`, `rnorm`, etc.): `yellow`
  via `@keyword` on `call > function: (identifier)` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS colors `repeat`, `in`, `return`, `next`, `break` as `brightmagenta` via
  `@keyword.control`. Legacy colors `repeat` as unrecognized (default), `return`
  as `yellow`, and does not specifically highlight `next`/`break`. TS provides
  consistent control flow coloring.
- TS colors special constants `TRUE`, `FALSE`, `NA`, `NULL`, `Inf`, `NaN` as
  `lightgray` via `@constant`. Legacy does not recognize these as special
  tokens, leaving them uncolored. TS makes them visually distinct.
- TS colors `=` (used for assignment in function arguments) as `brightred` via
  `@function.special`, consistent with `<-`. Legacy colors `=` as `red`
  (slightly different shade). TS unifies all assignment operators.
- TS colors `<=` and `>=` as complete `yellow` tokens. Legacy splits them -- `<`
  or `>` as `yellow` then `=` as `red` -- causing inconsistent coloring.
- TS colors commas as `brightcyan` via `@delimiter`, providing subtle
  punctuation visibility. Legacy leaves commas uncolored.
- TS colors namespace LHS identifiers (`stats`, `base`) as `yellow` via `@type`.
  Legacy colors the `::` and everything after as a single yellow token but does
  not distinguish the namespace part.
- TS colors property names after `$` (e.g. `$name`, `$age`, `$score`) as
  `brightcyan` via `@property`. Legacy leaves these as default color.
- TS colors named parameters in function calls (e.g. `by`, `times`, `nrow`,
  `ncol`, `sep`, `data`, `header`, `breaks`, `digits`, `mu`) as `brightcyan` via
  `@property`. Legacy only colors some of these (like `nrow`, `ncol`) if they
  happen to match known function names.
- TS recognizes `toupper`, `require`, `filter`, `select` as function calls and
  colors them `yellow`. Legacy misses `toupper`, `require`, and dplyr verbs
  since they are not in its hardcoded function list.

Differences from legacy
-----------------------

- Legacy colors parentheses `(` `)` as `brightcyan` and brackets `[` `]` as
  `brightblue`, and braces `{` `}` as `white`. TS does not explicitly color
  brackets/braces/parentheses (they remain default color). This is a minor loss
  of visual structure.

Known shortcomings
------------------

- TS does not color brackets `[` `]`, braces `{` `}`, or parentheses `(` `)`
  with distinct colors. Legacy uses three different colors for these three
  bracket types, which aids readability.
- Neither TS nor legacy highlights numeric literals with a distinct color --
  numbers remain in default foreground.
- TS does not distinguish single-quoted strings from double-quoted strings. Both
  get `brightgreen` via `@string.special`, which matches legacy behavior.
