;; Tree-sitter highlight queries for Muttrc
;; Colors aligned with MC's muttrc.syntax
;; MC: primary commands=brightgreen, hooks=brightcyan, options=yellow, comments=brown

;; Set directive options -> yellow (@keyword)
(set_directive
  (option) @keyword)

;; Comments -> brown
(comment) @comment

;; Primary commands (set, color, macro, source, etc.) -> brightgreen (@string.special)
;; These are aliased to "command" in the grammar
(command) @string.special

;; Hook keywords -> brightcyan (@delimiter)
[
  "account-hook"
  "charset-hook"
  "iconv-hook"
  "crypt-hook"
  "fcc-hook"
  "save-hook"
  "folder-hook"
  "mbox-hook"
  "message-hook"
  "open-hook"
  "close-hook"
  "append-hook"
  "reply-hook"
  "send-hook"
  "send2-hook"
  "timeout-hook"
  "startup-hook"
  "shutdown-hook"
] @delimiter

;; Alias keyword -> brightcyan (@delimiter)
"alias" @delimiter

;; String delimiters -> green (@string)
[
  "'"
  "\""
] @string

;; Backtick (shell command) -> brightred (@function.special)
"`" @function.special
