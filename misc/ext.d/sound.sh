#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

[ -n "${MC_XDG_OPEN}" ] || MC_XDG_OPEN="xdg-open"

do_view_action() {
    filetype=$1

    case "${filetype}" in
    common)
        mediainfo "${MC_EXT_FILENAME}"
        ;;

    mp3)
        mpg123 -vtn1 "${MC_EXT_FILENAME}" 2>&1 | \
            sed -n '/^Title/,/^Comment/p;/^MPEG/,/^Audio/p'
        ;;
    ogg)
        ogginfo "${MC_EXT_FILENAME}"
        ;;
    opus)
        opusinfo "${MC_EXT_FILENAME}"
        ;;
    wma)
        mplayer -quiet -slave -frames 0 -vo null -ao null -identify "${MC_EXT_FILENAME}" 2>/dev/null | \
            tail +13 || file "${MC_EXT_FILENAME}"
        ;;
    *)
        cat "${MC_EXT_FILENAME}"
        ;;
    esac
}

do_open_action() {
    filetype=$1

    case "${filetype}" in
    common)
        if [ -n "$DISPLAY" ]; then
            (audacious  "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        else
            play "${MC_EXT_FILENAME}"
        fi
        ;;
    mod)
        mikmod "${MC_EXT_FILENAME}"
        #tracker "${MC_EXT_FILENAME}"
        ;;
    wav22)
        vplay -s 22 "${MC_EXT_FILENAME}"
        ;;
    mp3)
        if [ -n "$DISPLAY" ]; then
            (audacious "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        else
            mpg123 "${MC_EXT_FILENAME}"
        fi
        ;;
    ogg)
        if [ -n "$DISPLAY" ]; then
            (audacious "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        else
            ogg123 "${MC_EXT_FILENAME}"
        fi
        ;;
    opus)
        if [ -n "$DISPLAY" ]; then
            (audacious "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        else
            play "${MC_EXT_FILENAME}"
        fi
        ;;
    midi)
        timidity "${MC_EXT_FILENAME}"
        ;;
    wma)
        mplayer -vo null "${MC_EXT_FILENAME}"
        ;;
    playlist)
        if [ -n "$DISPLAY" ]; then
            (audacious -p "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        else
            mplayer -vo null -playlist "${MC_EXT_FILENAME}"
        fi
        ;;
    *)
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
