Lisp syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `lisp.lisp`
Legacy reference: `misc/syntax/lisp.syntax`
TS query: `misc/syntax-ts/queries-override/commonlisp-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[commonlisp]`

Aligned with legacy
-------------------

- Comments (`;` lines): `brown` (TS `comment`) - MATCH.
- Strings (`"..."`): `green` (TS `string`) - MATCH.
- Parentheses (`(`, `)`): `brightcyan` (TS `delimiter`) - MATCH.
- Keyword arguments (`:name`, `:age`): `white` (TS `keyword.other`) - MATCH.
- Quote/unquote (`'`, `,`, `#'`): `brightmagenta` (TS `constant.builtin`) -
  MATCH.
- `nil`: `brightred` (TS `variable.builtin`) - MATCH.
- `defun`: `yellow` (TS `keyword`) - MATCH.
- Core functions used as first list element (`car`, `cdr`, `cons`, `format`,
  `print`, `mapcar`, `apply`, `list`, `setq`, etc.): both highlight in a
  distinct color (legacy `yellow` as keywords; TS `brightcyan` as `function`
  calls).

Intentional improvements over legacy
-------------------------------------

- TS highlights `defmacro`, `defgeneric`, `defmethod` as `yellow` (`keyword`);
  legacy only has `defun` in its keyword list.
- TS highlights function names in `defun` headers as `brightcyan` (`function`),
  giving definitions visual prominence separate from the `defun` keyword.
- TS highlights parameter variables in lambda lists as `lightgray` (`variable`),
  distinguishing them from function names.
- TS highlights numeric literals (`42`, `3.14`, `#xFF`, `#b1010`, `1.0e-5`) as
  `lightgray` (`number`); legacy does not highlight numbers.
- TS highlights character literals (`#\A`, `#\Space`) as `brightgreen`
  (`string.special`); legacy does not handle these.
- TS highlights block comments (`#| ... |#`) as `brown` (`comment`); legacy only
  supports `;` line comments.
- TS highlights `loop` macro keywords (`for`, `when`, `into`, `finally`,
  `return`) as `yellow` (`keyword`), providing structural awareness of the loop
  macro; legacy has no loop keyword support.
- TS highlights `lambda` as `yellow` (`keyword`) via `defun_keyword`; legacy
  colors it `brightred` (different but both highlighted).
- TS highlights symbols used as variables as `lightgray` (`variable`), providing
  a visual baseline distinct from function calls.

Known shortcomings
------------------

- TS does not highlight `t` as `brightred` like legacy does; instead `t` appears
  as `lightgray` (default variable) except when it appears as the first element
  of a list. Legacy treats both `t` and `nil` as special `brightred` keywords.
- TS does not specifically highlight `lambda` in `brightred` as legacy does; TS
  uses `yellow` (`keyword`) which matches other keywords but loses the legacy
  distinction between `lambda`/`nil`/`t` and regular keywords.
- `&body` and `&key` parameter markers show as `lightgray` (`variable`) in TS;
  legacy colors `&`-prefixed tokens as `white`.
- Some function calls like `cond`, `unless`, `when`, `null`, `let`, `let*`,
  `if`, `do`, `and`, `or` that legacy highlights as `yellow` keywords only
  appear as `brightcyan` (`function`) in TS when used as the first list element,
  not as `yellow` keywords (except those explicitly listed in the keyword list
  like `if`, `when`, `unless`, `do`, `and`, `loop`, `defun`).
- TS `#'` (function quote) in `#'car` colors `#'` as `brightmagenta` but the
  following symbol differently than legacy, which colors the entire `#'` token
  as `brightmagenta` and the following symbol normally.
