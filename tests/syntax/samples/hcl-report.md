HCL syntax highlighting: TS report
====================================

Sample file: `hcl.hcl`
TS query: `misc/syntax-ts/queries-override/hcl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[hcl]`
Grammar: `hcl` (language: `hcl`)
Legacy reference: none (legacy produces no highlighting for `.hcl`
files)

Note: There is no legacy syntax highlighting for generic HCL files
in MC. This report documents the TS highlighting choices only.

Color assignments
-----------------

- Top-level block names (`service`, `job`, `locals`, `output`,
  `variable`, etc.): `brightmagenta` (keyword.directive) — any
  identifier that starts a top-level block gets this color. HCL is
  a generic language with no reserved block names.
- Nested block names (`group`, `task`, `config`, `resources`, etc.):
  DEFAULT — inner blocks are not colored to avoid visual noise.
- Control flow keywords (`if`, `else`, `endif`, `for`, `endfor`,
  `in`): `yellow`.
- Comments (`#`, `//`, `/* */`): `brown`.
- Strings (quoted, heredocs): `green`.
- Numbers: `lightgray`.
- Booleans (`true`, `false`): `lightgray` (constant).
- Null (`null`): `lightgray` (constant).
- Type references (`string`, `number`, `bool`, `object`, `tuple`,
  `list`, `map`, `set`, `any`): `yellow` (type).
- Operators (`=`, `+`, `-`, `*`, `/`, `%`, `!`, `==`, `!=`, `<`,
  `>`, `<=`, `>=`, `&&`, `||`, `?`, `=>`, `:`): `brightcyan`.
- Delimiters (`{`, `}`, `[`, `]`, `(`, `)`, `,`, `.`, `.*`,
  `[*]`): `brightcyan`.
- Template interpolation (`${`, `}`): `brightcyan` (operator).
- Heredoc identifiers: `green`.
- Splat expressions (`[*]`, `.*`): `brightcyan` (delimiter).

Design decisions
----------------

- HCL is a generic configuration language with no reserved block
  names. All top-level block identifiers are colored uniformly as
  `brightmagenta` regardless of their name. This distinguishes
  structural blocks from attribute keys without hardcoding any
  specific keyword list.
- Nested block names are left as DEFAULT to avoid confusion with
  variable references and attribute keys, which also appear as
  plain identifiers.
- No variable reference prefixes are colored (unlike Terraform's
  `var.`, `local.`, etc.) since HCL itself has no such convention.
  Applications that embed HCL may define their own prefixes.

Known shortcomings
------------------

- Legacy MC has no highlighting for `.hcl` files at all, so there
  is nothing to compare against.
- Function calls in expressions (e.g. `join()`, `upper()`) are not
  colored. The HCL grammar parses them as `function_call` nodes but
  the query does not capture them to keep the highlighting minimal
  for a generic language.
