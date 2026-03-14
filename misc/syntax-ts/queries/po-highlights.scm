;; Tree-sitter highlight queries for PO (gettext) files
;; Colors aligned with MC's default po.syntax
;; MC: msgid/msgstr=brightcyan, # comments=brown, #:/,=white, strings inherit context

;; Keywords -> brightcyan (tag)
[
  "msgid"
  "msgstr"
  "msgctxt"
  "msgid_plural"
] @tag

;; Strings -> green (MC: msgstr context is green, msgid context is cyan)
(string) @string

;; Translator comments (# ...) -> brown
(comment) @comment
(translator_comment) @comment

;; Extracted comments (#. ...) -> white
(extracted_comment) @tag.special

;; Reference comments (#: ...) -> white
(reference) @tag.special

;; Flag comments (#, ...) -> white
(flag) @tag.special

;; Previous untranslated strings (#~ ...) -> white
(previous_untranslated_string) @tag.special
