#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2


do_view_action() {
    filetype=$1

    case "${filetype}" in
    *)
        cat "${MC_EXT_FILENAME}"
        ;;
    esac
}

do_open_action() {
    filetype=$1

    case "${filetype}" in
    ram)
        (realplay "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        ;;
    *)
        (mplayer "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        #(gtv "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        #(xanim "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
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
