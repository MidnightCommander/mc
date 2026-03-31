SQL syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `sql.sql`
Legacy reference: `misc/syntax/sql.syntax`
TS query: `misc/syntax-ts/queries-override/sql-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[sql]`

Aligned with legacy
-------------------

- Single-line comments (`-- ...`): `brown` (TS `comment`) - MATCH.
- Block comments (`/* ... */`): `brown` (TS `comment`) - MATCH.
- Strings (`'...'`): `green` (TS `string`) - MATCH.
- Operators (`=`, `<`, `>`, `+`, `-`, `*`, `/`, `%`, `>=`, `<=`, `<>`, `!=`):
  `brightcyan` (TS `operator`) - MATCH.
- Delimiters (`;`, `,`, `(`, `)`): `brightcyan` (TS `delimiter`) - MATCH.
- Data types (`INTEGER`, `VARCHAR`, `DECIMAL`, `TEXT`, `BLOB`, `DATE`,
  `TIMESTAMP`, `BOOLEAN`, `INT`, `NUMERIC`): `yellow` (TS `type`) - MATCH.

Intentional improvements over legacy
-------------------------------------

- TS query is minimal and focused: it highlights types, comments, strings,
  operators, and delimiters, letting the grammar handle keyword recognition
  structurally rather than via a long keyword list.
- TS correctly handles multi-line block comments as a single unit; legacy uses
  context-based matching which can occasionally misfire with nested comment
  markers.

Known shortcomings
------------------

- TS does not highlight SQL keywords (`SELECT`, `INSERT`, `UPDATE`, `DELETE`,
  `CREATE`, `FROM`, `WHERE`, `JOIN`, `ORDER`, `GROUP`, `HAVING`, `UNION`,
  `BEGIN`, `COMMIT`, etc.) in any color; legacy colors all of these as `yellow`.
  This is the most significant gap -- SQL keywords appear as default/uncolored
  in TS.
- Several lines show unexpected `RED` coloring in TS output (e.g., around
  `WHERE department_id IN (`, `FROM employees WHERE EXISTS (`), likely from the
  TS grammar producing error nodes for certain subquery patterns; legacy handles
  these correctly.
- TS does not highlight `LIKE` as a keyword; legacy colors it `yellow`.
- TS does not distinguish aggregate functions (`COUNT`, `AVG`, `MAX`, `MIN`,
  `SUM`) or window functions (`RANK`, `OVER`); legacy colors these as `yellow`
  keywords.
- TS does not color `AS`, `AND`, `OR`, `NOT`, `IN`, `EXISTS`, `CASE`, `WHEN`,
  `THEN`, `ELSE`, `END`, `SET`, `VALUES`, `INTO`, `ON`, `BY`, `DESC`, `ASC`,
  `LIMIT`, `HAVING`, `DISTINCT`, `UNION`, `ALTER`, `DROP`, `INDEX`, `IF`,
  `DEFAULT`, `PRIMARY`, `KEY`, `UNIQUE`, `NULL`, `TRUE`, `FALSE` -- all of which
  legacy highlights as `yellow`.
- The `.` (dot) in table-qualified names (e.g., `e.salary`) shows as `white` in
  legacy; TS does not specifically handle dot notation.
- Legacy has MySQL-specific `#` comment support and backtick-quoted identifier
  support; TS relies solely on the grammar's comment/string handling.
