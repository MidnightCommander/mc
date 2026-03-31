MATLAB syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `matlab.m`
Legacy reference: `misc/syntax/octave.syntax`
TS query: `misc/syntax-ts/queries-override/matlab-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[matlab]`

Aligned with legacy
-------------------

- Control flow keywords (`if`, `else`, `elseif`, `end`, `for`, `while`,
  `switch`, `case`, `otherwise`, `try`, `catch`, `return`, `continue`,
  `break`, `function`, `endfunction`, `parfor`, `spmd`): `white` - MATCH
- Comments (`%`): `brown` - MATCH
- Strings (single-quoted): `green` - MATCH
- Operators (`+`, `-`, `*`, `/`, `=`, `<=`, `>=`, `==`, `~=`, `<`, `>`,
  `&&`, `||`, `.*`, `./`, `.^`): `brightcyan` - MATCH
- Delimiters (`(`, `)`, `[`, `]`, `{`, `}`, `,`, `;`): `brightcyan` -
  MATCH
- Built-in function names (`disp`, `fprintf`, `sprintf`, `sqrt`, `mod`):
  `yellow` - MATCH (via legacy keyword list)
- Class keywords (`classdef`, `properties`, `methods`, `events`,
  `enumeration`, `global`, `persistent`, `arguments`): handled
  identically in both engines.

TS and Legacy produce identical output for the sample file. The
highlighting is a perfect match across all token categories.

Known shortcomings
------------------

- Format specifiers (`%s`, `%d`, `%f`) inside strings are not colored.
  Neither engine distinguishes format specifiers from string content.
- Double-quoted strings (`"..."`) are colored as `green` by TS but the
  legacy octave.syntax treats them the same way, so this is consistent.
- Block comments (`%{ ... %}`) are colored as `brown` in both engines.
- The `classdef`, `properties`, `events`, and `enumeration` keywords are
  not colored as `white` by TS because they are not in the keyword list.
  Legacy also leaves them uncolored. Both engines only color `methods`
  as `yellow` (via the built-in function name list).
