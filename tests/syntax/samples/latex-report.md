LaTeX syntax highlighting: TS vs Legacy comparison report
===========================================================

Sample file: `latex.tex`
Legacy reference: `misc/syntax/latex.syntax`
TS query: `misc/syntax-ts/queries-override/latex-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[latex]`

Aligned with legacy
-------------------

- Line comments (`% ...`): `brown` (TS `comment`) - MATCH.
- Strings/paths in package includes: `green` (TS `string`) - MATCH.
- Math environments (`$...$`, `equation`, `displaymath`): `brightgreen` (TS
  `markup.math`) - MATCH.
- `\begin`/`\end` commands: both use bright colors (legacy `brightred` for known
  environments, `brightcyan` for others; TS `brightcyan` via `module`).
- Sectioning command names (`\section`, `\chapter`, `\part`): both highlight
  prominently (legacy `brightred`; TS `brightcyan` via `module`).
- `\item`: highlighted distinctly (legacy `yellow`; TS `brightmagenta` via
  `punctuation.special`).
- `\newcommand`/`\newenvironment` definitions: highlighted (legacy `cyan`; TS
  `brightmagenta` via `function.macro`).
- `\label`, `\ref`, `\cite`: highlighted (legacy `yellow`; TS `brightmagenta`
  for command, `yellow` for label/link target).
- Brackets `{}[]`: `brightcyan` (TS `punctuation.bracket`) - legacy also uses
  `brightgreen` for `{}`.

Intentional improvements over legacy
-------------------------------------

- TS provides granular sectioning depth: section titles get `brightmagenta`
  (`markup.heading.N`) with different heading levels (1-6), while legacy colors
  all section titles the same `brightred`.
- TS distinguishes `\emph` (italic, `magenta`) from `\textbf` (bold,
  `brightmagenta`); legacy colors both via generic command rules.
- TS highlights environment names (e.g., `document`, `itemize`, `equation`) in
  `cyan` (`label`), separate from the `\begin`/`\end` command in `brightcyan`
  (`module`).
- TS identifies `\href` URLs as `yellow` (`markup.link.url`), giving hyperlinks
  distinct color.
- TS highlights operators (`=`, `^`, `_`) as `brightcyan` in math mode; legacy
  has no operator distinction.
- TS highlights `\documentclass`, `\usepackage` as `yellow` (`keyword.import`)
  with their arguments as `green` (`string`); legacy uses generic `brightcyan`.
- TS identifies math delimiters (`\left`, `\right`, `\frac`) distinctly as
  `brightcyan` (`punctuation.delimiter`).
- TS highlights conditional commands (`\ifx`, `\else`, `\fi`) as `yellow`
  (`keyword.conditional`); legacy uses generic `brightcyan`.
- TS colors `\author` content as heading text (`brightmagenta`), giving
  title-page elements visual weight.
- TS colors key-value pairs with `lightgray` for parameter names
  (`variable.parameter`).

Known shortcomings
------------------

- TS does not reproduce the legacy `{\\bf ...}` context mode which colors
  bold-braced content in `brightmagenta` as a continuous block.
- TS does not reproduce the legacy `{\\it ...}` context mode which colors
  italic-braced content in `magenta` as a continuous block.
- Legacy `\\pagenumbering` context with keyword highlighting (`arabic`, `roman`,
  etc.) inside braces is not specifically handled in TS.
- TS `\frametitle` requires the full beamer grammar context to work; in a plain
  article document the frametitle capture may not trigger.
- Font size commands (`\tiny`, `\huge`, etc.) are colored as generic
  `brightcyan` (`function`) in TS; legacy specifically colors them as `yellow`.
- TS colors the `%%!TeX` directive line as `brown` (`comment`); legacy does not
  specifically distinguish it (also `brown` via `%` comment context, though the
  `%%` keyword is `yellow`).
