;; Tree-sitter highlight queries for IDL (Interface Definition Language)
;; Colors aligned with MC's idl.syntax
;; MC: keywords=yellow, operators=yellow, comments=brown, strings=green

;; Keywords -> yellow (@keyword)
[
  "module"
  "interface"
  "struct"
  "union"
  "enum"
  "typedef"
  "const"
  "exception"
  "valuetype"
  "eventtype"
  "component"
  "home"
  "factory"
  "finder"
  "native"
  "local"
  "abstract"
  "custom"
  "truncatable"
  "supports"
  "public"
  "private"
  "switch"
  "case"
  "default"
  "void"
  "in"
  "out"
  "inout"
  "raises"
  "readonly"
  "attribute"
  "oneway"
  "context"
  "import"
  "provides"
  "uses"
  "emits"
  "publishes"
  "consumes"
  "manages"
  "primarykey"
  "porttype"
  "port"
  "mirrorport"
  "connector"
  "multiple"
  "typeid"
  "typeprefix"
] @keyword

;; Type keywords -> yellow (@keyword)
[
  "short"
  "long"
  "unsigned"
  "char"
  "octet"
  "wchar"
  "float"
  "double"
  "string"
  "wstring"
  "any"
  "sequence"
  "map"
  "fixed"
  "native"
  "Object"
  "ValueBase"
] @keyword

;; Boolean type -> yellow (@keyword)
(boolean_type) @keyword

;; Boolean literals -> yellow (@keyword)
[
  "TRUE"
  "FALSE"
] @keyword

;; Operators -> yellow (@operator.word)
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "="
  "::"
  "<<"
  ">>"
  "~"
] @operator.word

;; Preprocessor -> brightmagenta (@keyword.control)
(preproc_define) @keyword.control
(preproc_call) @keyword.control
(preproc_include) @keyword.control

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string_literal) @string
(system_lib_string) @string

;; Char literals -> brightgreen (@string.special)
(char_literal) @string.special

;; Escape sequences -> brightgreen
(escape_sequence) @string.special

;; Brackets -> brightcyan (@delimiter)
[
  "{"
  "}"
  "("
  ")"
  "["
  "]"
] @delimiter

;; Punctuation -> brightcyan (@delimiter)
[
  ","
  ":"
  "<"
  ">"
] @delimiter

;; Semicolons -> brightmagenta (@delimiter.special)
";" @delimiter.special
