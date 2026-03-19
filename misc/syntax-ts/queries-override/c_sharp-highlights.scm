;; Tree-sitter highlight queries for C# language
;; Colors aligned with MC's cs.syntax

;; Most keywords -> yellow (@keyword) -- MC uses yellow for majority
[
  "abstract"
  "as"
  "async"
  "await"
  "base"
  "break"
  "case"
  "catch"
  "checked"
  "const"
  "continue"
  "default"
  "do"
  "event"
  "explicit"
  "extern"
  "finally"
  "fixed"
  "for"
  "foreach"
  "goto"
  "if"
  "implicit"
  "in"
  "is"
  "lock"
  "new"
  "operator"
  "out"
  "override"
  "params"
  "partial"
  "protected"
  "readonly"
  "ref"
  "return"
  "sealed"
  "sizeof"
  "stackalloc"
  "static"
  "switch"
  "this"
  "throw"
  "try"
  "typeof"
  "unchecked"
  "unsafe"
  "var"
  "virtual"
  "volatile"
  "where"
  "while"
  "yield"
] @keyword

;; Type declaration keywords -> white (@keyword.other) -- MC uses white
[
  "class"
  "delegate"
  "enum"
  "interface"
  "namespace"
  "struct"
  "record"
] @keyword.other

;; Access modifiers -> brightred (@function.special) -- MC uses brightred
[
  "internal"
  "private"
  "public"
] @function.special

;; using -> brightcyan (@function) -- MC uses brightcyan
"using" @function

(predefined_type) @type

(comment) @comment
(string_literal) @string
(verbatim_string_literal) @string
(interpolated_string_expression) @string
(character_literal) @string.special

;; null, true, false -> yellow (@keyword) -- MC uses yellow
(null_literal) @keyword
(boolean_literal) @keyword

(labeled_statement
  (identifier) @label)

;; Operators -> yellow (@operator.word) -- MC uses yellow
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "&&"
  "||"
  "!"
  "&"
  "|"
  "^"
  "~"
  "<<"
  ">>"
  "++"
  "--"
  "??"
  "=>"
] @operator.word

;; Delimiters -> brightcyan (@delimiter) -- MC uses brightcyan
[
  "."
  ","
  ":"
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Semicolons -> brightmagenta (@delimiter.special) -- MC uses brightmagenta
";" @delimiter.special
