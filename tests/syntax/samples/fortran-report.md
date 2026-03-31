Fortran syntax highlighting: TS vs Legacy comparison report
=============================================================

Sample file: `fortran.f90`
Legacy reference: `misc/syntax/fortran.syntax`
TS query: `misc/syntax-ts/queries-override/fortran-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[fortran]`

Aligned with legacy
-------------------

- Type declarations (`integer`, `real`, `character`, `logical`, `complex`,
  `double`): `brightcyan` - MATCH
- Declaration keywords (`dimension`, `allocatable`, `intent`, `in`, `out`,
  `inout`, `parameter`, `type`, `contains`, `public`, `private`, `pointer`,
  `target`, `save`): `brightcyan` - MATCH
- Control flow (`do`, `if`, `then`, `else`, `elseif`, `endif`, `enddo`, `call`,
  `return`, `stop`, `continue`, `cycle`, `exit`, `goto`, `while`, `select`,
  `case`, `default`, `allocate`): `brightgreen` - MATCH
- I/O keywords (`read`, `write`, `print`, `open`, `close`, `inquire`):
  `brightmagenta` - MATCH
- Program structure keywords (`program`, `subroutine`, `function`, `module`,
  `end`): `yellow` - MATCH
- Comments (`!` to end of line): `brown` - MATCH
- Strings (single-quoted `'...'` and double-quoted `"..."`): `green` - MATCH
- Boolean literals (`.true.`, `.false.`): `brightred` - MATCH
- Arithmetic operators (`+`, `-`, `*`, `/`, `**`, `=`): `yellow` - MATCH
- Comparison operators (`==`, `/=`, `<`, `>`, `<=`, `>=`): `yellow` - MATCH
- Delimiters (`,`, `::`, `(`, `)`): `brightcyan` - MATCH
- Statement labels (`100`): legacy colors these as `brightred` (matching the
  label number pattern in column 1-5). TS also shows `100` at column 1 without
  specific label coloring. See differences below.

Intentional improvements over legacy
-------------------------------------

- `use` keyword: TS colors `use` as `yellow` (`@keyword`). Legacy colors `use`
  inconsistently -- it is not in the legacy keyword list.
- `implicit` keyword: TS colors `implicit` as `yellow` (`@keyword`). Legacy
  colors `implicit` as `brightcyan` (declaration). Both are reasonable choices;
  TS treats it as a structural keyword.
- `none` keyword: TS does not color `none` separately (appears as default text).
  Legacy colors `none` as `brightcyan`. The TS query includes `none` in the
  `@keyword` list, but it may not match in all contexts.
- `only` keyword: TS colors `only` as `yellow` (`@keyword`). Legacy does not
  have `only` in its keyword list.
- `result` keyword: TS colors `result` as `yellow` (`@keyword`). Legacy does not
  have `result` as a keyword.
- `contains` keyword: TS colors `contains` as `brightcyan` (`@property`). Legacy
  does not list `contains` as a keyword.
- `recursive` keyword: not in either keyword list; appears as default text in
  both engines.

Known shortcomings
------------------

- Logical operators `.and.`, `.or.`, `.not.`, `.eq.`, `.gt.`, `.lt.`, `.ge.`,
  `.le.`, `.ne.`, `.eqv.`, `.neqv.`: FIXED -- now captured via field-based
  `logical_expression`, `relational_expression`, and `unary_expression` operator
  patterns as `@function.special` (`brightred`), matching legacy.
- `name` identifier: legacy colors `name` as `brightmagenta` because it matches
  the I/O keyword `name` in the legacy list. TS correctly treats `name` as a
  regular identifier when used as a variable name. This is actually a legacy
  false positive.
- `implicit none`: TS colors `implicit` as `yellow` and `none` appears
  uncolored. Legacy colors both as `brightcyan`. Minor difference in
  presentation.
- `precision` after `double`: legacy colors `precision` as `brightcyan`. TS does
  not appear to capture `precision` separately -- it appears as default text.
- I/O specifier keywords within parenthesized I/O statements (e.g., `unit=`,
  `file=`, `status=`, `exist=`): legacy colors these as `brightmagenta` via the
  I/O keyword list. TS does not color these specifiers when they appear as
  assignment targets in I/O calls.
- Statement label `100` at column 1: legacy colors the label with `brightred`
  foreground via the line-start pattern matching. TS colors it as `brightred`
  too, but through `boolean_literal` or similar. The actual label coloring
  mechanism differs.
- Mathematical functions (`sqrt`, `mod`, `real`, `abs`, etc.): legacy colors
  these as `yellow` via a hardcoded list. TS does not capture intrinsic function
  calls specially -- `sqrt` and `mod` appear uncolored or as default text.
- `deallocate` keyword: TS does not include `deallocate` in the
  `@number.builtin` control flow list, so it appears uncolored. Legacy also does
  not list `deallocate`. The TS query has `allocate` but not `deallocate`.
