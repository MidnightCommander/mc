;; Tree-sitter highlight queries for SQL
;; Colors aligned with MC's default sql.syntax
;; MC: keywords=yellow, comments=brown, strings=green, operators=brightcyan

;; Types -> yellow (same as keywords in MC)
(type) @type

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string) @string

;; Operators -> brightcyan
[
  "="
  "!="
  "<>"
  "<"
  ">"
  "<="
  ">="
  "+"
  "-"
  "*"
  "/"
  "%"
  "^"
  "&"
  "|"
  "~"
  "<<"
  ">>"
  "&&"
  "||"
] @operator

;; Delimiters -> brightcyan
[
  ";"
  ","
  "("
  ")"
] @delimiter
