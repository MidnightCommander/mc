;; Tree-sitter highlight queries for COBOL
;; Colors aligned with MC's cobol.syntax
;; MC: keywords=yellow, division headings=cyan, I/O=brightred, comments=brown, strings=green

;; Division/section headings -> cyan (@label)
(identification_division) @label
(environment_division) @label
(data_division) @label
(procedure_division) @label
(section_header) @label
(paragraph_header) @label

;; Comments -> brown
(comment) @comment
(comment_entry) @comment

;; Strings -> green
(string) @string
(x_string) @string
(h_string) @string
(n_string) @string

;; Numbers
(integer) @constant
(decimal) @constant
(number) @constant

;; Level numbers -> brightgreen (@string.special)
(level_number) @string.special
(level_number_88) @string.special

;; PIC patterns -> brightgreen (@string.special)
(picture_x) @string.special
(picture_9) @string.special
(picture_a) @string.special
(picture_n) @string.special
(picture_edit) @string.special

;; Identifiers
(qualified_word) @constant
