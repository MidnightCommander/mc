;; Tree-sitter highlight queries for D language
;; Colors aligned with MC's d.syntax
;; D keywords are named nodes (sym_abstract, sym_break, etc.), not anonymous tokens

;; Keywords -> yellow (@keyword) -- MC uses yellow for most keywords
[
  (abstract)
  (alias)
  (assert)
  (break)
  (case)
  (cast)
  (catch)
  (class)
  (const)
  (continue)
  (debug)
  (default)
  (delegate)
  (delete)
  (deprecated)
  (do)
  (else)
  (enum)
  (export)
  (extern)
  (final)
  (finally)
  (for)
  (foreach)
  (foreach_reverse)
  (function)
  (goto)
  (if)
  (immutable)
  (in)
  (interface)
  (invariant)
  (is)
  (lazy)
  (mixin)
  (module)
  (new)
  (nothrow)
  (out)
  (override)
  (package)
  (pragma)
  (private)
  (protected)
  (public)
  (pure)
  (ref)
  (return)
  (scope)
  (shared)
  (static)
  (struct)
  (switch)
  (synchronized)
  (template)
  (throw)
  (try)
  (typeid)
  (typeof)
  (union)
  (unittest)
  (version)
  (while)
  (with)
] @keyword

;; import -> magenta (@keyword.directive) -- MC uses magenta
(import) @keyword.directive

;; Types -> yellow (@type) -- MC uses yellow for types too
[
  (auto)
  (bool)
  (byte)
  (ubyte)
  (short)
  (ushort)
  (int)
  (uint)
  (long)
  (ulong)
  (float)
  (double)
  (real)
  (char)
  (wchar)
  (dchar)
  (string)
  (wstring)
  (dstring)
  (size_t)
  (ptrdiff_t)
  (void)
] @type

(comment) @comment
(string_literal) @string
(raw_string) @string
(hex_string) @string
(token_string) @string
(char_literal) @string.special

;; true, false, null, super, this -> brightred (@variable.builtin)
;; MC uses brightred for these
(true) @variable.builtin
(false) @variable.builtin
(null) @variable.builtin
(super) @variable.builtin
(this) @variable.builtin

;; Special keywords (__FILE__ etc.) -> red -- closest is @function.special (brightred)
(special_keyword) @function.special

(module_declaration
  (module_fqn) @type)

;; Operators -> yellow (@operator.word) -- MC uses yellow for all operators
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "^^"
  "~"
  "&"
  "|"
  "^"
  "<<"
  ">>"
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "&="
  "|="
  "^="
  "<<="
  ">>="
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "&&"
  "||"
  "!"
  "++"
  "--"
  ".."
  "=>"
  ":"
] @operator.word

;; Delimiters -> brightcyan (@delimiter) -- MC uses brightcyan for parens, braces, etc.
[
  "."
  ","
] @delimiter

;; Semicolons -> brightmagenta (@delimiter.special) -- MC uses brightmagenta
";" @delimiter.special

(label (identifier) @label)
