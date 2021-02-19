#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

[ -n "${MC_XDG_OPEN}" ] || MC_XDG_OPEN="xdg-open"

STAROFFICE_REGEXP='\.(sxw|sdw|stw|sxc|stc|sxi|sti|sxd|std||sxm||sxg)$'

staroffice_console() {
    filename=$1;shift
    is_view=$1; shift
    if [ -n "${is_view}" ]; then
        is_view='-dump'
    fi

    tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
    cd $tmp
    soffice2html.pl "${filename}"
    elinks ${is_view} content.html
    rm -rf "$tmp"
}

get_ooffice_executable() {
    if which loffice >/dev/null 2>&1; then
        echo "loffice"
    elif which ooffice >/dev/null 2>&1; then
        echo "ooffice"
    else
        echo -n
    fi
}

do_view_action() {
    filetype=$1

    case "${filetype}" in
    ps)
        ps2ascii "${MC_EXT_FILENAME}"
        ;;
    pdf)
        pdftotext -layout -nopgbrk "${MC_EXT_FILENAME}" -
        ;;
    odt)
        if [ ` echo "${MC_EXT_FILENAME}" | grep -c "${STAROFFICE_REGEXP}"` -ne 0 ]; then
            staroffice_console "${MC_EXT_FILENAME}" "view"
        else
            odt2txt "${MC_EXT_FILENAME}"
        fi
        ;;
    msdoc)
        if which wvHtml >/dev/null 2>&1; then
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            wvHtml "${MC_EXT_FILENAME}" --targetdir="$tmp" page.html
            elinks -dump "$tmp/page.html"
            rm -rf "$tmp"
        elif which antiword >/dev/null 2>&1; then
            antiword -t "${MC_EXT_FILENAME}"
        elif which catdoc >/dev/null 2>&1; then
            catdoc -w "${MC_EXT_FILENAME}"
        elif which word2x >/dev/null 2>&1; then
            word2x -f text "${MC_EXT_FILENAME}" -
        else
            strings "${MC_EXT_FILENAME}"
        fi
        ;;
    msxls)
        if which xlhtml >/dev/null 2>&1; then
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            xlhtml -a "${MC_EXT_FILENAME}" > "$tmp/page.html"
            elinks -dump "$tmp/page.html"
            rm -rf "$tmp"
        elif which xls2csv >/dev/null 2>&1; then
            xls2csv "${MC_EXT_FILENAME}"
        else
            strings "${MC_EXT_FILENAME}"
        fi
        ;;
    dvi)
        which dvi2tty >/dev/null 2>&1 && \
            dvi2tty "${MC_EXT_FILENAME}" || \
            catdvi "${MC_EXT_FILENAME}"
        ;;
    djvu)
        djvused -e print-pure-txt "${MC_EXT_FILENAME}"
        ;;
    ebook)
        einfo -v "${MC_EXT_FILENAME}"
        ;;
    *)
        ;;
    esac
}

do_open_action() {
    filetype=$1

    case "${filetype}" in
    ps)
        if [ -n "$DISPLAY" ]; then
            (gv "${MC_EXT_FILENAME}" &)
        else
            ps2ascii "${MC_EXT_FILENAME}" | ${PAGER:-more}
        fi
        ;;
    pdf)
        if [ ! -n "$DISPLAY" ]; then
            pdftotext -layout -nopgbrk "${MC_EXT_FILENAME}" - | ${PAGER:-more}
        elif see > /dev/null 2>&1; then
            (see "${MC_EXT_FILENAME}" &)
        else
            (xpdf "${MC_EXT_FILENAME}" &)
        fi
        #(acroread "${MC_EXT_FILENAME}" &)
        #(ghostview "${MC_EXT_FILENAME}" &)
        ;;
    ooffice)
        if [ -n "$DISPLAY" ]; then
            OOFFICE=`get_ooffice_executable`
            if [ -n "${OOFFICE}" ]; then
                (${OOFFICE} "${MC_EXT_FILENAME}" &)
            fi
        else
            if [ `echo "${MC_EXT_FILENAME}" | grep -c "${STAROFFICE_REGEXP}"` -ne 0 ]; then
                staroffice_console "${MC_EXT_FILENAME}"
            else
                odt2txt "${MC_EXT_FILENAME}" | ${PAGER:-more}
            fi
        fi
        ;;
    abw)
        (abiword "${MC_EXT_FILENAME}" &)
        ;;
    gnumeric)
        (gnumeric "${MC_EXT_FILENAME}" &)
        ;;
    msdoc)
        if [ -n "$DISPLAY" ]; then
            OOFFICE=`get_ooffice_executable`
            if [ -n "${OOFFICE}" ]; then
                (${OOFFICE} "${MC_EXT_FILENAME}" &)
            else
                (abiword "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
            fi
        else
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            wvHtml "${MC_EXT_FILENAME}" --targetdir="$tmp" page.html -1
            elinks "$tmp/page.html"
            rm -rf "$tmp"
        fi
        ;;
    msxls)
        if [ -n "$DISPLAY" ]; then
            OOFFICE=`get_ooffice_executable`
            if [ -n "${OOFFICE}" ]; then
                (${OOFFICE} "${MC_EXT_FILENAME}" &)
            else
                (gnumeric "${MC_EXT_FILENAME}" >/dev/null 2>&1 &)
            fi
        else
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            xlhtml -a "${MC_EXT_FILENAME}" > "$tmp/page.html"
            elinks "$tmp/page.html"
            rm -rf "$tmp"
        fi
        ;;
    msppt)
        if [ -n "$DISPLAY" ]; then
            OOFFICE=`get_ooffice_executable`
            if [ -n "${OOFFICE}" ]; then
                (${OOFFICE} "${MC_EXT_FILENAME}" &)
            fi
        else
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            ppthtml "${MC_EXT_FILENAME}" > "$tmp/page.html"
            elinks "$tmp/page.html"
            rm -rf "$tmp"
        fi
        ;;
    framemaker)
        fmclient -f "${MC_EXT_FILENAME}"
        ;;
    dvi)
        if [ -n "$DISPLAY" ]; then
            (xdvi "${MC_EXT_FILENAME}" &)
        else
            dvisvga "${MC_EXT_FILENAME}" || \
                dvi2tty "${MC_EXT_FILENAME}" | ${PAGER:-more}
        fi
        ;;
    djvu)
        djview "${MC_EXT_FILENAME}" &
        ;;
    comic)
        cbrpager "${MC_EXT_FILENAME}" &
        ;;
    ebook)
        lucidor "${MC_EXT_FILENAME}" >/dev/null &
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
