Kotlin syntax highlighting: TS vs Legacy comparison report
===========================================================

Sample file: `kotlin.kt`
Legacy reference: `misc/syntax/kotlin.syntax`
TS query: `misc/syntax-ts/queries-override/kotlin-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[kotlin]`

Aligned with legacy
-------------------

- Hard keywords (`as`, `class`, `do`, `else`, `for`, `fun`, `if`, `in`,
  `interface`, `is`, `object`, `return`, `super`, `this`, `throw`, `try`,
  `typealias`, `val`, `var`, `when`, `while`): `yellow` - MATCH
- `package`, `import`: `brown` - MATCH
- Soft keywords (`by`, `catch`, `constructor`, `finally`, `get`, `init`, `set`,
  `where`): `brightgreen` - MATCH
- Modifier keywords (`abstract`, `annotation`, `companion`, `const`,
  `crossinline`, `data`, `enum`, `external`, `final`, `infix`, `inline`,
  `inner`, `internal`, `lateinit`, `noinline`, `open`, `operator`, `out`,
  `override`, `private`, `protected`, `public`, `sealed`, `suspend`, `tailrec`,
  `vararg`): `brightmagenta` - MATCH
- Built-in types (`Double`, `Float`, `Long`, `Int`, `Short`, `Byte`, `Char`,
  `Boolean`, `Array`, `String`, `ByteArray`): `brightred` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Multiline strings (`"""..."""`): `green` - MATCH
- Character literals (`'A'`): `green` via `string.special` capture ->
  `brightgreen` in colors.ini. Legacy also colors as `green`. Close MATCH.
- String interpolation (`$name`, `${e.message}`): legacy colors `$` references
  as `brightgreen`, TS colors them similarly within string nodes. MATCH.
- Comments (line `//` and block `/* */`): legacy colors as `gray`, TS colors as
  `brown`. See improvements section.
- Operators (`=`, `+=`, `-=`, `*=`, `/=`, `%=`, `==`, `!=`, `<`, `>`, `<=`,
  `>=`, `&&`, `||`, `!`, `+`, `-`, `*`, `/`, `%`, `++`, `--`, `->`, `?:`, `..`,
  `::`, `!!`): `brightcyan` - MATCH
- Delimiters (`.`, `,`, `:`): `brightcyan` - MATCH
- Semicolons `;`: `brightcyan` via `delimiter` capture. Legacy also colors as
  `brightcyan`. MATCH.
- Labels (`loop@`, `@loop`): legacy colors as `brightcyan` via annotation/label
  patterns. TS colors via `label` capture as `cyan`. Close match, slightly
  different shade.
- Annotations (`@MyAnnotation`, `@Deprecated`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Comments: TS colors comments as `brown` (matching most other languages in MC).
  Legacy colors Kotlin comments as `gray`, which is inconsistent with the rest
  of the MC syntax highlighting convention. TS provides a more uniform
  experience.
- `catch` keyword: legacy colors `catch` as `yellow` (hard keyword). TS colors
  it as `brightgreen` (soft keyword via `type.builtin`), which is semantically
  more accurate since `catch` is a soft keyword in Kotlin.
- `reified` modifier: TS colors it as `brightmagenta` (modifier keyword via
  `keyword.control`). Legacy lists it under modifier keywords as
  `brightmagenta`. MATCH. But TS captures it within the `inline fun <reified T>`
  context structurally.
- `as?` safe cast operator: TS captures this as a single `brightcyan` operator.
  Legacy colors `as?` as `yellow` (keyword `as?`). TS treats it as an operator
  rather than keyword, which is more consistent with other operators.
- `null`, `true`, `false`: legacy colors these as `yellow` (hard keywords). TS
  does not have explicit captures for these literals, so they appear as default
  text in contexts where no keyword rule matches. However, `true` and `false`
  appear as `yellow` in TS output because they are picked up by other grammar
  rules. `null` similarly appears as `yellow`.
- `it` special identifier: legacy colors `it` as `brightgreen`. TS colors `it`
  as `brightgreen` when the grammar recognizes it within lambda context. MATCH
  in the lambda body.

Known shortcomings
------------------

- `break`/`continue` keywords: legacy lists these as `yellow` hard keywords. TS
  does not explicitly include `break` and `continue` in the keyword capture list
  in the override query, but they are captured by the base grammar as keywords.
  Both show as `yellow`. No actual difference.
- `!in` and `!is` compound operators: legacy colors these as `yellow` via
  explicit keyword entries. TS handles `!` and `in`/`is` as separate tokens. The
  `!` gets `brightcyan` (operator) and `in`/`is` gets `yellow` (keyword),
  creating a split-color effect. Legacy colors the whole `!in`/`!is` as
  `yellow`.
- `delegate`, `dynamic`, `field`, `file`, `param`, `property`, `receiver`,
  `setparam`: legacy has these soft keywords colored as `brightgreen`. TS does
  not include all of them in the soft keyword capture (only `by`, `catch`,
  `constructor`, `finally`, `get`, `init`, `set`, `where`). Unused soft keywords
  like `delegate` and `dynamic` appear uncolored in TS.
- `expect` modifier keyword: listed in the TS query under `keyword.control`
  (`brightmagenta`). Legacy does not include `expect`. TS adds coverage for this
  Kotlin Multiplatform keyword.
- `?` (nullable type marker) and `?.` (safe call): legacy colors `?` and `?.` as
  `brightcyan`. TS does not explicitly capture these, so they appear as default
  text in some contexts. The `?:` elvis operator is captured but standalone `?`
  is not.
- Comment delimiters `//` and `/*`: in legacy output, the `//` appears
  separately as `brightcyan` (from the delimiter keyword) before the comment
  text in `gray`. In TS, the entire comment including `//` is colored as
  `brown`. TS is cleaner.
- `$` and `_` standalone: legacy colors these as `brightcyan`. TS does not
  explicitly capture standalone `$` or `_` characters.
