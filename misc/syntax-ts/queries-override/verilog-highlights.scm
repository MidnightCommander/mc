;; Tree-sitter highlight queries for Verilog
;; Colors aligned with MC's default verilog.syntax
;; MC: keywords=yellow, comments=brown, strings=green, operators=yellow,
;;     brackets=brightcyan, ;=brightmagenta, bitwise=brightmagenta

;; Keywords -> yellow
[
  "module"
  "endmodule"
  "input"
  "output"
  "inout"
  "wire"
  "reg"
  "integer"
  "real"
  "parameter"
  "localparam"
  "assign"
  "always"
  "initial"
  "begin"
  "end"
  "if"
  "else"
  "case"
  "casex"
  "casez"
  "endcase"
  "for"
  "while"
  "repeat"
  "forever"
  "generate"
  "endgenerate"
  "function"
  "endfunction"
  "task"
  "endtask"
  "posedge"
  "negedge"
  "specify"
  "endspecify"
  "default"
  "signed"
  "unsigned"
  "supply0"
  "supply1"
  "tri"
  "triand"
  "trior"
  "tri0"
  "tri1"
  "wand"
  "wor"
  "genvar"
  "defparam"
  "disable"
  "event"
  "force"
  "release"
  "wait"
] @keyword

;; Gate primitives -> yellow
[
  "and"
  "or"
  "not"
  "nand"
  "nor"
  "xor"
  "xnor"
  "buf"
] @keyword

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string_literal) @string
(double_quoted_string) @string

;; Arithmetic/comparison operators -> yellow (MC uses yellow for these)
[
  "="
  "=="
  "!="
  "==="
  "!=="
  "<"
  ">"
  "<="
  ">="
  "+"
  "-"
  "*"
  "/"
  "%"
  "&&"
  "||"
  "!"
  "<<"
  ">>"
  "?"
  ":"
] @keyword

;; Bitwise operators -> brightmagenta (MC uses brightmagenta for & | ^ ~)
[
  "&"
  "|"
  "^"
  "~"
] @delimiter.special

;; Delimiters -> brightcyan (MC uses brightcyan for brackets, comma, dot)
[
  "."
  ","
] @delimiter

;; Semicolons -> brightmagenta
";" @delimiter.special
