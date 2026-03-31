VHDL syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `vhdl.vhd`
Legacy reference: `misc/syntax/vhdl.syntax`
TS query: `misc/syntax-ts/queries-override/vhdl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[vhdl]`

Aligned with legacy
-------------------

- Core keywords (`library`, `use`, `entity`, `is`, `port`, `architecture`, `of`,
  `begin`, `end`, `signal`, `variable`, `constant`, `array`, `range`, `to`,
  `downto`, `process`, `if`, `then`, `else`, `elsif`, `case`, `when`, `others`,
  `for`, `while`, `loop`, `generate`, `component`, `generic`, `map`, `wait`,
  `until`, `assert`, `return`, `function`, `procedure`, `package`, `body`,
  `attribute`, `alias`, `record`, `with`, `select`, `after`, `block`, `open`,
  `all`): `yellow` - MATCH
- Word operators (`not`, `and`, `or`, `nand`, `nor`, `xor`, `xnor`): `green` -
  MATCH
- Port direction keywords (`in`, `out`, `inout`, `buffer`): `white` - MATCH
- Type/subtype declarations (`type`, `subtype`): `brightcyan` - MATCH
- Comments (`-- ...`): `magenta` - MATCH
- String literals (`"..."`): `green` - MATCH
- Character literals (`'0'`, `'1'`, `'Z'`): `brightgreen` - MATCH
- Symbol operators (`:=`, `<=`, `=>`, `=`, `/=`, `<`, `>`, `+`, `-`, `*`, `/`,
  `&`, `**`): `brightgreen` - MATCH
- Delimiters (`.`, `;`, `,`, `:`): `brightgreen` - MATCH
- Parentheses (`(`, `)`): `brightgreen` - MATCH
- Boolean literals (`true`, `false`): `brightred` - MATCH
- Time units (`ns`): `brightred` - MATCH
- Report/severity keywords (`report`, `severity`, `note`, `warning`, `error`,
  `failure`): `red` - MATCH
- Predefined types (`integer`, `natural`, `positive`, `string`, `character`,
  `boolean`, `real`, `bit`, `bit_vector`, `time`, `std_logic`,
  `std_logic_vector`, `severity_level`): `cyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Entity, architecture, and component names: TS colors these identifiers as
  `cyan` via the `@label` capture (e.g., `counter` in `entity counter is`).
  Legacy does not color entity/architecture names -- they appear as default
  text.
- Function and procedure names: TS colors `max_of` and `reset_counter` in their
  declarations as `brightcyan` via the `@function` capture. Legacy does not
  distinguish function/procedure names from other identifiers.
- Label names (e.g., `clk_proc`, `state_proc`, `gen_block`, `ctrl_block`,
  `stim_proc`, `add_inst`): TS colors these as `cyan` via the `label` capture.
  Legacy does not color label identifiers -- only the `:` after the label gets
  `brightgreen`.
- Type declaration names (e.g., `state_type`, `byte_t`, `mem_array`): TS colors
  these as `cyan` via the `@label` capture on `full_type_declaration` and
  `subtype_declaration` name nodes. Legacy leaves these as default text.
- Architecture body name (`rtl`): TS colors this as `cyan` via `@label`. Legacy
  leaves it as default text.

Known shortcomings
------------------

- TS does not color `report`, `severity`, `note`, `warning`, `error`, `failure`
  in assert statements. The TS query does not have captures for these
  report-related keywords. Legacy colors `report` and `severity` as `red` and
  severity levels (`note`, `warning`, `error`, `failure`) also as `red`.
  However, checking the output both legacy and TS show these as `red`, meaning
  the tree-sitter grammar does handle them. Actually the TS output shows
  `report`, `severity`, `failure`, `note`, `warning`, `error` as `red` matching
  legacy. These are likely captured by the base grammar queries rather than the
  override. MATCH in output.
- TS does not color some shift/rotate operators (`sll`, `srl`, `sla`, `sra`,
  `rol`, `ror`) or math operators (`rem`, `mod`, `abs`, `new`) as `green`.
  Legacy colors these as `green` via explicit keyword rules. These operators are
  not exercised in the sample file but would be missing in TS if they are not in
  the base grammar queries.
- The `|` (bar/pipe) character: legacy colors it as `brightgreen` via explicit
  keyword. TS may or may not capture it depending on the context. Not exercised
  in the sample.
- Bit string literal prefixes (`x`, `b`, `o` before quoted strings): TS colors
  only the quoted portion as `green` via string literal capture. The prefix
  letter appears as default text. Legacy behavior is the same since the prefix
  is not a keyword. Both show `x"DEAD"` with `x` as default and `"DEAD"` as
  `green`.
- The `'` tick character in attribute references (e.g., `sig'event`,
  `sig'range`): legacy colors `'` as `brightgreen`. TS does not capture isolated
  tick characters. Not exercised in the sample.
- Legacy has `guarded`, `postponded` (sic), `unaffected`, `disconnect` as
  `yellow` keywords. The TS query does not include these. They are uncommon VHDL
  keywords.
- Function calls like `rising_edge`, `to_unsigned`: TS does not color these as
  `brightcyan` since they are not matched by the `function_call` capture (the
  grammar may use a different node type for library function calls). Legacy also
  does not color them. Both show default text.
