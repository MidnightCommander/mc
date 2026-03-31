// D syntax sample: demonstrates all TS capture groups
// This file exercises every capture in d-highlights.scm

// import (@keyword.directive -> magenta)
module syntax_demo;
import std.stdio;
import std.string;

// Keywords (@keyword -> yellow)
class Shape {
    abstract void draw();
    final int getId() { return 0; }
}

struct Point {
    int x;
    int y;
}

union Value {
    int i;
    float f;
}

enum Color { Red, Green, Blue }

interface Drawable {
    void render();
}

// More keywords
void keywords_demo() {
    alias IntArray = int[];
    assert(true);
    const int cv = 42;
    immutable int iv = 100;
    static int sv = 0;
    extern int ev;
    export void efunc() {}
    deprecated("old") void oldFunc() {}
    pragma(msg, "compiling");
    synchronized void syncFunc() {}
    scope(exit) {}
    nothrow void ntFunc() {}
    pure int pureFunc() { return 0; }
    lazy int lazyParam;
    override void draw() {}
    private int priv;
    protected int prot;
    public int pub;
    package int pkg;
    ref int refParam;
    shared int sharedVar;
    mixin template Mix() {}
    version(linux) {}
    debug {}
}

// Control flow keywords (@keyword -> yellow)
void control_flow() {
    for (int i = 0; i < 10; i++) {
        if (i == 5) continue;
        if (i == 8) break;
    }
    foreach (item; [1, 2, 3]) {}
    foreach_reverse (item; [3, 2, 1]) {}
    while (true) { break; }
    do { } while (false);
    switch (1) {
        case 0: break;
        default: break;
    }
    try {
        throw new Exception("error");
    } catch (Exception e) {
    } finally {
    }
    goto end;
end:
    return;
}

// Types (@type -> yellow)
void type_demo() {
    auto a = 42;
    bool b = true;
    byte by = 0;
    ubyte uby = 0;
    short s = 0;
    ushort us = 0;
    int i = 0;
    uint ui = 0;
    long l = 0;
    ulong ul = 0;
    float f = 1.0;
    double d = 1.0;
    real r = 1.0;
    char c = 'x';
    wchar wc;
    dchar dc;
    string str = "hello";
    wstring ws;
    dstring ds;
    size_t sz;
    ptrdiff_t pt;
    void* vp;
}

// true, false, null, super, this (@variable.builtin -> brightred)
class Derived : Shape {
    this() { super(); }
    bool active = true;
    bool inactive = false;
    Object obj = null;
    void method() { this.draw(); }
}

// Special keywords (@function.special -> brightred)
void special() {
    auto f = __FILE__;
    auto l = __LINE__;
}

// module_fqn (@type -> yellow)
// Covered by: module syntax_demo; above

// Operators (@operator.word -> yellow)
void operators() {
    int a = 10;
    int b = 3;
    int r;
    r = a + b;
    r = a - b;
    r = a * b;
    r = a / b;
    r = a % b;
    r = a ^^ b;
    r++;
    r--;
    r += a;
    r -= b;
    r *= 2;
    r /= 2;
    r %= 3;
    if (a == b) {}
    if (a != b) {}
    if (a < b) {}
    if (a > b) {}
    if (a <= b) {}
    if (a >= b) {}
    if (a && b) {}
    if (a || b) {}
    if (!a) {}
    r = a & b;
    r = a | b;
    r = a ^ b;
    r = ~a;
    r = a << 2;
    r = a >> 1;
    r &= b;
    r |= b;
    r ^= b;
    r <<= 1;
    r >>= 1;
    auto rng = 0 .. 10;
    auto fn = (int x) => x * x;
    int[string] aa;
    aa["key" : "val"];
}

// String literals (@string -> green)
string s1 = "Hello, D!";
string raw = r"raw\nstring";
string hex = x"48 65 6C 6C 6F";
string tok = q{auto x = 1;};

// Character literal (@string.special -> brightgreen)
char c1 = 'A';
char c2 = '\n';
char c3 = '\\';

// Labels (@label -> cyan)
void label_demo() {
    goto target;
target:
    return;
}

// Delimiters (@delimiter -> brightcyan): . ,
// Semicolons (@delimiter.special -> brightmagenta): ;

// Comments (@comment -> brown)
/* Block comment */
// Line comment

// unittest, delegate, function, template, typeid, typeof
unittest {
    assert(typeid(int) !is null);
    auto dg = delegate int() { return 42; };
    auto fn = function int() { return 0; };
    typeof(42) x;
    delete x;
    new int;
    with (Point(1, 2)) {}
}
