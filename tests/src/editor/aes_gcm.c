/*
   src/editor - tests for mc_aes256_gcm_encrypt_encode/mc_aes256_gcm_decode_and_decrypt functions

   Copyright (C) 2021
   Free Software Foundation, Inc.

   Written by:
   Rafael Santiago <voidbrainvoid@tutanota.com>, 2021

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"
#include "src/editor/aes_gcm.h"
#include <ctype.h>
#include <string.h>

START_TEST (test_aes_gcm)
{
    char *ciphertext = NULL;
    size_t ciphertext_size = 0;
    unsigned char *plaintext = (unsigned char *)"'Essa gravata e o relatorio de harmonia de coisas belas\n"
                                                "E um jardim suspenso dependurado no pescoco\n"
                                                "De um homem simpatico e feliz'\n";
    size_t plaintext_size = strlen((char *)plaintext);
    const char *password = "o homem da gravata florida";
    size_t password_size = strlen(password);
    unsigned char *decrypted_data = NULL;
    size_t decrypted_data_size;
    size_t i;

    /* invalid parameters */
    ciphertext = mc_aes256_gcm_encrypt_and_encode (NULL, plaintext_size, password, password_size, &ciphertext_size);
    mctest_assert_ptr_eq (ciphertext, NULL);

    ciphertext = mc_aes256_gcm_encrypt_and_encode (plaintext, 0, password, password_size, &ciphertext_size);
    mctest_assert_ptr_eq (ciphertext, NULL);

    ciphertext = mc_aes256_gcm_encrypt_and_encode (plaintext, plaintext_size, NULL, password_size, &ciphertext_size);
    mctest_assert_ptr_eq (ciphertext, NULL);

    ciphertext = mc_aes256_gcm_encrypt_and_encode (plaintext, plaintext_size, password, 0, &ciphertext_size);
    mctest_assert_ptr_eq (ciphertext, NULL);

    ciphertext = mc_aes256_gcm_encrypt_and_encode (plaintext, plaintext_size, password, password_size, NULL);
    mctest_assert_ptr_eq (ciphertext, NULL);

    /* valid parameters but invalid password during decryption */
    ciphertext = mc_aes256_gcm_encrypt_and_encode (plaintext, plaintext_size, password, password_size, &ciphertext_size);
    mctest_assert_ptr_ne (ciphertext, NULL);

    decrypted_data = mc_aes256_gcm_decode_and_decrypt (ciphertext, ciphertext_size, "wr0ng", 5, &ciphertext_size);

    mctest_assert_ptr_eq (decrypted_data, NULL);
    g_free (ciphertext);

    ciphertext = mc_aes256_gcm_encrypt_and_encode (plaintext, plaintext_size, password, password_size, &ciphertext_size);
    mctest_assert_ptr_ne (ciphertext, NULL);

    /* invalid parameters */
    decrypted_data = mc_aes256_gcm_decode_and_decrypt (NULL, ciphertext_size, password, password_size, &decrypted_data_size);
    mctest_assert_ptr_eq (decrypted_data, NULL);

    decrypted_data = mc_aes256_gcm_decode_and_decrypt (ciphertext, 0, password, password_size, &decrypted_data_size);
    mctest_assert_ptr_eq (decrypted_data, NULL);

    decrypted_data = mc_aes256_gcm_decode_and_decrypt (ciphertext, ciphertext_size, NULL, password_size, &decrypted_data_size);
    mctest_assert_ptr_eq (decrypted_data, NULL);

    decrypted_data = mc_aes256_gcm_decode_and_decrypt (ciphertext, ciphertext_size, password, 0, &decrypted_data_size);
    mctest_assert_ptr_eq (decrypted_data, NULL);

    decrypted_data = mc_aes256_gcm_decode_and_decrypt (ciphertext, ciphertext_size, password, password_size, NULL);
    mctest_assert_ptr_eq (decrypted_data, NULL);

    /* valid parameters and valid decryption password */
    decrypted_data = mc_aes256_gcm_decode_and_decrypt (ciphertext, ciphertext_size,
                                                       password, password_size, &decrypted_data_size);
    mctest_assert_ptr_ne (decrypted_data, NULL);
    mctest_assert_int_eq (decrypted_data_size, plaintext_size);
    mctest_assert_int_eq (memcmp(decrypted_data, plaintext, plaintext_size), 0);
    mc_clear_sensitive_buffer(decrypted_data, decrypted_data_size);
    for (i = 0; i < decrypted_data_size; i++) {
        mctest_assert_int_eq(decrypted_data[i], 0);
    }
    g_free (ciphertext);
    g_free (decrypted_data);
}
END_TEST

int main (void)
{
    TCase *tc_core;
    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_aes_gcm);
    return mctest_run_all(tc_core);
}

