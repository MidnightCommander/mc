;; Tree-sitter highlight queries for Erlang
;; Colors aligned with MC's erlang.syntax

;; Control flow keywords -> yellow (@keyword)
;; MC uses yellow for after, begin, case, catch, cond, end, fun, if, etc.
[
  "fun"
  "end"
  "if"
  "case"
  "of"
  "receive"
  "after"
  "when"
  "begin"
  "try"
  "catch"
  "throw"
] @keyword

;; Module directives -> brightmagenta (@delimiter.special)
;; MC uses brightmagenta for -module, -export, -compile, etc.
(module_attribute) @delimiter.special
(module_export) @delimiter.special

;; Word operators -> brown (@comment) -- MC uses brown for and, or, band, etc.
;; This is a deliberate choice: MC colors these same as comments (brown)
[
  "and"
  "andalso"
  "band"
  "bnot"
  "bor"
  "bsl"
  "bsr"
  "bxor"
  "div"
  "not"
  "or"
  "orelse"
  "rem"
  "xor"
] @comment

(comment) @comment

(string) @string
(char) @string.special

;; Atoms -> lightgray (@constant)
(atom) @constant

;; Variables -> white (@keyword.other) -- MC uses white for Variables
(variable) @keyword.other

(function_clause
  name: (atom) @function)
(expr_function_call
  (atom) @function)

;; Operators -> yellow (@keyword) -- MC uses yellow for -> <-
[
  "->"
  "=>"
  ":="
  "="
  "!"
  "|"
  "::"
  "<-"
  "+"
  "-"
  "*"
  "++"
  "--"
  "<<"
  ">>"
] @keyword

;; Comparison operators -> brown (@comment) -- MC uses brown
[
  "=="
  "/="
  "=:="
  "=/="
  "<"
  ">"
  "=<"
  ">="
] @comment

;; Delimiters -> brightcyan (@delimiter) -- MC uses brightcyan
[
  ","
  "."
  ";"
  "("
  ")"
  "["
  "]"
] @delimiter

;; Curly braces -> cyan (@label) -- MC uses cyan for { }
[
  "{"
  "}"
] @label

;; Pipe operators -> brightcyan (@delimiter) -- MC uses brightcyan
[
  "||"
] @delimiter
