Turtle syntax highlighting: TS vs Legacy comparison report
===========================================================

Sample file: `tests/syntax/samples/turtle.ttl`
Legacy reference: `misc/syntax/turtle.syntax`
TS query: `misc/syntax-ts/queries-override/turtle-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[turtle]`

Aligned with legacy
-------------------

- Directives (`@prefix`, `@base`) -> `magenta` via `@keyword.directive` - MATCH.
- Keyword `a` (rdf:type shorthand) -> `yellow` via `@keyword` - MATCH.
- Boolean literals (`true`, `false`) -> `yellow` via `@keyword` - MATCH.
- IRI references (`<http://...>`) -> `brightred` via `@function.special` - MATCH
  with legacy's `< >` context.
- Datatype annotation (`^^`) -> `brightmagenta` via `@keyword.control` - MATCH.
- Language tags (`@en`, `@fr`, `@de`) -> `brightmagenta` via `@keyword.control`
  - MATCH.
- Strings (double-quoted `"..."`) -> `green` via `@string` - MATCH.
- Multi-line strings (`"""..."""`) -> `green` via `@string` - MATCH.
- Comments (`#` lines) -> `brown` via `@comment` - MATCH.
- Punctuation (`.`, `,`, `;`) -> `white` via `@keyword.other` - MATCH.
- Collection parens (`(`, `)`) -> `brightmagenta` via `@keyword.control` -
  MATCH.
- Escape sequences (`\t`, `\n`, `\r`, `\\`) -> `brightgreen` via
  `@string.special` - MATCH.

Intentional improvements over legacy
-------------------------------------

- TS highlights prefixed names (`ex:Alice`, `foaf:Person`, `rdf:subject`,
  `dc:title`, etc.) as `cyan` via `@label`, providing clear visual distinction
  for qualified names. Legacy uses a complex character-class pattern
  (`wholeleft`) that matches prefix patterns as `cyan` but may miss some cases.
- TS highlights namespace declarations (`rdf:`, `rdfs:`, `xsd:`, `foaf:`, `ex:`,
  `dc:`) as `cyan` via `@label` in prefix declarations, matching the prefixed
  name coloring.
- TS highlights blank node labels (`_:node1`, `_:node2`) as `cyan` via `@label`.
  Legacy matches `_:` as `cyan` via `wholeleft` but may not extend the color to
  the full label.
- TS highlights blank node brackets (`[`, `]`) as `cyan` via `@label`, visually
  grouping them with blank node labels.
- TS highlights SPARQL-style directives (`PREFIX`, `BASE`) as `magenta` via
  `@keyword.directive`, which legacy does not recognize (legacy only handles
  `@prefix` and `@base`).
- TS correctly handles the empty prefix (`:localName`) as `cyan`, while legacy
  requires at least one letter before the colon.
- TS structurally distinguishes subjects, predicates, and objects, providing
  consistent coloring even for complex nested structures with blank nodes and
  collections.

Known shortcomings
------------------

- Legacy produces no highlighting at all for this sample file. The legacy
  `turtle.syntax` uses `context linestart #` for comments which requires `#` at
  the exact start of the line, and the file's first line starts with `##` which
  should match but apparently does not trigger any highlighting. Additionally,
  legacy's `context default lightgray` with `spellcheck` and keyword rules
  appear to not produce any ANSI color output from `mc-syntax-dump`, suggesting
  a compatibility issue with the legacy engine for this syntax file.
- TS does not highlight the `\u00E9` Unicode escape in `"caf\u00E9"` as
  `brightgreen` -- it remains `green` (string color). The TS query has an
  `(echar)` capture for escape sequences, but `\u` escapes may use a different
  node type (`UCHAR`) not covered by the query.
- TS colors the period (`.`) as `white` via `@keyword.other`, while legacy
  intended it as `white` on `brightmagenta` background (the `keyword . white
  brightmagenta` rule). TS does not support background colors in its
  highlighting model.
- The multi-line string closing `."""` followed by ` .` shows the space and
  period after the string with correct coloring in TS, but the closing quotes
  are part of the green string span rather than being distinct delimiters.
