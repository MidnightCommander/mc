# Comment: demonstrate all Python syntax highlighting features
# This file exercises every TS capture from python-highlights.scm

import os
from sys import argv as args

# Keywords
def greet(name):
    if name is not None:
        print("Hello, " + name)
    elif name == "":
        pass
    else:
        raise ValueError("empty")

class Animal:
    """Docstring for Animal class."""

    def __init__(self, name):
        self.name = name

    def __repr__(self):
        return f"Animal({self.name})"

    def __str__(self):
        return self.name

    def __len__(self):
        return len(self.name)

    def __eq__(self, other):
        return self.name == other.name

# Async/await
async def fetch_data():
    await some_coroutine()
    return True

# Loop keywords
for i in range(10):
    if i == 5:
        continue
    if i == 8:
        break

while True:
    break

# Try/except/finally
try:
    x = 1 / 0
except ZeroDivisionError:
    pass
finally:
    del x

# With statement
with open("/dev/null") as f:
    pass

# Lambda
square = lambda x: x * x

# Yield
def gen():
    yield 1
    yield from range(5)

# Global/nonlocal
counter = 0
def increment():
    global counter
    counter += 1

def outer():
    x = 10
    def inner():
        nonlocal x
        x += 1

# Assert
assert True, "should be true"

# Operators
a = 10
b = 20
_ = a + b
_ = a - b
_ = a * b
_ = a ** 2
_ = a / b
_ = a // b
_ = a % b
_ = a | b
_ = a & b
_ = a ^ b
_ = ~a
_ = a << 2
_ = a >> 2
_ = a == b
_ = a != b
_ = a < b
_ = a > b
_ = a <= b
_ = a >= b
c = 0
c += 1
c -= 1
c *= 2
c /= 2
c //= 2
c %= 3
c **= 2
c <<= 1
c >>= 1
c &= 0xFF
c |= 0x01
c ^= 0x10

# Walrus operator
if (n := len("hello")) > 3:
    pass

# Type annotation arrow
def add(x: int, y: int) -> int:
    return x + y

# At operator (matrix multiply)
# result = matrix_a @ matrix_b

# Colon
data: dict = {"key": "value"}

# Brackets and delimiters
lst = [1, 2, 3]
tpl = (4, 5, 6)
st = {7, 8, 9}

# Semicolons
a = 1; b = 2; c = 3

# Strings
s1 = "double quoted"
s2 = 'single quoted'
s3 = """triple double
quoted"""
s4 = '''triple single
quoted'''

# Escape sequences
s5 = "newline\n tab\t backslash\\"

# self reference
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

# Dunder names
class Custom:
    __slots__ = ("_value",)
    def __getitem__(self, key):
        return self._value

# Decorators
@staticmethod
def helper():
    pass

class Decorated:
    @classmethod
    def create(cls):
        return cls()

    @property
    def value(self):
        return 42

# Concatenated strings
msg = ("hello " "world")
