#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

do_view_action() {
    filetype=$1

    case "${filetype}" in
    jpeg)
        identify "${MC_EXT_FILENAME}"; test -x /usr/bin/exif && echo && exif "${MC_EXT_FILENAME}" 2>/dev/null
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
        bitmap "${MC_EXT_FILENAME}"
        ;;
    xcf)
        (gimp "${MC_EXT_FILENAME}" &)
        ;;
    *)
        if [ "$DISPLAY" = "" ]; then
            zgv "${MC_EXT_FILENAME}"
        else
            (gqview "${MC_EXT_FILENAME}" &)
        fi
        ;;
    esac
}

case "${action}" in
view)
    do_view_action "${filetype}"
    ;;
open)
    xdg-open "${MC_EXT_FILENAME}" 2>/dev/null || \
        do_open_action "${filetype}"
    ;;
*)
    ;;
esac
