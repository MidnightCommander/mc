% S-Lang sample file for syntax highlighting comparison
% This exercises TS captures from slang-highlights.scm (inherits: c)

#include <stdio.h>
#define MAX_SIZE 100

% Variable definitions and types
variable x = 10;
variable name = "hello world";
variable flag = 1;

% S-Lang types
typedef struct {
    Char_Type ch;
    Integer_Type count;
    String_Type label;
    Double_Type value;
    Float_Type ratio;
    Array_Type items;
    Assoc_Type table;
} MyStruct;

% Function definition
define greet(name)
{
    variable msg = sprintf("Hello, %s!\n", name);
    return msg;
}

% Function with multiple parameters
define add(a, b)
{
    return a + b;
}

% If/else control flow
define check_value(x)
{
    if (x > 0)
    {
        printf("positive: %d\n", x);
    }
    else if (x < 0)
    {
        printf("negative: %d\n", x);
    }
    else
    {
        printf("zero\n");
    }
}

% While loop
define countdown(n)
{
    while (n > 0)
    {
        printf("%d\n", n);
        n = n - 1;
    }
}

% Foreach loop
define show_items(list)
{
    variable item;
    foreach item (list)
    {
        printf("item: %s\n", item);
    }
}

% Forever loop with break
define wait_loop()
{
    variable count = 0;
    forever
    {
        count++;
        if (count >= 10)
            break;
    }
}

% Switch/case
define describe(code)
{
    switch (code)
    {
        case 1: return "one";
        case 2: return "two";
        default: return "other";
    }
}

% String operations with escape sequences
variable s1 = "tab:\there\n";
variable s2 = "backslash: \\";
variable s3 = "quote: \"inside\"";
variable s4 = "octal: \101\102";

% Array operations
variable arr = Integer_Type[5];
arr[0] = 42;
arr[1] = arr[0] * 2;

% Associative array
variable dict = Assoc_Type[String_Type];

% Error handling blocks
define risky()
{
    EXIT_BLOCK
    {
        printf("cleanup\n");
    }
    ERROR_BLOCK
    {
        printf("error occurred\n");
    }
}

% Logical operators
variable result = (x > 0) and (x < 100);
variable other = (x == 0) or (flag != 0);
variable inv = not flag;
variable bits = x xor 0xFF;

% Operators
variable sum = x + 10;
variable diff = x - 5;
variable prod = x * 3;
variable quot = x / 2;
variable eq = (x == 10);
variable ne = (x != 5);
variable gt = (x > 0);
variable lt = (x < 100);
