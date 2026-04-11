;; Tree-sitter highlight queries for Fortran
;; Colors aligned with MC's fortran.syntax

;; Type declarations -> brightcyan (@property) -- MC uses brightcyan for types
[
  "integer"
  "real"
  "character"
  "logical"
  "complex"
  "double"
] @property

;; Program structure keywords -> yellow (@keyword) -- MC uses yellow
[
  "program"
  "end"
  "subroutine"
  "function"
  "module"
  "use"
  "implicit"
  "none"
  "only"
  "result"
] @keyword

;; Declaration keywords -> brightcyan (@property) -- MC uses brightcyan
[
  "dimension"
  "allocatable"
  "intent"
  "in"
  "out"
  "inout"
  "parameter"
  "data"
  "block"
  "common"
  "external"
  "format"
  "type"
  "class"
  "interface"
  "contains"
  "public"
  "private"
  "abstract"
  "extends"
  "procedure"
  "intrinsic"
  "save"
  "target"
  "pointer"
  "optional"
  "value"
  "volatile"
  "sequence"
  "generic"
] @property

;; Control flow -> brightgreen (@number.builtin) -- MC uses brightgreen
[
  "do"
  "if"
  "then"
  "else"
  "elseif"
  "endif"
  "enddo"
  "call"
  "return"
  "stop"
  "continue"
  "cycle"
  "exit"
  "goto"
  "while"
  "where"
  "forall"
  "select"
  "case"
  "default"
  "associate"
  "critical"
  "concurrent"
  "assign"
  "entry"
  "pause"
  "allocate"
] @number.builtin

;; I/O keywords -> brightmagenta (@constant.builtin) -- MC uses brightmagenta
[
  "read"
  "write"
  "print"
  "open"
  "close"
  "inquire"
  "rewind"
  "backspace"
  "endfile"
] @constant.builtin

(comment) @comment

(string_literal) @string

;; Boolean literals -> brightred (@function.special) -- MC uses brightred
(boolean_literal) @function.special

;; Operators -> yellow (@operator.word) -- MC uses yellow for arithmetic/comparison
[
  "="
  "+"
  "-"
  "*"
  "/"
  "**"
  "=="
  "/="
  "<"
  ">"
  "<="
  ">="
] @operator.word

;; Logical/relational operators -> brightred (@function.special) -- MC uses brightred
(logical_expression
  operator: _ @function.special)
(relational_expression
  operator: [
    ".eq."
    ".ne."
    ".lt."
    ".gt."
    ".le."
    ".ge."
  ] @function.special)
(unary_expression
  operator: ".not." @function.special)

;; Delimiters
[
  ","
  "::"
  "("
  ")"
] @delimiter
