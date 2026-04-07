YAML syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `yaml.yaml`
Legacy reference: `misc/syntax/yaml.syntax`
TS query: `misc/syntax-ts/queries-override/yaml-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[yaml]`

Aligned with legacy
-------------------

- Mapping keys: `yellow` - MATCH (legacy uses yellow for `key:` pattern, TS uses
  `@property.key`)
- Comments (`#`): `brown` - MATCH
- Double-quoted strings: `green` - MATCH
- Single-quoted strings: `green` - MATCH
- Booleans (`true`, `false`): `brightmagenta` - MATCH (legacy uses
  `brightmagenta` for whole-word `true`/`false`)
- `null`: `brightmagenta` - MATCH
- Document start marker `---`: `brightcyan` - MATCH
- Escape sequences in strings (`\n`, `\t`, `\u2603`, `\x41`): `brightgreen` -
  MATCH
- Block scalars (`|` and `>` content): `brown` - MATCH (both legacy and TS
  render block scalar content in brown)
- Flow delimiters (`{`, `}`, `[`, `]`, `,`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Tilde `~` as null is recognized and highlighted as `brightmagenta` by TS.
  Legacy did not have a rule for `~` as null.
- Numbers (integers and floats like `5432`, `3.14159`, `1.5e+10`) are
  highlighted in `lightgray` by TS. Legacy left numbers as plain default text.
- Special float values (`.inf`, `.nan`) are highlighted in `lightgray` by TS.
  Legacy had no rule for these.
- Anchors (`&defaults`) and aliases (`*defaults`) are highlighted in `cyan` by
  TS (via `@label`). Legacy left these as default text.
- Tags (`!!str`, `!!binary`, `!custom`) are highlighted in `yellow` by TS (via
  `@type`). Legacy had no tag highlighting.
- Colons, dashes, `>`, `|`, and `?` as operators are highlighted in `brightcyan`
  by TS. Legacy highlighted `-` in flow context but not consistently as
  operators.
- Document end marker `...` is highlighted in `brightcyan` by TS. Legacy did not
  highlight this marker.
- Flow pair keys (inside `{name: John}`) are properly highlighted as `yellow` by
  TS. Legacy had difficulty with inline flow mapping keys.
- The `?` explicit key indicator is highlighted in `brightcyan` by TS. Legacy
  did not handle this syntax.

Known shortcomings
------------------

- Timestamps (`2024-01-15`, `2024-01-15T10:30:00Z`) are not highlighted by TS
  despite having a `timestamp_scalar` capture mapped to `@string.special`. The
  tree-sitter YAML parser may not recognize all timestamp formats, or timestamps
  are being parsed as plain scalars.
- The `<<` merge key in alias references (`<<: *defaults`) is highlighted as a
  regular mapping key in `yellow`. There is no special treatment for merge keys.
- Unquoted string values (plain scalars like `localhost`, `mydb`) render as
  default text in both TS and legacy. This is intentional to keep plain text
  readable but means no distinction between unquoted strings and other plain
  text.
- The legacy `yaml.syntax` used complex regex to detect keys (requiring
  alphanumeric characters followed by colon-space), which sometimes missed keys
  or grabbed too much. TS parsing of keys is structurally correct but shows the
  colon separately in `brightcyan` rather than as part of the key.
