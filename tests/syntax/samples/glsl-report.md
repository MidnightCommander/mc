GLSL syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `glsl.glsl`
Legacy reference: `misc/syntax/glsl.syntax`
TS query: `misc/syntax-ts/queries-override/glsl-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[glsl]`

Aligned with legacy
-------------------

- Language keywords (`if`, `else`, `for`, `while`, `do`, `switch`, `case`,
  `default`, `break`, `continue`, `return`, `struct`, `const`): `yellow` - MATCH
- Storage qualifiers (`uniform`, `varying`, `attribute`, `in`, `out`, `inout`,
  `layout`, `flat`, `smooth`, `noperspective`, `centroid`, `sample`, `patch`,
  `buffer`, `shared`, `coherent`, `volatile`, `restrict`, `readonly`,
  `writeonly`): `yellow` - MATCH
- Precision qualifiers (`precision`, `highp`, `mediump`, `lowp`): `yellow` -
  MATCH
- Primitive types (`float`, `int`, `void`, `bool`, `vec2`, `vec3`, `vec4`,
  `mat2`, `mat3`, `mat4`, `mat3x4`, `ivec4`, `bvec3`, `dvec2`, `sampler2D`,
  `samplerCube`): `yellow` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Operators (`=`, `+=`, `-=`, `*=`, `/=`, `%=`, `==`, `!=`, `<`, `>`, `<=`,
  `>=`, `&&`, `||`, `!`, `&`, `|`, `^`, `~`, `+`, `-`, `*`, `/`, `%`, `++`,
  `--`): `white` - MATCH
- Brackets (`{`, `}`, `(`, `)`, `[`, `]`): `brightcyan` - MATCH
- Comma (`,`): `brightcyan` - MATCH
- Semicolons (`;`): `brightmagenta` - MATCH
- Char literals (`'A'`): `brightgreen` - MATCH
- Preprocessor directives (`#version`, `#define`, `#ifdef`, `#ifndef`,
  `#endif`): `brightred` - MATCH

Intentional improvements over legacy
-------------------------------------

- Type identifiers (user-defined types like `Light`): TS colors these as
  `yellow` via the `type_identifier` capture. Legacy does not color user-defined
  type names, leaving them as default text.
- The `^` operator: legacy fails to color `^` as `white` in the expression `0xA0
  ^ 0x55` (it appears uncolored). TS correctly colors it as `white` via the
  operator capture.
- The `!` operator: TS explicitly captures `!` as `white` via the operator list.
  Legacy colors `!=` as a unit but the standalone `!` (logical not) may not
  match consistently.
- Preprocessor directives are more cleanly handled in TS. Legacy uses a
  context-based approach where `#` starts a brightred context until end of line.
  TS captures specific preprocessor keywords (`#define`, `#ifdef`, etc.)
  individually as `function.special` -> `brightred`, giving more precise
  coloring.

Known shortcomings
------------------

- Built-in functions (`length`, `normalize`, `dot`, `cross`, `clamp`, `max`,
  `min`, `smoothstep`, `mix`, `sqrt`, `pow`, `sin`, `cos`, `texture`,
  `textureSize`): FIXED -- now captured via `call_expression` with `#any-of?`
  predicate on function identifier as `@function.macro` (`brightmagenta`),
  matching legacy.
- Deprecated built-in functions (`texture1D`, `texture2D`, `shadow1D`, etc.):
  legacy colors these as `magenta`. TS does not distinguish deprecated functions
  from other identifiers.
- Deprecated variables (`gl_FragColor`, `gl_FragData`): legacy colors these as
  `red`. TS does not have captures for built-in variable names.
- The `vec4` function call (type constructor) in `vec4(aPosition, 1.0)`: legacy
  colors it as `yellow` because `vec4` is a keyword. TS colors the type name
  `vec4` only when used as a type specifier; in function-call position, TS does
  not color the `vec4` at the call on line `vec4(aPosition, ...)`.
- Legacy has a very large list of sampler and image types (e.g., `isampler2D`,
  `usampler3D`, `image2DMS`) as keywords. TS relies on the `primitive_type`
  grammar node which may not cover every specialized sampler/image type.
- The colon (`:`) after `case` labels: legacy colors it as `brightcyan`. TS does
  not capture the colon in switch-case context, leaving it as default text.
- String literals (`"..."`) are not commonly used in GLSL shader code. Both
  legacy and TS handle them (`green`) but this is mostly relevant only in
  preprocessor contexts.
- Legacy colors the inner content of preprocessor lines differently (e.g.,
  `#include "file"` shows `"file"` as `red` inside the brightred context). TS
  captures the whole `preproc_directive` node uniformly as `brightred`.
- The `B2` identifier after `buffer` on the nested buffer declaration: legacy
  incorrectly enters a string-like context (shows `RED`). TS leaves it as
  default text, which is correct behavior.
