Go syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `go.go`
Legacy reference: `misc/syntax/go.syntax`
TS query: `misc/syntax-ts/queries-override/go-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[go]`

Aligned with legacy
-------------------

- Language keywords (`package`, `import`, `func`, `var`, `const`, `type`,
  `struct`, `interface`, `map`, `chan`, `go`, `defer`, `return`, `if`, `else`,
  `for`, `range`, `switch`, `case`, `default`, `break`, `continue`, `goto`,
  `select`, `fallthrough`): `yellow` - MATCH
- Builtin types (`int`, `int8`...`int64`, `uint`, `uint8`...`uint64`, `uintptr`,
  `float32`, `float64`, `byte`, `string`, `bool`): `brightgreen` - MATCH
- Builtin functions (`make`, `len`, `cap`, `new`, `close`, `print`, `println`,
  `panic`): `brown` - MATCH
- Builtin constants (`nil`, `true`, `false`): `brown` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Raw strings (backtick `` `...` ``): `green` - MATCH
- Rune literals (`'A'`): legacy colors as `gray`, TS colors as `lightgray` (both
  map to `constant` capture) - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Channel operator `<-`: `brightmagenta` - MATCH
- Operators (`+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `<<`, `>>`, `=`, `:=`,
  `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`, `==`, `!=`, `<`,
  `>`, `<=`, `>=`, `&&`, `||`, `!`): `brightcyan` - MATCH
- Delimiters (`(`, `)`, `[`, `]`, `{`, `}`, `.`, `,`, `;`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Comments are uniformly `brown` in TS. Legacy colors the `//` and `/* */`
  markers as `green` and comment text as `brown`, creating a two-tone effect. TS
  provides a cleaner single-color comment appearance.
- Builtin types `complex64`, `complex128`, `rune`, `error`: TS colors these as
  `brightgreen` via the `#match?` predicate on `type_identifier`. Legacy does
  not list `complex64`, `complex128`, `rune`, or `error` in its keyword list, so
  they appear uncolored.
- Builtin functions `append`, `complex`, `copy`, `delete`, `imag`, `real`,
  `recover`: TS colors these as `brown` via `#match?` predicate. Legacy does not
  list `append`, `complex`, `copy`, `delete`, `imag`, `real`, or `recover` in
  its builtin function list.
- Labels (`loop:`): TS colors label names as `cyan` via the `label_name`
  capture. Legacy does not color labels (they appear as default text, with only
  the `:` colored as `brightcyan`).
- Colon after `case`/`default` in `switch`: legacy colors the `:` as
  `brightcyan`, while TS leaves it uncolored (default). This is because the TS
  query does not capture `:` as a delimiter in case clauses. Minor difference,
  not a regression.
- Package names (`fmt`, `strings`): legacy colors builtin package names as
  `brightgreen` via hardcoded keyword list. TS does not color package names
  (they appear as default text). The legacy approach is brittle since it only
  covers stdlib packages.

Known shortcomings
------------------

- TS does not color builtin package names (`fmt`, `strings`, `io`, `os`, etc.)
  as `brightgreen`. Legacy has a hardcoded list of ~40 stdlib package names. TS
  intentionally omits this since package names are not language-level
  constructs.
- The `iota` constant is listed in the TS query under `function.builtin`
  (`brown`) but only matches when used as an identifier in a `const` block.
  Legacy colors `iota` as `brown` via a keyword match. Both produce the same
  color.
- Legacy colors special function names `init` and `main` as `brown`. TS does not
  distinguish these from regular function names.
- The `++` and `--` operators are not explicitly listed in the TS operator
  capture list. Legacy colors them as individual `+`/`-` characters. TS leaves
  `++`/`--` uncolored in the labels section where they are not inside an
  assignment expression.
- The colon `:` character is colored as `brightcyan` by legacy (as a standalone
  delimiter) but is not captured by TS in all contexts (e.g., after `case`
  labels, in map literals). In map literals, TS does not color the `:` between
  key and value.
