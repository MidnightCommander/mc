;; Tree-sitter highlight queries for Common Lisp
;; Colors aligned with MC's lisp.syntax
;; Note: grammar directory is "lisp" but grammar name is "commonlisp"

;; Core keywords/functions -> yellow (@keyword)
;; MC colors: apply, car, cdr, cons, defun, if, let, setq, etc. = yellow
[
  "defun"
  "defmacro"
  "defgeneric"
  "defmethod"
  "if"
  "when"
  "unless"
  "do"
  "loop"
  "for"
  "and"
  "as"
  "with"
  "while"
  "until"
  "repeat"
  "return"
  "else"
  "finally"
  "in"
  "then"
  "into"
] @keyword

(defun_keyword) @keyword

(comment) @comment
(block_comment) @comment

(str_lit) @string

(num_lit) @number

;; nil -> brightred (@variable.builtin) -- MC uses brightred for nil, t, lambda
(nil_lit) @variable.builtin

;; :keyword-args -> white (@keyword.other) -- MC uses white for :keywords
(kwd_lit) @keyword.other

(sym_lit) @variable

(defun_header
  function_name: (sym_lit) @function)
(defun_header
  lambda_list: (list_lit
    (sym_lit) @variable))

;; Function calls (first element of list)
(list_lit
  .
  (sym_lit) @function)

;; Parentheses -> brightcyan (@delimiter) -- MC uses brightcyan
[
  "("
  ")"
] @delimiter

(char_lit) @string.special

;; Quote/unquote -> brightmagenta (@constant.builtin) -- MC uses brightmagenta for #', ', ,
(quoting_lit) @constant.builtin
(unquoting_lit) @constant.builtin
