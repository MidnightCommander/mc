C++ syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `cpp.cpp`
Legacy reference: `misc/syntax/cxx.syntax`
TS query: `misc/syntax-ts/queries-override/cpp-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[cpp]`

Aligned with legacy
-------------------

- Keywords (`alignas`, `alignof`, `break`, `case`, `catch`, `class`, `co_await`,
  `co_return`, `co_yield`, `concept`, `const`, `consteval`, `constexpr`,
  `constinit`, `continue`, `decltype`, `default`, `delete`, `do`, `else`,
  `enum`, `explicit`, `extern`, `final`, `for`, `friend`, `goto`, `if`,
  `inline`, `mutable`, `namespace`, `new`, `noexcept`, `operator`, `override`,
  `private`, `protected`, `public`, `requires`, `return`, `sizeof`, `static`,
  `static_assert`, `struct`, `switch`, `template`, `throw`, `try`, `typedef`,
  `typename`, `union`, `using`, `virtual`, `volatile`, `while`): `yellow` -
  MATCH
- `true`, `false`, `nullptr`, `this`: `yellow` - MATCH
- Preprocessor directives (`#include`, `#define`, `#ifdef`, `#ifndef`, `#if`,
  `#else`, `#endif`, `#elif`): `brightred` - MATCH
- Primitive types (`int`, `char`, `float`, `double`, `void`, `short`, `long`,
  `bool`): `yellow` - MATCH
- Sized type specifiers (`signed int`, `unsigned int`): `yellow` - MATCH
- `auto` keyword: `yellow` - MATCH
- String literals: `green` - MATCH
- System lib strings (`<iostream>`): `green` (TS) vs `red` (legacy colors
  include paths as `red` inside preprocessor context)
- Raw string literals (`R"(...)"`): `green` - MATCH
- Character literals (`'A'`, `'\n'`, `'\\'`, `'\''`): `brightgreen` - MATCH
- Comments (line and block): `brown` - MATCH
- Semicolons: `brightmagenta` - MATCH
- Brackets/parens (`(`, `)`, `[`, `]`, `{`, `}`): `brightcyan` - MATCH
- Delimiters (`.`, `,`, `:`): `brightcyan` - MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, `=`, `++`, `--`, `+=`, `-=`):
  `yellow` - MATCH
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`): `yellow` - MATCH
- Logical operators (`&&`, `||`, `!`): `yellow` - MATCH
- Shift operators (`<<`, `>>`): `yellow` - MATCH
- Scope resolution (`::`): `yellow` - MATCH
- Labels (`done:`): `cyan` (TS) vs DEFAULT (legacy) - TS improvement

Intentional improvements over legacy
-------------------------------------

- Preprocessor body: legacy colors entire `#define` lines as `brightred`. TS
  only colors the directive keyword (`#define`, `#include`) as `brightred`,
  leaving the body in natural colors. This provides more accurate highlighting
  of macro definitions.
- Include paths: legacy colors `<iostream>` as `red` (preprocessor string). TS
  colors them as `green` (string via `system_lib_string` node). This is
  consistent with how strings are treated elsewhere.
- Bitwise operators (`&`, `|`, `^`, `~`): TS colors them as `brightmagenta`
  (@delimiter.special) to distinguish from arithmetic operators. Legacy colors
  them the same as all other operators. This helps differentiate bit
  manipulation from arithmetic.
- Labels: TS recognizes `statement_identifier` nodes and colors goto labels as
  `cyan`. Legacy does not have label support, leaving them as DEFAULT.
- `override` and `final` context keywords: TS correctly highlights both as
  `yellow` in all contexts. Legacy only highlights them when they appear as
  whole words but does not distinguish their special semantic role.
- Destructor tilde (`~Shape`): TS colors `~` as `brightmagenta`
  (@delimiter.special, bitwise operator). Legacy does not distinguish the
  destructor tilde from other context.

Known shortcomings
------------------

- Format specifiers (`%d`, `%s`, etc.) inside strings are not highlighted.
  Tree-sitter does not parse printf format strings. Legacy colors them as
  `brightgreen` via regex patterns.
- `friend class Other`: TS fails to highlight `friend` and `class` in the
  `friend class Other` declaration when it appears inside a function body
  (invalid context). The word `Other` appears as `red` instead of DEFAULT. This
  is a tree-sitter parse error edge case.
- Raw string literal delimiters: TS colors the `R"(` and `)"` parts
  inconsistently, with parentheses getting `brightcyan` (delimiter) while the
  content is `green`. Legacy colors the whole literal uniformly.
- `?` ternary operator: legacy colors it as `brightcyan`. TS does not capture
  `?` in the query, so it appears as DEFAULT.
- Alternative tokens (`and`, `or`, `not`, `xor`, `bitand`, `bitor`, `compl`,
  `and_eq`, `or_eq`, `xor_eq`, `not_eq`): TS now captures these as `yellow`
  via `@keyword`, matching legacy behavior.
