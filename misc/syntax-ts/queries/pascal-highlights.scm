;; Tree-sitter highlight queries for Pascal language
;; Colors aligned with MC's default pascal.syntax
;; MC: keywords=white, logical-ops=cyan, default-context=yellow, comments=brightgreen,
;;     strings=brightcyan, delimiters=lightgray

;; Keywords -> white (keyword.other)
[
  (kProgram)
  (kUnit)
  (kUses)
  (kInterface)
  (kImplementation)
  (kBegin)
  (kEnd)
  (kVar)
  (kConst)
  (kType)
  (kArray)
  (kOf)
  (kRecord)
  (kClass)
  (kObject)
  (kConstructor)
  (kDestructor)
  (kInherited)
  (kProperty)
  (kRead)
  (kWrite)
  (kIf)
  (kThen)
  (kElse)
  (kCase)
  (kFor)
  (kTo)
  (kDownto)
  (kWhile)
  (kRepeat)
  (kUntil)
  (kWith)
  (kDo)
  (kTry)
  (kExcept)
  (kFinally)
  (kRaise)
  (kSet)
  (kFunction)
  (kProcedure)
] @keyword.other

;; Logical operators -> cyan (label)
[
  (kAnd)
  (kOr)
  (kNot)
  (kXor)
  (kDiv)
  (kMod)
  (kShl)
  (kShr)
  (kIn)
  (kIs)
  (kAs)
] @label

;; Constants
[
  (kNil)
  (kTrue)
  (kFalse)
] @keyword.other

;; String type keyword
[
  (kString)
] @type

;; Operators -> cyan (MC uses cyan for operators)
[
  (kEq)
  (kNeq)
  (kLt)
  (kGt)
  (kLte)
  (kGte)
  (kAdd)
  (kSub)
  (kMul)
  (kFdiv)
  (kAssign)
  (kAt)
  (kHat)
] @label

;; Delimiters -> lightgray (MC uses lightgray for ; : , . ( ) [ ])
[
  ";"
  ":"
  ","
] @number

;; Comments -> brightgreen (MC uses brightgreen for comments)
(comment) @comment.special

;; Literal strings -> brightcyan (MC uses brightcyan for ' ' strings)
(literalString) @tag
