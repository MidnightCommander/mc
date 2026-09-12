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
    if command -v loffice >/dev/null 2>&1; then
        echo "loffice"
    elif command -v ooffice >/dev/null 2>&1; then
        echo "ooffice"
    else
        echo -n
    fi
}

format_markdown_table() {
    table_file=$1
    wrap_w=28

    awk -v W="$wrap_w" '
    function trim(s) { sub(/^[[:space:]]+/, "", s); sub(/[[:space:]]+$/, "", s); return s }
    function rep(ch, n,    i, out) { out=""; for (i=0; i<n; i++) out=out ch; return out }
    function boldify(s,    out,i,ch) {
        out=""
        for (i=1; i<=length(s); i++) {
            ch=substr(s,i,1)
            if (ch == " ")
                out = out ch
            else
                out = out ch "\b" ch
        }
        return out
    }
    function push_seg(r, c, s,    k) {
        k = ++cnt[r, c]
        seg[r, c, k] = s
        if (length(s) > colw[c])
            colw[c] = length(s)
    }
    function add_wrapped_text(r, c, txt, w,    s, n, i, word, cur, chunk) {
        s = trim(txt)
        if (s == "") {
            push_seg(r, c, "")
            return
        }

        n = split(s, a, /[[:space:]]+/)
        cur = ""
        for (i = 1; i <= n; i++) {
            word = a[i]
            if (word == "")
                continue

            while (length(word) > w) {
                if (cur != "") {
                    push_seg(r, c, cur)
                    cur = ""
                }
                chunk = substr(word, 1, w)
                push_seg(r, c, chunk)
                word = substr(word, w + 1)
            }

            if (cur == "")
                cur = word
            else if (length(cur) + 1 + length(word) <= w)
                cur = cur " " word
            else {
                push_seg(r, c, cur)
                cur = word
            }
        }

        if (cur != "")
            push_seg(r, c, cur)
    }
    function is_sep_row(fields, nf,    i, t) {
        if (nf < 2)
            return 0
        for (i = 1; i <= nf; i++) {
            t = trim(fields[i])
            if (t !~ /^:?-+:?$/)
                return 0
        }
        return 1
    }
    {
        line = $0
        sub(/^[[:space:]]*\|/, "", line)
        sub(/\|[[:space:]]*$/, "", line)

        nf = split(line, f, /\|/)
        if (is_sep_row(f, nf))
            next

        r++
        if (nf > maxc)
            maxc = nf

        for (c = 1; c <= nf; c++)
            raw[r, c] = trim(f[c])
    }
    END {
        if (r == 0)
            exit

        for (row = 1; row <= r; row++) {
            row_len = 0
            wrap_row[row] = 0
            for (c = 1; c <= maxc; c++) {
                cell_len = length(raw[row, c])
                row_len += cell_len
                if (cell_len > W)
                    wrap_row[row] = 1
            }
            row_len += (maxc > 0 ? (maxc - 1) * 3 : 0)
            if (row_len > 75)
                wrap_row[row] = 1
        }

        for (row = 1; row <= r; row++) {
            for (c = 1; c <= maxc; c++) {
                if (wrap_row[row])
                    add_wrapped_text(row, c, raw[row, c], W)
                else
                    push_seg(row, c, raw[row, c])
            }
        }

        for (row = 1; row <= r; row++) {
            rowh = 1
            for (c = 1; c <= maxc; c++) {
                if (cnt[row, c] > rowh)
                    rowh = cnt[row, c]
            }

            for (k = 1; k <= rowh; k++) {
                out = ""
                for (c = 1; c <= maxc; c++) {
                    cell = seg[row, c, k]
                    if (cell == "")
                        cell = ""
                    pad = colw[c] - length(cell)
                    if (row == 1)
                        cell = boldify(cell)
                    out = out (c == 1 ? "" : " │ ") cell rep(" ", pad)
                }
                print out
            }

            sep = ""
            for (c = 1; c <= maxc; c++)
                sep = sep (c == 1 ? "" : "─┼─") rep("─", colw[c])
            print sep
        }
    }
    ' "$table_file"
}

render_markdown_two_colors() {
    awk '
    function is_table_sep(s) {
        return (s ~ /^[[:space:]]*\|?[[:space:]]*:?-+:?[[:space:]]*(\|[[:space:]]*:?-+:?[[:space:]]*)+\|?[[:space:]]*$/)
    }
    function boldify(s,    out,i,c) {
        out=""
        for (i=1; i<=length(s); i++) {
            c=substr(s,i,1)
            if (c == " ")
                out = out c
            else
                out = out c "\b" c
        }
        return out
    }
    function underline_code(s,    out,i,c,in_code) {
        out=""
        in_code=0
        for (i=1; i<=length(s); i++) {
            c=substr(s,i,1)
            if (c == "`") {
                in_code = !in_code
                continue
            }
            if (in_code && c != " ")
                out = out "_\b" c
            else
                out = out c
        }
        return out
    }
    {
        lines[++n] = $0
    }
    END {
        for (i=1; i<=n; i++) {
            if (index(lines[i], "|") > 0 && i < n && is_table_sep(lines[i+1])) {
                print "__MC_TABLE_BEGIN__"
                print lines[i]
                i++
                print lines[i]
                while (i < n && lines[i+1] ~ /\|/ && lines[i+1] !~ /^[[:space:]]*$/) {
                    i++
                    print lines[i]
                }
                print "__MC_TABLE_END__"
                continue
            }

            line = lines[i]
            if (match(line, /^#+[ \t]*/)) {
                rest = substr(line, RLENGTH + 1)
                print boldify(rest)
            } else {
                print underline_code(line)
            }
        }
    }
    ' "$MC_EXT_FILENAME" | {
        in_table=0
        table_tmp=""
        while IFS= read -r line; do
            if [ "$line" = "__MC_TABLE_BEGIN__" ]; then
                in_table=1
                table_tmp=$(mktemp /tmp/mc-md-table.XXXXXX) || exit 1
                continue
            fi
            if [ "$line" = "__MC_TABLE_END__" ]; then
                in_table=0
                format_markdown_table "$table_tmp"
                rm -f "$table_tmp"
                table_tmp=""
                continue
            fi

            if [ "$in_table" -eq 1 ]; then
                printf '%s\n' "$line" >> "$table_tmp"
            else
                printf '%s\n' "$line"
            fi
        done
    }
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
        if command -v wvHtml >/dev/null 2>&1; then
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            wvHtml "${MC_EXT_FILENAME}" --targetdir="$tmp" page.html
            elinks -dump "$tmp/page.html"
            rm -rf "$tmp"
        elif command -v antiword >/dev/null 2>&1; then
            antiword -t "${MC_EXT_FILENAME}"
        elif command -v catdoc >/dev/null 2>&1; then
            catdoc -w "${MC_EXT_FILENAME}"
        elif command -v word2x >/dev/null 2>&1; then
            word2x -f text "${MC_EXT_FILENAME}" -
        else
            strings "${MC_EXT_FILENAME}"
        fi
        ;;
    msxls)
        if command -v xlhtml >/dev/null 2>&1; then
            tmp=`mktemp -d ${TMPDIR:-/tmp}/%p.XXXXXX`
            xlhtml -a "${MC_EXT_FILENAME}" > "$tmp/page.html"
            elinks -dump "$tmp/page.html"
            rm -rf "$tmp"
        elif command -v xls2csv >/dev/null 2>&1; then
            xls2csv "${MC_EXT_FILENAME}"
        else
            strings "${MC_EXT_FILENAME}"
        fi
        ;;
    dvi)
        command -v dvi2tty >/dev/null 2>&1 && \
            dvi2tty "${MC_EXT_FILENAME}" || \
            catdvi "${MC_EXT_FILENAME}"
        ;;
    djvu)
        djvused -e print-pure-txt "${MC_EXT_FILENAME}"
        ;;
    ebook)
        einfo -v "${MC_EXT_FILENAME}"
        ;;
    markdown)
        render_markdown_two_colors
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
