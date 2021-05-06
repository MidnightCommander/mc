/** \file aes_gcm.h
 *  \brief Header: editor encryption API
 *  \author Rafael Santiago
 *  \date 2021
 */
#ifndef MC__EDIT_AES_GCM_H
#define MC__EDIT_AES_GCM_H

#include <stdlib.h>

char *mc_aes256_gcm_encrypt_and_encode (const unsigned char *plaintext, size_t plaintext_size,
                                        const char *password, int password_size,
                                        size_t *output_size);

unsigned char *mc_aes256_gcm_decode_and_decrypt (const char *ciphertext, size_t ciphertext_size,
                                                 const char *password, int password_size,
                                                 size_t *output_size);

void mc_clear_sensitive_buffer (void *buf, size_t buf_size);

#endif /* MC__EDIT_AES_GCM_H */
