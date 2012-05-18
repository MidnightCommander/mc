#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

do_view_action() {
    filetype=$1
    filename=$2

    case "${filetype}" in
    iso9660)
        isoinfo -l -i "${MC_EXT_FILENAME}"
        ;;
    cat)
        /bin/cat "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    ar)
        file "${MC_EXT_FILENAME}" && nm -C "${MC_EXT_FILENAME}"
        ;;
    so)
        file "${MC_EXT_FILENAME}" && nm -C -D "${MC_EXT_FILENAME}"
        ;;
    elf)
        file "${MC_EXT_FILENAME}" && nm -C "${MC_EXT_FILENAME}"
        ;;
    dbf)
        dbview -b "${MC_EXT_FILENAME}"
        ;;
    mo)
        msgunfmt "${MC_EXT_FILENAME}" || \
            cat "${MC_EXT_FILENAME}"
        ;;
    lyx)
        lyxcat "${MC_EXT_FILENAME}"
        ;;
    torrent)
        ctorrent -x "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    *)
        ;;
    esac
}

do_open_action() {
    filetype=$1

    case "${filetype}" in
    imakefile)
        xmkmf -a
        ;;
    dbf)
        dbview "${MC_EXT_FILENAME}"
        ;;
    glade)
        if glade-3 --version >/dev/null 2>&1; then
            (glade-3 "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        else
            (glade-2 "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
        fi
        ;;
    lyx)
        lyx "${MC_EXT_FILENAME}"
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
