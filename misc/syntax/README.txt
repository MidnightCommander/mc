Contents
--------

* Introduction
* How tokens work
* Choosing a color scheme
* Token reference
* Recommendations for syntax file authors
* Checking a syntax file
* Writing a color scheme
* VS Code color themes


Introduction
------------

The syntax files of mcedit (the *.syntax files in this directory) give each
keyword and context a scope token instead of a literal color:

    keyword  whole  if      KEYWORD_CONTROL
    context  " " STRING_QUOTED_DOUBLE

A token says what the text is, not how it looks.  How it looks is decided by
a color scheme: a Sublime Text .sublime-color-scheme file, or the built-in
scheme that reproduces the classic mcedit colors.  The same syntax file can
therefore be shown in many color themes without being edited.

Literal colors (yellow, brightgreen, ...) still work, both in old user files
and as backgrounds, see mcedit(1).  The syntax files shipped with mcedit use
tokens for every foreground color.


How tokens work
---------------

A token is an upper-case name made of letters, digits and underscores.  It
is turned into a Sublime scope by lower-casing it and replacing each
underscore with a dot:

    KEYWORD_CONTROL_IMPORT   ->   keyword.control.import

A color scheme matches scopes by prefix.  The scope above is colored by a
scheme rule for "keyword.control.import" if there is one, otherwise by a rule
for "keyword.control", otherwise by a rule for "keyword".  A more specific
token never hurts: a scheme without a matching rule falls back to the
shorter scope.

A keyword is colored with the scope of its context as parent.  An escape
inside a string is "string.quoted.double constant.character.escape", so a
scheme rule "string constant.character.escape" applies to it.

A background or attributes written after the token override the scheme:

    keyword  linestart  \t  default  red         (only a red background)
    keyword  whole  linestart  #!*\n  COMMENT_LINE  black

The global background of a scheme is not used; the editor background stays
the one of the current skin.


Choosing a color scheme
-----------------------

In the editor: Options -> Color scheme (Alt-Shift-S).  The list shows the
format of each scheme (Sublime or VS Code) on the right.  The choice is stored
as editor_syntax_color_scheme in the [Midnight-Commander] section of the ini
file.

When that option is empty, a skin can suggest a scheme with the key
syntax_scheme in its [editor] section.  When neither is set, the built-in
classic scheme is used.

A scheme NAME is the file NAME.sublime-color-scheme, or a VS Code color theme
NAME.json.  It is looked up in

    ~/.local/share/mc/syntax/schemes/       (user's own schemes, first)
    <datadir>/mc/syntax/schemes/            (shipped with mc)

If both NAME.sublime-color-scheme and NAME.json exist, the Sublime scheme is
used.

mc ships ayu-dark, ayu-light, ayu-mirage, catppuccin-frappe, catppuccin-latte,
catppuccin-macchiato, catppuccin-mocha, dracula, gruvbox, nord, rose-pine,
solarized-dark and solarized-light.  Their licenses are in schemes/LICENSE.


Token reference
---------------

These are the tokens used by the shipped syntax files, grouped by the root of
their scope.  The column "classic" is the color shown when no scheme is
selected; "default" means the editor's normal text color.

Comments
    COMMENT_LINE                   brown      // ..., # ..., -- ...
    COMMENT_BLOCK                  brown      /* ... */
    COMMENT_BLOCK_DOCUMENTATION    brown      /** ... */, (** ... *)

Keywords
    KEYWORD_CONTROL                yellow     if, for, while, return, case
    KEYWORD_CONTROL_IMPORT         yellow     import, include, use, using
    KEYWORD_OPERATOR               yellow     + - * / = == < > && ||
    KEYWORD_OPERATOR_WORD          yellow     and, or, not, instanceof, sizeof
    KEYWORD_OTHER                  yellow     other keywords of the language
    KEYWORD_OTHER_RESERVED         yellow     reserved or less common keywords
    KEYWORD_OTHER_NEW              yellow     new, delete
    KEYWORD_OTHER_PACKAGE          yellow     package, namespace
    KEYWORD_OTHER_TODO             brightred  BUG, FIXME, TODO, NOTE, !!!, ???
                                              inside comments

Storage (declarations and types)
    STORAGE_TYPE                   yellow     declaring words: let, var, typedef
    STORAGE_TYPE_PRIMITIVE         yellow     int, char, float, bool, void
    STORAGE_TYPE_CLASS             yellow     class, struct, enum, interface
    STORAGE_TYPE_FUNCTION          yellow     function, def, func, fun, fn, sub
    STORAGE_TYPE_ANNOTATION        yellow     @Override, @Component, decorators
    STORAGE_MODIFIER               yellow     public, static, const, final

Constants
    CONSTANT_LANGUAGE              yellow     true, false, null, nil, None
    CONSTANT_NUMERIC               brightgreen  42, 0x1F, 3.14e-2
    CONSTANT_CHARACTER             brightgreen  'a'
    CONSTANT_CHARACTER_ESCAPE      brightgreen  \n, \t, \\, &amp;
    CONSTANT_OTHER_PLACEHOLDER     brightgreen  %d, %s in format strings
    CONSTANT_OTHER                 brightgreen  other named constants

Strings
    STRING_QUOTED_DOUBLE           green      "..."
    STRING_QUOTED_SINGLE           green      '...'
    STRING_QUOTED_OTHER            green      `...`, heredocs, <file.h>
    STRING_UNQUOTED                green      unquoted values, block scalars
    STRING_REGEXP                  green      regular expressions
    STRING_OTHER_MATH              green      LaTeX $...$ and equations

Support (things provided by the language or its libraries)
    SUPPORT_FUNCTION               brightred  built-in and library functions,
                                              shell commands
    SUPPORT_CLASS                  brightred  built-in classes and modules
    SUPPORT_TYPE                   brightred  built-in types
    SUPPORT_CONSTANT               brightred  built-in names and settings
    SUPPORT_OTHER                  brightred  other provided names

Names being defined
    ENTITY_NAME_FUNCTION           brightcyan  function name in a definition
    ENTITY_NAME_CLASS              brightcyan  class name in a definition
    ENTITY_NAME_TAG                brightcyan  HTML/XML tags, mapping keys
    ENTITY_NAME_SECTION            brightcyan  section headers ([section], %build)
    ENTITY_NAME_LABEL              brightcyan  goto labels, label@
    ENTITY_NAME_OTHER              brightcyan  other defined names
    ENTITY_OTHER_ATTRIBUTE_NAME    default    attributes, field names
    ENTITY_OTHER_PSEUDO_CLASS      default    CSS :hover, :first-child
    ENTITY_OTHER_PSEUDO_ELEMENT    default    CSS ::before, ::selection

Variables
    VARIABLE_LANGUAGE              yellow     this, self, super
    VARIABLE_OTHER                 default    $x, ${x}, %VAR%
    VARIABLE_OTHER_MEMBER          default    .length, .sizeof
    VARIABLE_PARAMETER             default    command-line flags, parameters

Punctuation
    PUNCTUATION_SECTION            brightcyan  { } ( ) [ ]
    PUNCTUATION_SEPARATOR          brightcyan  , : . ?
    PUNCTUATION_SEPARATOR_CONTINUATION  brightcyan  line continuation \
                                              (yellow inside META_PREPROCESSOR)
    PUNCTUATION_TERMINATOR         brightmagenta  ;

Meta (regions of a file)
    META_PREPROCESSOR              brightred  #include, #define lines, pragmas,
                                              template tags {% %}
    META_EMBEDDED                  default    code in another language: <? ?>
    META_HEADER                    default    mail header blocks
    META_DIFF_HEADER               brightmagenta  diff file headers ---, +++
    META_DIFF_RANGE                brightcyan  diff hunk headers @@ ... @@

Markup (prose and documents)
    MARKUP_HEADING                 brightcyan  headings
    MARKUP_BOLD                    white      bold text
    MARKUP_ITALIC                  yellow     italic text
    MARKUP_UNDERLINE_LINK          yellow     links, URLs, e-mail addresses
    MARKUP_LIST                    yellow     list bullets
    MARKUP_QUOTE                   green      quoted text: "> " in mail
    MARKUP_RAW                     green      verbatim and code blocks
    MARKUP_INSERTED                brightgreen  diff: added lines
    MARKUP_DELETED                 brightred  diff: removed lines
    MARKUP_CHANGED                 yellow     diff: changed lines

Invalid
    INVALID_ILLEGAL                white on red  syntax errors
    INVALID_DEPRECATED             white on red  deprecated commands and functions

Inside META_PREPROCESSOR, strings are red and punctuation is yellow.

Other tokens can be used too; any upper-case name works.  A scheme that has
no rule for it, and no rule for a shorter prefix of it, leaves the text in
the editor's normal color.  The scope names follow the Sublime Text guide at
https://www.sublimetext.com/docs/scope_naming.html.


Recommendations for syntax file authors
---------------------------------------

1.  Describe what the text is, never what color you want.  Pick the token
    for the meaning, then accept the color it gets.  A made-up token such as
    YELLOW or HIGHLIGHT matches no scheme rule.

2.  Use the most specific token you are sure of.  KEYWORD_CONTROL_IMPORT is
    better than KEYWORD_CONTROL, which is better than KEYWORD: schemes fall
    back to the shorter scope anyway.

3.  Use tokens from the reference above.  If nothing fits, add _OTHER under
    the closest root (KEYWORD_OTHER, SUPPORT_OTHER, ENTITY_NAME_OTHER)
    instead of inventing a new root.

4.  Be consistent across languages: the same construct gets the same token
    in every file.  Look at how other files handle it before choosing.  For
    example, every function-declaring keyword is STORAGE_TYPE_FUNCTION, and
    every comment marker such as TODO is KEYWORD_OTHER_TODO.

5.  Write tokens in upper case.  "keyword.control" is read as a color name
    and fails.

6.  A background may stay literal after the token: "COMMENT_LINE black".
    For a highlight that only sets a background (a tab marker, an indent
    marker), use "default" as the foreground: "default red".

7.  Leave plain text without a color.  Unchanged diff lines, prose bodies
    and continuation lines do not need a token.

8.  Comments are not keywords.  Use COMMENT_LINE or COMMENT_BLOCK for the
    comment context and for its delimiters, and KEYWORD_OTHER_TODO for the
    markers inside it.

9.  When you cannot decide, keep the literal color and put a comment line
    "# TODO scope" directly above the rule, so it can be found later.

10. A file whose literal colors are its content (syntax.syntax shows each
    color name in its own color) may contain the line

        # check-tokens: literal colors by design

    The checker then skips the whole file.


Checking a syntax file
----------------------

The script check-tokens.py, kept with the syntax files in the source tree
(misc/syntax/), reports every foreground that is a literal color instead of
a token:

    python3 misc/syntax/check-tokens.py               all *.syntax files
    python3 misc/syntax/check-tokens.py my.syntax     one file
    python3 misc/syntax/check-tokens.py --strict      also the "# TODO scope" ones

It exits with status 1 when it finds something.  The project's continuous
integration runs it with --strict on every change to misc/syntax/.


Writing a color scheme
----------------------

A scheme is a JSON file (comments and trailing commas are allowed), the same
format Sublime Text uses:

    {
        "name": "My scheme",
        "variables": { "blue": "#61afef" },
        "globals": { "foreground": "#abb2bf" },
        "rules": [
            { "scope": "comment", "foreground": "#5c6370",
              "font_style": "italic" },
            { "scope": "keyword, storage", "foreground": "var(blue)" },
            { "scope": "string", "foreground": "#98c379" },
            { "scope": "string constant.character.escape",
              "foreground": "#56b6c2" },
            { "scope": "invalid", "foreground": "#ffffff",
              "background": "#e06c75" }
        ]
    }

mcedit reads "variables", "globals" (foreground only; the background is
ignored) and the "rules" with "scope", "foreground", "background" and
"font_style" (bold, italic, underline).  Colors may be #rrggbb, CSS color
names, mc color names or var(name).  On terminals without true color, colors
are mapped to the nearest of 256 or 16 colors.

A selector can list several scopes separated by commas, and a scope with a
parent ("string constant") applies only inside that parent.  The most
specific matching rule wins, separately for foreground and background.

Put the file in ~/.local/share/mc/syntax/schemes/ and select it with
Alt-Shift-S.  The roots worth covering are comment, keyword, storage,
constant, string, support, entity.name, variable, punctuation,
meta.preprocessor, markup and invalid.


VS Code color themes
--------------------

VS Code color themes color the same TextMate scopes, so they can be used as
they are: copy the theme's JSON file (in a VS Code extension it is usually
themes/*-color-theme.json) into ~/.local/share/mc/syntax/schemes/ and select
it with Alt-Shift-S.

    {
        "name": "My theme",
        "colors": { "editor.foreground": "#d4d4d4" },
        "tokenColors": [
            { "scope": "comment",
              "settings": { "foreground": "#6a9955", "fontStyle": "italic" } },
            { "scope": ["string", "string.quoted"],
              "settings": { "foreground": "#ce9178" } }
        ]
    }

A file is read as a VS Code theme when it has "tokenColors" or "include".
mcedit reads "editor.foreground" from "colors" and each "tokenColors" entry's
"scope" (a selector string or an array of selectors) and its "settings":
"foreground", "background" and "fontStyle".  An entry without "scope" gives
the default foreground when "editor.foreground" is missing.  All other
"colors" are UI colors and are ignored.

"include" names another theme file, relative to the including one; its rules
come first, so the including theme's own rules win.  Includes may be nested
(VS Code's Dark Modern includes Dark+, which includes Dark), so copy the
whole chain into the same directory.  "tokenColors" may also be the name of a
JSON file holding the list of rules.  "semanticTokenColors" and .tmTheme
files are not supported.
