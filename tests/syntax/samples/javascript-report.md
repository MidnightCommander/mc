JavaScript syntax highlighting: TS report
==========================================

Sample file: `javascript.js`
TS query: `misc/syntax-ts/queries-override/javascript-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[javascript]`
Legacy reference: none

Color assignments
-----------------

- Keywords (`var`, `let`, `const`, `if`, `else`, `for`, `while`, `do`, `switch`,
  `case`, `default`, `break`, `continue`, `return`, `throw`, `try`, `catch`,
  `finally`, `new`, `delete`, `typeof`, `instanceof`, `in`, `of`, `void`,
  `class`, `extends`, `static`, `import`, `export`, `from`, `as`, `async`,
  `await`, `function`, `yield`, `with`, `debugger`, `get`, `set`, `this`,
  `super`, `undefined`): `yellow`
- Comments (`//`, `/* */`): `brown`
- Strings (double-quoted, single-quoted): `green`
- Template strings: `green`
- Regex: `brightgreen` (via `string.special`)
- Numbers: `brightgreen` (via `number.builtin`)
- Boolean literals (`true`, `false`) and `null`: `brightgreen` (via
  `number.builtin`)
- Operators (`+`, `-`, `*`, `/`, `%`, `=`, `==`, `===`, `!=`, `!==`, `<`, `>`,
  `<=`, `>=`, `&&`, `||`, `++`, `--`, `+=`, `-=`, etc.): `yellow` (via
  `operator.word`)
- Arrow `=>` and spread `...`: `brightcyan` (via `operator`)
- Semicolons: `brightmagenta` (via `delimiter.special`)
- Brackets, parens, commas, colons: `brightcyan` (via `delimiter`)
- Labels (`outer:`, `inner:`): `cyan` (via `label`)

Design decisions
----------------

- Keywords and operators share `yellow` since JavaScript uses operators
  frequently alongside keywords and the legacy `js.syntax` (which existed before
  TS support) used yellow for both categories.
- `undefined` is mapped to `@keyword` (yellow) rather than `@number.builtin`
  (brightgreen) because it is technically a global property, not a literal like
  `true`/`false`/`null`. The TS query treats it as a keyword.
- Semicolons get `brightmagenta` (via `delimiter.special`) to visually
  distinguish statement terminators from structural delimiters like brackets and
  commas.
- Arrow `=>` and spread `...` use `brightcyan` (via `operator`) to distinguish
  them from arithmetic/assignment operators which use `yellow`.
- Numbers and boolean/null literals share `brightgreen` to group all "literal
  values" visually.

Known shortcomings
------------------

- The `as` keyword on the last line renders as `<RED>` instead of `yellow`. This
  is because `as` in plain JavaScript (not TypeScript) is parsed as part of an
  expression statement rather than a keyword node, so the tree-sitter parser
  does not emit it as a keyword in this context.
- Template string interpolation `${x}` partially renders: the closing `}` shows
  as `brightcyan` delimiter while the content inside loses string coloring. This
  is a known limitation of how template_string fragments interact with
  delimiters.
- Regex literal `/pattern/gi` shows the slashes in `yellow` (operator) rather
  than `brightgreen` (string.special) because the tree-sitter parser captures
  the regex node but the `/` delimiters are separate tokens matched by the
  operator rule.
- The `function*` generator syntax shows the `*` attached to `function` keyword
  in yellow rather than being separately highlighted.
