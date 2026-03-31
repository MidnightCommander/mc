Smalltalk syntax highlighting: TS vs Legacy comparison report
==============================================================

Sample file: `tests/syntax/samples/smalltalk.st`
Legacy reference: `misc/syntax/smalltalk.syntax`
TS query: `misc/syntax-ts/queries-override/smalltalk-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[smalltalk]`

Aligned with legacy
-------------------

- Comments (double-quoted `"..."`) -> `brown` - MATCH.
- Strings (single-quoted `'...'`) -> `brightcyan` via `@tag` - MATCH.
- Special variables (`self`, `super`, `nil`) -> `yellow` via `@keyword` - MATCH.
- Boolean literals (`true`, `false`) -> `brightmagenta` via `@keyword.control` -
  MATCH.
- Keyword messages (`ifTrue:`, `ifFalse:`, `whileTrue:`, `do:`, `add:`,
  `value:`, `on:`, `return:`, `setName:`, `age:`, `with:`) -> `brightmagenta`
  via `@keyword.control` - MATCH.
- Binary operators (`>`, `<`, `+`, `<=`, `=`, `,`) -> `cyan` via `@label` -
  MATCH.
- Assignment (`:=`) -> `cyan` via `@label` - MATCH.
- Return operator (`^`) -> `brightred` via `@function.special` - MATCH (legacy
  uses `cyan` for `^` as generic operator, TS uses `brightred` which is more
  visually distinct).
- Statement separator (`.`) -> `brightred` via `@function.special` - MATCH with
  legacy's `brightred` for whole-right `.`.
- Temporaries delimiters (`|`) -> both engines highlight `|`. Legacy uses
  `cyan`, TS uses `brightred` via `@function.special`.
- Brackets (`[`, `]`, `(`, `)`, `{`, `}`) -> `brightcyan` via `@delimiter` in
  TS, `cyan` in legacy - close match.
- Cascade separator (`;`) -> `brightcyan` via `@delimiter` in TS, `cyan` in
  legacy - close match.
- `thisContext` -> `yellow` via `@keyword` in TS, not highlighted in legacy (no
  keyword entry).

Intentional improvements over legacy
-------------------------------------

- TS uses `@keyword.control` (`brightmagenta`) for keyword message selectors
  like `ifTrue:`, `whileTrue:`, `do:`, `add:`, providing consistent coloring for
  all keyword messages, not just the ones explicitly listed in legacy.
- TS highlights `thisContext` as `yellow` (`@keyword`), while legacy does not
  recognize it.
- TS highlights block arguments (`:each`, `:a`, `:b`, `:c`, `:ex`) as
  `lightgray` via `@constant`, distinguishing them from regular identifiers.
- TS highlights symbols (`#animal`) as `yellow` via `@keyword`, matching
  legacy's `#` operator color but extending to the whole symbol token.
- TS highlights character literals (`$A`) as `brightcyan` via `@tag`, giving
  them string-like coloring.
- TS structurally understands the difference between unary selectors, binary
  selectors, and keyword selectors, providing appropriate colors for each
  category.

Known shortcomings
------------------

- TS shows some lines with `RED` (uncolored/error) spans, notably after method
  signatures like `Animal >> isOlderThan:` and `Animal >> typeSymbol`. This
  suggests the tree-sitter parser has difficulty with Smalltalk's method
  definition syntax in some cases, producing error nodes.
- Legacy highlights class names (`Object`, `OrderedCollection`, `Array`,
  `Boolean`, etc.) as `brightgreen`, which TS does not replicate. TS shows
  `Object` as `yellow` (via `@keyword` for unary selector) and most class names
  as uncolored.
- Legacy highlights message names like `new`, `add`, `do`, `with`, `from` as
  `brightmagenta` even when used as plain unary messages, while TS only colors
  keyword messages (with colons).
- The `>>` method definition operator shows as `cyan` (`@label`) in TS but as
  `cyan` in legacy too -- however TS sometimes shows red error spans around the
  method name following `>>`.
- TS does not highlight the `!` method terminator as `brightred` the way legacy
  does -- it appears as `RED` (error/default).
- The first keyword in a multi-keyword message (e.g., `setName:` in `setName:
  aName age: anAge`) sometimes shows as `lightgray` instead of `brightmagenta`,
  while subsequent keywords color correctly. This appears to be a parser
  inconsistency.
