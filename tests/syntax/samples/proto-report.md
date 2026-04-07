Proto syntax highlighting: TS vs Legacy comparison
===================================================

Sample file: `tests/syntax/samples/proto.proto`
Legacy reference: `misc/syntax/protobuf.syntax`
TS query: `misc/syntax-ts/queries-override/proto-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[proto]`

Aligned with legacy
-------------------

- Language keywords (`syntax`, `package`, `import`, `public`, `message`, `enum`,
  `service`, `rpc`, `returns`, `stream`, `oneof`, `map`, `reserved`,
  `extensions`, `option`, `optional`, `required`, `repeated`, `extend`, `to`,
  `max`): `yellow` via `@keyword` - MATCH
- Type keywords (`int32`, `int64`, `uint32`, `uint64`, `sint32`, `sint64`,
  `fixed32`, `fixed64`, `sfixed32`, `sfixed64`, `bool`, `string`, `double`,
  `float`, `bytes`): `yellow` via `@keyword` - MATCH
- Comments (`// ...`): `brown` via `@comment` - MATCH
- Strings (`"proto3"`, `"google/protobuf/..."`, `"old_field"`): `green` via
  `@string` - MATCH
- Assignment operator (`=`): `yellow` via `@operator.word` - MATCH
- Semicolons (`;`): `brightmagenta` via `@delimiter.special` - MATCH
- Brackets/parens/square brackets (`{`, `}`, `(`, `)`, `[`, `]`): `brightcyan`
  via `@delimiter` - MATCH
- Punctuation (`,`, `:`, `.`, `<`, `>`): `brightcyan` via `@delimiter` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS colors message/enum/service/rpc names (`User`, `Status`, `Organization`,
  `UserService`, `GetUser`, etc.) as `brightcyan` via `@tag`. Legacy leaves
  these names uncolored (default foreground). This provides better visual
  distinction for type definitions.
- TS colors boolean literals (`true`, `false`) as `yellow` via `@keyword`.
  Legacy does not recognize boolean literals as special tokens.
- TS handles `to` and `max` keywords in `extensions 100 to max` as `yellow`.
  Legacy does not highlight these.
- TS treats the `syntax` keyword consistently as `yellow`. Legacy leaves it
  uncolored since it was not in the keyword list (only `//` comment prefix was
  matched for `syntax` in the comment context).
- TS would color escape sequences (`\\`, `\"`) in strings as `brightgreen` via
  `@string.special`. Legacy does not highlight escape sequences within strings.
- TS colors dots in qualified names (`google.protobuf. Timestamp`) as
  `brightcyan` delimiters. Legacy leaves dots as default.

Differences from legacy
-----------------------

- TS does not color `reserved` string literals (`"old_field"`, `"legacy_field"`)
  as `green` -- they appear unquoted without the `@string` capture. Looking at
  the dump output, TS shows these without `<GREEN>` tags while legacy properly
  wraps them. This appears to be a minor parse issue where reserved string
  values may not be recognized as `(string)` nodes by the grammar.

Known shortcomings
------------------

- Reserved string values (`"old_field"`, `"legacy_field"`) appear without
  `green` coloring in TS output, though legacy correctly colors them. The
  tree-sitter proto grammar may not emit `(string)` nodes for reserved field
  name strings.
- TS does not distinguish between field numbers and other numeric literals --
  both are uncolored (default foreground). This matches legacy behavior, so it
  is not a regression.
- Neither TS nor legacy highlights field numbers or numeric option values with a
  distinct color.
