Tcl syntax highlighting: TS vs Legacy comparison report
========================================================

Sample file: `tcl.tcl`
Legacy reference: `misc/syntax/tcl.syntax`
TS query: `misc/syntax-ts/queries-override/tcl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[tcl]`

Aligned with legacy
-------------------

- Language keywords (`if`, `else`, `elseif`, `while`, `foreach`, `proc`, `set`,
  `return`, `break`, `continue`, `puts`, `gets`, `open`, `close`, `expr`,
  `eval`, `exec`, `source`, `package`, `require`, `namespace`, `global`,
  `variable`, `upvar`, `uplevel`, `array`, `list`, `lindex`, `lappend`,
  `llength`, `lrange`, `lsearch`, `lsort`, `lreplace`, `string`, `regexp`,
  `regsub`, `incr`, `append`, `format`, `scan`, `info`, `rename`, `trace`,
  `after`, `vwait`, `update`, `interp`, `switch`): `yellow` - MATCH
- Comments (`# ...`): `brown` - MATCH
- Double-quoted strings (`"..."`): `green` - MATCH
- Variable substitutions (`$var`, `${var}`, `$::ns::var`): `brightgreen` - MATCH
- Escape sequences inside strings (`\n`, `\t`, `\\`): `brightgreen` - MATCH

Intentional improvements over legacy
-------------------------------------

- Braced words (`{...}`): TS colors the entire braced word as `green` (via
  `@string` capture on `braced_word`). Legacy colors only the `{` and `}`
  delimiters as `brightcyan` and leaves the content as default text. The TS
  approach is more semantically correct since braced words in Tcl are literal
  values (like strings).
- TS colors the `try`, `catch`, `finally`, `on`, `error` keywords as `yellow`
  via explicit keyword captures. Legacy does not color `try`, `finally`, or `on`
  since they are not in its keyword list (only `catch` and `error` are listed).
- Comments are uniformly `brown` in TS, including the `#` marker. Legacy also
  colors comments as `brown` via context, but the TS approach captures the
  entire comment node uniformly.
- TS now colors `[`, `]`, `{`, `}` brackets as `brightcyan` via `@delimiter`
  capture, matching legacy behavior for these delimiters.
- Procedure bodies in braces: TS colors the entire braced body as `green`
  (braced_word) with keywords inside still getting `yellow` overlay. Legacy
  colors only the `{}` delimiters as `brightcyan` and leaves the body content to
  be individually matched.

Known shortcomings
------------------

- TS now colors `[`, `]`, `{`, `}` brackets as `brightcyan` via `@delimiter`
  capture. This matches legacy behavior.
- TS does not color `(` and `)` parentheses as `brightcyan`. Legacy colors these
  as `brightcyan` via explicit keyword rules. The TCL grammar does not expose
  parentheses as anonymous literals.
- TS now colors `;` semicolons as `brightmagenta` via `@delimiter.special`
  capture. This matches legacy behavior.
- TS does not color operators (`>`, `<`, `=`, `||`, `&&`, `!=`, `==`, `::`) as
  `yellow`. Legacy has explicit keyword rules for these. The TS grammar does not
  expose operators as named nodes for Tcl.
- TS does not color command-line option flags (e.g., `-exact`, `-all`) as
  `cyan`. Legacy colors these via the `wholeright \s-{alpha}+` pattern. The TS
  grammar does not have a specific node type for option flags.
- TS braced-word coloring (`green`) applies broadly to all `{...}` blocks
  including code blocks (proc bodies, if bodies, while bodies). This means
  structural braces that delimit code get `green` background coloring where
  legacy would color them as `brightcyan`. Keywords inside braced code blocks
  still override to `yellow`.
- The `expr` command arguments in braces are colored as `yellow` by TS (via the
  `expr_cmd` keyword capture) rather than `green` for the braced content. Legacy
  colors the braces as `brightcyan` and the content as default. This is a
  deliberate TS choice to highlight `expr` expressions.
- Legacy colors `$` inside `"${name}"` separately from the variable name. TS
  captures the entire `${name}` substitution as `brightgreen` uniformly, which
  is cleaner.
