// this file is added by wendy
#include "ssd_monitor.h"

void ssd_die_pending_request_init(void)
{
	int i;
	
	for(i = 0; i < totalDie; i++) {
		die_pending_request[i] = 0;
	}
}

void ssd_die_monitor(void)
{
	ioreq_event *curr;
	int die_num;
	
	ssd_die_pending_request_init();

	for(die_num = 0; die_num < totalDie; die_num++) {
		die_pending_request[die_num] += die_state[die_num];
		die_pending_request[die_num] += waiting_pool_die_condition[die_num];
	}
	
}

int judge_die_light_loading(int die_num)
{
	ASSERT(die_num >= 0 && die_num < totalDie);
//#ifdef FIOS	
//	if(die_pending_request[die_num] < 1) {
//#else

	if(die_pending_request[die_num] < DISPATCH_THERSHOLD) {
//#endif
		return 1;
	}

	return 0;
}

void print_die_state(void)
{
	int i;
	for(i = 0; i < totalDie; i++) {
		printf("%d ", die_state[i]);
	}
	printf("\n");
}

void set_all_to_curr_period(void)
{
	ioreq_event *curr;
	curr = ssd_waiting_pool_front();
	while(curr != NULL) {
		curr->access_period = SSD_REQ_CURR;
		curr = curr->next;
	}
}

int get_waiting_pool_req_num(void)
{
	ioreq_event *curr;
	int num = 0;
	curr = ssd_waiting_pool_front();
	while(curr != NULL) {
		curr = curr->next;
		num++;
	}

	return num;
}
