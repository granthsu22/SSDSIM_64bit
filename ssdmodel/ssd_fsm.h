#ifndef __SSD_FSM_H__
#define __SSD_FSM_H__

#include "ssd.h"

enum ssd_reg_state {
	SSD_REG_EMPTY,
	SSD_REG_INPUTTING,
	SSD_REG_DATA,
	SSD_REG_OUTPUTTING
};

enum ssd_die_state {
	SSD_DIE_IDLE,
	SSD_DIE_READ,
	SSD_DIE_PROGRAM,
	SSD_DIE_ERASE
};

enum ssd_channel_state {
	SSD_CHANNEL_IDLE,
	SSD_CHANNEL_BUSY
};

struct ssd_reg_fsm {
	enum ssd_reg_state state;
	int ppn;
};

struct ssd_plane_fsm {
	struct ssd_reg_fsm cache_reg;
	struct ssd_reg_fsm data_reg;
};

struct ssd_die_fsm {
	enum ssd_die_state state;
	int activate_plane_id;
	double start_time;
	double end_time;
};

struct ssd_channel_fsm {
	enum ssd_channel_state state;
};

void ssd_fsm_init(ssd_t *currdisk);
int ssd_fsm_is_schedulable(ioreq_event *event);
void ssd_fsm_schedule(ioreq_event *event);

void ssd_fsm_on_event_created(ioreq_event *event);
void ssd_fsm_on_event_setup(ioreq_event *event);
void ssd_fsm_on_event_fired(ioreq_event *event);
void ssd_fsm_on_event_destroyed(ioreq_event *event);

void count_die_utilization(int die_id);

#endif

