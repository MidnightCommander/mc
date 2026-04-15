dnl
dnl Tree-sitter syntax highlighting support.
dnl

AC_DEFUN([mc_WITH_TREE_SITTER], [

    AC_ARG_WITH([tree-sitter],
        AS_HELP_STRING([--with-tree-sitter],
            [Enable tree-sitter syntax highlighting (in addition to legacy MC highlighting)]),
        [with_tree_sitter=$withval],
        [with_tree_sitter=no])

    AC_ARG_WITH([tree-sitter-grammars],
        AS_HELP_STRING([--with-tree-sitter-grammars=LIST],
            [Comma-separated list of tree-sitter grammars to build (default: all)]),
        [with_tree_sitter_grammars=$withval],
        [with_tree_sitter_grammars=all])

    AC_ARG_WITH([tree-sitter-static],
        AS_HELP_STRING([--with-tree-sitter-static],
            [Link tree-sitter grammars statically into the binary (default: build as shared modules)]),
        [with_tree_sitter_static=$withval],
        [with_tree_sitter_static=no])

    if test x"$with_tree_sitter" = xyes; then
        AC_CHECK_HEADER([tree_sitter/api.h], [],
            [AC_MSG_ERROR([tree-sitter headers not found (required for --with-tree-sitter)])])
        AC_CHECK_LIB([tree-sitter], [ts_parser_new], [],
            [AC_MSG_ERROR([tree-sitter library not found (required for --with-tree-sitter)])])
        AC_DEFINE([HAVE_TREE_SITTER], [1], [Define if tree-sitter syntax highlighting is enabled])

        if test x"$with_tree_sitter_static" = xyes; then
            AC_DEFINE([TREE_SITTER_STATIC], [1], [Define if tree-sitter grammars are linked statically])
            TREE_SITTER_BUILD_TARGET="libtsgrammars.a"
            TREE_SITTER_BUILD_MODE="static"
        else
            AC_DEFINE([TREE_SITTER_SHARED], [1], [Define if tree-sitter grammars are loaded as shared modules])
            PKG_CHECK_MODULES([GMODULE], [gmodule-2.0], [],
                [AC_MSG_ERROR([gmodule-2.0 required for shared tree-sitter grammars (use --with-tree-sitter-static to avoid)])])
            TREE_SITTER_BUILD_TARGET=""
            TREE_SITTER_BUILD_MODE="shared"
            dnl Resolve libdir for use in Makefile (which doesn't have automake's variable chain)
            eval "ts_libdir=\"$libdir\""
            TREE_SITTER_GRAMMAR_LIBDIR="${ts_libdir}/mc/ts-grammars"
        fi
        AC_SUBST([TREE_SITTER_BUILD_TARGET])
        AC_SUBST([TREE_SITTER_BUILD_MODE])
        AC_SUBST([TREE_SITTER_GRAMMAR_LIBDIR])

        TREE_SITTER_GRAMMAR_DEFS=""
        TREE_SITTER_GRAMMAR_ARCHIVES=""
        TREE_SITTER_GRAMMARS=""
        TREE_SITTER_LIBS="-ltree-sitter"
        TREE_SITTER_CFLAGS=""

        if test x"$with_tree_sitter_static" = xyes; then
            dnl Static mode: discover available .a files and build grammar list
            ts_grammar_dir="$srcdir/src/editor/ts-grammars"

            if test x"$with_tree_sitter_grammars" = xall; then
                dnl Auto-discover all available .a files
                tree_sitter_grammars=""
                for a in "$ts_grammar_dir"/*/*.a; do
                    test -f "$a" || continue
                    ts_grammar_a_dir=`dirname "$a"`
                    g=`basename "$ts_grammar_a_dir"`
                    tree_sitter_grammars="$tree_sitter_grammars $g"
                done
                tree_sitter_grammars=`echo $tree_sitter_grammars`
            else
                tree_sitter_grammars=`echo "$with_tree_sitter_grammars" | tr ',' ' '`
            fi

            for g in $tree_sitter_grammars; do
                if test ! -f "$ts_grammar_dir/$g/$g.a"; then
                    AC_MSG_ERROR([tree-sitter grammar static library not found: $ts_grammar_dir/$g/$g.a])
                fi
                upper=`echo "$g" | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
                TREE_SITTER_GRAMMAR_DEFS="$TREE_SITTER_GRAMMAR_DEFS -DHAVE_GRAMMAR_${upper}=1"
                TREE_SITTER_GRAMMAR_ARCHIVES="$TREE_SITTER_GRAMMAR_ARCHIVES $g/$g.a"
                TREE_SITTER_GRAMMARS="$TREE_SITTER_GRAMMARS $g"
            done
            dnl Remove leading spaces
            TREE_SITTER_GRAMMAR_DEFS=`echo $TREE_SITTER_GRAMMAR_DEFS`
            TREE_SITTER_GRAMMAR_ARCHIVES=`echo $TREE_SITTER_GRAMMAR_ARCHIVES`
            TREE_SITTER_GRAMMARS=`echo $TREE_SITTER_GRAMMARS`
        fi
        dnl Shared mode: no grammar compilation. .so files are loaded at runtime.

        AC_SUBST([TREE_SITTER_GRAMMAR_DEFS])
        AC_SUBST([TREE_SITTER_GRAMMAR_ARCHIVES])
        AC_SUBST([TREE_SITTER_GRAMMARS])
        AC_SUBST([TREE_SITTER_LIBS])
        AC_SUBST([TREE_SITTER_CFLAGS])

        if test x"$with_tree_sitter_static" = xyes; then
            ts_grammar_count=`echo $tree_sitter_grammars | wc -w | tr -d ' '`
            if test x"$with_tree_sitter_grammars" = xall; then
                AC_MSG_NOTICE([tree-sitter: enabled (static) with all $ts_grammar_count grammars])
            else
                AC_MSG_NOTICE([tree-sitter: enabled (static) with $ts_grammar_count grammars: $TREE_SITTER_GRAMMARS])
            fi
        else
            AC_MSG_NOTICE([tree-sitter: enabled (shared) -- grammar .so files loaded at runtime])
        fi

        dnl Check that grammar files exist
        ts_grammars_found=no
        if test x"$with_tree_sitter_static" = xyes; then
            dnl Static: check for at least one grammar .a file
            for g in $tree_sitter_grammars; do
                if test -f "$srcdir/src/editor/ts-grammars/$g/$g.a"; then
                    ts_grammars_found=yes
                    break
                fi
            done
            if test x"$ts_grammars_found" = xno; then
                AC_MSG_ERROR([tree-sitter grammar static libraries not found. Run:
  ./scripts/ts-grammars-download.sh --static])
            fi
        else
            dnl Shared mode: grammars loaded at runtime, no compile-time check needed.
            dnl Grammar .so files are an external dependency.
            ts_grammars_found=yes
        fi
    else
        AC_MSG_NOTICE([tree-sitter syntax highlighting disabled])
    fi
])
