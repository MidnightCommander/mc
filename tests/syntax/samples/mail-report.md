Mail syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `sample.mail`
Legacy reference: `misc/syntax/mail.syntax`
TS query: `misc/syntax-ts/queries-override/mail-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[mail]`

Aligned with legacy
-------------------

- `From ` envelope line: `brightred` for the `From` keyword - MATCH. Legacy uses
  `keyword linestart From\s brightred`. TS would use `(header_email)` mapped to
  `@type.builtin` = `brightgreen` for `From:` header specifically (see below).
- `From:` header: `brightgreen` - MATCH. Legacy uses `keyword linestart From:
  brightgreen`.
- `To:` header: `brightmagenta` - would match if TS were active. Legacy uses
  `keyword linestart To: brightmagenta`.
- `Subject:` header: Legacy uses `brightred/Orange` (brightred foreground on
  orange background). TS maps `(header_subject)` to `@tag` = `brightcyan`.
- Email addresses in angle brackets (`<john@example.com>`,
  `<sender@example.com>`): `white` - MATCH. Legacy uses `keyword <*@*> white`.
  TS uses `(email)` mapped to `@string` = `green`.
- Other headers (`Date:`, `Message-ID:`, `MIME-Version:`, `Content-Type:`,
  `X-Mailer:`, etc.): `brown` in legacy via `keyword linestart +: brown`. TS
  maps `(header_other)` to `@markup.environment` = `brightcyan`.
- Header context body text: `cyan` in legacy (the entire header area context is
  cyan). TS does not color non-header body text.
- Quoted text (`> ...`): `brightgreen` for odd levels, `brightred` for even
  levels - legacy context patterns handle nested quoting. TS does not appear to
  parse quoted text.

Intentional improvements over legacy
-------------------------------------

- The TS grammar would provide structured parsing of email headers via distinct
  node types (`header_email`, `header_subject`, `header_other`), whereas legacy
  uses repeated context blocks for each starting header pattern.
- TS would distinguish email addresses semantically via the `(email)` node type
  rather than relying on glob patterns like `<*@*>`.

Known shortcomings
------------------

- The TS engine FAILED to initialize for the `sample.mail` file and fell back to
  legacy mode. The output from `--ts` mode is identical to `--legacy` mode. This
  is because the `mail` grammar has NO extension or filename mapping in
  `misc/syntax-ts/extensions` or `misc/syntax-ts/filenames`. The legacy engine
  entry in `misc/syntax/Syntax.in` uses the filename pattern `Don_t_match_me`
  (intentionally unmatchable) and relies solely on first-line content matching
  (`^(From|Return-(P|p)ath:|From:|Date:)\s`). Mail files have no standard file
  extension -- they are identified by content, not by name. Adding a `.mail` or
  `.eml` extension mapping would be arbitrary and might conflict with other
  uses. This is a fundamental limitation: the TS engine currently supports only
  extension and filename matching, not content-based detection. Until the TS
  engine gains first-line content matching support, the mail grammar cannot be
  activated via TS.
- Because the TS engine cannot be tested, the following captures from the query
  override remain unverified: - `(header_email)` -> `@type.builtin`
  (`brightgreen`) - `(email)` -> `@string` (`green`) - `(header_subject)` ->
  `@tag` (`brightcyan`) - `(header_other)` -> `@markup.environment`
  (`brightcyan`)
- The legacy engine colors `Subject:` as `brightred` on orange background, while
  TS would use `brightcyan` (via `@tag`). This is a color difference, not an
  alignment.
- Neither engine highlights the message body text, email signatures, or URLs in
  the body.
- The legacy engine uses alternating `brightgreen`/`brightred` for nested quote
  levels (odd/even). The TS grammar does not appear to have captures for quoted
  reply lines.
- Bare email addresses in body text (`john@example.com`) are matched by legacy
  (`keyword whole +@+ white`) but have no corresponding TS capture.
- The `Return-Path:` header is handled by a dedicated context in legacy but
  would fall under `(header_other)` in TS, which changes its color from the
  legacy-specific treatment.
