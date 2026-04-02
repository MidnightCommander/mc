Terraform syntax highlighting: TS report
=========================================

Sample file: `terraform.tf`
TS query: `misc/syntax-ts/queries-override/terraform-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[terraform]`
Grammar: `terraform` (alias for `hcl` parser via symbols file)
Legacy reference: none (legacy produces no highlighting for `.tf`
files)

Note: There is no legacy syntax highlighting for Terraform files in
MC. The `terraform` grammar is an alias for the `hcl` parser with
Terraform-specific query overrides. This report documents the TS
highlighting choices only.

Color assignments
-----------------

- Top-level block names (`terraform`, `resource`, `data`, `variable`,
  `locals`, `module`, `provider`, `output`, `moved`, `removed`):
  `brightmagenta` (keyword.directive) — only blocks at the root
  level of the file get this color.
- Nested block names (`backend`, `required_providers`, `lifecycle`,
  `provisioner`, `dynamic`, `content`, `filter`, `ingress`):
  DEFAULT — inner blocks are not colored to reduce visual noise and
  avoid confusion with attribute keys.
- Variable reference prefixes (`var`, `local`, `data`, `module`,
  `path`, `terraform`, `count`, `each`, `self`): `yellow` — these
  are the standard Terraform reference objects used in expressions
  like `var.region`, `local.tags`, `data.aws_ami.id`,
  `count.index`, `each.value`, `self.id`, `path.module`.
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

Design decisions
----------------

- Terraform uses a separate grammar name (`terraform`) mapped to the
  same `hcl` parser via the `symbols` config file. This allows
  Terraform-specific keyword lists and color choices without
  affecting generic `.hcl` files.
- Top-level vs nested block distinction is achieved using the
  `config_file > body > block > identifier` pattern, which matches
  only blocks at the root level of the file.
- Variable reference prefixes are matched in `variable_expr` context
  only, preventing false coloring of map keys and attribute values
  that happen to share the same name (e.g. `count = 3` in a
  variable type definition vs `count.index` in an expression).
- Resource type references (e.g. `azurerm_user_assigned_identity` in
  `azurerm_user_assigned_identity.aks.id`) are left as DEFAULT.
  While these are meaningful references, they share the same
  `variable_expr` node type as map keys, making it impossible to
  distinguish them structurally.

Known shortcomings
------------------

- Legacy MC has no highlighting for `.tf` files at all, so there is
  nothing to compare against.
- Resource type references in expressions (e.g.
  `aws_instance.web[0].id`) are not colored. The first identifier
  is a `variable_expr` which is the same node type used for map
  keys, so coloring all `variable_expr` would cause false positives.
- Terraform built-in functions (`join`, `upper`, `merge`, `file`,
  `templatefile`, etc.) are not colored. Adding them would require
  a `#any-of?` predicate on `function_call` nodes, similar to the
  gotmpl Sprig function approach.
- Nested block names like `required_providers` inside `terraform {}`
  are DEFAULT. Tree-sitter queries cannot express "this block is
  inside a specific parent block", so all nested blocks are treated
  uniformly.
