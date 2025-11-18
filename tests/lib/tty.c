/*
   lib - tty function testing

   Copyright (C) 2011-2025
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <stdio.h>

#include "lib/strutil.h"
#include "lib/util.h"

#include "lib/tty/tty.h"
#include "lib/tty/tty-internal.h"
#include "lib/tty/key.h"

static int mock_input_buf[1000];
static int *mock_input_ptr;

static void
mock_input (const char *charinput)
{
    mock_input_ptr = mock_input_buf;

    while (*charinput != '\0')
        *mock_input_ptr++ = (*charinput++ & 0xFF);
    *mock_input_ptr = '\0';

    mock_input_ptr = mock_input_buf;
}

/* @Mock */
int
tty_lowlevel_getch (void)
{
    int key = *mock_input_ptr;
    if (key != '\0')
    {
        mock_input_ptr++;
        return key;
    }
    else
        return -1;
}

/* @Mock */
int
getch_with_timeout (unsigned int delay_us)
{
    (void) (delay_us);

    return tty_lowlevel_getch ();
}

/* --------------------------------------------------------------------------------------------- */
/* @CapturedValue */
static int my_exit__status__captured;

/* @Mock */
void
my_exit (int status)
{
    my_exit__status__captured = status;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_unset)
{
    // given
    setenv ("TERM", "", 1);

    // when
    tty_check_xterm_compat (FALSE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_non_xterm)
{
    // given
    setenv ("TERM", "gnome-terminal", 1);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    ck_assert_int_eq (actual_result_force_false, 0);
    ck_assert_int_eq (actual_result_force_true, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_xterm_like)
{
    // given
    setenv ("TERM", "alacritty-terminal", 1);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    ck_assert_int_eq (actual_result_force_false, 1);
    ck_assert_int_eq (actual_result_force_true, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_get_key_code)
{
    mc_global.tty.xterm_flag = TRUE;

    setenv ("TERM", "xterm", 1);
    init_key ();
#ifdef HAVE_SLANG
    SLtt_get_terminfo ();
    load_terminfo_keys ();
#endif

    mock_input ("\x1b[2~");
    ck_assert_int_eq (get_key_code (0), KEY_IC);
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[1;2A");
    ck_assert_int_eq (get_key_code (0), KEY_M_SHIFT | KEY_UP);
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[1;2A\x1b[13;6uA");
    ck_assert_int_eq (get_key_code (0), KEY_M_SHIFT | KEY_UP);
    ck_assert_int_eq (get_key_code (0), KEY_M_CTRL | KEY_M_SHIFT | '\n');
    ck_assert_int_eq (get_key_code (0), 'A');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[111;5u\x1b[57421;5:2ux");
    ck_assert_int_eq (get_key_code (0), XCTRL (KEY_M_CTRL | 'o'));
    ck_assert_int_eq (get_key_code (0), KEY_M_CTRL | KEY_PPAGE);
    ck_assert_int_eq (get_key_code (0), 'x');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("abc\x1b[44:63;132u");
    ck_assert_int_eq (get_key_code (0), 'a');
    ck_assert_int_eq (get_key_code (0), 'b');
    ck_assert_int_eq (get_key_code (0), 'c');
    ck_assert_int_eq (get_key_code (0), KEY_M_ALT | '?');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("😊FG");
    ck_assert_int_eq (get_key_code (0), 0xF0);
    ck_assert_int_eq (get_key_code (0), 0x9F);
    ck_assert_int_eq (get_key_code (0), 0x98);
    ck_assert_int_eq (get_key_code (0), 0x8A);
    ck_assert_int_eq (get_key_code (0), 'F');
    ck_assert_int_eq (get_key_code (0), 'G');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[128512;2uFG");
    ck_assert_int_eq (get_key_code (0), 0xF0);  // 😀
    ck_assert_int_eq (get_key_code (0), 0x9F);
    ck_assert_int_eq (get_key_code (0), 0x98);
    ck_assert_int_eq (get_key_code (0), 0x80);
    ck_assert_int_eq (get_key_code (0), 'F');
    ck_assert_int_eq (get_key_code (0), 'G');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("ц\x1b[1;2A=5ů§a");
    ck_assert_int_eq (get_key_code (0), 0xD1);  // 'ц'
    ck_assert_int_eq (get_key_code (0), 0x86);
    ck_assert_int_eq (get_key_code (0), KEY_M_SHIFT | KEY_UP);
    ck_assert_int_eq (get_key_code (0), '=');
    ck_assert_int_eq (get_key_code (0), '5');
    ck_assert_int_eq (get_key_code (0), 0xC5);  // 'ů'
    ck_assert_int_eq (get_key_code (0), 0xAF);
    ck_assert_int_eq (get_key_code (0), 0xC2);  // '§'
    ck_assert_int_eq (get_key_code (0), 0xA7);
    ck_assert_int_eq (get_key_code (0), 'a');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[85405465064;6065606146066:::::::u$");
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), '$');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[u");
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1b[.abc");
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), 'b');
    ck_assert_int_eq (get_key_code (0), 'c');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);

    mock_input ("\x1bOP\x1bO1;2abc");
    ck_assert_int_eq (get_key_code (0), KEY_F (1));
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), 'b');
    ck_assert_int_eq (get_key_code (0), 'c');
    ck_assert_int_eq (get_key_code (0), -1);
    ck_assert_int_eq (get_key_code (0), -1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_tty_check_term_unset);
    tcase_add_test (tc_core, test_tty_check_term_non_xterm);
    tcase_add_test (tc_core, test_tty_check_term_xterm_like);
    tcase_add_test (tc_core, test_tty_get_key_code);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
