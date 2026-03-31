/* Block comment: demonstrate all C color features */
/* TODO: fix this */
/* NOTE: important !! */

// Line comment
// FIXME: broken

#include <stdio.h>
#include "myheader.h"

#define MAX_SIZE 100
#define SQUARE(x) ((x) * (x))

#ifdef DEBUG
#define LOG(msg) printf("%s\n", msg)
#else
#define LOG(msg)
#endif

#if defined(__GNUC__)
#pragma GCC optimize("O2")
#endif

/* Keywords */
typedef struct {
    int x;
    int y;
} Point;

typedef union {
    int i;
    float f;
} Value;

enum Color { RED, GREEN, BLUE };

/* Data types and modifiers */
void func_void(void);
int func_int(int a);
char func_char(char c);
float func_float(float f);
double func_double(double d);
short func_short(short s);
long func_long(long l);
signed int si;
unsigned int ui;
wchar_t wc;

/* Storage class specifiers */
auto int auto_var;
extern int extern_var;
register int reg_var;
static int static_var;
const int const_var = 42;
volatile int vol_var;
inline int inline_func(int x) { return x; }

/* C11/C23 keywords */
_Bool bool_var;
_Atomic int atomic_var;
_Noreturn void abort_func(void);
static_assert(sizeof(int) >= 4, "int too small");
alignas(16) int aligned_var;
constexpr int ce_var = 10;

/* Constants */
int *p = NULL;
_Bool b1 = true;
_Bool b2 = false;
void *np = nullptr;

/* Numbers */
int dec = 42;
int hex = 0xFF;
int oct = 077;
int zero = 0;
long lng = 42L;
unsigned uns = 42u;
float flt = 3.14f;
double dbl = 3.14;
double sci = 1.5e10;
double sci2 = 1.5E-3;
int with_sep = 1_000_000;

/* Characters */
char ch1 = 'a';
char ch2 = '\n';
char ch3 = '\'';
char ch4 = '\\';
char ch5 = '\0';
char ch6 = '\077';

/* Strings */
char *str1 = "hello world";
char *str2 = "escape: \t\n\\\"";
char *str3 = "format: %d %s %f %x %p %c %%";
char *str4 = "format: %04d %-10s %6.2f %#08x";

/* Labels and goto */
void label_func(void) {
    goto done;
done:
    return;
}

/* Operators */
int arithmetic(int a, int b) {
    int r;
    r = a + b;
    r = a - b;
    r = a * b;
    r = a / b;
    r = a % b;
    r++;
    r--;
    r += a;
    r -= b;
    return r;
}

/* Comparison and logical */
int compare(int a, int b) {
    if (a == b) return 1;
    if (a != b) return 0;
    if (a < b) return -1;
    if (a > b) return 1;
    if (a <= b && a >= 0) return a;
    if (a || b) return a;
    if (!a) return b;
    return 0;
}

/* Bitwise operators */
int bitwise(int a, int b) {
    int r;
    r = a & b;
    r = a | b;
    r = a ^ b;
    r = ~a;
    r = a << 2;
    r = a >> 1;
    return r;
}

/* Ternary operator */
int ternary(int a) {
    return a > 0 ? a : -a;
}

/* Pointers and field access */
void pointers(Point *pt) {
    int val = pt->x;
    Point local;
    local.x = val;
    int *ptr = &local.x;
    *ptr = 42;
}

/* Switch/case */
int switch_func(int x) {
    switch (x) {
    case 0:
        return 0;
    case 1:
        return 1;
    default:
        break;
    }
    return -1;
}

/* Loops */
void loops(void) {
    int i;

    for (i = 0; i < 10; i++) {
        if (i == 5) continue;
        if (i == 8) break;
    }

    while (i > 0) {
        i--;
    }

    do {
        i++;
    } while (i < 5);
}

/* Sizeof */
void sizes(void) {
    size_t s1 = sizeof(int);
    size_t s2 = sizeof(Point);
    size_t s3 = sizeof(char *);
}

/* Function pointers */
int (*func_ptr)(int, int) = NULL;

/* Variadic */
void variadic(const char *fmt, ...);

/* Main */
int main(int argc, char *argv[]) {
    printf("Hello, %s! Count: %d\n", argv[0], argc);
    return 0;
}
