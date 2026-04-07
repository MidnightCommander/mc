PO syntax highlighting: TS vs Legacy comparison report
======================================================

Sample file: `tests/syntax/samples/po.po`
Legacy reference: `misc/syntax/po.syntax`
TS query: `misc/syntax-ts/queries-override/po-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[po]`

Aligned with legacy
-------------------

- Keywords `msgid`, `msgstr`, `msgctxt`, `msgid_plural`: `brightcyan` via `@tag`
  - MATCH
- Strings (both msgid and msgstr values): `green` via `@string` - MATCH (legacy
  uses `cyan` for msgid strings and `green` for msgstr strings; TS unifies to
  `green`)
- Translator comments (`# ...`): `brown` via `@comment` - MATCH
- Comments (`#` lines): `brown` via `@comment` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS properly recognizes `msgctxt` as a keyword and colors it `brightcyan`.
  Legacy treats `msgctxt` as part of the reference comment context (`#:` line),
  coloring the line after `msgctxt` as `white` rather than giving the keyword
  its own color.
- TS handles `msgid_plural` as a single `brightcyan` keyword. Legacy splits it,
  coloring `msgid` as `brightcyan` and `_plural` as part of the `cyan` string
  context.
- TS colors extracted comments (`#. ...`), references (`#: ...`), flags (`#,
  ...`), and previous untranslated strings (`#~ ...`) uniformly as `brown` via
  `@comment` / `@tag.special`. However the `@tag.special` capture maps to
  `white` in colors.ini, so `#.`, `#:`, `#,` lines render as `white` matching
  legacy. The `#~ ...` lines show as `white` via `@tag.special` instead of
  legacy's `red`, which is a minor difference but still readable.
- TS produces clean single-token output for each line. Legacy sometimes splits
  tokens awkwardly (e.g. `#, no-c-format` splits around `c-format`).

Differences from legacy
-----------------------

- TS colors all strings uniformly `green` via `@string`. Legacy distinguishes
  msgid strings (`cyan`) from msgstr strings (`green`). This is a simplification
  -- TS does not differentiate the two contexts.
- Legacy highlights format specifiers (`%d`, `%s`, `%%`, `\n`, `\t`, `\\`)
  inside strings as `brightgreen`. TS does not break out escape sequences or
  format specifiers -- the entire string is `green`. This loses the visual
  distinction of format parameters.
- Legacy colors fuzzy blocks (`#, fuzzy` through the next blank line) as
  `brightred`, marking the entire fuzzy entry. TS has no fuzzy-aware context --
  fuzzy entries look like normal entries.
- Legacy colors untranslated entries (`msgstr ""` at end of file) as
  `brightred`. TS colors them normally (`brightcyan` keyword + `green` string).
- Legacy colors `#~ ...` (previous untranslated) lines as `red`. TS colors them
  differently -- the `#~` prefix shows as `brown` while `msgid`/`msgstr`
  keywords within still get `brightcyan` and strings get `green`.
- Legacy highlights `c-format` within flag comments as `yellow`. TS does not
  distinguish flag content -- the entire flag line is `white` (via
  `@tag.special`) or `brown` (via `@comment`).

Known shortcomings
------------------

- TS lacks escape sequence highlighting inside strings. Format specifiers like
  `%d`, `%s`, and escape sequences like `\n`, `\t` are all `green` instead of
  the legacy `brightgreen`.
- TS does not distinguish fuzzy entries visually. In legacy, the entire fuzzy
  block is `brightred`, which is a useful visual cue for translators.
- TS does not differentiate msgid vs msgstr string colors. Legacy uses `cyan`
  for original strings and `green` for translated strings, making it easier to
  visually separate source from translation.
- The `#~` previous-untranslated lines lose their distinctive `red` color from
  legacy, blending in with normal comments.
