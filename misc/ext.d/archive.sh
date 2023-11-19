#!/bin/sh

# $1 - action
# $2 - type of file
# $3 - pager

action=$1
filetype=$2
pager=$3

[ -n "${MC_XDG_OPEN}" ] || MC_XDG_OPEN="xdg-open"

do_view_action() {
    filetype=$1

    case "${filetype}" in
    gz)
        gzip -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    bz2)
        bzip2 -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    bzip)
        bzip2 -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    lz)
        lzip -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    lz4)
        lz4 -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    lzma)
        lzma -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    lzo)
        lzop -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    xz)
        xz -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    zst)
        zstd -dc "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    tar)
        tar tvvf - < "${MC_EXT_FILENAME}"
        ;;
    tar.gz|tar.qpr)
        gzip -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.bzip)
        bzip -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.bzip2)
        bzip2 -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.lz)
        lzip -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.lz4)
        lz4 -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.lzma)
        lzma -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.lzo|tzo)
        lzop -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.xz)
        xz -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.zst|tzst)
        zstd -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;
    tar.F)
        freeze -dc "${MC_EXT_FILENAME}" 2>/dev/null | \
            tar tvvf -
        ;;

    lha)
        lha l "${MC_EXT_FILENAME}" 2>/dev/null || \
            jlha l "${MC_EXT_FILENAME}" 2>/dev/null || \
            lhasa l "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    arj)
        arj l "${MC_EXT_FILENAME}" 2>/dev/null || \
            unarj l "${MC_EXT_FILENAME}" 2>/dev/null || \
            7za l "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    cab)
        cabextract -l "${MC_EXT_FILENAME}" 2> /dev/null || \
            7za l "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    ha)
        ha lf "${MC_EXT_FILENAME}"
        ;;
    rar)
        rar v -c- "${MC_EXT_FILENAME}" 2>/dev/null || \
            unrar v -c- "${MC_EXT_FILENAME}"
        ;;
    alz)
        unalz -l "${MC_EXT_FILENAME}"
        ;;
    cpio.z|cpio.gz)
        gzip -dc "${MC_EXT_FILENAME}" | \
            cpio -itv 2>/dev/null
        ;;
    cpio.lz)
        lzip -dc "${MC_EXT_FILENAME}" | \
            cpio -itv 2>/dev/null
        ;;
    cpio.lz4)
        lz4 -dc "${MC_EXT_FILENAME}" | \
            cpio -itv 2>/dev/null
        ;;
    cpio.lzo)
        lzop -dc "${MC_EXT_FILENAME}" | \
            cpio -itv 2>/dev/null
        ;;
    cpio.xz)
        xz -dc "${MC_EXT_FILENAME}" | \
            cpio -itv 2>/dev/null
        ;;
    cpio.zst)
        zstd -dc "${MC_EXT_FILENAME}" | \
            cpio -itv 2>/dev/null
        ;;
    cpio)
        cpio -itv < "${MC_EXT_FILENAME}" 2>/dev/null
        ;;
    7z)
        7za l "${MC_EXT_FILENAME}" 2>/dev/null ||
            7z l "${MC_EXT_FILENAME}"

        ;;
    ace)
        unace l "${MC_EXT_FILENAME}"
        ;;
    arc)
        arc l "${MC_EXT_FILENAME}"
        ;;
    zip)
        unzip -v "${MC_EXT_FILENAME}" 2> /dev/null || \
            7za l "${MC_EXT_FILENAME}" 2> /dev/null
        ;;
    zipx)
        7za l "${MC_EXT_FILENAME}" 2> /dev/null
        ;;
    zoo)
        zoo l "${MC_EXT_FILENAME}"
        ;;
    wim)
        wimlib-imagex info "${MC_EXT_FILENAME}" 2> /dev/null
        ;;
    *)
        ;;
    esac
}

do_open_action() {
    filetype=$1
    pager=$2

    case "${filetype}" in
    bzip2)
        bzip2 -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    bzip)
        bzip -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    gz)
        gz -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    lz)
        lzip -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    lz4)
        lz4 -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    lzma)
        lzma -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    lzo)
        lzop -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    xz)
        xz -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    zst)
        zstd -dc "${MC_EXT_FILENAME}" | ${pager}
        ;;
    par2)
        par2 r "${MC_EXT_FILENAME}"
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
        do_open_action "${filetype}" "${pager}"
    ;;
*)
    ;;
esac
