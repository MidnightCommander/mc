;; Tree-sitter highlight queries for OCaml language
;; Colors aligned with MC's default ml.syntax
;; MC: keywords=yellow, operators/delimiters=cyan, strings=brightcyan, comments=brown

;; Keywords -> yellow
[
  "and"
  "as"
  "assert"
  "begin"
  "class"
  "constraint"
  "do"
  "done"
  "downto"
  "else"
  "end"
  "exception"
  "external"
  "for"
  "fun"
  "function"
  "functor"
  "if"
  "in"
  "include"
  "inherit"
  "initializer"
  "lazy"
  "let"
  "match"
  "method"
  "module"
  "mutable"
  "new"
  "nonrec"
  "object"
  "of"
  "open"
  "private"
  "rec"
  "sig"
  "struct"
  "then"
  "to"
  "try"
  "type"
  "val"
  "virtual"
  "when"
  "while"
  "with"
] @keyword

;; Boolean constants -> yellow (MC: true/false are keywords=yellow)
[
  "true"
  "false"
] @keyword

;; Operators -> cyan (MC uses cyan for operators)
[
  "="
  "<"
  ">"
  "|"
  "->"
  "::"
  ";;"
  "~"
  "!"
  ":="
  "+"
  "-"
  "*"
] @label

;; Delimiters -> cyan (MC uses cyan for delimiters)
[
  "."
  ","
  ":"
] @label

;; Brackets/parens/braces -> brightcyan (MC legacy colors these as cyan)
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Semicolons -> brightred (MC: ;; and ; are brightred)
";" @function.special

;; Strings -> brightcyan (MC uses brightcyan for strings in "")
(string) @tag

;; Characters -> brightcyan
(character) @tag

;; Booleans (named node)
(boolean) @keyword

;; Comments -> brown
(comment) @comment

;; Types -> yellow
(type_constructor) @type
(constructor_name) @type
(module_name) @type

;; Labels -> cyan
(label_name) @label

;; Fields -> cyan
(field_name) @label
