#include "ssd_fsm.h"
#include "disksim_global.h"

#define SSD_BYTES_PER_COMMAND 1
#define SSD_BYTES_PER_ROW_ADDRESS 3
#define SSD_BYTES_PER_COLUMN_ADDRESS 2
#define SSD_BYTES_PER_STATUS 1

int pagePerBlock;
int blockPerPlane;
int planePerDie;
int diePerChannel;
int nChannel;

int totalDie;
int totalPlane;

int pagePerPlane;
int pagePerDie; 
int pagePerChannel;

struct ssd_channel_fsm *channel_fsm;
struct ssd_die_fsm *die_fsm;
struct ssd_plane_fsm *plane_fsm;

#define SSD_POWER_PLANE_READ 50.0
#define SSD_POWER_PLANE_PROGRAM 50.0
#define SSD_POWER_PLANE_ERASE 50.0
#define SSD_POWER_CHANNEL_INPUT 20.0
#define SSD_POWER_CHANNEL_OUTPUT 20.0

double current_power;

extern char *power_trace_filename;
FILE *power_trace;
double last_time;
double last_power;
double recoreded_power;
void ssd_fsm_init_power_monitor();
void ssd_fsm_notify_power_monitor();

void ssd_fsm_init(ssd_t *currdisk) {

	pagePerBlock = currdisk->params.pages_per_block;
	blockPerPlane = currdisk->params.blocks_per_plane;
	planePerDie = currdisk->params.planes_per_pkg;
	diePerChannel = currdisk->params.elements_per_gang;
	nChannel = currdisk->params.nelements / currdisk->params.elements_per_gang;

	//printf("%d %d %d %d %d\n", pagePerBlock, blockPerPlane, planePerDie, diePerChannel, nChannel);

	totalDie = nChannel * diePerChannel;
	totalPlane = nChannel * diePerChannel * planePerDie;

	pagePerPlane = pagePerBlock * blockPerPlane;
	pagePerDie = pagePerBlock * blockPerPlane * planePerDie;
	pagePerChannel = pagePerBlock * blockPerPlane * planePerDie * diePerChannel;

	channel_fsm = (struct ssd_channel_fsm *) malloc(nChannel * sizeof(struct ssd_channel_fsm));
	ASSERT(channel_fsm);

	int channel_id;
	for (channel_id = 0; channel_id < nChannel; channel_id++) {
		channel_fsm[channel_id].state = SSD_CHANNEL_IDLE;
	}

	die_fsm = (struct ssd_die_fsm *) malloc(totalDie * sizeof(struct ssd_die_fsm));
	ASSERT(die_fsm);

	int die_id;
	for (die_id = 0; die_id < totalDie; die_id++) {
		die_fsm[die_id].state = SSD_DIE_IDLE;
		die_fsm[die_id].activate_plane_id = -1;
		die_state[die_id] = 0; // wendy
		die_time_accumulate[die_id] = 0.0; // wendy
	}

	plane_fsm = (struct ssd_plane_fsm *) malloc(totalPlane * sizeof(struct ssd_plane_fsm));
	ASSERT(plane_fsm);

	int plane_id;
	for (plane_id = 0; plane_id < totalPlane; plane_id++) {
		plane_fsm[plane_id].cache_reg.state = SSD_REG_EMPTY;
		plane_fsm[plane_id].cache_reg.ppn = -1;
		plane_fsm[plane_id].data_reg.state = SSD_REG_EMPTY;
		plane_fsm[plane_id].data_reg.ppn = -1;
	}

	current_power = 0.0;
	ssd_fsm_init_power_monitor();
}

int ssd_fsm_is_schedulable(ioreq_event *event) {

	int channel_id = event->ppn / pagePerChannel;
	ASSERT(channel_id < nChannel);

	int die_id = event->ppn / pagePerDie;
	ASSERT(die_id < totalDie);

	int plane_id = event->ppn / pagePerPlane;
	ASSERT(plane_id < totalPlane);

	switch (event->access_phase + 1) {

		case SSD_REQ_INPUTTED:

			if (channel_fsm[channel_id].state != SSD_CHANNEL_IDLE) {
				return 0;
			}

			switch (event->access_type) {

				case SSD_REQ_READ:

					if (die_fsm[die_id].state != SSD_DIE_IDLE
							&& die_fsm[die_id].state != SSD_DIE_READ) {
						return 0;
					}

					if (die_fsm[die_id].activate_plane_id != -1
							&& die_fsm[die_id].activate_plane_id != plane_id) {
						return 0;
					}

					if (plane_fsm[plane_id].data_reg.state != SSD_REG_EMPTY
							|| plane_fsm[plane_id].data_reg.ppn != -1) {
						return 0;
					}
				
					break;

				case SSD_REQ_PROGRAM:

					if (die_fsm[die_id].state != SSD_DIE_IDLE
							&& die_fsm[die_id].state != SSD_DIE_PROGRAM) {
						return 0;
					}

					if (die_fsm[die_id].activate_plane_id != -1
							&& die_fsm[die_id].activate_plane_id != plane_id) {
						return 0;
					}

					if (plane_fsm[plane_id].cache_reg.state != SSD_REG_EMPTY
							|| plane_fsm[plane_id].cache_reg.ppn != -1) {
						return 0;
					}

					break;

				case SSD_REQ_ERASE:

					if (die_fsm[die_id].state != SSD_DIE_IDLE) {
						return 0;
					}

					if (die_fsm[die_id].activate_plane_id != -1) {
						return 0;
					}

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_STARTED:

			if (channel_fsm[channel_id].state != SSD_CHANNEL_IDLE) {
				return 0;
			}

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					if (plane_fsm[plane_id].data_reg.state != SSD_REG_DATA
							|| plane_fsm[plane_id].data_reg.ppn != event->ppn) {

						return 0;
					}

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_OUTPUTTED:

			if (channel_fsm[channel_id].state != SSD_CHANNEL_IDLE) {
				return 0;
			}

			switch (event->access_type) {

				case SSD_REQ_READ:

					
					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					if (plane_fsm[plane_id].cache_reg.state != SSD_REG_DATA
							|| plane_fsm[plane_id].cache_reg.ppn != event->ppn) {
					
						return 0;
					}

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		default:

			ASSERT(0);

			break;
	}

	return 1;
}

void ssd_fsm_schedule(ioreq_event *event) {

	ASSERT(ssd_fsm_is_schedulable(event));

	ssd_t *currdisk = getssd(event->devno);

	switch (event->access_phase) {

		case SSD_REQ_CREATED:

			event->access_phase = SSD_REQ_INPUTTED;

			switch (event->access_type) {

				case SSD_REQ_READ:
					event->time = simtime
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_COLUMN_ADDRESS * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_ROW_ADDRESS * currdisk->params.chip_xfer_latency;
					break;

				case SSD_REQ_PROGRAM:
					event->time = simtime
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_COLUMN_ADDRESS * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_ROW_ADDRESS * currdisk->params.chip_xfer_latency
						+ currdisk->params.page_size * SSD_BYTES_PER_SECTOR * currdisk->params.chip_xfer_latency;
					break;

				case SSD_REQ_ERASE:
					event->time = simtime
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_ROW_ADDRESS * currdisk->params.chip_xfer_latency;
					break;

				default:
					ASSERT(0);
					break;

			}

			break;

		case SSD_REQ_INPUTTED:

			event->access_phase = SSD_REQ_STARTED;
			event->time = simtime
				+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency;

			break;

		case SSD_REQ_FINISHED:

			event->access_phase = SSD_REQ_OUTPUTTED;

			switch (event->access_type) {

				case SSD_REQ_READ:
					event->time = simtime
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_STATUS * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ currdisk->params.page_size * SSD_BYTES_PER_SECTOR * currdisk->params.chip_xfer_latency;
					break;

				case SSD_REQ_PROGRAM:
					event->time = simtime
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_STATUS * currdisk->params.chip_xfer_latency;
					break;

				case SSD_REQ_ERASE:
					event->time = simtime
						+ SSD_BYTES_PER_COMMAND * currdisk->params.chip_xfer_latency
						+ SSD_BYTES_PER_STATUS * currdisk->params.chip_xfer_latency;
					break;

				default:
					ASSERT(0);
					break;

			}

			break;

		default:

			ASSERT(0);

			break;
	}

	addtointq(event);
	ssd_fsm_on_event_setup(event);

}

void ssd_fsm_on_event_created(ioreq_event *event) {

	ASSERT(event->access_phase == SSD_REQ_CREATED);
}

void ssd_fsm_on_event_setup(ioreq_event *event) {

	int channel_id = event->ppn / pagePerChannel;
	ASSERT(channel_id < nChannel);

	int die_id = event->ppn / pagePerDie;
	ASSERT(die_id < totalDie);

	int plane_id = event->ppn / pagePerPlane;
	ASSERT(plane_id < totalPlane);

	switch (event->access_phase) {

		case SSD_REQ_INPUTTED:

			ASSERT(channel_fsm[channel_id].state == SSD_CHANNEL_IDLE);
			channel_fsm[channel_id].state = SSD_CHANNEL_BUSY;
			current_power += SSD_POWER_CHANNEL_INPUT;

			switch (event->access_type) {

				case SSD_REQ_READ:

					if (die_fsm[die_id].state == SSD_DIE_IDLE) {
						die_fsm[die_id].state = SSD_DIE_READ;
						die_state[die_id] = 1; // wendy
						die_fsm[die_id].start_time = simtime; // wendy
						die_fsm[die_id].activate_plane_id = plane_id;
					}

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == -1);

					plane_fsm[plane_id].data_reg.ppn = event->ppn;

					break;

				case SSD_REQ_PROGRAM:

					if (die_fsm[die_id].state == SSD_DIE_IDLE) {
						die_fsm[die_id].state = SSD_DIE_PROGRAM;
						die_state[die_id] = 1; // wendy
						die_fsm[die_id].start_time = simtime; // wendy
						die_fsm[die_id].activate_plane_id = plane_id;
					}

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].cache_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].cache_reg.ppn == -1);

					plane_fsm[plane_id].cache_reg.state = SSD_REG_INPUTTING;
					plane_fsm[plane_id].cache_reg.ppn = event->ppn;

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_IDLE);
					ASSERT(die_fsm[die_id].activate_plane_id == -1);
					die_fsm[die_id].state = SSD_DIE_ERASE;
					die_state[die_id] = 1; // wendy
					die_fsm[die_id].start_time = simtime; // wendy
					die_fsm[die_id].activate_plane_id = plane_id;

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_STARTED:

			ASSERT(channel_fsm[channel_id].state == SSD_CHANNEL_IDLE);
			channel_fsm[channel_id].state = SSD_CHANNEL_BUSY;
			current_power += SSD_POWER_CHANNEL_INPUT;

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_DATA);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_FINISHED:

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_INPUTTING);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_OUTPUTTING);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_OUTPUTTED:

			ASSERT(channel_fsm[channel_id].state == SSD_CHANNEL_IDLE);
			channel_fsm[channel_id].state = SSD_CHANNEL_BUSY;
			current_power += SSD_POWER_CHANNEL_OUTPUT;

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].cache_reg.state == SSD_REG_DATA);
					ASSERT(plane_fsm[plane_id].cache_reg.ppn == event->ppn);

					plane_fsm[plane_id].cache_reg.state = SSD_REG_OUTPUTTING;

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		default:

			ASSERT(0);

			break;
	}

	ssd_fsm_notify_power_monitor();
}

void ssd_fsm_on_event_fired(ioreq_event *event) {

	int channel_id = event->ppn / pagePerChannel;
	ASSERT(channel_id < nChannel);

	int die_id = event->ppn / pagePerDie;
	ASSERT(die_id < totalDie);

	int plane_id = event->ppn / pagePerPlane;
	ASSERT(plane_id < totalPlane);

	switch (event->access_phase) {

		case SSD_REQ_INPUTTED:

			ASSERT(channel_fsm[channel_id].state == SSD_CHANNEL_BUSY);
			channel_fsm[channel_id].state = SSD_CHANNEL_IDLE;
			current_power -= SSD_POWER_CHANNEL_INPUT;

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].cache_reg.state == SSD_REG_INPUTTING);
					ASSERT(plane_fsm[plane_id].cache_reg.ppn == event->ppn);

					plane_fsm[plane_id].cache_reg.state = SSD_REG_DATA;

					if (plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY
							&& plane_fsm[plane_id].data_reg.ppn == -1) {

						plane_fsm[plane_id].data_reg.state = plane_fsm[plane_id].cache_reg.state;
						plane_fsm[plane_id].data_reg.ppn = plane_fsm[plane_id].cache_reg.ppn;

						plane_fsm[plane_id].cache_reg.state = SSD_REG_EMPTY;
						plane_fsm[plane_id].cache_reg.ppn = -1;
					}

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_STARTED:

			ASSERT(channel_fsm[channel_id].state == SSD_CHANNEL_BUSY);
			channel_fsm[channel_id].state = SSD_CHANNEL_IDLE;
			current_power -= SSD_POWER_CHANNEL_INPUT;

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					plane_fsm[plane_id].data_reg.state = SSD_REG_INPUTTING;
					current_power += SSD_POWER_PLANE_READ;

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_DATA);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					plane_fsm[plane_id].data_reg.state = SSD_REG_OUTPUTTING;
					current_power += SSD_POWER_PLANE_PROGRAM;

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					current_power += SSD_POWER_PLANE_ERASE;

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_FINISHED:

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_INPUTTING);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					plane_fsm[plane_id].data_reg.state = SSD_REG_DATA;
					current_power -= SSD_POWER_PLANE_READ;

					if (plane_fsm[plane_id].cache_reg.state == SSD_REG_EMPTY && plane_fsm[plane_id].cache_reg.ppn == -1) {

						plane_fsm[plane_id].cache_reg.state = plane_fsm[plane_id].data_reg.state;
						plane_fsm[plane_id].cache_reg.ppn = plane_fsm[plane_id].data_reg.ppn;

						plane_fsm[plane_id].data_reg.state = SSD_REG_EMPTY;
						plane_fsm[plane_id].data_reg.ppn = -1;
					}

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_OUTPUTTING);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					plane_fsm[plane_id].data_reg.state = SSD_REG_EMPTY;
					current_power -= SSD_POWER_PLANE_PROGRAM;

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);

					current_power -= SSD_POWER_PLANE_ERASE;

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		case SSD_REQ_OUTPUTTED:

			ASSERT(channel_fsm[channel_id].state == SSD_CHANNEL_BUSY);
			channel_fsm[channel_id].state = SSD_CHANNEL_IDLE;
			current_power -= SSD_POWER_CHANNEL_OUTPUT;

			switch (event->access_type) {

				case SSD_REQ_READ:

					ASSERT(die_fsm[die_id].state == SSD_DIE_READ);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].cache_reg.state == SSD_REG_OUTPUTTING);
					ASSERT(plane_fsm[plane_id].cache_reg.ppn == event->ppn);

					plane_fsm[plane_id].cache_reg.state = SSD_REG_EMPTY;
					plane_fsm[plane_id].cache_reg.ppn = -1;

					if (plane_fsm[plane_id].data_reg.state == SSD_REG_DATA
							&& plane_fsm[plane_id].data_reg.ppn != -1) {

						plane_fsm[plane_id].cache_reg.state = plane_fsm[plane_id].data_reg.state;
						plane_fsm[plane_id].cache_reg.ppn = plane_fsm[plane_id].data_reg.ppn;

						plane_fsm[plane_id].data_reg.state = SSD_REG_EMPTY;
						plane_fsm[plane_id].data_reg.ppn = -1;
					}

					if (plane_fsm[plane_id].cache_reg.state == SSD_REG_EMPTY
							&& plane_fsm[plane_id].cache_reg.ppn == -1
							&& plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY
							&& plane_fsm[plane_id].data_reg.ppn == -1) {

						die_fsm[die_id].state = SSD_DIE_IDLE;
						die_state[die_id] = 0; // wendy
						die_fsm[die_id].end_time = simtime; // wendy
						count_die_utilization(die_id); // wendy
						die_fsm[die_id].activate_plane_id = -1;
					}

					break;

				case SSD_REQ_PROGRAM:

					ASSERT(die_fsm[die_id].state == SSD_DIE_PROGRAM);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					ASSERT(plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY);
					ASSERT(plane_fsm[plane_id].data_reg.ppn == event->ppn);

					plane_fsm[plane_id].data_reg.ppn = -1;

					if (plane_fsm[plane_id].cache_reg.state == SSD_REG_DATA
							&& plane_fsm[plane_id].cache_reg.ppn != -1) {

						plane_fsm[plane_id].data_reg.state = plane_fsm[plane_id].cache_reg.state;
						plane_fsm[plane_id].data_reg.ppn = plane_fsm[plane_id].cache_reg.ppn;

						plane_fsm[plane_id].cache_reg.state = SSD_REG_EMPTY;
						plane_fsm[plane_id].cache_reg.ppn = -1;
					}

					if (plane_fsm[plane_id].cache_reg.state == SSD_REG_EMPTY
							&& plane_fsm[plane_id].cache_reg.ppn == -1
							&& plane_fsm[plane_id].data_reg.state == SSD_REG_EMPTY
							&& plane_fsm[plane_id].data_reg.ppn == -1) {

						die_fsm[die_id].state = SSD_DIE_IDLE;
						die_state[die_id] = 0; // wendy
						die_fsm[die_id].end_time = simtime; // wendy
						count_die_utilization(die_id); // wendy

						die_fsm[die_id].activate_plane_id = -1;
					}

					break;

				case SSD_REQ_ERASE:

					ASSERT(die_fsm[die_id].state == SSD_DIE_ERASE);
					ASSERT(die_fsm[die_id].activate_plane_id == plane_id);
					die_fsm[die_id].state = SSD_DIE_IDLE;
					die_state[die_id] = 0; // wendy
					die_fsm[die_id].end_time = simtime; // wendy
					count_die_utilization(die_id); // wendy

					die_fsm[die_id].activate_plane_id = -1;

					break;

				default:
					ASSERT(0);
					break;
			}

			break;

		default:

			ASSERT(0);

			break;
	}

	ssd_fsm_notify_power_monitor();
}

void ssd_fsm_on_event_destroyed(ioreq_event *event) {

	ASSERT(event->access_phase == SSD_REQ_DESTROYED);
}

void ssd_fsm_init_power_monitor(void) {
	if (power_trace_filename) {
		last_time = simtime;
		last_power = 0.0;
		recoreded_power = 0.0;
		power_trace = fopen(power_trace_filename, "w");
	}
}

void ssd_fsm_notify_power_monitor(void) {
	if (power_trace_filename) {
		if (last_time != simtime) {
			if (last_power != recoreded_power) {
				fprintf(power_trace, "%f\t%f\n", last_time, last_power);
				recoreded_power = last_power;
			}
			last_time = simtime;
		}
		last_power = current_power;
	}
}

void count_die_utilization(int die_id)
{
	die_time_accumulate[die_id] += die_fsm[die_id].end_time - die_fsm[die_id].start_time;

}
