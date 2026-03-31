Ruby syntax highlighting: TS vs Legacy comparison report
========================================================

Sample file: `tests/syntax/samples/ruby.rb`
Legacy reference: `misc/syntax/ruby.syntax`
TS query: `misc/syntax-ts/queries-override/ruby-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[ruby]`

Aligned with legacy
-------------------

- Keywords (`if`, `elsif`, `else`, `unless`, `while`, `until`, `for`, `in`,
  `case`, `when`, `do`, `end`, `begin`, `rescue`, `ensure`, `retry`, `break`,
  `next`, `redo`, `return`, `def`, `class`, `module`, `alias`) -> `magenta` -
  MATCH.
- `self` and `super` -> `magenta` - MATCH.
- `nil`, `true`, `false` -> `brightred` via `function.special` - MATCH.
- Built-in methods (`require`, `require_relative`, `include`, `attr_reader`,
  `attr_writer`, `attr_accessor`, `public`, `private`, `protected`, `raise`) ->
  `magenta` - MATCH.
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`, `**`) -> `yellow` - MATCH.
- Comparison operators (`==`, `!=`, `===`, `<=>`, `<`, `>`, `<=`, `>=`) ->
  `yellow` - MATCH.
- Logical operators (`&&`, `||`, `!`, `&`, `|`, `^`, `~`) -> `yellow` - MATCH.
- Bitshift operators (`<<`, `>>`) -> `yellow` - MATCH.
- Range operators (`..`, `...`) -> `yellow` - MATCH.
- Assignment operators (`=`, `+=`, `-=`, `*=`, `/=`, `||=`, `&&=`) -> `yellow` -
  MATCH.
- Pattern matching operators (`=~`, `!~`) -> `yellow` - MATCH.
- Hash rocket (`=>`) -> `yellow` - MATCH.
- Brackets/parens (`(`, `)`, `[`, `]`, `{`, `}`) -> `brightcyan` - MATCH.
- Delimiters (`,`, `:`, `::`) -> `brightcyan` - MATCH.
- Double-quoted strings -> `green` - MATCH.
- Single-quoted strings -> `green` (TS) vs `brightgreen` (legacy) - MINOR
  MISMATCH.
- Symbols (`:symbol`, hash keys) -> `white` - MATCH.
- Global variables (`$global_var`) -> `brightgreen` - MATCH.
- Instance variables (`@instance_var`) -> `white` - MATCH.
- Comments (`#`) -> `brown` - MATCH.
- `not`, `and`, `or` keyword operators -> `magenta` - MATCH.
- `yield` -> `magenta` - MATCH.
- `undef` -> `magenta` - MATCH.
- `BEGIN`/`END` -> `magenta` (TS) vs `red` (legacy) - MISMATCH.

Intentional improvements over legacy
-------------------------------------

- TS properly highlights `BEGIN`/`END` as `magenta` keywords, consistent with
  other keywords, while legacy uses `red`.
- TS highlights escape sequences inside strings (e.g., `\t`, `\n`) as
  `brightgreen` via `string.special`, giving better visibility into string
  content.
- TS correctly colors regex content as `brightgreen` via `string.special`, while
  legacy does not distinguish regex internals.
- TS highlights `extend` as `magenta` via the `#match?` identifier rule, while
  legacy does not recognize it.
- TS correctly highlights `puts` calls contextually, while legacy colors them as
  `yellow` (function) -- both approaches are reasonable.
- TS highlights heredoc body content as `green` string, matching string
  semantics.
- TS highlights `%w[...]` word arrays as `green` string content.
- TS recognizes `:"dynamic symbol"` as a symbol construct.
- TS highlights hash key symbols (e.g., `a:` in `{a: 1}`) as `white` via
  `hash_key_symbol`.

Known shortcomings
------------------

- TS does not distinguish single-quoted strings (`brightgreen` in legacy) from
  double-quoted strings (`green`) -- both render as `green`.
- TS renders shebang line as `brown` (comment) while legacy uses
  `brightcyan on black`.
- TS does not highlight `puts` as `yellow` function -- appears uncolored in some
  positions where legacy colors it consistently.
- Legacy colors many class/module methods as `yellow` (e.g., `new`, `close`,
  `open`, `print`, `puts`, `eval`, `sort`, `push`, `pop`) that TS does not
  explicitly list.
- TS does not highlight system/special variables (`$DEBUG`, `$VERBOSE`, `STDIN`,
  `STDOUT`, `STDERR`, etc.) with `red` as legacy does.
- TS does not highlight math module constants (`Math::PI`, `Math::E`) or numeric
  class methods (`abs`, `modulo`) with color as legacy does.
- Block delimiter `|` for block parameters shows as `yellow` (operator) in both
  TS and legacy, but in TS it comes from the operator list while legacy gets it
  from the `|` operator keyword.
