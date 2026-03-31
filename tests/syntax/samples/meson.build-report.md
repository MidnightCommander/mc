Meson syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `meson.build`
Legacy reference: `misc/syntax/meson.syntax`
TS query: `misc/syntax-ts/queries-override/meson-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[meson]`

Aligned with legacy
-------------------

- Control flow keywords (`if`, `elif`, `else`, `endif`, `foreach`, `endforeach`,
  `and`, `or`, `not`): `yellow` - MATCH
- Boolean literals (`true`, `false`): `yellow` - MATCH
- Single-quoted strings (`'...'`): `green` - MATCH
- Comments (`# ...`): `brown` - MATCH
- Function calls (`project`, `dependency`, `executable`, `files`,
  `include_directories`, `message`, `warning`, `add_project_arguments`,
  `configuration_data`, `configure_file`, `shared_library`, `static_library`,
  `subdir`, `custom_target`, `find_program`, `get_option`, `run_command`,
  `install_data`, `install_headers`, `test`, `summary`): `white` - MATCH

Intentional improvements over legacy
-------------------------------------

- Dictionary keys (`version`, `license`, `default_options`, `required`,
  `language`, `dependencies`, `include_directories`, `input`, `output`,
  `configuration`, `install`, `install_dir`, `command`, `section`, `check`): TS
  colors these as `yellow` via `property.key`. Legacy does not distinguish
  dictionary keys from regular identifiers, leaving them as default text. This
  is a significant improvement for readability of meson build files where
  keyword arguments are very common.
- Method calls on objects (`meson.get_compiler`, `meson.get_option`,
  `meson.current_source_dir`, `host_machine.system`, `compiler.find_library`,
  `name.to_upper`, `conf.set`, `conf.set10`, `result.stdout`, `result.strip`,
  `zlib_dep.found`): TS colors the method name as `white` via `normal_command`.
  Legacy only colors standalone function names from its hardcoded list, missing
  method calls entirely.
- Built-in objects (`meson`, `host_machine`, `build_machine`, `target_machine`):
  legacy colors these as `yellow` via keyword matching. TS now colors them as
  `yellow` via `variable.builtin` using `#any-of?` predicate on `(identifier)`.
  TS also colors the method calls on these objects as `white`.
- Multi-line strings (`'''...'''`): both TS and legacy handle these as string
  contexts colored `green`. TS uses the tree-sitter string node which correctly
  spans multiple lines.

Known shortcomings
------------------

- Built-in type names (`compiler`, `string`, `Number`, `boolean`, `array`,
  `dictionary`): legacy colors all of these as `yellow` via keyword matching. TS
  does not color them as they are generic identifiers and would cause false
  positives. The four primary built-in objects (`meson`, `host_machine`,
  `build_machine`, `target_machine`) are now handled via `#any-of?`.
- Double-quoted strings (`"..."`): legacy colors these as `brightred` (a
  deliberate distinction from single-quoted strings). TS colors all strings
  uniformly as `green` via the `string` capture, losing this distinction.
- The `summary` function call: TS colors it as `white` via the function call
  capture. Legacy also colors it as `white` but does not have `summary` in its
  function keyword list, so legacy leaves it uncolored. TS is better here.
- Legacy hardcodes a specific list of known functions (`project`, `executable`,
  `dependency`, etc.). TS colors any function call as `white` via the
  `normal_command` grammar node, which is more flexible and future-proof but
  less selective.
- The `break` and `continue` keywords: TS captures these via `keyword_break` and
  `keyword_continue` nodes as `yellow`. The sample file does not exercise these
  but they are in the TS query.
- Comparison operators (`==`): neither legacy nor TS color operators in meson.
  They appear as default text in both.
