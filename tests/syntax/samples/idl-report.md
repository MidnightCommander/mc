IDL syntax highlighting: TS vs Legacy comparison report
========================================================

Sample file: `idl.idl`
Legacy reference: `misc/syntax/idl.syntax`
TS query: `misc/syntax-ts/queries-override/idl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[idl]`

Aligned with legacy
-------------------

- Keywords (`module`, `interface`, `struct`, `union`, `enum`, `typedef`,
  `const`, `exception`, `valuetype`, `switch`, `case`, `default`, `void`, `in`,
  `out`, `inout`, `raises`, `readonly`, `attribute`, `oneway`, `context`):
  `yellow` - MATCH
- Type keywords (`short`, `long`, `float`, `double`, `string`, `wstring`, `any`,
  `sequence`, `fixed`, `Object`): `yellow` - MATCH
- Boolean type (`boolean`): `yellow` - MATCH
- Boolean literals (`TRUE`, `FALSE`): `yellow` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Char literals (`'X'`): `brightgreen` - MATCH
- Operators (`+`, `-`, `*`, `/`, `%`, `=`, `::`, `<<`, `>>`, `~`): `yellow` -
  MATCH
- Brackets (`{`, `}`, `(`, `)`, `[`, `]`): `brightcyan` - MATCH
- Punctuation (`,`, `:`, `<`, `>`): `brightcyan` - MATCH
- Semicolons (`;`): `brightmagenta` - MATCH

Intentional improvements over legacy
-------------------------------------

- Preprocessor directives (`#ifndef`, `#define`, `#include`, `#pragma`,
  `#endif`): TS colors these as `brightmagenta` via `keyword.control`. Legacy
  uses a line context starting with `#` colored as `brightred`. The TS approach
  captures the entire preprocessor statement more precisely.
- The `public` and `private` keywords (in valuetype): TS colors these as
  `yellow` via the keyword list. Legacy does not include `public` and `private`
  in its keyword list, leaving them as default text.
- System include strings (`<orb.idl>`): TS captures these via
  `system_lib_string` as `green`. Legacy colors the `<` and `>` as `yellow`
  operators and the path as `red` inside the preprocessor context.
- The `|` operator in `0x0A | 0x50`: legacy does not explicitly list `|` as an
  operator keyword in the IDL syntax file, leaving it uncolored. TS does not
  capture it either (not in the TS operator list), so both leave it as default
  text.
- The `map` type keyword: TS includes `map` in the keyword list. Legacy does not
  have `map` as a keyword.
- The `fixed` type with angle brackets (`fixed<10,2>`): TS colors `fixed` as
  `yellow` and the angle brackets as `brightcyan` delimiters. Legacy colors
  `fixed` as `yellow` and `<` / `>` as `yellow` operators.
- Comment at end of closing: the `// end module` comment on the last closing
  brace line is correctly colored `brown` by TS even after the `#endif`
  preprocessor. Legacy has issues with context transitions near the end of the
  file.

Known shortcomings
------------------

- The `unsigned`, `char`, `octet`, `wchar`, `wstring`, `fixed`, `native`, and
  `ValueBase` type keywords: FIXED -- now included in the type keywords list as
  `@keyword` (`yellow`), matching legacy.
- The closing comment `/* ... */` opening markers: legacy colors `/*` and `*/`
  as `brown` markers. TS colors the entire comment node uniformly as `brown`,
  which is cleaner but legacy also shows the markers.
- Preprocessor string content: legacy colors `"types.idl"` as `red` inside the
  preprocessor context. TS colors it as `green` (via `string_literal`), matching
  the standard string color. This is arguably better but differs from legacy.
- The `#endif` line with a trailing comment: TS colors `#endif` as
  `brightmagenta` and the `// _SAMPLE_IDL_` comment continuation also as
  `brown`. Legacy colors the entire line as `brightred`.
