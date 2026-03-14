;; Tree-sitter highlight queries for CUDA language
;; Colors aligned with MC's default cuda.syntax
;; CUDA uses the C++ tree-sitter grammar with additional CUDA-specific keywords

;; CUDA-specific function/variable type qualifiers -> white (keyword.other)
((identifier) @keyword.other
 (#match? @keyword.other "^__(global|device|host|shared|constant|managed|restrict|noinline|forceinline)__$"))

;; CUDA built-in variables -> white (keyword.other)
((identifier) @keyword.other
 (#match? @keyword.other "^(threadIdx|blockIdx|blockDim|gridDim|warpSize)$"))

;; CUDA synchronization -> white (keyword.other)
((identifier) @keyword.other
 (#match? @keyword.other "^__(syncthreads|threadfence)$"))

[
  "break"
  "case"
  "catch"
  "class"
  "const"
  "constexpr"
  "continue"
  "default"
  "delete"
  "do"
  "else"
  "enum"
  "explicit"
  "extern"
  "final"
  "for"
  "friend"
  "if"
  "inline"
  "namespace"
  "new"
  "noexcept"
  "override"
  "private"
  "protected"
  "public"
  "return"
  "sizeof"
  "static"
  "struct"
  "switch"
  "template"
  "throw"
  "try"
  "typedef"
  "typename"
  "union"
  "using"
  "virtual"
  "volatile"
  "while"
] @keyword

;; true/false/nullptr -> yellow (keyword) in MC cuda.syntax
(true) @keyword
(false) @keyword
"nullptr" @keyword
(this) @keyword

;; Preprocessor -> brightred
[
  "#define"
  "#elif"
  "#else"
  "#endif"
  "#if"
  "#ifdef"
  "#ifndef"
  "#include"
] @function.special
(preproc_directive) @function.special

;; Operators -> yellow (@operator.word) -- MC uses yellow for all operators
[
  "--"
  "-"
  "-="
  "->"
  "="
  "!="
  "*"
  "&"
  "&&"
  "+"
  "++"
  "+="
  "<"
  "=="
  ">"
  ">="
  "<="
  "||"
  "!"
  "~"
  "<<"
  ">>"
  "%"
  "/"
  "|"
  "^"
  "::"
] @operator.word

;; Separators
"." @delimiter
"," @delimiter
":" @delimiter
";" @delimiter.special

(string_literal) @string
(system_lib_string) @string
(raw_string_literal) @string

(null) @constant
(char_literal) @string.special

(statement_identifier) @label
(primitive_type) @type
(sized_type_specifier) @type

(comment) @comment
