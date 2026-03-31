Go Template syntax highlighting: TS report
===========================================

Sample file: `gotmpl.tmpl`
TS query: `misc/syntax-ts/queries-override/gotmpl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[gotmpl]`
Grammar: `ngalaiko/tree-sitter-go-template` (language: `gotmpl`)
Legacy reference: none (no `misc/syntax/*.syntax` file for Go templates)

Note: There is no legacy syntax highlighting for Go templates in MC. This report
documents the TS highlighting choices only.

Color assignments
-----------------

- Keywords (`if`, `else`, `range`, `with`, `end`, `template`, `define`, `block`,
  `break`, `continue`): `yellow`.
- Comments (`/* ... */`): `brown` -- standard MC comment color.
- Builtin functions (Go template + Sprig + Helm, 240 total): `yellow`
  (function.builtin) -- distinguished from comments (`brown`) and from user
  functions (`brightcyan`). - Go template builtins: `and`, `call`, `eq`, `ge`,
  `gt`, `html`, `index`, `js`, `le`, `len`, `lt`, `ne`, `not`, `or`, `print`,
  `printf`, `println`, `urlquery`. - Sprig categories: strings (`upper`,
  `lower`, `trim`, `trunc`, `replace`, `quote`, etc.), lists (`list`, `first`,
  `last`, `append`, `sortAlpha`, etc.), dicts (`dict`, `get`, `set`, `keys`,
  `merge`, `deepCopy`, etc.), math (`add`, `sub`, `mul`, `div`, `ceil`, `floor`,
  `round`, etc.), dates (`now`, `date`, `dateModify`, `ago`, `unixEpoch`, etc.),
  crypto (`sha256sum`, `b64enc`, `uuidv4`, etc.), regex (`regexMatch`,
  `regexFind`, `regexReplaceAll`, etc.), paths (`base`, `dir`, `ext`, `clean`,
  `isAbs`), conversions (`toString`, `toJson`, `fromJson`, `toDate`, `kindOf`,
  `typeOf`, `default`, `empty`, `coalesce`, `ternary`), and flow control
  (`fail`, `until`, `seq`). - Helm-specific: `include`, `lookup`, `required`,
  `tpl`, `toYaml`, `toYamlPretty`, `fromYaml`, `fromYamlArray`, `fromJsonArray`,
  `toToml`, `mustToToml`.
- User function calls (custom functions not in the builtin list): `brightcyan`
  (function) -- clearly distinguished from builtins.
- Method calls (chained selectors): `brightcyan` (function).
- Fields (`.Values`, `.Release`, `.Name`, `.Items`): `white` (property) -- data
  references stand out from delimiters.
- Variables (`$name`, `$pool`, `$index`, `$`): `brightred` (variable.builtin) --
  matches bash convention for `$`-prefixed variables, clearly distinct from
  fields and delimiters.
- Strings (double-quoted, raw backtick, rune literals): `green`.
- Escape sequences (`\n`, `\t`, `\\`, `\"`): `brightgreen`.
- Numbers (integers, floats, imaginary): `lightgray`.
- Boolean and nil constants (`true`, `false`, `nil`): `brightmagenta`
  (constant.builtin).
- Pipe operator (`|`): `brightcyan` (operator).
- Assignment operators (`:=`, `=`): `brightcyan` (operator).
- Delimiters (`{{`, `}}`, `{{-`, `-}}`, `(`, `)`, `.`, `,`): `brightcyan` --
  template delimiters are visually prominent.
- Text outside template actions: DEFAULT -- plain text is uncolored as it could
  be any host language.

Design decisions
----------------

- Keywords and builtin functions share the same color (`yellow`). They are
  structurally distinguishable: keywords appear as `{{ if`, `{{ range`,
  `{{ end }}` while builtins appear as `{{ len`, `{{ default`, `| trunc`. Using
  `yellow` for builtins instead of `brown` distinguishes them from comments.
- Fields and variables have distinct colors (`white` vs `brightred`) to
  differentiate data access patterns: `.Values.name` (context data, white) vs
  `$name` (local variable, brightred).
- Dot (`.`) is colored as `brightcyan` (delimiter) in all contexts. In Go
  templates, dot serves double duty as the context accessor (`.Name`) and the
  current context value (`.`). Both uses benefit from being visually distinct.
- Template names in `{{ template "name" }}` and `{{ define "name" }}` are
  colored as `green` (string) since they are string literals.

Wrapper grammar features
------------------------

The `gotmpl` grammar is configured as a wrapper grammar in the `wrappers` config
file. This enables two automatic features:

- **ERROR fallback**: when a `.yaml` file (or `.json`, `.toml`, `.html`, `.xml`,
  `.css`) fails to parse with its native grammar (e.g. due to `{{ }}` Go
  template syntax), the system automatically tries `gotmpl` as a fallback. If
  successful, the original grammar is injected into `gotmpl`'s `text` nodes,
  providing host language highlighting alongside Go template highlighting.
- **Compound extensions**: files like `README.md.gotmpl` or `values.yaml.tmpl`
  automatically get the inner extension's grammar injected into `text` nodes.
  The outer extension (`.gotmpl`, `.tmpl`, `.tpl`) determines the gotmpl
  grammar, and the inner extension (`.md`, `.yaml`) determines the host language
  injection.

Recursive injection (up to 3 levels) is supported. For example, a `.md.gotmpl`
file gets: gotmpl -> markdown -> markdown_inline, and fenced code blocks within
the markdown get language-specific highlighting (e.g. gotmpl -> markdown ->
python).

Known shortcomings
------------------

- No language injection for text outside `{{ }}` in plain `.tmpl` files. Without
  a compound extension or ERROR fallback, the `text` nodes remain uncolored.
- The builtin function list is hardcoded in the query override file. New Sprig
  or Helm releases may add functions that require manual additions to the
  `#any-of?` predicate.
