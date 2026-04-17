dnl
dnl Tree-sitter syntax highlighting support.
dnl

AC_DEFUN([mc_WITH_TREE_SITTER], [

    AC_ARG_WITH([tree-sitter],
        AS_HELP_STRING([--with-tree-sitter],
            [Enable tree-sitter syntax highlighting (in addition to legacy MC highlighting)]),
        [with_tree_sitter=$withval],
        [with_tree_sitter=no])

    if test x"$with_tree_sitter" = xyes; then
        AC_CHECK_HEADER([tree_sitter/api.h], [],
            [AC_MSG_ERROR([tree-sitter headers not found (required for --with-tree-sitter)])])
        AC_CHECK_LIB([tree-sitter], [ts_parser_new], [],
            [AC_MSG_ERROR([tree-sitter library not found (required for --with-tree-sitter)])])
        AC_DEFINE([HAVE_TREE_SITTER], [1], [Define if tree-sitter syntax highlighting is enabled])

        AC_DEFINE([TREE_SITTER_SHARED], [1], [Define if tree-sitter grammars are loaded as shared modules])
        PKG_CHECK_MODULES([GMODULE], [gmodule-2.0], [],
            [AC_MSG_ERROR([gmodule-2.0 required for tree-sitter grammar loading])])

        dnl Resolve libdir for use in Makefile
        eval "ts_libdir=\"$libdir\""
        TREE_SITTER_GRAMMAR_LIBDIR="${ts_libdir}/mc/ts-grammars"

        TREE_SITTER_LIBS="-ltree-sitter"
        TREE_SITTER_CFLAGS=""

        AC_SUBST([TREE_SITTER_GRAMMAR_LIBDIR])
        AC_SUBST([TREE_SITTER_LIBS])
        AC_SUBST([TREE_SITTER_CFLAGS])

        AC_MSG_NOTICE([tree-sitter: enabled (shared) -- grammar .so files loaded at runtime])
    else
        AC_MSG_NOTICE([tree-sitter syntax highlighting disabled])
    fi
])
