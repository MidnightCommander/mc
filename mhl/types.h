/* 

    Micro Helper Library: generic type declarations

*/

#ifndef MHL_TYPES_H
#define MHL_TYPES_H

#if !defined(__bool_true_false_are_defined) && !defined(false) && !defined(true) && !defined(bool)
typedef enum
{
    false	= 0,
    true	= 1
} bool;
#endif

#endif
