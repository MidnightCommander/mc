;; Tree-sitter highlight queries for Ada language
;; Colors aligned with MC's ada95.syntax

;; General keywords -> yellow (@keyword)
[
  "abort"
  "abs"
  "and"
  "delay"
  "delta"
  "digits"
  "do"
  "in"
  "is"
  "mod"
  "not"
  "null"
  "of"
  "or"
  "others"
  "out"
  "pragma"
  "raise"
  "range"
  "renames"
  "return"
  "reverse"
  "separate"
  "task"
  "use"
  "with"
  "xor"
] @keyword

;; Control flow -> brightred (@function.special)
[
  "begin"
  "case"
  "declare"
  "else"
  "elsif"
  "end"
  "entry"
  "exception"
  "exit"
  "for"
  "if"
  "loop"
  "private"
  "protected"
  "select"
  "then"
  "until"
  "when"
  "while"
] @function.special

;; Type-like keywords -> cyan (@label)
[
  "array"
  "record"
  "some"
  "subtype"
] @label

;; Declaration keywords -> brightcyan (@property)
[
  "abstract"
  "accept"
  "access"
  "all"
  "at"
  "constant"
  "goto"
  "interface"
  "limited"
  "overriding"
  "tagged"
  "type"
] @property

;; Definition keywords -> brightmagenta (@constant.builtin)
[
  "body"
  "function"
  "generic"
  "new"
  "package"
  "procedure"
] @constant.builtin

(comment) @comment
(string_literal) @string
(character_literal) @string.special

;; Operators -> brightgreen (@string.special) to match MC
[
  "+"
  "-"
  "*"
  "/"
  "**"
  "&"
  "="
  "/="
  "<"
  ">"
  "<="
  ">="
  ":="
  "=>"
  ".."
  "<>"
] @string.special

;; Delimiters -> brightcyan (@delimiter)
[
  "."
  ","
  ":"
  ";"
  "("
  ")"
] @delimiter

;; Predefined type names -> yellow (keyword) to match legacy
((identifier) @keyword
 (#any-of? @keyword
  "Boolean" "Integer" "Natural" "Positive" "Float"
  "Character" "String" "Duration"
  "Wide_Character" "Wide_String"
  "Wide_Wide_Character" "Wide_Wide_String"))

(label (identifier) @label)
