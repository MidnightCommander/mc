Java syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `java.java`
Legacy reference: `misc/syntax/java.syntax`
TS query: `misc/syntax-ts/queries-override/java-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[java]`

Aligned with legacy
-------------------

- Language keywords (`abstract`, `assert`, `break`, `case`, `catch`, `class`,
  `continue`, `default`, `do`, `else`, `enum`, `extends`, `final`, `finally`,
  `for`, `if`, `implements`, `import`, `instanceof`, `interface`, `native`,
  `new`, `package`, `private`, `protected`, `public`, `return`, `static`,
  `strictfp`, `switch`, `synchronized`, `throw`, `throws`, `transient`, `try`,
  `volatile`, `while`): `yellow` - MATCH
- `this`, `super`: `yellow` - MATCH
- Primitive types (`byte`, `short`, `int`, `long`, `float`, `double`, `char`):
  `yellow` - MATCH
- `boolean`, `void`: `yellow` - MATCH
- Literals `true`, `false`, `null`: `yellow` - MATCH
- Operators (`+`, `-`, `*`, `/`, `%`, `=`, `==`, `!=`, `<`, `>`, `<=`, `>=`,
  `&&`, `||`, `!`, `&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`, `+=`, `-=`, `*=`,
  `/=`, `%=`, `++`, `--`, `->`): `yellow` - MATCH
- Semicolons `;`: `brightmagenta` - MATCH
- Brackets/parens/braces (`(`, `)`, `[`, `]`, `{`, `}`): `brightcyan` - MATCH
- Comma `,`: `brightcyan` - MATCH
- Dot `.`: `brightcyan` - MATCH (TS explicitly captures `.` as `delimiter`)
- Colon `:`: `brightcyan` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Character literals (`'A'`, `'\n'`): `brightgreen` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Annotations (`@Override`, `@SuppressWarnings`, `@Deprecated`): `brightred` -
  MATCH
- Labels (`outer:`): TS colors as `cyan` via `label` capture; legacy colors
  `outer` as `yellow` (matching the keyword `outer`). Different color but both
  intentional.

Intentional improvements over legacy
-------------------------------------

- Labels (`outer:`): TS colors the label identifier as `cyan` via the
  `labeled_statement` capture, which is semantically correct. Legacy does not
  have label detection; it colors `outer` as `yellow` because `outer` happens to
  be in the keyword list (a false positive from a different language era).
- Dot `.` in method chains (`System.err.println`): TS explicitly captures dots
  as `brightcyan` delimiters, providing consistent visual separation. Legacy
  also colors `.` but as part of the general punctuation keyword rule. Both are
  `brightcyan`.
- Escape sequences in strings (`\n`, `\t`, `\\`, `\"`): TS colors the entire
  string including escapes as `green` for the string node, with
  `character_literal` escape chars colored as `brightgreen` via
  `string.special`. Legacy also colors escape sequences as `brightgreen` inside
  string contexts. MATCH in behavior.
- Generic angle brackets `<>`: TS colors `<` and `>` as `yellow` (operator).
  Legacy also colors them `yellow`. MATCH.
- Ternary operator `?`: legacy colors `?` as `brightcyan`. TS does not
  explicitly capture `?` in the query, so it appears as default text. Minor
  difference.

Known shortcomings
------------------

- Object methods (`clone`, `equals`, `finalize`, `getClass`, `hashCode`,
  `notify`, `notifyAll`, `toString`, `wait`): legacy colors these as `brightred`
  via a keyword list. TS does not distinguish these from regular method names.
  `toString` is colored as default text by TS (except when preceded by
  `@Override`, where the annotation gets `brightred`).
- `record`, `sealed`, `permits`, `non-sealed`, `yield`: these newer Java
  keywords are in the TS query but the sample comments them out since they
  require newer Java syntax. Legacy does not have them at all.
- The `?` ternary operator: TS does not capture `?` explicitly. Legacy colors it
  as `brightcyan`. Minor visual difference in ternary expressions.
- Legacy keywords not in TS: `byvalue`, `cast`, `clone`, `def`, `equals`,
  `finalize`, `future`, `generic`, `getClass`, `goto`, `hashCode`, `inner`,
  `notify`, `notifyAll`, `outer`, `rest`, `toString`, `var`, `wait` are in
  legacy's keyword list but not all are standard Java keywords. TS only captures
  actual language keywords.
- Preprocessor-style directives (`#include`-like lines): legacy has a
  `context linestart # \n brightred` rule for C-style preprocessor lines that
  might appear in Java files. TS does not handle these since they are not valid
  Java syntax.
- String escape sequences: TS colors the entire string node as `green`
  uniformly. Individual escape sequences like `\n` are not separately
  highlighted within the string (the `string.special` capture applies to
  `character_literal`, not escape sequences inside strings). Legacy colors
  escape sequences as `brightgreen` inside string contexts.
