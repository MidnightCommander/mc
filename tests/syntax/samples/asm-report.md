Assembly syntax highlighting: TS vs Legacy comparison report
==============================================================

Sample file: `asm.asm`
Legacy reference: `misc/syntax/assembler.syntax`
TS query: `misc/syntax-ts/queries-override/asm-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[asm]`

Aligned with legacy
-------------------

- Line comments (`; ...`): `brown` (TS `comment`) - MATCH.
- Block comments (`/* ... */`): `brown` (TS `comment`) - MATCH.
- Strings (`"..."`): `green` (TS `string`) - MATCH.
- Labels (`_start:`, `print_string:`, `.large_value:`): `cyan` (TS `label`) -
  MATCH.
- Instructions/directives (`section`, `global`, `extern`, `db`, `dw`, `dd`,
  `dq`, `equ`, `align`, `times`, `resb`, `resd`): `white` (TS `keyword.other`) -
  MATCH (legacy also uses `white` for directives and pseudo-instructions).
- Pointer size keywords (`byte`, `word`, `dword`, `qword`): `white` (TS
  `keyword.other`) - MATCH.

Intentional improvements over legacy
-------------------------------------

- TS uses the grammar's structural `reg` node to identify all registers
  uniformly; legacy requires exhaustive per-register keyword lists (hundreds of
  entries for 16/32/64-bit, FPU, SSE, etc.).
- TS highlights registers in instruction operands as `brightmagenta`
  (`keyword.control`) consistently via the `reg` node; legacy achieves the same
  color but only for explicitly listed registers.
- TS highlights identifiers used as memory references (e.g., `msg`, `buffer`,
  `result` inside `[...]`) as `lightgray` (`constant`); legacy colors these as
  default `lightgray`.
- TS handles the instruction node as a whole unit with `white`
  (`keyword.other`), covering all mnemonics without needing individual keyword
  entries.

Known shortcomings
------------------

- TS does not distinguish registers by type: general-purpose registers (`eax`,
  `ebx`, etc.) and SSE registers (`xmm0`, `xmm1`, etc.) both get `lightgray` in
  TS when used as plain operands outside `[...]` context. Legacy colors
  general-purpose registers as `brightmagenta` and SSE/FPU registers as
  `brightcyan`, providing more visual distinction.
- The TS grammar's `reg` node only fires for registers inside memory references
  (`[ebp + 8]`, `[rax]`), coloring them as `brightmagenta`; registers used as
  direct operands (`mov eax, 42`) show as `lightgray` (default) in TS, whereas
  legacy colors them `brightmagenta` everywhere.
- TS does not highlight NASM preprocessor directives (`%ifdef`, `%else`,
  `%endif`, `%define`) as `brightred` like legacy does; TS shows `%` as `RED`
  (error) and the directive name as `white`/`lightgray`.
- TS does not color `$` and `$$` as `brightgreen` like legacy does for
  current-address references.
- GAS section names (`.data`, `.text`, `.bss`, `.rodata`) are not highlighted in
  TS; legacy colors them as `brightblue`.
- Legacy highlights `%%` (local label prefix) as `cyan`; TS does not
  specifically handle this.
- Commas between operands show as `RED` (error) in some TS contexts, suggesting
  the grammar may have parsing issues with certain instruction formats.
