;; Tree-sitter highlight queries for Rust
;; Colors aligned with MC's default rust.syntax

;; Keywords -> yellow
[
  "as"
  "async"
  "await"
  "break"
  "const"
  "continue"
  "dyn"
  "else"
  "enum"
  "extern"
  "fn"
  "for"
  "if"
  "impl"
  "in"
  "let"
  "loop"
  "match"
  "mod"
  "move"
  "pub"
  "ref"
  "return"
  "static"
  "struct"
  "trait"
  "type"
  "unsafe"
  "use"
  "where"
  "while"
  "yield"
] @keyword

(crate) @keyword
(super) @keyword
(mutable_specifier) @keyword

;; self -> brightgreen
(self) @string.special

;; true/false -> brightgreen
(boolean_literal) @string.special

;; Macros -> brightmagenta
(macro_invocation
  macro: (identifier) @function.macro)
(macro_invocation
  macro: (scoped_identifier
    name: (identifier) @function.macro))
(macro_definition
  name: (identifier) @function.macro)

;; Types -> brightcyan (MC colors builtin types as brightcyan)
(type_identifier) @function
(primitive_type) @function

(line_comment) @comment
(block_comment) @comment

(string_literal) @string
(raw_string_literal) @string
(char_literal) @string.special

;; Numbers -> brightgreen
(integer_literal) @number.builtin
(float_literal) @number.builtin

;; Attributes -> white
[
  "#"
] @tag.special

;; Enum variants Some/None/Ok/Err -> brightgreen
((identifier) @string.special
 (#match? @string.special "^(Some|None|Ok|Err)$"))

(label (identifier) @label)
