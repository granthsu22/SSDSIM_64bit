#include "ssd_scheduler.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "ssd_fsm.h"
#include "ssd_waiting_pool.h"

int pagePerBlock;
int blockPerPlane;
int planePerDie;
int diePerChannel;
int nChannel;

int totalDie;
int pagePerDie; 
int pagePerChannel;

void ssd_scheduler_init(ssd_t *currdisk) {

	pagePerBlock = currdisk->params.pages_per_block;
	blockPerPlane = currdisk->params.blocks_per_plane;
	planePerDie = currdisk->params.planes_per_pkg;
	diePerChannel = currdisk->params.elements_per_gang;
	nChannel = currdisk->params.nelements / currdisk->params.elements_per_gang;

	totalDie = nChannel * diePerChannel;
	pagePerDie = pagePerBlock * blockPerPlane * planePerDie;
	pagePerChannel = pagePerBlock * blockPerPlane * planePerDie * diePerChannel;

}

void ssd_scheduler_issue() {

	ioreq_event *curr;


	if(INTERNAL_SCHEDULER == 1) {

		/* ======================================== */
		/* Priority 1: exceed deadline request      */
		/*             has the highest priority     */
		/* ======================================== */

		if(INTERNAL_WRITE_DEADLINE_SCHEDULER == 1) {
			curr = ssd_waiting_pool_front();
			while(curr) {
				ssd_t *currdisk = getssd(curr->devno);
				ioreq_event *selected_req = (ioreq_event *) NULL;

				/* if(simtime - curr->ssd_arrive_time > WRITE_DEADLINE && curr->access_type == SSD_REQ_PROGRAM) {
				   curr->exceed_deadline = 1;
				   }*/

				if(curr->exceed_deadline == 1 && ssd_fsm_is_schedulable(curr)) {
					selected_req = curr;
				}

				curr = curr->next_waiting;
				if(selected_req) {
					ssd_waiting_pool_erase(selected_req);
					ssd_fsm_schedule(selected_req);
				}

			}
		}

		if(INTERNAL_WRITE_STARVE == 1) {
			curr = ssd_waiting_pool_front();
			while(curr) {
				ssd_t *currdisk = getssd(curr->devno);
				ioreq_event *selected_req = (ioreq_event *) NULL;

				/* if(simtime - curr->ssd_arrive_time > WRITE_DEADLINE && curr->access_type == SSD_REQ_PROGRAM) {
				   curr->exceed_deadline = 1;
				   }*/

				if(curr->exceed_starve == 1 && ssd_fsm_is_schedulable(curr)) {
					selected_req = curr;
				}

				curr = curr->next_waiting;
				if(selected_req) {
					ssd_waiting_pool_erase(selected_req);
					ssd_fsm_schedule(selected_req);
				}

			}

		}


		/* ======================================== */
		/* Priority 1: curr_period > next_period    */
		/*             read > write                 */
		/* ======================================== */

		if(CURR_OVER_NEXT == 1) {	
			curr = ssd_waiting_pool_front();
			if(READ_OVER_WRITE) {
				while(curr) { // handle read request
					ssd_t *currdisk = getssd(curr->devno);
					ioreq_event *selected_req = (ioreq_event *) NULL;

					if(curr->ssd_arrive_time <= replenish_time) {
						curr->access_period = SSD_REQ_CURR;
					} // raise the early-dispatched requests' priority when refreshing

					if(curr->access_period == SSD_REQ_CURR && curr->access_initiator == SSD_REQ_HOST && curr->access_type == SSD_REQ_READ && ssd_fsm_is_schedulable(curr)) {
						selected_req = curr;
					}

					curr = curr->next_waiting;
					if(selected_req) {
						ssd_waiting_pool_erase(selected_req);
						ssd_fsm_schedule(selected_req);
					}
				}
			}

			if(HOST_OVER_GC) {
				curr = ssd_waiting_pool_front();
				while(curr) { // handle write request
					ssd_t *currdisk = getssd(curr->devno);
					ioreq_event *selected_req = (ioreq_event *) NULL;
					if(curr->access_period == SSD_REQ_CURR && curr->access_initiator == SSD_REQ_HOST && ssd_fsm_is_schedulable(curr)) {
						selected_req = curr;
					}

					curr = curr->next_waiting;
					if(selected_req) {
						ssd_waiting_pool_erase(selected_req);
						ssd_fsm_schedule(selected_req);
					}
				}
			}

			curr = ssd_waiting_pool_front();
			while (curr) {
				ssd_t *currdisk = getssd (curr->devno);

				ioreq_event *selected_req = (ioreq_event *) NULL;

				if (curr->access_period == SSD_REQ_CURR && ssd_fsm_is_schedulable(curr)) {
					selected_req = curr;
				}

				/* since ssd_waiting_pool_erase() will clear the next_waiting pointer */
				/* before calling ssd_waiting_pool_erase(), we need to first move to the next request */
				curr = curr->next_waiting;

				/* now we can safely call ssd_waiting_pool_erase() for selected_req if it is not NULL */
				if (selected_req) {

					ssd_waiting_pool_erase(selected_req);		
					ssd_fsm_schedule(selected_req);

				}
			}

		}


		/* ======================================== */
		/* Priority 2: Read from Host PC            */
		/* ======================================== */

		if(READ_OVER_WRITE == 1) {	
			curr = ssd_waiting_pool_front();
			while (curr) {

				ssd_t *currdisk = getssd (curr->devno);

				ioreq_event *selected_req = (ioreq_event *) NULL;

				if (curr->access_initiator == SSD_REQ_HOST && curr->access_type == SSD_REQ_READ && ssd_fsm_is_schedulable(curr)) {
					selected_req = curr;
				}


				/* since ssd_waiting_pool_erase() will clear the next_waiting pointer */
				/* before calling ssd_waiting_pool_erase(), we need to first move to the next request */
				curr = curr->next_waiting;

				/* now we can safely call ssd_waiting_pool_erase() for selected_req if it is not NULL */
				if (selected_req) {
					ssd_waiting_pool_erase(selected_req);		
					ssd_fsm_schedule(selected_req);
				}
			}
		}


		/* ======================================== */
		/* Priority 3: Program/Erase from Host PC   */
		/* ======================================== */

		if(HOST_OVER_GC == 1) {
			curr = ssd_waiting_pool_front();
			while (curr) {

				ssd_t *currdisk = getssd (curr->devno);

				ioreq_event *selected_req = (ioreq_event *) NULL;

				if (curr->access_initiator == SSD_REQ_HOST && ssd_fsm_is_schedulable(curr)) {
					selected_req = curr;
				}


				/* since ssd_waiting_pool_erase() will clear the next_waiting pointer */
				/* before calling ssd_waiting_pool_erase(), we need to first move to the next request */
				curr = curr->next_waiting;

				/* now we can safely call ssd_waiting_pool_erase() for selected_req if it is not NULL */
				if (selected_req) {
					ssd_waiting_pool_erase(selected_req);		
					ssd_fsm_schedule(selected_req);
				}
			}
		}
	}

	/* ======================================== */
	/* Priority 4: Read/Program/Erase from GC   */
	/* ======================================== */

	curr = ssd_waiting_pool_front();
	while (curr) {
		ssd_t *currdisk = getssd (curr->devno);

		ioreq_event *selected_req = (ioreq_event *) NULL;

		if (ssd_fsm_is_schedulable(curr)) {
			selected_req = curr;
		}

		/* since ssd_waiting_pool_erase() will clear the next_waiting pointer */
		/* before calling ssd_waiting_pool_erase(), we need to first move to the next request */
		curr = curr->next_waiting;

		/* now we can safely call ssd_waiting_pool_erase() for selected_req if it is not NULL */
		if (selected_req) {

			ssd_waiting_pool_erase(selected_req);		
			ssd_fsm_schedule(selected_req);

		}
	}
}

