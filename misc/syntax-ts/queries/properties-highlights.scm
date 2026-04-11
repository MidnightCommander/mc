;; Tree-sitter highlight queries for Java properties files
;; Colors aligned with MC's default properties.syntax

;; Keys -> yellow (property.key)
(property
  (key) @property.key)

;; Values -> default (variable)
(property
  (value) @variable)

;; Substitutions ${...} -> brightgreen (variable.special)
(substitution) @variable.special

;; Escape sequences -> keyword.directive (magenta)
(escape) @keyword.directive

;; Comments -> brown (comment)
(comment) @comment

;; Separators = : -> brightcyan (delimiter)
[
  "="
  ":"
] @delimiter
