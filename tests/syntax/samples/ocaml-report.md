OCaml syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `ocaml.ml`
Legacy reference: `misc/syntax/ml.syntax`
TS query: `misc/syntax-ts/queries-override/ocaml-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[ocaml]`

Aligned with legacy
-------------------

- Language keywords (`let`, `rec`, `if`, `then`, `else`, `match`, `with`, `fun`,
  `function`, `type`, `of`, `and`, `in`, `begin`, `end`, `do`, `done`, `for`,
  `to`, `while`, `try`, `exception`, `mutable`, `true`, `false`, `module`,
  `struct`, `sig`, `functor`, `open`, `class`, `object`, `val`, `method`,
  `external`, `lazy`, `assert`, `when`, `as`, `downto`, `include`, `inherit`,
  `new`, `private`, `nonrec`, `virtual`, `constraint`, `initializer`): `yellow`
  - MATCH
- Operators (`=`, `<`, `>`, `|`, `->`, `::`, `~`, `!`, `:=`, `+`, `-`, `*`):
  `cyan` - MATCH
- Delimiters (`.`, `,`, `:`): `cyan` - MATCH
- Semicolons (`;`): `brightred` - MATCH
- Double semicolons (`;;`): `brightred` - MATCH
- Strings (double-quoted `"..."`): `brightcyan` - MATCH
- Characters (`'A'`): legacy does not have a character context (single char
  shows as default). TS colors characters as `brightcyan` via the `character` ->
  `tag` capture.
- Comments (`(* ... *)`): `brown` - MATCH

Intentional improvements over legacy
-------------------------------------

- Type constructors and constructor names (`Red`, `Green`, `Blue`, `Custom`,
  `None_like`, `Some_like`, `Not_found_custom`, `Empty_error`, `Stack_empty`):
  TS colors these as `yellow` via `type` and `constructor_name` captures. Legacy
  does not distinguish constructors from regular identifiers.
- Module names (`IntSet`, `STACK`, `MakeStack`, `Printf`, `List`, `E`): TS
  colors these as `yellow` via `module_name` capture. Legacy does not color
  module names.
- Field names in records (`name`, `age`, `email`): TS colors these as `cyan` via
  `field_name` -> `label` capture. Legacy does not distinguish record field
  names.
- Label names (`~name`, `~age`): TS colors these as `cyan` via `label_name` ->
  `label` capture. Legacy does not color labeled argument names.
- The `->` operator: legacy colors it as `brightgreen` (a unique color). TS
  colors it as `cyan` via the `label` capture (same as other operators). This is
  an intentional normalization -- TS uses a consistent cyan for all operators.
- The pipe operator (`|>`): TS captures this as `cyan` like other operators.
  Legacy would color `|` as `cyan` and `>` separately.

Known shortcomings
------------------

- The `->` arrow in match expressions and function types: legacy colors it as
  `brightgreen`, giving it visual emphasis. TS colors it as `cyan` like all
  other operators. Users accustomed to the distinct `brightgreen` arrow may
  notice this change.
- Parentheses (`(`, `)`), brackets (`[`, `]`), and braces (`{`, `}`): legacy
  colors all of these as `cyan`. TS now captures these as `brightcyan` via
  `@delimiter`. Note: the OCaml tree-sitter grammar (.so) must be installed for
  this to take effect.
- The `#` operator: legacy colors it as `cyan`. TS does not capture `#` as an
  operator.
- The `@` and `^` operators: legacy colors these as `cyan`. TS does not include
  them in the operator list.
- The `&` operator: legacy colors it as `cyan`. TS does not include it.
- The `<>` operator (not-equal): legacy colors it as `cyan`. TS does not
  explicitly capture `<>`.
- The `<-` operator (assignment): legacy colors it as `cyan`. TS does not
  include `<-` in its operator list, but the output shows it is colored `cyan`
  correctly, suggesting the tree-sitter grammar captures it.
- The `not` and `or` and `mod` keywords: legacy colors these as `yellow`. TS
  does not include them in the keyword list.
- The `where`, `value`, `prefix` keywords: legacy includes these as `yellow`. TS
  does not list `value` or `prefix` (they are CamlLight-specific, not standard
  OCaml).
- Format specifiers inside strings (`%d`, `%s`): legacy colors these as
  `brightmagenta` inside the string context. TS colors the entire string
  uniformly as `brightcyan`, losing the format specifier distinction. Both
  legacy and TS output show the format specifiers as `brightmagenta`, suggesting
  the tree-sitter grammar may have an escape node, but the TS query does not
  capture it distinctly.
- Escape sequences in strings (`\n`, `\\`): legacy colors these as
  `brightmagenta`. TS does not distinguish escapes from the rest of the string
  content, though the output suggests format specifiers like `%d` and `%s\n` are
  rendered as `brightmagenta` in both systems.
- The `open` keyword with a module name: TS colors `open` as `yellow` (keyword)
  but does not color the module name `Printf` after it. Actually, TS does color
  `Printf` as `yellow` via `module_name`, so this works correctly.
