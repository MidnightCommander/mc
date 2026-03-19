;; Tree-sitter highlight queries for Makefile language
;; Colors aligned with MC's default makefile.syntax

;; Directives -> magenta (keyword.directive)
[
  "include"
  "sinclude"
  "-include"
  "define"
  "endef"
  "ifdef"
  "ifndef"
  "ifeq"
  "ifneq"
  "else"
  "endif"
  "override"
  "export"
  "unexport"
  "undefine"
  "private"
  "vpath"
] @keyword.directive

;; Comments -> brown (comment)
(comment) @comment

;; Variable assignment name -> yellow (property.key)
(variable_assignment
  name: (word) @property.key)

;; Variable references $() -> keyword (yellow)
(variable_reference
  (word) @keyword)

;; Shell text in recipes -> default (variable)
(shell_text) @variable

;; Strings -> green (string)
(string) @string

;; Assignment operators = := etc -> white (tag.special)
[
  "="
  ":="
  "::="
  "?="
  "+="
  "!="
] @tag.special

;; : -> yellow (keyword)
[
  ":"
  "::"
] @keyword

[
  ";"
  ","
] @delimiter
