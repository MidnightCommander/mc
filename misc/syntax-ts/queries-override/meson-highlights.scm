;; Tree-sitter highlight queries for Meson build language
;; Colors aligned with MC's default meson.syntax
;; MC: functions=white, built-in objects=yellow, comments=brown,
;;     single-quoted strings=green, double-quoted strings=brightred

;; Keywords -> yellow
[
  "if"
  "elif"
  "else"
  "endif"
  "foreach"
  "endforeach"
  "and"
  "or"
  "not"
] @keyword

(keyword_break) @keyword
(keyword_continue) @keyword

;; Boolean literals -> yellow (MC: built-in objects are yellow)
[
  "true"
  "false"
] @keyword

;; Built-in objects -> yellow (MC: legacy colors these as keyword/yellow)
((identifier) @variable.builtin
  (#any-of? @variable.builtin
    "meson" "host_machine" "build_machine" "target_machine"))

;; Strings -> green (MC uses green for single-quoted strings)
(string) @string

;; Comments -> brown
(comment) @comment

;; Function calls -> white (MC uses white for function names)
(normal_command
  command: (identifier) @keyword.other)

;; Dictionary keys -> yellow (property.key maps to yellow)
(pair
  key: (identifier) @property.key)
