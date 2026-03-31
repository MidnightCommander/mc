C syntax highlighting: TS vs Legacy comparison report
=====================================================

Sample file: `c.c`
Legacy reference: `misc/syntax/c.syntax`
TS query: `misc/syntax-ts/queries-override/c-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[c]`

Aligned with legacy
-------------------

- Keywords (`if`, `for`, `while`, `return`, etc.): `yellow` - MATCH
- Storage class (`auto`, `extern`, `static`, `register`): `yellow` - MATCH
- Preprocessor directives (`#include`, `#define`, etc.): `brightred` - MATCH
- Types (`int`, `char`, `float`, `void`, `short`, `long`, etc.): `yellow` -
  MATCH
- Constants (`NULL`, `true`, `false`): `lightgray` - MATCH
- Numbers: `lightgray` - MATCH
- Character literals: `brightgreen` - MATCH
- Strings: `green` - MATCH
- Escape sequences in strings (`\n`, `\t`, `\\`, etc.): `brightgreen` - MATCH
- Comments: `brown` - MATCH
- Semicolons: `brightmagenta` - MATCH
- Bitwise operators (`&`, `|`, `^`, `~`): `brightmagenta` - MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, etc.): `yellow` - MATCH
- Field operators (`.`, `->`): `yellow` - MATCH
- Brackets (`(`, `)`, `[`, `]`, `{`, `}`): `brightcyan` - MATCH
- Ternary operators (`?`, `:`): `brightcyan` - MATCH
- Labels (goto targets): `cyan` - MATCH
- Ellipsis (`...`): `yellow` - MATCH
- C11/C23 keywords (`_Bool`, `_Atomic`, `_Noreturn`, etc.): `yellow` - MATCH

Intentional improvements over legacy
-------------------------------------

- Preprocessor body: legacy colors the entire `#define` line as `brightred`. TS
  only colors the directive keyword (`#define`, `#include`) as `brightred`,
  leaving the macro name and body in their natural colors. This is more accurate
  since the body contains valid C tokens.
- Include paths: legacy colors `<stdio.h>` and `"header.h"` as `red`
  (preprocessor string). TS colors them as `green` (string). This is consistent
  with how strings are treated elsewhere.
- `size_t`: legacy does not recognize `size_t` (DEFAULT). TS recognizes it as a
  type (`yellow`) via the `primitive_type` node.
- Number suffixes: legacy fails to color `42L` (suffix breaks pattern). TS
  correctly colors the full `number_literal` as `lightgray`.
- Negative literals: legacy colors the minus in `-1` as `yellow` (operator). TS
  colors it as part of the number (`lightgray`). Both are reasonable; TS is
  arguably more correct for literal constants.
- `default` keyword in switch: legacy colors it as `cyan` (label color). TS
  colors it as `yellow` (keyword). TS is more correct since `default` is a
  keyword, not a user-defined label.

Known shortcomings
------------------

- Format specifiers (`%d`, `%s`, `%f`, etc.) inside strings are not colored as
  `brightgreen`. Tree-sitter does not parse `printf` format strings. Legacy
  colors them via regex patterns. This would require custom injection or
  post-processing which is not currently supported.
- Comment special words (`TODO`, `FIXME`, `NOTE`) are not highlighted with a
  different background. Legacy uses black-on-brown for these. This would require
  either comment injection or special handling in the TS engine.
- Digit separators (`1_000_000`) are not recognized by the C grammar as a
  `number_literal`. They appear as DEFAULT.
