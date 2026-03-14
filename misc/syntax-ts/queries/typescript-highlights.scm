;; Tree-sitter highlight queries for TypeScript
;; Colors aligned with MC's default ts.syntax

;; Keywords -> yellow
[
  "as"
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
  "from"
  "function"
  "get"
  "if"
  "import"
  "in"
  "instanceof"
  "let"
  "new"
  "of"
  "return"
  "set"
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
  "type"
  "interface"
  "enum"
  "implements"
  "declare"
  "namespace"
  "abstract"
  "keyof"
  "readonly"
  "infer"
  "is"
  "asserts"
  "override"
  "satisfies"
] @keyword

(this) @keyword
(super) @keyword

;; true/false -> brightgreen
(true) @number.builtin
(false) @number.builtin

;; null/undefined -> cyan (basic types in MC ts.syntax)
(null) @label
(undefined) @label

;; Predefined types -> cyan (basic types in MC ts.syntax)
(predefined_type) @label

(comment) @comment

(string) @string
(template_string) @string
(regex) @string.special

;; Numbers -> brightgreen in MC ts.syntax
(number) @number.builtin

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
  "-"
  "--"
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
] @operator.word

;; Arrow -> brightcyan
"=>" @operator

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

;; Other delimiters -> brightcyan
[
  ":"
] @delimiter
