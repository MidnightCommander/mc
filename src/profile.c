/*
 * Initialization-File Functions.
 *
 * From the Wine project

   Copyright (C) 1993, 1994 Miguel de Icaza.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "global.h"
#include "profile.h"

#define STRSIZE 4096
#define overflow (next == &CharBuffer [STRSIZE-1])

enum { FirstBrace, OnSecHeader, IgnoreToEOL, KeyDef, KeyDefOnKey, KeyValue };

typedef struct TKeys {
    char *KeyName;
    char *Value;
    struct TKeys *link;
} TKeys;

typedef struct TSecHeader {
    char *AppName;
    TKeys *Keys;
    struct TSecHeader *link;
} TSecHeader;
    
typedef struct TProfile {
    char *FileName;
    TSecHeader *Section;
    struct TProfile *link;
} TProfile;

static TProfile *Base = 0;

static TProfile *
find_loaded (const char *FileName, TSecHeader ** section)
{
    TProfile *p = Base;

    while (p) {
	if (!g_strcasecmp (FileName, p->FileName)) {
	    *section = p->Section;
	    return p;
	}
	p = p->link;
    }
    return NULL;
}

#define TRANSLATION_CHAR '\200'

static char *
str_untranslate_newline_dup (char *s)
{
    int l = 0;
    char *p = s, *q;
    g_return_val_if_fail(s, NULL);
    while (*p) {
	l++;
	l += (*p == '\n' || *p == TRANSLATION_CHAR);
	p++;
    }
    q = p = g_malloc (l + 1);
    if (!q)
	return 0;
    for (;;) {
	switch (*s) {
	case '\n':
	    *p++ = TRANSLATION_CHAR;
	    *p++ = 'n';
	    break;
	case TRANSLATION_CHAR:
	    if (s[1] == 'n' || s[1] == TRANSLATION_CHAR)
		*p++ = TRANSLATION_CHAR;
	    *p++ = TRANSLATION_CHAR;
	    break;
	case '\0':
	    *p = '\0';
	    return q;
	    break;
	default:
	    *p++ = *s;
	}
	s++;
    }
    return 0;			/* not reached */
}

static char *
str_translate_newline_dup (char *s)
{
    char *p, *q;
    g_return_val_if_fail(s,NULL);
    q = p = g_malloc (strlen (s) + 1);
    if (!q)
	return 0;
    while (*s) {
	if (*s == TRANSLATION_CHAR) {
	    switch (*(++s)) {
	    case 'n':
		*p++ = '\n';
		break;
	    case TRANSLATION_CHAR:
		*p++ = TRANSLATION_CHAR;
		break;
	    case '\0':
		*p++ = TRANSLATION_CHAR;
		*p++ = '\0';
		return q;
	    default:
		*p++ = TRANSLATION_CHAR;
		*p++ = *s;
	    }
	} else {
	    *p++ = *s;
	}
	s++;
    }
    *p = '\0';
    return q;			/* not reached */
}

static TSecHeader *load (const char *file)
{
    FILE *f;
    int state;
    TSecHeader *SecHeader = 0;
    char CharBuffer [STRSIZE];
    char *next = NULL;		/* Not needed */
    int c;
    
    if ((f = fopen (file, "r"))==NULL)
	return NULL;

    state = FirstBrace;
    while ((c = getc (f)) != EOF){
	if (c == '\r')		/* Ignore Carriage Return */
	    continue;
	
	switch (state){

	case OnSecHeader:
	    if (c == ']' || overflow){
		*next = '\0';
		next = CharBuffer;
		SecHeader->AppName = g_strdup (CharBuffer);
		state = IgnoreToEOL;
	    } else
		*next++ = c;
	    break;

	case IgnoreToEOL:
	    if (c == '\n'){
		state = KeyDef;
		next = CharBuffer;
	    }
	    break;

	case FirstBrace:
	case KeyDef:
	case KeyDefOnKey:
	    if (c == '['){
		TSecHeader *temp;
		
		temp = SecHeader;
		SecHeader = g_new (TSecHeader, 1);
		SecHeader->link = temp;
		SecHeader->Keys = 0;
		state = OnSecHeader;
		next = CharBuffer;
		break;
	    }
	    if (state == FirstBrace) /* On first pass, don't allow dangling keys */
		break;
	    
	    if ((c == ' ' && state != KeyDefOnKey) || c == '\t')
		break;
	    
	    if (c == '\n' || overflow) {
		/* Abort Definition */
		next = CharBuffer;
		break;
	    }
	    
	    if (c == '=' || overflow){
		TKeys *temp;

		temp = SecHeader->Keys;
		*next = '\0';
		SecHeader->Keys =g_new (TKeys, 1);
		SecHeader->Keys->link = temp;
		SecHeader->Keys->KeyName = g_strdup (CharBuffer);
		state = KeyValue;
		next = CharBuffer;
	    } else {
		*next++ = c;
		state = KeyDefOnKey;
	    }
	    break;

	case KeyValue:
	    if (overflow || c == '\n'){
		*next = '\0';
		SecHeader->Keys->Value = str_translate_newline_dup (CharBuffer);
		state = c == '\n' ? KeyDef : IgnoreToEOL;
		next = CharBuffer;
#ifdef DEBUG
		printf ("[%s] (%s)=%s\n", SecHeader->AppName,
			SecHeader->Keys->KeyName, SecHeader->Keys->Value);
#endif
	    } else
		*next++ = c;
	    break;
	    
	} /* switch */
	
    } /* while ((c = getc (f)) != EOF) */

    switch (state) {
    case KeyValue:
	*next = '\0';
	SecHeader->Keys->Value = str_translate_newline_dup (CharBuffer);
	break;
    case OnSecHeader: {		/* Broken initialization file */
	    TSecHeader *link = SecHeader->link;
	    g_free (SecHeader);
	    SecHeader = link;
	    fprintf (stderr, "Warning: Corrupted initialization file `%s'\n",
		     file);
	    break;
	}
    }

    fclose (f);
    return SecHeader;
}

static void new_key (TSecHeader *section, const char *KeyName, const char *Value)
{
    TKeys *key;
    
    key = g_new (TKeys, 1);
    key->KeyName = g_strdup (KeyName);
    key->Value   = g_strdup (Value);
    key->link = section->Keys;
    section->Keys = key;
}

static char *
GetSetProfileChar (int set, const char *AppName, const char *KeyName,
		   const char *Default, const char *FileName)
{
    
    TProfile   *Current;
    TSecHeader *section;
    TKeys      *key;

    Current = find_loaded (FileName, &section);
    if (!Current) {
	Current = g_new (TProfile, 1);
	Current->link = Base;
	Current->FileName = g_strdup (FileName);
	Current->Section = load (FileName);
	Base = Current;
	section = Current->Section;
    }
    
    /* Start search */
    for (; section; section = section->link){
	if (section->AppName == 0 || g_strcasecmp (section->AppName, AppName))
	    continue;
	for (key = section->Keys; key; key = key->link){
	    if ( g_strcasecmp (key->KeyName, KeyName))
		continue;
	    if (set){
		g_free (key->Value);
		key->Value = g_strdup (Default);
	    }
	    return key->Value;
	}
	/* If getting the information, then don't write the information
	   to the INI file, need to run a couple of tests with windog */
	/* No key found */
	if (set){
	    new_key (section, KeyName, Default);
	    return 0;
	}
    }
    
    /* Non existent section */
    if (set && Default){
	section = g_new (TSecHeader, 1);
	section->AppName = g_strdup (AppName);
	section->Keys = 0;
	new_key (section, KeyName, Default);
	section->link = Current->Section;
	Current->Section = section;
    } 
    return Default;
}

static short GetSetProfile (int set, const char * AppName, const char * KeyName,
			    const char * Default, char * ReturnedString,
			    short Size, const char * FileName)

{
    char  *s;
    
    s = GetSetProfileChar (set, AppName, KeyName, Default, FileName);
    if (!set){
	ReturnedString [Size-1] = 0;
	g_strlcpy (ReturnedString, s, Size-1);
   }
    return 1;
}

short GetPrivateProfileString (const char * AppName, const char * KeyName,
			       const char * Default, char * ReturnedString,
			       short Size, const char * FileName)
{
    return (GetSetProfile (0, AppName, KeyName, Default, ReturnedString, Size, FileName));
}

char *get_profile_string (const char *AppName, const char *KeyName, const char *Default,
			  const char *FileName)
{
    return GetSetProfileChar (0, AppName, KeyName, Default, FileName);
}

int GetPrivateProfileInt (const char * AppName, const char * KeyName, int Default,
			  const char * File)
{
    char IntBuf [BUF_TINY];
    char buf [BUF_TINY];

    g_snprintf (buf, sizeof (buf), "%d", Default);
    
    /* Check the exact semantic with the SDK */
    GetPrivateProfileString (AppName, KeyName, buf, IntBuf, BUF_TINY, File);
    if (! g_strcasecmp (IntBuf, "true"))
	return 1;
    if (! g_strcasecmp (IntBuf, "yes"))
	return 1;
    return (int) atol (IntBuf);
}

int WritePrivateProfileString (const char * AppName, const char * KeyName, const char * String,
				const char * FileName)
{
    return GetSetProfile (1, AppName, KeyName, String, NULL, 0, FileName);
}

static void dump_keys (FILE * profile, TKeys * p)
{
    char *t;
    if (!p)
	return;
    dump_keys (profile, p->link);
    t = str_untranslate_newline_dup (p->Value);
    fprintf (profile, "%s=%s\n", p->KeyName, t);
    g_free (t);
}

static void dump_sections (FILE *profile, TSecHeader *p)
{
    if (!p)
	return;
    dump_sections (profile, p->link);
    if (p->AppName [0]){
	fprintf (profile, "\n[%s]\n", p->AppName);
	dump_keys (profile, p->Keys);
    }
}

static void dump_profile (TProfile *p)
{
    FILE *profile;
    
    if (!p)
	return;
    dump_profile (p->link);
    /* .ado: p->FileName can be empty, it's better to jump over */
    if (p->FileName[0] != (char) 0)
      if ((profile = fopen (p->FileName, "w")) != NULL){
	dump_sections (profile, p->Section);
	fclose (profile);
      }
}

/*
 * Must be called at the end of wine run
*/

void sync_profiles (void)
{
    dump_profile (Base);
}

static void free_keys (TKeys *p)
{
    if (!p)
	return;
    free_keys (p->link);
    g_free (p->KeyName);
    g_free (p->Value);
    g_free (p);
}

static void free_sections (TSecHeader *p)
{
    if (!p)
	return;
    free_sections (p->link);
    free_keys (p->Keys);
    g_free (p->AppName);
    p->link = 0;
    p->Keys = 0;
    g_free (p);
}

static void free_profile (TProfile *p)
{
    if (!p)
	return;
    free_profile (p->link);
    free_sections (p->Section);
    g_free (p->FileName);
    g_free (p);
}

void free_profile_name (const char *s)
{
    TProfile *p;
    
    if (!s)
	return;
    
    for (p = Base; p; p = p->link){
	if (strcmp (s, p->FileName) == 0){
	    free_sections (p->Section);
	    p->Section = 0;
	    p->FileName [0] = 0;
	    return;
	}
    }
}

void free_profiles (void)
{
    free_profile (Base);
}

void *profile_init_iterator (const char *appname, const char *file)
{
    TProfile   *Current;
    TSecHeader *section;

    Current = find_loaded (file, &section);
    if (!Current) {
	Current = g_new (TProfile, 1);
	Current->link = Base;
	Current->FileName = g_strdup (file);
	Current->Section = load (file);
	Base = Current;
	section = Current->Section;
    }
    for (; section; section = section->link){
	if ( g_strcasecmp (section->AppName, appname))
	    continue;
	return section->Keys;
    }
    return 0;
}

void *profile_iterator_next (void *s, char **key, char **value)
{
    TKeys *keys = (TKeys *) s;

    if (keys){
	*key   = keys->KeyName;
	*value = keys->Value;
	keys   = keys->link;
    }
    return keys;
}

void profile_clean_section (const char *appname, const char *file)
{
    TSecHeader *section;

    /* We assume the user has called one of the other initialization funcs */
    if (!find_loaded (file, &section)){
	fprintf (stderr,"Warning: profile_clean_section called before init\n");
	return;
    }
    /* We only disable the section, so it will still be freed, but it */
    /* won't be find by further walks of the structure */

    for (; section; section = section->link){
	if ( g_strcasecmp (section->AppName, appname))
	    continue;
	section->AppName [0] = 0;
    }
}

int profile_has_section (const char *section_name, const char *profile)
{
    TSecHeader *section;

    /* We assume the user has called one of the other initialization funcs */
    if (!find_loaded (profile, &section)){
	return 0;
    }
    for (; section; section = section->link){
	if ( g_strcasecmp (section->AppName, section_name))
	    continue;
	return 1;
    }
    return 0;
}

void profile_forget_profile (const char *file)
{
    TProfile *p;

    for (p = Base; p; p = p->link){
	if ( g_strcasecmp (file, p->FileName))
	    continue;
	p->FileName [0] = 0;
    }
}


