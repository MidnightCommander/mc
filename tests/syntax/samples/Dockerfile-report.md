Dockerfile syntax highlighting: TS vs Legacy comparison report
==============================================================

Sample file: `Dockerfile`
Legacy reference: `misc/syntax/dockerfile.syntax`
TS query: `misc/syntax-ts/queries-override/dockerfile-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[dockerfile]`

Note: The legacy `mc-syntax-dump` tool did not produce colored output for the
`Dockerfile` sample (all characters rendered as default color). The legacy
comparison below is therefore based on reading the `dockerfile.syntax` rules
directly rather than observed dump output.

Aligned with legacy
-------------------

- Instruction keywords (`FROM`, `RUN`, `CMD`, `COPY`, `ADD`, `EXPOSE`, `ENV`,
  `WORKDIR`, `ENTRYPOINT`, `VOLUME`, `USER`, `ARG`, `LABEL`, `HEALTHCHECK`,
  `SHELL`, `ONBUILD`, `STOPSIGNAL`, `AS`): `yellow` - MATCH
- Deprecated `MAINTAINER`: `brightred` - MATCH
- Comments (`#`): `brown` - MATCH
- Double-quoted strings: `green` - MATCH
- Variable expansion (`${VERSION}`, `$APP_HOME`): `brightgreen` - MATCH
- Port numbers (`8080`, `443/tcp`, `3000`): `brightgreen` - MATCH
- Params (`--mount=...`, `--network=...`, `--from=...`, `--chown=...`,
  `--chmod=...`, `--checksum=...`, `--interval=...`, `--timeout=...`):
  `brightmagenta` - MATCH

Intentional improvements over legacy
-------------------------------------

- Image names (`ubuntu`, `node`, `python`) are highlighted in `brightcyan` (via
  `@function`). Legacy left image names as default text.
- Image tags (`:22.04`, `:18-alpine`) and digests (`@sha256:...`) are
  highlighted in `brightgreen` (via `@string.special`). Legacy had no specific
  rule for tags/digests.
- `LABEL` keys (`maintainer`, `version`, `description`) are highlighted in
  `brightcyan` (via `@property`). Legacy treated them as plain text.
- `ENV` variable names (`APP_HOME`, `NODE_ENV`, `PATH`) are highlighted in
  `lightgray` (via `@variable`). Legacy treated them as plain text before the
  `=`.
- `ARG` variable names (`VERSION`, `BUILD_DATE`) are highlighted in `lightgray`
  (via `@variable`). Legacy treated them as plain text.
- Unquoted string values after `ENV`, `USER`, and other instructions are
  highlighted in `green` (via `@string`). Legacy only colored quoted strings.
- The `--link` option renders in `red` in TS output instead of `brightmagenta`.
  This appears to be a parser issue where `--link` is not parsed as a `param`
  node by the dockerfile tree-sitter grammar.

Known shortcomings
------------------

- The legacy dump tool did not produce any colored output for the Dockerfile
  sample, suggesting a file pattern matching issue in the dump tool for
  extensionless filenames. The `dockerfile.syntax` file entry pattern
  `Dockerfile.\*$` should match but did not trigger.
- Variable expansions inside double-quoted strings (e.g.,
  `"Version: ${VERSION}"`) are not individually highlighted by TS; the entire
  string including the expansion renders as plain unquoted string text.
- `COPY --link` renders as `red` instead of `brightmagenta`, suggesting the
  tree-sitter dockerfile grammar does not parse `--link` as a `param` node.
- Shell operators inside `RUN` commands (`&&`, `||`) are not highlighted by TS
  since the Dockerfile grammar does not parse shell command internals. Legacy
  had explicit keyword rules for `&&` and `||`.
- JSON array syntax in `RUN ["sh", "-c", ...]`, `ENTRYPOINT [...]`, and
  `CMD [...]` is not distinguished from regular strings in TS output.
