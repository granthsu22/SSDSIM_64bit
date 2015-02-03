#ifndef __SSD_FTL_H__ 
#define __SSD_FTL_H__

#include "ssd.h"
#include "ssd_timing.h"
#include "ssd_clean.h"
#include "ssd_gang.h"
#include "ssd_init.h"
#include "modules/ssdmodel_ssd_param.h"

extern int max_lba_in_disksim;

enum FLAG {
	FREE,
	VALID,
	INVALID,
	META
};

void ssd_ftl_init(ssd_t *currdisk);
void ssd_ftl_write(ioreq_event *curr);
void ssd_ftl_read(ioreq_event *curr);
int get_die_number(int lpn); // add by wendy

#endif

