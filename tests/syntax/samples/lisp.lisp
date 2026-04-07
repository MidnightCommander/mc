;;; Common Lisp sample file for syntax highlighting
;; Exercises all TS captures from commonlisp-highlights.scm

;; defun with lambda list
(defun factorial (n)
  "Compute factorial of N."
  (if (<= n 1)
      1
      (* n (factorial (- n 1)))))

;; defmacro
(defmacro when-positive (x &body body)
  `(when (> ,x 0)
     ,@body))

;; defgeneric and defmethod
(defgeneric describe-object (obj))
(defmethod describe-object ((obj string))
  (format t "String: ~a~%" obj))

;; let and let*
(let ((x 10)
      (y 20))
  (+ x y))

(let* ((a 5)
       (b (* a 2)))
  (print b))

;; cond expression
(defun classify (n)
  (cond
    ((< n 0) 'negative)
    ((= n 0) 'zero)
    (t 'positive)))

;; lambda
(mapcar (lambda (x) (* x x)) '(1 2 3 4 5))

;; car, cdr, cons
(defun first-and-rest (lst)
  (cons (car lst) (cdr lst)))

(car '(a b c))
(cdr '(a b c))
(cadr '(a b c))

;; do loop
(do ((i 0 (+ i 1)))
    ((>= i 10) nil)
  (print i))

;; loop macro
(loop for i from 1 to 10
      when (evenp i)
      collect i into evens
      finally (return evens))

;; Keyword arguments
(defun make-person (&key name age)
  (list :name name :age age))

(make-person :name "Alice" :age 30)

;; nil and t literals
(if nil
    (print "never")
    (print "always"))

(and t t nil)
(or nil nil t)

;; unless and when
(unless (null '(1 2 3))
  (print "not null"))

(when (> 5 3)
  (print "five is greater"))

;; Quoting and unquoting
'(a b c)
`(a ,x ,@rest)
#'car

;; Character literals
#\A
#\Space
#\Newline

;; Numbers
42
3.14
-17
1/3
#xFF
#b1010
1.0e-5

;; Strings
"Hello, World!"
"Escaped \"quotes\" here"
"Multi-word string value"

;; Nested function calls
(format t "Result: ~a~%" (+ (* 2 3) (- 10 4)))

;; setq and multiple values
(setq *global-var* 100)
(defvar *counter* 0)

;; apply and funcall
(apply #'+ '(1 2 3))
(funcall #'car '(a b))

;; Block comment
#|
This is a block comment.
It can span multiple lines.
|#

;; Deeply nested expressions
(defun deep-example ()
  (let ((result (mapcar (lambda (x)
                          (if (> x 0)
                              (* x 2)
                              (- x)))
                        '(-3 -1 0 1 3))))
    (format t "~a~%" result)))
