Ada syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `ada.adb`
Legacy reference: `misc/syntax/ada95.syntax`
TS query: `misc/syntax-ts/queries-override/ada-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[ada]`

Aligned with legacy
-------------------

- General keywords (`with`, `use`, `pragma`, `in`, `out`, `is`, `do`, `null`,
  `of`, `or`, `and`, `not`, `xor`, `mod`, `abs`, `delay`, `raise`, `return`,
  `others`, `task`, `reverse`, `range`, `renames`, `separate`): `yellow` - MATCH
- Control flow keywords (`begin`, `case`, `declare`, `else`, `elsif`, `end`,
  `entry`, `exception`, `exit`, `for`, `if`, `loop`, `private`, `protected`,
  `select`, `then`, `until`, `when`, `while`): `brightred` - MATCH
- Declaration keywords (`abstract`, `accept`, `access`, `all`, `at`, `constant`,
  `goto`, `tagged`, `type`, `limited`): `brightcyan` - MATCH
- Definition keywords (`body`, `function`, `generic`, `new`, `package`,
  `procedure`): `brightmagenta` (TS) vs `magenta` (legacy) - CLOSE MATCH
  (brightmagenta is the closest available TS mapping)
- Type-like keywords (`array`, `record`): `cyan` - MATCH
- Operators (`+`, `-`, `*`, `/`, `**`, `&`, `=`, `/=`, `<`, `>`, `<=`, `>=`,
  `:=`, `=>`, `..`, `<>`): `brightgreen` - MATCH
- Strings: `green` - MATCH
- Character literals (`'A'`, `' '`): `brightgreen` - MATCH
- Comments: `brown` - MATCH
- Labels (`<<Start_Label>>`): `cyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Delimiter distinction: TS separates delimiters (`.`, `,`, `:`, `;`, `(`, `)`)
  as `brightcyan` from operators (`+`, `-`, `:=`, etc.) as `brightgreen`. Legacy
  colors all punctuation uniformly as `brightgreen`. This provides better visual
  distinction between structural and operational punctuation.
- `overriding` keyword: TS correctly highlights `overriding` as `brightcyan`
  (@property). Legacy does not recognize `overriding` at all (DEFAULT color).
- `subtype` keyword: TS colors `subtype` as `cyan` (@label, type-like keyword).
  Legacy colors it as `brightcyan` (same group as `access`, `type`). TS
  categorization is arguably more accurate since `subtype` defines types.
- Case-sensitive identifiers: legacy uses `caseinsensitive` mode, which means
  `True`, `False`, `Boolean`, `Integer`, `Float`, `String`, `Character` are
  colored as `cyan` (type names) regardless of context. TS does not color these
  standard type names because they are user-defined identifiers in tree-sitter's
  Ada grammar. This is more correct from a parser perspective since Ada's
  predefined types are library-defined, not keywords.
- `interface` keyword: TS colors it as `brightcyan` (@property). Legacy does not
  include `interface` in its keyword list.

Known shortcomings
------------------

- Predefined type names (`Boolean`, `Integer`, `Float`, `String`, `Character`,
  `Natural`, `Positive`, `Duration`, `Wide_Character`, `Wide_String`,
  `Wide_Wide_Character`, `Wide_Wide_String`): TS now captures these as `yellow`
  via `@keyword` with `#any-of?` predicate on identifier nodes. Legacy colors
  them as `cyan` (keyword). The color differs (yellow vs cyan) but they are now
  highlighted rather than left as DEFAULT. `Universal_Integer` and
  `Universal_Float` are not included as they are compiler-internal types.
- `True` and `False` literals: legacy colors these as `cyan` (type color due to
  case-insensitive matching). TS leaves them as DEFAULT since they are parsed as
  identifiers. Ada programmers may expect boolean literals to be highlighted.
- `requeue` and `terminate` keywords: legacy highlights these as `yellow`. TS
  does not include them in its keyword lists, so they appear as DEFAULT.
- `#` (hash) and `'` (tick/attribute): legacy colors these as `brightgreen`
  (operator). TS does not explicitly capture them. The tick used for attributes
  (e.g., `Integer'Image`) is not highlighted.
- `aliased` keyword: legacy colors it as `brightcyan`. TS does not include it in
  its keyword lists.
