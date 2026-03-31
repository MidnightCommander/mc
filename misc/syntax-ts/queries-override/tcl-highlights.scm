;; Tree-sitter highlight queries for Tcl
;; Colors aligned with MC's default tcl.syntax
;; MC: keywords=yellow, comments=brown, strings=green, brackets=brightcyan, ;=brightmagenta

;; Keywords that are anonymous string literals in the grammar
[
  "if"
  "else"
  "elseif"
  "while"
  "foreach"
  "proc"
  "set"
  "regexp"
  "try"
  "catch"
  "finally"
  "error"
  "on"
  "namespace"
  "global"
  "expr"
] @keyword

;; Named keyword nodes
(while) @keyword
(foreach) @keyword
(global) @keyword
(namespace) @keyword
(try) @keyword
(expr_cmd) @keyword
(regexp) @keyword

;; Tcl built-in command names matched by regex
((simple_word) @keyword
 (#match? @keyword "^(return|break|continue|puts|gets|open|close|read|eval|exec|source|package|require|variable|upvar|uplevel|array|list|lindex|lappend|llength|lrange|lsearch|lsort|lreplace|string|regsub|incr|append|format|scan|info|rename|trace|after|vwait|update|interp|switch)$"))

;; Comments -> brown
(comment) @comment

;; Quoted strings -> green
(quoted_word) @string

;; Braced words -> green
(braced_word) @string

;; Variables -> brightgreen (MC uses brightgreen for $vars)
(variable_substitution) @string.special

;; Escape sequences -> brightgreen
(escaped_character) @string.special

;; Brackets -> brightcyan (MC legacy colors these as brightcyan)
[
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Semicolons -> brightmagenta (MC legacy colors ; as brightmagenta)
";" @delimiter.special
