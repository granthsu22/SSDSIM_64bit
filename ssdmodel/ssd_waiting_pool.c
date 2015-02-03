#include "ssd_waiting_pool.h"

void ssd_waiting_pool_push_back(ioreq_event *curr);
void ssd_waiting_pool_push_front(ioreq_event *curr);

int waiting_count = 0;
ioreq_event *waiting_head = (ioreq_event *) NULL;
ioreq_event *waiting_tail = (ioreq_event *) NULL;

int ssd_waiting_pool_count() {
	return waiting_count;
}

ioreq_event *ssd_waiting_pool_front() {
	return waiting_head;
}

ioreq_event *ssd_waiting_pool_back() {
	return waiting_tail;
}

void ssd_waiting_pool_insert(ioreq_event *curr) {
	ASSERT(curr->ssd_elem_num == 0);

	if (waiting_count == 0) {

		ASSERT(waiting_head == (ioreq_event *) NULL);
		ASSERT(waiting_tail == (ioreq_event *) NULL);

		curr->prev_waiting = (ioreq_event *) NULL;
		curr->next_waiting = (ioreq_event *) NULL;
		waiting_head = curr;
		waiting_tail = curr;

	} else if (curr->arrival_time <= waiting_head->arrival_time) {

		ASSERT(waiting_head != (ioreq_event *) NULL);
		ASSERT(waiting_tail != (ioreq_event *) NULL);

		waiting_head->prev_waiting = curr;
		curr->prev_waiting = (ioreq_event *) NULL;
		curr->next_waiting = waiting_head;
		waiting_head = curr;

	} else if (curr->arrival_time >= waiting_tail->arrival_time) {

		ASSERT(waiting_head != (ioreq_event *) NULL);
		ASSERT(waiting_tail != (ioreq_event *) NULL);

		waiting_tail->next_waiting = curr;
		curr->prev_waiting = waiting_tail;
		curr->next_waiting = (ioreq_event *) NULL;
		waiting_tail = curr;

	} else {
		
		ioreq_event *index = waiting_head;
		while (index->arrival_time < curr->arrival_time) {
			index = index->next_waiting;
		}
		
		ASSERT(index->arrival_time >= curr->arrival_time);
		ASSERT(index->prev_waiting != (ioreq_event *) NULL);

		curr->prev_waiting = index->prev_waiting;
		curr->next_waiting = index;

		curr->prev_waiting->next_waiting = curr;
		curr->next_waiting->prev_waiting = curr;
	}

	waiting_count++;
//	printf("waiting: %d simtime: %lf\n", waiting_count, simtime);
}

void ssd_waiting_pool_push_front(ioreq_event *curr) {

	ASSERT(curr->ssd_elem_num == 0);

	if (waiting_count > 0) {

		/* waiting pool already has some requests */
		ASSERT(waiting_head != (ioreq_event *) NULL);
		ASSERT(waiting_tail != (ioreq_event *) NULL);

		/* add current request to the head of waiting pool */
		waiting_head->prev_waiting = curr;
		curr->prev_waiting = (ioreq_event *) NULL;
		curr->next_waiting = waiting_head;
		waiting_head = curr;

	} else {

		/* waiting pool is empty */
		ASSERT(waiting_head == (ioreq_event *) NULL);
		ASSERT(waiting_tail == (ioreq_event *) NULL);

		/* current request will be the only one request in waiting pool */
		curr->prev_waiting = (ioreq_event *) NULL;
		curr->next_waiting = (ioreq_event *) NULL;
		waiting_head = curr;
		waiting_tail = curr;

	}

	waiting_count++;
//	printf("waiting: %d simtime: %lf\n", waiting_count, simtime);
}

void ssd_waiting_pool_push_back(ioreq_event *curr) {

	ASSERT(curr->ssd_elem_num == 0);

	waiting_pool_die_condition[get_die_number(curr->lpn)]++;

	if (waiting_count > 0) {

		/* waiting pool already has some requests */
		ASSERT(waiting_head != (ioreq_event *) NULL);
		ASSERT(waiting_tail != (ioreq_event *) NULL);

		/* add current request to the tail of waiting pool */
		waiting_tail->next_waiting = curr;
		curr->prev_waiting = waiting_tail;
		curr->next_waiting = (ioreq_event *) NULL;
		waiting_tail = curr;

	} else {

		/* waiting pool is empty */
		ASSERT(waiting_head == (ioreq_event *) NULL);
		ASSERT(waiting_tail == (ioreq_event *) NULL);

		/* current request will be the only one request in waiting pool */
		curr->prev_waiting = (ioreq_event *) NULL;
		curr->next_waiting = (ioreq_event *) NULL;
		waiting_head = curr;
		waiting_tail = curr;
	}

	waiting_count++;
//	printf("waiting: %d simtime: %lf\n", waiting_count, simtime);
}

void ssd_waiting_pool_erase(ioreq_event *curr) {

	waiting_pool_die_condition[get_die_number(curr->lpn)]--;

	ASSERT(curr->ssd_elem_num == 0);

	/* waiting pool must has some requests */
	ASSERT(waiting_count > 0);
	ASSERT(waiting_head != (ioreq_event *) NULL);
	ASSERT(waiting_tail != (ioreq_event *) NULL);

	if (!curr->prev_waiting && !curr->next_waiting) {

		/* this is the only one request in waiting pool */
		ASSERT(waiting_count == 1);
		ASSERT(waiting_head == curr);
		ASSERT(waiting_tail == curr);

		/* simply clear waiting pool */
		waiting_head = (ioreq_event *) NULL;
		waiting_tail = (ioreq_event *) NULL;

	} else if (!curr->prev_waiting) {

		/* more than one request in waiting pool */
		/* this is the first request in waiting pool */
		ASSERT(waiting_count > 1);
		ASSERT(waiting_head == curr);
		ASSERT(waiting_tail != curr);

		/* let the next request be the first request in waiting pool */
		waiting_head = curr->next_waiting;
		waiting_head->prev_waiting = (ioreq_event *) NULL;

		/* delete connection with next request */
		curr->next_waiting = (ioreq_event *) NULL;

	} else if (!curr->next_waiting) {

		/* more than one request in waiting pool */
		/* this is the last request in waiting pool */
		ASSERT(waiting_count > 1);
		ASSERT(waiting_head != curr);
		ASSERT(waiting_tail == curr);

		/* let the previous request be the last request in waiting pool */
		waiting_tail = curr->prev_waiting;
		waiting_tail->next_waiting = (ioreq_event *) NULL;

		/* delete connection with previous request */
		curr->prev_waiting = (ioreq_event *) NULL;

	} else {

		/* more than two requests in waiting pool */
		/* this is the middle request in waiting pool */
		ASSERT(waiting_count > 2);
		ASSERT(waiting_head != curr);
		ASSERT(waiting_tail != curr);

		/* create connection from previous request to next request */
		curr->prev_waiting->next_waiting = curr->next_waiting;

		/* create connection from next request to previous request */
		curr->next_waiting->prev_waiting = curr->prev_waiting;

		/* delete connection with next request */
		curr->next_waiting = (ioreq_event *) NULL;

		/* delete connection with previous request */
		curr->prev_waiting = (ioreq_event *) NULL;

	}

	waiting_count--;
//	printf("waiting: %d simtime: %lf\n", waiting_count, simtime);
}
