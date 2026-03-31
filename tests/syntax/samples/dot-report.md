DOT/Graphviz syntax highlighting: TS vs Legacy comparison
==========================================================

Sample file: `dot.dot`
Legacy reference: `misc/syntax/dot.syntax`
TS query: `misc/syntax-ts/queries-override/dot-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[dot]`

Aligned with legacy
-------------------

- Graph keywords (`digraph`, `graph`, `subgraph`): `brightred` - MATCH
- Node/edge keywords (`node`, `edge`): `yellow` - MATCH
- Edge operators (`->`, `--`): `brightred` - MATCH
- Attribute names (`rankdir`, `label`, `fontname`, `fontsize`, `bgcolor`,
  `splines`, `nodesep`, `shape`, `style`, `fillcolor`, `color`, `arrowhead`,
  `arrowsize`, `penwidth`, `tooltip`, `constraint`, `dir`, `weight`, `width`):
  `white` - MATCH
- Assignment operator (`=`): `yellow` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Brackets (`{`, `}`, `[`, `]`): `brightcyan` - MATCH
- Commas (`,`): `brightcyan` - MATCH
- Semicolons (`;`): `brightmagenta` - MATCH
- Preprocessor (`#include`): `brightred` - MATCH

Intentional improvements over legacy
-------------------------------------

- HTML strings (angle-bracket `<...>`): TS colors the entire HTML string as
  `green` via `@string`, with HTML tags recognized internally. Legacy colors
  angle brackets as `green` and recognizes `<TAG>` elements as `cyan` within the
  HTML content. TS provides structural HTML parsing (attribute names as
  `yellow`, attribute values as `cyan`, tags as `brightcyan`).
- Cluster subgraph names: legacy colors `cluster_` prefix as `yellow` via
  `wholeleft cluster_`. TS does not special-case the `cluster_` prefix -- the
  entire identifier after `subgraph` appears uncolored (default).
- Unquoted attribute values (e.g., `box`, `ellipse`, `diamond`, `true`,
  `dashed`, `LR`): legacy does not color these (default text). TS also does not
  color them, but they appear as `red` in the TS dump. This is because the TS
  DOT grammar marks uncaptured text in red as a fallback -- this is a rendering
  artifact, not an intentional color.

Known shortcomings
------------------

- TS shows `red` coloring for node identifiers (e.g., `SampleGraph`, `start`,
  `process_a`, `process_b`, `decision`, `end_node`, `api_server`, etc.) and
  unquoted attribute values (e.g., `LR`, `box`, `ellipse`, `14`, `0.8`,
  `normal`, `true`, `dashed`, `back`, `blue`). Legacy leaves these as default
  (uncolored). The `red` appears to be a fallback color for uncaptured nodes in
  the TS grammar. This is a regression -- identifiers and unquoted values should
  appear in default text color.
- The `strict` keyword is defined in the TS query but not exercised in the
  sample (no `strict graph` or `strict digraph`). Legacy also defines `strict`
  as `brightred`.
- TS does not recognize the `cluster_` prefix specially. Legacy colors
  `cluster_` as `yellow` via `wholeleft` matching, which helps visually identify
  cluster subgraphs. TS treats `cluster_services` and `cluster_frontend` as
  plain identifiers.
- The undirected `graph` keyword at the top level does not get
  `@function.special` coloring in TS when it is the first token on a line
  outside of a digraph block. In the sample output, the last `graph
  UndirectedSample` line shows `graph` without color in the TS dump. This
  appears to be a parsing issue.
- The `operator` node type for `=` is captured as `@operator.word` (`yellow`) in
  both, but the `:` is defined in the legacy syntax as `brightcyan` while TS
  does not separately capture `:` (it is not listed in the query).
