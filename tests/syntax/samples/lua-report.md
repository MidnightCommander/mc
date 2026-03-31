Lua syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `tests/syntax/samples/lua.lua`
Legacy reference: `misc/syntax/lua.syntax`
TS query: `misc/syntax-ts/queries-override/lua-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[lua]`

Aligned with legacy
-------------------

- Keywords (`and`, `break`, `do`, `else`, `elseif`, `end`, `for`, `function`,
  `goto`, `if`, `in`, `local`, `not`, `or`, `repeat`, `return`, `then`, `until`,
  `while`) -> `white` - MATCH.
- Constants (`true`, `false`, `nil`) -> `white` - MATCH.
- Operators (`=`, `==`, `~=`, `<=`, `>=`, `<`, `>`, `+`, `-`, `*`, `/`, `//`,
  `%`, `^`, `#`, `..`) -> `white` - MATCH.
- Brackets/parens/braces (`(`, `)`, `{`, `}`, `[`, `]`) -> `white` - MATCH.
- Delimiters (`.`, `,`, `;`, `:`, `::`) -> `white` - MATCH.
- Double-quoted strings -> `green` - MATCH.
- Single-quoted strings -> `green` - MATCH.
- Comments (`--`) -> `brown` - MATCH.
- Block comments (`--[[...]]`, `--[=[...]=]`) -> `brown` - MATCH.
- Library function calls (`print`, `type`, `tostring`, `tonumber`, `assert`,
  `error`, `pcall`, `xpcall`, `rawget`, `rawset`, `rawequal`, `setmetatable`,
  `getmetatable`, `require`, `ipairs`, `pairs`, `next`, `collectgarbage`,
  `loadfile`, `dofile`) -> `yellow` - MATCH.
- Dot-access library calls (`string.format`, `string.len`, `string.sub`,
  `table.concat`, `table.insert`, `math.abs`, `math.floor`, `io.read`,
  `io.write`, `os.clock`, `os.time`) -> `yellow` - MATCH.
- Global variables (`_VERSION`, `_G`) -> `brightmagenta` via `constant.builtin`
  - MATCH.
- Default context (identifiers, numbers) -> `lightgray` - MATCH.
- Labels (`::done::`) -> identifier shown but `::` delimiters as `white` in
  both.

Intentional improvements over legacy
-------------------------------------

- TS highlights the `goto` keyword as `white` and its target label identifier as
  `cyan` via `@label`, while legacy does not distinguish the label target.
- TS highlights label definitions (`::done::`) with `cyan` for the identifier,
  while legacy shows them in default color.
- TS colors function definitions (`function greet(name)`,
  `local function add(a, b)`) with function name as `yellow`, matching library
  function coloring.
- TS properly handles `break` as a statement node (`break_statement`) with
  `white`, while legacy only handles it as a keyword token.
- TS structurally understands function calls vs function definitions, providing
  accurate highlighting even for user-defined functions.
- TS handles escape sequences in strings (`\n`) showing them distinctly, while
  legacy uses `brightgreen` for escape sequences inside strings -- both provide
  visibility but legacy has slightly different color.

Known shortcomings
------------------

- TS colors long strings (`[[...]]`, `[==[...]==]`) the same as comments
  (`brown` for delimiters), while legacy colors them as `brightmagenta` -- this
  is a mismatch where legacy's `brightmagenta` for long strings is lost in TS
  (both the delimiters and content appear differently).
- TS does not highlight `%` as `white` operator when used as modulo -- it
  appears uncolored in `7 % 3` while other operators are correctly colored (this
  appears to be a parser issue where `%` is not matched).
- Legacy highlights escape sequences inside strings (`%d`, `%s`, `\n`, etc.) as
  `brightgreen`, while TS does not have explicit escape sequence highlighting
  for Lua strings.
- TS does not highlight the shebang line (`#!/usr/bin/env lua`) distinctly -- it
  shows as `brown` (comment), while legacy also uses `brown`.
- TS handles `goto` keyword coloring (`white`) but the target identifier after
  `goto` gets `cyan` label color which differs from legacy's plain handling --
  this is more of a style difference than a shortcoming.
