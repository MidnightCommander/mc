#ifndef MC__TEST
#define MC__TEST

#include <config.h>
#include <stdlib.h>
#include <check.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define mctest_add_parameterized_test(tc_core, test_func, test_data_source) { \
    tcase_add_loop_test (tc_core, test_func, 0, G_N_ELEMENTS (test_data_source)); \
}

#define mctest_assert_str_eq(actual_result, etalon_result) { \
    g_assert_cmpstr (actual_result, ==, etalon_result); \
}

#define mctest_assert_ptr_eq(actual_pointer, etalon_pointer) { \
    ck_assert_msg ( actual_pointer == etalon_pointer, \
        "%s(%p) pointer should be equal to %s(%p)\n", \
        #actual_pointer, actual_pointer, #etalon_pointer , etalon_pointer \
    ); \
}

#define mctest_assert_ptr_ne(actual_pointer, etalon_pointer) { \
    ck_assert_msg ( actual_pointer != etalon_pointer, \
        "%s(%p) pointer should not be equal to %s(%p)\n", \
        #actual_pointer, actual_pointer, #etalon_pointer , etalon_pointer \
    ); \
}

#define mctest_assert_null(actual_pointer) { \
    ck_assert_msg ( \
        (void *) actual_pointer == NULL, \
        "%s(%p) variable should be NULL", #actual_pointer, actual_pointer \
    ); \
}

#define mctest_assert_not_null(actual_pointer) { \
    ck_assert_msg ( \
        (void *) actual_pointer != NULL, \
        "%s(nil) variable should not be NULL", #actual_pointer \
    ); \
}

#define mctest_assert_true(actual_pointer) { \
    ck_assert_msg ( \
        (int) actual_pointer != 0, \
        "%s variable should be TRUE", #actual_pointer \
    ); \
}

#define mctest_assert_false(actual_pointer) { \
    ck_assert_msg ( \
        (int) actual_pointer == 0, \
        "%s variable should be FALSE", #actual_pointer \
    ); \
}

/**
 * Define header for a parameterized test.
 * Declare 'data' variable for access to the parameters in current iteration
 */
#define START_PARAMETRIZED_TEST(name_test, struct_name) \
    START_TEST (name_test) { \
        const struct struct_name *data = &struct_name[_i];

/**
 * Define footer for a parameterized test.
 */
#define END_PARAMETRIZED_TEST \
    } END_TEST

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

static inline int
mctest_run_all (TCase *tc_core)
{
    Suite *s;
    SRunner *sr;
    int number_failed;

    s = suite_create (TEST_SUITE_NAME);
    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "-");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif /* MC__TEST */
