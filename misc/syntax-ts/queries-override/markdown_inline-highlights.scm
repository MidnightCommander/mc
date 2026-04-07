;; Tree-sitter highlight queries for Markdown inline parser
;; Used via language injection from the Markdown block parser.
;; Colors aligned with MC's default markdown.syntax:
;;   bold = brightmagenta (markup.bold)
;;   italic = magenta (markup.italic)
;;   code spans = cyan (label)
;;   links = yellow (keyword)
;;   images = yellow (keyword)
;;   HTML tags = brightred (function.special)
;;   escape sequences = brightgreen (string.special)

;; Emphasis (*italic* and _italic_) -> magenta (markup.italic)
(emphasis) @markup.italic

;; Strong emphasis (**bold** and __bold__) -> brightmagenta (markup.bold)
(strong_emphasis) @markup.bold

;; Strikethrough (~~text~~) -> brightcyan (delimiter)
(strikethrough) @delimiter

;; Code spans (`code`) -> cyan (label)
(code_span) @label

;; Links -> yellow (keyword)
(shortcut_link) @keyword
(full_reference_link) @keyword
(collapsed_reference_link) @keyword
(inline_link) @keyword

;; Images -> yellow (keyword)
(image) @keyword

;; Autolinks -> yellow (keyword)
(uri_autolink) @keyword
(email_autolink) @keyword

;; Inline HTML tags -> brightred (function.special)
(html_tag) @function.special

;; Escape sequences -> brightgreen (string.special)
(backslash_escape) @string.special

;; Entity references -> brightgreen (string.special)
(entity_reference) @string.special
(numeric_character_reference) @string.special

;; LaTeX spans -> cyan (label)
(latex_block) @label
