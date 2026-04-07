Python syntax highlighting: TS vs Legacy comparison report
===========================================================

Sample file: `python.py`
Legacy reference: `misc/syntax/python.syntax`
TS query: `misc/syntax-ts/queries-override/python-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[python]`

Aligned with legacy
-------------------

- Language keywords (`if`, `else`, `elif`, `for`, `while`, `def`, `class`,
  `return`, `import`, `from`, `as`, `in`, `is`, `not`, `and`, `or`, `break`,
  `continue`, `pass`, `raise`, `try`, `except`, `finally`, `with`, `del`,
  `global`, `nonlocal`, `lambda`, `yield`, `assert`, `async`, `await`): `yellow`
  - MATCH.
- Operators (`+`, `-`, `*`, `**`, `/`, `//`, `%`, `=`, `==`, `!=`, `<`, `>`,
  `<=`, `>=`, `+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`, `<<=`, `>>=`, `|`,
  `&`, `^`, `~`, `<<`, `>>`, `->`, `:=`): `yellow` - MATCH.
- Colon `:`: `brightred` - MATCH.
- Brackets/parens (`(`, `)`, `[`, `]`, `{`, `}`): `brightcyan` - MATCH.
- Comma `,`: `brightcyan` - MATCH.
- Semicolons `;`: `brightmagenta` - MATCH.
- Strings (double-quoted `"..."`, single-quoted `'...'`, triple-quoted
  `"""..."""` and `'''...'''`): `green` - MATCH.
- Escape sequences (`\n`, `\t`, `\\`): `brightgreen` - MATCH.
- Comments (`# ...`): `brown` - MATCH.
- `self` keyword: `brightred` - MATCH.
- Dunder methods (`__init__`, `__repr__`, `__str__`, `__len__`, `__getitem__`):
  `lightgray` - MATCH.
- Decorators (`@staticmethod`, `@classmethod`, `@property`): `brightred` -
  MATCH.

Intentional improvements over legacy
-------------------------------------

- Dunder names used as attributes (e.g., `__slots__`): TS colors these as
  `brightred` via the `variable.builtin` capture when they match `^__.*__$` in
  identifier context. Legacy colors them as `lightgray` only when they appear as
  whole keywords. Both are reasonable; TS is more consistent since it applies to
  any dunder-pattern identifier.
- Dot `.` operator: legacy colors `.` as `white/Orange` (a custom color). TS
  does not explicitly capture dots, leaving them as default text. This avoids
  the unusual custom color.
- Bitwise operators `|`, `&`, `^`, `~`: TS colors these as `yellow` (via
  `operator.word`). Legacy does not color `|`, `&`, `^` (they appear as
  default). TS provides more complete operator coverage.
- Compound assignment `&=`, `|=`, `^=`: TS colors these as `yellow`. Legacy only
  colors the `=` portion as `yellow`, leaving the `&`/`|`/`^` prefix uncolored.
  TS treats the compound operator as a single token.
- `@` decorator syntax: TS colors the `@` as part of the decorator node and the
  decorator name as `brightred` via `function.special`. Legacy colors `@`
  followed by a known builtin name as `brightcyan` (from the builtins list), not
  as a decorator. TS is structurally more accurate.
- Walrus operator `:=`: TS colors it as `yellow` (unified operator). Legacy
  colors `:` as `brightred` and `=` as `yellow` separately, creating a
  split-color effect.

Known shortcomings
------------------

- Builtin functions (`len`, `range`, `open`, `print`, `int`, `str`, `list`,
  `dict`, `type`, `isinstance`, etc.): legacy colors these as `brightcyan` via a
  hardcoded keyword list of ~70 builtins. TS does not distinguish builtin
  functions from user-defined functions. `print` is colored as `yellow` by
  legacy (as a keyword) but uncolored by TS.
- String method names (`split`, `join`, `strip`, `upper`, `lower`, etc.): legacy
  colors these as `magenta` via a keyword list. TS does not color method names.
- Format specifiers in strings (`%s`, `%d`, etc.): legacy colors these as
  `brightgreen` inside string contexts. TS does not parse format specifiers
  within string nodes.
- F-string interpolation (`{self.name}`): TS colors the `{` and `}` delimiters
  inside f-strings as `brightcyan` and `self` as `brightred`. Legacy colors the
  entire f-string uniformly as `green`. TS provides richer f-string
  highlighting.
- Concatenated strings (`"hello " "world"`): TS captures both string parts as
  `green` via `concatenated_string`. Legacy also colors them `green`. Both
  match, though TS has a dedicated node type for this.
- The `__eq__` dunder in `def __eq__` line: TS colors it as `brightred`
  (matching `^__.*__$` via `variable.builtin`) rather than `lightgray` (which
  the `constant` capture would give). Legacy colors it as `brightred` (via
  `keyword whole __+__`). Both are `brightred` here - MATCH.
