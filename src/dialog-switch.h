
#ifndef MC_DIALOG_SWITCH_H
#define MC_DIALOG_SWITCH_H

struct Dlg_head;

void dialog_switch_add (struct Dlg_head *h);
void dialog_switch_remove (struct Dlg_head *h);

void dialog_switch_next (void);
void dialog_switch_prev (void);

void dialog_switch_list (void);

int dialog_switch_process_pending (void);
void dialog_switch_shutdown (void);

#endif /* MC_DIALOG_SWITCH_H */
