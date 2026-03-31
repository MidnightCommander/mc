Swift syntax highlighting: TS vs Legacy comparison
===================================================

Sample file: `tests/syntax/samples/swift.swift`
Legacy reference: `misc/syntax/swift.syntax`
TS query: `misc/syntax-ts/queries-override/swift-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` (no `[swift]`)

NOTE: The TS Swift grammar has an ABI version mismatch (version 10 is too old
  for tree-sitter 0.25). The query file is empty: `";; Swift -- empty query
  (grammar ABI version 10 is too old for tree-sitter 0.25)"`. As a result, TS
  produces NO syntax highlighting at all -- the entire file is rendered in
  default foreground.

Aligned with legacy
-------------------

No alignment is possible since TS produces zero highlighting output.

Legacy highlights the following (all lost in TS):

- Keywords (`class`, `struct`, `enum`, `protocol`, `func`, `var`, `let`,
  `import`, `init`, `deinit`, `static`, `return`, `if`, `else`, `for`, `in`,
  `while`, `repeat`, `switch`, `case`, `default`, `guard`, `defer`, `do`,
  `catch`, `throw`, `throws`, `try`, `as`, `is`, `where`, `self`, `Self`,
  `super`, `true`, `false`, `nil`, `typealias`, `subscript`, `extension`,
  `public`, `private`, `fileprivate`, `internal`, `open`, `mutating`, `get`,
  `set`, `override`, `weak`, `inout`, `rethrows`, `associatedtype`, `operator`,
  `continue`, `fallthrough`, `break`): `yellow`
- Type names (`String`, `Int`, `Double`, `Float`, `Bool`, `Any`, `Character`,
  `UInt`, etc.): `yellow`
- Comments (`// ...`): `red` (legacy uses `comments` defined as `red`)
- Block comments (`/* ... */`): `red`
- Strings (`"..."`, including interpolation): `green` (legacy uses `string`
  color)
- Escape sequences (`\t`, `\n`, `\\`, `\(`...`)`): `brightgreen`
- Unicode escapes (`\u{...}`): `brightgreen`
- Operators (`>`, `<`, `+`, `-`, `*`, `/`, `%`, `=`, `!=`, `==`, `|`, `&`, `^`,
  `~`, `!`, `_`): `brightcyan`
- Brackets (`{`, `}`, `(`, `)`, `[`, `]`): `brightcyan`
- Punctuation (`.`, `,`, `:`, `?`): `brightcyan`
- Range operators (`...`, `..<`): `brightcyan`
- Semicolons (`;`): `brightmagenta`
- Attributes (`@available`, `@discardableResult`, `@escaping`, etc.): `yellow`
- Compiler directives (`#if`, `#elseif`, `#else`, `#endif`): `brightred`

Differences from legacy
-----------------------

- TS produces no output at all. Every single highlight from legacy is lost. This
  is a complete regression for Swift files.

Known shortcomings
------------------

- The tree-sitter Swift grammar's ABI version (10) is incompatible with the
  tree-sitter runtime (0.25). Until the grammar is updated to a compatible ABI
  version, TS highlighting for Swift is completely nonfunctional.
- The query file is intentionally empty with a comment explaining the version
  mismatch.
- Legacy provides comprehensive Swift highlighting covering keywords, types,
  operators, strings, escapes, attributes, and compiler directives. All of this
  is unavailable via TS.
- To fix this, the upstream tree-sitter-swift grammar needs to be rebuilt with
  ABI version 14 or later to be compatible with tree-sitter 0.25.
