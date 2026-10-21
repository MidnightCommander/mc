# mc-syntax-dump

Dump MC syntax highlighting as ANSI-colored text.

Uses MC's actual syntax engine internals to produce the exact same
colors that mcedit would show for a given file.

## Building

From the MC build directory (after running `./configure` and `make`):

```bash
make -C tests/syntax
```

This builds the `mc-syntax-dump` binary using the Makefile in
`tests/syntax/`. The tool links against MC's `libinternal.la` and
`libmc.la` to use the real `edit_get_syntax_color()` path.

## Usage

```text
tests/syntax/mc-syntax-dump [--ts|--legacy] <source-file>
```

- `--ts` — force tree-sitter highlighting
- `--legacy` — force legacy regex-based highlighting

If neither flag is given, tree-sitter is tried first and falls back
to legacy. The output is the source file with ANSI color escape
sequences on stdout. Diagnostic messages go to stderr.

## Examples

Compare legacy and tree-sitter highlighting for a bash script:

```bash
tests/syntax/mc-syntax-dump --legacy tests/syntax/samples/bash.sh \
    > /tmp/legacy.txt
tests/syntax/mc-syntax-dump --ts tests/syntax/samples/bash.sh \
    > /tmp/ts.txt

cat /tmp/legacy.txt    # view legacy colors
cat /tmp/ts.txt        # view tree-sitter colors
```

## Prerequisites

- MC must be compiled with tree-sitter support (`--with-tree-sitter`)
- Grammar `.so` files must be installed in
  `~/.local/lib/mc/ts-grammars/`
- Syntax files must be present in `~/.local/share/mc/syntax/` (copy
  from `/usr/share/mc/syntax/` if needed)
- Query overrides and `colors.ini` in `~/.local/share/mc/syntax-ts/`

## Sample files

The `samples/` subdirectory contains sample source files that exercise
all syntax features of each language. These are used to compare legacy
and tree-sitter highlighting output. Each sample has a corresponding
`*-report.md` file documenting the comparison results.
