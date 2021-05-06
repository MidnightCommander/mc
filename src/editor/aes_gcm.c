/*
   Editor encryption API.

   Copyright (C) 2021
   Free Software Foundation, Inc.

   Written by:
   Rafael Santiago <voidbrainvoid@tutanota.com> 2021

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

/** \file aes_gcm.h
 *  \brief Header: editor encryption API
 *  \author Rafael Santiago
 *  \date 2021
 */

#include "aes_gcm.h"
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <glib.h>

#define AES_GCM_IV_SIZE 16

#define PBKDF2_ROUNDS_NR 10000

static void derive_user_password (const char *password,
                                  int password_size,
                                  const unsigned char *salt, int salt_size,
                                  unsigned char *key, int key_size);

static int get_random_block (unsigned char *block, size_t block_size);

static char *base64_encode (const unsigned char *buffer, size_t buffer_size, size_t *output_size);

static unsigned char *base64_decode (const char *buffer, size_t buffer_size, size_t *output_size);

static int aes256_gcm_encrypt (const unsigned char *plaintext, int plaintext_size,
                               const char *password, int password_size,
                               unsigned char *ciphertext,
                               int tag_size, const unsigned char *aad, int aad_size);

static int aes256_gcm_decrypt (const unsigned char *ciphertext, int ciphertext_size,
                               const char *password, int password_size,
                               unsigned char *plaintext,
                               int tag_size, const unsigned char *aad, int aad_size);


char *mc_aes256_gcm_encrypt_and_encode (const unsigned char *plaintext, size_t plaintext_size,
                                        const char *password, int password_size,
                                        size_t *output_size)
{
    unsigned char *ciphertext = NULL;
    int ciphertext_size = 0;
    char *output = NULL;

    if (plaintext == NULL || plaintext_size == 0 || password == NULL ||
        password_size == 0 || output_size == NULL) {
        return NULL;
    }

    ciphertext = (unsigned char *) g_malloc0 (plaintext_size * 2 + 1024);

    ciphertext_size = aes256_gcm_encrypt (plaintext, plaintext_size,
                                          password, password_size,
                                          ciphertext, 12, (unsigned char *)"", 0);

    if (ciphertext_size <= 0) {
        *output_size = 0;
        return NULL;
    }

    output = base64_encode (ciphertext, ciphertext_size, output_size);

    g_free (ciphertext);

    return output;
}

unsigned char *mc_aes256_gcm_decode_and_decrypt (const char *ciphertext, size_t ciphertext_size,
                                                 const char *password, int password_size,
                                                 size_t *output_size)
{
    unsigned char *raw_ciphertext = NULL;
    size_t raw_ciphertext_size = 0;
    unsigned char *output = NULL;
    int temp_size;

    if (ciphertext == NULL || ciphertext_size == 0 || password == NULL || password_size == 0 ||
        output_size == NULL) {
        return NULL;
    }

    *output_size = 0;

    raw_ciphertext = base64_decode (ciphertext, ciphertext_size, &raw_ciphertext_size);

    if (raw_ciphertext == NULL || raw_ciphertext_size == 0) {
        return NULL;
    }

    output = (unsigned char *) g_malloc0 (ciphertext_size + 2 * 1024);
    temp_size = aes256_gcm_decrypt (raw_ciphertext, raw_ciphertext_size,
			            password, password_size, output, 12, (unsigned char *)"", 0);
    if (temp_size <= 0) {
        *output_size = 0;
        g_free (output);
        output = NULL;
    } else {
        *output_size = (size_t) temp_size;
    }

    temp_size = 0;

    return output;
}

void mc_clear_sensitive_buffer (void *buf, size_t buf_size)
{
    unsigned char *bp = (unsigned char *)buf;
    unsigned char *bp_end = bp + buf_size;

    /* avoid using memset because it can be dropped during optimizations */
    while (bp != bp_end) {
        *bp = 0;
        bp++;
    }
}

static void derive_user_password (const char *password,
                                  int password_size,
                                  const unsigned char *salt, int salt_size,
                                  unsigned char *key, int key_size)
{
    PKCS5_PBKDF2_HMAC_SHA1 (password, password_size, salt, salt_size,
                            PBKDF2_ROUNDS_NR, key_size, key);
}

static int get_random_block (unsigned char *block, size_t block_size)
{
    int fd = open ("/dev/urandom", O_RDONLY);
    int done = (fd != -1);
    if (fd != -1) {
        read (fd, block, block_size);
        close (fd);
    }
    return done;
}

static char *base64_encode (const unsigned char *buffer, size_t buffer_size, size_t *output_size)
{
    BIO *bio;
    BUF_MEM *bufmem = NULL;

    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(BIO_new(BIO_f_base64()), bio);
    
    BIO_write(bio, buffer, buffer_size);
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bufmem);

    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);

    *output_size = (bufmem->data != NULL) ? strlen(bufmem->data) : 0;

    return bufmem->data;
}

static unsigned char *base64_decode (const char *buffer, size_t buffer_size, size_t *output_size)
{
    unsigned char *output;
    BIO *bio;

    output = (unsigned char *) g_malloc0(buffer_size);

    if (output == NULL) {
        *output_size = 0;
	free(output);
	return NULL;
    }

    bio = BIO_new_mem_buf(buffer, -1);
    bio = BIO_push(BIO_new(BIO_f_base64()), bio);

    *output_size = BIO_read(bio, output, buffer_size);

    BIO_free_all(bio);

    return output;
}

static int aes256_gcm_encrypt (const unsigned char *plaintext, int plaintext_size,
                               const char *password, int password_size,
                               unsigned char *ciphertext,
                               int tag_size, const unsigned char *aad, int aad_size)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int size;
    int ciphertext_size = -1;
    unsigned char iv[AES_GCM_IV_SIZE] = { 0 };
    unsigned char key[32] = { 0 };
    unsigned char *kp = NULL, *kp_end = NULL;

    if (!(ctx = EVP_CIPHER_CTX_new ())) {
        goto aes256_gcm_encrypt_epilogue;
    }

    if (EVP_EncryptInit_ex (ctx, EVP_aes_256_gcm (), NULL, NULL, NULL) != 1) {
        goto aes256_gcm_encrypt_epilogue;
    }

    if (!get_random_block (iv, AES_GCM_IV_SIZE)) {
        goto aes256_gcm_encrypt_epilogue;
    }

    if (EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_SIZE,
                             NULL) != 1) {
        goto aes256_gcm_encrypt_epilogue;
    }

    derive_user_password (password, password_size, (unsigned char *)"", 0, key, sizeof(key));

    if (EVP_EncryptInit_ex (ctx, NULL, NULL, key, iv) != 1) {
        goto aes256_gcm_encrypt_epilogue;
    }

    if (aad != NULL && aad_size > 0 &&
        EVP_EncryptUpdate (ctx, NULL, &size, aad, aad_size) != 1) {
        goto aes256_gcm_encrypt_epilogue;
    }

    if (EVP_EncryptUpdate (ctx, &ciphertext[AES_GCM_IV_SIZE + tag_size], &size,
                           plaintext, plaintext_size) != 1) {
        goto aes256_gcm_encrypt_epilogue;
    }

    ciphertext_size = size;

    memcpy (ciphertext, iv, AES_GCM_IV_SIZE);

    if (EVP_EncryptFinal_ex (ctx, ciphertext + size, &size) != 1) {
        ciphertext_size = -1;
        goto aes256_gcm_encrypt_epilogue;
    }

    ciphertext_size += size;

    if (EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_GET_TAG, tag_size,
                             &ciphertext[AES_GCM_IV_SIZE]) != 1) {
        ciphertext_size = -1;
        goto aes256_gcm_encrypt_epilogue;
    }

aes256_gcm_encrypt_epilogue:

    if (ctx != NULL) {
        EVP_CIPHER_CTX_free (ctx);
    }

    /*
     * A memset over an unreferenced variable after this memset, tends to be
     * dropped by the optimizer. Here we need to ensure that no key material
     * will leak. The following code seems clumsy but it will do the job.
     */
    kp = &key[0];
    kp_end = kp + sizeof (key);
    while (kp != kp_end) {
        *kp = 0;
        kp++;
    }

    return ciphertext_size +
                ((ciphertext_size > -1) ? (AES_GCM_IV_SIZE + tag_size) : 0);
}

static int aes256_gcm_decrypt (const unsigned char *ciphertext, int ciphertext_size,
                               const char *password, int password_size,
                               unsigned char *plaintext,
                               int tag_size, const unsigned char *aad, int aad_size)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int size;
    int plaintext_size;
    int ret = 0;
    unsigned char key[32] = { 0 };
    unsigned char *kp = NULL, *kp_end = NULL;

    if (!(ctx = EVP_CIPHER_CTX_new ())) {
        goto aes256_gcm_decrypt_epilogue;
    }

    if (EVP_DecryptInit_ex (ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 0) {
        goto aes256_gcm_decrypt_epilogue;
    }

    if (EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_SIZE,
                             NULL) == 0) {
        goto aes256_gcm_decrypt_epilogue;
    }

    derive_user_password (password, password_size, (unsigned char *)"", 0, key, sizeof(key));

    if (EVP_DecryptInit_ex (ctx, NULL, NULL, key, ciphertext) == 0) {
        goto aes256_gcm_decrypt_epilogue;
    }

    if (aad != NULL && aad_size > 0 &&
        EVP_DecryptUpdate (ctx, NULL, &size, aad, aad_size) == 0) {
        goto aes256_gcm_decrypt_epilogue;
    }

    if (EVP_DecryptUpdate (ctx,
                           plaintext,
                           &size,
                           (unsigned char *)&ciphertext[AES_GCM_IV_SIZE + tag_size],
                           ciphertext_size - AES_GCM_IV_SIZE - tag_size) == 0) {
        goto aes256_gcm_decrypt_epilogue;
    }

    plaintext_size = size;

    if (EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_SET_TAG, tag_size,
                             (unsigned char *)&ciphertext[AES_GCM_IV_SIZE]) == 0) {
        goto aes256_gcm_decrypt_epilogue;
    }

    ret = EVP_DecryptFinal_ex (ctx, plaintext + size, &size);

aes256_gcm_decrypt_epilogue:

    if (ctx != NULL) {
        EVP_CIPHER_CTX_free (ctx);
    }

    /*
     * A memset over an unreferenced variable after this memset, tends to be
     * dropped by the optimizer. Here we need to ensure that no key material
     * will leak. The following code seems clumsy but it will do the job.
     */
    kp = &key[0];
    kp_end = kp + sizeof (key);
    while (kp != kp_end) {
        *kp = 0;
        kp++;
    }

    return ((ret > 0) ? (plaintext_size + size) : -1);
}

#undef AES_GCM_IV_SIZE

#undef PBKDF2_ROUNDS_NR
