CUDA syntax highlighting: TS vs Legacy comparison report
========================================================

Sample file: `cuda.cu`
Legacy reference: `misc/syntax/cuda.syntax`
TS query: `misc/syntax-ts/queries-override/cuda-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[cuda]`

Aligned with legacy
-------------------

- C++ keywords (`break`, `case`, `catch`, `class`, `const`, `constexpr`,
  `continue`, `default`, `delete`, `do`, `else`, `enum`, `explicit`, `extern`,
  `final`, `for`, `friend`, `if`, `inline`, `namespace`, `new`, `noexcept`,
  `override`, `private`, `protected`, `public`, `return`, `sizeof`, `static`,
  `struct`, `switch`, `template`, `throw`, `try`, `typedef`, `typename`,
  `union`, `using`, `virtual`, `volatile`, `while`): `yellow` - MATCH
- `true`, `false`, `this`: `yellow` - MATCH
- CUDA built-in variables (`threadIdx`, `blockIdx`, `blockDim`, `gridDim`,
  `warpSize`): `white` - MATCH
- CUDA function qualifiers (`__global__`, `__device__`, `__host__`): `white`
  (TS) vs `white` (legacy) - MATCH
- CUDA memory qualifiers (`__shared__`, `__constant__`, `__managed__`,
  `__restrict__`): `white` (TS) vs `white` (legacy) - MATCH
- CUDA force-inline qualifiers (`__noinline__`, `__forceinline__`): `white` -
  MATCH
- CUDA synchronization (`__syncthreads`, `__threadfence`): `white` - MATCH
- Preprocessor directives (`#include`, `#define`, `#ifdef`, `#if`, `#endif`,
  `#pragma`): `brightred` - MATCH
- Primitive types (`int`, `float`, `double`, `void`, `char`, `long`): `yellow` -
  MATCH
- Sized type specifiers (`unsigned int`, `signed int`): `yellow` - MATCH
- String literals: `green` - MATCH
- Character literals: `brightgreen` - MATCH
- Comments (line and block): `brown` - MATCH
- Semicolons: `brightmagenta` - MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`, `=`, `++`, `--`, `+=`, `-=`):
  `yellow` - MATCH
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`): `yellow` - MATCH
- Logical operators (`&&`, `||`, `!`): `yellow` - MATCH
- Shift operators (`<<`, `>>`): `yellow` - MATCH
- Scope resolution (`::`): `yellow` - MATCH

Intentional improvements over legacy
-------------------------------------

- Preprocessor body: legacy colors entire `#define` lines as `brightred`. TS
  only colors the directive keyword (`#define`, `#include`) as `brightred`,
  leaving macro body tokens in their natural colors. This is more accurate.
- Include paths: legacy colors `<cuda_runtime.h>` as `red` (preprocessor
  string). TS colors them as `green` (`system_lib_string`). This is consistent
  with general string coloring.
- `nullptr` keyword: TS colors it as `lightgray` (@constant) to distinguish it
  from other keywords. Legacy colors it the same as all keywords (`yellow`).
  Note: The TS query maps `nullptr` to `@keyword` but `null` to `@constant`,
  creating a subtle inconsistency where `nullptr` gets `lightgray` instead of
  `yellow`.
- `NULL` macro: TS colors it as `lightgray` (@constant) via the `(null)` node.
  Legacy does not recognize `NULL` as a keyword and leaves it as DEFAULT. TS
  correctly identifies this constant.
- Labels: TS recognizes `statement_identifier` nodes and colors goto labels
  (`done:`) as `cyan`. Legacy does not have label support.
- Bitwise operators (`&`, `|`, `^`, `~`): TS colors them as `yellow`
  (@operator.word), matching the rest of the operators. Legacy treats them
  inconsistently.
- Delimiter dots: TS colors `.` as `brightcyan` in CUDA built-in variable member
  access (`threadIdx.x`). Legacy leaves `.` as DEFAULT.

Known shortcomings
------------------

- CUDA qualifiers not highlighted in function signatures: TS uses `#match?`
  predicates on `(identifier)` nodes to match `__global__`, `__device__`,
  `__host__` etc. However, some positions (e.g., function signature context) may
  not match because the tree-sitter C++ grammar parses these as different node
  types. In the output, qualifiers in function declaration positions appear as
  DEFAULT while those in variable declarations match correctly.
- Brackets, parens, braces (`(`, `)`, `{`, `}`, `[`, `]`): legacy colors these
  as `brightcyan`. TS is inconsistent -- some are colored as `brightcyan` but
  many appear as DEFAULT. The TS query does not include bracket/brace/paren
  captures for CUDA (unlike the C++ query which has them). The `@delimiter` only
  covers `.`, `,`, `:`.
- Kernel launch syntax (`<<<...>>>`): TS does not have special handling for CUDA
  triple-angle-bracket syntax. The `<<<` and `>>>` are parsed as shift
  operators, leading to inconsistent coloring.
- Format specifiers inside strings are not highlighted. Tree-sitter does not
  parse printf format strings.
- Raw string literal: TS colors the entire `R"(...)"` as `green`. Legacy
  similarly colors it uniformly.
- `?` ternary operator and `->` arrow: legacy colors `?` as `brightcyan`. TS
  captures `->` as `@operator.word` but does not capture `?`.
