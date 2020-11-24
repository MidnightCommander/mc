/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   name query routines

   Copyright (C) Andrew Tridgell 1994-1998

   Copyright (C) 2011-2020
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

const char *unix_error_string (int error_num);
extern pstring scope;
extern int DEBUGLEVEL;

/* nmbd.c sets this to True. */
const BOOL global_in_nmbd = False;

/****************************************************************************
  do a netbios name query to find someones IP
  returns an array of IP addresses or NULL if none
  *count will be set to the number of addresses returned
  ****************************************************************************/
struct in_addr *
name_query (int fd, const char *name, int name_type, BOOL bcast, BOOL recurse,
            struct in_addr to_ip, int *count, void (*fn) (struct packet_struct *))
{
    BOOL found = False;
    int i, retries = 3;
    int retry_time = bcast ? 250 : 2000;
    struct timeval tval;
    struct packet_struct p;
    struct packet_struct *p2;
    struct nmb_packet *nmb = &p.packet.nmb;
    static int name_trn_id = 0;
    struct in_addr *ip_list = NULL;

    memset ((char *) &p, '\0', sizeof (p));
    (*count) = 0;

    if (!name_trn_id)
        name_trn_id = ((unsigned) time (NULL) % (unsigned) 0x7FFF) +
            ((unsigned) getpid () % (unsigned) 100);
    name_trn_id = (name_trn_id + 1) % (unsigned) 0x7FFF;

    nmb->header.name_trn_id = name_trn_id;
    nmb->header.opcode = 0;
    nmb->header.response = False;
    nmb->header.nm_flags.bcast = bcast;
    nmb->header.nm_flags.recursion_available = False;
    nmb->header.nm_flags.recursion_desired = recurse;
    nmb->header.nm_flags.trunc = False;
    nmb->header.nm_flags.authoritative = False;
    nmb->header.rcode = 0;
    nmb->header.qdcount = 1;
    nmb->header.ancount = 0;
    nmb->header.nscount = 0;
    nmb->header.arcount = 0;

    make_nmb_name (&nmb->question.question_name, name, name_type);

    nmb->question.question_type = 0x20;
    nmb->question.question_class = 0x1;

    p.ip = to_ip;
    p.port = NMB_PORT;
    p.fd = fd;
    p.timestamp = time (NULL);
    p.packet_type = NMB_PACKET;

    GetTimeOfDay (&tval);

    if (!send_packet (&p))
        return NULL;

    retries--;

    while (1)
    {
        struct timeval tval2;
        GetTimeOfDay (&tval2);
        if (TvalDiff (&tval, &tval2) > retry_time)
        {
            if (!retries)
                break;
            if (!found && !send_packet (&p))
                return NULL;
            GetTimeOfDay (&tval);
            retries--;
        }

        if ((p2 = receive_packet (fd, NMB_PACKET, 90)))
        {
            struct nmb_packet *nmb2 = &p2->packet.nmb;
            debug_nmb_packet (p2);

            if (nmb->header.name_trn_id != nmb2->header.name_trn_id || !nmb2->header.response)
            {
                /* 
                 * Its not for us - maybe deal with it later 
                 * (put it on the queue?).
                 */
                if (fn)
                    fn (p2);
                else
                    free_packet (p2);
                continue;
            }

            if (nmb2->header.opcode != 0 ||
                nmb2->header.nm_flags.bcast || nmb2->header.rcode || !nmb2->header.ancount)
            {
                /* 
                 * XXXX what do we do with this? Could be a redirect, but
                 * we'll discard it for the moment.
                 */
                free_packet (p2);
                continue;
            }

            ip_list = (struct in_addr *) Realloc (ip_list, sizeof (ip_list[0]) *
                                                  ((*count) + nmb2->answers->rdlength / 6));
            if (ip_list)
            {
                DEBUG (fn ? 3 : 2, ("Got a positive name query response from %s ( ",
                                    inet_ntoa (p2->ip)));
                for (i = 0; i < nmb2->answers->rdlength / 6; i++)
                {
                    putip ((char *) &ip_list[(*count)], &nmb2->answers->rdata[2 + i * 6]);
                    DEBUG (fn ? 3 : 2, ("%s ", inet_ntoa (ip_list[(*count)])));
                    (*count)++;
                }
                DEBUG (fn ? 3 : 2, (")\n"));
            }

            found = True;
            retries = 0;
            free_packet (p2);
            if (fn)
                break;

            /*
             * If we're doing a unicast lookup we only
             * expect one reply. Don't wait the full 2
             * seconds if we got one. JRA.
             */
            if (!bcast && found)
                break;
        }
    }

    return ip_list;
}

/********************************************************
 Start parsing the lmhosts file.
*********************************************************/

FILE *
startlmhosts (const char *fname)
{
    FILE *fp = sys_fopen (fname, "r");
    if (!fp)
    {
        DEBUG (4, ("startlmhosts: Cannot open lmhosts file %s. Error was %s\n",
                   fname, unix_error_string (errno)));
        return NULL;
    }
    return fp;
}

/********************************************************
 Parse the next line in the lmhosts file.
*********************************************************/
BOOL
getlmhostsent (FILE * fp, pstring name, int *name_type, struct in_addr * ipaddr)
{
    pstring line;

    while (!feof (fp) && !ferror (fp))
    {
        pstring ip, flags, extra;
        char *ptr;
        int count = 0;

        *name_type = -1;

        if (!fgets_slash (line, sizeof (pstring), fp))
            continue;

        if (*line == '#')
            continue;

        pstrcpy (ip, "");
        pstrcpy (name, "");
        pstrcpy (flags, "");

        ptr = line;

        if (next_token (&ptr, ip, NULL, sizeof (ip)))
            ++count;
        if (next_token (&ptr, name, NULL, sizeof (pstring)))
            ++count;
        if (next_token (&ptr, flags, NULL, sizeof (flags)))
            ++count;
        if (next_token (&ptr, extra, NULL, sizeof (extra)))
            ++count;

        if (count <= 0)
            continue;

        if (count > 0 && count < 2)
        {
            DEBUG (0, ("getlmhostsent: Ill formed hosts line [%s]\n", line));
            continue;
        }

        if (count >= 4)
        {
            DEBUG (0, ("getlmhostsent: too many columns in lmhosts file (obsolete syntax)\n"));
            continue;
        }

        DEBUG (4, ("getlmhostsent: lmhost entry: %s %s %s\n", ip, name, flags));

        if (strchr (flags, 'G') || strchr (flags, 'S'))
        {
            DEBUG (0, ("getlmhostsent: group flag in lmhosts ignored (obsolete)\n"));
            continue;
        }

        *ipaddr = *interpret_addr2 (ip);

        /* Extra feature. If the name ends in '#XX', where XX is a hex number,
           then only add that name type. */
        if ((ptr = strchr (name, '#')) != NULL)
        {
            char *endptr;

            ptr++;
            *name_type = (int) strtol (ptr, &endptr, 16);

            if (!*ptr || (endptr == ptr))
            {
                DEBUG (0, ("getlmhostsent: invalid name %s containing '#'.\n", name));
                continue;
            }

            *(--ptr) = '\0';    /* Truncate at the '#' */
        }

        return True;
    }

    return False;
}

/********************************************************
 Finish parsing the lmhosts file.
*********************************************************/

void
endlmhosts (FILE * fp)
{
    fclose (fp);
}

/********************************************************
resolve via "bcast" method
*********************************************************/
static BOOL
resolve_bcast (const char *name, struct in_addr *return_ip, int name_type)
{
    int sock, i;

    /*
     * "bcast" means do a broadcast lookup on all the local interfaces.
     */

    DEBUG (3, ("resolve_name: Attempting broadcast lookup for name %s<0x%x>\n", name, name_type));

    sock = open_socket_in (SOCK_DGRAM, 0, 3, interpret_addr (lp_socket_address ()), True);

    if (sock != -1)
    {
        struct in_addr *iplist = NULL;
        int count;
        int num_interfaces = iface_count ();
        set_socket_options (sock, "SO_BROADCAST");
        /*
         * Lookup the name on all the interfaces, return on
         * the first successful match.
         */
        for (i = 0; i < num_interfaces; i++)
        {
            struct in_addr sendto_ip;
            /* Done this way to fix compiler error on IRIX 5.x */
            sendto_ip = *iface_bcast (*iface_n_ip (i));
            iplist = name_query (sock, name, name_type, True, True, sendto_ip, &count, NULL);
            if (iplist != NULL)
            {
                *return_ip = iplist[0];
                free ((char *) iplist);
                close (sock);
                return True;
            }
        }
        close (sock);
    }

    return False;
}



/********************************************************
resolve via "wins" method
*********************************************************/
static BOOL
resolve_wins (const char *name, struct in_addr *return_ip, int name_type)
{
    int sock;
    struct in_addr wins_ip;
    BOOL wins_ismyip;

    /*
     * "wins" means do a unicast lookup to the WINS server.
     * Ignore if there is no WINS server specified or if the
     * WINS server is one of our interfaces (if we're being
     * called from within nmbd - we can't do this call as we
     * would then block).
     */

    DEBUG (3, ("resolve_name: Attempting wins lookup for name %s<0x%x>\n", name, name_type));

    if (!*lp_wins_server ())
    {
        DEBUG (3, ("resolve_name: WINS server resolution selected and no WINS server present.\n"));
        return False;
    }

    wins_ip = *interpret_addr2 (lp_wins_server ());
    wins_ismyip = ismyip (wins_ip);

    if ((wins_ismyip && !global_in_nmbd) || !wins_ismyip)
    {
        sock = open_socket_in (SOCK_DGRAM, 0, 3, interpret_addr (lp_socket_address ()), True);

        if (sock != -1)
        {
            struct in_addr *iplist = NULL;
            int count;
            iplist = name_query (sock, name, name_type, False, True, wins_ip, &count, NULL);
            if (iplist != NULL)
            {
                *return_ip = iplist[0];
                free ((char *) iplist);
                close (sock);
                return True;
            }
            close (sock);
        }
    }

    return False;
}


/********************************************************
resolve via "lmhosts" method
*********************************************************/
static BOOL
resolve_lmhosts (const char *name, struct in_addr *return_ip, int name_type)
{
    /*
     * "lmhosts" means parse the local lmhosts file.
     */

    FILE *fp;
    pstring lmhost_name;
    int name_type2;

    DEBUG (3, ("resolve_name: Attempting lmhosts lookup for name %s<0x%x>\n", name, name_type));

    fp = startlmhosts (LMHOSTSFILE);
    if (fp)
    {
        while (getlmhostsent (fp, lmhost_name, &name_type2, return_ip))
        {
            if (strequal (name, lmhost_name) && ((name_type2 == -1) || (name_type == name_type2)))
            {
                endlmhosts (fp);
                return True;
            }
        }
        endlmhosts (fp);
    }
    return False;
}


/********************************************************
resolve via "hosts" method
*********************************************************/
static BOOL
resolve_hosts (const char *name, struct in_addr *return_ip)
{
    /*
     * "host" means do a localhost, or dns lookup.
     */
    struct hostent *hp;

    DEBUG (3, ("resolve_name: Attempting host lookup for name %s<0x20>\n", name));

    if (((hp = Get_Hostbyname (name)) != NULL) && (hp->h_addr != NULL))
    {
        putip ((char *) return_ip, (char *) hp->h_addr);
        return True;
    }
    return False;
}


/********************************************************
 Resolve a name into an IP address. Use this function if
 the string is either an IP address, DNS or host name
 or NetBIOS name. This uses the name switch in the
 smb.conf to determine the order of name resolution.
*********************************************************/
BOOL
resolve_name (const char *name, struct in_addr * return_ip, int name_type)
{
    int i;
    BOOL pure_address = True;
    pstring name_resolve_list;
    fstring tok;
    char *ptr;

    if (strcmp (name, "0.0.0.0") == 0)
    {
        return_ip->s_addr = 0;
        return True;
    }
    if (strcmp (name, "255.255.255.255") == 0)
    {
        return_ip->s_addr = 0xFFFFFFFF;
        return True;
    }

    for (i = 0; pure_address && name[i]; i++)
        if (!(isdigit ((int) name[i]) || name[i] == '.'))
            pure_address = False;

    /* if it's in the form of an IP address then get the lib to interpret it */
    if (pure_address)
    {
        return_ip->s_addr = inet_addr (name);
        return True;
    }

    pstrcpy (name_resolve_list, lp_name_resolve_order ());
    if (name_resolve_list == NULL || *name_resolve_list == '\0')
        pstrcpy (name_resolve_list, "host");
    ptr = name_resolve_list;

    while (next_token (&ptr, tok, LIST_SEP, sizeof (tok)))
    {
        if ((strequal (tok, "host") || strequal (tok, "hosts")))
        {
            if (name_type == 0x20 && resolve_hosts (name, return_ip))
            {
                return True;
            }
        }
        else if (strequal (tok, "lmhosts"))
        {
            if (resolve_lmhosts (name, return_ip, name_type))
            {
                return True;
            }
        }
        else if (strequal (tok, "wins"))
        {
            /* don't resolve 1D via WINS */
            if (name_type != 0x1D && resolve_wins (name, return_ip, name_type))
            {
                return True;
            }
        }
        else if (strequal (tok, "bcast"))
        {
            if (resolve_bcast (name, return_ip, name_type))
            {
                return True;
            }
        }
        else
        {
            DEBUG (0, ("resolve_name: unknown name switch type %s\n", tok));
        }
    }

    return False;
}


#if 0
/********************************************************
find the IP address of the master browser or DMB for a workgroup
*********************************************************/
BOOL
find_master_ip (char *group, struct in_addr * master_ip)
{
    if (resolve_name (group, master_ip, 0x1D))
        return True;

    return resolve_name (group, master_ip, 0x1B);
}
#endif /* 0 */
