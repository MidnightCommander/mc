;; Tree-sitter highlight queries for PHP language
;; Colors aligned with MC's default php.syntax (keywords=brightmagenta)

;; Keywords -> brightmagenta
[
  "abstract"
  "and"
  "array"
  "as"
  "break"
  "case"
  "catch"
  "class"
  "clone"
  "const"
  "continue"
  "declare"
  "default"
  "do"
  "echo"
  "else"
  "elseif"
  "enddeclare"
  "endfor"
  "endforeach"
  "endif"
  "endswitch"
  "endwhile"
  "exit"
  "extends"
  "final"
  "finally"
  "fn"
  "for"
  "foreach"
  "function"
  "global"
  "goto"
  "if"
  "implements"
  "include"
  "include_once"
  "instanceof"
  "insteadof"
  "interface"
  "list"
  "match"
  "namespace"
  "new"
  "or"
  "print"
  "private"
  "protected"
  "public"
  "readonly"
  "require"
  "require_once"
  "return"
  "static"
  "switch"
  "throw"
  "trait"
  "try"
  "unset"
  "use"
  "while"
  "xor"
  "yield"
] @keyword.control

;; null/true/false -> brightmagenta
"null" @keyword.control
(boolean) @keyword.control

;; UPPERCASE constants -> white
((name) @keyword.other
 (#match? @keyword.other "^[A-Z][A-Z\\d_]+$"))

;; Operators -> white
[
  "="
  "=="
  "==="
  "!="
  "!=="
  "<>"
  "<"
  ">"
  "<="
  ">="
  "<=>"
  "+"
  "-"
  "*"
  "/"
  "%"
  "**"
  "."
  ".="
  "+="
  "-="
  "*="
  "/="
  "&&"
  "||"
  "!"
  "&"
  "|"
  "^"
  "~"
  "<<"
  ">>"
  "->"
  "=>"
  "??"
  "::"
] @keyword.other

;; Semicolons -> brightmagenta
";" @delimiter.special

;; Brackets/parens -> brightcyan
[
  ","
  ":"
] @delimiter

(string) @string
(encapsed_string) @string
(heredoc_body) @string
(heredoc) @string
(nowdoc_body) @string

(comment) @comment

;; $variables -> brightgreen
(variable_name) @variable.special

;; Functions -> yellow
(function_definition name: (name) @keyword)
(method_declaration name: (name) @keyword)

(function_call_expression
  function: (name) @keyword)

(function_call_expression
  function: (qualified_name) @keyword)

(member_call_expression
  name: (name) @keyword)

(scoped_call_expression
  name: (name) @keyword)

;; Brackets/parens -> brightcyan
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @delimiter

(named_label_statement (name) @label)
