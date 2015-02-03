#ifndef __SSD_MONITOR_H__
#define __SSD_MONITOR_H__

#include "ssd.h"

void ssd_die_monitor(void);
int judge_die_light_loading(int die_num);
void print_die_state(void);
void set_all_to_curr_period(void);
int get_waiting_pool_req_num(void);

#endif
