PHP syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `tests/syntax/samples/php.php`
Legacy reference: `misc/syntax/php.syntax`
TS query: `misc/syntax-ts/queries-override/php-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[php]`

Aligned with legacy
-------------------

- Keywords (`if`, `elseif`, `else`, `while`, `for`, `foreach`, `do`, `switch`,
  `case`, `default`, `break`, `continue`, `return`, `class`, `extends`,
  `implements`, `function`, `new`, `try`, `catch`, `finally`, `throw`,
  `abstract`, `final`, `public`, `protected`, `private`, `static`, `const`,
  `use`, `namespace`, `require`, `require_once`, `include`, `include_once`,
  `echo`, `declare`, `global`, `goto`, `readonly`, `trait`, `interface`,
  `yield`, `match`) -> `brightmagenta` - MATCH.
- Alternative syntax keywords (`endfor`, `endforeach`, `endif`, `endswitch`,
  `endwhile`) -> `brightmagenta` - MATCH.
- `null`, `true`, `false` -> `brightmagenta` - MATCH.
- Operators (`=`, `==`, `===`, `!=`, `!==`, `<>`, `<`, `>`, `<=`, `>=`, `<=>`,
  `+`, `-`, `*`, `/`, `%`, `**`, `.`, `.=`, `+=`, `-=`, `*=`, `/=`, `&&`, `||`,
  `!`, `&`, `|`, `^`, `~`, `<<`, `>>`, `->`, `=>`, `??`, `::`) -> `white` -
  MATCH.
- Semicolons -> `brightmagenta` via `delimiter.special` - MATCH.
- Commas and colons -> `brightcyan` via `delimiter` - MATCH.
- Brackets/parens (`(`, `)`, `[`, `]`, `{`, `}`) -> `brightcyan` - MATCH.
- Variables (`$name`, `$age`, `$this`) -> `brightgreen` via `variable.special` -
  MATCH (legacy uses `brightmagenta` for `$this` specifically, TS uses
  `brightgreen` like other variables).
- Double-quoted strings -> `green` - MATCH.
- Single-quoted strings -> `brightgreen` (legacy) vs `green` (TS uses same
  `@string` capture) - MINOR MISMATCH.
- Heredoc body -> `green` - MATCH.
- Comments (`//`, `#`, `/* */`) -> `brown` - MATCH.
- Function calls (`strlen`, `array_map`) -> `yellow` via `@keyword` on function
  calls - MATCH.
- Function definitions (`add`, `gen`) -> function name not separately colored in
  either (both show as plain text after `function` keyword).
- UPPERCASE constants (`PHP_VERSION`, `API_KEY`) -> `white` via `keyword.other`
  - MATCH.
- Labels (`end:`) -> `cyan` via `@label` (legacy has no label support for PHP) -
  TS IMPROVEMENT.

Intentional improvements over legacy
-------------------------------------

- TS highlights `$this` consistently as `brightgreen` (variable), while legacy
  uses `brightmagenta` -- TS is more consistent.
- TS highlights escape sequences inside strings (`\n`) as `brightgreen`, giving
  visibility into escape codes.
- TS properly recognizes `namespace`, `use`, `trait`, `interface`, `match`,
  `fn`, `readonly`, `clone`, `instanceof` keywords that were added in newer PHP
  versions.
- TS highlights function call names (`strlen`, `array_map`, `unset`, `list`,
  `print`) as `yellow`, matching legacy's approach of coloring all PHP built-in
  functions.
- TS highlights labels (`end:`) with `cyan` via `@label` -- legacy has no label
  highlighting for PHP.
- TS recognizes method calls (`$dog->speak()`, `$e->getMessage()`) and scoped
  calls (`Dog::$count`) structurally.
- TS colors nowdoc body content as string, while legacy may not handle nowdoc
  syntax.
- TS handles the `match` expression (PHP 8.0) properly, which legacy predates.

Known shortcomings
------------------

- TS does not distinguish single-quoted strings (`brightgreen` in legacy) from
  double-quoted strings (`green`) -- both use `@string` mapped to `green`.
- TS does not color the `<?php` opening tag -- it appears as `white` (uncolored)
  in both legacy and TS.
- Legacy colors `define` as `brightmagenta` (keyword), while TS also correctly
  colors it as `brightmagenta`.
- TS does not highlight `xor` as `brightmagenta` keyword -- it appears uncolored
  while legacy would color it.
- TS does not color `clone` and `instanceof` keywords in all contexts where
  legacy does -- some appear uncolored.
- TS does not distinguish function definitions from function calls at the color
  level -- both use `@keyword` (`yellow`), though legacy also uses `yellow` for
  function names.
- The `#` hash comment on the last line shows as `brown` in both, but legacy
  does not properly close the comment context (it bleeds into any following
  line), while TS handles it correctly.
- TS handles the `abstract`, `final` modifiers without explicit color (they
  appear as default text before `class`/`function`), matching legacy behavior.
