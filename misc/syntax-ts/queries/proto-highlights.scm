;; Tree-sitter highlight queries for Protocol Buffers
;; Colors aligned with MC's protobuf.syntax
;; MC: keywords=yellow, comments=brown, strings=green

;; Keywords -> yellow (@keyword)
[
  "syntax"
  "edition"
  "import"
  "weak"
  "public"
  "package"
  "option"
  "message"
  "enum"
  "service"
  "rpc"
  "returns"
  "stream"
  "extend"
  "oneof"
  "map"
  "reserved"
  "extensions"
  "to"
  "max"
  "optional"
  "required"
  "repeated"
  "group"
] @keyword

;; Type keywords -> yellow (@keyword)
[
  "int32"
  "int64"
  "uint32"
  "uint64"
  "sint32"
  "sint64"
  "fixed32"
  "fixed64"
  "sfixed32"
  "sfixed64"
  "bool"
  "string"
  "double"
  "float"
  "bytes"
] @keyword

;; Boolean literals -> yellow
(true) @keyword
(false) @keyword

;; Assignment -> yellow (@operator.word)
"=" @operator.word

;; Comments -> brown
(comment) @comment

;; Strings -> green
(string) @string

;; Escape sequences -> brightgreen
(escape_sequence) @string.special

;; Message/enum/service names -> brightcyan (@tag)
(message_name) @tag
(enum_name) @tag
(service_name) @tag
(rpc_name) @tag

;; Brackets -> brightcyan (@delimiter)
[
  "{"
  "}"
  "("
  ")"
  "["
  "]"
] @delimiter

;; Punctuation -> brightcyan (@delimiter)
[
  ","
  ":"
  "."
  "<"
  ">"
] @delimiter

;; Semicolons -> brightmagenta (@delimiter.special)
";" @delimiter.special
