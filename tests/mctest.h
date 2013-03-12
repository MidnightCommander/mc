#ifndef MC__TEST
#define MC__TEST

#include <config.h>
#include <check.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define mctest_add_parameterized_test(tc_core, test_func, test_data_source) {\
    tcase_add_loop_test (tc_core, test_func, 0, \
                         sizeof (test_data_source) / sizeof (test_data_source[0])); \
}

#define mctest_assert_str_eq(actual_result, etalon_result) { \
    g_assert_cmpstr (actual_result, ==, etalon_result); \
}

#define mctest_assert_int_eq(actual_result, etalon_result) { \
    ck_assert_int_eq (actual_result, etalon_result); \
}

#define mctest_assert_int_ne(actual_result, etalon_result) { \
    ck_assert_int_ne (actual_result, etalon_result); \
}

#define mctest_assert_ptr_eq(actual_pointer, etalon_pointer) { \
    fail_unless ( actual_pointer == etalon_pointer, \
        "%s(%p) pointer should be equal to %s(%p)\n", \
        #actual_pointer, actual_pointer, #etalon_pointer , etalon_pointer \
    );\
}

#define mctest_assert_ptr_ne(actual_pointer, etalon_pointer) { \
    fail_unless ( actual_pointer != etalon_pointer, \
        "%s(%p) pointer should not be equal to %s(%p)\n", \
        #actual_pointer, actual_pointer, #etalon_pointer , etalon_pointer \
    );\
}

#define mctest_assert_null(actual_pointer) { \
    fail_unless( \
        (void *) actual_pointer == NULL, \
        "%s(%p) variable should be NULL", #actual_pointer, actual_pointer \
    ); \
}

#define mctest_assert_not_null(actual_pointer) { \
    fail_if( \
        (void *) actual_pointer == NULL, \
        "%s(nil) variable should not be NULL", #actual_pointer \
    ); \
}

/**
 * Define header for a parameterized test.
 * Declare 'data' variable for access to the parameters in current iteration
 */
#define START_PARAMETRIZED_TEST(name_test, struct_name) START_TEST (name_test) { const struct struct_name *data = &struct_name[_i];

/**
 * Define footer for a parameterized test.
 */
#define END_PARAMETRIZED_TEST } END_TEST

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC__TEST */
