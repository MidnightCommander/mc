COBOL syntax highlighting: TS vs Legacy comparison report
==========================================================

Sample file: `cobol.cob`
Legacy reference: `misc/syntax/cobol.syntax`
TS query: `misc/syntax-ts/queries-override/cobol-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[cobol]`

Aligned with legacy
-------------------

- Division headings (`IDENTIFICATION DIVISION.`, `ENVIRONMENT DIVISION.`, `DATA
  DIVISION.`, `PROCEDURE DIVISION.`): `cyan` - MATCH Both legacy and TS color
  these as `cyan`.
- General keywords (`PROGRAM-ID.`, `AUTHOR.`, `DATE-WRITTEN.`, `CONFIGURATION`,
  `SECTION.`, `SOURCE-COMPUTER.`, `OBJECT-COMPUTER.`, `INPUT-OUTPUT`,
  `FILE-CONTROL.`, `ASSIGN`, `TO`, `IS`, `SEQUENTIAL`, `ACCESS`, `MODE`, `FILE`,
  `STATUS`, `DATA`, `FD`, `PIC`, `VALUE`, `WORKING-STORAGE`, `ADD`, `SUBTRACT`,
  `MULTIPLY`, `DIVIDE`, `COMPUTE`, `IF`, `ELSE`, `END-IF`, `EVALUATE`, `WHEN`,
  `END-EVALUATE`, `PERFORM`, `VARYING`, `FROM`, `BY`, `UNTIL`, `MOVE`,
  `DISPLAY`, `STRING`, `DELIMITED`, `INTO`, `END-STRING`, `STOP`, `RUN`, `TRUE`,
  `OTHER`, `NOT`, `AT`, `END`, `GIVING`, `REMAINDER`, `THRU`, `SPACES`, `SIZE`,
  `INPUT`): `yellow` - MATCH
- I/O keywords (`OPEN`, `READ`, `END-READ`, `CLOSE`, `SELECT`): `brightred` -
  MATCH
- Comments (`*` in column 7): `brown` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Level number prefix `S9` in PIC: `brightgreen` - MATCH Legacy has `s9` and
  `v9` as `brightgreen` keywords.
- Edited PIC prefix `$ZZZ`: `brightgreen` in both, though this is coincidental
  -- legacy matches `$ZZZ` via the `$+` pattern, TS would capture via
  `picture_edit`.

The legacy and TS outputs are identical
---------------------------------------

The legacy and TS syntax dumps produce exactly the same coloring for the entire
sample file. Every keyword, string, comment, division heading, I/O verb, and PIC
clause matches perfectly between the two engines. This indicates the TS grammar
and query are very well aligned with the legacy syntax file.

Known shortcomings
------------------

- TS query defines captures for `level_number`, `level_number_88`, `picture_x`,
  `picture_9`, `picture_a`, `picture_n`, and `picture_edit` as `@string.special`
  (`brightgreen`), but the tree-sitter COBOL parser may not produce these node
  types for all PIC patterns. In the sample, level numbers like `01`, `05`, `88`
  appear uncolored in both engines.
- TS query defines `integer`, `decimal`, and `number` captures as `@constant`
  (`lightgray`), but numeric literals in the sample (e.g., `100`, `50`, `1000`)
  appear uncolored in both engines.
- TS query defines `qualified_word` as `@constant` (`lightgray`), but qualified
  data names in the sample do not appear to receive this coloring.
- The `x_string`, `h_string`, and `n_string` captures in the TS query are meant
  to color hex strings (`X"..."`) and national strings (`N"..."`). In the
  sample, the prefix `X` and `N` before the string are uncolored in both engines
  -- only the quoted portion gets `green`.
- The COBOL legacy syntax file appears to be based on a shell syntax template
  (it contains shell-specific patterns like `$()`, `${}`, backticks, heredocs,
  etc.) rather than being a pure COBOL syntax definition. This does not affect
  the COBOL-specific keyword highlighting but means the legacy file has many
  irrelevant rules.
