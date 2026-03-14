;; Tree-sitter highlight queries for TOML
;; Colors aligned with MC's default toml.syntax

;; Booleans -> brightcyan (tag, MC uses brightcyan for true/false)
(boolean) @tag

;; Comments -> brown (comment)
(comment) @comment

;; Bare keys -> white/default (variable)
(bare_key) @variable
(dotted_key) @variable
(quoted_key) @variable

;; Strings -> brightgreen (string.special)
(string) @string.special

;; Numbers -> brightcyan (tag, MC uses brightcyan for numbers)
(integer) @tag
(float) @tag

;; Date/time values -> brightgreen (string.special)
(offset_date_time) @string.special
(local_date_time) @string.special
(local_date) @string.special
(local_time) @string.special

;; Table brackets -> keyword (yellow, MC uses yellow for [ ])
[
  "["
  "]"
  "[["
  "]]"
] @keyword

[
  "="
] @operator

[
  "."
  ","
] @delimiter
