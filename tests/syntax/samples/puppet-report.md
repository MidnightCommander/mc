Puppet syntax highlighting: TS vs Legacy comparison
====================================================

Sample file: `tests/syntax/samples/puppet.pp`
Legacy reference: `misc/syntax/puppet.syntax`
TS query: `misc/syntax-ts/queries-override/puppet-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[puppet]`

NOTE: The TS puppet grammar produces significantly broken output. Large sections
  of code are mis-parsed, resulting in `<RED>` (error) color flooding most of
  the file. The upstream tree-sitter-puppet grammar has substantial parsing
  issues with common Puppet constructs.

Aligned with legacy
-------------------

Very few elements align due to pervasive parse errors. Where parsing succeeds
partially:

- The `class` keyword is recognized but colored `red` via `@error` rather than
  the intended `yellow` via `@keyword`. Legacy colors it `red` as a type.
- String literals in single quotes get `brightgreen` in some places where
  parsing succeeds, partially matching legacy's `brightgreen` for single-quoted
  strings.
- The `String` type annotation gets `yellow` via `@type` in a few places,
  matching legacy's yellow for language keywords.

Differences from legacy
-----------------------

- TS floods most of the file with `red` error color. Legacy provides clean,
  consistent coloring throughout the entire file with proper keyword, operator,
  variable, string, and comment highlighting.
- Legacy correctly colors variables (`$package_name`, `$port`, etc.) as `white`.
  TS either misses them entirely or they fall within error spans.
- Legacy colors operators (`=>`, `==`, `!=`, `+=`, `->`, `~>`) as `yellow`. TS
  rarely reaches these tokens due to parse failures.
- Legacy colors conditionals (`if`, `elsif`, `else`, `case`, `default`,
  `unless`) as `yellow`. TS mostly shows these within error-colored blocks.
- Legacy colors boolean values (`true`, `false`, `undef`) as `brightred`. TS
  does not reliably parse these.
- Legacy distinguishes resource types (`file`, `package`, `service`) in `red`,
  meta parameters (`require`, `notify`, `subscribe`) in `brightmagenta`, and
  functions (`template`, `notice`, `fail`, `epp`, `include`, `each`) in
  `brightred`. TS collapses most of these into error spans.
- Legacy properly handles string interpolation (`${variable}`) with `white`
  variable coloring inside `green` double-quoted strings. TS does not reach
  interpolation handling due to parse errors.
- Legacy colors comments (`# ...`) as `brown`. TS colors them as `red` (error)
  in most cases because the parser fails before reaching comment nodes.
- Legacy colors brackets, braces, and parentheses as `brightcyan`. TS
  occasionally gets these right but mostly they are absorbed by error regions.

Known shortcomings
------------------

- The tree-sitter-puppet grammar has severe parsing issues. Most Puppet
  constructs (class definitions, resource declarations, defined types, node
  definitions, conditionals, selectors, collectors, lambdas, function
  declarations) fail to parse correctly, resulting in `(ERROR)` nodes that map
  to `red` via the `@error` capture.
- The TS output is essentially unusable for real Puppet files. The grammar needs
  significant upstream fixes before it can match legacy quality.
- Specific constructs that fail: class with `inherits`, resource declarations
  with hash parameters, `case` statements, `unless` blocks, node definitions
  with regex, resource collectors (`<| |>`, `<<| |>>`), chaining arrows (`->`,
  `~>`), lambdas with `each`, and function declarations with return type
  annotations.
- The `@punctuation.special` capture for `$`, `@`, `@@`, and `${` interpolation
  delimiters is never exercised due to parse failures.
- The `@string.escape`, `@string.regex`, `@float`, `@number`, `@namespace`,
  `@method`, `@method.call`, `@function`, `@function.call`, `@variable`,
  `@variable.builtin`, `@boolean`, `@conditional`, `@keyword.operator`, and
  `@keyword.function` captures are mostly unreachable.
