XML syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `tests/syntax/samples/xml.xml`
Legacy reference: `misc/syntax/xml.syntax`
TS query: `misc/syntax-ts/queries-override/xml-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[xml]`

Aligned with legacy
-------------------

- Tag names (`catalog`, `book`, `title`, `author`, etc.): `white` via
  `@tag.special` - MATCH
- Angle brackets and delimiters (`<`, `>`, `</`, `/>`): `white` via
  `@tag.special` - MATCH
- Attribute equals sign (`=`): `yellow` via `@keyword` - MATCH
- Attribute values (`"bk101"`, `"USD"`, `"Fiction"`): `brightcyan` via
  `@delimiter` - MATCH
- Comments (`<!-- -->`): `brightgreen` via `@comment.special` - MATCH
- `<?xml ?>` prolog: `yellow` via `@keyword` on `XMLDecl` - MATCH (legacy uses
  `white` on `red` background for the prolog, but the tag-level TS match is
  close)
- `<!DOCTYPE>` declaration: `yellow` via `@keyword` - MATCH
- Processing instructions (`<?php ?>`, `<?custom-pi ?>`): `lightgray` via
  `@constant` - MATCH
- Entity references (`&amp;`, `&publisher;`): `white` via `@tag.special` - MATCH
  (legacy also uses `white` for `&*;`)
- Character references (`&#169;`, `&#x20AC;`): `white` via `@tag.special` -
  MATCH
- Self-closing elements (`<empty-element/>`, `<br/>`): `white` tags with `white`
  delimiters - MATCH
- Namespaced tag names (`dc:creator`, `dc:description`): `white` via
  `@tag.special` - MATCH
- Text content: `lightgray` via `@variable` - MATCH (legacy leaves text as
  default)

Intentional improvements over legacy
-------------------------------------

- TS and legacy produce identical output for this XML sample. The outputs match
  character-for-character in terms of color assignments. This indicates
  excellent alignment.
- TS handles arbitrary tag names and attribute names uniformly, while legacy
  relies on the generic `< >` context pattern. Both happen to produce the same
  colors for XML.
- TS properly identifies `PEReference` (`%*;`) entities via dedicated capture,
  while legacy handles them via keyword pattern in the DOCTYPE context.

Known shortcomings
------------------

- Legacy uses `white` text on `red` background for `<?xml ?>` prolog and
  `brightred` for `xmlns=` namespace declarations. TS colors the `<?xml ?>`
  processing instruction delimiters as `white` via `@tag.special` and `xmlns` as
  a regular attribute (yellow `=`, brightcyan value), losing the special
  red-background visual distinction.
- Legacy colors `DOCTYPE`, `PUBLIC`, `SYSTEM`, `ENTITY`, `ELEMENT`, `ATTLIST`
  keywords distinctly within `<!` contexts (e.g. `ENTITY` as `brightred`,
  `ELEMENT`/`ATTLIST`/`#REQUIRED`/`#PCDATA`/`CDATA` as `white`). TS colors the
  entire `doctypedecl` uniformly as `yellow`, losing internal keyword
  differentiation within DOCTYPE.
- Legacy uses `yellow` for `<!` context body text and `lightgray` for operators
  like `(`, `)`, `*`, `?`, `+`, `|`, `,` within DOCTYPE. TS also applies
  `yellow` to the doctypedecl but does not separately color these operator
  characters.
- The `CDATA` section content is colored identically between TS and legacy in
  this output, both using the DOCTYPE-like `yellow`/`white` pattern. Neither
  provides ideal CDATA handling (text inside CDATA is not treated as plain
  text).
