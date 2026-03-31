Rust syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `rust.rs`
Legacy reference: `misc/syntax/rust.syntax`
TS query: `misc/syntax-ts/queries-override/rust-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[rust]`

Aligned with legacy
-------------------

- Language keywords (`as`, `async`, `await`, `break`, `const`, `continue`,
  `dyn`, `else`, `enum`, `extern`, `fn`, `for`, `if`, `impl`, `in`, `let`,
  `loop`, `match`, `mod`, `move`, `pub`, `ref`, `return`, `static`, `struct`,
  `trait`, `type`, `unsafe`, `use`, `where`, `while`): `yellow` - MATCH
- `crate`, `super`, `mut` as keywords: `yellow` - MATCH
- `self`: `brightgreen` - MATCH
- Boolean literals (`true`, `false`): `brightgreen` - MATCH
- Enum variants `Some`, `None`, `Ok`, `Err`: `brightgreen` - MATCH
- Primitive types (`bool`, `char`, `i8`...`i64`, `u8`...`u64`, `f32`, `f64`,
  `isize`, `usize`, `str`): `brightcyan` - MATCH
- Type identifiers (`String`, `Vec`, `Option`, `Result`, `HashMap`, `Point`,
  `Shape`, `Drawable`, etc.): `brightcyan` - MATCH
- Macros (`println!`, `eprintln!`, `format!`, `vec!`, `assert!`, `assert_eq!`,
  `dbg!`, `todo!`, `write!`): `brightmagenta` - MATCH
- `macro_rules!` definition name: `brightmagenta` - MATCH
- Strings (double-quoted `"..."`): `green` - MATCH
- Raw strings (`r#"..."#`): `green` - MATCH
- Char literals (`'A'`, `'\n'`): `brightgreen` - MATCH
- Number literals (decimal, hex, octal, binary, float, scientific,
  underscore-separated): `brightgreen` - MATCH
- Comments (line `//` and block `/* */`): `brown` - MATCH
- Attributes (`#[derive(...)]`, `#[cfg(test)]`, `#[test]`): `white` via
  `tag.special` capture on `#` - MATCH
- Labels (`'outer`): `cyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- All type identifiers (user-defined types like `Point`, `Shape`, `Drawable`,
  `Tagged`, `Formatter`, `Error`, `Box`, `Clone`) are colored as `brightcyan`
  via the `type_identifier` capture. Legacy only colors a hardcoded list of
  common types (`String`, `Vec`, `Option`, `Result`, plus primitives). TS gives
  consistent type coloring for all types.
- `Self` (capitalized) as a type: TS colors it `brightcyan` via
  `type_identifier`. Legacy colors it as `yellow` (keyword). TS is more
  semantically correct since `Self` refers to a type.
- `async`/`await`/`dyn`/`yield` keywords: TS includes these in the keyword list
  (`yellow`). Legacy does not list `async`, `await`, or `dyn` (they would appear
  uncolored). `yield` is colored `red` by legacy (reserved keyword) vs `yellow`
  by TS.
- Attribute content after `#`: TS only colors the `#` as `white` and lets the
  rest be default. Legacy colors the entire `#[...]` or `#![...]` context as
  `white`. TS is more granular.
- `macro_rules!` body: TS colors `$x` metavariables via the grammar (the
  upstream grammar handles these). Legacy colors `$+` patterns as `brightblue`
  via a keyword match. TS does not replicate the `brightblue` for metavariables
  but the macro name coloring is consistent.

Known shortcomings
------------------

- Reserved keywords (`abstract`, `become`, `box`, `do`, `final`, `macro`,
  `override`, `priv`, `sizeof`, `typeof`, `unsized`, `virtual`): legacy colors
  these as `red` to flag them. TS does not distinguish reserved-for-future
  keywords; they appear uncolored. This is a minor loss of feedback for code
  using reserved words.
- `yield` keyword: legacy colors it `red` (reserved). TS colors it `yellow`
  (regular keyword). Since `yield` is now a real keyword in nightly Rust, the TS
  approach is more forward-looking.
- Lifetime annotations (`'_` in `Formatter<'_>`): TS does not have a specific
  capture for lifetimes. Legacy does not color them either. Both leave lifetimes
  as default text (except when they appear as labels).
- Attribute brackets `[...]` after `#`: TS only colors the `#` as `white`. The
  rest of the attribute (including `[`, `]`, and content like `derive`, `cfg`)
  appears as default. Legacy colors the entire `#[...]` block as `white`. This
  means attribute content is less visible in TS.
- Range operator `..`: neither TS nor legacy explicitly colors the `..` range
  operator. It appears as default text in both.
