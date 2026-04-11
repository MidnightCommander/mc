;; Tree-sitter highlight queries for Terraform (.tf/.tfvars) files
;; Uses the HCL grammar with Terraform-specific variable reference prefixes.
;; Block name coloring follows the same generic principle as HCL.

[
  "if"
  "else"
  "endif"
  "for"
  "endfor"
  "in"
] @keyword

[
  (ellipsis)
  "?"
  "=>"
] @operator

[
  "!"
  "*"
  "/"
  "%"
  "+"
  "-"
  ">"
  ">="
  "<"
  "<="
  "=="
  "!="
  "&&"
  "||"
] @operator

[
  "."
  ".*"
  ","
  "[*]"
] @delimiter

[
  "{"
  "}"
  "["
  "]"
  "("
  ")"
] @delimiter

[
  ":"
  "="
] @operator

((identifier) @type
 (#match? @type "^(bool|string|number|object|tuple|list|map|set|any)$"))

;; Top-level block names -> brightmagenta (keyword.directive)
(config_file
  (body
    (block
      (identifier) @keyword.directive)))


;; Terraform variable reference prefixes
(variable_expr
  (identifier) @keyword
  (#match? @keyword "^(var|local|data|module|path|terraform|count|each|self)$"))

(comment) @comment
(null_lit) @constant
(numeric_lit) @number
(bool_lit) @constant

[
  (template_interpolation_start)
  (template_interpolation_end)
  (template_directive_start)
  (template_directive_end)
  (strip_marker)
] @operator

[
  (heredoc_identifier)
  (heredoc_start)
] @string

[
  (quoted_template_start)
  (quoted_template_end)
  (template_literal)
] @string
