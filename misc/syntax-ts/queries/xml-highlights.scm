;; Tree-sitter highlight queries for XML
;; Colors aligned with MC's default xml.syntax

;; Tag names -> white (tag.special)
(Name) @tag.special

;; Attribute values -> brightcyan (delimiter)
(AttValue) @delimiter

;; Comments -> brightgreen (comment.special)
(Comment) @comment.special

;; <?xml ...?> prolog -> keyword (yellow)
(XMLDecl) @keyword

;; <!DOCTYPE> -> keyword (yellow)
(doctypedecl) @keyword

;; Processing instructions -> constant (lightgray)
(PI) @constant

;; Character/entity references -> tag.special (white)
(EntityRef) @tag.special
(CharRef) @tag.special
(PEReference) @tag.special

;; CharData (text content) -> default
(CharData) @variable

;; = -> keyword (yellow, MC uses yellow for \s*=)
[
  "="
] @keyword

;; Angle brackets / delimiters -> tag.special (white)
[
  "<"
  ">"
  "</"
  "/>"
  "<?"
  "?>"
] @tag.special
