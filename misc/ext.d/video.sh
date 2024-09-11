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
    *)
        if command -v mplayer >/dev/null 2>&1; then
            mplayer -identify -vo null -ao null -frames 0 "${MC_EXT_FILENAME}" 2>&1 | \
                sed -n 's/^ID_//p'
        elif command -v mpv_identify.sh >/dev/null 2>&1; then
            mpv_identify.sh "${MC_EXT_FILENAME}"
        else
            echo "Please install either mplayer or mpv to get information for this file"
        fi
        ;;
    esac
}

do_open_action() {
    filetype=$1

    if command -v mpv >/dev/null 2>&1; then
        PLAYER="mpv --really-quiet"
    else
        PLAYER="mplayer -really-quiet"
    fi

    if [ -n "$DISPLAY" ]; then
        ($PLAYER "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
    else
        $PLAYER -vo null "${MC_EXT_FILENAME}"
    fi
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
