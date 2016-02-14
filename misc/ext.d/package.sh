#!/bin/sh

# $1 - action
# $2 - type of file

action=$1
filetype=$2

[ -n "${MC_XDG_OPEN}" ] || MC_XDG_OPEN="xdg-open"

do_view_action() {
    filetype=$1

    case "${filetype}" in
    trpm)
        rpm -qivl --scripts `basename "${MC_EXT_BASENAME}" .trpm`
        ;;
    src.rpm|rpm)
        if rpm --nosignature --version >/dev/null 2>&1; then 
            RPM="rpm --nosignature"
        else
            RPM="rpm"
        fi
        $RPM -qivlp --scripts "${MC_EXT_FILENAME}"
        ;;
    deb)
        dpkg-deb -I "${MC_EXT_FILENAME}" && echo && dpkg-deb -c "${MC_EXT_FILENAME}"
        ;;
    debd)
        dpkg -s `echo "${MC_EXT_BASENAME}" | sed 's/\([0-9a-z.-]*\).*/\1/'`
        ;;
    deba)
        apt-cache show `echo "${MC_EXT_BASENAME}" | sed 's/\([0-9a-z.-]*\).*/\1/'`
        ;;
    *)
        ;;
    esac
}

do_open_action() {
    filetype=$1

    case "${filetype}" in
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
