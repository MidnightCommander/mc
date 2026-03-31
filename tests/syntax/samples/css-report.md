CSS syntax highlighting: TS vs Legacy comparison report
=======================================================

Sample file: `tests/syntax/samples/css.css`
Legacy reference: `misc/syntax/css.syntax`
TS query: `misc/syntax-ts/queries-override/css-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[css]`

Aligned with legacy
-------------------

- Comments (`/* */`): `brown` - MATCH
- Tag selectors (`body`, `h1`, `h2`, `div`, `p`, `a`, `input`, `li`): `white`
  via `@tag.special` - MATCH
- Class selectors (`.container`, `.box`): `green` via `@string` - MATCH
- Pseudo-classes (`:hover`, `:visited`, `:first-child`): `brightmagenta` via
  `@delimiter.special` - MATCH
- Pseudo-elements (`::before`, `::after`): `white` via `@tag.special` - MATCH
- Property names (`margin`, `padding`, `color`, `font-size`, etc.): `lightgray`
  via `@constant` - MATCH
- Color hex values (`#333333`, `#ff0000`): `red` via `@comment.error` - MATCH
- Numbers with units (`16px`, `1.5em`, `10pt`, `300px`): `brightgreen` via
  `@string.special` - MATCH
- Plain values (`bold`, `center`, `auto`, `block`, etc.): `brightgreen` via
  `@string.special` - MATCH
- `!important`: `brightred` via `@function.special` - MATCH
- Semicolons: `brightmagenta` via `@delimiter.special` - MATCH
- Commas and colons: `brightcyan` via `@delimiter` - MATCH
- `@media` at-rule: `brightred` via `@function.special` - MATCH
- `@import` at-rule: `brightred` via `@function.special` - MATCH
- `@charset` at-rule: `brightred` via `@function.special` - MATCH
- `@keyframes` at-rule: `brightred` via `@function.special` - MATCH
- `@supports` at-rule: `brightred` via `@function.special` - MATCH
- `@namespace` at-rule: `brightred` via `@function.special` - MATCH
- `@scope` at-rule: `brightred` via `@function.special` - MATCH
- Function names (`url`, `calc`, `rotate`, `blur`, `linear-gradient`,
  `translateX`, `repeat`, `scale`): `magenta` via `@keyword.directive` - MATCH
- Logical operators (`and`, `not`, `only`): `yellow` via `@keyword` - MATCH
- String values (`"UTF-8"`, `"reset.css"`, `"Hello, world!"`): `white` via
  `@keyword.other` - MATCH
- ID selectors (`#main-content`): `green` via `@string` - MATCH
- Combinators (`>`, `+`, `~`): `brightcyan` via `@operator` - MATCH
- Attribute selector operators (`=`, `^=`, `$=`, `*=`): `brightcyan` via
  `@operator` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS recognizes ALL CSS property names via the grammar's `property_name` node,
  while legacy has a fixed list of known properties. Properties like `z-index`,
  `transition`, `transform`, `grid-template-columns`, `gap`, `background-size`,
  `border-radius`, `opacity`, `filter` are all properly colored `lightgray` by
  TS. Legacy misses `z-index`, `transition`, `border-radius`, `background-size`,
  `grid-template-columns`, `gap` and others.
- TS properly handles `#abc` (3-digit hex) as `red` color value. Legacy only
  matched 6-digit hex codes.
- TS parses percentage values (`50%`, `100%`) as `brightgreen` (number + unit),
  consistent with other numeric values. Legacy colored percentages as
  `brightred`.
- TS handles `@scope` at-rule which legacy did not know about.
- TS highlights `nth-child()` arguments properly, with the pseudo-class in
  `brightmagenta`.
- TS properly handles decimal numbers like `0.85`, `0.3s`, `1.2` as complete
  tokens in `brightgreen`. Legacy sometimes splits them at the decimal point.
- TS correctly identifies `::` double-colon pseudo-element prefix as
  `brightcyan` delimiter separate from the pseudo-element name.
- Curly braces `{` `}` are not highlighted by TS (default color), while legacy
  colored them `yellow`. This is a neutral change -- both approaches are
  reasonable.

Known shortcomings
------------------

- Legacy colored curly braces `{` `}` in `yellow`, TS leaves them uncolored
  (default). This is a minor visual difference.
- Legacy recognized specific font names (`Arial`, `Verdana`, `Helvetica`,
  `sans-serif`) as `brightgreen` keywords. TS treats these as plain values via
  `@string.special` which also results in `brightgreen`, so the effect is the
  same in most cases.
- Legacy had distinct `context` blocks for `counter()`, `counters()`, `rgb()`,
  `url()` functions with `magenta` interiors. TS colors only the function name
  in `magenta` via `@keyword.directive`, with arguments following normal value
  rules.
- Inside media query feature parentheses, TS does not color `max-width` as
  `lightgray` property (no property_name node in that context in the legacy
  output), though in the TS output it does appear to handle this correctly.
