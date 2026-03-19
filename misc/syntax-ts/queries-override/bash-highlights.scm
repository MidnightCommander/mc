;; Tree-sitter highlight queries for Bash/Shell language
;; Colors aligned with MC's default sh.syntax

;; Keywords -> yellow
[
  "if"
  "then"
  "else"
  "elif"
  "fi"
  "case"
  "esac"
  "for"
  "while"
  "until"
  "do"
  "done"
  "in"
  "select"
  "declare"
  "local"
  "export"
  "readonly"
  "typeset"
  "unset"
] @keyword

"function" @keyword.control

(comment) @comment
(string) @string
(raw_string) @string
(heredoc_body) @string
(heredoc_start) @string

;; $VAR, ${VAR} -> brightgreen
(simple_expansion) @variable.special
(expansion) @variable.special

;; Special variables $?, $#, $@, etc -> brightred
(special_variable_name) @variable.builtin

;; ;; -> brightred
";;" @function.special

;; ; -> brightcyan
";" @delimiter

;; Braces -> brightcyan
[
  "{"
  "}"
] @delimiter

(regex) @string.special

;; Brackets/parens -> brightcyan
[
  "("
  ")"
  "["
  "]"
] @delimiter
