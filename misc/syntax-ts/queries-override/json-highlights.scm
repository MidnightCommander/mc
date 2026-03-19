;; Tree-sitter highlight queries for JSON
;; Colors aligned with MC's default json.syntax

;; Keys (first string in pair) -> green (string)
(pair
  key: (string) @string)

;; Value strings -> green (string)
(pair
  value: (string) @string)

;; Top-level / array strings -> green (string)
(array (string) @string)

;; Escape sequences -> brightgreen (string.special)
(escape_sequence) @string.special

;; Numbers -> brightgreen (string.special, MC uses brightgreen)
(number) @string.special

;; Constants -> brightgreen (string.special, MC uses brightgreen)
[
  (true)
  (false)
  (null)
] @string.special

;; Comments (JSON5 / jsonc)
(comment) @comment

;; Delimiters -> brightcyan
[
  ","
  ":"
  "{"
  "}"
  "["
  "]"
] @delimiter
