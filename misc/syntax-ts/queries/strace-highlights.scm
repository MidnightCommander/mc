;; Tree-sitter highlight queries for strace output
;; Colors aligned with MC's strace.syntax
;; MC colors syscalls by category: file I/O=cyan, read/write=magenta, memory=red, etc.

;; Syscall names -> brightred (@function.special) -- most common default
(syscall) @function.special

;; Strings -> green (@string)
(string) @string

;; Return values -> yellow (@keyword)
(returnValue) @keyword

;; Error names -> brightred (@function.special)
(errorName) @function.special

;; Error descriptions -> brown (@comment)
(errorDescription) @comment

;; Signal names -> brightred (@function.special)
(signal) @function.special

;; PIDs -> brightcyan (@tag)
(pid) @tag

;; Pointers -> lightgray (@constant)
(pointer) @constant

;; Integers -> lightgray (@constant)
(integer) @constant

;; Macros/flags -> yellow (@keyword)
(macro) @keyword

;; Comments -> brown
(comment) @comment

;; Punctuation
[
  "("
  ")"
  ","
  "="
] @delimiter
