;; Tree-sitter highlight queries for R language
;; Colors aligned with MC's default r.syntax

;; Control flow keywords -> brightmagenta (keyword.control)
[
  "if"
  "for"
  "while"
  "repeat"
  "in"
] @keyword.control

;; return/next/break -> brightmagenta (keyword.control)
(return) @keyword.control
(next) @keyword.control
(break) @keyword.control

;; function keyword -> yellow (keyword)
"function" @keyword

;; Boolean/null/special constants
(true) @constant
(false) @constant
(null) @constant
(inf) @constant
(nan) @constant
(na) @constant

;; Assignment operators -> brightred (function.special)
[
  "<-"
  "<<-"
  "->"
  "->>"
] @function.special

;; Equals sign (assignment) -> brightred (function.special)
"=" @function.special

;; Comparison and arithmetic operators -> yellow (operator.word)
[
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "+"
  "-"
  "*"
  "/"
  "^"
  "|"
  "||"
  "&"
  "&&"
  "!"
  "~"
  "|>"
  "$"
  "@"
  ":"
  "::"
  ":::"
  "**"
] @operator.word

(comma) @delimiter

;; Strings -> brightgreen (string.special) to match MC
(string) @string.special

(comment) @comment

;; Function calls -> yellow (keyword) to match MC
(call
  function: (identifier) @keyword)

(call
  function: (namespace_operator
    rhs: (identifier) @keyword))

(namespace_operator
  lhs: (identifier) @type)

(parameter
  name: (identifier) @property)

(extract_operator
  (identifier) @property .)