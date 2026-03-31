TOML syntax highlighting: TS vs Legacy comparison report
========================================================

Sample file: `tests/syntax/samples/toml.toml`
Legacy reference: `misc/syntax/toml.syntax`
TS query: `misc/syntax-ts/queries-override/toml-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[toml]`

Aligned with legacy
-------------------

- Comments (`# ...`): `brown` via `@comment` - MATCH
- Table header brackets (`[`, `]`): `yellow` via `@keyword` - MATCH
- Array-of-tables brackets (`[[`, `]]`): `yellow` via `@keyword` - MATCH
- Double-quoted strings (`"TOML Example"`, `"localhost"`): `brightgreen` via
  `@string.special` - MATCH
- Boolean `true`: `brightcyan` via `@tag` - MATCH
- Boolean `false`: `brightcyan` via `@tag` - MATCH
- Simple integers (`42`, `8080`, `5432`): `brightcyan` via `@tag` - MATCH
- Float values (`9.99`, `0.05`, `1.5`, `3.14159`): `brightcyan` via `@tag` -
  MATCH
- Equals sign (`=`): `brightcyan` via `@operator` - MATCH (legacy colors `=` as
  part of the assignment context)

Intentional improvements over legacy
-------------------------------------

- TS highlights bare keys (variable names) as `lightgray` via `@variable`,
  clearly distinguishing them from other elements. Legacy colors keys as `white`
  (default context color), which is visually similar but less intentional.
- TS properly handles dotted keys (`physical.color`, `server.advanced`) with the
  dot separator colored `brightcyan` via `@delimiter` and each key segment as
  `lightgray`. Legacy does not distinguish dots from the key text.
- TS handles single-quoted literal strings (`'No \escapes here'`) as
  `brightgreen` via `@string.special`. Legacy did not color single-quoted
  strings on key-value lines.
- TS correctly handles multiline basic strings (`"""..."""`) and multiline
  literal strings (`'''...'''`) as complete `brightgreen` tokens. Legacy
  mishandled these, splitting the triple quotes incorrectly and leaving content
  partially uncolored or as `white` default.
- TS handles hex (`0xDEADBEEF`), octal (`0o755`), and binary (`0b11010110`)
  integer literals as complete `brightcyan` tokens. Legacy only recognized
  decimal integers, splitting these prefixed forms.
- TS handles special float values (`inf`, `-inf`, `nan`) as `brightcyan` via
  `@tag`. Legacy left these as default/uncolored.
- TS handles scientific notation floats (`5e+22`, `1e-7`) as complete
  `brightcyan` tokens. Legacy split these at the `e` character.
- TS properly colors date/time values (`2024-01-15`, `14:30:00`,
  `2024-01-15T14:30:00Z`, `2024-01-15T14:30:00-05:00`) as `brightgreen` via
  `@string.special`. Legacy colored the numeric parts as `brightcyan` with
  separators (`-`, `:`, `T`, `Z`) uncolored, resulting in fragmented
  highlighting.
- TS handles quoted keys (`"google.com"`) as `lightgray` via `@variable`. Legacy
  treated these as regular strings.
- TS handles negative integers (`-17`) and negative floats (`-0.001`) as
  complete tokens. Legacy split the minus sign from the number.
- TS handles inline table contents (`{x = 1, y = 2}`) with proper key/value
  coloring. Legacy treated the entire line after `=` uniformly.
- TS colors table header names (`server`, `database`, `products`) as `lightgray`
  via `@variable` inside the brackets. Legacy colors them as `brown` (the table
  header context color).
- TS colors array brackets (`[`, `]`) in value arrays as `yellow` via
  `@keyword`, matching the table bracket style. Legacy did not color value-level
  array brackets.

Known shortcomings
------------------

- Legacy colored table header names (text between `[` and `]`) as `brown`,
  creating a visual grouping effect. TS colors them as `lightgray` (same as bare
  keys), which is more uniform but loses the distinct section-header feel.
- The comma separator inside inline tables and arrays is colored `brightcyan` by
  TS via `@delimiter`, which differs from legacy where commas inside array
  values were part of the default context. This is a minor cosmetic difference.
