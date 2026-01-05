This is an overview of the color capabilities of skin files.

See the FAQ for instructions on setting up 256 color or truecolor support in
your terminal.


Color definitions
-----------------

Keywords defining the colors and attributes to be used in a certain context
have the following syntax:

Color definitions have the syntax of

    keyword = foreground_color;background_color;attributes

Omitted values fall back to the section's "_default_", or in turn to the
"[core]" section's "_default_".


Legacy 16 color mode
--------------------

If you aim for the highest compatibility with terminals, you should restrict
your skin to the standard 16 colors. These colors each have two names: the
preferred human-friendly one, and a technical one referring to their color
index.

        black = color0             gray = color8
          red = color1        brightred = color9
        green = color2      brightgreen = color10
        brown = color3           yellow = color11
         blue = color4       brightblue = color12
      magenta = color5    brightmagenta = color13
         cyan = color6       brightcyan = color14
    lightgray = color7            white = color15

With 8 color terminal settings (such as TERM=linux or TERM=xterm), bright
foreground colors (second column) are emulated in mc by enabling the bold
attribute, and the colors are mapped to their darker counterpart (first
column). In turn, the terminal may (or may not) interpret the bold flag as
brighter foreground color, thus reverting to the originally specified bright
foreground.

Even in terminals that support more colors (such as TERM=xterm-256color), the
first 8 foreground colors combined with the bold attribute might be converted
to their brighter counterpart. This depends on choice of the terminal or a
setting thereof, mc having no control over it.

Plus, the exact color values heavily depend on the user's chosen palette.

For all these reasons, a skin using these colors can look quite different
across systems.


256 color extension
-------------------

Almost all graphical terminal emulators support the 256 color extension.

By adding "256colors = true" to the "[skin]" section, you get access to 240
additional colors (a 6x6x6 color cube and a 24 color grayscale ramp) on top of
the standard 16.

Colors of the 6x6x6 cube are called "rgb000" to "rgb555", all three digits
ranging from 0 to 5, corresponding to the R, G and B color components.
Alternatively, you can use their technical color index: "color16" to
"color231".

Colors of the grayscale ramp are "gray0" to "gray23", or "color232" to
"color255".

There are scripts available that display these colors in the terminal (e.g.
"256colors2.pl" shipped in Xterm's source code), as well as web pages showing
them (e.g. https://www.ditig.com/256-colors-cheat-sheet).


Truecolor extension
-------------------

A wide range of terminals support the truecolor extension.

By adding "truecolors = true" to the "[skin]" section (instead of the
"256colors" keyword), you get access to 2^24 (about 16 million) direct colors
on top of the 256 palette ones.

Truecolors are referred to by the standard "#rrggbb" or "#rgb" notation. The
short form is interpreted by repeating each hex digit, e.g. "#5AF" is a
shorthand for "#55AAFF".


Attributes
----------

The third value can specify special attributes to apply. Supported attributes
are:

    bold
    underline
    italic
    reverse
    blink

Append more with a plus sign, e.g. "bold+italic".

Omitting the field makes it use the fallback value as described above. Use any
unrecognized word (e.g. "none") to prevent fallback and disable all
attributes.

