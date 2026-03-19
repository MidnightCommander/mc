;; Tree-sitter highlight queries for Scala
;; No MC syntax file — using reasonable defaults:
;; keywords=yellow, strings=green, comments=brown

;; Keywords -> yellow
[
  "abstract"
  "case"
  "catch"
  "class"
  "def"
  "do"
  "else"
  "extends"
  "final"
  "finally"
  "for"
  "if"
  "implicit"
  "import"
  "lazy"
  "match"
  "new"
  "object"
  "override"
  "package"
  "private"
  "protected"
  "return"
  "sealed"
  "this"
  "throw"
  "trait"
  "try"
  "type"
  "val"
  "var"
  "while"
  "with"
  "yield"
] @keyword

;; Null/boolean literals -> brightmagenta (constant.builtin)
(null_literal) @constant.builtin
(boolean_literal) @constant.builtin

;; Comments -> brown
(comment) @comment
(block_comment) @comment

;; Strings -> green
(string) @string
(interpolated_string_expression) @string

;; Chars -> brightgreen
(character_literal) @string.special

;; Operators -> brightcyan
[
  "="
  "=>"
  "<-"
  "+"
  "-"
  "*"
  "!"
  ">"
  "|"
  "~"
] @operator

;; Delimiters -> brightcyan
[
  "."
  ","
  ":"
] @delimiter

;; Semicolons -> brightmagenta
";" @delimiter.special
