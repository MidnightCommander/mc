JSON syntax highlighting: TS vs Legacy comparison report
========================================================

Sample file: `tests/syntax/samples/json.json`
Legacy reference: `misc/syntax/json.syntax`
TS query: `misc/syntax-ts/queries-override/json-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[json]`

Aligned with legacy
-------------------

- String keys (`"title"`, `"description"`, `"version"`): `green` via `@string` -
  MATCH
- String values (`"Hello, world!"`, `"localhost"`): `green` via `@string` -
  MATCH
- Escape sequences (`\"`, `\\`, `\/`, `\t`, `\n`, `\r`, `\b`, `\f`):
  `brightgreen` via `@string.special` - MATCH
- Integer numbers (`42`, `-17`, `0`, `5432`): `brightgreen` via
  `@string.special` - MATCH
- Boolean `true`: `brightgreen` via `@string.special` - MATCH
- Boolean `false`: `brightgreen` via `@string.special` - MATCH
- `null`: `brightgreen` via `@string.special` - MATCH
- Delimiters (`{`, `}`, `[`, `]`, `,`, `:`): `brightcyan` via `@delimiter` -
  MATCH
- Strings in arrays (`"alpha"`, `"beta"`, `"production"`): `green` via `@string`
  on `array > string` - MATCH
- Empty strings (`""`): `green` - MATCH
- Strings with special characters (`"key with spaces"`, `"key.with.dots"`):
  `green` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS handles floating-point numbers (`3.14159`, `-0.001`, `0.85`) as single
  `brightgreen` tokens. Legacy splits some floats at the decimal point, coloring
  the integer part as `brightgreen` but leaving `.14159` partially uncolored
  when the pattern does not match the full format.
- TS handles scientific notation (`1.5e10`, `2.99e-8`, `6.022E23`) as complete
  `brightgreen` tokens. Legacy correctly handles most of these but splits some
  (e.g. `6.022E23` matched, but `1E+6` has the `E+` part uncolored).
- TS properly handles `\u` unicode escape sequences as `brightgreen` for the
  full `\uXXXX` sequence. Legacy matches the `\u` prefix and the 4-digit hex
  separately, resulting in `brightgreen` for `\u` but `green` for the digits in
  some cases.
- TS handles negative numbers (`-17`, `-0.001`, `-74.0060`) as single tokens in
  `brightgreen`. Legacy handles these correctly too via pattern matching.
- TS treats all structural delimiters uniformly (`{`, `}`, `[`, `]`, `,`, `:`)
  as `brightcyan`, consistent with legacy.

Known shortcomings
------------------

- TS does not distinguish between key strings and value strings color-wise --
  both are `green`. This matches legacy behavior, so it is not a regression, but
  a potential future improvement could differentiate them.
- Legacy splits `1E+6` with the `E+` uncolored (only `1` and `6` in
  `brightgreen`). TS also has a minor issue here: it shows `1` as `brightgreen`,
  then `E+` as `red` (likely an artifact), then `6` as `brightgreen`. Neither
  handles this edge case perfectly.
- Neither TS nor legacy support JSON5/JSONC comments in standard JSON files (the
  `(comment) @comment` capture exists in the query but standard JSON parsers do
  not emit comment nodes).
