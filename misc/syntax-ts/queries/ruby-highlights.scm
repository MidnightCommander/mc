;; Tree-sitter highlight queries for Ruby language
;; Colors aligned with MC's default ruby.syntax (keywords=magenta, operators=yellow)

;; Keywords -> magenta
[
  "BEGIN"
  "END"
  "alias"
  "and"
  "begin"
  "break"
  "case"
  "class"
  "def"
  "do"
  "else"
  "elsif"
  "end"
  "ensure"
  "for"
  "if"
  "in"
  "module"
  "next"
  "not"
  "or"
  "redo"
  "rescue"
  "retry"
  "return"
  "then"
  "undef"
  "unless"
  "until"
  "when"
  "while"
  "yield"
] @keyword.directive

;; self/super -> magenta
(self) @keyword.directive
(super) @keyword.directive

;; nil/true/false -> brightred
[
  "nil"
  (true)
  (false)
] @function.special

;; Built-in class methods -> magenta (same as keywords)
((identifier) @keyword.directive
 (#match? @keyword.directive "^(require|require_relative|include|extend|attr_reader|attr_writer|attr_accessor|public|private|protected|raise)$"))

;; Operators -> yellow
[
  "="
  "=="
  "==="
  "!="
  "<=>"
  "<"
  ">"
  "<="
  ">="
  "+"
  "-"
  "*"
  "/"
  "%"
  "**"
  "&&"
  "||"
  "!"
  "&"
  "|"
  "^"
  "~"
  "<<"
  ">>"
  ".."
  "..."
  "=>"
  "+="
  "-="
  "*="
  "/="
  "||="
  "&&="
  "=~"
  "!~"
] @operator.word

;; Brackets/parens -> brightcyan
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

;; Other delimiters -> brightcyan
[
  ","
  ":"
  "::"
] @delimiter

(string) @string
(bare_string) @string
(string_array) @string
(heredoc_body) @string
(heredoc_beginning) @string

(escape_sequence) @string.special

(regex) @string.special

;; Symbols -> white
(simple_symbol) @keyword.other
(hash_key_symbol) @keyword.other
(bare_symbol) @keyword.other

;; Global variables -> brightgreen
(global_variable) @variable.special

;; Instance variables -> white
(instance_variable) @keyword.other

(comment) @comment
