/* Convenience functions for metadata handling in the MIdnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GMETADATA_H
#define GMETADATA_H


int gmeta_get_icon_pos (char *filename, int *x, int *y);
void gmeta_set_icon_pos (char *filename, int x, int y);
void gmeta_del_icon_pos (char *filename);


#endif
