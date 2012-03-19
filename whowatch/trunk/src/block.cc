/*
 * To reduce malloc/free overheads linked list of _block_tbl_t
 * is used. Pre-allocated memory is managed by a bit map.
 */

#include <time.h>
#include "whowatch.h"

#define USED 		1
#define NOT_USED	0
#define TBL_SIZE	(sizeof(unsigned int)*8)

static FILE *logfile;
static int nr_blocks;

void dolog (const char *t, ...)
{
        va_list ap;
        char *c;
        time_t tm = time(0);
return;	
	if(!logfile) {
		logfile = fopen("/var/log/whowatch.log", "a");
		if(!logfile) exit(1);
		fprintf(logfile, " #############\n");
	}
        c = ctime(&tm);
        *(c + strlen(c) - 1) = 0;
        fprintf(logfile, "%s: ", c);
        va_start(ap, t);
        vfprintf(logfile, t, ap);
        va_end(ap);
        fflush(logfile);
}

struct _block_tbl_t {
	struct list_head head;
	void *_block_t;
	unsigned int map;
};

static struct _block_tbl_t *new_block(int size, struct list_head *h)
{
	struct _block_tbl_t *tmp;
	tmp = (_block_tbl_t*)calloc(1, sizeof *tmp);
	if(!tmp) prg_exit("new_block(): Cannot allocate memory. [1]\n");
	tmp->_block_t = calloc(1, size * TBL_SIZE);
	dolog("%s: new block(%d) - alloc size = %d\n",
		__FUNCTION__, nr_blocks, size * TBL_SIZE);
	if(!tmp->_block_t) prg_exit("new_block(): Cannot allocate memory. [2]\n");
	list_add(&tmp->head, h);
	return tmp;
}

/* 
 * Returns address of unused memory space. If necessary
 * allocate new block.
 */
void *get_empty(int size, struct list_head *h)
{
	int i;
	int nr = 0;
	struct _block_tbl_t *tmp;
	struct list_head *t;
	list_for_each(t, h) {
		tmp = list_entry(t, struct _block_tbl_t, head);
		if(tmp->map == ~0) continue;
		for(i = 0; i < TBL_SIZE; i++) {
			if((1<<i & tmp->map) == NOT_USED) goto FOUND;
		}
nr++;
	}
	dolog("%s: no empty space, getting new one.\n", __FUNCTION__);
	tmp = new_block(size, h);
	i = 0;
FOUND:
	dolog("%s: empty in %d block at %d pos\n", __FUNCTION__, nr, i);
	tmp->map |= 1<<i;
	return (char*)tmp->_block_t + (size * i);
}
