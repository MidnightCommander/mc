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

;; C11/C23 keywords that are grammar tokens -> yellow
[
  "_Atomic"
  "_Noreturn"
  "alignas"
  "constexpr"
] @keyword

;; C11/C23 keywords recognized as type identifiers by the grammar
((type_identifier) @keyword
  (#any-of? @keyword
    "_Bool" "_Complex" "_Imaginary" "wchar_t"))

;; C11/C23 keywords recognized as identifiers by the grammar
((identifier) @keyword
  (#any-of? @keyword
    "static_assert" "_Static_assert"
    "_Alignas" "_Alignof"
    "_Noreturn" "_Generic"
    "thread_local" "_Thread_local"
    "typeof" "typeof_unqual"
    "alignof" "restrict" "asm"))

;; Ellipsis -> yellow
"..." @keyword

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

;; Strings -> green
(string_literal) @string
(system_lib_string) @string

;; Escape sequences inside strings -> brightgreen
(escape_sequence) @string.special

;; Constants -> lightgray
(null) @constant
(true) @constant
(false) @constant

;; Numbers -> lightgray
(number_literal) @number

;; Character literals -> brightgreen
(char_literal) @string.special

;; Labels -> cyan
(statement_identifier) @label

;; Types -> yellow
(primitive_type) @type
(sized_type_specifier) @type

;; Comments -> brown
(comment) @comment
