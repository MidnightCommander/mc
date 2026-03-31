#!/usr/bin/awk -f
# Sample AWK file demonstrating syntax highlighting features
# Exercises all tree-sitter captures from awk-highlights.scm

# BEGIN and END blocks
BEGIN {
    FS = ":"
    OFS = "\t"
    RS = "\n"
    ORS = "\n"
    count = 0
    print "Starting processing"
}

END {
    printf "Processed %d records\n", count
}

# BEGINFILE and ENDFILE (gawk extensions)
BEGINFILE {
    print "Reading: " FILENAME
}

ENDFILE {
    print "Done with: " FILENAME
}

# Function definition
function max(a, b) {
    if (a > b)
        return a
    else
        return b
}

# func keyword (gawk extension)
func min(a, b) {
    return (a < b) ? a : b
}

# Pattern-action with regex
/^#/ { next }
/^$/ { nextfile }

# Field references
{
    first = $1
    second = $2
    last = $NF
    whole = $0
}

# Arithmetic operators
{
    sum = $1 + $2
    diff = $1 - $2
    prod = $1 * $2
    quot = $1 / $2
    mod = $1 % $2
    power = $1 ^ 2
    power2 = $1 ** 2
}

# Assignment operators
{
    x = 1
    x += 10
    x -= 5
    x *= 2
    x /= 3
    x %= 7
    x ^= 2
}

# Comparison and logical operators
{
    if (x == 1 && y != 2)
        print "match"
    if (x < 10 || x > 100)
        print "range"
    if (x >= 5 && x <= 50)
        print "in range"
    if (!found)
        print "not found"
}

# Regex match operators
$0 ~ /pattern/ { print "matched" }
$0 !~ /exclude/ { print "not excluded" }

# Increment and decrement
{
    count++
    x--
    ++count
    --x
}

# Ternary operator
{
    result = (x > 0) ? "positive" : "non-positive"
}

# Control flow
{
    for (i = 1; i <= NF; i++) {
        if ($i == "skip")
            continue
        if ($i == "stop")
            break
        print $i
    }
}

# do-while loop
{
    i = 0
    do {
        i++
    } while (i < 10)
}

# for-in loop with delete
{
    for (key in arr) {
        print key, arr[key]
    }
    delete arr
}

# switch-case (gawk)
{
    switch ($1) {
        case "a":
            print "alpha"
            break
        case "b":
            print "beta"
            break
        default:
            print "other"
    }
}

# Builtin variables
{
    print NR, FNR, NF
    print ARGC, ARGV[0]
    print ENVIRON["HOME"]
    print RLENGTH, RSTART
    print SUBSEP
    print OFMT, CONVFMT
}

# getline
{
    getline line < "/etc/hostname"
    "date" | getline today
}

# String with escape sequences
{
    print "tab:\there"
    print "newline:\nhere"
    printf "%s has %d items (%.2f%%)\n", name, count, pct
}

# exit
END {
    exit 0
}

# print and printf
{
    print "hello", "world"
    printf "%10s %5d\n", $1, $2
}
