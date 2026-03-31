Haskell syntax highlighting: TS vs Legacy comparison report
============================================================

Sample file: `haskell.hs`
Legacy reference: `misc/syntax/haskell.syntax`
TS query: `misc/syntax-ts/queries-override/haskell-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[haskell]`

Aligned with legacy
-------------------

- Language keywords (`module`, `where`, `import`, `qualified`, `as`, `hiding`,
  `data`, `newtype`, `type`, `class`, `instance`, `deriving`, `do`, `let`, `in`,
  `case`, `of`, `if`, `then`, `else`, `infixl`, `infixr`, `forall`): `yellow` -
  MATCH
- Constructors and type names (`Tree`, `Leaf`, `Node`, `Shape`, `Circle`,
  `Rectangle`, `Person`, `Wrapper`, `Int`, `String`, `Double`, `Float`, `Char`,
  `Bool`, `IO`, `True`, `False`, `Show`, `Eq`): `white` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Char literals (`'a'`, `'\n'`, `'\\'`): `brightgreen` - MATCH
- Comments (line `--` and block `{- -}`): `brown` - MATCH
- Operators (`=`, `->`, `<-`): `yellow` - MATCH
- Symbol operators (`::`, `=>`, `|`, `\`, `@`, `~`, `&&`, `||`, `++`, `.`, `$`):
  `white` - MATCH
- Commas (`,`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Pragmas (`{-# LANGUAGE GADTs #-}`): TS colors these as `brightgreen` via
  `comment.special`. Legacy colors them as `green` (the `{-# ... #-}` context).
  Both are green-family colors; TS uses a slightly brighter shade.
- Function signatures (`area`, `classify`, `describe`, `depth`, `processIO`,
  `transform`, `numbers`, `hexAndOctal`, `charExamples`, `greeting`, `ops`,
  `evens`, `identity`, `main`): TS colors function names in type signatures as
  `brightcyan` via the `function` capture. Legacy does not distinguish function
  names from other identifiers.
- Numeric literals (`42`, `3.14`, `1.0e-5`, `0xFF`, `0o77`): TS colors these as
  `brightgreen` via `number.builtin`. Legacy also colors digits as `brightgreen`
  but uses regex patterns that may miss some formats. TS handles `0o77` (octal)
  which legacy does not match.
- The `_` wildcard pattern: legacy colors it as `brightmagenta` (matching the
  `_\[...\]` pattern for identifiers starting with underscore). TS leaves `_` as
  default text, which is more appropriate since `_` is a pattern wildcard, not a
  warning.
- Float literals like `3.14` and `5.0`: legacy splits these at the `.`
  character, coloring digits as `brightgreen` and `.` as `white`. TS colors the
  entire float literal uniformly as `brightgreen`.

Known shortcomings
------------------

- Module names in imports (`Data.List`, `Data.Map`, `Data.Maybe`): legacy colors
  these as `white` because uppercase identifiers match the constructor/type
  pattern. TS does not color module names in import statements, leaving them as
  default text.
- Brackets and parentheses (`(`, `)`, `[`, `]`): legacy colors these as
  `brightcyan`. TS does not capture brackets as delimiters, leaving them as
  default text.
- Curly braces (`{`, `}`): legacy colors these as `white`. TS does not capture
  them, leaving as default text.
- The semicolons (`;`): legacy colors them as `yellow`. TS colors them as
  `brightcyan` via the `delimiter` capture. Minor color difference.
- The backtick infix operator (`` `mod` ``): legacy colors the content between
  backticks as `white` via an exclusive context. TS does not have a special
  capture for backtick-quoted infix operators, so `mod` appears as default text.
- The `$` operator: legacy colors it as `yellow`. TS colors it as `white` via
  `keyword.other` (grouped with other symbol operators). Minor color difference.
- The `<` and `>` operators: legacy colors them as `yellow`. TS may not capture
  standalone `<` and `>` in all expression contexts since they are not in the
  explicit operator list of the TS query.
- String escape sequences (`\n`, `\t`, `\0`): legacy colors escape sequences
  inside strings as `brightgreen`. TS colors the entire string uniformly as
  `green`, not distinguishing escape sequences.
- The Haskell `where` block indentation-sensitive bindings and `do` notation are
  structurally parsed by tree-sitter, giving TS an advantage in correctness over
  legacy regex matching.
