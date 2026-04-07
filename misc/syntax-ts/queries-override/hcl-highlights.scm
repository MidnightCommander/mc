;; Tree-sitter highlight queries for generic HCL files
;; For Terraform-specific highlighting (.tf/.tfvars), see terraform-highlights.scm
;; HCL is a generic language where block names are arbitrary identifiers.

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
