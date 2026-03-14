;; Tree-sitter highlight queries for JavaScript
;; Colors aligned with MC's default js.syntax

;; Keywords -> yellow
[
  "async"
  "await"
  "break"
  "case"
  "catch"
  "class"
  "const"
  "continue"
  "debugger"
  "default"
  "delete"
  "do"
  "else"
  "export"
  "extends"
  "finally"
  "for"
  "function"
  "if"
  "import"
  "in"
  "instanceof"
  "let"
  "new"
  "of"
  "return"
  "static"
  "switch"
  "throw"
  "try"
  "typeof"
  "var"
  "void"
  "while"
  "with"
  "yield"
  "from"
  "as"
  "get"
  "set"
] @keyword

(this) @keyword
(super) @keyword

;; undefined -> yellow (keyword)
(undefined) @keyword

(comment) @comment

(string) @string
(template_string) @string
(regex) @string.special

;; Numbers -> brightgreen in MC js.syntax
(number) @number.builtin

;; true/false/null -> brightgreen in MC js.syntax
[
  (true)
  (false)
  (null)
] @number.builtin

;; Operators -> yellow
[
  "!"
  "!="
  "!=="
  "%"
  "&"
  "&&"
  "*"
  "**"
  "+"
  "++"
  "+="
  "-"
  "--"
  "-="
  "."
  "/"
  "<"
  "<<"
  "<="
  "="
  "=="
  "==="
  ">"
  ">="
  ">>"
  ">>>"
  "^"
  "|"
  "||"
  "~"
  "*="
  "/="
  "%="
  "**="
  "&&="
  "||="
] @operator.word

;; Arrow -> brightcyan
[
  "=>"
  "..."
] @operator

;; Semicolons -> brightmagenta
";" @delimiter.special

;; Brackets/parens -> brightcyan
[
  ","
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Colon -> brightcyan
":" @delimiter

(statement_identifier) @label
