#ifndef __PROFILE_H
#define __PROFILE_H
/* Prototypes for the profile management functions */

short GetPrivateProfileString (const char * AppName, const char * KeyName,
			       const char * Default, char * ReturnedString,
			       short Size, const char * FileName);

int GetProfileString (const char * AppName, const char * KeyName, const char * Default, 
		      char * ReturnedString, int Size);

int GetPrivateProfileInt (const char * AppName, const char * KeyName, int Default,
			   const char * File);

int GetProfileInt (const char * AppName, const char * KeyName, int Default);

int WritePrivateProfileString (const char * AppName, const char * KeyName, const char * String,
				const char * FileName);

int WriteProfileString (const char * AppName, const char * KeyName, const char * String);

void sync_profiles (void);

void free_profiles (void);
const char *get_profile_string (const char *AppName, const char *KeyName, const char *Default,
			  const char *FileName);

/* New profile functions */

/* Returns a pointer for iterating on appname section, on profile file */
void *profile_init_iterator (const char *appname, const char *file);

/* Returns both the key and the value of the current section. */
/* You pass the current iterating pointer and it returns the new pointer */
void *profile_iterator_next (void *s, char **key, char **value);

/* Removes all the definitions from section appname on file */
void profile_clean_section (const char *appname, const char *file);
int profile_has_section (const char *section_name, const char *profile);

/* Forgets about a .ini file, to disable updating of it */
void profile_forget_profile (const char *file);

/* Removes information from a profile */
void free_profile_name (const char *s);

#endif	/* __PROFILE_H */
