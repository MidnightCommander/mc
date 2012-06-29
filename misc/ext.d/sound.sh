#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

do_view_action() {
    filetype=$1

    case "${filetype}" in
    mp3)
        mpg123 -vtn1 "${MC_EXT_FILENAME}" 2>&1 | \
            sed -n '/^Title/,/^Comment/p;/^MPEG/,/^Audio/p'
        ;;
    ogg)
        ogginfo "${MC_EXT_SELECTED}"
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
        if [ "$DISPLAY" = "" ]; then
            play "${MC_EXT_FILENAME}"
        else
            (xmms  "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
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
        if [ "$DISPLAY" = "" ]; then
            mpg123 "${MC_EXT_FILENAME}"
        else
            (xmms "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        fi
        ;;
    ogg)
        if [ "$DISPLAY" = "" ]; then
            ogg123 "${MC_EXT_FILENAME}"
        else
            (xmms "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        fi
        ;;
    midi)
        timidity "${MC_EXT_FILENAME}"
        ;;
    wma)
        mplayer -vo null "${MC_EXT_FILENAME}"
        ;;
    playlist)
        if [ -z "$DISPLAY" ]; then
            mplayer -vo null -playlist "${MC_EXT_FILENAME}"
        else
            (xmms -p "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
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
    xdg-open "${MC_EXT_FILENAME}" 2>/dev/null || \
        do_open_action "${filetype}"
    ;;
*)
    ;;
esac
