;; Tree-sitter highlight queries for Markdown language
;; Block-level parser only (inline nodes are in a separate parser)
;; Colors aligned with MC's default markdown.syntax:
;;   headings = brightred, heading #### = red (we use brightred for all)
;;   blockquote > = green
;;   code blocks = cyan
;;   links = yellow

;; Heading markers (#, ##, etc.) -> brightred (function.special)
(atx_heading
  (atx_h1_marker) @function.special)

(atx_heading
  (atx_h2_marker) @function.special)

(atx_heading
  (atx_h3_marker) @function.special)

(atx_heading
  (atx_h4_marker) @function.special)

(atx_heading
  (atx_h5_marker) @function.special)

(atx_heading
  (atx_h6_marker) @function.special)

;; Heading content -> brightred (function.special)
(atx_heading
  heading_content: (_) @function.special)

(setext_heading
  heading_content: (_) @function.special)

(setext_h1_underline) @function.special
(setext_h2_underline) @function.special

;; Blockquote marker -> green (string)
(block_quote
  (block_quote_marker) @string)

;; Code block delimiters and info string -> label (cyan)
;; Content is left uncolored so injected language highlights show correctly.
(fenced_code_block
  (fenced_code_block_delimiter) @label)
(fenced_code_block
  (info_string) @label)

(indented_code_block) @label

;; Thematic breaks (---) -> delimiter
(thematic_break) @delimiter

;; Links -> yellow (keyword)
(link_destination) @keyword
(link_label) @keyword
(link_title) @keyword

;; List markers -> default
(list_marker_plus) @variable
(list_marker_minus) @variable
(list_marker_star) @variable
(list_marker_dot) @variable
(list_marker_parenthesis) @variable

;; HTML blocks -> function.special (brightred, same as preprocessor)
(html_block) @function.special
