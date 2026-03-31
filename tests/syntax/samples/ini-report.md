INI syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `ini.ini`
Legacy reference: `misc/syntax/ini.syntax`
TS query: `misc/syntax-ts/queries-override/ini-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[ini]`

Aligned with legacy
-------------------

- Section headers (`[general]`, `[database]`, etc.): `yellow` - MATCH (legacy
  colors `[...]` context yellow, TS uses `@keyword` for `section_name` and
  brackets).
- Setting keys (`name`, `host`, `port`, etc.): `cyan` - MATCH (legacy default
  context is cyan, TS uses `@label`).
- Equals sign `=`: `brightred` - MATCH (legacy uses `brightred` for `=`, TS uses
  `@variable.builtin`).
- Setting values (`My Application`, `localhost`, `5432`, etc.): `brightcyan` -
  MATCH (legacy uses `brightcyan` via exclusive `= \n` context, TS uses
  `@delimiter`).
- Hash comments (`# ...`): `brown` - MATCH.
- Semicolon comments (`; ...`): `brown` - MATCH.

Intentional improvements over legacy
-------------------------------------

- TS and legacy produce effectively identical output for INI files. The color
  mapping was designed to exactly replicate the legacy behavior.
- The TS section header highlighting includes the brackets `[` and `]` as part
  of the `@keyword` capture (yellow), matching the legacy context-based approach
  where the entire `[...]` span is yellow.

Known shortcomings
------------------

- The TS output shows section headers (`[general]`, `[database]`, etc.) without
  a closing color tag on the same line, meaning the yellow color from the
  section name may bleed into the next line visually. In practice the line ends
  and color resets, but the dump output shows unclosed tags. Legacy behaves
  similarly with its context-based approach.
- Neither TS nor legacy distinguish between different value types (numbers,
  booleans, paths, URLs). All values after `=` are uniformly `brightcyan`.
- Inline comments after values (e.g., `key = value # comment`) are not
  separately highlighted by either engine. The entire value portion through
  end-of-line is treated as the setting value.
- The legacy engine colors the default context as `cyan`, meaning any text not
  matched by a specific rule (including blank lines between sections) appears in
  cyan. The TS engine only colors explicitly matched nodes, leaving unmatched
  text as default. Both dumps show the same effective result for well-formed INI
  files.
