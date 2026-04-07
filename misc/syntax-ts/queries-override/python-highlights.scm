;; Tree-sitter highlight queries for Python language
;; Colors aligned with MC's default python.syntax

;; Keywords -> yellow (matching MC: keyword whole ... yellow)
[
  "and"
  "as"
  "assert"
  "async"
  "await"
  "break"
  "class"
  "continue"
  "def"
  "del"
  "elif"
  "else"
  "except"
  "finally"
  "for"
  "from"
  "global"
  "if"
  "import"
  "in"
  "is"
  "lambda"
  "nonlocal"
  "not"
  "or"
  "pass"
  "raise"
  "return"
  "try"
  "while"
  "with"
  "yield"
] @keyword

;; Operators -> yellow (matching MC: keyword +,-,*,/,%,=,!=,==,<,> yellow)
[
  "="
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "+"
  "-"
  "*"
  "**"
  "/"
  "//"
  "%"
  "@"
  "|"
  "&"
  "^"
  "~"
  "<<"
  ">>"
  "+="
  "-="
  "*="
  "/="
  "//="
  "%="
  "**="
  "<<="
  ">>="
  "&="
  "|="
  "^="
  "->"
  ":="
] @operator.word

;; Colon -> brightred (matching MC: keyword : brightred)
":" @variable.builtin

;; Brackets/parens -> brightcyan (matching MC: keyword {,},(,),[,] brightcyan)
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Comma -> brightcyan (matching MC: keyword , brightcyan)
"," @delimiter

;; Semicolons -> brightmagenta (matching MC: keyword ; brightmagenta)
";" @delimiter.special

;; Strings -> green (matching MC: context " " green, etc.)
(string) @string
(concatenated_string) @string

;; Escape sequences in strings -> brightgreen (matching MC: keyword \\" brightgreen)
(escape_sequence) @string.special

;; Comments -> brown (matching MC: context # \n brown)
(comment) @comment

;; self -> brightred (matching MC: keyword whole self brightred)
((identifier) @variable.builtin
 (#match? @variable.builtin "^self$"))

;; Dunder methods -> lightgray (matching MC: keyword whole __init__ etc.)
((identifier) @constant
 (#match? @constant "^__.*__$"))

;; Decorators -> brightred (matching MC: keyword whole __+__ brightred)
(decorator
  (identifier) @function.special)

(decorator
  (attribute
    attribute: (identifier) @function.special))
