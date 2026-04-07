;; Tree-sitter highlight queries for x86 Assembly
;; Colors aligned with MC's assembler.syntax
;; MC: keywords=white, registers=brightmagenta, comments=brown, strings=green

;; Registers -> brightmagenta (@keyword.control)
(reg) @keyword.control

;; Labels -> cyan (@label)
(label) @label

;; Instructions (opcode mnemonics) -> white (@keyword.other)
(instruction) @keyword.other

;; Meta directives -> white (@keyword.other)
(meta) @keyword.other

;; Constants -> white (@keyword.other)
(const) @keyword.other

;; Pointer types -> white (@keyword.other)
[
  "byte"
  "word"
  "dword"
  "qword"
  "ptr"
] @keyword.other

;; Comments -> brown
(line_comment) @comment
(block_comment) @comment

;; Strings -> green
(string) @string

;; Identifiers
(ident) @constant
