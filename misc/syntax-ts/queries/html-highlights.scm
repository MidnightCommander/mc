;; Tree-sitter highlight queries for HTML
;; Colors aligned with MC's default html.syntax

;; Tags -> brightcyan
(tag_name) @tag

;; Attributes -> yellow
(attribute_name) @property.key

;; Attribute values -> cyan (MC uses cyan for quoted values)
(attribute_value) @label
(quoted_attribute_value) @label

;; Entities like &amp; -> brightgreen
(entity) @string.special

;; Comments -> brown
(comment) @comment

;; <!DOCTYPE> -> brightred
(doctype) @function.special
(erroneous_end_tag_name) @tag

;; = -> brightred
[
  "="
] @variable.builtin

;; Angle brackets -> brightcyan
[
  "<"
  ">"
  "</"
  "/>"
] @delimiter
