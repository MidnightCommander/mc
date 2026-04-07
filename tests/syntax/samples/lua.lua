#!/usr/bin/env lua
-- Sample file demonstrating Lua syntax highlighting features.
-- Exercises all captures from the TS query override.

-- Keywords -> white
local x = 10
local y = 20

-- Constants -> white
local a = true
local b = false
local c = nil

-- Function definitions -> yellow
function greet(name)
    return "Hello, " .. name
end

local function add(a, b)
    return a + b
end

-- Control flow
if x == 10 then
    print("ten")
elseif x > 10 then
    print("more")
else
    print("less")
end

-- While loop
while x > 0 do
    x = x - 1
end

-- Repeat/until loop
repeat
    y = y - 1
until y == 0

-- For loops
for i = 1, 10 do
    if i == 5 then
        break
    end
end

for i = 10, 1, -1 do
    -- counting down
end

for k, v in pairs({a = 1, b = 2}) do
    print(k, v)
end

for i, v in ipairs({10, 20, 30}) do
    print(i, v)
end

-- Goto and labels
goto done

::done::
print("reached label")

-- Logical operators
local r1 = true and false
local r2 = true or false
local r3 = not true

-- Operators -> white
local sum = 1 + 2
local diff = 5 - 3
local prod = 4 * 2
local quot = 10 / 2
local idiv = 10 // 3
local mod = 7 % 3
local pow = 2 ^ 8
local len = #"hello"

-- Comparison operators
local eq = (1 == 2)
local ne = (1 ~= 2)
local lt = (1 < 2)
local gt = (1 > 2)
local le = (1 <= 2)
local ge = (1 >= 2)

-- String concatenation
local str = "hello" .. " " .. "world"

-- Brackets/parens/braces -> white
local tbl = {1, 2, 3}
local nested = {{1, 2}, {3, 4}}
local val = tbl[1]

-- Delimiters -> white
local t = {
    a = 1,
    b = 2;
    c = 3
}

-- Dot, colon access
local s = string.format("%d", 42)
local f = io.open("file.txt", "r")

-- Method call with colon
local str2 = "hello"

-- Strings -> green
local s1 = "double quoted\n"
local s2 = 'single quoted\n'
local s3 = [[
long string
with multiple lines
]]
local s4 = [==[
another long string
with equals
]==]

-- Library functions -> yellow (function calls)
print("hello")
type(42)
tostring(42)
tonumber("42")
assert(true)
error("oops")
pcall(print, "safe")
xpcall(print, print)
rawget(t, "a")
rawset(t, "d", 4)
rawequal(1, 1)
setmetatable(t, {})
getmetatable(t)
require("os")
ipairs(tbl)
pairs(t)
next(t)
collectgarbage()
loadfile("test.lua")
dofile("test.lua")

-- Library functions with dot access -> yellow
string.len("hello")
string.sub("hello", 1, 3)
string.lower("HELLO")
string.upper("hello")
string.format("%s %d", "age", 25)
string.find("hello", "ell")
string.gsub("hello", "l", "r")
string.byte("A")
string.char(65)
string.rep("ab", 3)

table.concat(tbl, ", ")
table.insert(tbl, 4)
table.remove(tbl, 1)
table.sort(tbl)

math.abs(-5)
math.floor(3.7)
math.ceil(3.2)
math.sqrt(16)
math.sin(0)
math.cos(0)
math.random()
math.max(1, 2, 3)
math.min(1, 2, 3)

io.read()
io.write("output")

os.clock()
os.time()
os.date()
os.exit(0)

-- Global variables -> brightmagenta
print(_VERSION)
print(type(_G))

-- Comments
-- Single line comment

--[[
Multi-line
block comment
]]

--[=[
Another style of
long comment
]=]
