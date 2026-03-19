;; Tree-sitter highlight queries for HCL (Terraform) language
;; Adapted from helix-editor queries for the tree-sitter-hcl grammar

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

((identifier) @keyword
 (#match? @keyword "^(var|local|path|module|root|cwd|resource|variable|data|locals|terraform|provider|output)$"))

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
