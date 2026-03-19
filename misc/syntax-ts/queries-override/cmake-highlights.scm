;; Tree-sitter highlight queries for CMake language
;; Colors aligned with MC's default cmake.syntax

;; Control flow keywords -> brightred (function.special, MC uses brightred for all commands)
(if) @function.special
(else) @function.special
(elseif) @function.special
(endif) @function.special
(foreach) @function.special
(endforeach) @function.special
(while) @function.special
(endwhile) @function.special
(function) @function.special
(endfunction) @function.special
(macro) @function.special
(endmacro) @function.special
(block) @function.special
(endblock) @function.special

;; Commands -> brightred (function.special)
(normal_command
  (identifier) @function.special)

;; Comments -> brown (comment)
(line_comment) @comment
(bracket_comment) @comment

;; Strings -> green (string)
(quoted_argument) @string

;; Unquoted arguments -> default (variable)
(unquoted_argument) @variable

;; All-caps arguments (properties/constants) -> white (tag.special)
((unquoted_argument) @tag.special
 (#match? @tag.special "^[A-Z][A-Z\\d_]*$"))

;; Variable references ${...} -> brightgreen (variable.special)
(variable_ref) @variable.special

;; Parentheses -> brightcyan (delimiter)
[
  "("
  ")"
] @delimiter
