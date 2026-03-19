;; Tree-sitter highlight queries for MATLAB/Octave
;; Colors aligned with MC's octave.syntax
;; MC: control keywords=white, functions=yellow, operators=brightcyan, comments=brown, strings=green

;; Control flow keywords -> white (@keyword.other)
[
  "if"
  "else"
  "elseif"
  "end"
  "for"
  "parfor"
  "while"
  "switch"
  "case"
  "otherwise"
  "try"
  "catch"
  "return"
  "continue"
  "break"
  "function"
  "endfunction"
  "classdef"
  "properties"
  "methods"
  "events"
  "enumeration"
  "spmd"
  "global"
  "persistent"
  "arguments"
] @keyword.other

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string) @string

;; Escape sequences -> brightgreen
(escape_sequence) @string.special

;; Operators -> brightcyan (@operator)
(binary_operator) @operator
(comparison_operator) @operator
(boolean_operator) @operator
(unary_operator) @operator
(not_operator) @operator
(postfix_operator) @operator

;; Assignment
"=" @operator

;; Delimiters -> brightcyan (@delimiter)
[
  ","
  ";"
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter
