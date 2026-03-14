;; Tree-sitter highlight queries for CSS language
;; Colors aligned with MC's default css.syntax

(comment) @comment

;; Tag selectors -> white
(tag_name) @tag.special
;; Class selectors -> green
(class_selector) @string
(id_selector) @string
;; Pseudo-classes -> brightmagenta
(pseudo_class_selector (class_name) @delimiter.special)
;; Pseudo-elements -> white
(pseudo_element_selector (tag_name) @tag.special)

;; Properties -> lightgray
(property_name) @constant

(string_value) @keyword.other
;; Color values like #AABBCC -> red
(color_value) @comment.error
;; Numbers -> brightgreen (MC uses brightgreen for values)
(integer_value) @string.special
(float_value) @string.special

;; Plain values (keywords like bold, center, etc) -> brightgreen
(plain_value) @string.special

;; !important -> brightred
(important) @function.special

;; Function names like rgb(), url() -> magenta
(function_name) @keyword.directive

;; At-rules -> brightred
[
  "@media"
  "@import"
  "@charset"
  "@keyframes"
  "@supports"
  "@namespace"
  "@scope"
] @function.special

(at_keyword) @function.special

[
  "and"
  "not"
  "or"
  "only"
] @keyword

[
  "~"
  ">"
  "+"
  "="
  "^="
  "|="
  "~="
  "$="
  "*="
] @operator

;; Semicolons -> brightmagenta
[
  ";"
] @delimiter.special

;; Other delimiters -> brightcyan
[
  ","
  ":"
  "::"
] @delimiter

;; Units like px, em -> brightgreen (MC uses brightgreen for units)
(unit) @string.special
