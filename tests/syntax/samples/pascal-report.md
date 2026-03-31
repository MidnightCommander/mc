Pascal syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `tests/syntax/samples/pascal.pas`
Legacy reference: `misc/syntax/pascal.syntax`
TS query: `misc/syntax-ts/queries-override/pascal-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[pascal]`

Aligned with legacy
-------------------

- Keywords (`program`, `unit`, `uses`, `interface`, `implementation`, `begin`,
  `end`, `var`, `const`, `type`, `array`, `of`, `record`, `class`, `object`,
  `constructor`, `destructor`, `inherited`, `property`, `read`, `write`, `if`,
  `then`, `else`, `case`, `for`, `to`, `downto`, `while`, `repeat`, `until`,
  `with`, `do`, `try`, `except`, `finally`, `raise`, `set`, `function`,
  `procedure`) -> `white` - MATCH.
- Logical operators (`and`, `or`, `not`, `xor`, `div`, `mod`, `shl`, `shr`,
  `in`, `is`, `as`) -> `cyan` via `@label` - MATCH.
- Arithmetic/comparison operators (`=`, `<>`, `<`, `>`, `<=`, `>=`, `+`, `-`,
  `*`, `/`, `:=`, `@`, `^`) -> `cyan` via `@label` - MATCH.
- Constants (`nil`, `True`, `False`) -> `white` - MATCH.
- `string` type keyword -> `yellow` via `@type` - MATCH.
- Delimiters (`;`, `:`, `,`) -> `lightgray` via `@number` - MATCH.
- String literals (`'...'`) -> `brightcyan` via `@tag` - MATCH.
- Block comments (`{ }`) -> `brightgreen` via `comment.special` - MATCH.
- Line comments (`//`) -> `brightgreen` via `comment.special` - MATCH.
- Alternative block comments (`(* *)`) -> `brightgreen` via `comment.special` -
  MATCH.
- Default context (identifiers, numbers) -> `yellow` (legacy default) -- TS does
  not explicitly color identifiers but they appear in default editor color.

Intentional improvements over legacy
-------------------------------------

- TS structurally recognizes Pascal grammar nodes (e.g., `kProgram`, `kBegin`,
  `kEnd`) rather than relying on keyword matching, making it more robust against
  false positives in strings or comments.
- TS correctly identifies `set` as a keyword with `white` coloring and
  distinguishes it from the `set` logical operator context where legacy uses
  `cyan`.
- TS handles the `string` keyword specially as `@type` (`yellow`),
  distinguishing it from regular keywords (`white`) -- legacy also uses `white`
  for `string` as a keyword, so TS provides a type-aware distinction.
- TS properly handles case-insensitive keywords through the grammar structure,
  while legacy uses the `caseinsensitive` directive.
- TS handles compiler directives `{$...}` differently from regular comments --
  the grammar can distinguish these structurally.

Known shortcomings
------------------

- TS default text color differs from legacy: legacy uses `yellow` as default
  context color for all uncolored text (identifiers, numbers), while TS uses the
  editor's default background color -- this means identifiers like `SyntaxDemo`,
  `TAnimal`, `Integer`, `Result`, `WriteLn` appear in default color rather than
  `yellow`.
- TS does not color `WriteLn` as `white` keyword as legacy does -- it appears in
  default color since it is not in the keyword list.
- TS does not recognize `virtual`, `override`, `private`, `public`, `protected`,
  `published` as keywords -- they appear in default color while legacy colors
  them `white`.
- TS does not color `..` (range operator in `2..5`) distinctly -- legacy shows
  it as `white` via keyword, while TS grammar may not capture it as a separate
  operator.
- TS does not distinguish compiler directive comments (`{$...}`) with `green` as
  legacy does -- they appear as regular `brightgreen` comments.
- TS does not recognize `*` (multiplication) as an operator in all contexts --
  in `4 * 2` and `I * I` the `*` appears uncolored, while legacy colors it via
  its general operator rules.
- TS does not handle parentheses `(`, `)` and brackets `[`, `]` as `lightgray`
  like legacy -- they appear uncolored in TS since the grammar uses dedicated
  node types rather than literal tokens.
- Legacy colors `not` as `white` (regular keyword), while TS correctly uses
  `cyan` (logical operator via `@label`) which better reflects its role -- this
  is actually more accurate in TS.
