; MC-specific markdown injections.
; Adds pipe_table_cell injection for markdown_inline (upstream only has inline).

(fenced_code_block
  (info_string
    (language) @injection.language)
  (code_fence_content) @injection.content)

((html_block) @injection.content
  (#set! injection.language "html"))

(document
  .
  (section
    .
    (thematic_break)
    (_) @injection.content
    (thematic_break))
  (#set! injection.language "yaml"))

((minus_metadata) @injection.content
  (#set! injection.language "yaml"))

((plus_metadata) @injection.content
  (#set! injection.language "toml"))

; Inline content (paragraphs, headings)
((inline) @injection.content
  (#set! injection.language "markdown_inline"))

; Table cells (not wrapped in inline nodes)
((pipe_table_cell) @injection.content
  (#set! injection.language "markdown_inline"))
