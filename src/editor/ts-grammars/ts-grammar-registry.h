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
#ifdef HAVE_GRAMMAR_ASM
extern const TSLanguage *tree_sitter_asm (void);
#endif
#ifdef HAVE_GRAMMAR_AWK
extern const TSLanguage *tree_sitter_awk (void);
#endif
#ifdef HAVE_GRAMMAR_BASH
extern const TSLanguage *tree_sitter_bash (void);
#endif
#ifdef HAVE_GRAMMAR_BISON
extern const TSLanguage *tree_sitter_bison (void);
#endif
#ifdef HAVE_GRAMMAR_C
extern const TSLanguage *tree_sitter_c (void);
#endif
#ifdef HAVE_GRAMMAR_CADDY
extern const TSLanguage *tree_sitter_caddy (void);
#endif
#ifdef HAVE_GRAMMAR_CMAKE
extern const TSLanguage *tree_sitter_cmake (void);
#endif
#ifdef HAVE_GRAMMAR_COBOL
extern const TSLanguage *tree_sitter_COBOL (void);
#endif
#ifdef HAVE_GRAMMAR_LISP
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
#ifdef HAVE_GRAMMAR_CUDA
extern const TSLanguage *tree_sitter_cuda (void);
#endif
#ifdef HAVE_GRAMMAR_D
extern const TSLanguage *tree_sitter_d (void);
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
#ifdef HAVE_GRAMMAR_ERLANG
extern const TSLanguage *tree_sitter_erlang (void);
#endif
#ifdef HAVE_GRAMMAR_FORTRAN
extern const TSLanguage *tree_sitter_fortran (void);
#endif
#ifdef HAVE_GRAMMAR_GLSL
extern const TSLanguage *tree_sitter_glsl (void);
#endif
#ifdef HAVE_GRAMMAR_GO
extern const TSLanguage *tree_sitter_go (void);
#endif
#ifdef HAVE_GRAMMAR_HASKELL
extern const TSLanguage *tree_sitter_haskell (void);
#endif
#ifdef HAVE_GRAMMAR_HCL
extern const TSLanguage *tree_sitter_hcl (void);
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
#ifdef HAVE_GRAMMAR_JSON
extern const TSLanguage *tree_sitter_json (void);
#endif
#ifdef HAVE_GRAMMAR_KOTLIN
extern const TSLanguage *tree_sitter_kotlin (void);
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
#ifdef HAVE_GRAMMAR_OCAML
extern const TSLanguage *tree_sitter_ocaml (void);
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
#ifdef HAVE_GRAMMAR_PO
extern const TSLanguage *tree_sitter_po (void);
#endif
#ifdef HAVE_GRAMMAR_PROPERTIES
extern const TSLanguage *tree_sitter_properties (void);
#endif
#ifdef HAVE_GRAMMAR_PROTO
extern const TSLanguage *tree_sitter_proto (void);
#endif
#ifdef HAVE_GRAMMAR_PYTHON
extern const TSLanguage *tree_sitter_python (void);
#endif
#ifdef HAVE_GRAMMAR_R
extern const TSLanguage *tree_sitter_r (void);
#endif
#ifdef HAVE_GRAMMAR_RUBY
extern const TSLanguage *tree_sitter_ruby (void);
#endif
#ifdef HAVE_GRAMMAR_RUST
extern const TSLanguage *tree_sitter_rust (void);
#endif
#ifdef HAVE_GRAMMAR_SCALA
extern const TSLanguage *tree_sitter_scala (void);
#endif
#ifdef HAVE_GRAMMAR_SMALLTALK
extern const TSLanguage *tree_sitter_smalltalk (void);
#endif
#ifdef HAVE_GRAMMAR_SQL
extern const TSLanguage *tree_sitter_sql (void);
#endif
#ifdef HAVE_GRAMMAR_STRACE
extern const TSLanguage *tree_sitter_strace (void);
#endif
#ifdef HAVE_GRAMMAR_SWIFT
extern const TSLanguage *tree_sitter_swift (void);
#endif
#ifdef HAVE_GRAMMAR_TCL
extern const TSLanguage *tree_sitter_tcl (void);
#endif
#ifdef HAVE_GRAMMAR_TOML
extern const TSLanguage *tree_sitter_toml (void);
#endif
#ifdef HAVE_GRAMMAR_TURTLE
extern const TSLanguage *tree_sitter_turtle (void);
#endif
#ifdef HAVE_GRAMMAR_TYPESCRIPT
extern const TSLanguage *tree_sitter_typescript (void);
#endif
#ifdef HAVE_GRAMMAR_VERILOG
extern const TSLanguage *tree_sitter_verilog (void);
#endif
#ifdef HAVE_GRAMMAR_VHDL
extern const TSLanguage *tree_sitter_vhdl (void);
#endif
#ifdef HAVE_GRAMMAR_XML
extern const TSLanguage *tree_sitter_xml (void);
#endif
#ifdef HAVE_GRAMMAR_YAML
extern const TSLanguage *tree_sitter_yaml (void);
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
#ifdef HAVE_GRAMMAR_ASM
    { "asm",             tree_sitter_asm },
#endif
#ifdef HAVE_GRAMMAR_AWK
    { "awk",             tree_sitter_awk },
#endif
#ifdef HAVE_GRAMMAR_BASH
    { "bash",            tree_sitter_bash },
#endif
#ifdef HAVE_GRAMMAR_BISON
    { "bison",           tree_sitter_bison },
#endif
#ifdef HAVE_GRAMMAR_C
    { "c",               tree_sitter_c },
#endif
#ifdef HAVE_GRAMMAR_CADDY
    { "caddy",           tree_sitter_caddy },
#endif
#ifdef HAVE_GRAMMAR_COBOL
    { "cobol",           tree_sitter_COBOL },
#endif
#ifdef HAVE_GRAMMAR_CMAKE
    { "cmake",           tree_sitter_cmake },
#endif
#ifdef HAVE_GRAMMAR_LISP
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
#ifdef HAVE_GRAMMAR_CUDA
    { "cuda",            tree_sitter_cuda },
#endif
#ifdef HAVE_GRAMMAR_D
    { "d",               tree_sitter_d },
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
#ifdef HAVE_GRAMMAR_ERLANG
    { "erlang",          tree_sitter_erlang },
#endif
#ifdef HAVE_GRAMMAR_FORTRAN
    { "fortran",         tree_sitter_fortran },
#endif
#ifdef HAVE_GRAMMAR_GLSL
    { "glsl",            tree_sitter_glsl },
#endif
#ifdef HAVE_GRAMMAR_GO
    { "go",              tree_sitter_go },
#endif
#ifdef HAVE_GRAMMAR_HASKELL
    { "haskell",         tree_sitter_haskell },
#endif
#ifdef HAVE_GRAMMAR_HCL
    { "hcl",             tree_sitter_hcl },
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
#ifdef HAVE_GRAMMAR_JSON
    { "json",            tree_sitter_json },
#endif
#ifdef HAVE_GRAMMAR_KOTLIN
    { "kotlin",          tree_sitter_kotlin },
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
#ifdef HAVE_GRAMMAR_OCAML
    { "ocaml",           tree_sitter_ocaml },
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
#ifdef HAVE_GRAMMAR_PO
    { "po",              tree_sitter_po },
#endif
#ifdef HAVE_GRAMMAR_PROPERTIES
    { "properties",      tree_sitter_properties },
#endif
#ifdef HAVE_GRAMMAR_PROTO
    { "proto",           tree_sitter_proto },
#endif
#ifdef HAVE_GRAMMAR_PYTHON
    { "python",          tree_sitter_python },
#endif
#ifdef HAVE_GRAMMAR_R
    { "r",               tree_sitter_r },
#endif
#ifdef HAVE_GRAMMAR_RUBY
    { "ruby",            tree_sitter_ruby },
#endif
#ifdef HAVE_GRAMMAR_RUST
    { "rust",            tree_sitter_rust },
#endif
#ifdef HAVE_GRAMMAR_SCALA
    { "scala",           tree_sitter_scala },
#endif
#ifdef HAVE_GRAMMAR_SMALLTALK
    { "smalltalk",       tree_sitter_smalltalk },
#endif
#ifdef HAVE_GRAMMAR_SQL
    { "sql",             tree_sitter_sql },
#endif
#ifdef HAVE_GRAMMAR_STRACE
    { "strace",          tree_sitter_strace },
#endif
#ifdef HAVE_GRAMMAR_SWIFT
    { "swift",           tree_sitter_swift },
#endif
#ifdef HAVE_GRAMMAR_TCL
    { "tcl",             tree_sitter_tcl },
#endif
#ifdef HAVE_GRAMMAR_TOML
    { "toml",            tree_sitter_toml },
#endif
#ifdef HAVE_GRAMMAR_TURTLE
    { "turtle",          tree_sitter_turtle },
#endif
#ifdef HAVE_GRAMMAR_TYPESCRIPT
    { "typescript",      tree_sitter_typescript },
#endif
#ifdef HAVE_GRAMMAR_VERILOG
    { "verilog",         tree_sitter_verilog },
#endif
#ifdef HAVE_GRAMMAR_VHDL
    { "vhdl",            tree_sitter_vhdl },
#endif
#ifdef HAVE_GRAMMAR_XML
    { "xml",             tree_sitter_xml },
#endif
#ifdef HAVE_GRAMMAR_YAML
    { "yaml",            tree_sitter_yaml },
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
