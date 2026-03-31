C# syntax highlighting: TS vs Legacy comparison report
======================================================

Sample file: `c_sharp.cs`
Legacy reference: `misc/syntax/cs.syntax`
TS query: `misc/syntax-ts/queries-override/c_sharp-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[c_sharp]`

Aligned with legacy
-------------------

- Keywords (`abstract`, `as`, `async`, `await`, `base`, `break`, `case`,
  `catch`, `checked`, `const`, `continue`, `default`, `do`, `event`, `explicit`,
  `extern`, `finally`, `fixed`, `for`, `foreach`, `goto`, `if`, `implicit`,
  `in`, `is`, `lock`, `new`, `operator`, `out`, `override`, `params`, `partial`,
  `readonly`, `ref`, `return`, `sealed`, `sizeof`, `stackalloc`, `static`,
  `switch`, `this`, `throw`, `try`, `typeof`, `unchecked`, `unsafe`, `var`,
  `virtual`, `volatile`, `where`, `while`, `yield`): `yellow` - MATCH
- Type declaration keywords (`class`, `delegate`, `enum`, `interface`,
  `namespace`, `struct`, `record`): `white` - MATCH
- Access modifiers (`internal`, `private`, `public`): `brightred` - MATCH
- `using` keyword: `brightcyan` - MATCH
- Predefined types (`int`, `long`, `float`, `double`, `bool`, `string`, `char`,
  `byte`, `decimal`, `object`, `void`): `yellow` - MATCH
- `null`, `true`, `false` literals: `yellow` - MATCH
- Strings: `green` - MATCH
- Verbatim strings (`@"..."`): `green` - MATCH
- Interpolated strings (`$"..."`): `green` - MATCH
- Character literals: `brightgreen` - MATCH
- Comments: `brown` - MATCH
- Semicolons: `brightmagenta` - MATCH
- Brackets/parens (`(`, `)`, `[`, `]`, `{`, `}`): `brightcyan` - MATCH
- Delimiters (`.`, `,`, `:`): `brightcyan` - MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`, `=`, `+=`, etc.): `yellow` -
  MATCH
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`): `yellow` - MATCH
- Logical operators (`&&`, `||`, `!`): `yellow` - MATCH
- Null coalescing (`??`): `yellow` (TS) vs `brightcyan` (legacy uses `?` as
  delimiter) - close match
- Lambda (`=>`): `yellow` - MATCH
- Labels (`done:`): `cyan` (TS) vs DEFAULT (legacy) - TS improvement

Intentional improvements over legacy
-------------------------------------

- Preprocessor lines: legacy colors entire `#` lines as `brightmagenta`. TS does
  not handle C# preprocessor directives since they are not common in modern C#.
  This avoids false positives on comment lines starting with `#`.
- `protected` keyword: legacy colors it as `yellow` (general keyword). TS
  correctly colors it as `yellow` too, matching legacy. However, the TS query
  places it under `@keyword` rather than `@function.special`, which differs from
  the query definition. Looking at the output, `protected` appears as `yellow`
  in both.
- Dot-separated namespaces (`System.Collections.Generic`): TS colors `.` as
  `brightcyan` (delimiter), giving structure to qualified names. Legacy treats
  them as plain text with no delimiter coloring.
- `async`/`await` keywords: TS correctly colors both as `yellow`. Legacy does
  not include `async` or `await` in its keyword list, leaving them as DEFAULT.
- Bitwise operators (`&`, `|`, `^`, `~`): TS colors them as `yellow`
  (@operator.word). Legacy leaves them as DEFAULT or partial matches. TS
  provides consistent operator coloring.
- Labels: TS correctly identifies labeled statements (`done:`) and colors the
  label name as `cyan`. Legacy does not have label support.

Known shortcomings
------------------

- `get`/`set` property accessors: legacy colors them as `brightgreen`. TS does
  not highlight these contextual keywords because tree-sitter treats them as
  regular identifiers in most contexts.
- `value` keyword (in property setters): legacy colors it as `yellow`. TS does
  not capture it since it is a contextual keyword recognized only inside
  property setter bodies.
- Format specifiers inside strings (`%d`, `%s`, etc.): legacy has regex patterns
  for C-style format strings. TS does not parse string contents, so format
  specifiers are not highlighted.
- Escape sequences in strings (`\n`, `\\`, etc.): legacy colors them as
  `brightgreen` inside string contexts. TS does not distinguish escape sequences
  within regular strings (they are part of the `@string` capture). Only
  character literals get `brightgreen`.
- Verbatim string `\n`: legacy incorrectly colors `\n` inside `@"..."` as an
  escape (`brightgreen`). TS correctly treats the entire verbatim string as
  `green` since `\n` is literal text in verbatim strings.
