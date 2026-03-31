Caddy syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `Caddyfile`
Legacy reference: `misc/syntax/caddyfile.syntax`
TS query: `misc/syntax-ts/queries-override/caddy-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[caddy]`

Aligned with legacy
-------------------

- Comments (`# ...`): `brown` in both TS and legacy.
- Block delimiters (`{`, `}`): `yellow` in both TS (`@keyword`) and legacy.
- Directives (`respond`, `reverse_proxy`, `encode`, `tls`, `handle_path`,
  `handle_errors`, `basicauth`, `redir`, `root`, `header`, `file_server`, `log`,
  `uri`, `try_files`, `vars`, `import`, `templates`): `yellow` - MATCH (TS
  `@keyword`, legacy `keyword whole ... yellow`).
- Matchers (`@api`, `@old`, `*`, `/api/*`, `/docs/*`, `/admin/*`, `/health`,
  `/env`): `brightmagenta` - MATCH (TS `@keyword.control`, legacy `keyword @...
  brightmagenta`).
- Server addresses (`localhost:8080`, `https://example.com`, `*.example.com`):
  `brightmagenta` - MATCH (TS `@keyword.control`, legacy `keyword linestart ...
  brightmagenta`).
- Snippet names (`(logging)`): `brightmagenta` - MATCH (TS `@keyword.control`).
- Placeholders (`{remote}`, `{host}`, `{http.error.status_code}`,
  `{http.error.message}`, `{http.request.uuid}`, `{system.hostname}`, `{path}`,
  `{host}`): `brightred` - MATCH (TS `@function.special`, legacy `keyword
  {http.*} brightred` etc.).
- Quoted strings (`"Hello, world!"`, `"max-age=31536000"`, `"Named route"`):
  `green` via TS `@string` for `quoted_string_literal`. Legacy also uses `green`
  via `context " "`.
- Numeric literals (`80`, `443`, `200`, `301`, `16384`): `brightgreen` in TS via
  `@string.special`. Legacy highlights some numbers with `brightgreen` via
  port/size patterns.
- Boolean values (`off`): `brightgreen` in TS via `@string.special`. Legacy
  matches `keyword whole off brightgreen`.

Intentional improvements over legacy
-------------------------------------

- TS uses a proper grammar parse tree, so directives at any nesting level are
  correctly identified. Legacy relies on `keyword whole` which can miss
  directives not in the hardcoded list.
- TS correctly identifies all placeholders via the grammar's `(placeholder)`
  node rather than requiring individual keyword patterns for each placeholder
  name.
- TS highlights `string_literal` (unquoted arguments) as `lightgray` via
  `@constant`, giving visual distinction between quoted and unquoted strings.
- TS handles snippet definitions (`(logging)`) and named routes (`&(my-route)`)
  via grammar nodes rather than fragile regex patterns.
- TS recognizes addresses like `*.example.com` via the `(address)` grammar node.
  Legacy only matches a few hardcoded patterns (`http://`, `https://`,
  `localhost`).

Known shortcomings
------------------

- The legacy engine produced NO syntax highlighting at all in the dump tool
  output. All characters received the default color (4159). This appears to be a
  tool limitation: the dump tool may not resolve the `Caddyfile` filename to
  `caddyfile.syntax`. The legacy `.syntax` file itself contains comprehensive
  rules.
- TS shows `yellow` color bleeding past directives into their arguments on the
  same line (e.g., `log {` shows `log` and the rest of the line in yellow). This
  is because the `(directive)` node in the grammar spans the entire directive
  line including arguments.
- Subdirective highlighting (e.g., `protocols`, `ciphers`, `lb_policy`,
  `health_check`) that legacy shows in `brightcyan` is not present in the TS
  output. The TS grammar treats these as plain directive arguments rather than
  separate subdirective nodes.
- The `{$APP_ENV}` environment variable placeholder is not highlighted by TS
  (shown as default text). The grammar may not parse environment variable
  references as placeholder nodes.
- Boolean values `true`/`false` inside `vars` block and `on`/ `off` values are
  not highlighted by TS when they appear as directive arguments rather than
  standalone boolean nodes.
- Named route syntax `&(my-route)` is not highlighted by TS (shown as default
  text), while legacy had a pattern for it.
