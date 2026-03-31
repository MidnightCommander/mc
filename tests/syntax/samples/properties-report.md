Properties syntax highlighting: TS vs Legacy comparison
========================================================

Sample file: `sample.properties`
Legacy reference: `misc/syntax/properties.syntax`
TS query: `misc/syntax-ts/queries-override/properties-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[properties]`

Aligned with legacy
-------------------

- Hash comments (`# ...`): `brown` - MATCH (TS `@comment`, legacy `context
  linestart # \n brown`).
- Exclamation comments (`! ...`): `brown` - MATCH (TS `@comment`, legacy
  `context linestart ! \n brown`).
- Property keys (`db.host`, `app.name`, `feature.enabled`, etc.): `yellow` -
  MATCH (TS `(property (key))` -> `@property.key`, legacy `keyword linestart ...
  yellow`).
- Equals separator (`=`): `brightcyan` - MATCH (TS `@delimiter`, legacy `keyword
  = brightcyan`).
- Colon separator (`:`): `brightcyan` - MATCH (TS `@delimiter`, legacy `keyword
  : brightcyan`).
- Property values: `lightgray` - MATCH (TS `(property (value))` -> `@variable` =
  `lightgray`, legacy default context `lightgray`).
- Unicode escape sequences (`\u0048`, `\u3053`, etc.): `magenta` - MATCH (TS
  `(escape)` -> `@keyword.directive`, legacy `keyword \\u... magenta`).
- Substitutions (`${app.home}`, `${user.home}`, etc.): `brightgreen` - MATCH (TS
  `(substitution)` -> `@variable.special`, legacy `keyword ${*} brightgreen`).

Intentional improvements over legacy
-------------------------------------

- TS correctly parses property values as complete spans via the grammar's
  `(value)` node. Legacy's value highlighting relies on the default context
  color and can be disrupted by embedded `:` or `=` characters (e.g., URLs like
  `https://example.com/api?key=value` cause legacy to insert extra `brightcyan`
  coloring at each `:` and `=`).
- TS highlights escape sequences (`\n`, `\t`, `\\`) within values via the
  `(escape)` node (`magenta`). Legacy only recognizes `\uXXXX` unicode escapes
  but not `\n`, `\t`, or `\\`.
- TS treats the entire multiline continuation (backslash at end of line) as part
  of the value node, maintaining consistent value coloring. Legacy uses a
  separate `context exclusive \\\n \n` which can cause color inconsistencies at
  continuation boundaries.
- TS handles the space separator (`key3 value3`) correctly by parsing the
  grammar structure. Legacy only matches `=` and `:` as explicit separators.

Known shortcomings
------------------

- Legacy highlights numbers after `=` as `brightcyan` (e.g., `=5432`, `=8080`,
  `=100`), treating them as part of the `=` keyword match. TS shows all values
  uniformly as `lightgray` via `@variable`. This is actually more correct but
  visually different.
- Legacy highlights HTML color codes (`#3498DB`, `#FFFFFF`) as `green` via
  `keyword whole #... green`. TS shows these as plain `lightgray` values with no
  special treatment.
- Legacy highlights boolean values (`true`, `false`) as `white` via `keyword
  whole true/false white`. TS shows these as plain `lightgray` values like any
  other value text.
- The `unicode.smile=\u263A` line shows the unicode escape highlighted in the
  legacy output (`magenta`) but NOT in the TS output. The TS grammar may not
  parse `\u263A` as an escape node in this context since the hex digits A is
  uppercase but the pattern expects specific casing.
- Legacy mishandles colons within values (e.g., `jdbc:postgresql://localhost/db`
  shows extra `brightcyan` at each `:`). TS correctly treats the entire value as
  a single `lightgray` span.
- Legacy mishandles equals signs within values (e.g., `key=value&fmt=json` in
  URLs). TS correctly treats the full value after the first separator as one
  span.
- The multiline continuation in TS output shows no special highlighting for the
  backslash or continuation lines. Legacy highlights the trailing `\` in
  `yellow` and uses a special context for continuation lines.
- Neither engine highlights property key hierarchies (dot- separated namespaces)
  with distinct colors for each level.
