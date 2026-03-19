;; Tree-sitter highlight queries for Lua language
;; Colors aligned with MC's default lua.syntax (keywords=white, operators=white)

;; Keywords -> white
[
  "and"
  "do"
  "else"
  "elseif"
  "end"
  "for"
  "function"
  "goto"
  "if"
  "in"
  "local"
  "not"
  "or"
  "repeat"
  "return"
  "then"
  "until"
  "while"
] @keyword.other

(break_statement) @keyword.other

;; Constants -> white
[
  (false)
  (nil)
  (true)
] @keyword.other

;; Operators -> white
[
  "="
  "~="
  "=="
  "<="
  ">="
  "<"
  ">"
  "+"
  "-"
  "*"
  "/"
  "//"
  "%"
  "^"
  "#"
  ".."
] @keyword.other

;; Brackets/parens/braces -> white
[
  "("
  ")"
  "{"
  "}"
  "["
  "]"
] @keyword.other

;; Delimiters -> white
[
  "."
  ","
  ";"
  ":"
  "::"
] @keyword.other

(string) @string

(comment) @comment

;; _VERSION, _G -> brightmagenta
((identifier) @constant.builtin
 (#match? @constant.builtin "^_(VERSION|G)$"))

;; Library functions -> yellow
(function_call
  name: (identifier) @keyword)

(function_call
  name: (dot_index_expression
    field: (identifier) @keyword))

(function_call
  name: (method_index_expression
    method: (identifier) @keyword))

(function_declaration
  name: (identifier) @keyword)

(label_statement (identifier) @label)

(goto_statement (identifier) @label)
