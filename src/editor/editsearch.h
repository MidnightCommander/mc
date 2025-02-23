#ifndef MC__EDIT_SEARCH_H
#define MC__EDIT_SEARCH_H 1

/*** typedefs(not structures) and defined constants **********************************************/

#define B_REPLACE_ALL  (B_USER + 1)
#define B_REPLACE_ONE  (B_USER + 2)
#define B_SKIP_REPLACE (B_USER + 3)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct edit_search_options_t
{
    mc_search_type_t type;
    gboolean case_sens;
    gboolean backwards;
    gboolean only_in_selection;
    gboolean whole_words;
    gboolean all_codepages;
} edit_search_options_t;

typedef struct
{
    simple_status_msg_t status_msg;  // base class

    gboolean first;
    WEdit *edit;
    off_t offset;
} edit_search_status_msg_t;

/*** global variables defined in .c file *********************************************************/

extern edit_search_options_t edit_search_options;

/*** declarations of public functions ************************************************************/

gboolean edit_search_init (WEdit *edit, const char *s);
void edit_search_deinit (WEdit *edit);

mc_search_cbret_t edit_search_cmd_callback (const void *user_data, off_t char_offset,
                                            int *current_char);
MC_MOCKABLE mc_search_cbret_t edit_search_update_callback (const void *user_data,
                                                           off_t char_offset);
int edit_search_status_update_cb (status_msg_t *sm);

void edit_search_cmd (WEdit *edit, gboolean again);
void edit_replace_cmd (WEdit *edit, gboolean again);

/*** inline functions ****************************************************************************/

#endif
