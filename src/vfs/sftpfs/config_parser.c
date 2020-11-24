/* Virtual File System: SFTP file system.
   The SSH config parser

   Copyright (C) 2011-2020
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2011
   Slava Zanko <slavazanko@gmail.com>, 2011, 2012, 2013

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

#include <config.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>             /* atoi() */

#include "lib/global.h"

#include "lib/search.h"
#include "lib/util.h"           /* tilde_expand() */
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define SFTP_DEFAULT_PORT 22

#ifndef SFTPFS_SSH_CONFIG
#define SFTPFS_SSH_CONFIG "~/.ssh/config"
#endif

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *real_host;            /* host DNS name or ip address */
    int port;                   /* port for connect to host */
    char *user;                 /* the user to log in as */
    gboolean password_auth;     /* FALSE - no passwords allowed (default TRUE) */
    gboolean identities_only;   /* TRUE - no ssh agent (default FALSE) */
    gboolean pubkey_auth;       /* FALSE - disable public key authentication (default TRUE) */
    char *identity_file;        /* A file from which the user's DSA, ECDSA or DSA authentication identity is read. */
} sftpfs_ssh_config_entity_t;

enum config_var_type
{
    STRING,
    INTEGER,
    BOOLEAN,
    FILENAME
};

/*** file scope variables ************************************************************************/

/* *INDENT-OFF* */
static struct
{
    const char *pattern;
    mc_search_t *pattern_regexp;
    enum config_var_type type;
    size_t offset;
} config_variables[] =
{
    {"^\\s*User\\s+(.*)$", NULL, STRING, offsetof (sftpfs_ssh_config_entity_t, user)},
    {"^\\s*HostName\\s+(.*)$", NULL, STRING, offsetof (sftpfs_ssh_config_entity_t, real_host)},
    {"^\\s*IdentitiesOnly\\s+(.*)$", NULL, BOOLEAN, offsetof (sftpfs_ssh_config_entity_t, identities_only)},
    {"^\\s*IdentityFile\\s+(.*)$", NULL, FILENAME, offsetof (sftpfs_ssh_config_entity_t, identity_file)},
    {"^\\s*Port\\s+(.*)$", NULL, INTEGER, offsetof (sftpfs_ssh_config_entity_t, port)},
    {"^\\s*PasswordAuthentication\\s+(.*)$", NULL, BOOLEAN, offsetof (sftpfs_ssh_config_entity_t, password_auth)},
    {"^\\s*PubkeyAuthentication\\s+(.*)$", NULL, STRING, offsetof (sftpfs_ssh_config_entity_t, pubkey_auth)},
    {NULL, NULL, 0, 0}
};
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Free one config entity.
 *
 * @param config_entity config entity structure
 */

static void
sftpfs_ssh_config_entity_free (sftpfs_ssh_config_entity_t * config_entity)
{
    g_free (config_entity->real_host);
    g_free (config_entity->user);
    g_free (config_entity->identity_file);
    g_free (config_entity);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Transform tilda (~) to full home dirname.
 *
 * @param filename file name with tilda
 * @return newly allocated file name with full home dirname
 */

static char *
sftpfs_correct_file_name (const char *filename)
{
    vfs_path_t *vpath;
    char *ret_value;

    ret_value = tilde_expand (filename);
    vpath = vfs_path_from_str (ret_value);
    g_free (ret_value);
    ret_value = g_strdup (vfs_path_as_str (vpath));
    vfs_path_free (vpath);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */

#define POINTER_TO_STRUCTURE_MEMBER(type)  \
    ((type) ((char *) config_entity + (size_t) config_variables[i].offset))

/**
 * Parse string and filling one config entity by parsed data.
 *
 * @param config_entity config entity structure
 * @param buffer        string for parce
 */

static void
sftpfs_fill_config_entity_from_string (sftpfs_ssh_config_entity_t * config_entity, char *buffer)
{
    int i;

    for (i = 0; config_variables[i].pattern != NULL; i++)
    {
        if (mc_search_run (config_variables[i].pattern_regexp, buffer, 0, strlen (buffer), NULL))
        {
            int value_offset;
            char *value;

            int *pointer_int;
            char **pointer_str;
            gboolean *pointer_bool;

            /* Calculate start of value in string */
            value_offset = mc_search_getstart_result_by_num (config_variables[i].pattern_regexp, 1);
            value = &buffer[value_offset];

            switch (config_variables[i].type)
            {
            case STRING:
                pointer_str = POINTER_TO_STRUCTURE_MEMBER (char **);
                *pointer_str = g_strdup (value);
                break;
            case FILENAME:
                pointer_str = POINTER_TO_STRUCTURE_MEMBER (char **);
                *pointer_str = sftpfs_correct_file_name (value);
                break;
            case INTEGER:
                pointer_int = POINTER_TO_STRUCTURE_MEMBER (int *);
                *pointer_int = atoi (value);
                break;
            case BOOLEAN:
                pointer_bool = POINTER_TO_STRUCTURE_MEMBER (gboolean *);
                *pointer_bool = strcasecmp (value, "True") == 0;
                break;
            default:
                continue;
            }
            return;
        }
    }
}

#undef POINTER_TO_STRUCTURE_MEMBER

/* --------------------------------------------------------------------------------------------- */
/**
 * Fill one config entity from config file.
 *
 * @param ssh_config_handler file descriptor for the ssh config file
 * @param config_entity      config entity structure
 * @param vpath_element      path element with host data (hostname, port)
 * @param mcerror            pointer to the error handler
 * @return TRUE if config entity was filled successfully, FALSE otherwise
 */

static gboolean
sftpfs_fill_config_entity_from_config (FILE * ssh_config_handler,
                                       sftpfs_ssh_config_entity_t * config_entity,
                                       const vfs_path_element_t * vpath_element, GError ** mcerror)
{
    char buffer[BUF_MEDIUM];
    gboolean host_block_hit = FALSE;
    gboolean pattern_block_hit = FALSE;
    mc_search_t *host_regexp;
    gboolean ok = TRUE;

    mc_return_val_if_error (mcerror, FALSE);

    host_regexp = mc_search_new ("^\\s*host\\s+(.*)$", DEFAULT_CHARSET);
    host_regexp->search_type = MC_SEARCH_T_REGEX;
    host_regexp->is_case_sensitive = FALSE;

    while (TRUE)
    {
        char *cr;

        if (fgets (buffer, sizeof (buffer), ssh_config_handler) == NULL)
        {
            int e;

            e = errno;

            if (!feof (ssh_config_handler))
            {
                mc_propagate_error (mcerror, e,
                                    _("sftp: an error occurred while reading %s: %s"),
                                    SFTPFS_SSH_CONFIG, strerror (e));
                ok = FALSE;
                goto done;
            }

            break;
        }

        cr = strrchr (buffer, '\n');
        if (cr != NULL)
            *cr = '\0';

        if (mc_search_run (host_regexp, buffer, 0, strlen (buffer), NULL))
        {
            const char *host_pattern;
            int host_pattern_offset;

            /* if previous host block exactly describe our connection */
            if (host_block_hit)
                goto done;

            host_pattern_offset = mc_search_getstart_result_by_num (host_regexp, 1);
            host_pattern = &buffer[host_pattern_offset];
            if (strcmp (host_pattern, vpath_element->host) == 0)
            {
                /* current host block describe our connection */
                host_block_hit = TRUE;
            }
            else
            {
                mc_search_t *pattern_regexp;

                pattern_regexp = mc_search_new (host_pattern, DEFAULT_CHARSET);
                pattern_regexp->search_type = MC_SEARCH_T_GLOB;
                pattern_regexp->is_case_sensitive = FALSE;
                pattern_regexp->is_entire_line = TRUE;
                pattern_block_hit =
                    mc_search_run (pattern_regexp, vpath_element->host, 0,
                                   strlen (vpath_element->host), NULL);
                mc_search_free (pattern_regexp);
            }
        }
        else if (pattern_block_hit || host_block_hit)
        {
            sftpfs_fill_config_entity_from_string (config_entity, buffer);
        }
    }

  done:
    mc_search_free (host_regexp);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Open the ssh config file and fill config entity.
 *
 * @param vpath_element path element with host data (hostname, port)
 * @param mcerror       pointer to the error handler
 * @return newly allocated config entity structure
 */

static sftpfs_ssh_config_entity_t *
sftpfs_get_config_entity (const vfs_path_element_t * vpath_element, GError ** mcerror)
{
    sftpfs_ssh_config_entity_t *config_entity;
    FILE *ssh_config_handler;
    char *config_filename;

    mc_return_val_if_error (mcerror, FALSE);

    config_entity = g_new0 (sftpfs_ssh_config_entity_t, 1);
    config_entity->password_auth = TRUE;
    config_entity->identities_only = FALSE;
    config_entity->pubkey_auth = TRUE;
    config_entity->port = SFTP_DEFAULT_PORT;

    config_filename = sftpfs_correct_file_name (SFTPFS_SSH_CONFIG);
    ssh_config_handler = fopen (config_filename, "r");
    g_free (config_filename);

    if (ssh_config_handler != NULL)
    {
        gboolean ok;

        ok = sftpfs_fill_config_entity_from_config
            (ssh_config_handler, config_entity, vpath_element, mcerror);
        fclose (ssh_config_handler);

        if (!ok)
        {
            sftpfs_ssh_config_entity_free (config_entity);
            return NULL;
        }
    }

    if (config_entity->user == NULL)
    {
        config_entity->user = vfs_get_local_username ();
        if (config_entity->user == NULL)
        {
            sftpfs_ssh_config_entity_free (config_entity);
            config_entity = NULL;
            mc_propagate_error (mcerror, EPERM, "%s", _("sftp: Unable to get current user name."));
        }
    }
    return config_entity;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Reads data from the ssh config file related to connection.
 *
 * @param super connection data
 * @param error pointer to the error handler
 */

void
sftpfs_fill_connection_data_from_config (struct vfs_s_super *super, GError ** mcerror)
{
    sftpfs_super_t *sftpfs_super = SFTP_SUPER (super);
    sftpfs_ssh_config_entity_t *config_entity;

    mc_return_if_error (mcerror);

    config_entity = sftpfs_get_config_entity (super->path_element, mcerror);
    if (config_entity == NULL)
        return;

    sftpfs_super->config_auth_type = (config_entity->pubkey_auth) ? PUBKEY : 0;
    sftpfs_super->config_auth_type |= (config_entity->identities_only) ? 0 : AGENT;
    sftpfs_super->config_auth_type |= (config_entity->password_auth) ? PASSWORD : 0;

    if (super->path_element->port == 0)
        super->path_element->port = config_entity->port;

    if (super->path_element->user == NULL)
        super->path_element->user = g_strdup (config_entity->user);

    if (config_entity->real_host != NULL)
    {
        g_free (super->path_element->host);
        super->path_element->host = g_strdup (config_entity->real_host);
    }

    if (config_entity->identity_file != NULL)
    {
        sftpfs_super->privkey = g_strdup (config_entity->identity_file);
        sftpfs_super->pubkey = g_strdup_printf ("%s.pub", config_entity->identity_file);
    }

    sftpfs_ssh_config_entity_free (config_entity);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Initialize the SSH config parser.
 */

void
sftpfs_init_config_variables_patterns (void)
{
    int i;

    for (i = 0; config_variables[i].pattern != NULL; i++)
    {
        config_variables[i].pattern_regexp =
            mc_search_new (config_variables[i].pattern, DEFAULT_CHARSET);
        config_variables[i].pattern_regexp->search_type = MC_SEARCH_T_REGEX;
        config_variables[i].pattern_regexp->is_case_sensitive = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Deinitialize the SSH config parser.
 */

void
sftpfs_deinit_config_variables_patterns (void)
{
    int i;

    for (i = 0; config_variables[i].pattern != NULL; i++)
    {
        mc_search_free (config_variables[i].pattern_regexp);
        config_variables[i].pattern_regexp = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
