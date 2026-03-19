;; Tree-sitter highlight queries for Go
;; Colors aligned with MC's default go.syntax

;; Keywords -> yellow
[
  "break"
  "case"
  "chan"
  "const"
  "continue"
  "default"
  "defer"
  "else"
  "fallthrough"
  "for"
  "func"
  "go"
  "goto"
  "if"
  "import"
  "interface"
  "map"
  "package"
  "range"
  "return"
  "select"
  "struct"
  "switch"
  "type"
  "var"
] @keyword

(comment) @comment

(interpreted_string_literal) @string
(raw_string_literal) @string
(rune_literal) @constant

;; nil/true/false/iota -> brown (function.builtin) in MC go.syntax
[
  (true)
  (false)
  (nil)
  (iota)
] @function.builtin

(label_name) @label

;; Channel operator <- -> brightmagenta
"<-" @delimiter.special

;; Operators -> brightcyan
[
  "="
  ":="
  "+="
  "-="
  "*="
  "/="
  "%="
  "&="
  "|="
  "^="
  "<<="
  ">>="
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "&&"
  "||"
  "!"
  "+"
  "-"
  "*"
  "/"
  "%"
  "&"
  "|"
  "^"
  "<<"
  ">>"
] @operator

;; Builtin types -> brightgreen
((type_identifier) @type.builtin
 (#match? @type.builtin "^(int|int8|int16|int32|int64|uint|uint8|uint16|uint32|uint64|uintptr|float32|float64|complex64|complex128|byte|rune|string|bool|error)$"))

;; Builtin functions -> brown
((identifier) @function.builtin
 (#match? @function.builtin "^(append|cap|close|complex|copy|delete|imag|len|make|new|panic|print|println|real|recover)$"))

;; Brackets/parens -> brightcyan
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Delimiters -> brightcyan
[
  "."
  ";"
  ","
] @delimiter
