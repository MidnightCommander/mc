;; Tree-sitter highlight queries for QML (Qt Modeling Language)
;; Based on JavaScript/TypeScript syntax with QML extensions.

;; QML keywords -> yellow
[
  "import"
  "pragma"
  "component"
  "property"
  "signal"
  "required"
  "readonly"
  "default"
  "on"
] @keyword

;; JS/TS keywords -> yellow
[
  "if"
  "else"
  "for"
  "while"
  "switch"
  "case"
  "break"
  "continue"
  "return"
  "function"
  "var"
  "let"
  "const"
  "class"
  "enum"
  "new"
  "delete"
  "typeof"
  "instanceof"
  "in"
  "of"
  "try"
  "catch"
  "finally"
  "throw"
  "async"
  "await"
  "yield"
  "export"
  "void"
  "with"
] @keyword

;; Named keyword nodes -> yellow
(this) @keyword
(super) @keyword

;; Boolean and null literals -> brightgreen
[
  (true)
  (false)
  (null)
  (undefined)
] @number.builtin

;; Number literals -> brightgreen
(number) @number.builtin

;; Strings -> green
(string) @string
(template_string) @string

;; Regex -> brightgreen
(regex) @string.special

;; Comments -> brown
(comment) @comment

;; Type annotations -> yellow
(type_identifier) @type

;; Operators -> yellow
[
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "+"
  "-"
  "*"
  "/"
  "%"
  "=="
  "==="
  "!="
  "!=="
  "<"
  ">"
  "<="
  ">="
  "&&"
  "||"
  "!"
  "?"
  ":"
  "."
] @operator.word

;; Arrow -> brightcyan
"=>" @operator

;; Semicolons -> brightmagenta
";" @delimiter.special

;; Delimiters -> brightcyan
[
  ","
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter
