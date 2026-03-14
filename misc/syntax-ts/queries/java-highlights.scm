;; Tree-sitter highlight queries for Java
;; Colors aligned with MC's default java.syntax

;; Keywords -> yellow
[
  "abstract"
  "assert"
  "break"
  "case"
  "catch"
  "class"
  "continue"
  "default"
  "do"
  "else"
  "enum"
  "extends"
  "final"
  "finally"
  "for"
  "if"
  "implements"
  "import"
  "instanceof"
  "interface"
  "native"
  "new"
  "package"
  "private"
  "protected"
  "public"
  "return"
  "static"
  "strictfp"
  "switch"
  "synchronized"
  "throw"
  "throws"
  "transient"
  "try"
  "volatile"
  "while"
  "yield"
  "record"
  "sealed"
  "permits"
  "non-sealed"
] @keyword

(this) @keyword
(super) @keyword

;; Primitive types -> yellow (keyword)
[
  "byte"
  "char"
  "double"
  "float"
  "int"
  "long"
  "short"
] @keyword

(boolean_type) @keyword
(void_type) @keyword

;; true/false/null -> yellow (keyword) in MC's java.syntax
[
  (true)
  (false)
  (null_literal)
] @keyword

(line_comment) @comment
(block_comment) @comment

(string_literal) @string
(character_literal) @string.special

;; Annotations -> brightred
(marker_annotation
  name: (identifier) @function.special)
(annotation
  name: (identifier) @function.special)

(labeled_statement
  (identifier) @label)

;; Operators -> yellow
[
  "!"
  "!="
  "%"
  "&"
  "&&"
  "*"
  "+"
  "++"
  "+="
  "-"
  "--"
  "-="
  "/"
  "<"
  "<<"
  "<="
  "="
  "=="
  ">"
  ">="
  ">>"
  ">>>"
  "->"
  "*="
  "/="
  "%="
  "|"
  "||"
  "^"
  "~"
] @operator.word

;; Semicolons -> brightmagenta
";" @delimiter.special

;; Brackets/parens -> brightcyan
[
  ","
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Ternary/colon -> brightcyan
[
  "."
  ":"
] @delimiter
