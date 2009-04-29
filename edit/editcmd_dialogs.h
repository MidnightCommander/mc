#ifndef MC__EDITCMD_DIALOGS_H
#define MC__EDITCMD_DIALOGS_H

/*** typedefs(not structures) and defined constants **********************************************/

struct etags_hash_struct;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct selection {
    unsigned char *text;
    int len;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void editcmd_dialog_replace_show (WEdit *, const char *, const char *, char **, char **);

void editcmd_dialog_search_show (WEdit *, char **);

int editcmd_dialog_raw_key_query (const char *, const char *, int);

void editcmd_dialog_completion_show (WEdit *, int, int, struct selection *, int);

void editcmd_dialog_select_definition_show (WEdit *, char *, int, int, struct etags_hash_struct *,
                                            int);

#endif
