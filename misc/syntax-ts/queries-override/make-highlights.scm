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
(shell_assignment
  name: (word) @property.key)

;; Variable references $() ${} -> keyword (yellow)
(variable_reference
  (word) @keyword)

;; Automatic variables ($@, $<, $^, $*, $?, $%) -> brightred (function.special)
(automatic_variable) @function.special

;; Escaped dollar $$ -> brightcyan (delimiter)
(escape) @delimiter

;; Line continuation \ -> yellow (keyword)
"\\" @keyword

;; Special targets -> white (tag.special)
(rule
  (targets
    (word) @tag.special
    (#any-of? @tag.special
      ".PHONY" ".SUFFIXES" ".DEFAULT" ".PRECIOUS"
      ".INTERMEDIATE" ".SECONDARY" ".DELETE_ON_ERROR"
      ".IGNORE" ".LOW_RESOLUTION_TIME" ".SILENT"
      ".EXPORT_ALL_VARIABLES" ".NOTPARALLEL" ".NOEXPORT")))

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

;; Semicolons, commas -> brightcyan (delimiter)
[
  ";"
  ","
] @delimiter

;; Function calls -> brightcyan (label)
(function_call
  function: (_) @label)
