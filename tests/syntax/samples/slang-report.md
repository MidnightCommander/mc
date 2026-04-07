S-Lang syntax highlighting: TS vs Legacy comparison report
===========================================================

Sample file: `tests/syntax/samples/slang.sl`
Legacy reference: `misc/syntax/slang.syntax`
TS query: `misc/syntax-ts/queries-override/slang-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[slang]`

Aligned with legacy
-------------------

- Keywords (`variable`, `define`, `if`, `else`, `while`, `foreach`, `forever`,
  `for`, `break`, `continue`, `return`, `switch`, `case`, `do`, `goto`,
  `struct`, `typedef`, `static`, `sizeof`, `using`, `delete`, `private`,
  `protected`, `public`, `namespace`) -> `white` - MATCH.
- Logical operators (`and`, `or`, `xor`, `not`) -> `white` - MATCH.
- Error blocks (`EXIT_BLOCK`, `ERROR_BLOCK`) -> `white` - MATCH.
- S-Lang types (`Char_Type`, `Integer_Type`, `String_Type`, `Double_Type`,
  `Float_Type`, `Array_Type`, `Assoc_Type`) -> `yellow` - MATCH.
- Double-quoted strings -> `green` - MATCH.
- Escape sequences inside strings (`\t`, `\n`, `\\`, `\"`, `\101`) ->
  `brightgreen` - MATCH.
- Format specifiers inside strings (`%s`, `%d`) -> `brightgreen` - MATCH.
- Comments (`%` to end of line) -> `brown` - MATCH.
- Preprocessor lines (`#include`, `#define`) -> `brightred` - MATCH.
- Include file in angle brackets (`<stdio.h>`) -> `red` - MATCH.
- Arithmetic operators (`+`, `-`, `*`, `/`, `=`, `==`, `!=`, `>`, `<`, `>=`) ->
  `white` - MATCH.
- Semicolons -> `white` - MATCH.
- Braces (`{`, `}`) -> `brightcyan` - MATCH.
- Parentheses (`(`, `)`) -> `brightcyan` - MATCH.
- Brackets (`[`, `]`) -> `brightcyan` - MATCH.
- Commas -> `brightcyan` - MATCH.
- Colons -> `brightcyan` - MATCH.
- Identifiers and numbers -> `lightgray` (default) - MATCH.

Intentional improvements over legacy
-------------------------------------

- The TS output is identical to legacy for this file. Since the slang grammar
  inherits from C and the S-Lang language closely resembles C syntax, both
  engines produce the same highlighting. The TS grammar uses C/C++ tree-sitter
  captures which happen to align perfectly with legacy's keyword-based approach
  for this sample.

Known shortcomings
------------------

- The slang grammar has no entry in `misc/syntax-ts/extensions`, meaning TS
  file-type detection may not automatically associate `.sl` files with the slang
  grammar. Legacy uses `Syntax.in` to map `.sl` files to `slang.syntax`.
- TS inherits from the C grammar rather than having a dedicated S-Lang parser.
  S-Lang-specific constructs like `EXIT_BLOCK`, `ERROR_BLOCK`,
  `EXECUTE_ERROR_BLOCK`, `orelse`, `andelse`, `loop`, and `typecast` are matched
  by legacy as keywords but may not be structurally recognized by the C
  tree-sitter parser.
- Legacy highlights `default` in switch/case as default text, while TS also
  leaves it uncolored -- neither marks it as a keyword, though in S-Lang
  `default` is not actually used (S-Lang uses a different switch syntax).
- The `++` operator after `count` shows as `white` (`++` and `;` merged) in both
  engines, but TS could potentially distinguish the increment operator
  structurally.
