;; Tree-sitter highlight queries for AWK language
;; Colors aligned with MC's default awk.syntax

;; Control flow keywords -> white (@keyword.other)
[
  "if"
  "else"
  "while"
  "for"
  "do"
  "delete"
  "exit"
  "return"
  "in"
  "getline"
  "switch"
  "case"
  "default"
] @keyword.other

(break_statement) @keyword.other
(continue_statement) @keyword.other
(next_statement) @keyword.other
(nextfile_statement) @keyword.other

;; BEGIN/END -> brightred (@function.special) -- MC uses red
[
  "BEGIN"
  "END"
  "BEGINFILE"
  "ENDFILE"
] @function.special

;; function keyword -> brightmagenta (@delimiter.special) -- MC uses brightmagenta
"function" @delimiter.special

;; func (gawk extension) -> brightmagenta (@delimiter.special)
"func" @delimiter.special

(comment) @comment
(string) @string
(regex) @string.special

(field_ref) @variable.builtin

;; print/printf -> white (@keyword.other) -- MC uses white
[
  "print"
  "printf"
] @keyword.other

;; Operators -> yellow (@operator.word) -- MC uses yellow/24 for math/comparison operators
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "^"
  "**"
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "^="
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "&&"
  "||"
  "!"
  "~"
  "!~"
  "++"
  "--"
  "?"
  ":"
] @operator.word

;; Delimiters
[
  ","
  ";"
] @delimiter
