;; Tree-sitter highlight queries for C language
;; Colors aligned with MC's default c.syntax

;; Keywords -> yellow
[
  "break"
  "case"
  "const"
  "continue"
  "default"
  "do"
  "else"
  "enum"
  "for"
  "goto"
  "if"
  "inline"
  "return"
  "sizeof"
  "struct"
  "switch"
  "typedef"
  "union"
  "volatile"
  "while"
] @keyword

;; Storage class specifiers -> yellow (same as keyword in MC)
[
  "auto"
  "extern"
  "register"
  "static"
] @keyword

;; Preprocessor -> brightred
[
  "#define"
  "#elif"
  "#else"
  "#endif"
  "#if"
  "#ifdef"
  "#ifndef"
  "#include"
] @function.special
(preproc_directive) @function.special

;; Arithmetic/comparison/logical operators -> yellow
[
  "!"
  "!="
  "%"
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
  "||"
] @operator.word

;; Field operators -> yellow
[
  "."
  "->"
] @operator.word

;; Bitwise operators -> brightmagenta
[
  "&"
  "|"
  "^"
  "~"
] @delimiter.special

;; Semicolons -> brightmagenta
";" @delimiter.special

;; Ternary operators -> brightcyan
[
  "?"
  ":"
] @delimiter

;; Brackets -> brightcyan
[
  ","
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

(string_literal) @string
(system_lib_string) @string

;; Constants -> lightgray
(null) @constant
(true) @constant
(false) @constant

(number_literal) @number
(char_literal) @string.special

(statement_identifier) @label
(primitive_type) @type
(sized_type_specifier) @type

(comment) @comment
