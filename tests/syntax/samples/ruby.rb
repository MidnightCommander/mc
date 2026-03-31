#!/usr/bin/ruby
# Sample file demonstrating Ruby syntax highlighting features.
# Exercises all captures from the TS query override.

# Keywords -> magenta
require 'json'
require_relative 'helper'

module MyModule
  class MyClass
    attr_reader :name
    attr_writer :age
    attr_accessor :email

    def initialize(name, age)
      @name = name
      @age = age
      self
    end

    def greet
      return "Hello, #{@name}"
    end

    public
    def public_method
      yield if block_given?
    end

    private
    def private_method
      super
    end

    protected
    def protected_method
      nil
    end
  end
end

# Control flow
if true
  puts "true"
elsif false
  puts "false"
else
  puts "other"
end

unless nil
  puts "not nil"
end

# Loops
while true
  break
end

until false
  break
end

for i in 0..10
  next if i == 5
  redo if i == 3
end

case 42
when 1
  puts "one"
when 42
  puts "forty-two"
end

# Exception handling
begin
  raise "error"
rescue => e
  retry if false
ensure
  puts "cleanup"
end

# Boolean and nil constants -> brightred
x = true
y = false
z = nil

# Operators -> yellow
a = 1 + 2
b = 5 - 3
c = 4 * 2
d = 10 / 2
e = 7 % 3
f = 2 ** 8

# Comparison operators
eq = (1 == 2)
neq = (1 != 2)
teq = (1 === 2)
cmp = (1 <=> 2)
lt = (1 < 2)
gt = (1 > 2)
lte = (1 <= 2)
gte = (1 >= 2)

# Logical operators
g = true && false
h = true || false
i = !true
j = true & false
k = true | false
l = true ^ false
m = ~0

# Bitshift operators
n = 1 << 4
o = 16 >> 2

# Range operators
range1 = 1..10
range2 = 1...10

# Assignment operators
p = 0
p += 1
p -= 1
p *= 2
p /= 2
p ||= 42
p &&= 0

# Pattern matching
result = ("hello" =~ /hel/)
nomatch = ("hello" !~ /xyz/)

# Hash rocket
hash = {a: 1, "b" => 2}

# Brackets/parens/delimiters -> brightcyan
arr = [1, 2, 3]
hash2 = {key: "value"}
method_call(arg1, arg2)

# Strings -> green
str1 = "double quoted"
str2 = 'single quoted'
str3 = %w[one two three]
heredoc = <<~HEREDOC
  This is a heredoc
  with multiple lines
HEREDOC

# Escape sequences -> brightgreen
esc = "tab\there\nnewline"

# Regex -> brightgreen
regex = /pattern/i

# Symbols -> white
sym1 = :symbol
sym2 = :"dynamic symbol"
hash3 = {key: "value"}

# Global variables -> brightgreen
$global_var = "global"

# Instance variables -> white
@instance_var = "instance"

# do..end blocks
[1, 2, 3].each do |item|
  puts item
end

# alias, undef, not, and, or
alias new_method greet
undef_method :old if false
result2 = not false
result3 = true and false
result4 = true or false

# BEGIN/END
BEGIN { puts "start" }
END { puts "finish" }

# include, extend
include Comparable if false
extend Enumerable if false

# Comment
# This is a line comment
