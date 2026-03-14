;; Tree-sitter highlight queries for YAML
;; Colors aligned with MC's default yaml.syntax

;; Keys in mappings -> yellow (property.key)
(block_mapping_pair
  key: (_) @property.key)
(flow_pair
  key: (_) @property.key)

;; Booleans and null -> brightmagenta (constant.builtin)
(boolean_scalar) @constant.builtin
(null_scalar) @constant.builtin

;; Comments -> brown
(comment) @comment

;; Quoted strings -> green (unquoted string_scalar left as default, matching MC)
(double_quote_scalar) @string
(single_quote_scalar) @string

;; Block scalars -> brown (comment) - MC uses brown context for | and > blocks
(block_scalar) @comment

;; Escape sequences in strings -> brightgreen
(escape_sequence) @string.special

;; Numbers -> lightgray
(integer_scalar) @number
(float_scalar) @number

;; Timestamps
(timestamp_scalar) @string.special

;; Anchors and aliases -> cyan
(anchor) @label
(alias) @label

;; Tags -> yellow
(tag) @type

;; Document markers -> brightcyan
[
  "---"
  "..."
] @delimiter

;; Operators
[
  ":"
  "-"
  ">"
  "|"
  "?"
] @operator

;; Delimiters -> brightcyan
[
  ","
  "["
  "]"
  "{"
  "}"
] @delimiter
