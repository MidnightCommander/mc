;; Tree-sitter highlight queries for RDF Turtle
;; Colors aligned with MC's turtle.syntax
;; MC: declarations=magenta, keyword 'a'=yellow, comments=brown, strings=green, URIs=brightred

;; Directives -> magenta (@keyword.directive)
[
  "@prefix"
  "@base"
  "BASE"
  "PREFIX"
] @keyword.directive

;; Keyword 'a' (rdf:type) -> yellow (@keyword)
"a" @keyword

;; Boolean literals -> yellow (@keyword)
[
  "true"
  "false"
] @keyword

;; IRI references -> brightred (@function.special)
(iri_reference) @function.special

;; Prefixed names -> cyan (@label)
(prefixed_name) @label
(namespace) @label

;; Blank nodes -> cyan (@label)
(blank_node_label) @label

;; Type annotation -> brightmagenta (@keyword.control)
"^^" @keyword.control

;; Language tags -> brightmagenta (@keyword.control)
(lang_tag) @keyword.control

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string) @string

;; Escape sequences -> brightgreen
(echar) @string.special

;; Punctuation -> white (@keyword.other)
[
  "."
  ","
  ";"
] @keyword.other

;; Collection parens -> brightmagenta (@keyword.control)
[
  "("
  ")"
] @keyword.control

;; Brackets -> cyan (@label)
[
  "["
  "]"
] @label
