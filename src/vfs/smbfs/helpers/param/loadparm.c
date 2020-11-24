/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   Parameter loading functions

   Copyright (C) Karl Auer 1993-1998

   Copyright (C) 2011-2020
   Free Software Foundation, Inc.

   Largely re-written by Andrew Tridgell, September 1994

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

/*
 *  Load parameters.
 *
 *  This module provides suitable callback functions for the params
 *  module. It builds the internal table of service details which is
 *  then used by the rest of the server.
 *
 * To add a parameter:
 *
 * 1) add it to the global or service structure definition
 * 2) add it to the parm_table
 * 3) add it to the list of available functions (eg: using FN_GLOBAL_STRING())
 * 4) If it's a global then initialise it in init_globals. If a local
 *    (ie. service) parameter then initialise it in the sDefault structure
 *  
 *
 * Notes:
 *   The configuration file is processed sequentially for speed. It is NOT
 *   accessed randomly as happens in 'real' Windows. For this reason, there
 *   is a fair bit of sequence-dependent code here - ie., code which assumes
 *   that certain things happen before others. In particular, the code which
 *   happens at the boundary between sections is delicately poised, so be
 *   careful!
 *
 */

#include "includes.h"

/* Set default coding system for KANJI if none specified in Makefile. */
/* 
 * We treat KANJI specially due to historical precedent (it was the
 * first non-english codepage added to Samba). With the new dynamic
 * codepage support this is not needed anymore.
 *
 * The define 'KANJI' is being overloaded to mean 'use kanji codepage
 * by default' and also 'this is the filename-to-disk conversion 
 * method to use'. This really should be removed and all control
 * over this left in the smb.conf parameters 'client codepage'
 * and 'coding system'.
 */
#ifndef KANJI
#define KANJI "sbcs"
#endif /* KANJI */

BOOL in_client = False;         /* Not in the client by default */
BOOL bLoaded = False;

extern int DEBUGLEVEL;
extern pstring user_socket_options;
extern pstring global_myname;
pstring global_scope = "";

#ifndef GLOBAL_NAME
#define GLOBAL_NAME "global"
#endif

#ifndef PRINTERS_NAME
#define PRINTERS_NAME "printers"
#endif

#ifndef HOMES_NAME
#define HOMES_NAME "homes"
#endif

/* some helpful bits */
#define pSERVICE(i) ServicePtrs[i]
#define iSERVICE(i) (*pSERVICE(i))
#define LP_SNUM_OK(iService) (((iService) >= 0) && ((iService) < iNumServices) && iSERVICE(iService).valid)
#define VALID(i) iSERVICE(i).valid

int keepalive = DEFAULT_KEEPALIVE;
static BOOL use_getwd_cache;

extern int extra_time_offset;

/* 
 * This structure describes global (ie., server-wide) parameters.
 */
typedef struct
{
    char *szPrintcapname;
    char *szLockDir;
    char *szRootdir;
    char *szDefaultService;
    char *szDfree;
    char *szMsgCommand;
    char *szHostsEquiv;
    char *szServerString;
    char *szAutoServices;
    char *szPasswdProgram;
    char *szPasswdChat;
    char *szLogFile;
    char *szConfigFile;
    char *szSMBPasswdFile;
    char *szPasswordServer;
    char *szSocketOptions;
    char *szValidChars;
    char *szWorkGroup;
    char *szDomainAdminGroup;
    char *szDomainGuestGroup;
    char *szDomainAdminUsers;
    char *szDomainGuestUsers;
    char *szDomainHostsallow;
    char *szDomainHostsdeny;
    char *szUsernameMap;
#ifdef USING_GROUPNAME_MAP
    char *szGroupnameMap;
#endif                          /* USING_GROUPNAME_MAP */
    char *szCharacterSet;
    char *szLogonScript;
    char *szLogonPath;
    char *szLogonDrive;
    char *szLogonHome;
    char *szSmbrun;
    char *szWINSserver;
    char *szCodingSystem;
    char *szInterfaces;
    char *szRemoteAnnounce;
    char *szRemoteBrowseSync;
    char *szSocketAddress;
    char *szNISHomeMapName;
    char *szAnnounceVersion;    /* This is initialised in init_globals */
    char *szNetbiosAliases;
    char *szDomainOtherSIDs;
    char *szDomainGroups;
    char *szDriverFile;
    char *szNameResolveOrder;
    char *szLdapServer;
    char *szLdapSuffix;
    char *szLdapFilter;
    char *szLdapRoot;
    char *szLdapRootPassword;
    char *szPanicAction;
    char *szAddUserScript;
    char *szDelUserScript;
    int max_log_size;
    int mangled_stack;
    int max_xmit;
    int max_mux;
    int max_open_files;
    int max_packet;
    int pwordlevel;
    int unamelevel;
    int deadtime;
    int maxprotocol;
    int security;
    int maxdisksize;
    int lpqcachetime;
    int syslog;
    int os_level;
    int max_ttl;
    int max_wins_ttl;
    int min_wins_ttl;
    int ReadSize;
    int lm_announce;
    int lm_interval;
    int shmem_size;
    int client_code_page;
    int announce_as;            /* This is initialised in init_globals */
    int machine_password_timeout;
    int change_notify_timeout;
    int stat_cache_size;
    int map_to_guest;
    int min_passwd_length;
    int oplock_break_wait_time;
#ifdef WITH_LDAP
    int ldap_port;
#endif                          /* WITH_LDAP */
#ifdef WITH_SSL
    int sslVersion;
    char *sslHostsRequire;
    char *sslHostsResign;
    char *sslCaCertDir;
    char *sslCaCertFile;
    char *sslCert;
    char *sslPrivKey;
    char *sslClientCert;
    char *sslClientPrivKey;
    char *sslCiphers;
    BOOL sslEnabled;
    BOOL sslReqClientCert;
    BOOL sslReqServerCert;
    BOOL sslCompatibility;
#endif                          /* WITH_SSL */
    BOOL bDNSproxy;
    BOOL bWINSsupport;
    BOOL bWINSproxy;
    BOOL bLocalMaster;
    BOOL bPreferredMaster;
    BOOL bDomainMaster;
    BOOL bDomainLogons;
    BOOL bEncryptPasswords;
    BOOL bUpdateEncrypt;
    BOOL bStripDot;
    BOOL bNullPasswords;
    BOOL bLoadPrinters;
    BOOL bUseRhosts;
    BOOL bReadRaw;
    BOOL bWriteRaw;
    BOOL bReadPrediction;
    BOOL bReadbmpx;
    BOOL bSyslogOnly;
    BOOL bBrowseList;
    BOOL bUnixRealname;
    BOOL bNISHomeMap;
    BOOL bTimeServer;
    BOOL bBindInterfacesOnly;
    BOOL bUnixPasswdSync;
    BOOL bPasswdChatDebug;
    BOOL bOleLockingCompat;
    BOOL bTimestampLogs;
    BOOL bNTSmbSupport;
    BOOL bNTPipeSupport;
    BOOL bNTAclSupport;
    BOOL bStatCache;
    BOOL bKernelOplocks;
    BOOL bAllowTrustedDomains;
    BOOL bRestrictAnonymous;
} global;

static global Globals;



/* 
 * This structure describes a single service. 
 */
typedef struct
{
    BOOL valid;
    char *szService;
    char *szPath;
    char *szUsername;
    char *szGuestaccount;
    char *szInvalidUsers;
    char *szValidUsers;
    char *szAdminUsers;
    char *szCopy;
    char *szInclude;
    char *szPreExec;
    char *szPostExec;
    char *szRootPreExec;
    char *szRootPostExec;
    char *szPrintcommand;
    char *szLpqcommand;
    char *szLprmcommand;
    char *szLppausecommand;
    char *szLpresumecommand;
    char *szQueuepausecommand;
    char *szQueueresumecommand;
    char *szPrintername;
    char *szPrinterDriver;
    char *szPrinterDriverLocation;
    char *szDontdescend;
    char *szHostsallow;
    char *szHostsdeny;
    char *szMagicScript;
    char *szMagicOutput;
    char *szMangledMap;
    char *szVetoFiles;
    char *szHideFiles;
    char *szVetoOplockFiles;
    char *comment;
    char *force_user;
    char *force_group;
    char *readlist;
    char *writelist;
    char *volume;
    char *fstype;
    int iMinPrintSpace;
    int iCreate_mask;
    int iCreate_force_mode;
    int iDir_mask;
    int iDir_force_mode;
    int iMaxConnections;
    int iDefaultCase;
    int iPrinting;
    int iOplockContentionLimit;
    BOOL bAlternatePerm;
    BOOL bRevalidate;
    BOOL bCaseSensitive;
    BOOL bCasePreserve;
    BOOL bShortCasePreserve;
    BOOL bCaseMangle;
    BOOL status;
    BOOL bHideDotFiles;
    BOOL bBrowseable;
    BOOL bAvailable;
    BOOL bRead_only;
    BOOL bNo_set_dir;
    BOOL bGuest_only;
    BOOL bGuest_ok;
    BOOL bPrint_ok;
    BOOL bPostscript;
    BOOL bMap_system;
    BOOL bMap_hidden;
    BOOL bMap_archive;
    BOOL bLocking;
    BOOL bStrictLocking;
    BOOL bShareModes;
    BOOL bOpLocks;
    BOOL bOnlyUser;
    BOOL bMangledNames;
    BOOL bWidelinks;
    BOOL bSymlinks;
    BOOL bSyncAlways;
    BOOL bStrictSync;
    char magic_char;
    BOOL *copymap;
    BOOL bDeleteReadonly;
    BOOL bFakeOplocks;
    BOOL bDeleteVetoFiles;
    BOOL bDosFiletimes;
    BOOL bDosFiletimeResolution;
    BOOL bFakeDirCreateTimes;
    BOOL bBlockingLocks;
    BOOL bMangleLocks;
    char dummy[3];              /* for alignment */
} service;


/* This is a default service used to prime a services structure */
static service sDefault = {
    True,                       /* valid */
    NULL,                       /* szService */
    NULL,                       /* szPath */
    NULL,                       /* szUsername */
    NULL,                       /* szGuestAccount  - this is set in init_globals() */
    NULL,                       /* szInvalidUsers */
    NULL,                       /* szValidUsers */
    NULL,                       /* szAdminUsers */
    NULL,                       /* szCopy */
    NULL,                       /* szInclude */
    NULL,                       /* szPreExec */
    NULL,                       /* szPostExec */
    NULL,                       /* szRootPreExec */
    NULL,                       /* szRootPostExec */
    NULL,                       /* szPrintcommand */
    NULL,                       /* szLpqcommand */
    NULL,                       /* szLprmcommand */
    NULL,                       /* szLppausecommand */
    NULL,                       /* szLpresumecommand */
    NULL,                       /* szQueuepausecommand */
    NULL,                       /* szQueueresumecommand */
    NULL,                       /* szPrintername */
    NULL,                       /* szPrinterDriver - this is set in init_globals() */
    NULL,                       /* szPrinterDriverLocation */
    NULL,                       /* szDontdescend */
    NULL,                       /* szHostsallow */
    NULL,                       /* szHostsdeny */
    NULL,                       /* szMagicScript */
    NULL,                       /* szMagicOutput */
    NULL,                       /* szMangledMap */
    NULL,                       /* szVetoFiles */
    NULL,                       /* szHideFiles */
    NULL,                       /* szVetoOplockFiles */
    NULL,                       /* comment */
    NULL,                       /* force user */
    NULL,                       /* force group */
    NULL,                       /* readlist */
    NULL,                       /* writelist */
    NULL,                       /* volume */
    NULL,                       /* fstype */
    0,                          /* iMinPrintSpace */
    0744,                       /* iCreate_mask */
    0000,                       /* iCreate_force_mode */
    0755,                       /* iDir_mask */
    0000,                       /* iDir_force_mode */
    0,                          /* iMaxConnections */
    CASE_LOWER,                 /* iDefaultCase */
    DEFAULT_PRINTING,           /* iPrinting */
    2,                          /* iOplockContentionLimit */
    False,                      /* bAlternatePerm */
    False,                      /* revalidate */
    False,                      /* case sensitive */
    True,                       /* case preserve */
    True,                       /* short case preserve */
    False,                      /* case mangle */
    True,                       /* status */
    True,                       /* bHideDotFiles */
    True,                       /* bBrowseable */
    True,                       /* bAvailable */
    True,                       /* bRead_only */
    True,                       /* bNo_set_dir */
    False,                      /* bGuest_only */
    False,                      /* bGuest_ok */
    False,                      /* bPrint_ok */
    False,                      /* bPostscript */
    False,                      /* bMap_system */
    False,                      /* bMap_hidden */
    True,                       /* bMap_archive */
    True,                       /* bLocking */
    False,                      /* bStrictLocking */
    True,                       /* bShareModes */
    True,                       /* bOpLocks */
    False,                      /* bOnlyUser */
    True,                       /* bMangledNames */
    True,                       /* bWidelinks */
    True,                       /* bSymlinks */
    False,                      /* bSyncAlways */
    False,                      /* bStrictSync */
    '~',                        /* magic char */
    NULL,                       /* copymap */
    False,                      /* bDeleteReadonly */
    False,                      /* bFakeOplocks */
    False,                      /* bDeleteVetoFiles */
    False,                      /* bDosFiletimes */
    False,                      /* bDosFiletimeResolution */
    False,                      /* bFakeDirCreateTimes */
    True,                       /* bBlockingLocks */
    True,                       /* bMangleLocks */
    ""                          /* dummy */
};



/* local variables */
static service **ServicePtrs = NULL;
static int iNumServices = 0;
static int iServiceIndex = 0;
static BOOL bInGlobalSection = True;
static BOOL bGlobalOnly = False;

#define NUMPARAMETERS (sizeof(parm_table) / sizeof(struct parm_struct))

/* prototypes for the special type handlers */
static BOOL handle_valid_chars (const char *pszParmValue, char **ptr);
static BOOL handle_include (const char *pszParmValue, char **ptr);
static BOOL handle_copy (const char *pszParmValue, char **ptr);
static BOOL handle_character_set (const char *pszParmValue, char **ptr);
static BOOL handle_coding_system (const char *pszParmValue, char **ptr);
#if 0
static void set_default_server_announce_type (void);
#endif /* 0 */
static struct enum_list const enum_protocol[] =
    { {PROTOCOL_NT1, "NT1"}, {PROTOCOL_LANMAN2, "LANMAN2"},
{PROTOCOL_LANMAN1, "LANMAN1"}, {PROTOCOL_CORE, "CORE"},
{PROTOCOL_COREPLUS, "COREPLUS"},
{PROTOCOL_COREPLUS, "CORE+"}, {-1, NULL}
};

static struct enum_list const enum_security[] = { {SEC_SHARE, "SHARE"}, {SEC_USER, "USER"},
{SEC_SERVER, "SERVER"}, {SEC_DOMAIN, "DOMAIN"},
{-1, NULL}
};

static struct enum_list const enum_printing[] = { {PRINT_SYSV, "sysv"}, {PRINT_AIX, "aix"},
{PRINT_HPUX, "hpux"}, {PRINT_BSD, "bsd"},
{PRINT_QNX, "qnx"}, {PRINT_PLP, "plp"},
{PRINT_LPRNG, "lprng"}, {PRINT_SOFTQ, "softq"},
{-1, NULL}
};

/* Types of machine we can announce as. */
#define ANNOUNCE_AS_NT_SERVER 1
#define ANNOUNCE_AS_WIN95 2
#define ANNOUNCE_AS_WFW 3
#define ANNOUNCE_AS_NT_WORKSTATION 4

/* *INDENT-OFF* */
static struct enum_list const enum_announce_as[] =
{
    {ANNOUNCE_AS_NT_SERVER, "NT"},
    {ANNOUNCE_AS_NT_SERVER, "NT Server"},
    {ANNOUNCE_AS_NT_WORKSTATION, "NT Workstation"},
    {ANNOUNCE_AS_WIN95, "win95"},
    {ANNOUNCE_AS_WFW, "WfW"},
    {-1, NULL}
};
/* *INDENT-ON* */

static struct enum_list const enum_case[] =
    { {CASE_LOWER, "lower"}, {CASE_UPPER, "upper"}, {-1, NULL} };

static struct enum_list const enum_lm_announce[] =
    { {0, "False"}, {1, "True"}, {2, "Auto"}, {-1, NULL} };

/* 
   Do you want session setups at user level security with a invalid
   password to be rejected or allowed in as guest? WinNT rejects them
   but it can be a pain as it means "net view" needs to use a password

   You have 3 choices in the setting of map_to_guest:

   "Never" means session setups with an invalid password
   are rejected. This is the default.

   "Bad User" means session setups with an invalid password
   are rejected, unless the username does not exist, in which case it
   is treated as a guest login

   "Bad Password" means session setups with an invalid password
   are treated as a guest login

   Note that map_to_guest only has an effect in user or server
   level security.
 */

/* *INDENT-OFF* */
static struct enum_list const enum_map_to_guest[] =
{
    {NEVER_MAP_TO_GUEST, "Never"},
    {MAP_TO_GUEST_ON_BAD_USER, "Bad User"},
    {MAP_TO_GUEST_ON_BAD_PASSWORD, "Bad Password"},
    {-1, NULL}
};
/* *INDENT-ON* */

#ifdef WITH_SSL
static struct enum_list const enum_ssl_version[] = { {SMB_SSL_V2, "ssl2"}, {SMB_SSL_V3, "ssl3"},
{SMB_SSL_V23, "ssl2or3"}, {SMB_SSL_TLS1, "tls1"}, {-1, NULL}
};
#endif

#define PARM_SEPARATOR(label) {label, P_SEP, P_SEPARATOR, NULL, NULL, NULL, 0, {0}}
/* note that we do not initialise the defaults union - it is not allowed in ANSI C */
static struct parm_struct const parm_table[] = {
    PARM_SEPARATOR ("Base Options"),
    {"comment", P_STRING, P_LOCAL, &sDefault.comment, NULL, NULL,
     FLAG_BASIC | FLAG_SHARE | FLAG_PRINT, {0}},
    {"path", P_STRING, P_LOCAL, &sDefault.szPath, NULL, NULL, FLAG_BASIC | FLAG_SHARE | FLAG_PRINT,
     {0}},
    {"directory", P_STRING, P_LOCAL, &sDefault.szPath, NULL, NULL, 0, {0}},
    {"workgroup", P_USTRING, P_GLOBAL, &Globals.szWorkGroup, NULL, NULL, FLAG_BASIC, {0}},
    {"netbios name", P_UGSTRING, P_GLOBAL, global_myname, NULL, NULL, FLAG_BASIC, {0}},
    {"netbios aliases", P_STRING, P_GLOBAL, &Globals.szNetbiosAliases, NULL, NULL, 0, {0}},
    {"netbios scope", P_UGSTRING, P_GLOBAL, global_scope, NULL, NULL, 0, {0}},
    {"server string", P_STRING, P_GLOBAL, &Globals.szServerString, NULL, NULL, FLAG_BASIC, {0}},
    {"interfaces", P_STRING, P_GLOBAL, &Globals.szInterfaces, NULL, NULL, FLAG_BASIC, {0}},
    {"bind interfaces only", P_BOOL, P_GLOBAL, &Globals.bBindInterfacesOnly, NULL, NULL, 0, {0}},

    PARM_SEPARATOR ("Security Options"),
    {"security", P_ENUM, P_GLOBAL, &Globals.security, NULL, enum_security, FLAG_BASIC, {0}},
    {"encrypt passwords", P_BOOL, P_GLOBAL, &Globals.bEncryptPasswords, NULL, NULL, FLAG_BASIC,
     {0}},
    {"update encrypted", P_BOOL, P_GLOBAL, &Globals.bUpdateEncrypt, NULL, NULL, FLAG_BASIC, {0}},
    {"allow trusted domains", P_BOOL, P_GLOBAL, &Globals.bAllowTrustedDomains, NULL, NULL, 0, {0}},
    {"alternate permissions", P_BOOL, P_LOCAL, &sDefault.bAlternatePerm, NULL, NULL,
     FLAG_GLOBAL | FLAG_DEPRECATED, {0}},
    {"hosts equiv", P_STRING, P_GLOBAL, &Globals.szHostsEquiv, NULL, NULL, 0, {0}},
    {"min passwd length", P_INTEGER, P_GLOBAL, &Globals.min_passwd_length, NULL, NULL, 0, {0}},
    {"min password length", P_INTEGER, P_GLOBAL, &Globals.min_passwd_length, NULL, NULL, 0, {0}},
    {"map to guest", P_ENUM, P_GLOBAL, &Globals.map_to_guest, NULL, enum_map_to_guest, 0, {0}},
    {"null passwords", P_BOOL, P_GLOBAL, &Globals.bNullPasswords, NULL, NULL, 0, {0}},
    {"password server", P_STRING, P_GLOBAL, &Globals.szPasswordServer, NULL, NULL, 0, {0}},
    {"smb passwd file", P_STRING, P_GLOBAL, &Globals.szSMBPasswdFile, NULL, NULL, 0, {0}},
    {"root directory", P_STRING, P_GLOBAL, &Globals.szRootdir, NULL, NULL, 0, {0}},
    {"root dir", P_STRING, P_GLOBAL, &Globals.szRootdir, NULL, NULL, 0, {0}},
    {"root", P_STRING, P_GLOBAL, &Globals.szRootdir, NULL, NULL, 0, {0}},
    {"passwd program", P_STRING, P_GLOBAL, &Globals.szPasswdProgram, NULL, NULL, 0, {0}},
    {"passwd chat", P_STRING, P_GLOBAL, &Globals.szPasswdChat, NULL, NULL, 0, {0}},
    {"passwd chat debug", P_BOOL, P_GLOBAL, &Globals.bPasswdChatDebug, NULL, NULL, 0, {0}},
    {"username map", P_STRING, P_GLOBAL, &Globals.szUsernameMap, NULL, NULL, 0, {0}},
    {"password level", P_INTEGER, P_GLOBAL, &Globals.pwordlevel, NULL, NULL, 0, {0}},
    {"username level", P_INTEGER, P_GLOBAL, &Globals.unamelevel, NULL, NULL, 0, {0}},
    {"unix password sync", P_BOOL, P_GLOBAL, &Globals.bUnixPasswdSync, NULL, NULL, 0, {0}},
    {"restrict anonymous", P_BOOL, P_GLOBAL, &Globals.bRestrictAnonymous, NULL, NULL, 0, {0}},
    {"revalidate", P_BOOL, P_LOCAL, &sDefault.bRevalidate, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"use rhosts", P_BOOL, P_GLOBAL, &Globals.bUseRhosts, NULL, NULL, 0, {0}},
    {"username", P_STRING, P_LOCAL, &sDefault.szUsername, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"user", P_STRING, P_LOCAL, &sDefault.szUsername, NULL, NULL, 0, {0}},
    {"users", P_STRING, P_LOCAL, &sDefault.szUsername, NULL, NULL, 0, {0}},
    {"guest account", P_STRING, P_LOCAL, &sDefault.szGuestaccount, NULL, NULL,
     FLAG_BASIC | FLAG_SHARE | FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"invalid users", P_STRING, P_LOCAL, &sDefault.szInvalidUsers, NULL, NULL,
     FLAG_GLOBAL | FLAG_SHARE, {0}},
    {"valid users", P_STRING, P_LOCAL, &sDefault.szValidUsers, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"admin users", P_STRING, P_LOCAL, &sDefault.szAdminUsers, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"read list", P_STRING, P_LOCAL, &sDefault.readlist, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE, {0}},
    {"write list", P_STRING, P_LOCAL, &sDefault.writelist, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"force user", P_STRING, P_LOCAL, &sDefault.force_user, NULL, NULL, FLAG_SHARE, {0}},
    {"force group", P_STRING, P_LOCAL, &sDefault.force_group, NULL, NULL, FLAG_SHARE, {0}},
    {"group", P_STRING, P_LOCAL, &sDefault.force_group, NULL, NULL, 0, {0}},
    {"read only", P_BOOL, P_LOCAL, &sDefault.bRead_only, NULL, NULL, FLAG_BASIC | FLAG_SHARE, {0}},
    {"write ok", P_BOOLREV, P_LOCAL, &sDefault.bRead_only, NULL, NULL, 0, {0}},
    {"writeable", P_BOOLREV, P_LOCAL, &sDefault.bRead_only, NULL, NULL, 0, {0}},
    {"writable", P_BOOLREV, P_LOCAL, &sDefault.bRead_only, NULL, NULL, 0, {0}},
    {"create mask", P_OCTAL, P_LOCAL, &sDefault.iCreate_mask, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"create mode", P_OCTAL, P_LOCAL, &sDefault.iCreate_mask, NULL, NULL, FLAG_GLOBAL, {0}},
    {"force create mode", P_OCTAL, P_LOCAL, &sDefault.iCreate_force_mode, NULL, NULL,
     FLAG_GLOBAL | FLAG_SHARE, {0}},
    {"directory mask", P_OCTAL, P_LOCAL, &sDefault.iDir_mask, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE,
     {0}},
    {"directory mode", P_OCTAL, P_LOCAL, &sDefault.iDir_mask, NULL, NULL, FLAG_GLOBAL, {0}},
    {"force directory mode", P_OCTAL, P_LOCAL, &sDefault.iDir_force_mode, NULL, NULL,
     FLAG_GLOBAL | FLAG_SHARE, {0}},
    {"guest only", P_BOOL, P_LOCAL, &sDefault.bGuest_only, NULL, NULL, FLAG_SHARE, {0}},
    {"only guest", P_BOOL, P_LOCAL, &sDefault.bGuest_only, NULL, NULL, 0, {0}},
    {"guest ok", P_BOOL, P_LOCAL, &sDefault.bGuest_ok, NULL, NULL,
     FLAG_BASIC | FLAG_SHARE | FLAG_PRINT, {0}},
    {"public", P_BOOL, P_LOCAL, &sDefault.bGuest_ok, NULL, NULL, 0, {0}},
    {"only user", P_BOOL, P_LOCAL, &sDefault.bOnlyUser, NULL, NULL, FLAG_SHARE, {0}},
    {"hosts allow", P_STRING, P_LOCAL, &sDefault.szHostsallow, NULL, NULL,
     FLAG_GLOBAL | FLAG_BASIC | FLAG_SHARE | FLAG_PRINT, {0}},
    {"allow hosts", P_STRING, P_LOCAL, &sDefault.szHostsallow, NULL, NULL, 0, {0}},
    {"hosts deny", P_STRING, P_LOCAL, &sDefault.szHostsdeny, NULL, NULL,
     FLAG_GLOBAL | FLAG_BASIC | FLAG_SHARE | FLAG_PRINT, {0}},
    {"deny hosts", P_STRING, P_LOCAL, &sDefault.szHostsdeny, NULL, NULL, 0, {0}},

#ifdef WITH_SSL
    PARM_SEPARATOR ("Secure Socket Layer Options"),
    {"ssl", P_BOOL, P_GLOBAL, &Globals.sslEnabled, NULL, NULL, 0, {0}},
    {"ssl hosts", P_STRING, P_GLOBAL, &Globals.sslHostsRequire, NULL, NULL, 0, {0}},
    {"ssl hosts resign", P_STRING, P_GLOBAL, &Globals.sslHostsResign, NULL, NULL, 0},
    {"ssl CA certDir", P_STRING, P_GLOBAL, &Globals.sslCaCertDir, NULL, NULL, 0, {0}},
    {"ssl CA certFile", P_STRING, P_GLOBAL, &Globals.sslCaCertFile, NULL, NULL, 0, {0}},
    {"ssl server cert", P_STRING, P_GLOBAL, &Globals.sslCert, NULL, NULL, 0, {0}},
    {"ssl server key", P_STRING, P_GLOBAL, &Globals.sslPrivKey, NULL, NULL, 0, {0}},
    {"ssl client cert", P_STRING, P_GLOBAL, &Globals.sslClientCert, NULL, NULL, 0, {0}},
    {"ssl client key", P_STRING, P_GLOBAL, &Globals.sslClientPrivKey, NULL, NULL, 0, {0}},
    {"ssl require clientcert", P_BOOL, P_GLOBAL, &Globals.sslReqClientCert, NULL, NULL, 0, {0}},
    {"ssl require servercert", P_BOOL, P_GLOBAL, &Globals.sslReqServerCert, NULL, NULL, 0, {0}},
    {"ssl ciphers", P_STRING, P_GLOBAL, &Globals.sslCiphers, NULL, NULL, 0, {0}},
    {"ssl version", P_ENUM, P_GLOBAL, &Globals.sslVersion, NULL, enum_ssl_version, 0, {0}},
    {"ssl compatibility", P_BOOL, P_GLOBAL, &Globals.sslCompatibility, NULL, NULL, 0, {0}},
#endif /* WITH_SSL */

    PARM_SEPARATOR ("Logging Options"),
    {"log level", P_INTEGER, P_GLOBAL, &DEBUGLEVEL, NULL, NULL, FLAG_BASIC, {0}},
    {"debuglevel", P_INTEGER, P_GLOBAL, &DEBUGLEVEL, NULL, NULL, 0, {0}},
    {"syslog", P_INTEGER, P_GLOBAL, &Globals.syslog, NULL, NULL, 0, {0}},
    {"syslog only", P_BOOL, P_GLOBAL, &Globals.bSyslogOnly, NULL, NULL, 0, {0}},
    {"log file", P_STRING, P_GLOBAL, &Globals.szLogFile, NULL, NULL, 0, {0}},
    {"max log size", P_INTEGER, P_GLOBAL, &Globals.max_log_size, NULL, NULL, 0, {0}},
    {"timestamp logs", P_BOOL, P_GLOBAL, &Globals.bTimestampLogs, NULL, NULL, 0, {0}},
    {"debug timestamp", P_BOOL, P_GLOBAL, &Globals.bTimestampLogs, NULL, NULL, 0, {0}},
    {"status", P_BOOL, P_LOCAL, &sDefault.status, NULL, NULL, FLAG_GLOBAL | FLAG_SHARE | FLAG_PRINT,
     {0}},

    PARM_SEPARATOR ("Protocol Options"),
    {"protocol", P_ENUM, P_GLOBAL, &Globals.maxprotocol, NULL, enum_protocol, 0, {0}},
    {"read bmpx", P_BOOL, P_GLOBAL, &Globals.bReadbmpx, NULL, NULL, 0, {0}},
    {"read raw", P_BOOL, P_GLOBAL, &Globals.bReadRaw, NULL, NULL, 0, {0}},
    {"write raw", P_BOOL, P_GLOBAL, &Globals.bWriteRaw, NULL, NULL, 0, {0}},
    {"nt smb support", P_BOOL, P_GLOBAL, &Globals.bNTSmbSupport, NULL, NULL, 0, {0}},
    {"nt pipe support", P_BOOL, P_GLOBAL, &Globals.bNTPipeSupport, NULL, NULL, 0, {0}},
    {"nt acl support", P_BOOL, P_GLOBAL, &Globals.bNTAclSupport, NULL, NULL, 0, {0}},
    {"announce version", P_STRING, P_GLOBAL, &Globals.szAnnounceVersion, NULL, NULL, 0, {0}},
    {"announce as", P_ENUM, P_GLOBAL, &Globals.announce_as, NULL, enum_announce_as, 0, {0}},
    {"max mux", P_INTEGER, P_GLOBAL, &Globals.max_mux, NULL, NULL, 0, {0}},
    {"max xmit", P_INTEGER, P_GLOBAL, &Globals.max_xmit, NULL, NULL, 0, {0}},
    {"name resolve order", P_STRING, P_GLOBAL, &Globals.szNameResolveOrder, NULL, NULL, 0, {0}},
    {"max packet", P_INTEGER, P_GLOBAL, &Globals.max_packet, NULL, NULL, 0, {0}},
    {"packet size", P_INTEGER, P_GLOBAL, &Globals.max_packet, NULL, NULL, 0, {0}},
    {"max ttl", P_INTEGER, P_GLOBAL, &Globals.max_ttl, NULL, NULL, 0, {0}},
    {"max wins ttl", P_INTEGER, P_GLOBAL, &Globals.max_wins_ttl, NULL, NULL, 0, {0}},
    {"min wins ttl", P_INTEGER, P_GLOBAL, &Globals.min_wins_ttl, NULL, NULL, 0, {0}},
    {"time server", P_BOOL, P_GLOBAL, &Globals.bTimeServer, NULL, NULL, 0, {0}},

    PARM_SEPARATOR ("Tuning Options"),
    {"change notify timeout", P_INTEGER, P_GLOBAL, &Globals.change_notify_timeout, NULL, NULL, 0,
     {0}},
    {"deadtime", P_INTEGER, P_GLOBAL, &Globals.deadtime, NULL, NULL, 0, {0}},
    {"getwd cache", P_BOOL, P_GLOBAL, &use_getwd_cache, NULL, NULL, 0, {0}},
    {"keepalive", P_INTEGER, P_GLOBAL, &keepalive, NULL, NULL, 0, {0}},
    {"lpq cache time", P_INTEGER, P_GLOBAL, &Globals.lpqcachetime, NULL, NULL, 0, {0}},
    {"max connections", P_INTEGER, P_LOCAL, &sDefault.iMaxConnections, NULL, NULL, FLAG_SHARE, {0}},
    {"max disk size", P_INTEGER, P_GLOBAL, &Globals.maxdisksize, NULL, NULL, 0, {0}},
    {"max open files", P_INTEGER, P_GLOBAL, &Globals.max_open_files, NULL, NULL, 0, {0}},
    {"min print space", P_INTEGER, P_LOCAL, &sDefault.iMinPrintSpace, NULL, NULL, FLAG_PRINT, {0}},
    {"read prediction", P_BOOL, P_GLOBAL, &Globals.bReadPrediction, NULL, NULL, 0, {0}},
    {"read size", P_INTEGER, P_GLOBAL, &Globals.ReadSize, NULL, NULL, 0, {0}},
    {"shared mem size", P_INTEGER, P_GLOBAL, &Globals.shmem_size, NULL, NULL, 0, {0}},
    {"socket options", P_GSTRING, P_GLOBAL, user_socket_options, NULL, NULL, 0, {0}},
    {"stat cache size", P_INTEGER, P_GLOBAL, &Globals.stat_cache_size, NULL, NULL, 0, {0}},
    {"strict sync", P_BOOL, P_LOCAL, &sDefault.bStrictSync, NULL, NULL, FLAG_SHARE, {0}},
    {"sync always", P_BOOL, P_LOCAL, &sDefault.bSyncAlways, NULL, NULL, FLAG_SHARE, {0}},

    PARM_SEPARATOR ("Printing Options"),
    {"load printers", P_BOOL, P_GLOBAL, &Globals.bLoadPrinters, NULL, NULL, FLAG_PRINT, {0}},
    {"printcap name", P_STRING, P_GLOBAL, &Globals.szPrintcapname, NULL, NULL, FLAG_PRINT, {0}},
    {"printcap", P_STRING, P_GLOBAL, &Globals.szPrintcapname, NULL, NULL, 0, {0}},
    {"printer driver file", P_STRING, P_GLOBAL, &Globals.szDriverFile, NULL, NULL, FLAG_PRINT, {0}},
    {"print ok", P_BOOL, P_LOCAL, &sDefault.bPrint_ok, NULL, NULL, FLAG_PRINT, {0}},
    {"printable", P_BOOL, P_LOCAL, &sDefault.bPrint_ok, NULL, NULL, 0, {0}},
    {"postscript", P_BOOL, P_LOCAL, &sDefault.bPostscript, NULL, NULL, FLAG_PRINT, {0}},
    {"printing", P_ENUM, P_LOCAL, &sDefault.iPrinting, NULL, enum_printing,
     FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"print command", P_STRING, P_LOCAL, &sDefault.szPrintcommand, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"lpq command", P_STRING, P_LOCAL, &sDefault.szLpqcommand, NULL, NULL, FLAG_PRINT | FLAG_GLOBAL,
     {0}},
    {"lprm command", P_STRING, P_LOCAL, &sDefault.szLprmcommand, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"lppause command", P_STRING, P_LOCAL, &sDefault.szLppausecommand, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"lpresume command", P_STRING, P_LOCAL, &sDefault.szLpresumecommand, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"queuepause command", P_STRING, P_LOCAL, &sDefault.szQueuepausecommand, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},
    {"queueresume command", P_STRING, P_LOCAL, &sDefault.szQueueresumecommand, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},

    {"printer name", P_STRING, P_LOCAL, &sDefault.szPrintername, NULL, NULL, FLAG_PRINT, {0}},
    {"printer", P_STRING, P_LOCAL, &sDefault.szPrintername, NULL, NULL, 0, {0}},
    {"printer driver", P_STRING, P_LOCAL, &sDefault.szPrinterDriver, NULL, NULL, FLAG_PRINT, {0}},
    {"printer driver location", P_STRING, P_LOCAL, &sDefault.szPrinterDriverLocation, NULL, NULL,
     FLAG_PRINT | FLAG_GLOBAL, {0}},


    PARM_SEPARATOR ("Filename Handling"),
    {"strip dot", P_BOOL, P_GLOBAL, &Globals.bStripDot, NULL, NULL, 0, {0}},
    {"character set", P_STRING, P_GLOBAL, &Globals.szCharacterSet, handle_character_set, NULL, 0,
     {0}},
    {"mangled stack", P_INTEGER, P_GLOBAL, &Globals.mangled_stack, NULL, NULL, 0, {0}},
    {"coding system", P_STRING, P_GLOBAL, &Globals.szCodingSystem, handle_coding_system, NULL, 0,
     {0}},
    {"client code page", P_INTEGER, P_GLOBAL, &Globals.client_code_page, NULL, NULL, 0, {0}},
    {"default case", P_ENUM, P_LOCAL, &sDefault.iDefaultCase, NULL, enum_case, FLAG_SHARE, {0}},
    {"case sensitive", P_BOOL, P_LOCAL, &sDefault.bCaseSensitive, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"casesignames", P_BOOL, P_LOCAL, &sDefault.bCaseSensitive, NULL, NULL, 0, {0}},
    {"preserve case", P_BOOL, P_LOCAL, &sDefault.bCasePreserve, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"short preserve case", P_BOOL, P_LOCAL, &sDefault.bShortCasePreserve, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"mangle case", P_BOOL, P_LOCAL, &sDefault.bCaseMangle, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"mangling char", P_CHAR, P_LOCAL, &sDefault.magic_char, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"hide dot files", P_BOOL, P_LOCAL, &sDefault.bHideDotFiles, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"delete veto files", P_BOOL, P_LOCAL, &sDefault.bDeleteVetoFiles, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"veto files", P_STRING, P_LOCAL, &sDefault.szVetoFiles, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"hide files", P_STRING, P_LOCAL, &sDefault.szHideFiles, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"veto oplock files", P_STRING, P_LOCAL, &sDefault.szVetoOplockFiles, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"map system", P_BOOL, P_LOCAL, &sDefault.bMap_system, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"map hidden", P_BOOL, P_LOCAL, &sDefault.bMap_hidden, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"map archive", P_BOOL, P_LOCAL, &sDefault.bMap_archive, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"mangled names", P_BOOL, P_LOCAL, &sDefault.bMangledNames, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"mangled map", P_STRING, P_LOCAL, &sDefault.szMangledMap, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"stat cache", P_BOOL, P_GLOBAL, &Globals.bStatCache, NULL, NULL, 0, {0}},

    PARM_SEPARATOR ("Domain Options"),
    {"domain groups", P_STRING, P_GLOBAL, &Globals.szDomainGroups, NULL, NULL, 0, {0}},
    {"domain admin group", P_STRING, P_GLOBAL, &Globals.szDomainAdminGroup, NULL, NULL, 0, {0}},
    {"domain guest group", P_STRING, P_GLOBAL, &Globals.szDomainGuestGroup, NULL, NULL, 0, {0}},
    {"domain admin users", P_STRING, P_GLOBAL, &Globals.szDomainAdminUsers, NULL, NULL, 0, {0}},
    {"domain guest users", P_STRING, P_GLOBAL, &Globals.szDomainGuestUsers, NULL, NULL, 0, {0}},
#ifdef USING_GROUPNAME_MAP
    {"groupname map", P_STRING, P_GLOBAL, &Globals.szGroupnameMap, NULL, NULL, 0, {0}},
#endif /* USING_GROUPNAME_MAP */
    {"machine password timeout", P_INTEGER, P_GLOBAL, &Globals.machine_password_timeout, NULL, NULL,
     0, {0}},

    PARM_SEPARATOR ("Logon Options"),
    {"add user script", P_STRING, P_GLOBAL, &Globals.szAddUserScript, NULL, NULL, 0, {0}},
    {"delete user script", P_STRING, P_GLOBAL, &Globals.szDelUserScript, NULL, NULL, 0, {0}},
    {"logon script", P_STRING, P_GLOBAL, &Globals.szLogonScript, NULL, NULL, 0, {0}},
    {"logon path", P_STRING, P_GLOBAL, &Globals.szLogonPath, NULL, NULL, 0, {0}},
    {"logon drive", P_STRING, P_GLOBAL, &Globals.szLogonDrive, NULL, NULL, 0, {0}},
    {"logon home", P_STRING, P_GLOBAL, &Globals.szLogonHome, NULL, NULL, 0, {0}},
    {"domain logons", P_BOOL, P_GLOBAL, &Globals.bDomainLogons, NULL, NULL, 0, {0}},

    PARM_SEPARATOR ("Browse Options"),
    {"os level", P_INTEGER, P_GLOBAL, &Globals.os_level, NULL, NULL, FLAG_BASIC, {0}},
    {"lm announce", P_ENUM, P_GLOBAL, &Globals.lm_announce, NULL, enum_lm_announce, 0, {0}},
    {"lm interval", P_INTEGER, P_GLOBAL, &Globals.lm_interval, NULL, NULL, 0, {0}},
    {"preferred master", P_BOOL, P_GLOBAL, &Globals.bPreferredMaster, NULL, NULL, FLAG_BASIC, {0}},
    {"prefered master", P_BOOL, P_GLOBAL, &Globals.bPreferredMaster, NULL, NULL, 0, {0}},
    {"local master", P_BOOL, P_GLOBAL, &Globals.bLocalMaster, NULL, NULL, FLAG_BASIC, {0}},
    {"domain master", P_BOOL, P_GLOBAL, &Globals.bDomainMaster, NULL, NULL, FLAG_BASIC, {0}},
    {"browse list", P_BOOL, P_GLOBAL, &Globals.bBrowseList, NULL, NULL, 0, {0}},
    {"browseable", P_BOOL, P_LOCAL, &sDefault.bBrowseable, NULL, NULL,
     FLAG_BASIC | FLAG_SHARE | FLAG_PRINT, {0}},
    {"browsable", P_BOOL, P_LOCAL, &sDefault.bBrowseable, NULL, NULL, 0, {0}},

    PARM_SEPARATOR ("WINS Options"),
    {"dns proxy", P_BOOL, P_GLOBAL, &Globals.bDNSproxy, NULL, NULL, 0, {0}},
    {"wins proxy", P_BOOL, P_GLOBAL, &Globals.bWINSproxy, NULL, NULL, 0, {0}},
    {"wins server", P_STRING, P_GLOBAL, &Globals.szWINSserver, NULL, NULL, FLAG_BASIC, {0}},
    {"wins support", P_BOOL, P_GLOBAL, &Globals.bWINSsupport, NULL, NULL, FLAG_BASIC, {0}},

    PARM_SEPARATOR ("Locking Options"),
    {"blocking locks", P_BOOL, P_LOCAL, &sDefault.bBlockingLocks, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"fake oplocks", P_BOOL, P_LOCAL, &sDefault.bFakeOplocks, NULL, NULL, FLAG_SHARE, {0}},
    {"kernel oplocks", P_BOOL, P_GLOBAL, &Globals.bKernelOplocks, NULL, NULL, FLAG_GLOBAL, {0}},
    {"locking", P_BOOL, P_LOCAL, &sDefault.bLocking, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"mangle locks", P_BOOL, P_LOCAL, &sDefault.bMangleLocks, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"ole locking compatibility", P_BOOL, P_GLOBAL, &Globals.bOleLockingCompat, NULL, NULL,
     FLAG_GLOBAL, {0}},
    {"oplocks", P_BOOL, P_LOCAL, &sDefault.bOpLocks, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"oplock break wait time", P_INTEGER, P_GLOBAL, &Globals.oplock_break_wait_time, NULL, NULL,
     FLAG_GLOBAL, {0}},
    {"oplock contention limit", P_INTEGER, P_LOCAL, &sDefault.iOplockContentionLimit, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"strict locking", P_BOOL, P_LOCAL, &sDefault.bStrictLocking, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"share modes", P_BOOL, P_LOCAL, &sDefault.bShareModes, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},

#ifdef WITH_LDAP
    PARM_SEPARATOR ("Ldap Options"),
    {"ldap server", P_STRING, P_GLOBAL, &Globals.szLdapServer, NULL, NULL, 0, {0}},
    {"ldap port", P_INTEGER, P_GLOBAL, &Globals.ldap_port, NULL, NULL, 0, {0}},
    {"ldap suffix", P_STRING, P_GLOBAL, &Globals.szLdapSuffix, NULL, NULL, 0, {0}},
    {"ldap filter", P_STRING, P_GLOBAL, &Globals.szLdapFilter, NULL, NULL, 0, {0}},
    {"ldap root", P_STRING, P_GLOBAL, &Globals.szLdapRoot, NULL, NULL, 0, {0}},
    {"ldap root passwd", P_STRING, P_GLOBAL, &Globals.szLdapRootPassword, NULL, NULL, 0, {0}},
#endif /* WITH_LDAP */


    PARM_SEPARATOR ("Miscellaneous Options"),
    {"smbrun", P_STRING, P_GLOBAL, &Globals.szSmbrun, NULL, NULL, 0, {0}},
    {"config file", P_STRING, P_GLOBAL, &Globals.szConfigFile, NULL, NULL, FLAG_HIDE, {0}},
    {"preload", P_STRING, P_GLOBAL, &Globals.szAutoServices, NULL, NULL, 0, {0}},
    {"auto services", P_STRING, P_GLOBAL, &Globals.szAutoServices, NULL, NULL, 0, {0}},
    {"lock dir", P_STRING, P_GLOBAL, &Globals.szLockDir, NULL, NULL, 0, {0}},
    {"lock directory", P_STRING, P_GLOBAL, &Globals.szLockDir, NULL, NULL, 0, {0}},
    {"default service", P_STRING, P_GLOBAL, &Globals.szDefaultService, NULL, NULL, 0, {0}},
    {"default", P_STRING, P_GLOBAL, &Globals.szDefaultService, NULL, NULL, 0, {0}},
    {"message command", P_STRING, P_GLOBAL, &Globals.szMsgCommand, NULL, NULL, 0, {0}},
    {"dfree command", P_STRING, P_GLOBAL, &Globals.szDfree, NULL, NULL, 0, {0}},
    {"valid chars", P_STRING, P_GLOBAL, &Globals.szValidChars, handle_valid_chars, NULL, 0, {0}},
    {"remote announce", P_STRING, P_GLOBAL, &Globals.szRemoteAnnounce, NULL, NULL, 0, {0}},
    {"remote browse sync", P_STRING, P_GLOBAL, &Globals.szRemoteBrowseSync, NULL, NULL, 0, {0}},
    {"socket address", P_STRING, P_GLOBAL, &Globals.szSocketAddress, NULL, NULL, 0, {0}},
    {"homedir map", P_STRING, P_GLOBAL, &Globals.szNISHomeMapName, NULL, NULL, 0, {0}},
    {"time offset", P_INTEGER, P_GLOBAL, &extra_time_offset, NULL, NULL, 0, {0}},
    {"unix realname", P_BOOL, P_GLOBAL, &Globals.bUnixRealname, NULL, NULL, 0, {0}},
    {"NIS homedir", P_BOOL, P_GLOBAL, &Globals.bNISHomeMap, NULL, NULL, 0, {0}},
    {"-valid", P_BOOL, P_LOCAL, &sDefault.valid, NULL, NULL, FLAG_HIDE, {0}},
    {"copy", P_STRING, P_LOCAL, &sDefault.szCopy, handle_copy, NULL, FLAG_HIDE, {0}},
    {"include", P_STRING, P_LOCAL, &sDefault.szInclude, handle_include, NULL, FLAG_HIDE, {0}},
    {"exec", P_STRING, P_LOCAL, &sDefault.szPreExec, NULL, NULL, FLAG_SHARE | FLAG_PRINT, {0}},
    {"preexec", P_STRING, P_LOCAL, &sDefault.szPreExec, NULL, NULL, 0, {0}},
    {"postexec", P_STRING, P_LOCAL, &sDefault.szPostExec, NULL, NULL, FLAG_SHARE | FLAG_PRINT, {0}},
    {"root preexec", P_STRING, P_LOCAL, &sDefault.szRootPreExec, NULL, NULL,
     FLAG_SHARE | FLAG_PRINT, {0}},
    {"root postexec", P_STRING, P_LOCAL, &sDefault.szRootPostExec, NULL, NULL,
     FLAG_SHARE | FLAG_PRINT, {0}},
    {"available", P_BOOL, P_LOCAL, &sDefault.bAvailable, NULL, NULL,
     FLAG_BASIC | FLAG_SHARE | FLAG_PRINT, {0}},
    {"volume", P_STRING, P_LOCAL, &sDefault.volume, NULL, NULL, FLAG_SHARE, {0}},
    {"fstype", P_STRING, P_LOCAL, &sDefault.fstype, NULL, NULL, FLAG_SHARE, {0}},
    {"set directory", P_BOOLREV, P_LOCAL, &sDefault.bNo_set_dir, NULL, NULL, FLAG_SHARE, {0}},
    {"wide links", P_BOOL, P_LOCAL, &sDefault.bWidelinks, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"follow symlinks", P_BOOL, P_LOCAL, &sDefault.bSymlinks, NULL, NULL, FLAG_SHARE | FLAG_GLOBAL,
     {0}},
    {"dont descend", P_STRING, P_LOCAL, &sDefault.szDontdescend, NULL, NULL, FLAG_SHARE, {0}},
    {"magic script", P_STRING, P_LOCAL, &sDefault.szMagicScript, NULL, NULL, FLAG_SHARE, {0}},
    {"magic output", P_STRING, P_LOCAL, &sDefault.szMagicOutput, NULL, NULL, FLAG_SHARE, {0}},
    {"delete readonly", P_BOOL, P_LOCAL, &sDefault.bDeleteReadonly, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"dos filetimes", P_BOOL, P_LOCAL, &sDefault.bDosFiletimes, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"dos filetime resolution", P_BOOL, P_LOCAL, &sDefault.bDosFiletimeResolution, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},

    {"fake directory create times", P_BOOL, P_LOCAL, &sDefault.bFakeDirCreateTimes, NULL, NULL,
     FLAG_SHARE | FLAG_GLOBAL, {0}},
    {"panic action", P_STRING, P_GLOBAL, &Globals.szPanicAction, NULL, NULL, 0, {0}},

    {NULL, P_BOOL, P_NONE, NULL, NULL, NULL, 0, {0}}
};



/***************************************************************************
Initialise the global parameter structure.
***************************************************************************/
static void
init_globals (void)
{
    static BOOL done_init = False;
    fstring s;

    if (!done_init)
    {
        int i;
        memset ((void *) &Globals, '\0', sizeof (Globals));

        for (i = 0; parm_table[i].label; i++)
            if ((parm_table[i].type == P_STRING ||
                 parm_table[i].type == P_USTRING) && parm_table[i].ptr)
                string_init (parm_table[i].ptr, "");

        string_set (&sDefault.szGuestaccount, GUEST_ACCOUNT);
        string_set (&sDefault.szPrinterDriver, "NULL");
        string_set (&sDefault.fstype, FSTYPE_STRING);

        done_init = True;
    }


    DEBUG (3, ("Initialising global parameters\n"));

    string_set (&Globals.szWorkGroup, WORKGROUP);
    string_set (&Globals.szPrintcapname, PRINTCAP_NAME);
    string_set (&Globals.szDriverFile, DRIVERFILE);
    string_set (&Globals.szRootdir, "/");
    string_set (&Globals.szSocketAddress, "0.0.0.0");
    string_set (&Globals.szServerString, "Samba " VERSION);
    slprintf (s, sizeof (s) - 1, "%d.%d", DEFAULT_MAJOR_VERSION, DEFAULT_MINOR_VERSION);
    string_set (&Globals.szAnnounceVersion, s);

    pstrcpy (user_socket_options, DEFAULT_SOCKET_OPTIONS);

    string_set (&Globals.szLogonDrive, "");
    /* %N is the NIS auto.home server if -DAUTOHOME is used, else same as %L */
    string_set (&Globals.szLogonHome, "\\\\%N\\%U");
    string_set (&Globals.szLogonPath, "\\\\%N\\%U\\profile");

    string_set (&Globals.szNameResolveOrder, "lmhosts host wins bcast");

    Globals.bLoadPrinters = True;
    Globals.bUseRhosts = False;
    Globals.max_packet = 65535;
    Globals.mangled_stack = 50;
    Globals.max_xmit = 65535;
    Globals.max_mux = 50;       /* This is *needed* for profile support. */
    Globals.lpqcachetime = 10;
    Globals.pwordlevel = 0;
    Globals.unamelevel = 0;
    Globals.deadtime = 0;
    Globals.max_log_size = 5000;
    Globals.max_open_files = MAX_OPEN_FILES;
    Globals.maxprotocol = PROTOCOL_NT1;
    Globals.security = SEC_USER;
    Globals.bEncryptPasswords = False;
    Globals.bUpdateEncrypt = False;
    Globals.bReadRaw = True;
    Globals.bWriteRaw = True;
    Globals.bReadPrediction = False;
    Globals.bReadbmpx = False;
    Globals.bNullPasswords = False;
    Globals.bStripDot = False;
    Globals.syslog = 1;
    Globals.bSyslogOnly = False;
    Globals.bTimestampLogs = True;
    Globals.os_level = 0;
    Globals.max_ttl = 60 * 60 * 24 * 3; /* 3 days default. */
    Globals.max_wins_ttl = 60 * 60 * 24 * 6;    /* 6 days default. */
    Globals.min_wins_ttl = 60 * 60 * 6; /* 6 hours default. */
    Globals.machine_password_timeout = 60 * 60 * 24 * 7;        /* 7 days default. */
    Globals.change_notify_timeout = 60; /* 1 minute default. */
    Globals.ReadSize = 16 * 1024;
    Globals.lm_announce = 2;    /* = Auto: send only if LM clients found */
    Globals.lm_interval = 60;
    Globals.shmem_size = SHMEM_SIZE;
    Globals.stat_cache_size = 50;       /* Number of stat translations we'll keep */
    Globals.announce_as = ANNOUNCE_AS_NT_SERVER;
    Globals.bUnixRealname = False;
#if (defined(HAVE_NETGROUP) && defined(WITH_AUTOMOUNT))
    Globals.bNISHomeMap = False;
#ifdef WITH_NISPLUS_HOME
    string_set (&Globals.szNISHomeMapName, "auto_home.org_dir");
#else
    string_set (&Globals.szNISHomeMapName, "auto.home");
#endif
#endif
    Globals.client_code_page = DEFAULT_CLIENT_CODE_PAGE;
    Globals.bTimeServer = False;
    Globals.bBindInterfacesOnly = False;
    Globals.bUnixPasswdSync = False;
    Globals.bPasswdChatDebug = False;
    Globals.bOleLockingCompat = True;
    Globals.bNTSmbSupport = True;       /* Do NT SMB's by default. */
    Globals.bNTPipeSupport = True;      /* Do NT pipes by default. */
    Globals.bNTAclSupport = True;       /* Use NT ACLs by default. */
    Globals.bStatCache = True;  /* use stat cache by default */
    Globals.bRestrictAnonymous = False;
    Globals.map_to_guest = 0;   /* By Default, "Never" */
    Globals.min_passwd_length = MINPASSWDLENGTH;        /* By Default, 5. */
    Globals.oplock_break_wait_time = 10;        /* By Default, 10 msecs. */

#ifdef WITH_LDAP
    /* default values for ldap */
    string_set (&Globals.szLdapServer, "localhost");
    Globals.ldap_port = 389;
#endif /* WITH_LDAP */

#ifdef WITH_SSL
    Globals.sslVersion = SMB_SSL_V23;
    string_set (&Globals.sslHostsRequire, "");
    string_set (&Globals.sslHostsResign, "");
    string_set (&Globals.sslCaCertDir, "");
    string_set (&Globals.sslCaCertFile, "");
    string_set (&Globals.sslCert, "");
    string_set (&Globals.sslPrivKey, "");
    string_set (&Globals.sslClientCert, "");
    string_set (&Globals.sslClientPrivKey, "");
    string_set (&Globals.sslCiphers, "");
    Globals.sslEnabled = False;
    Globals.sslReqClientCert = False;
    Globals.sslReqServerCert = False;
    Globals.sslCompatibility = False;
#endif /* WITH_SSL */

    /* these parameters are set to defaults that are more appropriate
       for the increasing samba install base:

       as a member of the workgroup, that will possibly become a
       _local_ master browser (lm = True).  this is opposed to a forced
       local master browser startup (pm = True).

       doesn't provide WINS server service by default (wsupp = False),
       and doesn't provide domain master browser services by default, either.

     */

    Globals.bPreferredMaster = False;
    Globals.bLocalMaster = True;
    Globals.bDomainMaster = False;
    Globals.bDomainLogons = False;
    Globals.bBrowseList = True;
    Globals.bWINSsupport = False;
    Globals.bWINSproxy = False;

    Globals.bDNSproxy = True;

    /*
     * smbd will check at runtime to see if this value
     * will really be used or not.
     */
    Globals.bKernelOplocks = True;

    Globals.bAllowTrustedDomains = True;

    /*
     * This must be done last as it checks the value in 
     * client_code_page.
     */

    interpret_coding_system (KANJI);
}

#if 0
/***************************************************************************
check if a string is initialised and if not then initialise it
***************************************************************************/
static void
string_initial (char **s, char *v)
{
    if (!*s || !**s)
        string_init (s, v);
}

#else
#define init_locals()
#endif /* 0 */

static size_t
size_max (size_t a, size_t b)
{
    return (a > b) ? a : b;
}

/******************************************************************* a
convenience routine to grab string parameters into a rotating buffer,
and run standard_sub_basic on them. The buffers can be written to by
callers without affecting the source string.
********************************************************************/
static char *
lp_string (const char *s)
{
    static char *bufs[10];
    static int buflen[10];
    static int next = -1;
    char *ret;
    int i;
    int len = s ? strlen (s) : 0;

    if (next == -1)
    {
        /* initialisation */
        for (i = 0; i < 10; i++)
        {
            bufs[i] = NULL;
            buflen[i] = 0;
        }
        next = 0;
    }

    len = size_max (len + 100, sizeof (pstring));       /* the +100 is for some
                                                           substitution room */

    if (buflen[next] != len)
    {
        buflen[next] = len;
        if (bufs[next])
            free (bufs[next]);
        bufs[next] = (char *) malloc (len);
        if (!bufs[next])
        {
            DEBUG (0, ("out of memory in lp_string()"));
            exit (1);
        }
    }

    ret = &bufs[next][0];
    next = (next + 1) % 10;

    if (!s)
        *ret = 0;
    else
        StrCpy (ret, s);

    trim_string (ret, "\"", "\"");

    standard_sub_basic (ret);
    return (ret);
}


/*
   In this section all the functions that are used to access the 
   parameters from the rest of the program are defined 
 */

#define FN_GLOBAL_STRING(fn_name,ptr) \
 char *fn_name(void) {return(lp_string(*(char **)(ptr) ? *(char **)(ptr) : ""));}
#define FN_GLOBAL_BOOL(fn_name,ptr) \
 BOOL fn_name(void) {return(*(BOOL *)(ptr));}
#define FN_GLOBAL_CHAR(fn_name,ptr) \
 char fn_name(void) {return(*(char *)(ptr));}
#define FN_GLOBAL_INTEGER(fn_name,ptr) \
 int fn_name(void) {return(*(int *)(ptr));}

#define FN_LOCAL_STRING(fn_name,val) \
 char *fn_name(int i) {return(lp_string((LP_SNUM_OK(i)&&pSERVICE(i)->val)?pSERVICE(i)->val : sDefault.val));}
#define FN_LOCAL_BOOL(fn_name,val) \
 BOOL fn_name(int i) {return(LP_SNUM_OK(i)? pSERVICE(i)->val : sDefault.val);}
#define FN_LOCAL_CHAR(fn_name,val) \
 char fn_name(int i) {return(LP_SNUM_OK(i)? pSERVICE(i)->val : sDefault.val);}
#define FN_LOCAL_INTEGER(fn_name,val) \
 int fn_name(int i) {return(LP_SNUM_OK(i)? pSERVICE(i)->val : sDefault.val);}

FN_GLOBAL_STRING (lp_logfile, &Globals.szLogFile)
FN_GLOBAL_STRING (lp_configfile, &Globals.szConfigFile)
FN_GLOBAL_STRING (lp_serverstring, &Globals.szServerString)
FN_GLOBAL_STRING (lp_printcapname, &Globals.szPrintcapname)
FN_GLOBAL_STRING (lp_lockdir, &Globals.szLockDir)
FN_GLOBAL_STRING (lp_rootdir, &Globals.szRootdir)
FN_GLOBAL_STRING (lp_defaultservice, &Globals.szDefaultService)
FN_GLOBAL_STRING (lp_msg_command, &Globals.szMsgCommand)
FN_GLOBAL_STRING (lp_hosts_equiv, &Globals.szHostsEquiv)
FN_GLOBAL_STRING (lp_auto_services, &Globals.szAutoServices)
FN_GLOBAL_STRING (lp_passwordserver, &Globals.szPasswordServer)
FN_GLOBAL_STRING (lp_name_resolve_order, &Globals.szNameResolveOrder)
FN_GLOBAL_STRING (lp_workgroup, &Globals.szWorkGroup)
FN_GLOBAL_STRING (lp_username_map, &Globals.szUsernameMap)
#ifdef USING_GROUPNAME_MAP
    FN_GLOBAL_STRING (lp_groupname_map, &Globals.szGroupnameMap)
#endif /* USING_GROUPNAME_MAP */
    FN_GLOBAL_STRING (lp_remote_announce, &Globals.szRemoteAnnounce)
FN_GLOBAL_STRING (lp_remote_browse_sync, &Globals.szRemoteBrowseSync)
FN_GLOBAL_STRING (lp_wins_server, &Globals.szWINSserver)
FN_GLOBAL_STRING (lp_interfaces, &Globals.szInterfaces)
FN_GLOBAL_STRING (lp_socket_address, &Globals.szSocketAddress)
FN_GLOBAL_STRING (lp_nis_home_map_name, &Globals.szNISHomeMapName)
#if 0
     static FN_GLOBAL_STRING (lp_announce_version, &Globals.szAnnounceVersion)
#endif                          /* 0 */
FN_GLOBAL_STRING (lp_netbios_aliases, &Globals.szNetbiosAliases)
FN_GLOBAL_STRING (lp_driverfile, &Globals.szDriverFile)
FN_GLOBAL_STRING (lp_panic_action, &Globals.szPanicAction)
FN_GLOBAL_STRING (lp_domain_groups, &Globals.szDomainGroups)
FN_GLOBAL_STRING (lp_domain_admin_group, &Globals.szDomainAdminGroup)
FN_GLOBAL_STRING (lp_domain_guest_group, &Globals.szDomainGuestGroup)
FN_GLOBAL_STRING (lp_domain_admin_users, &Globals.szDomainAdminUsers)
FN_GLOBAL_STRING (lp_domain_guest_users, &Globals.szDomainGuestUsers)
#ifdef WITH_LDAP
FN_GLOBAL_STRING (lp_ldap_server, &Globals.szLdapServer);
FN_GLOBAL_STRING (lp_ldap_suffix, &Globals.szLdapSuffix);
FN_GLOBAL_STRING (lp_ldap_filter, &Globals.szLdapFilter);
FN_GLOBAL_STRING (lp_ldap_root, &Globals.szLdapRoot);
FN_GLOBAL_STRING (lp_ldap_rootpasswd, &Globals.szLdapRootPassword);
#endif /* WITH_LDAP */

#ifdef WITH_SSL
FN_GLOBAL_INTEGER (lp_ssl_version, &Globals.sslVersion);
FN_GLOBAL_STRING (lp_ssl_hosts, &Globals.sslHostsRequire);
FN_GLOBAL_STRING (lp_ssl_hosts_resign, &Globals.sslHostsResign);
FN_GLOBAL_STRING (lp_ssl_cacertdir, &Globals.sslCaCertDir);
FN_GLOBAL_STRING (lp_ssl_cacertfile, &Globals.sslCaCertFile);
FN_GLOBAL_STRING (lp_ssl_cert, &Globals.sslCert);
FN_GLOBAL_STRING (lp_ssl_privkey, &Globals.sslPrivKey);
FN_GLOBAL_STRING (lp_ssl_client_cert, &Globals.sslClientCert);
FN_GLOBAL_STRING (lp_ssl_client_privkey, &Globals.sslClientPrivKey);
FN_GLOBAL_STRING (lp_ssl_ciphers, &Globals.sslCiphers);
FN_GLOBAL_BOOL (lp_ssl_enabled, &Globals.sslEnabled);
FN_GLOBAL_BOOL (lp_ssl_reqClientCert, &Globals.sslReqClientCert);
FN_GLOBAL_BOOL (lp_ssl_reqServerCert, &Globals.sslReqServerCert);
FN_GLOBAL_BOOL (lp_ssl_compatibility, &Globals.sslCompatibility);
#endif /* WITH_SSL */

FN_GLOBAL_BOOL (lp_dns_proxy, &Globals.bDNSproxy)
FN_GLOBAL_BOOL (lp_wins_support, &Globals.bWINSsupport)
FN_GLOBAL_BOOL (lp_we_are_a_wins_server, &Globals.bWINSsupport)
FN_GLOBAL_BOOL (lp_wins_proxy, &Globals.bWINSproxy)
FN_GLOBAL_BOOL (lp_local_master, &Globals.bLocalMaster)
FN_GLOBAL_BOOL (lp_domain_master, &Globals.bDomainMaster)
FN_GLOBAL_BOOL (lp_domain_logons, &Globals.bDomainLogons)
FN_GLOBAL_BOOL (lp_preferred_master, &Globals.bPreferredMaster)
FN_GLOBAL_BOOL (lp_use_rhosts, &Globals.bUseRhosts)
FN_GLOBAL_BOOL (lp_readprediction, &Globals.bReadPrediction)
FN_GLOBAL_BOOL (lp_readbmpx, &Globals.bReadbmpx)
FN_GLOBAL_BOOL (lp_readraw, &Globals.bReadRaw)
FN_GLOBAL_BOOL (lp_writeraw, &Globals.bWriteRaw)
FN_GLOBAL_BOOL (lp_null_passwords, &Globals.bNullPasswords)
FN_GLOBAL_BOOL (lp_strip_dot, &Globals.bStripDot)
FN_GLOBAL_BOOL (lp_encrypted_passwords, &Globals.bEncryptPasswords)
FN_GLOBAL_BOOL (lp_update_encrypted, &Globals.bUpdateEncrypt)
FN_GLOBAL_BOOL (lp_syslog_only, &Globals.bSyslogOnly)
FN_GLOBAL_BOOL (lp_timestamp_logs, &Globals.bTimestampLogs)
FN_GLOBAL_BOOL (lp_browse_list, &Globals.bBrowseList)
FN_GLOBAL_BOOL (lp_unix_realname, &Globals.bUnixRealname)
FN_GLOBAL_BOOL (lp_nis_home_map, &Globals.bNISHomeMap)
#if 0
     static FN_GLOBAL_BOOL (lp_time_server, &Globals.bTimeServer)
#endif                          /* 0 */
FN_GLOBAL_BOOL (lp_bind_interfaces_only, &Globals.bBindInterfacesOnly)
FN_GLOBAL_BOOL (lp_passwd_chat_debug, &Globals.bPasswdChatDebug)
FN_GLOBAL_BOOL (lp_ole_locking_compat, &Globals.bOleLockingCompat)
FN_GLOBAL_BOOL (lp_nt_smb_support, &Globals.bNTSmbSupport)
FN_GLOBAL_BOOL (lp_nt_pipe_support, &Globals.bNTPipeSupport)
FN_GLOBAL_BOOL (lp_nt_acl_support, &Globals.bNTAclSupport)
FN_GLOBAL_BOOL (lp_stat_cache, &Globals.bStatCache)
FN_GLOBAL_BOOL (lp_allow_trusted_domains, &Globals.bAllowTrustedDomains)
FN_GLOBAL_BOOL (lp_restrict_anonymous, &Globals.bRestrictAnonymous)
FN_GLOBAL_INTEGER (lp_os_level, &Globals.os_level)
FN_GLOBAL_INTEGER (lp_max_ttl, &Globals.max_ttl)
FN_GLOBAL_INTEGER (lp_max_wins_ttl, &Globals.max_wins_ttl)
FN_GLOBAL_INTEGER (lp_min_wins_ttl, &Globals.min_wins_ttl)
FN_GLOBAL_INTEGER (lp_maxxmit, &Globals.max_xmit)
FN_GLOBAL_INTEGER (lp_maxmux, &Globals.max_mux)
FN_GLOBAL_INTEGER (lp_passwordlevel, &Globals.pwordlevel)
FN_GLOBAL_INTEGER (lp_usernamelevel, &Globals.unamelevel)
FN_GLOBAL_INTEGER (lp_readsize, &Globals.ReadSize)
FN_GLOBAL_INTEGER (lp_deadtime, &Globals.deadtime)
FN_GLOBAL_INTEGER (lp_maxprotocol, &Globals.maxprotocol)
FN_GLOBAL_INTEGER (lp_security, &Globals.security)
FN_GLOBAL_INTEGER (lp_maxdisksize, &Globals.maxdisksize)
FN_GLOBAL_INTEGER (lp_client_code_page, &Globals.client_code_page)
#if 0
     static FN_GLOBAL_INTEGER (lp_announce_as, &Globals.announce_as)
#endif
FN_GLOBAL_INTEGER (lp_lm_announce, &Globals.lm_announce)
FN_GLOBAL_INTEGER (lp_lm_interval, &Globals.lm_interval)
FN_GLOBAL_INTEGER (lp_machine_password_timeout, &Globals.machine_password_timeout)
FN_GLOBAL_INTEGER (lp_change_notify_timeout, &Globals.change_notify_timeout)
FN_GLOBAL_INTEGER (lp_stat_cache_size, &Globals.stat_cache_size)
FN_GLOBAL_INTEGER (lp_map_to_guest, &Globals.map_to_guest)
FN_GLOBAL_INTEGER (lp_min_passwd_length, &Globals.min_passwd_length)
FN_GLOBAL_INTEGER (lp_oplock_break_wait_time, &Globals.oplock_break_wait_time)
#ifdef WITH_LDAP
FN_GLOBAL_INTEGER (lp_ldap_port, &Globals.ldap_port)
#endif                          /* WITH_LDAP */
FN_LOCAL_STRING (lp_preexec, szPreExec)
FN_LOCAL_STRING (lp_postexec, szPostExec)
FN_LOCAL_STRING (lp_rootpreexec, szRootPreExec)
FN_LOCAL_STRING (lp_rootpostexec, szRootPostExec)
FN_LOCAL_STRING (lp_servicename, szService)
FN_LOCAL_STRING (lp_pathname, szPath)
FN_LOCAL_STRING (lp_dontdescend, szDontdescend)
FN_LOCAL_STRING (lp_username, szUsername)
FN_LOCAL_STRING (lp_guestaccount, szGuestaccount)
FN_LOCAL_STRING (lp_invalid_users, szInvalidUsers)
FN_LOCAL_STRING (lp_valid_users, szValidUsers)
FN_LOCAL_STRING (lp_admin_users, szAdminUsers)
#if 0
FN_LOCAL_STRING (lp_printcommand, szPrintcommand)
FN_LOCAL_STRING (lp_lpqcommand, szLpqcommand)
FN_LOCAL_STRING (lp_lprmcommand, szLprmcommand)
FN_LOCAL_STRING (lp_lppausecommand, szLppausecommand)
FN_LOCAL_STRING (lp_lpresumecommand, szLpresumecommand)
FN_LOCAL_STRING (lp_queuepausecommand, szQueuepausecommand)
FN_LOCAL_STRING (lp_queueresumecommand, szQueueresumecommand)
FN_LOCAL_STRING (lp_printername, szPrintername)
FN_LOCAL_STRING (lp_printerdriver, szPrinterDriver)
#endif                          /* 0 */
FN_LOCAL_STRING (lp_hostsallow, szHostsallow)
FN_LOCAL_STRING (lp_hostsdeny, szHostsdeny)
FN_LOCAL_STRING (lp_magicscript, szMagicScript)
FN_LOCAL_STRING (lp_magicoutput, szMagicOutput)
FN_LOCAL_STRING (lp_comment, comment)
FN_LOCAL_STRING (lp_force_user, force_user)
FN_LOCAL_STRING (lp_force_group, force_group)
FN_LOCAL_STRING (lp_readlist, readlist)
FN_LOCAL_STRING (lp_writelist, writelist)
FN_LOCAL_STRING (lp_fstype, fstype)
#if 0
     static FN_LOCAL_STRING (lp_volume, volume)
#endif
FN_LOCAL_STRING (lp_mangled_map, szMangledMap)
FN_LOCAL_STRING (lp_veto_files, szVetoFiles)
FN_LOCAL_STRING (lp_hide_files, szHideFiles)
FN_LOCAL_STRING (lp_veto_oplocks, szVetoOplockFiles)
FN_LOCAL_BOOL (lp_revalidate, bRevalidate)
FN_LOCAL_BOOL (lp_casesensitive, bCaseSensitive)
FN_LOCAL_BOOL (lp_preservecase, bCasePreserve)
FN_LOCAL_BOOL (lp_shortpreservecase, bShortCasePreserve)
FN_LOCAL_BOOL (lp_casemangle, bCaseMangle)
FN_LOCAL_BOOL (lp_status, status)
FN_LOCAL_BOOL (lp_hide_dot_files, bHideDotFiles)
FN_LOCAL_BOOL (lp_browseable, bBrowseable)
FN_LOCAL_BOOL (lp_readonly, bRead_only)
FN_LOCAL_BOOL (lp_no_set_dir, bNo_set_dir)
FN_LOCAL_BOOL (lp_guest_ok, bGuest_ok)
FN_LOCAL_BOOL (lp_guest_only, bGuest_only)
FN_LOCAL_BOOL (lp_print_ok, bPrint_ok)
FN_LOCAL_BOOL (lp_postscript, bPostscript)
FN_LOCAL_BOOL (lp_map_hidden, bMap_hidden)
FN_LOCAL_BOOL (lp_map_archive, bMap_archive)
FN_LOCAL_BOOL (lp_locking, bLocking)
FN_LOCAL_BOOL (lp_strict_locking, bStrictLocking)
FN_LOCAL_BOOL (lp_share_modes, bShareModes)
FN_LOCAL_BOOL (lp_oplocks, bOpLocks)
FN_LOCAL_BOOL (lp_onlyuser, bOnlyUser)
FN_LOCAL_BOOL (lp_manglednames, bMangledNames)
FN_LOCAL_BOOL (lp_widelinks, bWidelinks)
FN_LOCAL_BOOL (lp_symlinks, bSymlinks)
FN_LOCAL_BOOL (lp_syncalways, bSyncAlways)
FN_LOCAL_BOOL (lp_strict_sync, bStrictSync)
FN_LOCAL_BOOL (lp_map_system, bMap_system)
FN_LOCAL_BOOL (lp_delete_readonly, bDeleteReadonly)
FN_LOCAL_BOOL (lp_fake_oplocks, bFakeOplocks)
FN_LOCAL_BOOL (lp_recursive_veto_delete, bDeleteVetoFiles)
FN_LOCAL_BOOL (lp_dos_filetimes, bDosFiletimes)
FN_LOCAL_BOOL (lp_dos_filetime_resolution, bDosFiletimeResolution)
FN_LOCAL_BOOL (lp_fake_dir_create_times, bFakeDirCreateTimes)
FN_LOCAL_BOOL (lp_blocking_locks, bBlockingLocks)
FN_LOCAL_BOOL (lp_mangle_locks, bMangleLocks)
FN_LOCAL_INTEGER (lp_create_mode, iCreate_mask)
FN_LOCAL_INTEGER (lp_force_create_mode, iCreate_force_mode)
FN_LOCAL_INTEGER (lp_dir_mode, iDir_mask)
FN_LOCAL_INTEGER (lp_force_dir_mode, iDir_force_mode)
FN_LOCAL_INTEGER (lp_max_connections, iMaxConnections)
FN_LOCAL_INTEGER (lp_defaultcase, iDefaultCase)
FN_LOCAL_INTEGER (lp_minprintspace, iMinPrintSpace)
FN_LOCAL_INTEGER (lp_printing, iPrinting)
FN_LOCAL_INTEGER (lp_oplock_contention_limit, iOplockContentionLimit)
FN_LOCAL_CHAR (lp_magicchar, magic_char)
/* local prototypes */
     static int strwicmp (const char *psz1, const char *psz2);
     static int map_parameter (const char *pszParmName);
     static BOOL set_boolean (BOOL * pb, const char *pszParmValue);
     static int getservicebyname (const char *pszServiceName, service * pserviceDest);
     static void copy_service (service * pserviceDest,
                               service * pserviceSource, BOOL * pcopymapDest);
     static BOOL service_ok (int iService);
     static BOOL do_parameter (const char *pszParmName, const char *pszParmValue);
     static BOOL do_section (const char *pszSectionName);
     static void init_copymap (service * pservice);


/***************************************************************************
initialise a service to the defaults
***************************************************************************/
     static void init_service (service * pservice)
{
    memset ((char *) pservice, '\0', sizeof (service));
    copy_service (pservice, &sDefault, NULL);
}


/***************************************************************************
free the dynamically allocated parts of a service struct
***************************************************************************/
static void
free_service (service * pservice)
{
    int i;
    if (!pservice)
        return;

    if (pservice->szService)
        DEBUG (5, ("free_service: Freeing service %s\n", pservice->szService));

    string_free (&pservice->szService);
    if (pservice->copymap)
    {
        free (pservice->copymap);
        pservice->copymap = NULL;
    }

    for (i = 0; parm_table[i].label; i++)
        if ((parm_table[i].type == P_STRING ||
             parm_table[i].type == P_USTRING) && parm_table[i].class == P_LOCAL)
            string_free ((char **) (((char *) pservice) + PTR_DIFF (parm_table[i].ptr, &sDefault)));
}

/***************************************************************************
add a new service to the services array initialising it with the given 
service
***************************************************************************/
static int
add_a_service (service * pservice, const char *name)
{
    int i;
    service tservice;
    int num_to_alloc = iNumServices + 1;

    tservice = *pservice;

    /* it might already exist */
    if (name)
    {
        i = getservicebyname (name, NULL);
        if (i >= 0)
            return (i);
    }

    /* find an invalid one */
    for (i = 0; i < iNumServices; i++)
        if (!pSERVICE (i)->valid)
            break;

    /* if not, then create one */
    if (i == iNumServices)
    {
        ServicePtrs = (service **) Realloc (ServicePtrs, sizeof (service *) * num_to_alloc);
        if (ServicePtrs)
            pSERVICE (iNumServices) = (service *) malloc (sizeof (service));

        if (!ServicePtrs || !pSERVICE (iNumServices))
            return (-1);

        iNumServices++;
    }
    else
        free_service (pSERVICE (i));

    pSERVICE (i)->valid = True;

    init_service (pSERVICE (i));
    copy_service (pSERVICE (i), &tservice, NULL);
    if (name)
        string_set (&iSERVICE (i).szService, name);

    return (i);
}

/***************************************************************************
add a new home service, with the specified home directory, defaults coming 
from service ifrom
***************************************************************************/
BOOL
lp_add_home (const char *pszHomename, int iDefaultService, const char *pszHomedir)
{
    int i = add_a_service (pSERVICE (iDefaultService), pszHomename);

    if (i < 0)
        return (False);

    if (!(*(iSERVICE (i).szPath)) || strequal (iSERVICE (i).szPath, lp_pathname (-1)))
        string_set (&iSERVICE (i).szPath, pszHomedir);
    if (!(*(iSERVICE (i).comment)))
    {
        pstring comment;
        slprintf (comment, sizeof (comment) - 1, "Home directory of %s", pszHomename);
        string_set (&iSERVICE (i).comment, comment);
    }
    iSERVICE (i).bAvailable = sDefault.bAvailable;
    iSERVICE (i).bBrowseable = sDefault.bBrowseable;

    DEBUG (3, ("adding home directory %s at %s\n", pszHomename, pszHomedir));

    return (True);
}

/***************************************************************************
add a new service, based on an old one
***************************************************************************/
int
lp_add_service (char *pszService, int iDefaultService)
{
    return (add_a_service (pSERVICE (iDefaultService), pszService));
}

#if 0
/***************************************************************************
add the IPC service
***************************************************************************/
static BOOL
lp_add_ipc (void)
{
    pstring comment;
    int i = add_a_service (&sDefault, "IPC$");

    if (i < 0)
        return (False);

    slprintf (comment, sizeof (comment) - 1, "IPC Service (%s)", Globals.szServerString);

    string_set (&iSERVICE (i).szPath, tmpdir ());
    string_set (&iSERVICE (i).szUsername, "");
    string_set (&iSERVICE (i).comment, comment);
    string_set (&iSERVICE (i).fstype, "IPC");
    iSERVICE (i).status = False;
    iSERVICE (i).iMaxConnections = 0;
    iSERVICE (i).bAvailable = True;
    iSERVICE (i).bRead_only = True;
    iSERVICE (i).bGuest_only = False;
    iSERVICE (i).bGuest_ok = True;
    iSERVICE (i).bPrint_ok = False;
    iSERVICE (i).bBrowseable = sDefault.bBrowseable;

    DEBUG (3, ("adding IPC service\n"));

    return (True);
}
#endif /* 0 */

/***************************************************************************
Do a case-insensitive, whitespace-ignoring string compare.
***************************************************************************/
static int
strwicmp (const char *psz1, const char *psz2)
{
    /* if BOTH strings are NULL, return TRUE, if ONE is NULL return */
    /* appropriate value. */
    if (psz1 == psz2)
        return (0);
    else if (psz1 == NULL)
        return (-1);
    else if (psz2 == NULL)
        return (1);

    /* sync the strings on first non-whitespace */
    while (1)
    {
        while (isspace ((unsigned char) *psz1))
            psz1++;
        while (isspace ((unsigned char) *psz2))
            psz2++;
        if (toupper ((unsigned char) *psz1) != toupper ((unsigned char) *psz2) || *psz1 == '\0'
            || *psz2 == '\0')
            break;
        psz1++;
        psz2++;
    }
    return (*psz1 - *psz2);
}

/***************************************************************************
Map a parameter's string representation to something we can use. 
Returns False if the parameter string is not recognised, else TRUE.
***************************************************************************/
static int
map_parameter (const char *pszParmName)
{
    int iIndex;

    if (*pszParmName == '-')
        return (-1);

    for (iIndex = 0; parm_table[iIndex].label; iIndex++)
        if (strwicmp (parm_table[iIndex].label, pszParmName) == 0)
            return (iIndex);

    DEBUG (0, ("Unknown parameter encountered: \"%s\"\n", pszParmName));
    return (-1);
}


/***************************************************************************
Set a boolean variable from the text value stored in the passed string.
Returns True in success, False if the passed string does not correctly 
represent a boolean.
***************************************************************************/
static BOOL
set_boolean (BOOL * pb, const char *pszParmValue)
{
    BOOL bRetval;

    bRetval = True;
    if (strwicmp (pszParmValue, "yes") == 0 ||
        strwicmp (pszParmValue, "true") == 0 || strwicmp (pszParmValue, "1") == 0)
        *pb = True;
    else if (strwicmp (pszParmValue, "no") == 0 ||
             strwicmp (pszParmValue, "False") == 0 || strwicmp (pszParmValue, "0") == 0)
        *pb = False;
    else
    {
        DEBUG (0, ("ERROR: Badly formed boolean in configuration file: \"%s\".\n", pszParmValue));
        bRetval = False;
    }
    return (bRetval);
}

/***************************************************************************
Find a service by name. Otherwise works like get_service.
***************************************************************************/
static int
getservicebyname (const char *pszServiceName, service * pserviceDest)
{
    int iService;

    for (iService = iNumServices - 1; iService >= 0; iService--)
        if (VALID (iService) && strwicmp (iSERVICE (iService).szService, pszServiceName) == 0)
        {
            if (pserviceDest != NULL)
                copy_service (pserviceDest, pSERVICE (iService), NULL);
            break;
        }

    return (iService);
}



/***************************************************************************
Copy a service structure to another

If pcopymapDest is NULL then copy all fields
***************************************************************************/
static void
copy_service (service * pserviceDest, service * pserviceSource, BOOL * pcopymapDest)
{
    int i;
    BOOL bcopyall = (pcopymapDest == NULL);

    for (i = 0; parm_table[i].label; i++)
        if (parm_table[i].ptr && parm_table[i].class == P_LOCAL && (bcopyall || pcopymapDest[i]))
        {
            void *def_ptr = parm_table[i].ptr;
            void *src_ptr = ((char *) pserviceSource) + PTR_DIFF (def_ptr, &sDefault);
            void *dest_ptr = ((char *) pserviceDest) + PTR_DIFF (def_ptr, &sDefault);

            switch (parm_table[i].type)
            {
            case P_BOOL:
            case P_BOOLREV:
                *(BOOL *) dest_ptr = *(BOOL *) src_ptr;
                break;

            case P_INTEGER:
            case P_ENUM:
            case P_OCTAL:
                *(int *) dest_ptr = *(int *) src_ptr;
                break;

            case P_CHAR:
                *(char *) dest_ptr = *(char *) src_ptr;
                break;

            case P_STRING:
                string_set (dest_ptr, *(char **) src_ptr);
                break;

            case P_USTRING:
                string_set (dest_ptr, *(char **) src_ptr);
                strupper (*(char **) dest_ptr);
                break;
            default:
                break;
            }
        }

    if (bcopyall)
    {
        init_copymap (pserviceDest);
        if (pserviceSource->copymap)
            memcpy ((void *) pserviceDest->copymap,
                    (void *) pserviceSource->copymap, sizeof (BOOL) * NUMPARAMETERS);
    }
}

/***************************************************************************
Check a service for consistency. Return False if the service is in any way
incomplete or faulty, else True.
***************************************************************************/
static BOOL
service_ok (int iService)
{
    BOOL bRetval;

    bRetval = True;
    if (iSERVICE (iService).szService[0] == '\0')
    {
        DEBUG (0, ("The following message indicates an internal error:\n"));
        DEBUG (0, ("No service name in service entry.\n"));
        bRetval = False;
    }

    /* The [printers] entry MUST be printable. I'm all for flexibility, but */
    /* I can't see why you'd want a non-printable printer service...        */
    if (strwicmp (iSERVICE (iService).szService, PRINTERS_NAME) == 0)
        if (!iSERVICE (iService).bPrint_ok)
        {
            DEBUG (0, ("WARNING: [%s] service MUST be printable!\n",
                       iSERVICE (iService).szService));
            iSERVICE (iService).bPrint_ok = True;
        }

    if (iSERVICE (iService).szPath[0] == '\0' &&
        strwicmp (iSERVICE (iService).szService, HOMES_NAME) != 0)
    {
        DEBUG (0, ("No path in service %s - using %s\n", iSERVICE (iService).szService, tmpdir ()));
        string_set (&iSERVICE (iService).szPath, tmpdir ());
    }

    /* If a service is flagged unavailable, log the fact at level 0. */
    if (!iSERVICE (iService).bAvailable)
        DEBUG (1, ("NOTE: Service %s is flagged unavailable.\n", iSERVICE (iService).szService));

    return (bRetval);
}

#if 0
static struct file_lists
{
    struct file_lists *next;
    char *name;
    time_t modtime;
} *file_lists = NULL;

/*******************************************************************
keep a linked list of all config files so we know when one has changed 
it's date and needs to be reloaded
********************************************************************/
static void
add_to_file_list (const char *fname)
{
    struct file_lists *f = file_lists;

    while (f)
    {
        if (f->name && !strcmp (f->name, fname))
            break;
        f = f->next;
    }

    if (!f)
    {
        f = (struct file_lists *) malloc (sizeof (file_lists[0]));
        if (!f)
            return;
        f->next = file_lists;
        f->name = strdup (fname);
        if (!f->name)
        {
            free (f);
            return;
        }
        file_lists = f;
    }

    {
        pstring n2;
        pstrcpy (n2, fname);
        standard_sub_basic (n2);
        f->modtime = file_modtime (n2);
    }

}


/*******************************************************************
check if a config file has changed date
********************************************************************/
BOOL
lp_file_list_changed (void)
{
    struct file_lists *f = file_lists;
    DEBUG (6, ("lp_file_list_changed()\n"));

    while (f)
    {
        pstring n2;
        time_t mod_time;

        pstrcpy (n2, f->name);
        standard_sub_basic (n2);

        DEBUGADD (6, ("file %s -> %s  last mod_time: %s\n", f->name, n2, ctime (&f->modtime)));

        mod_time = file_modtime (n2);

        if (f->modtime != mod_time)
        {
            DEBUGADD (6, ("file %s modified: %s\n", n2, ctime (&mod_time)));
            f->modtime = mod_time;
            return (True);
        }
        f = f->next;
    }
    return (False);
}
#else
#define add_to_file_list(x)
#endif /* 0 */

/***************************************************************************
  handle the interpretation of the coding system parameter
  *************************************************************************/
static BOOL
handle_coding_system (const char *pszParmValue, char **ptr)
{
    string_set (ptr, pszParmValue);
    interpret_coding_system (pszParmValue);
    return (True);
}

/***************************************************************************
handle the interpretation of the character set system parameter
***************************************************************************/
static BOOL
handle_character_set (const char *pszParmValue, char **ptr)
{
    string_set (ptr, pszParmValue);
    interpret_character_set (pszParmValue);
    return (True);
}


/***************************************************************************
handle the valid chars lines
***************************************************************************/
static BOOL
handle_valid_chars (const char *pszParmValue, char **ptr)
{
    string_set (ptr, pszParmValue);

    /* A dependency here is that the parameter client code page must be
       set before this is called - as calling codepage_initialise()
       would overwrite the valid char lines.
     */
    codepage_initialise (lp_client_code_page ());

    add_char_string (pszParmValue);
    return (True);
}


/***************************************************************************
handle the include operation
***************************************************************************/
static BOOL
handle_include (const char *pszParmValue, char **ptr)
{
    pstring fname;
    pstrcpy (fname, pszParmValue);

    add_to_file_list (fname);

    standard_sub_basic (fname);

    string_set (ptr, fname);

    if (file_exist (fname, NULL))
        return (pm_process (fname, do_section, do_parameter));

    DEBUG (2, ("Cannot find include file %s\n", fname));

    return (False);
}


/***************************************************************************
handle the interpretation of the copy parameter
***************************************************************************/
static BOOL
handle_copy (const char *pszParmValue, char **ptr)
{
    BOOL bRetval;
    int iTemp;
    service serviceTemp;

    string_set (ptr, pszParmValue);

    init_service (&serviceTemp);

    bRetval = False;

    DEBUG (3, ("Copying service from service %s\n", pszParmValue));

    if ((iTemp = getservicebyname (pszParmValue, &serviceTemp)) >= 0)
    {
        if (iTemp == iServiceIndex)
        {
            DEBUG (0, ("Cannot copy service %s - unable to copy self!\n", pszParmValue));
        }
        else
        {
            copy_service (pSERVICE (iServiceIndex), &serviceTemp, iSERVICE (iServiceIndex).copymap);
            bRetval = True;
        }
    }
    else
    {
        DEBUG (0, ("Unable to copy service - source not found: %s\n", pszParmValue));
        bRetval = False;
    }

    free_service (&serviceTemp);
    return (bRetval);
}


/***************************************************************************
initialise a copymap
***************************************************************************/
static void
init_copymap (service * pservice)
{
    size_t i;
    if (pservice->copymap)
        free (pservice->copymap);
    pservice->copymap = (BOOL *) malloc (sizeof (BOOL) * NUMPARAMETERS);
    if (!pservice->copymap)
        DEBUG (0, ("Couldn't allocate copymap!! (size %d)\n", (int) NUMPARAMETERS));
    else
        for (i = 0; i < NUMPARAMETERS; i++)
            pservice->copymap[i] = True;
}

#if 0
/***************************************************************************
 return the local pointer to a parameter given the service number and the 
 pointer into the default structure
***************************************************************************/
void *
lp_local_ptr (int snum, void *ptr)
{
    return (void *) (((char *) pSERVICE (snum)) + PTR_DIFF (ptr, &sDefault));
}
#endif /* 0 */

/***************************************************************************
Process a parameter for a particular service number. If snum < 0
then assume we are in the globals
***************************************************************************/
BOOL
lp_do_parameter (int snum, const char *pszParmName, const char *pszParmValue)
{
    int parmnum, i;
    void *parm_ptr = NULL;      /* where we are going to store the result */
    void *def_ptr = NULL;

    parmnum = map_parameter (pszParmName);

    if (parmnum < 0)
    {
        DEBUG (0, ("Ignoring unknown parameter \"%s\"\n", pszParmName));
        return (True);
    }

    if (parm_table[parmnum].flags & FLAG_DEPRECATED)
    {
        DEBUG (1, ("WARNING: The \"%s\"option is deprecated\n", pszParmName));
    }

    def_ptr = parm_table[parmnum].ptr;

    /* we might point at a service, the default service or a global */
    if (snum < 0)
    {
        parm_ptr = def_ptr;
    }
    else
    {
        if (parm_table[parmnum].class == P_GLOBAL)
        {
            DEBUG (0, ("Global parameter %s found in service section!\n", pszParmName));
            return (True);
        }
        parm_ptr = ((char *) pSERVICE (snum)) + PTR_DIFF (def_ptr, &sDefault);
    }

    if (snum >= 0)
    {
        if (!iSERVICE (snum).copymap)
            init_copymap (pSERVICE (snum));

        /* this handles the aliases - set the copymap for other entries with
           the same data pointer */
        for (i = 0; parm_table[i].label; i++)
            if (parm_table[i].ptr == parm_table[parmnum].ptr)
                iSERVICE (snum).copymap[i] = False;
    }

    /* if it is a special case then go ahead */
    if (parm_table[parmnum].special)
    {
        parm_table[parmnum].special (pszParmValue, (char **) parm_ptr);
        return (True);
    }

    /* now switch on the type of variable it is */
    switch (parm_table[parmnum].type)
    {
    case P_BOOL:
        set_boolean (parm_ptr, pszParmValue);
        break;

    case P_BOOLREV:
        set_boolean (parm_ptr, pszParmValue);
        *(BOOL *) parm_ptr = !*(BOOL *) parm_ptr;
        break;

    case P_INTEGER:
        *(int *) parm_ptr = atoi (pszParmValue);
        break;

    case P_CHAR:
        *(char *) parm_ptr = *pszParmValue;
        break;

    case P_OCTAL:
        sscanf (pszParmValue, "%o", (int *) parm_ptr);
        break;

    case P_STRING:
        string_set (parm_ptr, pszParmValue);
        break;

    case P_USTRING:
        string_set (parm_ptr, pszParmValue);
        strupper (*(char **) parm_ptr);
        break;

    case P_GSTRING:
        pstrcpy ((char *) parm_ptr, pszParmValue);
        break;

    case P_UGSTRING:
        pstrcpy ((char *) parm_ptr, pszParmValue);
        strupper ((char *) parm_ptr);
        break;

    case P_ENUM:
        for (i = 0; parm_table[parmnum].enum_list[i].name; i++)
        {
            if (strequal (pszParmValue, parm_table[parmnum].enum_list[i].name))
            {
                *(int *) parm_ptr = parm_table[parmnum].enum_list[i].value;
                break;
            }
        }
        break;
    case P_SEP:
        break;
    }

    return (True);
}

/***************************************************************************
Process a parameter.
***************************************************************************/
static BOOL
do_parameter (const char *pszParmName, const char *pszParmValue)
{
    if (!bInGlobalSection && bGlobalOnly)
        return (True);

    DEBUGADD (3, ("doing parameter %s = %s\n", pszParmName, pszParmValue));

    return (lp_do_parameter (bInGlobalSection ? -2 : iServiceIndex, pszParmName, pszParmValue));
}

#if 0
/***************************************************************************
check if two parameters are equal
***************************************************************************/
static BOOL
equal_parameter (parm_type type, void *ptr1, void *ptr2)
{
    switch (type)
    {
    case P_BOOL:
    case P_BOOLREV:
        return (*((BOOL *) ptr1) == *((BOOL *) ptr2));

    case P_INTEGER:
    case P_ENUM:
    case P_OCTAL:
        return (*((int *) ptr1) == *((int *) ptr2));

    case P_CHAR:
        return (*((char *) ptr1) == *((char *) ptr2));

    case P_GSTRING:
    case P_UGSTRING:
        {
            char *p1 = (char *) ptr1, *p2 = (char *) ptr2;
            if (p1 && !*p1)
                p1 = NULL;
            if (p2 && !*p2)
                p2 = NULL;
            return (p1 == p2 || strequal (p1, p2));
        }
    case P_STRING:
    case P_USTRING:
        {
            char *p1 = *(char **) ptr1, *p2 = *(char **) ptr2;
            if (p1 && !*p1)
                p1 = NULL;
            if (p2 && !*p2)
                p2 = NULL;
            return (p1 == p2 || strequal (p1, p2));
        }
    case P_SEP:
        break;
    }
    return (False);
}
#endif /* 0 */

/***************************************************************************
Process a new section (service). At this stage all sections are services.
Later we'll have special sections that permit server parameters to be set.
Returns True on success, False on failure.
***************************************************************************/
static BOOL
do_section (const char *pszSectionName)
{
    BOOL bRetval;
    BOOL isglobal = ((strwicmp (pszSectionName, GLOBAL_NAME) == 0) ||
                     (strwicmp (pszSectionName, GLOBAL_NAME2) == 0));
    bRetval = False;

    /* if we were in a global section then do the local inits */
    if (bInGlobalSection && !isglobal)
    {
        init_locals ();
    }

    /* if we've just struck a global section, note the fact. */
    bInGlobalSection = isglobal;

    /* check for multiple global sections */
    if (bInGlobalSection)
    {
        DEBUG (3, ("Processing section \"[%s]\"\n", pszSectionName));
        return (True);
    }

    if (!bInGlobalSection && bGlobalOnly)
        return (True);

    /* if we have a current service, tidy it up before moving on */
    bRetval = True;

    if (iServiceIndex >= 0)
        bRetval = service_ok (iServiceIndex);

    /* if all is still well, move to the next record in the services array */
    if (bRetval)
    {
        /* We put this here to avoid an odd message order if messages are */
        /* issued by the post-processing of a previous section. */
        DEBUG (2, ("Processing section \"[%s]\"\n", pszSectionName));

        if ((iServiceIndex = add_a_service (&sDefault, pszSectionName)) < 0)
        {
            DEBUG (0, ("Failed to add a new service\n"));
            return (False);
        }
    }

    return (bRetval);
}

#if 0
/***************************************************************************
return True if a local parameter is currently set to the global default
***************************************************************************/
BOOL
lp_is_default (int snum, struct parm_struct * parm)
{
    int pdiff = PTR_DIFF (parm->ptr, &sDefault);

    return equal_parameter (parm->type,
                            ((char *) pSERVICE (snum)) + pdiff, ((char *) &sDefault) + pdiff);
}
#endif /* 0 */
#if 0
/***************************************************************************
return info about the next service  in a service. snum==-1 gives the globals

return NULL when out of parameters
***************************************************************************/
struct parm_struct *
lp_next_parameter (int snum, int *i, int allparameters)
{
    if (snum == -1)
    {
        /* do the globals */
        for (; parm_table[*i].label; (*i)++)
        {
            if (parm_table[*i].class == P_SEPARATOR)
                return &parm_table[(*i)++];

            if (!parm_table[*i].ptr || (*parm_table[*i].label == '-'))
                continue;

            if ((*i) > 0 && (parm_table[*i].ptr == parm_table[(*i) - 1].ptr))
                continue;

            return &parm_table[(*i)++];
        }
    }
    else
    {
        service *pService = pSERVICE (snum);

        for (; parm_table[*i].label; (*i)++)
        {
            if (parm_table[*i].class == P_SEPARATOR)
                return &parm_table[(*i)++];

            if (parm_table[*i].class == P_LOCAL &&
                parm_table[*i].ptr &&
                (*parm_table[*i].label != '-') &&
                ((*i) == 0 || (parm_table[*i].ptr != parm_table[(*i) - 1].ptr)))
            {
                int pdiff = PTR_DIFF (parm_table[*i].ptr, &sDefault);

                if (allparameters ||
                    !equal_parameter (parm_table[*i].type,
                                      ((char *) pService) + pdiff, ((char *) &sDefault) + pdiff))
                {
                    return &parm_table[(*i)++];
                }
            }
        }
    }

    return NULL;
}
#endif /* 0 */
#if 0
/***************************************************************************
Return TRUE if the passed service number is within range.
***************************************************************************/
BOOL
lp_snum_ok (int iService)
{
    return (LP_SNUM_OK (iService) && iSERVICE (iService).bAvailable);
}
#endif /* 0 */

/***************************************************************************
auto-load some home services
***************************************************************************/
static void
lp_add_auto_services (char *str)
{
    char *s;
    char *p;
    int homes;

    if (!str)
        return;

    s = strdup (str);
    if (!s)
        return;

    homes = lp_servicenumber (HOMES_NAME);

    for (p = strtok (s, LIST_SEP); p; p = strtok (NULL, LIST_SEP))
    {
        const char *home = get_home_dir (p);

        if (lp_servicenumber (p) >= 0)
            continue;

        if (home && homes >= 0)
        {
            lp_add_home (p, homes, home);
        }
    }
    free (s);
}


/***************************************************************************
have we loaded a services file yet?
***************************************************************************/
BOOL
lp_loaded (void)
{
    return (bLoaded);
}

#if 0
/***************************************************************************
unload unused services
***************************************************************************/
void
lp_killunused (BOOL (*snumused) (int))
{
    int i;
    for (i = 0; i < iNumServices; i++)
        if (VALID (i) && (!snumused || !snumused (i)))
        {
            iSERVICE (i).valid = False;
            free_service (pSERVICE (i));
        }
}
#endif /* 0 */
#if 0
/***************************************************************************
save the current values of all global and sDefault parameters into the 
defaults union. This allows swat and testparm to show only the
changed (ie. non-default) parameters.
***************************************************************************/
static void
lp_save_defaults (void)
{
    static BOOL defaults_saved = False;
    int i;

    for (i = 0; parm_table[i].label; i++)
    {
        if (i > 0 && parm_table[i].ptr == parm_table[i - 1].ptr)
            continue;
        switch (parm_table[i].type)
        {
        case P_STRING:
        case P_USTRING:
            parm_table[i].def.svalue = strdup (*(char **) parm_table[i].ptr);
            break;
        case P_GSTRING:
        case P_UGSTRING:
            parm_table[i].def.svalue = strdup ((char *) parm_table[i].ptr);
            break;
        case P_BOOL:
        case P_BOOLREV:
            parm_table[i].def.bvalue = *(BOOL *) parm_table[i].ptr;
            break;
        case P_CHAR:
            parm_table[i].def.cvalue = *(char *) parm_table[i].ptr;
            break;
        case P_INTEGER:
        case P_OCTAL:
        case P_ENUM:
            parm_table[i].def.ivalue = *(int *) parm_table[i].ptr;
            break;
        case P_SEP:
            break;
        }
    }
    defaults_saved = True;
}
#endif /* 0 */

/***************************************************************************
Load the services array from the services file. Return True on success, 
False on failure.
***************************************************************************/
BOOL
lp_load (const char *pszFname, BOOL global_only, BOOL save_defaults, BOOL add_ipc)
{
    pstring n2;
    BOOL bRetval;

    add_to_file_list (pszFname);

    bRetval = False;

    bInGlobalSection = True;
    bGlobalOnly = global_only;

    init_globals ();
#if 0
    if (save_defaults)
    {
        init_locals ();
        lp_save_defaults ();
    }
#else
    (void) &save_defaults;
#endif /* 0 */
    pstrcpy (n2, pszFname);
    standard_sub_basic (n2);

    /* We get sections first, so have to start 'behind' to make up */
    iServiceIndex = -1;
    bRetval = pm_process (n2, do_section, do_parameter);

    /* finish up the last section */
    DEBUG (3, ("pm_process() returned %s\n", BOOLSTR (bRetval)));
    if (bRetval)
        if (iServiceIndex >= 0)
            bRetval = service_ok (iServiceIndex);

    lp_add_auto_services (lp_auto_services ());
#if 0
    if (add_ipc)
        lp_add_ipc ();
    set_default_server_announce_type ();
#else
    (void) &add_ipc;
#endif /* 0 */

    bLoaded = True;

    /* Now we check bWINSsupport and set szWINSserver to 127.0.0.1 */
    /* if bWINSsupport is true and we are in the client            */

    if (in_client && Globals.bWINSsupport)
    {

        string_set (&Globals.szWINSserver, "127.0.0.1");

    }

    return (bRetval);
}

#if 0
/***************************************************************************
reset the max number of services
***************************************************************************/
void
lp_resetnumservices (void)
{
    iNumServices = 0;
}


/***************************************************************************
return the max number of services
***************************************************************************/
int
lp_numservices (void)
{
    return (iNumServices);
}
#endif /* 0 */

/***************************************************************************
Return the number of the service with the given name, or -1 if it doesn't
exist. Note that this is a DIFFERENT ANIMAL from the internal function
getservicebyname()! This works ONLY if all services have been loaded, and
does not copy the found service.
***************************************************************************/
int
lp_servicenumber (const char *pszServiceName)
{
    int iService;

    for (iService = iNumServices - 1; iService >= 0; iService--)
        if (VALID (iService) && strequal (lp_servicename (iService), pszServiceName))
            break;

    if (iService < 0)
        DEBUG (7, ("lp_servicenumber: couldn't find %s\n", pszServiceName));

    return (iService);
}

#if 0
/*******************************************************************
  a useful volume label function
  ******************************************************************/
char *
volume_label (int snum)
{
    char *ret = lp_volume (snum);
    if (!*ret)
        return (lp_servicename (snum));
    return (ret);
}

/***********************************************************
 Set the global name resolution order (used in smbclient).
************************************************************/

void
lp_set_name_resolve_order (char *new_order)
{
    Globals.szNameResolveOrder = new_order;
}
#endif /* 0 */
