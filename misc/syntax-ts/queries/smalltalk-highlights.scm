;; Tree-sitter highlight queries for Smalltalk
;; Colors aligned with MC's smalltalk.syntax
;; MC: class/meta keywords=yellow, message keywords=brightmagenta, comments=brown, strings=brightcyan

;; Special variables -> yellow (@keyword)
(self) @keyword
(super) @keyword
(nil) @keyword
(thisContext) @keyword

;; Boolean literals -> brightmagenta (@keyword.control)
(true) @keyword.control
(false) @keyword.control

;; Keywords in keyword messages -> brightmagenta (@keyword.control)
(keyword) @keyword.control

;; Unary selectors -> yellow (@keyword)
(unary_selector) @keyword

;; Binary operators -> cyan (@label)
(binary_operator) @label
(binary_selector) @label

;; Return -> brightred (@function.special)
"^" @function.special

;; Statement separators -> brightred (@function.special)
"." @function.special

;; Temporaries delimiters -> brightred (@function.special)
"|" @function.special

;; Assignment -> cyan (@label)
":=" @label

;; Comments -> brown
(comment) @comment

;; Strings -> brightcyan (@tag)
(string) @tag

;; Symbols -> yellow (@keyword)
(symbol) @keyword

;; Characters -> brightcyan (@tag)
(character) @tag

;; Block arguments
(block_argument) @constant

;; Cascade separator
";" @delimiter

;; Brackets
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter
