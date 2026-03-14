;; Tree-sitter highlight queries for Kotlin
;; Colors aligned with MC's default kotlin.syntax

;; Hard keywords -> yellow (keyword)
[
  "as"
  "class"
  "do"
  "else"
  "for"
  "fun"
  "if"
  "in"
  "interface"
  "is"
  "object"
  "return"
  "super"
  "this"
  "throw"
  "try"
  "typealias"
  "val"
  "var"
  "when"
  "while"
] @keyword

;; package/import -> brown (function.builtin)
[
  "package"
  "import"
] @function.builtin

;; Soft keywords -> brightgreen (type.builtin)
[
  "by"
  "catch"
  "constructor"
  "finally"
  "get"
  "init"
  "set"
  "where"
] @type.builtin

;; Modifier keywords -> brightmagenta (keyword.control)
[
  "abstract"
  "annotation"
  "companion"
  "const"
  "crossinline"
  "data"
  "enum"
  "expect"
  "external"
  "final"
  "infix"
  "inline"
  "inner"
  "internal"
  "lateinit"
  "noinline"
  "open"
  "operator"
  "out"
  "override"
  "private"
  "protected"
  "public"
  "sealed"
  "suspend"
  "tailrec"
  "vararg"
] @keyword.control

;; Comments -> brown (comment)
(line_comment) @comment
(block_comment) @comment

(string_literal) @string
(multiline_string_literal) @string
(character_literal) @string.special

;; Annotations -> brightcyan (delimiter)
(annotation) @delimiter

;; Built-in types -> brightred (variable.builtin)
;; MC colors Double/Float/Long/Int/Short/Byte/Char/Boolean/Array/String as brightred
((identifier) @variable.builtin
 (#match? @variable.builtin "^(Double|Float|Long|Int|Short|Byte|Char|Boolean|Array|String|ByteArray|ByteSequence)$"))

(label) @label

;; Operators -> brightcyan (operator)
[
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "&&"
  "||"
  "!"
  "+"
  "-"
  "*"
  "/"
  "%"
  "++"
  "--"
  "->"
  "?:"
  ".."
  "::"
  "!!"
  "as?"
] @operator

[
  "."
  ","
  ":"
] @delimiter

";" @delimiter