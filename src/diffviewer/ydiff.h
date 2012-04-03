#ifndef MC__DIFFVIEW_YDIFF_H
#define MC__DIFFVIEW_YDIFF_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int dview_diff_cmd (const void *f0, const void *f1);
int diff_view (const char *file1, const char *file2, const char *label1, const char *label2);

#endif /* MC__DIFFVIEW_YDIFF_H */
