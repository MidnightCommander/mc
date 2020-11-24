#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

[ -n "${MC_XDG_OPEN}" ] || MC_XDG_OPEN="xdg-open"

do_view_action() {
    filetype=$1

    case "${filetype}" in
    jpeg)
        identify "${MC_EXT_FILENAME}"
        which exif >/dev/null 2>&1 && exif "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    xpm)
        sxpm "${MC_EXT_FILENAME}"
        ;;
    *)
        identify "${MC_EXT_FILENAME}"
        ;;
    esac
}

do_open_action() {
    filetype=$1

    case "${filetype}" in
    xbm)
        (bitmap "${MC_EXT_FILENAME}" &)
        ;;
    xcf)
        (gimp "${MC_EXT_FILENAME}" &)
        ;;
    svg)
        (inkscape "${MC_EXT_FILENAME}" &)
        ;;
    *)
        if [ -n "$DISPLAY" ]; then
            if which geeqie >/dev/null 2>&1; then
                (geeqie "${MC_EXT_FILENAME}" &)
            else
                (gqview "${MC_EXT_FILENAME}" &)
            fi
        elif which see >/dev/null 2>&1; then
            (see "${MC_EXT_FILENAME}" &)
        else
            (zgv "${MC_EXT_FILENAME}" &)
        fi
        ;;
    esac
}

case "${action}" in
view)
    do_view_action "${filetype}"
    ;;
open)
    ("${MC_XDG_OPEN}" "${MC_EXT_FILENAME}" >/dev/null 2>&1) || \
        do_open_action "${filetype}"
    ;;
*)
    ;;
esac
