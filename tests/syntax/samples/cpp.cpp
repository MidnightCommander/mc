/* C++ syntax sample: demonstrates all TS capture groups */
/* This file exercises every capture in cpp-highlights.scm */

// Line comment
// TODO: fix this

#include <iostream>
#include <vector>
#include <string>
#include "myheader.h"

#define MAX_SIZE 100
#define SQUARE(x) ((x) * (x))

#ifdef DEBUG
#define LOG(msg) std::cout << msg
#else
#define LOG(msg)
#endif

#if defined(__GNUC__)
#pragma GCC optimize("O2")
#endif

// Keywords (@keyword -> yellow)
class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
protected:
    int id;
private:
    std::string name;
};

struct Point {
    int x;
    int y;
};

union Value {
    int i;
    float f;
};

enum Color { Red, Green, Blue };

namespace geometry {
    using namespace std;

    template <typename T>
    concept Numeric = requires(T a) { a + a; };

    template <Numeric T>
    T add(T a, T b) noexcept {
        return a + b;
    }
}

// Primitive types (@type -> yellow)
void func_void();
int func_int(int a);
char func_char(char c);
float func_float(float f);
double func_double(double d);
short func_short(short s);
long func_long(long l);
signed int si;
unsigned int ui;
auto au = 42;

// true, false, nullptr, this (@keyword -> yellow)
bool trueval = true;
bool falseval = false;
void* np = nullptr;

// null (@keyword -> yellow)

// Operators (@operator.word -> yellow)
int arithmetic(int a, int b) {
    int r;
    r = a + b;
    r = a - b;
    r = a * b;
    r = a / b;
    r++;
    r--;
    r += a;
    r -= b;
    if (a == b) return 0;
    if (a != b) return 1;
    if (a < b) return -1;
    if (a > b) return 1;
    if (a <= b && a >= 0) return a;
    if (a || b) return a;
    if (!a) return b;
    r = a << 2;
    r = a >> 1;
    return r;
}

// Bitwise operators (@delimiter.special -> brightmagenta)
int bitwise(int a, int b) {
    int r;
    r = a & b;
    r = a | b;
    r = a ^ b;
    r = ~a;
    return r;
}

// Scope resolution (@operator.word -> yellow)
void scope_demo() {
    std::cout << "hello" << std::endl;
    geometry::add(1, 2);
}

// String literals (@string -> green)
const char* s1 = "Hello, World!";
const char* s2 = "escape: \t\n\\\"";

// System lib string (@string -> green)
// Covered by #include <iostream> above

// Raw string literal (@string -> green)
const char* raw = R"(raw string with "quotes")";

// Character literals (@string.special -> brightgreen)
char ch1 = 'A';
char ch2 = '\n';
char ch3 = '\\';
char ch4 = '\'';

// Labels (@label -> cyan)
void label_func() {
    goto done;
done:
    return;
}

// Semicolons (@delimiter.special -> brightmagenta)
// Every statement-ending ;

// Delimiters (@delimiter -> brightcyan)
// Covered by: , ( ) [ ] { } . :

// Keywords continued
class Derived : public Shape {
public:
    explicit Derived(int id) : Shape() {
        this->id = id;
    }

    double area() const override final {
        return 0.0;
    }
};

// Control flow keywords
void control_flow(int x) {
    switch (x) {
    case 0:
        break;
    default:
        break;
    }

    try {
        throw std::runtime_error("error");
    } catch (const std::exception& e) {
        // handled
    }

    for (int i = 0; i < 10; i++) {
        if (i == 5) continue;
    }

    while (x > 0) { x--; }
    do { x++; } while (x < 5);
}

// More keywords: sizeof, typedef, static_assert
void more_keywords() {
    typedef int Int32;
    static_assert(sizeof(int) >= 4, "int too small");
    constexpr int ce = 42;
    constinit static int ci = 0;
    consteval int square(int n) { return n * n; }
    mutable int m;
    volatile int v;
    extern int e;
    inline int il = 0;
    alignas(16) int al;
    alignof(int);
    decltype(ce) dt = ce;
    friend class Other;
    delete np;
    new int(42);
    static int sv;
}

// Co-routines
// co_await, co_return, co_yield

// Comments (@comment -> brown)
/* Block comment */
// Line comment
