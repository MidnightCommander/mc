;; Tree-sitter highlight queries for Haskell
;; Colors aligned with MC's default haskell.syntax

;; Keywords -> yellow
[
  "module"
  "where"
  "import"
  "qualified"
  "as"
  "hiding"
  "data"
  "newtype"
  "type"
  "class"
  "instance"
  "deriving"
  "do"
  "let"
  "in"
  "case"
  "of"
  "if"
  "then"
  "else"
  "infixl"
  "infixr"
  "infix"
  "forall"
] @keyword

;; Comments -> brown
(comment) @comment
(haddock) @comment

;; Pragmas -> green (MC: {-# ... #-} context is green)
(pragma) @comment.special

;; Strings -> green
(string) @string

;; Chars -> brightgreen
(char) @string.special

;; Numbers -> brightgreen (MC uses brightgreen for digits)
(integer) @number.builtin
(float) @number.builtin

;; Constructors / Type names -> white (MC uses white for uppercase identifiers)
(constructor) @keyword.other

;; Function signatures
(signature
  name: (variable) @function)

;; Named operators -> white (MC uses white for &&, ||, |, ^, ~, and infix ops)
(operator) @keyword.other

;; Symbol operators -> yellow for $, *, +, /, <, >, -, =; white for |, \, @, ~
[
  "="
  "->"
  "<-"
] @operator.word

[
  "::"
  "=>"
  "|"
  "\\"
  "@"
  "~"
] @keyword.other

;; Delimiters -> brightcyan
[
  ","
  ";"
] @delimiter
