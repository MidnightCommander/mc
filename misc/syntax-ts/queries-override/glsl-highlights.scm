;; Tree-sitter highlight queries for GLSL (OpenGL Shading Language)
;; Colors aligned with MC's default glsl.syntax

;; Keywords -> yellow
[
  "break"
  "case"
  "const"
  "continue"
  "default"
  "do"
  "else"
  "for"
  "if"
  "return"
  "struct"
  "switch"
  "while"
  "uniform"
  "varying"
  "attribute"
  "in"
  "out"
  "inout"
  "layout"
  "flat"
  "smooth"
  "noperspective"
  "centroid"
  "sample"
  "patch"
  "buffer"
  "shared"
  "coherent"
  "volatile"
  "restrict"
  "readonly"
  "writeonly"
  "precision"
  "highp"
  "mediump"
  "lowp"
] @keyword

;; Preprocessor directives -> brightred
"#define" @function.special
"#elif" @function.special
"#else" @function.special
"#endif" @function.special
"#if" @function.special
"#ifdef" @function.special
"#ifndef" @function.special
"#include" @function.special
(preproc_directive) @function.special

;; Types -> yellow
(primitive_type) @type
(type_identifier) @type
(sized_type_specifier) @type

;; Comments -> brown
(comment) @comment

;; Strings -> green, chars -> brightgreen
(string_literal) @string
(char_literal) @string.special

;; Built-in functions -> brightmagenta (MC uses brightmagenta for builtins)
(call_expression
  function: (identifier) @function.macro
  (#any-of? @function.macro
    "length" "normalize" "dot" "cross" "reflect" "refract"
    "mix" "clamp" "smoothstep" "step" "min" "max"
    "abs" "sign" "floor" "ceil" "fract" "mod"
    "pow" "exp" "log" "sqrt" "inversesqrt"
    "sin" "cos" "tan" "asin" "acos" "atan"
    "radians" "degrees"
    "texture" "texture2D" "texture3D" "textureCube"
    "texelFetch" "textureSize"
    "dFdx" "dFdy" "fwidth"
    "transpose" "inverse" "determinant" "distance"
    "lessThan" "greaterThan" "equal" "notEqual"
    "any" "all" "not"))

;; Operators -> white (MC uses white for arithmetic/comparison operators)
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
  "&"
  "|"
  "^"
  "~"
  "+"
  "-"
  "*"
  "/"
  "%"
  "++"
  "--"
] @keyword.other

;; Brackets -> brightcyan
[
  "{"
  "}"
  "("
  ")"
  "["
  "]"
] @delimiter

;; Delimiters -> brightcyan for comma/dot, brightmagenta for semicolons
[
  ","
] @delimiter

";" @delimiter.special
