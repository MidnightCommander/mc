;; Tree-sitter highlight queries for Caddyfile
;; Colors aligned with MC's caddyfile.syntax
;; MC: directives=yellow, subdirectives=brightcyan, comments=brown, strings=green

;; Directives -> yellow (@keyword)
(directive) @keyword

;; Server addresses -> brightmagenta (@keyword.control)
(address) @keyword.control

;; Snippet names -> brightmagenta (@keyword.control)
(snippet_name) @keyword.control

;; Matchers -> brightmagenta (@keyword.control)
(matcher) @keyword.control

;; Booleans -> brightgreen (@string.special)
(boolean) @string.special

;; Placeholders -> brightred (@function.special)
(placeholder) @function.special

;; Comments -> brown
(comment) @comment

;; Strings -> green
(quoted_string_literal) @string
(string_literal) @constant

;; Numeric literals -> brightgreen (@string.special)
(numeric_literal) @string.special

;; Block delimiters -> yellow (@keyword)
[
  "{"
  "}"
] @keyword
