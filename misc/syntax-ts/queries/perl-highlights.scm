;; Tree-sitter highlight queries for Perl language
;; Colors aligned with MC's default perl.syntax

;; sub -> yellow
"sub" @keyword

;; Control flow keywords -> magenta
[
  "my"
  "our"
  "local"
  "use"
  "require"
  "package"
  "return"
  "if"
  "elsif"
  "else"
  "unless"
  "while"
  "until"
  "for"
  "foreach"
  "do"
  "last"
  "next"
  "redo"
  "and"
  "or"
  "goto"
  "BEGIN"
  "END"
] @keyword.directive

;; __END__ / __DATA__ -> brown
(data_section) @comment

;; Builtin functions -> yellow
[
  "chomp"
  "chop"
  "defined"
  "undef"
  "eval"
] @keyword

;; Operators -> yellow
[
  "="
  "=="
  "!="
  "eq"
  "ne"
  "lt"
  "gt"
  "le"
  "ge"
  "<=>"
  "cmp"
  "<"
  ">"
  "<="
  ">="
  "=~"
  "!~"
  "+"
  "-"
  "*"
  "/"
  "%"
  "**"
  "."
  ".."
  "&&"
  "||"
  "!"
  "->"
] @operator.word

;; Semicolons -> brightmagenta
";" @delimiter.special

;; Comma -> brightcyan
"," @delimiter

(string_literal) @string
(interpolated_string_literal) @string
(heredoc_content) @string
(heredoc_token) @string
(command_string) @string

(match_regexp) @string.special
(quoted_regexp) @string.special

(comment) @comment

;; Variables -> brightgreen
(container_variable) @variable.special
(scalar) @variable.special
(array) @variable.special
(hash) @variable.special

;; Brackets/parens -> brightcyan
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

(label) @label
