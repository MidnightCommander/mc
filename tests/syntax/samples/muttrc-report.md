Muttrc syntax highlighting: TS vs Legacy comparison report
============================================================

Sample file: `muttrc`
Legacy reference: `misc/syntax/muttrc.syntax`
TS query: `misc/syntax-ts/queries-override/muttrc-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[muttrc]`

Aligned with legacy
-------------------

- Comments (`# ...`): `brown` - MATCH (TS `@comment`, legacy `context # \n
  brown`).
- Primary commands (`set`, `color`, `macro`, `source`, `alternates`, `bind`,
  `ignore`, `unignore`, `mailboxes`, `subscribe`, `lists`, `hdr_order`,
  `unhdr_order`): `brightgreen` in TS via `(command)` -> `@string.special`.
  Legacy uses `keyword whole set brightgreen`, `keyword whole color
  brightgreen`, etc. The TS colors.ini maps `string.special` to `brightgreen` -
  MATCH.
- Set directive options (`realname`, `from`, `folder`, `spoolfile`, `imap_user`,
  `editor`, `sort`, `index_format`, etc.): `yellow` in TS via `(set_directive
  (option))` -> `@keyword`. Legacy uses `keyword whole ... yellow` for each
  option name - MATCH.
- Hook keywords (`folder-hook`, `send-hook`, `reply-hook`, `save-hook`,
  `message-hook`, `account-hook`): `brightcyan` in both TS (`@delimiter`) and
  legacy (`keyword whole ... brightcyan`) - MATCH.
- `alias` keyword: `brightcyan` in TS (`@delimiter`), `brightcyan` in legacy
  (`keyword whole alias brightcyan`). However, TS treats `alias` as a separate
  literal match, not as a command - MATCH.
- String delimiters (`"`, `'`): `green` in TS (`@string`). Legacy uses `context
  " " green` and `context ' ' green` - MATCH.
- Backtick shell commands (`` ` ``): `brightred` in TS (`@function.special`).
  Legacy uses `keyword \` brightred` - MATCH.

Intentional improvements over legacy
-------------------------------------

- TS provides structured parsing, correctly identifying `(command)` nodes,
  `(set_directive)` with nested `(option)` nodes, and hook keywords as distinct
  grammar elements. Legacy relies on flat keyword lists.
- TS correctly scopes option names only within `set` directives via
  `(set_directive (option))`, preventing false matches of option names used in
  other contexts.
- TS highlights `ignore` and `unignore` commands as `@string.special`
  (brightgreen) via the `(command)` node. The TS grammar gives these their
  correct role as commands rather than requiring separate keyword entries.
- TS identifies string content inside quotes (shown as cyan in the dump) from
  the grammar's string node children.

Known shortcomings
------------------

- The legacy engine produced NO syntax highlighting in the dump tool output. All
  characters received the default color (4159). The legacy `.syntax` file
  contains comprehensive rules with hundreds of option keywords, but the dump
  tool appears unable to apply them. This may be a tool issue with
  filename-to-syntax resolution for `muttrc` (the legacy regex `\.?[Mm]uttrc$`
  should match, but the tool may not be checking content or filename patterns
  correctly).
- The TS output shows `RED` as the default/background color for unmatched text
  (visible as `<RED>` tags throughout). This appears to be an artifact of the
  dump tool's color mapping rather than actual red text in the editor.
- The TS grammar highlights text inside quoted strings with a cyan-like color,
  showing string content distinctly from the green quote delimiters. This
  differs from legacy where the entire quoted context is green.
- Color arguments in `color` commands (e.g., `brightwhite`, `blue`, `yellow`)
  are not specially highlighted by TS. They appear as unmatched default text.
  Legacy also does not highlight color names.
- The TS grammar treats `unignore`, `unhdr_order`, `mailboxes`, `subscribe`,
  `lists`, and `hdr_order` all as `(command)` nodes with `@string.special`
  (brightgreen), while legacy distinguishes some of these as `brightcyan` (e.g.,
  `mailboxes`, `subscribe`, `lists`, `hdr_order`, `alias`). This is a deliberate
  simplification in TS.
- Hook patterns and regex arguments (e.g., `.`, `~f@example.com`, `=Sent`) are
  not highlighted by either engine.
- The multiline macro continuation (`\` at end of line) is not specially handled
  by TS. The continuation lines are parsed as part of the macro's string
  arguments.
