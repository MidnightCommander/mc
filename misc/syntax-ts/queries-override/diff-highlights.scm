;; Tree-sitter highlight queries for Diff/Patch output
;; Colors aligned with MC's default diff.syntax

;; # comments -> brightcyan (tag, MC uses brightcyan for # lines)
(comment) @tag

;; diff command line -> brightred (function.special, MC uses white+red)
(command) @function.special

;; file change headers (new file, deleted file, etc) -> brightmagenta
(file_change) @markup.heading

;; --- +++ *** filenames -> brightmagenta (markup.heading)
(filename) @markup.heading

;; similarity index -> brown (comment)
(similarity) @comment

;; index abc..def -> brown (comment)
(index) @comment

;; mode changes -> brown (comment)
(mode) @comment

;; @@ location -> brightcyan (delimiter)
(location) @delimiter

;; Additions -> brightgreen
(addition) @markup.addition

;; Deletions -> brightred
(deletion) @markup.deletion

;; Context lines -> yellow (keyword, MC default context is yellow)
(context) @keyword
