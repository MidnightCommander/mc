CMake syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `cmake.cmake`
Legacy reference: `misc/syntax/cmake.syntax`
TS query: `misc/syntax-ts/queries-override/cmake-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[cmake]`

Aligned with legacy
-------------------

- Control flow keywords (`if`, `elseif`, `else`, `endif`, `foreach`,
  `endforeach`, `while`, `endwhile`, `function`, `endfunction`, `macro`,
  `endmacro`): `brightred` - MATCH
- Normal commands (`cmake_minimum_required`, `project`, `set`, `message`,
  `find_package`, `find_library`, `add_library`, `add_executable`,
  `target_link_libraries`, `install`, `include`, `export`, `configure_file`,
  `add_test`, `enable_testing`, `option`, `string`, `math`,
  `target_compile_definitions`, `target_compile_options`,
  `target_include_directories`): `brightred` - MATCH
- Comments (line `#...`): `brown` - MATCH
- Bracket comments (`#[[ ... ]]`): `brown` - MATCH
- Strings (quoted `"..."`): `green` - MATCH
- Variable references (`${...}`): `brightgreen` - MATCH Both legacy and TS color
  `${VAR}` as `brightgreen` inside strings and outside strings.
- Parentheses (`(`, `)`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- `block`/`endblock` keywords: TS colors these as `brightred` via
  `@function.special`. Legacy does not include `block`/`endblock` in its keyword
  list, so they appear uncolored.
- Bracket comments: TS correctly colors the entire bracket comment block as
  `brown`. Legacy partially colors bracket comments -- it colors the `#[[` line
  as a comment but leaks the word `documentation` as `brightmagenta` because it
  matches the module keyword list.
- Unquoted arguments: TS colors all unquoted arguments uniformly as `lightgray`
  (`@variable`). Legacy only colors specific known arguments from hardcoded
  lists: known CMAKE_* variables as `brightgreen`, known properties as `white`,
  known modules as `brightmagenta`, leaving others uncolored or miscolored.
- ALL_CAPS arguments (e.g., `VERSION`, `PROPERTIES`, `STATIC`, `PRIVATE`,
  `PUBLIC`, `REQUIRED`, `STATUS`): TS applies `@tag.special` (`white`) via a
  regex match `^[A-Z][A-Z\d_]*$`. However, the TS output shows these as
  `lightgray` (same as other `@variable`), suggesting the `#match?` predicate
  may not be applying in all cases. Legacy colors a hardcoded subset of
  properties as `white`.

Known shortcomings
------------------

- TS does not distinguish between CMake modules (e.g., `GNUInstallDirs`,
  `CMakePackageConfigHelpers`, `CPack`) and regular unquoted arguments. Legacy
  colors these as `brightmagenta` via a hardcoded module list. TS shows them as
  `lightgray` like any other argument.
- TS does not color known CMAKE_* variables (e.g., `CMAKE_BUILD_TYPE`,
  `CMAKE_INSTALL_PREFIX`) differently. Legacy has a large hardcoded list
  coloring them as `brightgreen`. TS treats them as regular `@variable`
  (`lightgray`).
- TS does not color compatibility/deprecated commands (e.g., `exec_program`,
  `subdirs`, `write_file`) in `red` as legacy does. These appear as regular
  `brightred` commands in TS.
- The `@tag.special` capture for ALL_CAPS unquoted arguments does not seem to
  override `@variable` in all cases. In the TS dump, keywords like `VERSION`,
  `SOVERSION`, `PROPERTIES`, `STATIC`, `PRIVATE`, etc. appear as `lightgray`
  rather than the expected `white`. This may be a precedence issue with the
  `#match?` predicate.
- Variable references outside of `${}` but inside command arguments (e.g.,
  `${TARGET}` without proper braces) show as `lightgray` in TS. Legacy
  recognizes the `${*}` glob pattern more broadly.
- The `FILE` keyword inside `export(TARGETS mylib FILE ...)` is colored
  `brightred` in legacy (matching the command keyword list) but `lightgray` in
  TS (treated as a regular unquoted argument).
