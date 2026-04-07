/*
   Static registry of tree-sitter grammars.

   This file declares all tree_sitter_*() functions from the grammar sources
   and provides a lookup table mapping grammar names to their corresponding
   language function pointers.

   Each grammar is guarded by HAVE_GRAMMAR_<UPPER> defines, which are set by
   configure based on --with-tree-sitter-grammars=LIST.  Only selected grammars
   are compiled and registered.

   Used by syntax.c to find grammars at compile time instead of dlopen().
 */

#ifndef MC__TS_GRAMMAR_REGISTRY_H
#define MC__TS_GRAMMAR_REGISTRY_H

#include <tree_sitter/api.h>

/*** Forward declarations of grammar functions (conditional) ***/

#ifdef HAVE_GRAMMAR_ADA
extern const TSLanguage *tree_sitter_ada (void);
#endif
#ifdef HAVE_GRAMMAR_AGDA
extern const TSLanguage *tree_sitter_agda (void);
#endif
#ifdef HAVE_GRAMMAR_ASM
extern const TSLanguage *tree_sitter_asm (void);
#endif
#ifdef HAVE_GRAMMAR_ASTRO
extern const TSLanguage *tree_sitter_astro (void);
#endif
#ifdef HAVE_GRAMMAR_AWK
extern const TSLanguage *tree_sitter_awk (void);
#endif
#ifdef HAVE_GRAMMAR_BASH
extern const TSLanguage *tree_sitter_bash (void);
#endif
#ifdef HAVE_GRAMMAR_BIBTEX
extern const TSLanguage *tree_sitter_bibtex (void);
#endif
#ifdef HAVE_GRAMMAR_BICEP
extern const TSLanguage *tree_sitter_bicep (void);
#endif
#ifdef HAVE_GRAMMAR_BISON
extern const TSLanguage *tree_sitter_bison (void);
#endif
#ifdef HAVE_GRAMMAR_BLUEPRINT
extern const TSLanguage *tree_sitter_blueprint (void);
#endif
#ifdef HAVE_GRAMMAR_C
extern const TSLanguage *tree_sitter_c (void);
#endif
#ifdef HAVE_GRAMMAR_CADDY
extern const TSLanguage *tree_sitter_caddy (void);
#endif
#ifdef HAVE_GRAMMAR_CAIRO
extern const TSLanguage *tree_sitter_cairo (void);
#endif
#ifdef HAVE_GRAMMAR_CLOJURE
extern const TSLanguage *tree_sitter_clojure (void);
#endif
#ifdef HAVE_GRAMMAR_CMAKE
extern const TSLanguage *tree_sitter_cmake (void);
#endif
#ifdef HAVE_GRAMMAR_COBOL
extern const TSLanguage *tree_sitter_COBOL (void);
#endif
#ifdef HAVE_GRAMMAR_COMMONLISP
extern const TSLanguage *tree_sitter_commonlisp (void);
#endif
#ifdef HAVE_GRAMMAR_CPP
extern const TSLanguage *tree_sitter_cpp (void);
#endif
#ifdef HAVE_GRAMMAR_C_SHARP
extern const TSLanguage *tree_sitter_c_sharp (void);
#endif
#ifdef HAVE_GRAMMAR_CSS
extern const TSLanguage *tree_sitter_css (void);
#endif
#ifdef HAVE_GRAMMAR_CSV
extern const TSLanguage *tree_sitter_csv (void);
#endif
#ifdef HAVE_GRAMMAR_CUDA
extern const TSLanguage *tree_sitter_cuda (void);
#endif
#ifdef HAVE_GRAMMAR_CUE
extern const TSLanguage *tree_sitter_cue (void);
#endif
#ifdef HAVE_GRAMMAR_D
extern const TSLanguage *tree_sitter_d (void);
#endif
#ifdef HAVE_GRAMMAR_DART
extern const TSLanguage *tree_sitter_dart (void);
#endif
#ifdef HAVE_GRAMMAR_DEVICETREE
extern const TSLanguage *tree_sitter_devicetree (void);
#endif
#ifdef HAVE_GRAMMAR_DHALL
extern const TSLanguage *tree_sitter_dhall (void);
#endif
#ifdef HAVE_GRAMMAR_DIFF
extern const TSLanguage *tree_sitter_diff (void);
#endif
#ifdef HAVE_GRAMMAR_DOCKERFILE
extern const TSLanguage *tree_sitter_dockerfile (void);
#endif
#ifdef HAVE_GRAMMAR_DOT
extern const TSLanguage *tree_sitter_dot (void);
#endif
#ifdef HAVE_GRAMMAR_EARTHFILE
extern const TSLanguage *tree_sitter_earthfile (void);
#endif
#ifdef HAVE_GRAMMAR_EDITORCONFIG
extern const TSLanguage *tree_sitter_editorconfig (void);
#endif
#ifdef HAVE_GRAMMAR_ELIXIR
extern const TSLanguage *tree_sitter_elixir (void);
#endif
#ifdef HAVE_GRAMMAR_ELM
extern const TSLanguage *tree_sitter_elm (void);
#endif
#ifdef HAVE_GRAMMAR_ERLANG
extern const TSLanguage *tree_sitter_erlang (void);
#endif
#ifdef HAVE_GRAMMAR_FENNEL
extern const TSLanguage *tree_sitter_fennel (void);
#endif
#ifdef HAVE_GRAMMAR_FISH
extern const TSLanguage *tree_sitter_fish (void);
#endif
#ifdef HAVE_GRAMMAR_FORTH
extern const TSLanguage *tree_sitter_forth (void);
#endif
#ifdef HAVE_GRAMMAR_FORTRAN
extern const TSLanguage *tree_sitter_fortran (void);
#endif
#ifdef HAVE_GRAMMAR_GDSCRIPT
extern const TSLanguage *tree_sitter_gdscript (void);
#endif
#ifdef HAVE_GRAMMAR_GDSHADER
extern const TSLanguage *tree_sitter_gdshader (void);
#endif
#ifdef HAVE_GRAMMAR_GITATTRIBUTES
extern const TSLanguage *tree_sitter_gitattributes (void);
#endif
#ifdef HAVE_GRAMMAR_GITIGNORE
extern const TSLanguage *tree_sitter_gitignore (void);
#endif
#ifdef HAVE_GRAMMAR_GLEAM
extern const TSLanguage *tree_sitter_gleam (void);
#endif
#ifdef HAVE_GRAMMAR_GLSL
extern const TSLanguage *tree_sitter_glsl (void);
#endif
#ifdef HAVE_GRAMMAR_GO
extern const TSLanguage *tree_sitter_go (void);
#endif
#ifdef HAVE_GRAMMAR_GRAPHQL
extern const TSLanguage *tree_sitter_graphql (void);
#endif
#ifdef HAVE_GRAMMAR_GROOVY
extern const TSLanguage *tree_sitter_groovy (void);
#endif
#ifdef HAVE_GRAMMAR_HACK
extern const TSLanguage *tree_sitter_hack (void);
#endif
#ifdef HAVE_GRAMMAR_HARE
extern const TSLanguage *tree_sitter_hare (void);
#endif
#ifdef HAVE_GRAMMAR_HASKELL
extern const TSLanguage *tree_sitter_haskell (void);
#endif
#ifdef HAVE_GRAMMAR_HAXE
extern const TSLanguage *tree_sitter_haxe (void);
#endif
#ifdef HAVE_GRAMMAR_HCL
extern const TSLanguage *tree_sitter_hcl (void);
#endif
#ifdef HAVE_GRAMMAR_HEEX
extern const TSLanguage *tree_sitter_heex (void);
#endif
#ifdef HAVE_GRAMMAR_HJSON
extern const TSLanguage *tree_sitter_hjson (void);
#endif
#ifdef HAVE_GRAMMAR_HLSL
extern const TSLanguage *tree_sitter_hlsl (void);
#endif
#ifdef HAVE_GRAMMAR_HTML
extern const TSLanguage *tree_sitter_html (void);
#endif
#ifdef HAVE_GRAMMAR_IDL
extern const TSLanguage *tree_sitter_idl (void);
#endif
#ifdef HAVE_GRAMMAR_INI
extern const TSLanguage *tree_sitter_ini (void);
#endif
#ifdef HAVE_GRAMMAR_JAVA
extern const TSLanguage *tree_sitter_java (void);
#endif
#ifdef HAVE_GRAMMAR_JAVASCRIPT
extern const TSLanguage *tree_sitter_javascript (void);
#endif
#ifdef HAVE_GRAMMAR_JQ
extern const TSLanguage *tree_sitter_jq (void);
#endif
#ifdef HAVE_GRAMMAR_JSON
extern const TSLanguage *tree_sitter_json (void);
#endif
#ifdef HAVE_GRAMMAR_JSON5
extern const TSLanguage *tree_sitter_json5 (void);
#endif
#ifdef HAVE_GRAMMAR_JSONNET
extern const TSLanguage *tree_sitter_jsonnet (void);
#endif
#ifdef HAVE_GRAMMAR_JULIA
extern const TSLanguage *tree_sitter_julia (void);
#endif
#ifdef HAVE_GRAMMAR_JUST
extern const TSLanguage *tree_sitter_just (void);
#endif
#ifdef HAVE_GRAMMAR_KCL
extern const TSLanguage *tree_sitter_kcl (void);
#endif
#ifdef HAVE_GRAMMAR_KDL
extern const TSLanguage *tree_sitter_kdl (void);
#endif
#ifdef HAVE_GRAMMAR_KOTLIN
extern const TSLanguage *tree_sitter_kotlin (void);
#endif
#ifdef HAVE_GRAMMAR_LATEX
extern const TSLanguage *tree_sitter_latex (void);
#endif
#ifdef HAVE_GRAMMAR_LUA
extern const TSLanguage *tree_sitter_lua (void);
#endif
#ifdef HAVE_GRAMMAR_MAKE
extern const TSLanguage *tree_sitter_make (void);
#endif
#ifdef HAVE_GRAMMAR_MARKDOWN
extern const TSLanguage *tree_sitter_markdown (void);
#endif
#ifdef HAVE_GRAMMAR_MARKDOWN_INLINE
extern const TSLanguage *tree_sitter_markdown_inline (void);
#endif
#ifdef HAVE_GRAMMAR_MATLAB
extern const TSLanguage *tree_sitter_matlab (void);
#endif
#ifdef HAVE_GRAMMAR_MESON
extern const TSLanguage *tree_sitter_meson (void);
#endif
#ifdef HAVE_GRAMMAR_MUTTRC
extern const TSLanguage *tree_sitter_muttrc (void);
#endif
#ifdef HAVE_GRAMMAR_NICKEL
extern const TSLanguage *tree_sitter_nickel (void);
#endif
#ifdef HAVE_GRAMMAR_NIM
extern const TSLanguage *tree_sitter_nim (void);
#endif
#ifdef HAVE_GRAMMAR_NIX
extern const TSLanguage *tree_sitter_nix (void);
#endif
#ifdef HAVE_GRAMMAR_NU
extern const TSLanguage *tree_sitter_nu (void);
#endif
#ifdef HAVE_GRAMMAR_OBJC
extern const TSLanguage *tree_sitter_objc (void);
#endif
#ifdef HAVE_GRAMMAR_OCAML
extern const TSLanguage *tree_sitter_ocaml (void);
#endif
#ifdef HAVE_GRAMMAR_ODIN
extern const TSLanguage *tree_sitter_odin (void);
#endif
#ifdef HAVE_GRAMMAR_ORG
extern const TSLanguage *tree_sitter_org (void);
#endif
#ifdef HAVE_GRAMMAR_PASCAL
extern const TSLanguage *tree_sitter_pascal (void);
#endif
#ifdef HAVE_GRAMMAR_PERL
extern const TSLanguage *tree_sitter_perl (void);
#endif
#ifdef HAVE_GRAMMAR_PHP
extern const TSLanguage *tree_sitter_php (void);
#endif
#ifdef HAVE_GRAMMAR_PKL
extern const TSLanguage *tree_sitter_pkl (void);
#endif
#ifdef HAVE_GRAMMAR_PO
extern const TSLanguage *tree_sitter_po (void);
#endif
#ifdef HAVE_GRAMMAR_POWERSHELL
extern const TSLanguage *tree_sitter_powershell (void);
#endif
#ifdef HAVE_GRAMMAR_PRISMA
extern const TSLanguage *tree_sitter_prisma (void);
#endif
#ifdef HAVE_GRAMMAR_PROPERTIES
extern const TSLanguage *tree_sitter_properties (void);
#endif
#ifdef HAVE_GRAMMAR_PROTO
extern const TSLanguage *tree_sitter_proto (void);
#endif
#ifdef HAVE_GRAMMAR_PRQL
extern const TSLanguage *tree_sitter_prql (void);
#endif
#ifdef HAVE_GRAMMAR_PUPPET
extern const TSLanguage *tree_sitter_puppet (void);
#endif
#ifdef HAVE_GRAMMAR_PURESCRIPT
extern const TSLanguage *tree_sitter_purescript (void);
#endif
#ifdef HAVE_GRAMMAR_PYTHON
extern const TSLanguage *tree_sitter_python (void);
#endif
#ifdef HAVE_GRAMMAR_QMLJS
extern const TSLanguage *tree_sitter_qmljs (void);
#endif
#ifdef HAVE_GRAMMAR_R
extern const TSLanguage *tree_sitter_r (void);
#endif
#ifdef HAVE_GRAMMAR_RACKET
extern const TSLanguage *tree_sitter_racket (void);
#endif
#ifdef HAVE_GRAMMAR_RESCRIPT
extern const TSLanguage *tree_sitter_rescript (void);
#endif
#ifdef HAVE_GRAMMAR_ROBOT
extern const TSLanguage *tree_sitter_robot (void);
#endif
#ifdef HAVE_GRAMMAR_ROC
extern const TSLanguage *tree_sitter_roc (void);
#endif
#ifdef HAVE_GRAMMAR_RON
extern const TSLanguage *tree_sitter_ron (void);
#endif
#ifdef HAVE_GRAMMAR_RST
extern const TSLanguage *tree_sitter_rst (void);
#endif
#ifdef HAVE_GRAMMAR_RUBY
extern const TSLanguage *tree_sitter_ruby (void);
#endif
#ifdef HAVE_GRAMMAR_RUST
extern const TSLanguage *tree_sitter_rust (void);
#endif
#ifdef HAVE_GRAMMAR_SATYSFI
extern const TSLanguage *tree_sitter_satysfi (void);
#endif
#ifdef HAVE_GRAMMAR_SCALA
extern const TSLanguage *tree_sitter_scala (void);
#endif
#ifdef HAVE_GRAMMAR_SCHEME
extern const TSLanguage *tree_sitter_scheme (void);
#endif
#ifdef HAVE_GRAMMAR_SCSS
extern const TSLanguage *tree_sitter_scss (void);
#endif
#ifdef HAVE_GRAMMAR_SLIM
extern const TSLanguage *tree_sitter_slim (void);
#endif
#ifdef HAVE_GRAMMAR_SLINT
extern const TSLanguage *tree_sitter_slint (void);
#endif
#ifdef HAVE_GRAMMAR_SMALLTALK
extern const TSLanguage *tree_sitter_smalltalk (void);
#endif
#ifdef HAVE_GRAMMAR_SML
extern const TSLanguage *tree_sitter_sml (void);
#endif
#ifdef HAVE_GRAMMAR_SNAKEMAKE
extern const TSLanguage *tree_sitter_snakemake (void);
#endif
#ifdef HAVE_GRAMMAR_SOLIDITY
extern const TSLanguage *tree_sitter_solidity (void);
#endif
#ifdef HAVE_GRAMMAR_SQL
extern const TSLanguage *tree_sitter_sql (void);
#endif
#ifdef HAVE_GRAMMAR_STARLARK
extern const TSLanguage *tree_sitter_starlark (void);
#endif
#ifdef HAVE_GRAMMAR_STRACE
extern const TSLanguage *tree_sitter_strace (void);
#endif
#ifdef HAVE_GRAMMAR_SVELTE
extern const TSLanguage *tree_sitter_svelte (void);
#endif
#ifdef HAVE_GRAMMAR_SWIFT
extern const TSLanguage *tree_sitter_swift (void);
#endif
#ifdef HAVE_GRAMMAR_TCL
extern const TSLanguage *tree_sitter_tcl (void);
#endif
#ifdef HAVE_GRAMMAR_TEAL
extern const TSLanguage *tree_sitter_teal (void);
#endif
#ifdef HAVE_GRAMMAR_TEMPL
extern const TSLanguage *tree_sitter_templ (void);
#endif
#ifdef HAVE_GRAMMAR_TERA
extern const TSLanguage *tree_sitter_tera (void);
#endif
#ifdef HAVE_GRAMMAR_TEXTPROTO
extern const TSLanguage *tree_sitter_textproto (void);
#endif
#ifdef HAVE_GRAMMAR_THRIFT
extern const TSLanguage *tree_sitter_thrift (void);
#endif
#ifdef HAVE_GRAMMAR_TLAPLUS
extern const TSLanguage *tree_sitter_tlaplus (void);
#endif
#ifdef HAVE_GRAMMAR_TMUX
extern const TSLanguage *tree_sitter_tmux (void);
#endif
#ifdef HAVE_GRAMMAR_TOML
extern const TSLanguage *tree_sitter_toml (void);
#endif
#ifdef HAVE_GRAMMAR_TSV
extern const TSLanguage *tree_sitter_tsv (void);
#endif
#ifdef HAVE_GRAMMAR_TURTLE
extern const TSLanguage *tree_sitter_turtle (void);
#endif
#ifdef HAVE_GRAMMAR_TWIG
extern const TSLanguage *tree_sitter_twig (void);
#endif
#ifdef HAVE_GRAMMAR_TYPESCRIPT
extern const TSLanguage *tree_sitter_typescript (void);
#endif
#ifdef HAVE_GRAMMAR_TYPST
extern const TSLanguage *tree_sitter_typst (void);
#endif
#ifdef HAVE_GRAMMAR_USD
extern const TSLanguage *tree_sitter_usd (void);
#endif
#ifdef HAVE_GRAMMAR_VALA
extern const TSLanguage *tree_sitter_vala (void);
#endif
#ifdef HAVE_GRAMMAR_VERILOG
extern const TSLanguage *tree_sitter_verilog (void);
#endif
#ifdef HAVE_GRAMMAR_VHDL
extern const TSLanguage *tree_sitter_vhdl (void);
#endif
#ifdef HAVE_GRAMMAR_VIM
extern const TSLanguage *tree_sitter_vim (void);
#endif
#ifdef HAVE_GRAMMAR_VUE
extern const TSLanguage *tree_sitter_vue (void);
#endif
#ifdef HAVE_GRAMMAR_WGSL
extern const TSLanguage *tree_sitter_wgsl (void);
#endif
#ifdef HAVE_GRAMMAR_WIT
extern const TSLanguage *tree_sitter_wit (void);
#endif
#ifdef HAVE_GRAMMAR_XML
extern const TSLanguage *tree_sitter_xml (void);
#endif
#ifdef HAVE_GRAMMAR_XQUERY
extern const TSLanguage *tree_sitter_xquery (void);
#endif
#ifdef HAVE_GRAMMAR_YAML
extern const TSLanguage *tree_sitter_yaml (void);
#endif
#ifdef HAVE_GRAMMAR_YANG
extern const TSLanguage *tree_sitter_yang (void);
#endif
#ifdef HAVE_GRAMMAR_YUCK
extern const TSLanguage *tree_sitter_yuck (void);
#endif
#ifdef HAVE_GRAMMAR_ZEEK
extern const TSLanguage *tree_sitter_zeek (void);
#endif
#ifdef HAVE_GRAMMAR_ZIG
extern const TSLanguage *tree_sitter_zig (void);
#endif
#ifdef HAVE_GRAMMAR_ZSH
extern const TSLanguage *tree_sitter_zsh (void);
#endif

/*** Lookup table: grammar name -> language function pointer ***/

typedef struct
{
    const char *name;
    const TSLanguage *(*func) (void);
} ts_grammar_entry_static_t;

/* clang-format off */
static const ts_grammar_entry_static_t ts_grammar_registry[] = {
#ifdef HAVE_GRAMMAR_ADA
    { "ada",             tree_sitter_ada },
#endif
#ifdef HAVE_GRAMMAR_AGDA
    { "agda",            tree_sitter_agda },
#endif
#ifdef HAVE_GRAMMAR_ASM
    { "asm",             tree_sitter_asm },
#endif
#ifdef HAVE_GRAMMAR_ASTRO
    { "astro",           tree_sitter_astro },
#endif
#ifdef HAVE_GRAMMAR_AWK
    { "awk",             tree_sitter_awk },
#endif
#ifdef HAVE_GRAMMAR_BASH
    { "bash",            tree_sitter_bash },
#endif
#ifdef HAVE_GRAMMAR_BIBTEX
    { "bibtex",          tree_sitter_bibtex },
#endif
#ifdef HAVE_GRAMMAR_BICEP
    { "bicep",           tree_sitter_bicep },
#endif
#ifdef HAVE_GRAMMAR_BISON
    { "bison",           tree_sitter_bison },
#endif
#ifdef HAVE_GRAMMAR_BLUEPRINT
    { "blueprint",       tree_sitter_blueprint },
#endif
#ifdef HAVE_GRAMMAR_C
    { "c",               tree_sitter_c },
#endif
#ifdef HAVE_GRAMMAR_CADDY
    { "caddy",           tree_sitter_caddy },
#endif
#ifdef HAVE_GRAMMAR_CAIRO
    { "cairo",           tree_sitter_cairo },
#endif
#ifdef HAVE_GRAMMAR_CLOJURE
    { "clojure",         tree_sitter_clojure },
#endif
#ifdef HAVE_GRAMMAR_CMAKE
    { "cmake",           tree_sitter_cmake },
#endif
#ifdef HAVE_GRAMMAR_COBOL
    { "cobol",           tree_sitter_COBOL },
#endif
#ifdef HAVE_GRAMMAR_COMMONLISP
    { "commonlisp",      tree_sitter_commonlisp },
#endif
#ifdef HAVE_GRAMMAR_CPP
    { "cpp",             tree_sitter_cpp },
#endif
#ifdef HAVE_GRAMMAR_C_SHARP
    { "c_sharp",         tree_sitter_c_sharp },
#endif
#ifdef HAVE_GRAMMAR_CSS
    { "css",             tree_sitter_css },
#endif
#ifdef HAVE_GRAMMAR_CSV
    { "csv",             tree_sitter_csv },
#endif
#ifdef HAVE_GRAMMAR_CUDA
    { "cuda",            tree_sitter_cuda },
#endif
#ifdef HAVE_GRAMMAR_CUE
    { "cue",             tree_sitter_cue },
#endif
#ifdef HAVE_GRAMMAR_D
    { "d",               tree_sitter_d },
#endif
#ifdef HAVE_GRAMMAR_DART
    { "dart",            tree_sitter_dart },
#endif
#ifdef HAVE_GRAMMAR_DEVICETREE
    { "devicetree",      tree_sitter_devicetree },
#endif
#ifdef HAVE_GRAMMAR_DHALL
    { "dhall",           tree_sitter_dhall },
#endif
#ifdef HAVE_GRAMMAR_DIFF
    { "diff",            tree_sitter_diff },
#endif
#ifdef HAVE_GRAMMAR_DOCKERFILE
    { "dockerfile",      tree_sitter_dockerfile },
#endif
#ifdef HAVE_GRAMMAR_DOT
    { "dot",             tree_sitter_dot },
#endif
#ifdef HAVE_GRAMMAR_EARTHFILE
    { "earthfile",       tree_sitter_earthfile },
#endif
#ifdef HAVE_GRAMMAR_EDITORCONFIG
    { "editorconfig",    tree_sitter_editorconfig },
#endif
#ifdef HAVE_GRAMMAR_ELIXIR
    { "elixir",          tree_sitter_elixir },
#endif
#ifdef HAVE_GRAMMAR_ELM
    { "elm",             tree_sitter_elm },
#endif
#ifdef HAVE_GRAMMAR_ERLANG
    { "erlang",          tree_sitter_erlang },
#endif
#ifdef HAVE_GRAMMAR_FENNEL
    { "fennel",          tree_sitter_fennel },
#endif
#ifdef HAVE_GRAMMAR_FISH
    { "fish",            tree_sitter_fish },
#endif
#ifdef HAVE_GRAMMAR_FORTH
    { "forth",           tree_sitter_forth },
#endif
#ifdef HAVE_GRAMMAR_FORTRAN
    { "fortran",         tree_sitter_fortran },
#endif
#ifdef HAVE_GRAMMAR_GDSCRIPT
    { "gdscript",        tree_sitter_gdscript },
#endif
#ifdef HAVE_GRAMMAR_GDSHADER
    { "gdshader",        tree_sitter_gdshader },
#endif
#ifdef HAVE_GRAMMAR_GITATTRIBUTES
    { "gitattributes",   tree_sitter_gitattributes },
#endif
#ifdef HAVE_GRAMMAR_GITIGNORE
    { "gitignore",       tree_sitter_gitignore },
#endif
#ifdef HAVE_GRAMMAR_GLEAM
    { "gleam",           tree_sitter_gleam },
#endif
#ifdef HAVE_GRAMMAR_GLSL
    { "glsl",            tree_sitter_glsl },
#endif
#ifdef HAVE_GRAMMAR_GO
    { "go",              tree_sitter_go },
#endif
#ifdef HAVE_GRAMMAR_GRAPHQL
    { "graphql",         tree_sitter_graphql },
#endif
#ifdef HAVE_GRAMMAR_GROOVY
    { "groovy",          tree_sitter_groovy },
#endif
#ifdef HAVE_GRAMMAR_HACK
    { "hack",            tree_sitter_hack },
#endif
#ifdef HAVE_GRAMMAR_HARE
    { "hare",            tree_sitter_hare },
#endif
#ifdef HAVE_GRAMMAR_HASKELL
    { "haskell",         tree_sitter_haskell },
#endif
#ifdef HAVE_GRAMMAR_HAXE
    { "haxe",            tree_sitter_haxe },
#endif
#ifdef HAVE_GRAMMAR_HCL
    { "hcl",             tree_sitter_hcl },
#endif
#ifdef HAVE_GRAMMAR_HEEX
    { "heex",            tree_sitter_heex },
#endif
#ifdef HAVE_GRAMMAR_HJSON
    { "hjson",           tree_sitter_hjson },
#endif
#ifdef HAVE_GRAMMAR_HLSL
    { "hlsl",            tree_sitter_hlsl },
#endif
#ifdef HAVE_GRAMMAR_HTML
    { "html",            tree_sitter_html },
#endif
#ifdef HAVE_GRAMMAR_IDL
    { "idl",             tree_sitter_idl },
#endif
#ifdef HAVE_GRAMMAR_INI
    { "ini",             tree_sitter_ini },
#endif
#ifdef HAVE_GRAMMAR_JAVA
    { "java",            tree_sitter_java },
#endif
#ifdef HAVE_GRAMMAR_JAVASCRIPT
    { "javascript",      tree_sitter_javascript },
#endif
#ifdef HAVE_GRAMMAR_JQ
    { "jq",              tree_sitter_jq },
#endif
#ifdef HAVE_GRAMMAR_JSON
    { "json",            tree_sitter_json },
#endif
#ifdef HAVE_GRAMMAR_JSON5
    { "json5",           tree_sitter_json5 },
#endif
#ifdef HAVE_GRAMMAR_JSONNET
    { "jsonnet",         tree_sitter_jsonnet },
#endif
#ifdef HAVE_GRAMMAR_JULIA
    { "julia",           tree_sitter_julia },
#endif
#ifdef HAVE_GRAMMAR_JUST
    { "just",            tree_sitter_just },
#endif
#ifdef HAVE_GRAMMAR_KCL
    { "kcl",             tree_sitter_kcl },
#endif
#ifdef HAVE_GRAMMAR_KDL
    { "kdl",             tree_sitter_kdl },
#endif
#ifdef HAVE_GRAMMAR_KOTLIN
    { "kotlin",          tree_sitter_kotlin },
#endif
#ifdef HAVE_GRAMMAR_LATEX
    { "latex",           tree_sitter_latex },
#endif
#ifdef HAVE_GRAMMAR_LUA
    { "lua",             tree_sitter_lua },
#endif
#ifdef HAVE_GRAMMAR_MAKE
    { "make",            tree_sitter_make },
#endif
#ifdef HAVE_GRAMMAR_MARKDOWN
    { "markdown",        tree_sitter_markdown },
#endif
#ifdef HAVE_GRAMMAR_MARKDOWN_INLINE
    { "markdown_inline", tree_sitter_markdown_inline },
#endif
#ifdef HAVE_GRAMMAR_MATLAB
    { "matlab",          tree_sitter_matlab },
#endif
#ifdef HAVE_GRAMMAR_MESON
    { "meson",           tree_sitter_meson },
#endif
#ifdef HAVE_GRAMMAR_MUTTRC
    { "muttrc",          tree_sitter_muttrc },
#endif
#ifdef HAVE_GRAMMAR_NICKEL
    { "nickel",          tree_sitter_nickel },
#endif
#ifdef HAVE_GRAMMAR_NIM
    { "nim",             tree_sitter_nim },
#endif
#ifdef HAVE_GRAMMAR_NIX
    { "nix",             tree_sitter_nix },
#endif
#ifdef HAVE_GRAMMAR_NU
    { "nu",              tree_sitter_nu },
#endif
#ifdef HAVE_GRAMMAR_OBJC
    { "objc",            tree_sitter_objc },
#endif
#ifdef HAVE_GRAMMAR_OCAML
    { "ocaml",           tree_sitter_ocaml },
#endif
#ifdef HAVE_GRAMMAR_ODIN
    { "odin",            tree_sitter_odin },
#endif
#ifdef HAVE_GRAMMAR_ORG
    { "org",             tree_sitter_org },
#endif
#ifdef HAVE_GRAMMAR_PASCAL
    { "pascal",          tree_sitter_pascal },
#endif
#ifdef HAVE_GRAMMAR_PERL
    { "perl",            tree_sitter_perl },
#endif
#ifdef HAVE_GRAMMAR_PHP
    { "php",             tree_sitter_php },
#endif
#ifdef HAVE_GRAMMAR_PKL
    { "pkl",             tree_sitter_pkl },
#endif
#ifdef HAVE_GRAMMAR_PO
    { "po",              tree_sitter_po },
#endif
#ifdef HAVE_GRAMMAR_POWERSHELL
    { "powershell",      tree_sitter_powershell },
#endif
#ifdef HAVE_GRAMMAR_PRISMA
    { "prisma",          tree_sitter_prisma },
#endif
#ifdef HAVE_GRAMMAR_PROPERTIES
    { "properties",      tree_sitter_properties },
#endif
#ifdef HAVE_GRAMMAR_PROTO
    { "proto",           tree_sitter_proto },
#endif
#ifdef HAVE_GRAMMAR_PRQL
    { "prql",            tree_sitter_prql },
#endif
#ifdef HAVE_GRAMMAR_PUPPET
    { "puppet",          tree_sitter_puppet },
#endif
#ifdef HAVE_GRAMMAR_PURESCRIPT
    { "purescript",      tree_sitter_purescript },
#endif
#ifdef HAVE_GRAMMAR_PYTHON
    { "python",          tree_sitter_python },
#endif
#ifdef HAVE_GRAMMAR_QMLJS
    { "qmljs",           tree_sitter_qmljs },
#endif
#ifdef HAVE_GRAMMAR_R
    { "r",               tree_sitter_r },
#endif
#ifdef HAVE_GRAMMAR_RACKET
    { "racket",          tree_sitter_racket },
#endif
#ifdef HAVE_GRAMMAR_RESCRIPT
    { "rescript",        tree_sitter_rescript },
#endif
#ifdef HAVE_GRAMMAR_ROBOT
    { "robot",           tree_sitter_robot },
#endif
#ifdef HAVE_GRAMMAR_ROC
    { "roc",             tree_sitter_roc },
#endif
#ifdef HAVE_GRAMMAR_RON
    { "ron",             tree_sitter_ron },
#endif
#ifdef HAVE_GRAMMAR_RST
    { "rst",             tree_sitter_rst },
#endif
#ifdef HAVE_GRAMMAR_RUBY
    { "ruby",            tree_sitter_ruby },
#endif
#ifdef HAVE_GRAMMAR_RUST
    { "rust",            tree_sitter_rust },
#endif
#ifdef HAVE_GRAMMAR_SATYSFI
    { "satysfi",         tree_sitter_satysfi },
#endif
#ifdef HAVE_GRAMMAR_SCALA
    { "scala",           tree_sitter_scala },
#endif
#ifdef HAVE_GRAMMAR_SCHEME
    { "scheme",          tree_sitter_scheme },
#endif
#ifdef HAVE_GRAMMAR_SCSS
    { "scss",            tree_sitter_scss },
#endif
#ifdef HAVE_GRAMMAR_SLIM
    { "slim",            tree_sitter_slim },
#endif
#ifdef HAVE_GRAMMAR_SLINT
    { "slint",           tree_sitter_slint },
#endif
#ifdef HAVE_GRAMMAR_SMALLTALK
    { "smalltalk",       tree_sitter_smalltalk },
#endif
#ifdef HAVE_GRAMMAR_SML
    { "sml",             tree_sitter_sml },
#endif
#ifdef HAVE_GRAMMAR_SNAKEMAKE
    { "snakemake",       tree_sitter_snakemake },
#endif
#ifdef HAVE_GRAMMAR_SOLIDITY
    { "solidity",        tree_sitter_solidity },
#endif
#ifdef HAVE_GRAMMAR_SQL
    { "sql",             tree_sitter_sql },
#endif
#ifdef HAVE_GRAMMAR_STARLARK
    { "starlark",        tree_sitter_starlark },
#endif
#ifdef HAVE_GRAMMAR_STRACE
    { "strace",          tree_sitter_strace },
#endif
#ifdef HAVE_GRAMMAR_SVELTE
    { "svelte",          tree_sitter_svelte },
#endif
#ifdef HAVE_GRAMMAR_SWIFT
    { "swift",           tree_sitter_swift },
#endif
#ifdef HAVE_GRAMMAR_TCL
    { "tcl",             tree_sitter_tcl },
#endif
#ifdef HAVE_GRAMMAR_TEAL
    { "teal",            tree_sitter_teal },
#endif
#ifdef HAVE_GRAMMAR_TEMPL
    { "templ",           tree_sitter_templ },
#endif
#ifdef HAVE_GRAMMAR_TERA
    { "tera",            tree_sitter_tera },
#endif
#ifdef HAVE_GRAMMAR_TEXTPROTO
    { "textproto",       tree_sitter_textproto },
#endif
#ifdef HAVE_GRAMMAR_THRIFT
    { "thrift",          tree_sitter_thrift },
#endif
#ifdef HAVE_GRAMMAR_TLAPLUS
    { "tlaplus",         tree_sitter_tlaplus },
#endif
#ifdef HAVE_GRAMMAR_TMUX
    { "tmux",            tree_sitter_tmux },
#endif
#ifdef HAVE_GRAMMAR_TOML
    { "toml",            tree_sitter_toml },
#endif
#ifdef HAVE_GRAMMAR_TSV
    { "tsv",             tree_sitter_tsv },
#endif
#ifdef HAVE_GRAMMAR_TURTLE
    { "turtle",          tree_sitter_turtle },
#endif
#ifdef HAVE_GRAMMAR_TWIG
    { "twig",            tree_sitter_twig },
#endif
#ifdef HAVE_GRAMMAR_TYPESCRIPT
    { "typescript",      tree_sitter_typescript },
#endif
#ifdef HAVE_GRAMMAR_TYPST
    { "typst",           tree_sitter_typst },
#endif
#ifdef HAVE_GRAMMAR_USD
    { "usd",             tree_sitter_usd },
#endif
#ifdef HAVE_GRAMMAR_VALA
    { "vala",            tree_sitter_vala },
#endif
#ifdef HAVE_GRAMMAR_VERILOG
    { "verilog",         tree_sitter_verilog },
#endif
#ifdef HAVE_GRAMMAR_VHDL
    { "vhdl",            tree_sitter_vhdl },
#endif
#ifdef HAVE_GRAMMAR_VIM
    { "vim",             tree_sitter_vim },
#endif
#ifdef HAVE_GRAMMAR_VUE
    { "vue",             tree_sitter_vue },
#endif
#ifdef HAVE_GRAMMAR_WGSL
    { "wgsl",            tree_sitter_wgsl },
#endif
#ifdef HAVE_GRAMMAR_WIT
    { "wit",             tree_sitter_wit },
#endif
#ifdef HAVE_GRAMMAR_XML
    { "xml",             tree_sitter_xml },
#endif
#ifdef HAVE_GRAMMAR_XQUERY
    { "xquery",          tree_sitter_xquery },
#endif
#ifdef HAVE_GRAMMAR_YAML
    { "yaml",            tree_sitter_yaml },
#endif
#ifdef HAVE_GRAMMAR_YANG
    { "yang",            tree_sitter_yang },
#endif
#ifdef HAVE_GRAMMAR_YUCK
    { "yuck",            tree_sitter_yuck },
#endif
#ifdef HAVE_GRAMMAR_ZEEK
    { "zeek",            tree_sitter_zeek },
#endif
#ifdef HAVE_GRAMMAR_ZIG
    { "zig",             tree_sitter_zig },
#endif
#ifdef HAVE_GRAMMAR_ZSH
    { "zsh",             tree_sitter_zsh },
#endif
    { NULL, NULL }
};
/* clang-format on */

/**
 * Look up a grammar by name in the static registry.
 * Returns the TSLanguage* or NULL if not found.
 */
static inline const TSLanguage *
ts_grammar_registry_lookup (const char *name)
{
    const ts_grammar_entry_static_t *entry;

    for (entry = ts_grammar_registry; entry->name != NULL; entry++)
    {
        if (strcmp (entry->name, name) == 0)
            return entry->func ();
    }

    return NULL;
}

#endif /* MC__TS_GRAMMAR_REGISTRY_H */
