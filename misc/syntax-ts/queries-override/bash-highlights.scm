;; Tree-sitter highlight queries for Bash/Shell
;; Colors matched to MC's built-in sh.syntax

;; -----------------------------------------------------------
;; 1. Language keywords that are grammar tokens -> yellow
;;    sh.syntax: break case continue declare do done echo elif
;;    else esac exit export fi for getopts if in printf read
;;    return select set shift source then trap umask unset
;;    until wait while clear
;; -----------------------------------------------------------

;; Keywords that are anonymous grammar tokens
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

;; "function" keyword -> brightmagenta
"function" @keyword.function

;; -----------------------------------------------------------
;; 2. Function definitions -> brightmagenta
;;    sh.syntax: function...() and name() at line start
;; -----------------------------------------------------------

;; function foo() { ... }
(function_definition name: (word) @function.definition)
(function_definition "(" @function.definition)
(function_definition ")" @function.definition)

;; -----------------------------------------------------------
;; 3. Commands -> cyan (external) or yellow (builtins)
;;    The builtin list matches sh.syntax yellow keywords.
;;    Security commands (gpg, ssh, etc.) -> red.
;;    Captures are on command_name (same node width) so
;;    pattern order determines priority: builtins first.
;; -----------------------------------------------------------

;; Shell builtins -> yellow (must be before generic @function)
((command_name) @keyword
  (#any-of? @keyword
    "echo" "printf" "read" "set" "eval" "exec" "exit"
    "return" "source" "trap" "wait" "getopts" "umask"
    "shift" "break" "continue" "clear"))

;; Security commands -> red
((command_name) @function.security
  (#any-of? @function.security
    "gpg" "md5sum" "openssl" "ssh" "scp"))

;; All other commands -> cyan
(command_name) @function

;; -----------------------------------------------------------
;; 5. Comments -> brown
;;    sh.syntax: context # \n brown
;; -----------------------------------------------------------

;; Shebang -> brightcyan (distinct from plain comments)
;;    sh.syntax: context exclusive #! \n brightcyan/black
((comment) @comment.shebang
  (#match? @comment.shebang "^#!"))

(comment) @comment

;; -----------------------------------------------------------
;; 6. Strings -> green
;;    sh.syntax: context " " green / context ' ' green
;; -----------------------------------------------------------

(string) @string
(raw_string) @string
(ansi_c_string) @string
(heredoc_body) @string
(heredoc_start) @string
(heredoc_end) @string

;; -----------------------------------------------------------
;; 7. Variable expansions
;; -----------------------------------------------------------

;; $VAR -> brightgreen
;;    sh.syntax: keyword wholeright $+ brightgreen / keyword $ brightgreen
(simple_expansion) @variable.special

;; Special variables $?, $#, $@, $*, $-, $$, $!, $_ -> brightred
;;    sh.syntax: keyword $? brightred etc.
;; Capture both the $ and the special name inside simple_expansion
(simple_expansion "$" @variable.builtin (special_variable_name) @variable.builtin)

;; Positional parameters $0-$9 -> brightred
;;    sh.syntax: keyword wholeright $[0123456789] brightred
(simple_expansion "$" @variable.builtin
  ((variable_name) @variable.positional
    (#match? @variable.positional "^[0-9]$")))

;; ${VAR} -> brightgreen
;;    sh.syntax: keyword ${*} brightgreen
(expansion) @variable.special

;; $() command substitution delimiters -> brightgreen
;;    sh.syntax: keyword $(*) brightgreen
;; Only color the $( and ) delimiters, not the whole content,
;; so commands inside get their normal colors (same as standalone).
"$(" @variable.special

;; Backticks ` -> brightred
;;    sh.syntax: keyword ` brightred
"`" @punctuation.backtick

;; Variable names in assignments (left side of =) -> default (white)
;;    sh.syntax does not color assignment targets
(variable_assignment
  name: (variable_name) @variable)

;; -----------------------------------------------------------
;; 8. Punctuation and operators
;; -----------------------------------------------------------

;; ;; -> brightred
;;    sh.syntax: keyword ;; brightred
";;" @punctuation.special

;; ; -> brightcyan
;;    sh.syntax: keyword ; brightcyan
";" @delimiter

;; { } in compound statements -> brightcyan
;;    sh.syntax: keyword { brightcyan / keyword } brightcyan
;; NOTE: not capturing bare "{" "}" globally to avoid overriding
;; the green color inside ${VAR} expansions (narrower wins)
(compound_statement "{" @delimiter)
(compound_statement "}" @delimiter)

;; ( ) -> brightcyan
[
  "("
  ")"
] @delimiter

;; Heredoc operators -> brightcyan
"<<" @operator
"<<-" @operator

;; File descriptors (2>&1) -> brightred
;;    sh.syntax: keyword whole 2>&1 brightred etc.
(file_descriptor) @variable.builtin

;; Redirections -> brightcyan
[
  ">"
  ">>"
  "<"
  ">&"
  "&>"
] @operator

;; Logical/pipe operators -> brightcyan
[
  "&&"
  "||"
  "|"
  "&"
] @operator

;; -----------------------------------------------------------
;; 9. Miscellaneous
;; -----------------------------------------------------------

;; Regex patterns -> brightmagenta
(regex) @string.special

;; Test operators (-f, -d, -z, -eq, -ne, -lt, -gt, etc.)
(test_operator) @operator
