#!/usr/bin/tclsh
# Tcl syntax highlighting sample file
# Exercises all TS captures: keyword, comment, string,
# string.special (variables, escapes), braced_word

package require Tcl 8.6

# Namespace and variable declarations
namespace eval ::myapp {
    variable version "1.0"
    variable count 0
}

# Procedure definition with multiple args
proc greet {name {greeting "Hello"}} {
    puts "$greeting, $name!"
    return "$greeting $name"
}

# Set and expr usage
set x 42
set y [expr {$x * 2 + 10}]
set name "world"
set result [expr {($x + $y) / 3}]

# String operations
set msg "Line one\nLine two\ttabbed"
set upper [string toupper $name]
set len [string length $name]
set sub [string range $name 0 2]

# List operations
set colors {red green blue yellow}
set first [lindex $colors 0]
lappend colors "purple"
set sorted [lsort $colors]
set found [lsearch $colors "blue"]
set count [llength $colors]
set replaced [lreplace $colors 0 0 "orange"]

# If/elseif/else control flow
if {$x > 100} {
    puts "Large"
} elseif {$x > 50} {
    puts "Medium"
} else {
    puts "Small: $x"
}

# While loop
set i 0
while {$i < 5} {
    puts "Iteration $i"
    incr i
}

# Foreach loop
foreach color $colors {
    puts "Color: $color"
}

# Regexp matching
set text "Error: code 404"
if {[regexp {(\w+):\s+code\s+(\d+)} $text match word code]} {
    puts "Word=$word Code=$code"
}
regsub -all {[aeiou]} $name "*" masked

# Try/catch/finally error handling
try {
    set fd [open "/tmp/test.txt" w]
    puts $fd "test data"
    close $fd
} on error {msg opts} {
    puts "Error: $msg"
} finally {
    puts "Cleanup done"
}

# Eval and exec
eval {set dynamic 99}
set output [exec ls /tmp]

# Global and upvar
proc modify_global {} {
    global count
    upvar 1 x local_x
    incr count
    set local_x [expr {$local_x + 1}]
}

# Array operations
array set config {
    host "localhost"
    port 8080
    debug 1
}
set keys [array names config]

# Format and scan
set formatted [format "Value: %05d (%.2f)" $x 3.14]
scan "42 hello" "%d %s" num word

# Info, rename, trace
set procs [info procs]
rename greet old_greet
trace add variable x write {apply {{n1 n2 op} {
    puts "Variable $n1 changed"
}}}

# Source, after, vwait, update
after 1000 {puts "Delayed message"}
update idletasks

# Uplevel for meta-programming
proc debug_eval {script} {
    uplevel 1 $script
}

# Variable substitution in different contexts
set path "/home/${name}/docs"
set ref $::myapp::version
set arr_val $config(host)

# Braced word (no substitution)
set pattern {[A-Za-z]+\d+}

# Switch statement
switch -exact -- $x {
    42      {puts "The answer"}
    default {puts "Unknown"}
}

# Escaped characters in strings
set special "Tab:\there\nNewline\\Backslash"

puts "All tests completed: $result"
