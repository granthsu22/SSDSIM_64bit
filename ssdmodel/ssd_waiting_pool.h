#ifndef __SSD_WAITING_POOL_H__
#define __SSD_WAITING_POOL_H__

#include "ssd.h"

int ssd_waiting_pool_count();

ioreq_event *ssd_waiting_pool_front();
ioreq_event *ssd_waiting_pool_back();

void ssd_waiting_pool_insert(ioreq_event *curr);
void ssd_waiting_pool_erase(ioreq_event *curr);

#endif

