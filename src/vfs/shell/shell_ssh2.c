/*
   Virtual File System: SHELL implementation for transferring files over
   shell connections — libssh2 transport layer.

   Copyright (C) 2025
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2025

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

#include <config.h>

#ifdef ENABLE_SHELL_SSH2

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <libssh2.h>

#include "lib/global.h"

#include "lib/util.h"
#include "lib/tty/tty.h"
#include "lib/vfs/utilvfs.h"
#include "lib/mcconfig.h"
#include "lib/widget.h"

#include "lib/vfs/vfs.h"
#include "lib/vfs/xdirentry.h"

#include "shell_ssh2.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define SHELL_SSH2_DEFAULT_PORT 22
#define SHA1_DIGEST_LENGTH      20

/* LIBSSH2_INVALID_SOCKET is defined in libssh2 >= 1.4.1 */
#ifndef LIBSSH2_INVALID_SOCKET
#define LIBSSH2_INVALID_SOCKET -1
#endif

/* port flag values from shell.c */
#define SHELL_FLAG_COMPRESSED 1
#define SHELL_FLAG_RSH        2

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
static const char *const hostkey_method_ssh_ed25519 = "ssh-ed25519";
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
static const char *const hostkey_method_ssh_ecdsa_521 = "ecdsa-sha2-nistp521";
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
static const char *const hostkey_method_ssh_ecdsa_384 = "ecdsa-sha2-nistp384";
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
static const char *const hostkey_method_ssh_ecdsa_256 = "ecdsa-sha2-nistp256";
#endif
static const char *const hostkey_method_ssh_rsa = "ssh-rsa";
static const char *const hostkey_method_ssh_dss = "ssh-dss";

static const char *default_hostkey_methods =
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
    "ecdsa-sha2-nistp256,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
    "ecdsa-sha2-nistp384,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
    "ecdsa-sha2-nistp521,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
    "ecdsa-sha2-nistp256-cert-v01@openssh.com,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
    "ecdsa-sha2-nistp384-cert-v01@openssh.com,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
    "ecdsa-sha2-nistp521-cert-v01@openssh.com,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
    "ssh-ed25519,"
    "ssh-ed25519-cert-v01@openssh.com,"
#endif
    "rsa-sha2-256,"
    "rsa-sha2-512,"
    "ssh-rsa,"
    "ssh-rsa-cert-v01@openssh.com,"
    "ssh-dss";

static const char *kbi_passwd = NULL;
static const struct vfs_s_super *kbi_super = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
shell_ssh2_open_socket (struct vfs_s_super *super, GError **mcerror)
{
    struct addrinfo hints, *res = NULL, *curr_res;
    int my_socket = 0;
    char port[BUF_TINY];
    int e;
    int ssh_port;

    mc_return_val_if_error (mcerror, LIBSSH2_INVALID_SOCKET);

    if (super->path_element->host == NULL || *super->path_element->host == '\0')
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: Invalid host name."));
        return LIBSSH2_INVALID_SOCKET;
    }

    ssh_port = super->path_element->port;
    if (ssh_port <= SHELL_FLAG_RSH)
        ssh_port = SHELL_SSH2_DEFAULT_PORT;

    sprintf (port, "%d", ssh_port);

    tty_enable_interrupt_key ();

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    e = getaddrinfo (super->path_element->host, port, &hints, &res);

#ifdef AI_ADDRCONFIG
    if (e == EAI_BADFLAGS)
    {
        hints.ai_flags = 0;
        e = getaddrinfo (super->path_element->host, port, &hints, &res);
    }
#endif

    if (e != 0)
    {
        mc_propagate_error (mcerror, e, _ ("shell: %s"), gai_strerror (e));
        my_socket = LIBSSH2_INVALID_SOCKET;
        goto ret;
    }

    for (curr_res = res; curr_res != NULL; curr_res = curr_res->ai_next)
    {
        int save_errno;

        my_socket = socket (curr_res->ai_family, curr_res->ai_socktype, curr_res->ai_protocol);

        if (my_socket < 0)
        {
            if (curr_res->ai_next != NULL)
                continue;

            vfs_print_message (_ ("shell: %s"), unix_error_string (errno));
            my_socket = LIBSSH2_INVALID_SOCKET;
            goto ret;
        }

        vfs_print_message (_ ("shell: making connection to %s"), super->path_element->host);

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

        save_errno = errno;

        close (my_socket);

        if (save_errno == EINTR && tty_got_interrupt ())
            mc_propagate_error (mcerror, 0, "%s", _ ("shell: connection interrupted by user"));
        else if (curr_res->ai_next == NULL)
            mc_propagate_error (mcerror, save_errno, _ ("shell: connection to server failed: %s"),
                                unix_error_string (save_errno));
        else
            continue;

        my_socket = LIBSSH2_INVALID_SOCKET;
        break;
    }

ret:
    if (res != NULL)
        freeaddrinfo (res);
    tty_disable_interrupt_key ();
    return my_socket;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_read_known_hosts (shell_ssh2_t *ssh2, struct vfs_s_super *super, GError **mcerror)
{
    struct libssh2_knownhost *store = NULL;
    int rc;
    gboolean found = FALSE;

    ssh2->known_hosts = libssh2_knownhost_init (ssh2->session);
    if (ssh2->known_hosts == NULL)
        goto err;

    ssh2->known_hosts_file =
        mc_build_filename (mc_config_get_home_dir (), ".ssh", "known_hosts", (char *) NULL);

    if (!exist_file (ssh2->known_hosts_file))
    {
        mc_propagate_error (mcerror, 0, _ ("shell: cannot open %s:\n%s"), ssh2->known_hosts_file,
                            unix_error_string (errno));
        return FALSE;
    }

    rc = libssh2_knownhost_readfile (ssh2->known_hosts, ssh2->known_hosts_file,
                                     LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if (rc > 0)
    {
        const char *kh_name_end = NULL;

        while (!found && libssh2_knownhost_get (ssh2->known_hosts, &store, store) == 0)
        {
            if (store == NULL)
                continue;

            if (store->name == NULL)
                continue;

            if (store->name[0] != '[')
                found = strcmp (store->name, super->path_element->host) == 0;
            else
            {
                int kh_port;

                kh_name_end = strstr (store->name, "]:");
                if (kh_name_end == NULL)
                    continue;

                kh_port = (int) g_ascii_strtoll (kh_name_end + 2, NULL, 10);
                if (kh_port == super->path_element->port)
                {
                    size_t kh_name_size;

                    kh_name_size = strlen (store->name) - 1 - strlen (kh_name_end);
                    found = strncmp (store->name + 1, super->path_element->host, kh_name_size) == 0;
                }
            }
        }
    }

    if (found)
    {
        int mask;
        const char *hostkey_method = NULL;
        char *hostkey_methods;

        mask = store->typemask & LIBSSH2_KNOWNHOST_KEY_MASK;

        switch (mask)
        {
#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
        case LIBSSH2_KNOWNHOST_KEY_ED25519:
            hostkey_method = hostkey_method_ssh_ed25519;
            break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
        case LIBSSH2_KNOWNHOST_KEY_ECDSA_521:
            hostkey_method = hostkey_method_ssh_ecdsa_521;
            break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
        case LIBSSH2_KNOWNHOST_KEY_ECDSA_384:
            hostkey_method = hostkey_method_ssh_ecdsa_384;
            break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
        case LIBSSH2_KNOWNHOST_KEY_ECDSA_256:
            hostkey_method = hostkey_method_ssh_ecdsa_256;
            break;
#endif
        case LIBSSH2_KNOWNHOST_KEY_SSHRSA:
            hostkey_method = hostkey_method_ssh_rsa;
            break;
        case LIBSSH2_KNOWNHOST_KEY_SSHDSS:
            hostkey_method = hostkey_method_ssh_dss;
            break;
        case LIBSSH2_KNOWNHOST_KEY_RSA1:
            mc_propagate_error (mcerror, 0, "%s",
                                _ ("shell: found host key of unsupported type: RSA1"));
            return FALSE;
        default:
            mc_propagate_error (mcerror, 0, "%s 0x%x", _ ("shell: unknown host key type:"),
                                (unsigned int) mask);
            return FALSE;
        }

        hostkey_methods = g_strdup_printf ("%s,%s", hostkey_method, default_hostkey_methods);
        rc = libssh2_session_method_pref (ssh2->session, LIBSSH2_METHOD_HOSTKEY, hostkey_methods);
        g_free (hostkey_methods);
        if (rc < 0)
            goto err;
    }

    return TRUE;

err:
{
    char *err = NULL;
    int err_len;

    libssh2_session_last_error (ssh2->session, &err, &err_len, 1);
    mc_propagate_error (mcerror, 0, "%s", err);
    g_free (err);
}
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
shell_ssh2_compute_fingerprint_hash (LIBSSH2_SESSION *session)
{
    static char result[SHA1_DIGEST_LENGTH * 3 + 1];
    const char *fingerprint;
    size_t i;

    fingerprint = libssh2_hostkey_hash (session, LIBSSH2_HOSTKEY_HASH_SHA1);
    if (fingerprint == NULL)
        return NULL;

    for (i = 0; i < SHA1_DIGEST_LENGTH && i * 3 < sizeof (result) - 1; i++)
        g_snprintf ((gchar *) (result + i * 3), 4, "%02x:", (guint8) fingerprint[i]);

    result[i * 3 - 1] = '\0';

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_ssh2_update_known_hosts (shell_ssh2_t *ssh2, struct vfs_s_super *super,
                               const char *remote_key, size_t remote_key_len, int type_mask)
{
    int rc;

    rc = libssh2_knownhost_addc (ssh2->known_hosts, super->path_element->host, NULL, remote_key,
                                 remote_key_len, NULL, 0, type_mask, NULL);
    if (rc < 0)
        return rc;

    rc = libssh2_knownhost_writefile (ssh2->known_hosts, ssh2->known_hosts_file,
                                      LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    if (rc < 0)
        return rc;

    (void) message (D_NORMAL, _ ("Information"),
                    _ ("Permanently added\n%s\nto the list of known hosts."),
                    super->path_element->host);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_process_known_host (shell_ssh2_t *ssh2, struct vfs_s_super *super, GError **mcerror)
{
    const char *remote_key;
    const char *key_type;
    const char *fingerprint_hash;
    size_t remote_key_len = 0;
    int remote_key_type = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
    int keybit = 0;
    struct libssh2_knownhost *host = NULL;
    int rc;
    char *msg = NULL;
    gboolean handle_query = FALSE;

    remote_key = libssh2_session_hostkey (ssh2->session, &remote_key_len, &remote_key_type);
    if (remote_key == NULL || remote_key_len == 0
        || remote_key_type == LIBSSH2_HOSTKEY_TYPE_UNKNOWN)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: cannot get the remote host key"));
        return FALSE;
    }

    switch (remote_key_type)
    {
    case LIBSSH2_HOSTKEY_TYPE_RSA:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
        key_type = "RSA";
        break;
    case LIBSSH2_HOSTKEY_TYPE_DSS:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
        key_type = "DSS";
        break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
        keybit = LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
        key_type = "ECDSA";
        break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_384
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
        keybit = LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
        key_type = "ECDSA";
        break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_521
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
        keybit = LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
        key_type = "ECDSA";
        break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
    case LIBSSH2_HOSTKEY_TYPE_ED25519:
        keybit = LIBSSH2_KNOWNHOST_KEY_ED25519;
        key_type = "ED25519";
        break;
#endif
    default:
        mc_propagate_error (mcerror, 0, "%s",
                            _ ("shell: unsupported key type, can't check remote host key"));
        return FALSE;
    }

    fingerprint_hash = shell_ssh2_compute_fingerprint_hash (ssh2->session);
    if (fingerprint_hash == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: can't compute host key fingerprint hash"));
        return FALSE;
    }

    rc = libssh2_knownhost_checkp (
        ssh2->known_hosts, super->path_element->host, super->path_element->port, remote_key,
        remote_key_len, LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | keybit,
        &host);

    switch (rc)
    {
    default:
    case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
        goto err;

    case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        break;

    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
        msg = g_strdup_printf (
            _ ("The authenticity of host\n%s\ncan't be established!\n"
               "%s key fingerprint hash is\nSHA1:%s.\n"
               "Do you want to add it to the list of known hosts and continue connecting?"),
            super->path_element->host, key_type, fingerprint_hash);
        query_set_sel (2);
        rc = query_dialog (_ ("Warning"), msg, D_NORMAL, 3, _ ("&Yes"), _ ("&Ignore"), _ ("&No"));
        g_free (msg);
        handle_query = TRUE;
        break;

    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        msg = g_strdup_printf (_ ("%s\nis found in the list of known hosts but\n"
                                  "KEYS DO NOT MATCH! THIS COULD BE A MITM ATTACK!\n"
                                  "Are you sure you want to add it to the list of known hosts and "
                                  "continue connecting?"),
                               super->path_element->host);
        query_set_sel (2);
        rc = query_dialog (MSG_ERROR, msg, D_ERROR, 3, _ ("&Yes"), _ ("&Ignore"), _ ("&No"));
        g_free (msg);
        handle_query = TRUE;
        break;
    }

    if (handle_query)
        switch (rc)
        {
        case 0:
            if (shell_ssh2_update_known_hosts (ssh2, super, remote_key, remote_key_len,
                                               LIBSSH2_KNOWNHOST_TYPE_PLAIN
                                                   | LIBSSH2_KNOWNHOST_KEYENC_RAW | keybit)
                < 0)
                goto err;
            break;
        case 1:
            break;
        case 2:
        default:
            mc_propagate_error (mcerror, 0, "%s", _ ("shell: host key verification failed"));
            goto err;
        }

    return TRUE;

err:
{
    char *err = NULL;
    int err_len;

    libssh2_session_last_error (ssh2->session, &err, &err_len, 1);
    if (err != NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", err);
        g_free (err);
    }
}

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_auth_agent (shell_ssh2_t *ssh2, struct vfs_s_super *super)
{
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;
    int rc;

    ssh2->agent = libssh2_agent_init (ssh2->session);
    if (ssh2->agent == NULL)
        return FALSE;

    if (libssh2_agent_connect (ssh2->agent) != 0)
        return FALSE;

    if (libssh2_agent_list_identities (ssh2->agent) != 0)
        return FALSE;

    while (TRUE)
    {
        rc = libssh2_agent_get_identity (ssh2->agent, &identity, prev_identity);
        if (rc == 1)
            break;

        if (rc < 0)
            return FALSE;

        if (libssh2_agent_userauth (ssh2->agent, super->path_element->user, identity) == 0)
            break;

        prev_identity = identity;
    }

    return (rc == 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_auth_pubkey (shell_ssh2_t *ssh2, struct vfs_s_super *super)
{
    static const char *const key_names[] = { "id_ed25519", "id_ecdsa", "id_rsa", "id_dsa", NULL };
    int i;

    for (i = 0; key_names[i] != NULL; i++)
    {
        char *privkey;
        char *pubkey;

        privkey =
            mc_build_filename (mc_config_get_home_dir (), ".ssh", key_names[i], (char *) NULL);
        pubkey = g_strdup_printf ("%s.pub", privkey);

        if (exist_file (privkey))
        {
            int rc;

            rc = libssh2_userauth_publickey_fromfile (ssh2->session, super->path_element->user,
                                                      exist_file (pubkey) ? pubkey : NULL, privkey,
                                                      super->path_element->password);
            if (rc == 0)
            {
                g_free (pubkey);
                g_free (privkey);
                return TRUE;
            }

            /* If key needs passphrase and we don't have one, try prompting */
            if (rc == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED || rc == LIBSSH2_ERROR_FILE)
            {
                char *p, *passwd;

                p = g_strdup_printf (_ ("shell: Enter passphrase for %s "), key_names[i]);
                passwd = vfs_get_password (p);
                g_free (p);

                if (passwd != NULL)
                {
                    rc = libssh2_userauth_publickey_fromfile (
                        ssh2->session, super->path_element->user,
                        exist_file (pubkey) ? pubkey : NULL, privkey, passwd);
                    g_free (passwd);

                    if (rc == 0)
                    {
                        g_free (pubkey);
                        g_free (privkey);
                        return TRUE;
                    }
                }
            }
        }

        g_free (pubkey);
        g_free (privkey);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC (shell_ssh2_keyboard_interactive_helper)
{
    int i;
    size_t len;

    (void) instruction;
    (void) instruction_len;
    (void) abstract;

    if (kbi_super == NULL || kbi_passwd == NULL)
        return;

    if (strncmp (name, kbi_super->path_element->user, name_len) != 0)
        return;

    len = strlen (kbi_passwd);

    for (i = 0; i < num_prompts; ++i)
        if (memcmp (prompts[i].text, "Password: ", prompts[i].length) == 0)
        {
            responses[i].text = strdup (kbi_passwd);
            responses[i].length = len;
        }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_auth_password (shell_ssh2_t *ssh2, struct vfs_s_super *super)
{
    int rc;

    if (super->path_element->password != NULL)
    {
        while ((rc = libssh2_userauth_password (ssh2->session, super->path_element->user,
                                                super->path_element->password))
               == LIBSSH2_ERROR_EAGAIN)
            ;
        if (rc == 0)
            return TRUE;

        kbi_super = super;
        kbi_passwd = super->path_element->password;

        while (
            (rc = libssh2_userauth_keyboard_interactive (ssh2->session, super->path_element->user,
                                                         shell_ssh2_keyboard_interactive_helper))
            == LIBSSH2_ERROR_EAGAIN)
            ;

        kbi_super = NULL;
        kbi_passwd = NULL;

        if (rc == 0)
            return TRUE;
    }

    {
        char *p, *passwd;

        p = g_strdup_printf (_ ("shell: Enter password for %s "), super->path_element->user);
        passwd = vfs_get_password (p);
        g_free (p);

        if (passwd == NULL)
            return FALSE;

        while ((rc = libssh2_userauth_password (ssh2->session, super->path_element->user, passwd))
               == LIBSSH2_ERROR_EAGAIN)
            ;

        if (rc != 0)
        {
            kbi_super = super;
            kbi_passwd = passwd;

            while ((rc = libssh2_userauth_keyboard_interactive (
                        ssh2->session, super->path_element->user,
                        shell_ssh2_keyboard_interactive_helper))
                   == LIBSSH2_ERROR_EAGAIN)
                ;

            kbi_super = NULL;
            kbi_passwd = NULL;
        }

        if (rc == 0)
        {
            g_free (super->path_element->password);
            super->path_element->password = passwd;
            return TRUE;
        }

        g_free (passwd);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

shell_ssh2_t *
shell_ssh2_open (struct vfs_s_super *super, GError **mcerror)
{
    shell_ssh2_t *ssh2;
    int rc;

    mc_return_val_if_error (mcerror, NULL);

    /* rsh mode — libssh2 is not applicable */
    if (super->path_element->port == SHELL_FLAG_RSH)
        return NULL;

    if (super->path_element->user == NULL)
        super->path_element->user = vfs_get_local_username ();

    ssh2 = g_new0 (shell_ssh2_t, 1);
    ssh2->socket_fd = LIBSSH2_INVALID_SOCKET;

    /* 1. TCP socket */
    ssh2->socket_fd = shell_ssh2_open_socket (super, mcerror);
    if (ssh2->socket_fd == LIBSSH2_INVALID_SOCKET)
        goto err;

    /* 2. libssh2 session */
    ssh2->session = libssh2_session_init ();
    if (ssh2->session == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: failed to init SSH session"));
        goto err;
    }

    /* 2a. Enable SSH compression if requested */
    if (super->path_element->port == SHELL_FLAG_COMPRESSED)
        libssh2_session_flag (ssh2->session, LIBSSH2_FLAG_COMPRESS, 1);

    /* 3. Read known hosts, set hostkey preference */
    if (!shell_ssh2_read_known_hosts (ssh2, super, mcerror))
        goto err;

    /* 4. SSH handshake */
    while ((rc = libssh2_session_handshake (ssh2->session, (libssh2_socket_t) ssh2->socket_fd))
           == LIBSSH2_ERROR_EAGAIN)
        ;
    if (rc != 0)
    {
        mc_propagate_error (mcerror, rc, "%s", _ ("shell: failure establishing SSH session"));
        goto err;
    }

    /* 5. Verify server hostkey */
    if (!shell_ssh2_process_known_host (ssh2, super, mcerror))
        goto err;

    /* 6. Auth cascade: agent -> pubkey -> password */
    if (!shell_ssh2_auth_agent (ssh2, super) && !shell_ssh2_auth_pubkey (ssh2, super)
        && !shell_ssh2_auth_password (ssh2, super))
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: authentication failed"));
        goto err;
    }

    /* 7. Set blocking mode */
    libssh2_session_set_blocking (ssh2->session, 1);

    /* 8. Open channel and exec shell */
    ssh2->channel = libssh2_channel_open_session (ssh2->session);
    if (ssh2->channel == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: failed to open SSH channel"));
        goto err;
    }

    rc = libssh2_channel_exec (ssh2->channel, "echo SHELL:; /bin/sh");
    if (rc != 0)
    {
        mc_propagate_error (mcerror, rc, "%s", _ ("shell: failed to exec remote shell"));
        goto err;
    }

    return ssh2;

err:
    /* Clear the error — caller will handle fallback */
    if (mcerror != NULL && *mcerror != NULL)
    {
        g_error_free (*mcerror);
        *mcerror = NULL;
    }

    shell_ssh2_close (ssh2);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
shell_ssh2_close (shell_ssh2_t *ssh2)
{
    if (ssh2 == NULL)
        return;

    if (ssh2->channel != NULL)
    {
        libssh2_channel_close (ssh2->channel);
        libssh2_channel_free (ssh2->channel);
        ssh2->channel = NULL;
    }

    if (ssh2->agent != NULL)
    {
        libssh2_agent_disconnect (ssh2->agent);
        libssh2_agent_free (ssh2->agent);
        ssh2->agent = NULL;
    }

    if (ssh2->known_hosts != NULL)
    {
        libssh2_knownhost_free (ssh2->known_hosts);
        ssh2->known_hosts = NULL;
    }

    MC_PTR_FREE (ssh2->known_hosts_file);

    if (ssh2->session != NULL)
    {
        libssh2_session_disconnect (ssh2->session, "shell: closing connection");
        libssh2_session_free (ssh2->session);
        ssh2->session = NULL;
    }

    if (ssh2->socket_fd != LIBSSH2_INVALID_SOCKET)
    {
        close (ssh2->socket_fd);
        ssh2->socket_fd = LIBSSH2_INVALID_SOCKET;
    }

    g_free (ssh2);
}

/* --------------------------------------------------------------------------------------------- */

ssize_t
shell_ssh2_read (shell_ssh2_t *ssh2, void *buf, size_t len)
{
    ssize_t n;

    while (TRUE)
    {
        n = libssh2_channel_read (ssh2->channel, buf, len);
        if (n != LIBSSH2_ERROR_EAGAIN)
            break;
    }

    if (n < 0)
    {
        errno = EIO;
        return -1;
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */

ssize_t
shell_ssh2_write (shell_ssh2_t *ssh2, const void *buf, size_t len)
{
    const char *p = (const char *) buf;
    size_t remaining = len;

    while (remaining > 0)
    {
        ssize_t n;

        n = libssh2_channel_write (ssh2->channel, p, remaining);
        if (n == LIBSSH2_ERROR_EAGAIN)
            continue;
        if (n < 0)
        {
            errno = EIO;
            return -1;
        }
        p += n;
        remaining -= (size_t) n;
    }

    return (ssize_t) len;
}

/* --------------------------------------------------------------------------------------------- */

#endif /* ENABLE_SHELL_SSH2 */
