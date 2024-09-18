#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

if [ -n "$DISPLAY" ]; then
    [ -n "${MC_XDG_OPEN}" ] || MC_XDG_OPEN="xdg-open"
fi

do_view_action() {
    filetype=$1

    case "${filetype}" in
    xpm)
        [ -n "$DISPLAY" ] && sxpm "${MC_EXT_FILENAME}"
        ;;
    *)
        if command -v exif >/dev/null 2>&1; then
            exif "${MC_EXT_FILENAME}" 2>/dev/null
            E=$?
        else
            E=1
        fi
        if [ $E != 0 ] && command -v exiftool >/dev/null 2>&1; then
            exiftool "${MC_EXT_FILENAME}" 2>/dev/null
        fi
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
            if command -v geeqie >/dev/null 2>&1; then
                (geeqie "${MC_EXT_FILENAME}" &)
            else
                (gqview "${MC_EXT_FILENAME}" &)
            fi
        # no backgrounding for console viewers
        elif command -v fim >/dev/null 2>&1; then
            fim "${MC_EXT_FILENAME}"
        elif command -v fbi >/dev/null 2>&1; then
            fbi "${MC_EXT_FILENAME}"
        elif command -v zgv >/dev/null 2>&1; then
            zgv "${MC_EXT_FILENAME}"
        # run-mailcap as a last resort
        elif command -v see >/dev/null 2>&1; then
            (see "${MC_EXT_FILENAME}" &)
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
