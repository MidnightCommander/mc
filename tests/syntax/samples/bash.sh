#!/bin/bash
# Comment: demonstrate all bash color features

# Variable assignment
FOO="hello"
BAR='world'
NUM=42

# Keywords: if/then/elif/else/fi
if [ -f /tmp/test ]; then
    echo "file exists"
elif [ -d /tmp ]; then
    printf "dir exists\n"
else
    exit 1
fi

# Keywords: for/do/done/in
for i in 1 2 3; do
    continue
done

# Keywords: while/until
while read -r line; do
    break
done < /tmp/test

until true; do
    shift
done

if [[ "x" -eq "y" ]]; then
    umpopssible
fi

# Keywords: case/esac with ;;
case "$FOO" in
    hello) echo "matched";;
    *)     echo "default";;
esac

# Keywords: select
select opt in "yes" "no"; do
    break
done

# Keywords: declare/local/export/readonly/typeset/unset
declare -i COUNT=0
export PATH
readonly PI=3
unset FOO

# Function definitions
function msg() {
    local val="$1"
    return 0
}

msg_short() {
    echo "short"
}

# Function call
msg foo
msg_short

# Variable expansions
echo $FOO
echo ${BAR}
echo "$FOO and ${BAR}"
echo "positional: $0 $1 $9"
echo "special: $? $# $@ $* $$ $! $_ $-"

# Command substitution
RESULT=$(date +%s)
OLD=`uname -r`

# Strings
echo "double quoted with $VAR and ${VAR}"
echo 'single quoted no $expansion'
cat <<EOF
heredoc with $FOO expansion
EOF
cat <<'EOF'
heredoc without expansion
EOF

# External commands (cyan in sh.syntax)
cat /etc/hostname
grep -r "pattern" /tmp
ls -la /
mkdir -p /tmp/test
rm -rf /tmp/test
sed 's/old/new/' file
awk '{print $1}' file
find / -name "*.txt"
sort -u file
wc -l file

# Security commands (red in sh.syntax)
gpg --verify file.sig
ssh user@host
scp file user@host:/tmp
openssl genrsa 2048
md5sum file

# Operators and redirections
echo "out" > /tmp/out
echo "append" >> /tmp/out
cat < /tmp/in
cmd 2>&1
cmd &>/dev/null

# Logical and pipe operators
true && false || echo "fallback"
cat file | grep pattern | sort

# Brackets and test
[[ -n "$FOO" ]] && echo "set"
[ -z "$BAR" ] && echo "empty"
test -x /bin/bash && echo "exec"

# File descriptors
exec 3>/tmp/fd3
echo "to fd3" >&3

# Arithmetic
(( COUNT++ ))
(( COUNT = COUNT + 1 ))

# Booleans (cyan in sh.syntax)
true
false

# Source and trap
source /etc/profile
trap 'echo bye' EXIT
wait
getopts "abc:" opt
umask 022
set -o errexit -o pipefail

# Backtick context
echo `for N in {1..2}; do ls -la; done`
