;; Tree-sitter highlight queries for VHDL
;; Colors aligned with MC's default vhdl.syntax
;; MC: keywords=yellow, word-operators=green, symbol-operators=brightgreen,
;;     comments=magenta, strings=green, types=cyan, booleans=brightred,
;;     type/subtype decl=brightcyan, ports(in/out/etc)=white

;; Core keywords -> yellow
[
  "library"
  "use"
  "entity"
  "is"
  "port"
  "architecture"
  "of"
  "begin"
  "end"
  "signal"
  "variable"
  "constant"
  "array"
  "range"
  "to"
  "downto"
  "process"
  "if"
  "then"
  "else"
  "elsif"
  "case"
  "when"
  "others"
  "for"
  "while"
  "loop"
  "generate"
  "component"
  "generic"
  "map"
  "wait"
  "until"
  "assert"
  "return"
  "function"
  "procedure"
  "package"
  "body"
  "attribute"
  "file"
  "access"
  "alias"
  "record"
  "with"
  "select"
  "after"
  "transport"
  "inertial"
  "reject"
  "block"
  "configuration"
  "impure"
  "pure"
  "shared"
  "open"
  "null"
  "new"
] @keyword

;; Word operators -> green (MC uses green for and/or/not/nand/etc)
[
  "not"
  "and"
  "or"
  "nand"
  "nor"
  "xor"
  "xnor"
] @string

;; Port direction keywords -> white (MC uses white for in/out/inout/buffer)
[
  "in"
  "out"
  "inout"
  "buffer"
] @keyword.other

;; Type/subtype declarations -> brightcyan
[
  "type"
  "subtype"
] @tag

;; Entity/architecture/component names -> cyan (type)
(entity_declaration
  name: (identifier) @label)
(architecture_body
  (identifier) @label)
(component_declaration
  name: (identifier) @label)
(full_type_declaration
  (identifier) @label)
(subtype_declaration
  (identifier) @label)

;; Function/procedure calls and declarations -> brightcyan
(procedure_call_statement
  procedure: (simple_name) @function)
(function_call
  function: (simple_name) @function)
(function_declaration
  designator: (identifier) @function)
(procedure_declaration
  designator: (identifier) @function)

;; Comments -> magenta (MC uses magenta for comments)
(comment) @keyword.directive

;; Strings -> green
(string_literal) @string

;; Character/bit literals -> brightgreen (string.special)
(character_literal) @string.special
(bit_string_literal) @string.special

;; Symbol operators -> brightgreen (MC uses brightgreen for := . ; : , ' etc)
[
  "=>"
  "<="
  ":="
  "="
  "/="
  "<"
  ">"
  "+"
  "-"
  "*"
  "/"
  "&"
  "**"
] @string.special

;; Delimiters -> brightgreen (MC uses brightgreen for . ; : , ( ) [ ] |)
[
  "."
  ";"
  ","
  ":"
] @string.special

;; Labels
(label (identifier) @label)
