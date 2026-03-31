;; Tree-sitter highlight queries for C++ language
;; Colors aligned with MC's default cxx.syntax

;; Keywords -> yellow
[
  "alignas"
  "alignof"
  "break"
  "case"
  "catch"
  "class"
  "co_await"
  "co_return"
  "co_yield"
  "concept"
  "const"
  "consteval"
  "constexpr"
  "constinit"
  "continue"
  "decltype"
  "default"
  "delete"
  "do"
  "else"
  "enum"
  "explicit"
  "extern"
  "final"
  "for"
  "friend"
  "goto"
  "if"
  "inline"
  "mutable"
  "namespace"
  "new"
  "noexcept"
  "operator"
  "override"
  "private"
  "protected"
  "public"
  "requires"
  "return"
  "sizeof"
  "static"
  "static_assert"
  "struct"
  "switch"
  "template"
  "throw"
  "try"
  "typedef"
  "typename"
  "union"
  "using"
  "virtual"
  "volatile"
  "while"
] @keyword

;; true/false/nullptr/this -> yellow (keyword) in MC cxx.syntax
(true) @keyword
(false) @keyword
"nullptr" @keyword
(this) @keyword

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

;; Operators -> yellow
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
  "::"
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

;; Other delimiters -> brightcyan
[
  "."
  ":"
] @delimiter

(string_literal) @string
(system_lib_string) @string
(raw_string_literal) @string

(null) @keyword
(char_literal) @string.special

(statement_identifier) @label
(primitive_type) @type
(sized_type_specifier) @type
(auto) @type

;; Bitwise operators -> brightmagenta
[
  "&"
  "|"
  "^"
  "~"
] @delimiter.special

;; Alternative operator tokens -> yellow (keyword)
[
  "and"
  "or"
  "not"
  "xor"
  "bitand"
  "bitor"
  "compl"
  "and_eq"
  "or_eq"
  "xor_eq"
  "not_eq"
] @keyword

(comment) @comment
