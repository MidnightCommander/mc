D syntax highlighting: TS vs Legacy comparison report
=====================================================

Sample file: `d.d`
Legacy reference: `misc/syntax/d.syntax`
TS query: `misc/syntax-ts/queries-override/d-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[d]`

Aligned with legacy
-------------------

- Keywords (`abstract`, `alias`, `assert`, `break`, `case`, `cast`, `catch`,
  `class`, `const`, `continue`, `debug`, `default`, `delegate`, `delete`,
  `deprecated`, `do`, `else`, `enum`, `export`, `extern`, `final`, `finally`,
  `for`, `foreach`, `foreach_reverse`, `function`, `goto`, `if`, `interface`,
  `invariant`, `in`, `is`, `lazy`, `mixin`, `module`, `new`, `out`, `override`,
  `package`, `pragma`, `private`, `protected`, `public`, `return`, `scope`,
  `static`, `struct`, `switch`, `synchronized`, `template`, `throw`, `try`,
  `typeid`, `typeof`, `union`, `unittest`, `version`, `while`, `with`): `yellow`
  - MATCH
- `import` keyword: `magenta` - MATCH
- Types (`auto`, `bool`, `byte`, `ubyte`, `short`, `ushort`, `int`, `uint`,
  `long`, `ulong`, `float`, `double`, `real`, `char`, `wchar`, `dchar`,
  `string`, `wstring`, `dstring`, `void`): `yellow` - MATCH
- `true`, `false`, `null`, `super`, `this`: `brightred` - MATCH
- String literals: `green` - MATCH
- Raw strings (`r"..."`): `green` - MATCH
- Hex strings (`x"..."`): `green` (TS) vs `brightgreen` (legacy) - CLOSE (legacy
  treats hex strings specially)
- Token strings (`q{...}`): `green` - MATCH
- Character literals: `brightgreen` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Semicolons: `brightmagenta` - MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`, `=`, `++`, `--`, `+=`, `-=`,
  `*=`, `/=`, `%=`): `yellow` - MATCH
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`): `yellow` - MATCH
- Logical operators (`&&`, `||`, `!`): `yellow` - MATCH
- Bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`, `&=`, `|=`, `^=`, `<<=`,
  `>>=`): `yellow` - MATCH
- Power operator (`^^`): `yellow` - MATCH
- Range operator (`..`): `yellow` (TS) vs `yellow` (legacy) - MATCH
- Lambda operator (`=>`): `yellow` - MATCH
- Labels (`end:`): `cyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Special keywords (`__FILE__`, `__LINE__`): TS colors them as `brightred`
  (@function.special). Legacy colors them as `red`. TS uses a brighter variant
  for better visibility.
- `size_t` and `ptrdiff_t` types: TS correctly highlights these as `yellow`
  (@type). Legacy does not recognize them, leaving them as DEFAULT.
- `immutable`, `nothrow`, `pure`, `shared` keywords: TS correctly highlights
  these as `yellow` (@keyword). Legacy includes some of these but TS provides
  more consistent coverage.
- Module fully-qualified name (`syntax_demo` after `module`): TS colors it as
  `yellow` (@type) via the `module_fqn` capture. Legacy does not highlight the
  module name.
- Delimiter distinction: TS separates `.` and `,` as `brightcyan` (@delimiter)
  from operators. Legacy colors many delimiters as `brightcyan` too but is
  inconsistent with `:` (legacy uses `brightcyan`, TS uses `yellow` as
  @operator.word for `:` since it appears in slice expressions).
- `Object` class name: TS does not special-case `Object` (appears as DEFAULT).
  Legacy colors it as `brightmagenta` (special object). TS approach is more
  accurate since `Object` is a library type, not a language keyword.

Known shortcomings
------------------

- Brackets, parens, braces (`(`, `)`, `{`, `}`, `[`, `]`): legacy colors these
  as `brightcyan`. TS does not capture them in the D query. Many brackets appear
  as DEFAULT in TS output. Only a few contexts (array literals, function params)
  get colored via other captures.
- Operator overload names (`opAdd`, `opSub`, `opCmp`, etc.): legacy colors these
  as `gray` (special operator names). TS does not recognize them as they are
  regular identifiers to tree-sitter.
- D array/type attributes (`.length`, `.ptr`, `.sizeof`, `.init`, etc.): legacy
  highlights these as `yellow` via `wholeright` rules. TS does not recognize
  property-style attributes.
- Hex string: TS colors `x"48 65 6C 6C 6F"` as `green` (generic string). Legacy
  colors it as `brightgreen` to distinguish hex strings from regular strings.
- Nested block comments (`/+ ... +/`): legacy has support for D's nestable block
  comments. TS should support them via the grammar but the sample does not test
  them.
- `lazy` keyword context issue: TS output shows `lazy` followed by `int` colored
  as `red` (error node) in certain positions, suggesting a tree-sitter parse
  issue with `lazy` parameter declarations outside of function signatures.
- `?` ternary and `$` operators: legacy colors `?` and `$` as `brightcyan`. TS
  does not capture these.
- D-specific predefined entities (`_argptr`, `_arguments`): legacy colors these
  as `brightred`. TS does not recognize them.
- Escape sequences inside strings: legacy colors `\n`, `\\`, etc. as
  `brightgreen` within string contexts. TS does not distinguish escapes inside
  strings (all part of `@string` in `green`).
