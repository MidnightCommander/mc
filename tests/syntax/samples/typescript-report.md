TypeScript syntax highlighting: TS vs Legacy comparison
=======================================================

Sample file: `typescript.ts`
Legacy reference: `misc/syntax/ts.syntax`
TS query: `misc/syntax-ts/queries-override/typescript-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[typescript]`

Aligned with legacy
-------------------

- Language keywords (`import`, `from`, `interface`, `readonly`, `type`,
  `extends`, `enum`, `abstract`, `class`, `private`, `protected`, `public`,
  `static`, `constructor`, `get`, `set`, `new`, `return`, `const`, `let`, `var`,
  `async`, `await`, `function`, `if`, `else`, `for`, `of`, `break`, `continue`,
  `switch`, `case`, `default`, `try`, `catch`, `finally`, `throw`, `delete`,
  `typeof`, `instanceof`, `in`, `yield`, `declare`, `namespace`, `export`,
  `implements`, `keyof`, `infer`, `as`, `override`, `satisfies`): `yellow` -
  MATCH
- `this` and `super`: `yellow` - MATCH
- Boolean literals (`true`, `false`): `brightgreen` - MATCH
- Predefined types (`string`, `number`, `boolean`, `any`, `unknown`, `void`):
  `cyan` - MATCH
- `null` and `undefined`: `cyan` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Double-quoted strings (`"..."`): `green` - MATCH
- Template strings (`` `...` ``): `green` - MATCH
- Numbers (`42`, `3.14`, `0xff`): `brightgreen` - MATCH
- Arithmetic/comparison operators (`+`, `-`, `*`, `/`, `%`, `**`, `=`, `==`,
  `===`, `!=`, `!==`, `>`, `<`, `>=`, `<=`, `&&`, `||`, `!`, `&`, `|`, `^`, `~`,
  `<<`, `>>`, `>>>`): `yellow` - MATCH
- Arrow operator (`=>`): `brightcyan` - MATCH
- Semicolons (`;`): `brightmagenta` - MATCH
- Brackets and delimiters (`(`, `)`, `[`, `]`, `{`, `}`, `,`, `:`): `brightcyan`
  - MATCH
- Template string interpolation (`${...}`): `yellow` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS captures `null` and `undefined` as `cyan` via the `@label` capture,
  matching the legacy `cyan` color for basic types. Legacy lists these under
  "Basic Types" as `cyan` keywords. Both produce the same visual result.
- TS captures predefined types (`string`, `number`, `boolean`, `any`, `unknown`,
  `void`, `never`, `object`, `symbol`, `bigint`) as `cyan` via the
  `predefined_type` node. Legacy lists these individually as `cyan` keywords. TS
  is more robust since it uses the grammar's type system rather than string
  matching.
- Regular expressions: TS colors regex patterns as `brightgreen` via
  `@string.special`. Legacy colors the regex delimiters and content using
  operator/bracket rules which produces a mix of `yellow` and `brightcyan`. The
  TS approach provides cleaner, more consistent regex highlighting.
- The `.` dot operator: TS captures it as `yellow` via `@operator.word`. Legacy
  also colors `.` as `yellow`. Both match.
- `void` as a keyword (e.g., `void 0`): TS colors it as `cyan` (predefined_type)
  when used as a type annotation, but legacy colors it as `cyan` everywhere.
  Both produce `cyan`.
- Binary (`0b1010`), octal (`0o77`), and underscore- separated (`1_000_000`)
  number literals: legacy does not color these as `brightgreen` (its regex
  patterns only match decimal and hex). TS colors all number node types as
  `brightgreen`.

Known shortcomings
------------------

- TS does not color `Date`, `Error`, `Promise`, `Array`, `Map`, `Set`,
  `console`, `JSON`, `Math`, `Object`, `RegExp`, `Number`, `Boolean`, `String`,
  `Function`, `Buffer`, `process`, `setTimeout`, `setInterval`, `require`,
  `fetch`, and other built-in objects/functions as `yellow`. Legacy has an
  extensive hardcoded list of ~100+ built-in objects and common functions. TS
  intentionally omits these since they are not language- level keywords.
- TS does not color `NaN`, `Infinity`, `__dirname`, `__filename` as `yellow`.
  Legacy lists these as special constants/globals.
- The `?` ternary operator is colored as `brightcyan` (delimiter) in both TS and
  legacy, which is consistent.
- Access modifiers `private`, `protected`, `public`: both TS and legacy color
  these as `yellow`. Legacy lists them as keywords. TS captures them via the
  keyword list. However, in the TS output the `private` keyword is not part of
  the TS keyword list in the query -- it appears uncolored. Legacy colors it as
  `yellow`. This is because the TS query does not include `"private"`,
  `"protected"`, or `"public"` in its keyword list. They appear as `yellow` in
  legacy but default in TS. UPDATE: checking the output, both legacy and TS show
  these as `yellow`, so both match.
- The `type` keyword in type alias declarations: TS does not capture the
  standalone `type` keyword when used at the start of a type alias declaration
  (e.g., `type KeyOf<T>`). It appears as default text. Legacy colors `type` as
  not listed (it would be default). Actually both legacy and TS do not
  explicitly show `type` as colored in the output -- it appears as default in
  both. The TS query does include `"type"` in the keyword list.
- The `satisfies` and `override` keywords: TS includes them in the keyword list.
  Legacy does not list them (they were added in newer TypeScript versions). TS
  improvement.
