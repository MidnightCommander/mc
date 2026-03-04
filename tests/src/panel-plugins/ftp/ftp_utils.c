/*
   src/panel-plugins/ftp - tests for FTP utility functions

   Copyright (C) 2026
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

#define TEST_SUITE_NAME "/src/panel-plugins/ftp"

#include "tests/mctest.h"

#include <string.h>

/* --------------------------------------------------------------------------------------------- */
/* Pull in only the pure utility functions from ftp.c by redefining types and constants needed.  */
/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    char *label;
    char *host;
    int port;
    char *user;
    char *password;
    char *path;
    gboolean passive_mode;
    char *encoding;
    int timeout;
    int connect_timeout;
    gboolean keepalive;
    int keepalive_interval;
    int ip_version;
    int ssl_mode;
    char *post_connect_commands;
    char *proxy_host;
    int proxy_port;
    char *proxy_user;
    char *proxy_password;
    int proxy_type;
} ftp_connection_t;

#define FTP_DEFAULT_PORT 21

/* ---- ftp_normalize_host ---- */

static char *
ftp_normalize_host (const char *host_in)
{
    const char *host;
    gsize len;

    if (host_in == NULL)
        return NULL;

    host = host_in;
    while (g_ascii_isspace (*host))
        host++;

    if (g_ascii_strncasecmp (host, "ftp://", 6) == 0)
        host += 6;
    else if (g_ascii_strncasecmp (host, "ftps://", 7) == 0)
        host += 7;

    while (*host == '/')
        host++;

    len = strlen (host);
    while (len > 0 && (host[len - 1] == '/' || g_ascii_isspace (host[len - 1])))
        len--;

    return g_strndup (host, len);
}

/* ---- ftp_password_encode / ftp_password_decode ---- */

static const unsigned char ftp_obfuscation_key[] = "Mc4FtpPanelKey!";

static char *
ftp_password_encode (const char *plain)
{
    size_t i, len, klen;
    guchar *xored;
    gchar *b64;
    char *result;

    if (plain == NULL || plain[0] == '\0')
        return NULL;

    len = strlen (plain);
    klen = sizeof (ftp_obfuscation_key) - 1;
    xored = g_new (guchar, len);

    for (i = 0; i < len; i++)
        xored[i] = (guchar) plain[i] ^ ftp_obfuscation_key[i % klen];

    b64 = g_base64_encode (xored, len);
    g_free (xored);

    result = g_strdup_printf ("enc:%s", b64);
    g_free (b64);

    return result;
}

static char *
ftp_password_decode (const char *encoded)
{
    guchar *xored;
    gsize len;
    size_t i, klen;
    char *plain;

    if (encoded == NULL)
        return NULL;

    if (strncmp (encoded, "enc:", 4) != 0)
        return g_strdup (encoded);

    xored = g_base64_decode (encoded + 4, &len);
    if (xored == NULL || len == 0)
    {
        g_free (xored);
        return g_strdup ("");
    }

    klen = sizeof (ftp_obfuscation_key) - 1;
    plain = g_new (char, len + 1);

    for (i = 0; i < len; i++)
        plain[i] = (char) (xored[i] ^ ftp_obfuscation_key[i % klen]);
    plain[len] = '\0';

    g_free (xored);
    return plain;
}

/* ---- ftp_build_url ---- */

static char *
ftp_build_url (const ftp_connection_t *conn, const char *path)
{
    const char *scheme;

    scheme = (conn->ssl_mode >= 2) ? "ftps" : "ftp";

    if (path != NULL && path[0] != '\0')
        return g_strdup_printf ("%s://%s:%d%s", scheme, conn->host, conn->port, path);

    return g_strdup_printf ("%s://%s:%d/", scheme, conn->host, conn->port);
}

/* ---- ftp_connection_dup ---- */

static ftp_connection_t *
ftp_connection_dup (const ftp_connection_t *src)
{
    ftp_connection_t *d;

    d = g_new0 (ftp_connection_t, 1);
    d->label = g_strdup (src->label);
    d->host = g_strdup (src->host);
    d->port = src->port;
    d->user = g_strdup (src->user);
    d->password = g_strdup (src->password);
    d->path = g_strdup (src->path);
    d->passive_mode = src->passive_mode;
    d->encoding = g_strdup (src->encoding);
    d->timeout = src->timeout;
    d->connect_timeout = src->connect_timeout;
    d->keepalive = src->keepalive;
    d->keepalive_interval = src->keepalive_interval;
    d->ip_version = src->ip_version;
    d->ssl_mode = src->ssl_mode;
    d->post_connect_commands = g_strdup (src->post_connect_commands);
    d->proxy_host = g_strdup (src->proxy_host);
    d->proxy_port = src->proxy_port;
    d->proxy_user = g_strdup (src->proxy_user);
    d->proxy_password = g_strdup (src->proxy_password);
    d->proxy_type = src->proxy_type;
    return d;
}

static void
ftp_connection_free (ftp_connection_t *conn)
{
    if (conn == NULL)
        return;

    g_free (conn->label);
    g_free (conn->host);
    g_free (conn->user);
    g_free (conn->password);
    g_free (conn->path);
    g_free (conn->encoding);
    g_free (conn->post_connect_commands);
    g_free (conn->proxy_host);
    g_free (conn->proxy_user);
    g_free (conn->proxy_password);
    g_free (conn);
}

/* ======================================================================================= */
/*                                     TEST DATA                                           */
/* ======================================================================================= */

/* ---- normalize_host ---- */

/* @DataSource("test_normalize_host_ds") */
static const struct test_normalize_host_ds
{
    const char *input;
    const char *expected;
} test_normalize_host_ds[] = {
    { "example.com", "example.com" },                   // 0: plain host
    { "  example.com  ", "example.com" },               // 1: trim whitespace
    { "ftp://example.com", "example.com" },             // 2: strip ftp://
    { "ftps://example.com", "example.com" },            // 3: strip ftps://
    { "FTP://example.com", "example.com" },             // 4: case insensitive
    { "FTPS://example.com", "example.com" },            // 5: case insensitive ftps
    { "ftp://example.com/", "example.com" },            // 6: trailing slash
    { "ftp://example.com///", "example.com" },          // 7: multiple trailing slashes
    { "  ftp://example.com/  ", "example.com" },        // 8: whitespace + scheme + slash
    { "ftp:///example.com", "example.com" },            // 9: extra slashes after scheme
    { NULL, NULL },                                     // 10: NULL input
    { "", "" },                                         // 11: empty string
    { "192.168.1.1", "192.168.1.1" },                   // 12: IP address
    { "ftp://192.168.1.1/path/", "192.168.1.1/path" },  // 13: IP with path trailing slash
};

/* ---- password encode/decode roundtrip ---- */

/* @DataSource("test_password_roundtrip_ds") */
static const struct test_password_roundtrip_ds
{
    const char *password;
} test_password_roundtrip_ds[] = {
    { "secret" },                   // 0: simple password
    { "p@$$w0rd!#" },               // 1: special characters
    { "a" },                        // 2: single char
    { "very long password 1234" },  // 3: long password
    { "пароль" },                   // 4: UTF-8 password
};

/* ---- password decode backward compat ---- */

/* @DataSource("test_password_decode_plain_ds") */
static const struct test_password_decode_plain_ds
{
    const char *input;
    const char *expected;
} test_password_decode_plain_ds[] = {
    { "plaintext", "plaintext" },  // 0: no enc: prefix → return as-is
    { "", "" },                    // 1: empty string
    { NULL, NULL },                // 2: NULL
};

/* ---- build_url ---- */

/* @DataSource("test_build_url_ds") */
static const struct test_build_url_ds
{
    int ssl_mode;
    const char *host;
    int port;
    const char *path;
    const char *expected;
} test_build_url_ds[] = {
    { 0, "example.com", 21, "/pub", "ftp://example.com:21/pub" },       // 0: ftp basic
    { 1, "example.com", 21, "/pub", "ftp://example.com:21/pub" },       // 1: ssl_mode=1 → ftp
    { 2, "example.com", 990, "/", "ftps://example.com:990/" },          // 2: ssl_mode=2 → ftps
    { 3, "example.com", 990, "/data", "ftps://example.com:990/data" },  // 3: ssl_mode=3 → ftps
    { 0, "example.com", 21, NULL, "ftp://example.com:21/" },            // 4: NULL path → /
    { 0, "example.com", 21, "", "ftp://example.com:21/" },              // 5: empty path → /
};

/* ---- connection_dup ---- */

/* @DataSource("test_connection_dup_ds") */
static const struct test_connection_dup_ds
{
    const char *label;
    const char *host;
    int port;
    const char *user;
    int ssl_mode;
    int proxy_type;
} test_connection_dup_ds[] = {
    { "test", "example.com", 21, "user1", 0, 0 },  // 0: basic
    { NULL, "host", 990, NULL, 3, 2 },             // 1: NULL strings, FTPS + SOCKS4
    { "prod", "10.0.0.1", 2121, "admin", 2, 1 },   // 2: custom port + proxy
};

/* ======================================================================================= */
/*                                       TESTS                                             */
/* ======================================================================================= */

/* @Test(dataSource = "test_normalize_host_ds") */
START_PARAMETRIZED_TEST (test_normalize_host, test_normalize_host_ds)
{
    char *result;

    result = ftp_normalize_host (data->input);

    if (data->expected == NULL)
    {
        mctest_assert_null (result);
    }
    else
    {
        mctest_assert_str_eq (result, data->expected);
    }

    g_free (result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_password_roundtrip_ds") */
START_PARAMETRIZED_TEST (test_password_roundtrip, test_password_roundtrip_ds)
{
    char *encoded;
    char *decoded;

    encoded = ftp_password_encode (data->password);

    /* encoded must start with "enc:" */
    ck_assert_msg (strncmp (encoded, "enc:", 4) == 0, "encoded should start with 'enc:'");

    /* encoded must differ from plain */
    ck_assert_msg (strcmp (encoded + 4, data->password) != 0,
                   "encoded payload should differ from plain text");

    decoded = ftp_password_decode (encoded);
    mctest_assert_str_eq (decoded, data->password);

    g_free (encoded);
    g_free (decoded);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_password_decode_plain_ds") */
START_PARAMETRIZED_TEST (test_password_decode_plain, test_password_decode_plain_ds)
{
    char *result;

    result = ftp_password_decode (data->input);

    if (data->expected == NULL)
    {
        mctest_assert_null (result);
    }
    else
    {
        mctest_assert_str_eq (result, data->expected);
    }

    g_free (result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_password_encode_null)
{
    char *result;

    result = ftp_password_encode (NULL);
    mctest_assert_null (result);

    result = ftp_password_encode ("");
    mctest_assert_null (result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_build_url_ds") */
START_PARAMETRIZED_TEST (test_build_url, test_build_url_ds)
{
    ftp_connection_t conn = { 0 };
    char *result;

    conn.host = (char *) data->host;
    conn.port = data->port;
    conn.ssl_mode = data->ssl_mode;

    result = ftp_build_url (&conn, data->path);
    mctest_assert_str_eq (result, data->expected);

    g_free (result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_connection_dup_ds") */
START_PARAMETRIZED_TEST (test_connection_dup, test_connection_dup_ds)
{
    ftp_connection_t src = { 0 };
    ftp_connection_t *dup;

    src.label = (char *) data->label;
    src.host = (char *) data->host;
    src.port = data->port;
    src.user = (char *) data->user;
    src.ssl_mode = data->ssl_mode;
    src.proxy_type = data->proxy_type;

    dup = ftp_connection_dup (&src);

    /* verify values */
    if (data->label != NULL)
    {
        mctest_assert_str_eq (dup->label, data->label);
    }
    else
    {
        mctest_assert_null (dup->label);
    }

    mctest_assert_str_eq (dup->host, data->host);
    ck_assert_int_eq (dup->port, data->port);
    ck_assert_int_eq (dup->ssl_mode, data->ssl_mode);
    ck_assert_int_eq (dup->proxy_type, data->proxy_type);

    /* verify deep copy — different pointers */
    if (data->host != NULL)
    {
        mctest_assert_ptr_ne (dup->host, src.host);
    }

    /* clean up — don't free src strings, they are const */
    dup->label = NULL;
    dup->host = NULL;
    dup->user = NULL;
    dup->password = NULL;
    dup->path = NULL;
    dup->encoding = NULL;
    dup->post_connect_commands = NULL;
    dup->proxy_host = NULL;
    dup->proxy_user = NULL;
    dup->proxy_password = NULL;

    /* ftp_connection_free expects to g_free all strings, set NULLs for non-duped ones */
    ftp_connection_free (dup);
}
END_PARAMETRIZED_TEST

/* ======================================================================================= */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* normalize_host */
    mctest_add_parameterized_test (tc_core, test_normalize_host, test_normalize_host_ds);

    /* password encode/decode */
    mctest_add_parameterized_test (tc_core, test_password_roundtrip, test_password_roundtrip_ds);
    mctest_add_parameterized_test (tc_core, test_password_decode_plain,
                                   test_password_decode_plain_ds);
    tcase_add_test (tc_core, test_password_encode_null);

    /* build_url */
    mctest_add_parameterized_test (tc_core, test_build_url, test_build_url_ds);

    /* connection_dup */
    mctest_add_parameterized_test (tc_core, test_connection_dup, test_connection_dup_ds);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
