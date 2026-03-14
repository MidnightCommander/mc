;; Tree-sitter highlight queries for Graphviz DOT
;; Colors aligned with MC's dot.syntax
;; MC: graph keywords=brightred, node/edge=yellow, attributes=white, comments=brown, strings=green

;; Graph keywords -> brightred (@function.special)
[
  "strict"
  "graph"
  "digraph"
] @function.special

;; Subgraph keyword -> brightred (@function.special)
"subgraph" @function.special

;; Node/edge keywords -> yellow (@keyword)
[
  "node"
  "edge"
] @keyword

;; Edge operators -> brightred (@function.special)
[
  "->"
  "--"
] @function.special

;; Attribute names -> white (@keyword.other)
(attribute
  name: (id) @keyword.other)

;; Operators (=, :, +) -> yellow (@operator.word)
(operator) @operator.word

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string_literal) @string

;; HTML strings -> green
(html_string) @string

;; Preprocessor -> brightred (@function.special)
(preproc) @function.special

;; Brackets -> brightcyan (@delimiter)
[
  "{"
  "}"
  "["
  "]"
] @delimiter

;; Punctuation -> brightcyan (@delimiter)
"," @delimiter

;; Semicolons -> brightmagenta (@delimiter.special)
";" @delimiter.special
