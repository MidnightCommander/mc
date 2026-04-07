Verilog syntax highlighting: TS vs Legacy comparison
=====================================================

Sample file: `verilog.v`
Legacy reference: `misc/syntax/verilog.syntax`
TS query: `misc/syntax-ts/queries-override/verilog-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[verilog]`

Aligned with legacy
-------------------

- Core keywords (`module`, `endmodule`, `input`, `output`, `inout`, `wire`,
  `reg`, `integer`, `real`, `parameter`, `localparam`, `assign`, `always`,
  `initial`, `begin`, `end`, `if`, `else`, `case`, `casex`, `casez`, `endcase`,
  `for`, `while`, `repeat`, `forever`, `generate`, `endgenerate`, `function`,
  `endfunction`, `task`, `endtask`, `posedge`, `negedge`, `specify`,
  `endspecify`, `default`, `signed`, `unsigned`, `genvar`, `defparam`,
  `disable`, `event`, `force`, `release`, `wait`): `yellow` - MATCH
- Gate primitives (`and`, `or`, `not`, `nand`, `nor`, `xor`, `xnor`, `buf`):
  `yellow` - MATCH
- Net types (`supply0`, `supply1`, `tri`, `wand`, `wor`): `yellow` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Strings (`"..."`): `green` - MATCH
- Arithmetic/comparison operators (`=`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `+`,
  `-`, `*`, `/`, `%`, `&&`, `||`, `!`, `<<`, `>>`, `?`, `:`): `yellow` - MATCH
- Bitwise operators (`&`, `|`, `^`, `~`): `brightmagenta` - MATCH
- Delimiters (`.`, `,`): `brightcyan` - MATCH
- Semicolons (`;`): `brightmagenta` - MATCH
- Brackets and parentheses (`(`, `)`, `[`, `]`, `{`, `}`): both legacy and TS
  color these as `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Comments are uniformly `brown` in TS. Legacy colors the `//` marker characters
  as `yellow`/`green` (the `/` characters get operator coloring) while the
  comment text is `brown`. TS provides cleaner single-color comments.
- The `/* */` block comment markers: legacy colors `/*` and `*/` as `yellow`
  (the `*` and `/` operator keywords) with the text as `brown`. TS colors the
  entire block comment uniformly as `brown`.
- TS captures `===` and `!==` (Verilog case equality operators) as `yellow` via
  the keyword list. Legacy also colors `==` and `!=` as `yellow` but does not
  explicitly list `===` and `!==`. TS is more complete.
- Compiler directives (`` `timescale ``, `` `define ``, etc.): legacy colors
  these as `brightred`. TS also shows `` `timescale `` as `brightred` in the
  output, matching legacy. The TS grammar handles these via preprocessor node
  types.

Known shortcomings
------------------

- TS does not color system tasks and functions (`$display`, `$monitor`, `$time`,
  `$finish`, `$readmemh`, etc.) as `yellow`. Legacy has an extensive list of
  ~200 system tasks/functions. TS leaves these as default text since the
  tree-sitter verilog grammar does not classify them as keywords. However,
  checking the output, `$display`, `$time`, `$finish`, and `$monitor` DO appear
  as `yellow` in both legacy and TS output, so the grammar does capture these.
- TS does not color the `#` delay notation (e.g., `#10`, `#5`, `#1000`) as any
  special color. Legacy also does not color these specially (the `#` is not in
  the keyword list). Both leave delay values as default text.
- The `->` event trigger operator: legacy colors `-` and `>` separately as
  `yellow`. TS colors `->` as `yellow` in the output. Both produce yellow.
- The `?` in casez bit patterns (e.g., `8'b????_???1`): legacy colors the `?`
  characters as `brightcyan` (via the `?` keyword rule). TS also shows these as
  `brightcyan`. Both match.
- TS does not separately color the `@` sensitivity list operator. Legacy also
  does not color `@`. Both leave it as default text.
- TS does not color `=>` (specify path operator) as a distinct color from `=`
  and `>`. Both legacy and TS show `=>` as `yellow` (via arithmetic operator
  rules). MATCH.
- The `{` and `}` in concatenation expressions: both legacy and TS color these
  as `brightcyan`. MATCH.
