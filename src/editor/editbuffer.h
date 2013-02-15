/** \file
 *  \brief Header: text keep buffer for WEdit
 */

#ifndef MC__EDIT_BUFFER_H
#define MC__EDIT_BUFFER_H

/*** typedefs(not structures) and defined constants **********************************************/

/*
 * The editor keeps data in two arrays of buffers.
 * All buffers have the same size, which must be a power of 2.
 */

/* Configurable: log2 of the buffer size in bytes */
#ifndef S_EDIT_BUF_SIZE
#define S_EDIT_BUF_SIZE 16
#endif

/* Size of the buffer */
#define EDIT_BUF_SIZE (((off_t) 1) << S_EDIT_BUF_SIZE)

/* Buffer mask (used to find cursor position relative to the buffer) */
#define M_EDIT_BUF_SIZE (EDIT_BUF_SIZE - 1)

/*
 * Configurable: Maximal allowed number of buffers in each buffer array.
 * This number can be increased for systems with enough physical memory.
 */
#ifndef MAXBUFF
#define MAXBUFF 1024
#endif

/* Maximal length of file that can be opened */
#define SIZE_LIMIT (EDIT_BUF_SIZE * (MAXBUFF - 2))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct edit_buffer_struct {
    off_t curs1;                /* position of the cursor from the beginning of the file. */
    off_t curs2;                /* position from the end of the file */
    unsigned char *buffers1[MAXBUFF + 1];       /* all data up to curs1 */
    unsigned char *buffers2[MAXBUFF + 1];       /* all data from end of file down to curs2 */
} edit_buffer_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void edit_buffer_init (edit_buffer_t * buf);
void edit_buffer_clean (edit_buffer_t * buf);

int edit_buffer_get_byte (const edit_buffer_t * buf, off_t byte_index);
#ifdef HAVE_CHARSET
int edit_buffer_get_utf (const edit_buffer_t * buf, off_t byte_index, int *char_width);
int edit_buffer_get_prev_utf (const edit_buffer_t * buf, off_t byte_index, int *char_width);
#endif

off_t edit_buffer_read_file (edit_buffer_t * buf, int fd, off_t size);
gboolean edit_buffer_write_file (edit_buffer_t * buf, int fd);

/*** inline functions ****************************************************************************/

static inline int
edit_buffer_get_current_byte (const edit_buffer_t * buf)
{
    return edit_buffer_get_byte (buf, buf->curs1);
}

/* --------------------------------------------------------------------------------------------- */

static inline int
edit_buffer_get_previous_byte (const edit_buffer_t * buf)
{
    return edit_buffer_get_byte (buf, buf->curs1 - 1);
}

/* --------------------------------------------------------------------------------------------- */

#endif  /* MC__EDIT_BUFFER_H */
