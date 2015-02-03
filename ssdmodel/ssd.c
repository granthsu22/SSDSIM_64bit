// DiskSim SSD support
// ?2008 Microsoft Corporation. All Rights Reserved
#include <math.h>

#include "ssd.h"
#include "ssd_timing.h"
#include "ssd_clean.h"
#include "ssd_gang.h"
#include "ssd_init.h"
#include "modules/ssdmodel_ssd_param.h"

/* modified by gengyouchen */
#include "ssd_fsm.h"
#include "ssd_ftl.h"
#include "ssd_waiting_pool.h"
#include "ssd_scheduler.h"

#ifndef sprintf_s
#define sprintf_s3(x,y,z) sprintf(x,z)
#define sprintf_s4(x,y,z,w) sprintf(x,z,w)
#define sprintf_s5(x,y,z,w,s) sprintf(x,z,w,s)
#else
#define sprintf_s3(x,y,z) sprintf_s(x,z)
#define sprintf_s4(x,y,z,w) sprintf_s(x,z,w)
#define sprintf_s5(x,y,z,w,s) sprintf_s(x,z,w,s)
#endif

#ifndef _strdup
#define _strdup strdup
#endif


static void ssd_request_complete(ioreq_event *curr);
static void ssd_media_access_request(ioreq_event *curr);


struct ssd *getssd (int devno)
{
	struct ssd *s;
	ASSERT1((devno >= 0) && (devno < MAXDEVICES), "devno", devno);

	s = disksim->ssdinfo->ssds[devno];

	return (disksim->ssdinfo->ssds[devno]);
}

int ssd_set_depth (int devno, int inbusno, int depth, int slotno)
{
	ssd_t *currdisk;
	int cnt;

	currdisk = getssd (devno);
	assert(currdisk);
	cnt = currdisk->numinbuses;
	currdisk->numinbuses++;
	if ((cnt + 1) > MAXINBUSES) {
		fprintf(stderr, "Too many inbuses specified for ssd %d - %d\n", devno, (cnt+1));
		exit(1);
	}
	currdisk->inbuses[cnt] = inbusno;
	currdisk->depth[cnt] = depth;
	currdisk->slotno[cnt] = slotno;
	return(0);
}

int ssd_get_depth (int devno)
{
	ssd_t *currdisk;
	currdisk = getssd (devno);
	return(currdisk->depth[0]);
}

int ssd_get_slotno (int devno)
{
	ssd_t *currdisk;
	currdisk = getssd (devno);
	return(currdisk->slotno[0]);
}

int ssd_get_inbus (int devno)
{
	ssd_t *currdisk;
	currdisk = getssd (devno);
	return(currdisk->inbuses[0]);
}

int ssd_get_maxoutstanding (int devno)
{
	ssd_t *currdisk;
	currdisk = getssd (devno);
	return(currdisk->maxqlen);
}

double ssd_get_blktranstime (ioreq_event *curr)
{
	ssd_t *currdisk;
	double tmptime;

	currdisk = getssd (curr->devno);
	tmptime = bus_get_transfer_time(ssd_get_busno(curr), 1, (curr->flags & READ));
	if (tmptime < currdisk->blktranstime) {
		tmptime = currdisk->blktranstime;
	}
	return(tmptime);
}

int ssd_get_busno (ioreq_event *curr)
{
	ssd_t *currdisk;
	intchar busno;
	int depth;

	currdisk = getssd (curr->devno);
	busno.value = curr->busno;
	depth = currdisk->depth[0];
	return(busno.byte[depth]);
}

static void ssd_assert_current_activity(ssd_t *currdisk, ioreq_event *curr)
{
	/*	assert(currdisk->channel_activity != NULL &&
		currdisk->channel_activity->devno == curr->devno &&
		currdisk->channel_activity->blkno == curr->blkno &&
		currdisk->channel_activity->bcount == curr->bcount);*/
}

/*
 * ssd_send_event_up_path()
 *
 * Acquires the bus (if not already acquired), then uses bus_delay to
 * send the event up the path.
 *
 * If the bus is already owned by this device or can be acquired
 * immediately (interleaved bus), the event is sent immediately.
 * Otherwise, ssd_bus_ownership_grant will later send the event.
 */
static void ssd_send_event_up_path (ioreq_event *curr, double delay)
{
	ssd_t *currdisk;
	int busno;
	int slotno;

	// fprintf (outputfile, "ssd_send_event_up_path - devno %d, type %d, cause %d, blkno %d\n", curr->devno, curr->type, curr->cause, curr->blkno);

	currdisk = getssd (curr->devno);

	ssd_assert_current_activity(currdisk, curr);

	busno = ssd_get_busno(curr);
	slotno = currdisk->slotno[0];

	/* Put new request at head of buswait queue */
	curr->next = currdisk->buswait;
	currdisk->buswait = curr;

	curr->tempint1 = busno;
	curr->time = delay;
	if (currdisk->busowned == -1) {

		// fprintf (outputfile, "Must get ownership of the bus first\n");

		if (curr->next) {
			//fprintf(stderr,"Multiple bus requestors detected in ssd_send_event_up_path\n");
			/* This should be ok -- counting on the bus module to sequence 'em */
		}
		if (bus_ownership_get(busno, slotno, curr) == FALSE) {
			/* Remember when we started waiting (only place this is written) */
			currdisk->stat.requestedbus = simtime;
		} else {
			currdisk->busowned = busno;
			bus_delay(busno, DEVICE, curr->devno, delay, curr); /* Never for SCSI */
		}
	} else if (currdisk->busowned == busno) {

		//fprintf (outputfile, "Already own bus - so send it on up\n");

		bus_delay(busno, DEVICE, curr->devno, delay, curr);
	} else {
		fprintf(stderr, "Wrong bus owned for transfer desired\n");
		exit(1);
	}
}

/* The idea here is that only one request can "possess" the channel back to the
   controller at a time. All others are enqueued on queue of pending activities.
   "Completions" ... those operations that need only be signaled as done to the
   controller ... are given on this queue.  The "channel_activity" field indicates
   whether any operation currently possesses the channel.

   It is our hope that new requests cannot enter the system when the channel is
   possessed by another operation.  This would not model reality!!  However, this
   code (and that in ssd_request_arrive) will handle this case "properly" by enqueuing
   the incoming request.  */

void generate_fake_event2()
{
    ioreq_event *temp;
    temp = (ioreq_event*) getfromextraq();

    fios_process_time_total += (simtime - fios_process_time_start);
    fios_process_time_start = simtime;

    temp->pid = FAKE_EVENT;
    temp->time = simtime + 3;
    temp->devno = 0;
    temp->blkno = 8000;
    temp->bcount = 64;
    temp->flags = 0;
    temp->buf = 0;
    temp->opid = 0;
    temp->busno = 0;
    temp->cause = 0;
    temp->next = NULL;
    temp->prev = NULL;
    temp->trace_time = temp->time;
    temp->type = IO_REQUEST_ARRIVE;

    anticipate_event++;
    addtointq((event*) temp);

}


static void ssd_check_channel_activity (ssd_t *currdisk)
{
	int enter_time = 0;
	//	printf("==========ssd_check_channel_activity begin==========\n");	
	//	printf("ssd_check_channel_activity: %lf\n", simtime);	
	while (1) {
		ioreq_event *curr = currdisk->completion_queue;
		currdisk->channel_activity = curr;
		if (curr != NULL) {
			currdisk->completion_queue = curr->next;
			if (currdisk->neverdisconnect) { // enter
				/* already connected */
				if (curr->flags & READ) {
					/* transfer data up the line: curr->bcount, which is still set to */
					/* original requested value, indicates how many blks to transfer. */
					curr->type = DEVICE_DATA_TRANSFER_COMPLETE;
					ssd_send_event_up_path(curr, (double) 0.0);
				} else {
					ssd_request_complete (curr);
				}
			} else { // never enter
				/* reconnect to controller */
				curr->type = IO_INTERRUPT_ARRIVE;
				curr->cause = RECONNECT;
				ssd_send_event_up_path (curr, currdisk->bus_transaction_latency);
				currdisk->reconnect_reason = DEVICE_ACCESS_COMPLETE;
			}
		} else { // no requests want to transfer from SSD to host
			//			printf("ssd_check_channel_activity\n");
			enter_time = 0;
			while(NCQ_command_size < NCQ_COMMAND_SIZE_BOUND) {
				if(NCQ_command_size < NCQ_COMMAND_SIZE_BOUND) {
					enter_time++;
					if(FAIRNESS_SCHEDULER == CFQ) {
						curr = cfq_queue_get_next_request(currdisk->queue);
					} else 	if(FAIRNESS_SCHEDULER == FIOS) {
						curr = fios_queue_get_next_request(currdisk->queue, enter_time);
					} else 	if(FAIRNESS_SCHEDULER == WENDY) {
						curr = wendy_queue_get_next_request(currdisk->queue, enter_time);	
					} else 	if(FAIRNESS_SCHEDULER == ORACLE) {
						curr = oracle_queue_get_next_request(currdisk->queue);	
					} else 	if(FAIRNESS_SCHEDULER == NOOP) {
						curr = ioqueue_get_next_request(currdisk->queue);
					} else {
						fprintf(stderr, "no fairness scheduler\n");
					}
				}
				if(curr != NULL){
					generate_fake_event2();
				}
				currdisk->channel_activity = curr;
				if (curr != NULL) {
					NCQ_command_size++; // wendy
					//					printf("ncq: %d simtime: %lf\n", NCQ_command_size, simtime);
					if (curr->flags & READ) {
						if (!currdisk->neverdisconnect) {
							ssd_media_access_request(ioreq_copy(curr));
							curr->type = IO_INTERRUPT_ARRIVE;
							curr->cause = DISCONNECT;
							ssd_send_event_up_path (curr, currdisk->bus_transaction_latency);
						} else {
							ssd_media_access_request(curr);
							continue;
						}
					} else {
						curr->cause = RECONNECT;
						curr->type = IO_INTERRUPT_ARRIVE;
						currdisk->reconnect_reason = IO_INTERRUPT_ARRIVE;
						ssd_send_event_up_path (curr, currdisk->bus_transaction_latency);
					}
				} else {
					break;
				}

				if(FAIRNESS_SCHEDULER != WENDY) {
					//break;
				}
			}
		}
		break;
	}
	//	printf("==========ssd_check_channel_activity end==========\n");	
}

/*
 * ssd_bus_ownership_grant
 * Calls bus_delay to handle the event that the disk has been granted the bus.  I believe
 * this is always initiated by a call to ssd_send_even_up_path.
 */
void ssd_bus_ownership_grant (int devno, ioreq_event *curr, int busno, double arbdelay)
{
	ssd_t *currdisk;
	ioreq_event *tmp;

	currdisk = getssd (devno);

	ssd_assert_current_activity(currdisk, curr);
	tmp = currdisk->buswait;
	while ((tmp != NULL) && (tmp != curr)) {
		tmp = tmp->next;
	}
	if (tmp == NULL) {
		fprintf(stderr, "Bus ownership granted to unknown ssd request - devno %d, busno %d\n", devno, busno);
		exit(1);
	}
	currdisk->busowned = busno;
	currdisk->stat.waitingforbus += arbdelay;
	//ASSERT (arbdelay == (simtime - currdisk->stat.requestedbus));
	currdisk->stat.numbuswaits++;
	bus_delay(busno, DEVICE, devno, tmp->time, tmp);
}

void ssd_bus_delay_complete (int devno, ioreq_event *curr, int sentbusno)
{
	//	printf("ssd_bus_delay_complete\n");
	ssd_t *currdisk;
	intchar slotno;
	intchar busno;
	int depth;

	currdisk = getssd (devno);
	ssd_assert_current_activity(currdisk, curr);

	// fprintf (outputfile, "Entered ssd_bus_delay_complete\n");

	// EPW: I think the buswait logic doesn't do anything, is confusing, and risks
	// overusing the "next" field, although an item shouldn't currently be a queue.
	if (curr == currdisk->buswait) {
		currdisk->buswait = curr->next;
	} else {
		ioreq_event *tmp = currdisk->buswait;
		while ((tmp->next != NULL) && (tmp->next != curr)) {
			tmp = tmp->next;
		}
		if (tmp->next != curr) {
			// fixed a warning here
			//fprintf(stderr, "Bus delay complete for unknown ssd request - devno %d, busno %d\n", devno, busno.value);
			fprintf(stderr, "Bus delay complete for unknown ssd request - devno %d, busno %d\n", devno, curr->busno);
			exit(1);
		}
		tmp->next = curr->next;
	}
	busno.value = curr->busno;
	slotno.value = curr->slotno;
	depth = currdisk->depth[0];
	slotno.byte[depth] = slotno.byte[depth] >> 4;
	curr->time = 0.0;
	if (depth == 0) {
		intr_request ((event *)curr);
	} else {
		bus_deliver_event(busno.byte[depth], slotno.byte[depth], curr);
	}
}


/*
 * send completion up the line
 */
static void ssd_request_complete(ioreq_event *curr)
{
	//	printf("ssd_request_complete=>simtime: %lf, pid: %d, trace_time: %lf, blkno: %d, bcount: %d\n", simtime, curr->pid, curr->trace_time, curr->blkno, curr->bcount);
	ssd_t *currdisk;
	ioreq_event *x;

	// fprintf (outputfile, "Entering ssd_request_complete: %12.6f\n", simtime);

	currdisk = getssd (curr->devno);
	ssd_assert_current_activity(currdisk, curr);
	//	printf("ssd_request_complete\n");
	if(FAIRNESS_SCHEDULER == CFQ) {
		cfq_queue_remove_completed_request(currdisk->queue, curr);
	} else if(FAIRNESS_SCHEDULER == FIOS) {
		fios_queue_remove_completed_request(currdisk->queue, curr);
	} else if(FAIRNESS_SCHEDULER == WENDY) {
		wendy_queue_remove_completed_request(currdisk->queue, curr);
	} else if(FAIRNESS_SCHEDULER == ORACLE) {
		oracle_queue_remove_completed_request(currdisk->queue, curr);
	} else if(FAIRNESS_SCHEDULER == NOOP) {
		//	printf("ioqueue_physical_access_done\n");
		if ((x = ioqueue_physical_access_done(currdisk->queue,curr)) == NULL) {
			fprintf(stderr, "ssd_request_complete:  ioreq_event not found by ioqueue_physical_access_done call\n");
			exit(1);
		}
	} else {
		fprintf(stderr, "no fairness scheduler\n");
	}

	NCQ_command_size--;
	//	printf("ncq: %d simtime: %lf\n", NCQ_command_size, simtime);
	/* send completion interrupt */
	curr->type = IO_INTERRUPT_ARRIVE;
	curr->cause = COMPLETION;
	ssd_send_event_up_path(curr, currdisk->bus_transaction_latency);
	//	printf("ssd_request_complete end\n");
}

static void ssd_bustransfer_complete (ioreq_event *curr)
{
	// fprintf (outputfile, "Entering ssd_bustransfer_complete for disk %d: %12.6f\n", curr->devno, simtime);

	if (curr->flags & READ) {
		ssd_request_complete (curr);
	} else {
		ssd_t *currdisk = getssd (curr->devno);
		ssd_assert_current_activity(currdisk, curr);
		if (currdisk->neverdisconnect == FALSE) {
			/* disconnect from bus */
			ioreq_event *tmp = ioreq_copy (curr);
			tmp->type = IO_INTERRUPT_ARRIVE;
			tmp->cause = DISCONNECT;
			ssd_send_event_up_path (tmp, currdisk->bus_transaction_latency);
			ssd_media_access_request (curr);
		} else {
			ssd_media_access_request (curr);
			ssd_check_channel_activity (currdisk);
		}
	}
}

/*
 * returns the logical page number within an element given a block number as
 * issued by the file system
 */
int ssd_logical_pageno(int blkno, ssd_t *s)
{
	int apn;
	int lpn;

	// absolute page number is the block number as written by the above layer
	apn = blkno/s->params.page_size;

	// find the logical page number within the ssd element. we maintain the
	// mapping between the logical page number and the actual physical page
	// number. an alternative is that we could maintain the mapping between
	// apn we calculated above and the physical page number. but the range
	// of apn is several times bigger and so we chose to go with the mapping
	// b/w lpn --> physical page number
	lpn = ((apn - (apn % (s->params.element_stride_pages * s->params.nelements)))/
			s->params.nelements) + (apn % s->params.element_stride_pages);

	return lpn;
}

int ssd_already_present(ssd_req **reqs, int total, ioreq_event *req)
{
	int i;
	int found = 0;

	for (i = 0; i < total; i ++) {
		if ((req->blkno == reqs[i]->org_req->blkno) &&
				(req->flags == reqs[i]->org_req->flags)) {
			found = 1;
			break;
		}
	}

	return found;
}

double _ssd_invoke_element_cleaning(int elem_num, ssd_t *s)
{
	double clean_cost = ssd_clean_element(s, elem_num);
	return clean_cost;
}

static int ssd_invoke_element_cleaning(int elem_num, ssd_t *s)
{
	double max_cost = 0;
	int cleaning_invoked = 0;
	ssd_element *elem = &s->elements[elem_num];

	// element must be free
	ASSERT(elem->media_busy == FALSE);

	max_cost = _ssd_invoke_element_cleaning(elem_num, s);

	// cleaning was invoked on this element. we can start
	// the next operation on this elem only after the cleaning
	// gets over.
	if (max_cost > 0) {
		ioreq_event *tmp;

		elem->media_busy = 1;
		cleaning_invoked = 1;

		// we use the 'blkno' field to store the element number
		tmp = (ioreq_event *)getfromextraq();
		tmp->devno = s->devno;
		tmp->time = simtime + max_cost;
		tmp->blkno = elem_num;
		tmp->ssd_elem_num = elem_num;
		tmp->type = SSD_CLEAN_ELEMENT;
		tmp->flags = SSD_CLEAN_ELEMENT;
		tmp->busno = -1;
		tmp->bcount = -1;
		stat_update (&s->stat.acctimestats, max_cost);
		addtointq ((event *)tmp);

		// stat
		elem->stat.tot_clean_time += max_cost;
	}

	return cleaning_invoked;
}

/* modified by gengyouchen */
static void ssd_activate_elem(ssd_t *currdisk, int elem_num) {

	ASSERT(elem_num == 0);

	int num = ssd_waiting_pool_count();
	if (ssd_waiting_pool_count() > 0) {
		ssd_scheduler_issue();
	}
}

/* modified by gengyouchen */
static void ssd_media_access_request_element (ioreq_event *curr) {

	ssd_t *currdisk = getssd(curr->devno);

	ASSERT(curr != NULL);
	/* make sure requests are aligned with page granularity */
	ASSERT(curr->blkno % currdisk->params.page_size == 0);
	ASSERT(curr->bcount % currdisk->params.page_size == 0);

	/* parent request has a counter to count down the finished sectors */
	curr->tempint2 = curr->bcount;

	//	printf("ssd=> time: %lf, blkno: %d, bcount: %d\n", curr->time, curr->blkno, curr->bcount);

	int blkno = curr->blkno;
	int count = curr->bcount;
	while (count != 0) {

		/* create sub-request for each SSD page */
		ioreq_event *tmp = (ioreq_event *) getfromextraq();
		tmp->devno = curr->devno;
		tmp->busno = curr->busno;
		tmp->flags = curr->flags;
		tmp->blkno = blkno;
		tmp->opid = curr->opid;
		tmp->bcount = currdisk->params.page_size;
		tmp->type = DEVICE_ACCESS_COMPLETE;

		/* we don't care about original ssdsim elements for eclab simulator */
		tmp->ssd_elem_num = 0;

		/* point to the parent request (used for counting down finished sectors) */
		/* noticed that if the request is not from host, tempptr2 should be NULL */
		tmp->tempptr2 = curr;

		/* fill information used for eclab FTL */
		tmp->lpn = blkno / currdisk->params.page_size;
		tmp->parent_lpn = curr->blkno / currdisk->params.page_size;
		tmp->ppn = -1;
		tmp->arrival_time = simtime;
		tmp->access_type = (tmp->flags & READ) ? SSD_REQ_READ : SSD_REQ_PROGRAM;
		tmp->access_phase = SSD_REQ_CREATED;
		tmp->access_initiator = SSD_REQ_HOST;
		tmp->access_period = curr->access_period; // add by wendy
		tmp->next_waiting = (ioreq_event *) NULL;
		tmp->prev_waiting = (ioreq_event *) NULL;

		//		printf("ssd(small)=> time: %lf, blkno: %d, bcount: %d, flags: %d\n", tmp->arrival_time, tmp->blkno, tmp->bcount, tmp->flags); // wendy

		/* send sub-request to eclab FTL */
		if (tmp->flags & READ) {
			ssd_ftl_read(tmp);
		} else {
			ssd_ftl_write(tmp);
		}

		/* iterate address to the next sub-request */
		blkno += currdisk->params.page_size;
		count -= currdisk->params.page_size;

	}

	/* trigger SSD scheduler */
	ssd_activate_elem(currdisk, 0);
}

static void ssd_media_access_request (ioreq_event *curr)
{
	ssd_t *currdisk = getssd(curr->devno);

	//	printf("%d %lf %d %d %d %d, type=%d\n", curr->pid, simtime, curr->devno, curr->blkno, curr->bcount, curr->flags, curr->type);
	switch(currdisk->params.alloc_pool_logic) {
		case SSD_ALLOC_POOL_PLANE:
		case SSD_ALLOC_POOL_CHIP:
			ssd_media_access_request_element(curr);
			break;

		case SSD_ALLOC_POOL_GANG:
#if SYNC_GANG
			ssd_media_access_request_gang_sync(curr);
#else
			ssd_media_access_request_gang(curr);
#endif
			break;

		default:
			printf("Unknown alloc pool logic %d\n", currdisk->params.alloc_pool_logic);
			ASSERT(0);
	}
}

static void ssd_reconnect_done (ioreq_event *curr)
{
	ssd_t *currdisk;

	// fprintf (outputfile, "Entering ssd_reconnect_done for disk %d: %12.6f\n", curr->devno, simtime);

	currdisk = getssd (curr->devno);
	ssd_assert_current_activity(currdisk, curr);

	if (curr->flags & READ) {
		if (currdisk->neverdisconnect) {
			/* Just holding on to bus; data transfer will be initiated when */
			/* media access is complete.                                    */
			addtoextraq((event *) curr);
			ssd_check_channel_activity (currdisk);
		} else {
			/* data transfer: curr->bcount, which is still set to original */
			/* requested value, indicates how many blks to transfer.       */
			curr->type = DEVICE_DATA_TRANSFER_COMPLETE;
			ssd_send_event_up_path(curr, (double) 0.0);
		}

	} else {
		if (currdisk->reconnect_reason == DEVICE_ACCESS_COMPLETE) {
			ssd_request_complete (curr);

		} else {
			/* data transfer: curr->bcount, which is still set to original */
			/* requested value, indicates how many blks to transfer.       */
			curr->type = DEVICE_DATA_TRANSFER_COMPLETE;
			ssd_send_event_up_path(curr, (double) 0.0);
		}
	}
}

static void ssd_request_arrive (ioreq_event *curr)
{
	ssd_t *currdisk;
	int enter_time;
	//	printf("ssd_request_arrive: %lf blkno: %d trace_time: %lf pid: %d\n", simtime, curr->blkno, curr->trace_time, curr->pid);
	//	printf("==========ssd_request_arrive begin==========\n");
	// fprintf (outputfile, "Entering ssd_request_arrive: %12.6f\n", simtime);
	// fprintf (outputfile, "ssd = %d, blkno = %d, bcount = %d, read = %d\n",curr->devno, curr->blkno, curr->bcount, (READ & curr->flags));

	currdisk = getssd(curr->devno);

	/* verify that request is valid. */
	if ((curr->blkno < 0) || (curr->bcount <= 0) || ((curr->blkno + curr->bcount) > currdisk->numblocks)) {
		fprintf(stderr, "Invalid set of blocks requested from ssd - blkno %d, bcount %d, numblocks %d\n", curr->blkno, curr->bcount, currdisk->numblocks);
		exit(1);
	}

	/* create a new request, set it up for initial interrupt */
	//	printf("ssd_request_arrive\n");
	if(curr->pid != FAKE_EVENT) {
		if(FAIRNESS_SCHEDULER == CFQ) {
			cfq_queue_add_new_request(currdisk->queue, curr);
		} else if(FAIRNESS_SCHEDULER == FIOS) {
			fios_queue_add_new_request(currdisk->queue, curr);
		} else if(FAIRNESS_SCHEDULER == WENDY) {
			wendy_queue_add_new_request(currdisk->queue, curr);
		} else if(FAIRNESS_SCHEDULER == ORACLE) {
			oracle_queue_add_new_request(currdisk->queue, curr);
		} else if(FAIRNESS_SCHEDULER == NOOP) {
			ioqueue_add_new_request(currdisk->queue, curr);
		} else {
			fprintf(stderr, "no fairness scheduler\n");
		}
	} else { // remove the fake request from iodriver queue and overallqueue
		curr->type = IO_INTERRUPT_ARRIVE;
		curr->cause = COMPLETION;
		ioqueue_physical_access_done(iodrivers[0]->devices[0].queue, curr);
		logorg_mapcomplete(sysorgs, numsysorgs, curr);
		ioreq_event *temp = ioqueue_get_specific_request (overallqueue, curr);
		ioreq_event *temp2 = ioqueue_physical_access_done (overallqueue, temp);
		ASSERT (temp2 != NULL);
		addtoextraq((event *)temp);
		temp = NULL;
	}
	//	if (currdisk->channel_activity == NULL) {
	//	printf("ssd_request_arrive\n");
	enter_time = 0;
	while(NCQ_command_size < NCQ_COMMAND_SIZE_BOUND) {
		curr = NULL;
		if(NCQ_command_size < NCQ_COMMAND_SIZE_BOUND) {
			enter_time++;
			if(FAIRNESS_SCHEDULER == CFQ) {
				curr = cfq_queue_get_next_request(currdisk->queue);
			} else if(FAIRNESS_SCHEDULER == FIOS) {
				curr = fios_queue_get_next_request(currdisk->queue, enter_time);
			} else if(FAIRNESS_SCHEDULER == WENDY) {
				curr = wendy_queue_get_next_request(currdisk->queue, enter_time);
			} else if(FAIRNESS_SCHEDULER == ORACLE) {
				curr = oracle_queue_get_next_request(currdisk->queue);
			} else if(FAIRNESS_SCHEDULER == NOOP) {
				curr = ioqueue_get_next_request(currdisk->queue);
			} else {
				fprintf(stderr, "no fairness scheduler\n");
			}
		}
		if(curr != NULL){
			generate_fake_event2();
		}
		currdisk->channel_activity = curr;

		if(curr != NULL) { // add by wendy
			NCQ_command_size++; // wendy
			//			printf("ncq: %d simtime: %lf\n", NCQ_command_size, simtime);
			currdisk->busowned = ssd_get_busno(curr);
			currdisk->reconnect_reason = IO_INTERRUPT_ARRIVE;

			if (curr->flags & READ) {
				if (!currdisk->neverdisconnect) {
					ssd_media_access_request (ioreq_copy(curr));
					curr->cause = DISCONNECT;
					curr->type = IO_INTERRUPT_ARRIVE;
					ssd_send_event_up_path(curr, currdisk->bus_transaction_latency);
				} else { // enter
					ssd_media_access_request (curr);
					ssd_check_channel_activity(currdisk);
				}
			} else {
				curr->cause = READY_TO_TRANSFER;
				curr->type = IO_INTERRUPT_ARRIVE;
				ssd_send_event_up_path(curr, currdisk->bus_transaction_latency);
			}
		} else {
			break;
		}

		if(FAIRNESS_SCHEDULER != WENDY) {
			//break;
		}

	}
	//	}
	//	printf("==========ssd_request_arrive end==========\n");
}

/*
 * cleaning in an element is over.
 */
static void ssd_clean_element_complete(ioreq_event *curr)
{
	ssd_t *currdisk;
	int elem_num;

	currdisk = getssd (curr->devno);
	elem_num = curr->ssd_elem_num;
	ASSERT(currdisk->elements[elem_num].media_busy == TRUE);

	// release this event
	addtoextraq((event *) curr);

	// activate the gang to serve the next set of requests
	currdisk->elements[elem_num].media_busy = 0;
	ssd_activate_elem(currdisk, elem_num);
}

void ssd_complete_parent(ioreq_event *curr, ssd_t *currdisk)
{
	//	printf("ssd_complete_parent\n");
	ioreq_event *parent;

	/* **** CAREFUL ... HIJACKING tempint2 and tempptr2 fields here **** */
	parent = curr->tempptr2;
	parent->tempint2 -= curr->bcount;

	if (parent->tempint2 == 0) {
		ioreq_event *prev;

		// assert(parent != currdisk->channel_activity); // wendy
		prev = currdisk->completion_queue;
		if (prev == NULL) {
			currdisk->completion_queue = parent;
			parent->next = prev;
		} else {
			while (prev->next != NULL)
				prev = prev->next;
			parent->next = prev->next;
			prev->next = parent;
		}
		//		if (currdisk->channel_activity == NULL) {
		ssd_check_channel_activity (currdisk);
		//		}
	}
}

/* modified by gengyouchen */
static void ssd_access_complete_element(ioreq_event *curr) {

	ssd_t *currdisk = getssd(curr->devno);

	ASSERT(curr->ssd_elem_num == 0);

	switch (curr->access_phase) {

		case SSD_REQ_INPUTTED:
			ssd_fsm_on_event_fired(curr);
			ssd_waiting_pool_insert(curr);
			break;

		case SSD_REQ_STARTED:
			ssd_fsm_on_event_fired(curr);
			curr->access_phase = SSD_REQ_FINISHED;
			switch (curr->access_type) {
				case SSD_REQ_READ:
					curr->time = simtime + currdisk->params.page_read_latency;
					break;
				case SSD_REQ_PROGRAM:
					curr->time = simtime + currdisk->params.page_write_latency;
					break;
				case SSD_REQ_ERASE:
					curr->time = simtime + currdisk->params.block_erase_latency;
					break;
				default:
					ASSERT(0);
					break;
			}
			ssd_fsm_on_event_setup(curr);
			addtointq((event *) curr);
			break;

		case SSD_REQ_FINISHED:
			ssd_fsm_on_event_fired(curr);
			ssd_waiting_pool_insert(curr);
			break;

		case SSD_REQ_OUTPUTTED:
			ssd_fsm_on_event_fired(curr);
			if (curr->tempptr2) {
				ssd_complete_parent(curr, currdisk);
			}
			curr->access_phase = SSD_REQ_DESTROYED;
			curr->time = simtime;
			ssd_fsm_on_event_destroyed(curr);
			addtoextraq((event *) curr);
			break;

		default:
			ASSERT(0);
			break;
	}

	ssd_activate_elem(currdisk, 0);
}

static void ssd_access_complete(ioreq_event *curr)
{
	ssd_t *currdisk = getssd (curr->devno);;

	switch(currdisk->params.alloc_pool_logic) {
		case SSD_ALLOC_POOL_PLANE:
		case SSD_ALLOC_POOL_CHIP:
			ssd_access_complete_element(curr);
			break;

		case SSD_ALLOC_POOL_GANG:
#if SYNC_GANG
			ssd_access_complete_gang_sync(curr);
#else
			ssd_access_complete_gang(curr);
#endif
			break;

		default:
			printf("Unknown alloc pool logic %d\n", currdisk->params.alloc_pool_logic);
			ASSERT(0);
	}
}

/* intermediate disconnect done */
static void ssd_disconnect_done (ioreq_event *curr)
{
	ssd_t *currdisk;

	currdisk = getssd (curr->devno);
	ssd_assert_current_activity(currdisk, curr);

	// fprintf (outputfile, "Entering ssd_disconnect for disk %d: %12.6f\n", currdisk->devno, simtime);

	addtoextraq((event *) curr);

	if (currdisk->busowned != -1) {
		bus_ownership_release(currdisk->busowned);
		currdisk->busowned = -1;
	}
	ssd_check_channel_activity (currdisk);
}

/* completion disconnect done */
static void ssd_completion_done (ioreq_event *curr)
{
	ssd_t *currdisk = getssd (curr->devno);
	ssd_assert_current_activity(currdisk, curr);

	// fprintf (outputfile, "Entering ssd_completion for disk %d: %12.6f\n", currdisk->devno, simtime);

	addtoextraq((event *) curr);

	if (currdisk->busowned != -1) {
		bus_ownership_release(currdisk->busowned);
		currdisk->busowned = -1;
	}

	ssd_check_channel_activity (currdisk);
}

static void ssd_interrupt_complete (ioreq_event *curr)
{
	// fprintf (outputfile, "Entered ssd_interrupt_complete - cause %d\n", curr->cause);

	switch (curr->cause) {

		case RECONNECT:
			ssd_reconnect_done(curr);
			break;

		case DISCONNECT:
			ssd_disconnect_done(curr);
			break;

		case COMPLETION:
			ssd_completion_done(curr);
			break;

		default:
			ddbg_assert2(0, "bad event type");
	}
}


void ssd_event_arrive (ioreq_event *curr)
{
	ssd_t *currdisk;
	//	printf("ssd_event_arrive: %lf, blkno: %d, bcount: %d, trace_time: %lf\n", simtime, curr->blkno, curr->bcount, curr->trace_time);
	// fprintf (outputfile, "Entered ssd_event_arrive: time %f (simtime %f)\n", curr->time, simtime);
	// fprintf (outputfile, " - devno %d, blkno %d, type %d, cause %d, read = %d\n", curr->devno, curr->blkno, curr->type, curr->cause, curr->flags & READ);

	//	printf("enter ssd_event_arrive: pid=%d, time=%lf(simtime=%lf), devno=%d, blkno=%d, bcount=%d, flags=%d, type=%d\n", curr->pid, curr->time, simtime, curr->devno, curr->blkno, curr->bcount, curr->flags, curr->type);

	currdisk = getssd (curr->devno);

	switch (curr->type) {
		case IO_ACCESS_ARRIVE: // 101
			//			printf("IO_ACCESS_ARRIVE\n");
			curr->time = simtime + currdisk->overhead;
			curr->type = DEVICE_OVERHEAD_COMPLETE;
			addtointq((event *) curr);
			break;

		case DEVICE_OVERHEAD_COMPLETE: // 106
			//			printf("DEVICE_OVERHEAD_COMPLETE\n");
			ssd_request_arrive(curr);
			break;

		case DEVICE_ACCESS_COMPLETE: // 107
			//			printf("DEVICE_ACCESS_COMPLETE\n");
			ssd_access_complete (curr);
			break;

		case DEVICE_DATA_TRANSFER_COMPLETE: // 109
			//			printf("DEVICE_DATA_TRANSFER_COMPLETE\n");
			ssd_bustransfer_complete(curr);
			break;

		case IO_INTERRUPT_COMPLETE: // 105
			//			printf("IO_INTERRUPT_COMPLETE\n");
			ssd_interrupt_complete(curr);
			break;

		case IO_QLEN_MAXCHECK: // 120
			//			printf("IO_QLEN_MAXCHECK\n");
			/* Used only at initialization time to set up queue stuff */
			curr->tempint1 = -1;
			curr->tempint2 = ssd_get_maxoutstanding(curr->devno);
			curr->bcount = 0;
			break;

		case SSD_CLEAN_GANG: // 302
			//			printf("SSD_CLEAN_GANG\n");
			ssd_clean_gang_complete(curr);
			break;

		case SSD_CLEAN_ELEMENT: // 301
			//			printf("SSD_CLEAN_ELEMENT\n");
			ssd_clean_element_complete(curr);
			break;

		default:
			fprintf(stderr, "Unrecognized event type at ssd_event_arrive\n");
			exit(1);
	}

	// fprintf (outputfile, "Exiting ssd_event_arrive\n");
}


long long int ssd_get_number_of_blocks (int devno)
{
	ssd_t *currdisk = getssd (devno);
	return (currdisk->numblocks);
}


int ssd_get_numcyls (int devno)
{
	ssd_t *currdisk = getssd (devno);
	return (currdisk->numblocks);
}


void ssd_get_mapping (int maptype, int devno, int blkno, int *cylptr, int *surfaceptr, int *blkptr)
{
	ssd_t *currdisk = getssd (devno);

	if ((blkno < 0) || (blkno >= currdisk->numblocks)) {
		fprintf(stderr, "Invalid blkno at ssd_get_mapping: %d\n", blkno);
		exit(1);
	}

	if (cylptr) {
		*cylptr = blkno;
	}
	if (surfaceptr) {
		*surfaceptr = 0;
	}
	if (blkptr) {
		*blkptr = 0;
	}
}


int ssd_get_avg_sectpercyl (int devno)
{
	return (1);
}


int ssd_get_distance (int devno, ioreq_event *req, int exact, int direction)
{
	/* just return an arbitrary constant, since acctime is constant */
	return 1;
}


// returning 0 to remove warning
double  ssd_get_servtime (int devno, ioreq_event *req, int checkcache, double maxtime)
{
	fprintf(stderr, "device_get_seektime not supported for ssd devno %d\n",  devno);
	assert(0);
	return 0;
}


// returning 0 to remove warning
double  ssd_get_acctime (int devno, ioreq_event *req, double maxtime)
{
	fprintf(stderr, "device_get_seektime not supported for ssd devno %d\n",  devno);
	assert(0);
	return 0;
}


int ssd_get_numdisks (void)
{
	return(numssds);
}


void ssd_cleanstats (void)
{
	int i, j;

	for (i=0; i<MAXDEVICES; i++) {
		ssd_t *currdisk = getssd (i);
		if (currdisk) {
			ioqueue_cleanstats(currdisk->queue);
			for (j=0; j<currdisk->params.nelements; j++)
				ioqueue_cleanstats(currdisk->elements[j].queue);
		}
	}
}

void ssd_setcallbacks ()
{
	ioqueue_setcallbacks();
}

int ssd_add(struct ssd *d) {
	int c;

	if(!disksim->ssdinfo) ssd_initialize_diskinfo();

	for(c = 0; c < disksim->ssdinfo->ssds_len; c++) {
		if(!disksim->ssdinfo->ssds[c]) {
			disksim->ssdinfo->ssds[c] = d;
			numssds++;
			return c;
		}
	}

	/* note that numdisks must be equal to diskinfo->disks_len */
	disksim->ssdinfo->ssds =
		realloc(disksim->ssdinfo->ssds,
				2 * c * sizeof(struct ssd *));

	bzero(disksim->ssdinfo->ssds + numssds,
			numssds);

	disksim->ssdinfo->ssds[c] = d;
	numssds++;
	disksim->ssdinfo->ssds_len *= 2;
	return c;
}


struct ssd *ssdmodel_ssd_loadparams(struct lp_block *b, int *num)
{
	/* temp vars for parameters */
	int n;
	struct ssd *result;

	if(!disksim->ssdinfo) ssd_initialize_diskinfo();

	result = malloc(sizeof(struct ssd));
	if(!result) return 0;
	bzero(result, sizeof(struct ssd));

	n = ssd_add(result);

	result->hdr = ssd_hdr_initializer;
	if(b->name)
		result->hdr.device_name = _strdup(b->name);

	lp_loadparams(result, b, &ssdmodel_ssd_mod);

	device_add((struct device_header *)result, n);
	if (num != NULL)
		*num = n;
	return result;
}


struct ssd *ssd_copy(struct ssd *orig) {
	int i;
	struct ssd *result = malloc(sizeof(struct ssd));
	bzero(result, sizeof(struct ssd));
	memcpy(result, orig, sizeof(struct ssd));
	result->queue = ioqueue_copy(orig->queue);
	for (i=0;i<orig->params.nelements;i++)
		result->elements[i].queue = ioqueue_copy(orig->elements[i].queue);
	return result;
}

void ssd_set_syncset (int setstart, int setend)
{
}


static void ssd_acctime_printstats (int *set, int setsize, char *prefix)
{
	int i;
	statgen * statset[MAXDEVICES];

	if (device_printacctimestats) {
		for (i=0; i<setsize; i++) {
			ssd_t *currdisk = getssd (set[i]);
			statset[i] = &currdisk->stat.acctimestats;
		}
		stat_print_set(statset, setsize, prefix);
	}
}


static void ssd_other_printstats (int *set, int setsize, char *prefix)
{
	int i;
	int numbuswaits = 0;
	double waitingforbus = 0.0;

	for (i=0; i<setsize; i++) {
		ssd_t *currdisk = getssd (set[i]);
		numbuswaits += currdisk->stat.numbuswaits;
		waitingforbus += currdisk->stat.waitingforbus;
	}

	fprintf(outputfile, "%sTotal bus wait time: %f\n", prefix, waitingforbus);
	fprintf(outputfile, "%sNumber of bus waits: %d\n", prefix, numbuswaits);
}

void ssd_print_block_lifetime_distribution(int elem_num, ssd_t *s, int ssdno, double avg_lifetime, char *sourcestr)
{
	const int bucket_size = 20;
	int no_buckets = (100/bucket_size + 1);
	int i;
	int *hist;
	int dead_blocks = 0;
	int n;
	double sum;
	double sum_sqr;
	double mean;
	double variance;
	ssd_element_metadata *metadata = &(s->elements[elem_num].metadata);

	// allocate the buckets
	hist = (int *) malloc(no_buckets * sizeof(int));
	memset(hist, 0, no_buckets * sizeof(int));

	// to calc the variance
	n = s->params.blocks_per_element;
	sum = 0;
	sum_sqr = 0;

	for (i = 0; i < s->params.blocks_per_element; i ++) {
		int bucket;
		int rem_lifetime = metadata->block_usage[i].rem_lifetime;
		double perc = (rem_lifetime * 100.0) / avg_lifetime;

		// find out how many blocks have completely been erased.
		if (metadata->block_usage[i].rem_lifetime == 0) {
			dead_blocks ++;
		}

		if (perc >= 100) {
			// this can happen if a particular block was not
			// cleaned at all and so its remaining life time
			// is greater than the average life time. put these
			// blocks in the last bucket.
			bucket = no_buckets - 1;
		} else {
			bucket = (int) perc / bucket_size;
		}

		hist[bucket] ++;

		// calculate the variance
		sum = sum + rem_lifetime;
		sum_sqr = sum_sqr + (rem_lifetime*rem_lifetime);
	}


	fprintf(outputfile, "%s #%d elem #%d   ", sourcestr, ssdno, elem_num);
	fprintf(outputfile, "Block Lifetime Distribution\n");

	// print the bucket size
	fprintf(outputfile, "%s #%d elem #%d   ", sourcestr, ssdno, elem_num);
	for (i = bucket_size; i <= 100; i += bucket_size) {
		fprintf(outputfile, "< %d\t", i);
	}
	fprintf(outputfile, ">= 100\t\n");

	// print the histogram bar lengths
	fprintf(outputfile, "%s #%d elem #%d   ", sourcestr, ssdno, elem_num);
	for (i = bucket_size; i <= 100; i += bucket_size) {
		fprintf(outputfile, "%d\t", hist[i/bucket_size - 1]);
	}
	fprintf(outputfile, "%d\t\n", hist[no_buckets - 1]);

	mean = sum/n;
	variance = (sum_sqr - sum*mean)/(n - 1);
	fprintf(outputfile, "%s #%d elem #%d   Average of life time:\t%f\n",
			sourcestr, ssdno, elem_num, mean);
	fprintf(outputfile, "%s #%d elem #%d   Variance of life time:\t%f\n",
			sourcestr, ssdno, elem_num, variance);
	fprintf(outputfile, "%s #%d elem #%d   Total dead blocks:\t%d\n",
			sourcestr, ssdno, elem_num, dead_blocks);
}

//prints the cleaning algo statistics
void ssd_printcleanstats(int *set, int setsize, char *sourcestr)
{
	int i;
	int tot_ssd = 0;
	int elts_count = 0;
	double iops = 0;

	fprintf(outputfile, "\n\nSSD CLEANING STATISTICS\n");
	fprintf(outputfile, "---------------------------------------------\n\n");
	for (i = 0; i < setsize; i ++) {
		int j;
		int tot_elts = 0;
		ssd_t *s = getssd(set[i]);

		if (s->params.write_policy == DISKSIM_SSD_WRITE_POLICY_OSR) {

			elts_count += s->params.nelements;

			for (j = 0; j < s->params.nelements; j ++) {
				int plane_num;
				double avg_lifetime;
				double elem_iops = 0;
				double elem_clean_iops = 0;

				ssd_element_stat *stat = &(s->elements[j].stat);

				avg_lifetime = ssd_compute_avg_lifetime(-1, j, s);

				fprintf(outputfile, "%s #%d elem #%d   Total reqs issued:\t%d\n",
						sourcestr, set[i], j, s->elements[j].stat.tot_reqs_issued);
				fprintf(outputfile, "%s #%d elem #%d   Total time taken:\t%f\n",
						sourcestr, set[i], j, s->elements[j].stat.tot_time_taken);
				if (s->elements[j].stat.tot_time_taken > 0) {
					elem_iops = ((s->elements[j].stat.tot_reqs_issued*1000.0)/s->elements[j].stat.tot_time_taken);
					fprintf(outputfile, "%s #%d elem #%d   IOPS:\t%f\n",
							sourcestr, set[i], j, elem_iops);
				}

				fprintf(outputfile, "%s #%d elem #%d   Total cleaning reqs issued:\t%d\n",
						sourcestr, set[i], j, s->elements[j].stat.num_clean);
				fprintf(outputfile, "%s #%d elem #%d   Total cleaning time taken:\t%f\n",
						sourcestr, set[i], j, s->elements[j].stat.tot_clean_time);
				fprintf(outputfile, "%s #%d elem #%d   Total migrations:\t%d\n",
						sourcestr, set[i], j, s->elements[j].metadata.tot_migrations);
				fprintf(outputfile, "%s #%d elem #%d   Total pages migrated:\t%d\n",
						sourcestr, set[i], j, s->elements[j].metadata.tot_pgs_migrated);
				fprintf(outputfile, "%s #%d elem #%d   Total migrations cost:\t%f\n",
						sourcestr, set[i], j, s->elements[j].metadata.mig_cost);


				if (s->elements[j].stat.tot_clean_time > 0) {
					elem_clean_iops = ((s->elements[j].stat.num_clean*1000.0)/s->elements[j].stat.tot_clean_time);
					fprintf(outputfile, "%s #%d elem #%d   clean IOPS:\t%f\n",
							sourcestr, set[i], j, elem_clean_iops);
				}

				fprintf(outputfile, "%s #%d elem #%d   Overall IOPS:\t%f\n",
						sourcestr, set[i], j, ((s->elements[j].stat.num_clean+s->elements[j].stat.tot_reqs_issued)*1000.0)/(s->elements[j].stat.tot_clean_time+s->elements[j].stat.tot_time_taken));

				iops += elem_iops;

				fprintf(outputfile, "%s #%d elem #%d   Number of free blocks:\t%d\n",
						sourcestr, set[i], j, s->elements[j].metadata.tot_free_blocks);
				fprintf(outputfile, "%s #%d elem #%d   Number of cleans:\t%d\n",
						sourcestr, set[i], j, stat->num_clean);
				fprintf(outputfile, "%s #%d elem #%d   Pages moved:\t%d\n",
						sourcestr, set[i], j, stat->pages_moved);
				fprintf(outputfile, "%s #%d elem #%d   Total xfer time:\t%f\n",
						sourcestr, set[i], j, stat->tot_xfer_cost);
				if (stat->tot_xfer_cost > 0) {
					fprintf(outputfile, "%s #%d elem #%d   Xfer time per page:\t%f\n",
							sourcestr, set[i], j, stat->tot_xfer_cost/(1.0*stat->pages_moved));
				} else {
					fprintf(outputfile, "%s #%d elem #%d   Xfer time per page:\t0\n",
							sourcestr, set[i], j);
				}
				fprintf(outputfile, "%s #%d elem #%d   Average lifetime:\t%f\n",
						sourcestr, set[i], j, avg_lifetime);
				fprintf(outputfile, "%s #%d elem #%d   Plane Level Statistics\n",
						sourcestr, set[i], j);
				fprintf(outputfile, "%s #%d elem #%d   ", sourcestr, set[i], j);
				for (plane_num = 0; plane_num < s->params.planes_per_pkg; plane_num ++) {
					fprintf(outputfile, "%d:(%d)  ",
							plane_num, s->elements[j].metadata.plane_meta[plane_num].num_cleans);
				}
				fprintf(outputfile, "\n");


				ssd_print_block_lifetime_distribution(j, s, set[i], avg_lifetime, sourcestr);
				fprintf(outputfile, "\n");

				tot_elts += stat->pages_moved;
			}

			//fprintf(outputfile, "%s SSD %d average # of pages moved per element %d\n",
			//  sourcestr, set[i], tot_elts / s->params.nelements);

			tot_ssd += tot_elts;
			fprintf(outputfile, "\n");
		}
	}

	if (elts_count > 0) {
		fprintf(outputfile, "%s   Total SSD IOPS:\t%f\n",
				sourcestr, iops);
		fprintf(outputfile, "%s   Average SSD element IOPS:\t%f\n",
				sourcestr, iops/elts_count);
	}

	//fprintf(outputfile, "%s SSD average # of pages moved per ssd %d\n\n",
	//  sourcestr, tot_ssd / setsize);
}

void ssd_printsetstats (int *set, int setsize, char *sourcestr)
{
	int i;
	struct ioq * queueset[MAXDEVICES*SSD_MAX_ELEMENTS];
	int queuecnt = 0;
	int reqcnt = 0;
	char prefix[80];

	//using more secure functions
	sprintf_s4(prefix, 80, "%sssd ", sourcestr);
	for (i=0; i<setsize; i++) {
		ssd_t *currdisk = getssd (set[i]);
		struct ioq *q = currdisk->queue;
		queueset[queuecnt] = q;
		queuecnt++;
		reqcnt += ioqueue_get_number_of_requests(q);
	}
	if (reqcnt == 0) {
		fprintf (outputfile, "\nNo ssd requests for members of this set\n\n");
		return;
	}
	ioqueue_printstats(queueset, queuecnt, prefix);

	ssd_acctime_printstats(set, setsize, prefix);
	ssd_other_printstats(set, setsize, prefix);
}


void ssd_printstats (void)
{
	struct ioq * queueset[MAXDEVICES*SSD_MAX_ELEMENTS];
	int set[MAXDEVICES];
	int i,j;
	int reqcnt = 0;
	char prefix[80];
	int diskcnt;
	int queuecnt;

	fprintf(outputfile, "\nSSD STATISTICS\n");
	fprintf(outputfile, "---------------------\n\n");

	sprintf_s3(prefix, 80, "ssd ");

	diskcnt = 0;
	queuecnt = 0;
	for (i=0; i<MAXDEVICES; i++) {
		ssd_t *currdisk = getssd (i);
		if (currdisk) {
			struct ioq *q = currdisk->queue;
			queueset[queuecnt] = q;
			queuecnt++;
			reqcnt += ioqueue_get_number_of_requests(q);
			diskcnt++;
		}
	}
	assert (diskcnt == numssds);

	if (reqcnt == 0) {
		fprintf(outputfile, "No ssd requests encountered\n");
		return;
	}

	ioqueue_printstats(queueset, queuecnt, prefix);

	diskcnt = 0;
	for (i=0; i<MAXDEVICES; i++) {
		ssd_t *currdisk = getssd (i);
		if (currdisk) {
			set[diskcnt] = i;
			diskcnt++;
		}
	}
	assert (diskcnt == numssds);

	ssd_acctime_printstats(set, numssds, prefix);
	ssd_other_printstats(set, numssds, prefix);

	ssd_printcleanstats(set, numssds, prefix);

	fprintf (outputfile, "\n\n");

	for (i=0; i<numssds; i++) {
		ssd_t *currdisk = getssd (set[i]);
		if (currdisk->printstats == FALSE) {
			continue;
		}
		reqcnt = 0;
		{
			struct ioq *q = currdisk->queue;
			reqcnt += ioqueue_get_number_of_requests(q);
		}
		if (reqcnt == 0) {
			fprintf(outputfile, "No requests for ssd #%d\n\n\n", set[i]);
			continue;
		}
		fprintf(outputfile, "ssd #%d:\n\n", set[i]);
		sprintf_s4(prefix, 80, "ssd #%d ", set[i]);
		{
			struct ioq *q;
			q = currdisk->queue;
			ioqueue_printstats(&q, 1, prefix);
		}
		for (j=0;j<currdisk->params.nelements;j++) {
			char pprefix[100];
			struct ioq *q;
			sprintf_s5(pprefix, 100, "%s elem #%d ", prefix, j);
			q = currdisk->elements[j].queue;
			ioqueue_printstats(&q, 1, pprefix);
		}
		ssd_acctime_printstats(&set[i], 1, prefix);
		ssd_other_printstats(&set[i], 1, prefix);
		fprintf (outputfile, "\n\n");
	}
}

// returning 0 to remove warning
double ssd_get_seektime (int devno,
		ioreq_event *req,
		int checkcache,
		double maxtime)
{
	fprintf(stderr, "device_get_seektime not supported for ssd devno %d\n",  devno);
	assert(0);
	return 0;
}

/* default ssd dev header */
struct device_header ssd_hdr_initializer = {
	DEVICETYPE_SSD,
	sizeof(struct ssd),
	"unnamed ssd",
	(void *)ssd_copy,
	ssd_set_depth,
	ssd_get_depth,
	ssd_get_inbus,
	ssd_get_busno,
	ssd_get_slotno,
	ssd_get_number_of_blocks,
	ssd_get_maxoutstanding,
	ssd_get_numcyls,
	ssd_get_blktranstime,
	ssd_get_avg_sectpercyl,
	ssd_get_mapping,
	ssd_event_arrive,
	ssd_get_distance,
	ssd_get_servtime,
	ssd_get_seektime,
	ssd_get_acctime,
	ssd_bus_delay_complete,
	ssd_bus_ownership_grant
};
