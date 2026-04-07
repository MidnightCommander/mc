;; Tree-sitter highlight queries for INI/Config files
;; Colors aligned with MC's default ini.syntax

;; Section names [] -> yellow (keyword)
(section_name) @keyword

;; Keys -> cyan (label, MC default context is cyan for keys)
(setting_name) @label

;; Values -> brightcyan (delimiter, MC uses brightcyan for values after =)
(setting_value) @delimiter

;; Comments -> brown (comment)
(comment) @comment

;; = -> brightred (variable.builtin)
[
  "="
] @variable.builtin

;; Section brackets -> yellow (keyword)
[
  "["
  "]"
] @keyword
