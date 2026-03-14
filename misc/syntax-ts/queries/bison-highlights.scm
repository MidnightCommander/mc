;; Tree-sitter highlight queries for Yacc/Bison
;; Colors aligned with MC's yxx.syntax
;; MC: keywords=yellow, preprocessor=brightred, comments=brown, strings=green

;; Bison declaration names (%token, %type, %left, etc.) -> yellow (@keyword)
(declaration_name) @keyword

;; Section markers -> brightcyan (@delimiter)
[
  "%{"
  "%}"
  "%%"
] @delimiter

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string_literal) @string
(char_literal) @string.special

;; Number literals -> lightgray
(number_literal) @number

;; Grammar rule identifiers -> brightcyan (@tag)
(grammar_rule_identifier) @tag

;; Type tags -> yellow (@type)
(type_tag) @type

;; Punctuation
[
  ":"
  "|"
  ";"
] @delimiter
