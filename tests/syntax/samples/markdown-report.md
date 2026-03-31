Markdown syntax highlighting: TS vs Legacy comparison report
=============================================================

Sample file: `markdown.md`
Legacy reference: `misc/syntax/markdown.syntax`
TS query: `misc/syntax-ts/queries-override/markdown-highlights.scm`
TS inline query:
`misc/syntax-ts/queries-override/markdown_inline-highlights.scm`
TS injections:
`misc/syntax-ts/queries-override/markdown-injections.scm`
TS colors: `misc/syntax-ts/colors.ini` `[markdown]` and `[markdown_inline]`

Aligned with legacy
-------------------

- Headings `#` through `###` (marker and content): `brightred` - MATCH
- Inline code spans (`` `code` ``): `cyan` - MATCH
- Indented code blocks (4-space indent): `cyan` - MATCH
- Fenced code block delimiters (`` ``` ``): `cyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- Headings `####` through `######`: TS colors all heading levels as `brightred`
  uniformly. Legacy uses `red` for `####`+ and `brightred` for `#`-`###`. The
  distinction is arbitrary and TS's uniform treatment is cleaner.
- Setext headings (`===`/`---` underlines): TS colors both the content and the
  underline as `brightred`. Legacy does not recognize setext headings at all
  (they appear as DEFAULT).
- Bold (`**...**`, `__...__`): TS colors the entire node including content as
  `brightmagenta`. Legacy only colors the `**`/`__` markers as `white` and
  leaves content as DEFAULT. TS's full-node coloring is more readable and
  clearly marks the extent of bold text.
- Italic (`*...*`, `_..._`): TS colors the entire node including content as
  `magenta`. Legacy only colors the `*`/`_` markers as `yellow` and leaves
  content as DEFAULT. Same rationale as bold.
- Bold italic (`***...***`, `___...___`): TS correctly nests bold inside italic
  (or vice versa) with both `brightmagenta` and `magenta` visible. Legacy
  partially handles this with `**` as `white` and `*` as `yellow` but the
  content is DEFAULT.
- Strikethrough (`~~...~~`): TS colors as `brightcyan`. Legacy does not
  recognize strikethrough at all.
- Code span content: TS colors the entire span (delimiters + content) as `cyan`.
  Legacy only colors the backtick delimiters as `cyan` and content as DEFAULT.
  TS's approach is more consistent and clearly marks the full extent of inline
  code.
- Fenced code block language injection: TS parses fenced code block content
  with the appropriate language grammar (e.g. Python inside
  `` ```python ``), providing full syntax highlighting within code blocks.
  Legacy colors the entire block uniformly as `cyan` regardless of language.
- Fenced code block info string
  (`` ```python ``): TS colors the language identifier as `cyan` alongside the
  delimiters. Legacy does not distinguish the info string.
- Blockquote markers (`>`): TS colors the `>` marker as `green`. Legacy has a
  `context linestart > \n green` rule but it only activates for the `>`
  character itself; the rest of the line is inconsistently handled.
- Links (`[text](url)`, `[ref]`, `![alt](url)`): TS colors entire link
  constructs as `yellow`, including reference links, images, autolinks, and
  email autolinks. Legacy has a limited `[*](*)` keyword pattern that only
  matches simple inline links.
- Escape sequences (`\*`, `\_`, `` \` ``, `\\`, etc.): TS colors all backslash
  escapes as `brightgreen`. Legacy only partially handles `\_` (as DEFAULT) and
  `\*` (triggers the `*` keyword pattern).
- Entity references (`&amp;`, `&#42;`): TS colors as `brightgreen`. Legacy does
  not recognize entity references.
- HTML blocks (`<div>...</div>`): TS colors as `brightred` with HTML injection
  for full tag highlighting. Legacy does not recognize HTML blocks.
- Inline HTML tags (`<b>`, `</b>`): TS colors tags as `brightred`. Legacy does
  not recognize inline HTML.
- Thematic breaks (`---`, `***`, `___`): TS colors as `brightcyan`. Legacy does
  not recognize thematic breaks.
- List markers (`-`, `+`, `*`, `1.`, `3)`): TS colors as `lightgray` (matching
  DEFAULT). Legacy does not distinguish list markers.
- Table cells: TS applies `markdown_inline` injection to pipe table cells,
  enabling inline formatting (`*italic*`, `**bold**`, `` `code` ``) inside
  tables. Legacy does not recognize tables.

Known shortcomings
------------------

- Fenced code blocks without a language identifier: TS only colors the
  `` ``` `` delimiters as `cyan` and leaves the content as DEFAULT. Legacy
  colors the entire block (delimiters + content) as `cyan`.
  This is a tradeoff of the injection approach -- without a language, no
  injection occurs and content is uncolored.
- Blockquote content beyond the `>` marker is not colored. TS only colors the
  `>` marker itself as `green`. The content within blockquotes gets inline
  injection for formatting but has no blockquote-specific background or
  foreground color.
- Nested blockquote markers on continuation lines (second `>` in `> > text`) are
  colored, but the first `>` on those lines may not be since tree-sitter
  attributes it to the outer blockquote structure rather than a marker node.
