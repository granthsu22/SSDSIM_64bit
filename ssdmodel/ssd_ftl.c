#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h> // for memcpy

#include "disksim_global.h"

#include "ssd_fsm.h"
#include "ssd_ftl.h"
#include "ssd_waiting_pool.h"

/*int sectorPerPage;
int pagePerBlock;
int blockPerPlane;
int planePerDie;
int diePerChannel;
int nChannel;
float freePagePercentage;
float gcPercentage;

int totalDie;  
int totalPlane;
int totalBlock;
int totalPage;

int exposedPage; 
int gcThreshold; */

//statistics
int numHostRead = 0;
int numHostWrite = 0;
int numGcRead = 0;
int numGcWrite = 0;
int numErase = 0;

// in ram
int  *lpnToPpnTable;
int  *freePageOfPlane;
int  *freePageOfBlock;
int  *validPageOfBlock;
int  *invalidPageOfBlock;

int  *eraseCount;
enum FLAG *flag;
int  *writePointer;

// in flash
int *ppnToLpnTable;

void initMapping(int lpn, int *ppn)
{
	int c, d, pl, b, p;

	pl = lpn % planePerDie;
	lpn /= planePerDie;

	c = lpn % nChannel;
	lpn /= nChannel;

	d = lpn % diePerChannel;
	lpn /= diePerChannel;

	p = lpn % (pagePerBlock-1);
	lpn /= (pagePerBlock-1);

	b = lpn % blockPerPlane;
	lpn /= blockPerPlane;

	*ppn = p + pagePerBlock * (b + blockPerPlane * ( pl + planePerDie * (d + diePerChannel * c)));

	return;
}

/* written by Ren-Shuo */
void mapping(int lpn, int *ppn, int *c, int *d, int *pl, int *b, int *p) {

	int tmp;
	tmp = lpnToPpnTable[lpn];
	*ppn = lpnToPpnTable[lpn];

	*p = tmp % pagePerBlock;
	tmp /= pagePerBlock;

	*b = tmp % blockPerPlane;
	tmp /= blockPerPlane;

	*pl = tmp % planePerDie;
	tmp /= planePerDie;

	*d = tmp % diePerChannel;
	tmp /= diePerChannel;

	*c = tmp % nChannel;

	assert(*c<nChannel);

	return;
}

void assignWritePointer(int plane) {
	const int pagePerPlane = pagePerBlock * blockPerPlane;
	const int start = plane * pagePerPlane;

	int i;
	for (i = start; i < (start + pagePerPlane); i += pagePerBlock) {
		if (flag[i] == FREE) {
			writePointer[plane] = i;
			ASSERT(freePageOfBlock[i / pagePerBlock] == pagePerBlock);
			return;
		}
	}
	ASSERT(0);
}

void ssd_ftl_init(ssd_t *currdisk) {

	sectorPerPage = currdisk->params.page_size;
	pagePerBlock = currdisk->params.pages_per_block;
	blockPerPlane = currdisk->params.blocks_per_plane;
	planePerDie = currdisk->params.planes_per_pkg;
	diePerChannel = currdisk->params.elements_per_gang;
	nChannel = currdisk->params.nelements / currdisk->params.elements_per_gang;
	freePagePercentage = (float) (currdisk->params.reserve_blocks) / 100.0;
	gcPercentage = (float) (currdisk->params.min_freeblks_percent) / 100.0;

	totalDie = nChannel * diePerChannel;
	totalPlane = totalDie * planePerDie;
	totalBlock = totalPlane * blockPerPlane;
	totalPage = totalBlock * pagePerBlock;
	exposedPage = (pagePerBlock-1) * (int) (blockPerPlane*(1.0-freePagePercentage)) * planePerDie * diePerChannel * nChannel;
	gcThreshold = pagePerBlock * (int) (blockPerPlane*(gcPercentage));

	max_lba_in_disksim = exposedPage * sectorPerPage; // wendy: for trace to limit the size

	// in ram
	lpnToPpnTable = (int *) malloc(exposedPage * sizeof(int));
	ASSERT(lpnToPpnTable);
	freePageOfPlane = (int *) malloc(totalPlane * sizeof(int));
	ASSERT(freePageOfPlane);
	freePageOfBlock = (int *) malloc(totalBlock * sizeof(int));
	ASSERT(freePageOfBlock);
	validPageOfBlock = (int *) malloc(totalBlock * sizeof(int));
	ASSERT(validPageOfBlock);
	invalidPageOfBlock = (int *) malloc(totalBlock * sizeof(int));
	ASSERT(invalidPageOfBlock);

	eraseCount = (int *) malloc(totalBlock * sizeof(int));
	ASSERT(eraseCount);
	flag = (enum FLAG *) malloc((size_t) totalPage * sizeof(enum FLAG));
	ASSERT(flag);
	writePointer = (int *) malloc(totalPlane * sizeof(int));
	ASSERT(writePointer);

	// in flash
	ppnToLpnTable = (int *) malloc(totalPage * sizeof(int));
	ASSERT(ppnToLpnTable);

	int i;
	for (i = 0; i < totalBlock; i++) {
		eraseCount[i] = 0;
		freePageOfBlock[i] = pagePerBlock;
		validPageOfBlock[i] = 0;
		invalidPageOfBlock[i] = 0;
	}

	for (i = 0; i < totalPlane; i++) {
		freePageOfPlane[i] = pagePerBlock * blockPerPlane;
	}

	for (i = 0; i < totalPage; i++) {
		flag[i] = FREE;
	}

	int lpn, ppn, c, d, pl, b, p;


	for (lpn = 0; lpn < exposedPage; lpn++) {
		initMapping(lpn, &ppn);
		ASSERT(flag[ppn] == FREE);

		lpnToPpnTable[lpn] = ppn;
		ppnToLpnTable[ppn] = lpn;

		assert(flag[ppn] == FREE);
		flag[ppn] = VALID;
		validPageOfBlock[ppn/pagePerBlock]++;

		int die, plane, block;
		block = ppn/(pagePerBlock);
		plane = ppn/(pagePerBlock*blockPerPlane);

		freePageOfPlane[plane] --;
		freePageOfBlock[block] --;

		if(freePageOfBlock[block] == 1){
			freePageOfBlock[block] --;
			freePageOfPlane[plane] --;
			flag[ppn+1] = META;
		}

		ASSERT(freePageOfPlane[plane] >= 0);
		ASSERT(freePageOfBlock[block] >= 0);
	}

	for (i = 0; i < nChannel * diePerChannel * planePerDie; i++) {
		assignWritePointer(i);
	}

	// wendy
	gc_num = 0;
	host_read_num = 0;
	host_write_num = 0;
	gc_read_num = 0;
	gc_write_num = 0;
	erase_num = 0;

}

void garbageCollection(int plane, ioreq_event *curr) {

	gc_num++; // wendy
	ssd_t *currdisk = getssd(curr->devno);

	ioreq_event *tmp;

	const int start = plane * blockPerPlane; 

	// find a victim using greedy
	int max = 0;
	int victim = -1;
	int i;
	for (i = start; i < (start + blockPerPlane); i++) {
		if (max < invalidPageOfBlock[i]) {
			max = invalidPageOfBlock[i];
			victim = i;
		}
	}
	ASSERT(victim != -1);
	ASSERT(invalidPageOfBlock[victim] > 0);

	for (i = victim * pagePerBlock; i < (victim + 1) * pagePerBlock; i++) {
		if (flag[i] == VALID) {

			/* =================================== */
			/* Generate GC page read request       */
			/* =================================== */

			ioreq_event *tmp = (ioreq_event *) getfromextraq();
			tmp->devno = curr->devno;
			tmp->busno = curr->busno;
			tmp->flags = curr->flags;
			tmp->opid = curr->opid;
			tmp->bcount = currdisk->params.page_size;
			tmp->type = DEVICE_ACCESS_COMPLETE;

			/* recover request's sector address */
			tmp->blkno = ppnToLpnTable[i] * currdisk->params.page_size;

			/* we don't care about original ssdsim elements for eclab simulator */
			tmp->ssd_elem_num = 0;

			/* point to the parent request (used for counting down finished sectors) */
			tmp->tempptr2 = NULL;

			/* fill information used for eclab FTL */
			tmp->lpn = ppnToLpnTable[i];
			tmp->parent_lpn = -1;
			tmp->ppn = i;
			tmp->arrival_time = simtime;
			tmp->access_type = SSD_REQ_READ;
			tmp->access_phase = SSD_REQ_CREATED;
			tmp->access_initiator = SSD_REQ_GC;
			tmp->next_waiting = (ioreq_event *) NULL;
			tmp->prev_waiting = (ioreq_event *) NULL;

			/* insert this request to waiting pool */
			numGcRead++;
			gc_read_num++; // wendy
			ssd_fsm_on_event_created(tmp);
			ssd_waiting_pool_push_back(tmp);

			/* =================================== */
			/* =================================== */


			// write data to the pointer
			ASSERT(flag[writePointer[plane]] == FREE);
			flag[writePointer[plane]] = VALID;
			validPageOfBlock[writePointer[plane] / pagePerBlock] ++;

			ASSERT(freePageOfBlock[writePointer[plane] / pagePerBlock] > 0);
			ASSERT(freePageOfPlane[plane] > 0);
			freePageOfBlock[writePointer[plane] / pagePerBlock]--;
			freePageOfPlane[plane]--;

			// invalidate old data
			ASSERT(flag[i] == VALID);
			flag[i] = INVALID;
			ASSERT(validPageOfBlock[i/pagePerBlock]>0);
			validPageOfBlock[victim] --;

			invalidPageOfBlock[victim]++;

			// update the mapping table
			lpnToPpnTable[ppnToLpnTable[i]] = writePointer[plane];
			ppnToLpnTable[writePointer[plane]] = ppnToLpnTable[i];
			ppnToLpnTable[i] = -1;

			/* =================================== */
			/* Generate GC page write request      */
			/* =================================== */

			tmp = (ioreq_event *) getfromextraq();
			tmp->devno = curr->devno;
			tmp->busno = curr->busno;
			tmp->flags = curr->flags;
			tmp->opid = curr->opid;
			tmp->bcount = currdisk->params.page_size;
			tmp->type = DEVICE_ACCESS_COMPLETE;

			/* recover request's sector address */
			tmp->blkno = ppnToLpnTable[writePointer[plane]] * currdisk->params.page_size;

			/* we don't care about original ssdsim elements for eclab simulator */
			tmp->ssd_elem_num = 0;

			/* point to the parent request (used for counting down finished sectors) */
			tmp->tempptr2 = NULL;

			/* fill information used for eclab FTL */
			tmp->lpn = ppnToLpnTable[writePointer[plane]];
			tmp->parent_lpn = -1;
			tmp->ppn = writePointer[plane];
			tmp->arrival_time = simtime;
			tmp->access_type = SSD_REQ_PROGRAM;
			tmp->access_phase = SSD_REQ_CREATED;
			tmp->access_initiator = SSD_REQ_GC;
			tmp->next_waiting = (ioreq_event *) NULL;
			tmp->prev_waiting = (ioreq_event *) NULL;

			/* insert this request to waiting pool */
			numGcWrite++;
			gc_write_num++; // wendy
			ssd_fsm_on_event_created(tmp);
			ssd_waiting_pool_push_back(tmp);

			/* =================================== */
			/* =================================== */

			/* update the write pointer */
			writePointer[plane]++;

			/* check if we need to write a summary page */
			if ((writePointer[plane] % pagePerBlock) == (pagePerBlock - 1)) {

				flag[writePointer[plane]] = META;

				ppnToLpnTable[writePointer[plane]] = -1;

				ASSERT(freePageOfBlock[writePointer[plane] / pagePerBlock] > 0);
				ASSERT(freePageOfPlane[plane] > 0);
				freePageOfBlock[writePointer[plane] / pagePerBlock]--;
				freePageOfPlane[plane]--;

				ASSERT(freePageOfBlock[writePointer[plane] / pagePerBlock] == 0);

				/* =================================== */
				/* Generate summary page write request */
				/* =================================== */

				ioreq_event *tmp = (ioreq_event *) getfromextraq();
				tmp->devno = curr->devno;
				tmp->busno = curr->busno;
				tmp->flags = curr->flags;
				tmp->opid = curr->opid;
				tmp->blkno = -1;
				tmp->bcount = currdisk->params.page_size;
				tmp->type = DEVICE_ACCESS_COMPLETE;

				/* we don't care about original ssdsim elements for eclab simulator */
				tmp->ssd_elem_num = 0;

				/* point to the parent request (used for counting down finished sectors) */
				tmp->tempptr2 = NULL;

				/* fill information used for eclab FTL */
				tmp->lpn = -1;
				tmp->parent_lpn = -1;
				tmp->ppn = writePointer[plane];
				tmp->arrival_time = simtime;
				tmp->access_type = SSD_REQ_PROGRAM;
				tmp->access_phase = SSD_REQ_CREATED;
				tmp->access_initiator = SSD_REQ_GC;
				tmp->next_waiting = (ioreq_event *) NULL;
				tmp->prev_waiting = (ioreq_event *) NULL;

				/* insert this request to waiting pool */
				numGcWrite++;
				gc_write_num++; // wendy
				ssd_fsm_on_event_created(tmp);
				ssd_waiting_pool_push_back(tmp);

				/* =================================== */
				/* =================================== */

				assignWritePointer(plane);
			}
		}
	}
	ASSERT(validPageOfBlock[victim] == 0);
	ASSERT(invalidPageOfBlock[victim] == pagePerBlock - 1);

	for (i = victim * pagePerBlock; i < (victim + 1) * pagePerBlock; i++) {
		flag[i] = FREE;        
	}
	invalidPageOfBlock[victim] = 0;    

	/* =================================== */
	/* Generate erase block request        */
	/* =================================== */

	tmp = (ioreq_event *) getfromextraq();
	tmp->devno = curr->devno;
	tmp->busno = curr->busno;
	tmp->flags = curr->flags;
	tmp->opid = curr->opid;
	tmp->blkno = -1;
	tmp->bcount = currdisk->params.page_size;
	tmp->type = DEVICE_ACCESS_COMPLETE;

	/* we don't care about original ssdsim elements for eclab simulator */
	tmp->ssd_elem_num = 0;

	/* point to the parent request (used for counting down finished sectors) */
	tmp->tempptr2 = NULL;

	/* fill information used for eclab FTL */
	tmp->lpn = -1;
	tmp->parent_lpn = -1;
	tmp->ppn = victim * pagePerBlock; 
	tmp->arrival_time = simtime;
	tmp->access_type = SSD_REQ_ERASE;
	tmp->access_phase = SSD_REQ_CREATED;
	tmp->access_initiator = SSD_REQ_GC;
	tmp->next_waiting = (ioreq_event *) NULL;
	tmp->prev_waiting = (ioreq_event *) NULL;

	/* insert this request to waiting pool */
	numErase++;
	erase_num++; // wendy
	ssd_fsm_on_event_created(tmp);
	ssd_waiting_pool_push_back(tmp);

	/* =================================== */
	/* =================================== */

	freePageOfPlane[plane] += pagePerBlock;
	freePageOfBlock[victim] = pagePerBlock;
}

void ssd_ftl_read(ioreq_event *curr) {

	/* make sure we're handling the read request */
	ASSERT(curr->access_type == SSD_REQ_READ);
	ASSERT(curr->access_phase == SSD_REQ_CREATED);
	ASSERT(curr->access_initiator == SSD_REQ_HOST);

	/* get position of the old data */
	int lpn, ppn, c, d, pl, b, p;
	lpn = curr->lpn;
	mapping(lpn, &ppn, &c, &d, &pl, &b, &p);

	/* make sure the reverse mapping table are also correct */
	if(flag[ppn] != VALID) {
		printf("flag[ppn] != VALID, gc_num: %d, flag[%d] = %d\n", gc_num, ppn, flag[ppn]);
	}
	ASSERT(flag[ppn] == VALID);
	if(ppnToLpnTable[ppn] != lpn) { // wendy
		printf("ppnToLpnTable[ppn]: %d, lpn: %d, gc_num: %d\n", ppnToLpnTable[ppn], lpn, gc_num);
		printf("curr->time: %lf, curr->blkno: %d, curr->bcount: %d\n", curr->arrival_time, curr->blkno, curr->bcount);
		printf("curr->blkno: %d, curr->bcount: %d\n", curr->blkno, curr->bcount);
	}
	ASSERT(ppnToLpnTable[ppn] == lpn);

	/* fill physical address information */
	curr->ppn = ppn; 

	/* insert this request to waiting pool */
	numHostRead++;
	host_read_num++; // wendy
	ssd_fsm_on_event_created(curr);
	ssd_waiting_pool_push_back(curr);
}

void ssd_ftl_write(ioreq_event *curr) {

	ssd_t *currdisk = getssd(curr->devno);

	/* make sure we're handling the write request */
	ASSERT(curr->access_type == SSD_REQ_PROGRAM);
	ASSERT(curr->access_phase == SSD_REQ_CREATED);
	ASSERT(curr->access_initiator == SSD_REQ_HOST);

	/* get position of the old data */
	int lpn, ppn, c, d, pl, b, p;
	lpn = curr->lpn;
	mapping(lpn, &ppn, &c, &d, &pl, &b, &p);

	/* make sure the reverse mapping table are also correct */
	ASSERT(ppn != -1);
	if(ppnToLpnTable[ppn] != lpn) {
		printf("ppnToLpnTable[ppn]: %d, lpn: %d, gc_num: %d\n", ppnToLpnTable[ppn], lpn, gc_num);
		printf("curr->time: %lf, curr->blkno: %d, curr->bcount: %d\n", curr->time, curr->blkno, curr->bcount);
	}
	ASSERT(ppnToLpnTable[ppn] == lpn);

	/* integrated codes written by Ren-Shuo */
	int plane = ppn / (pagePerBlock * blockPerPlane); 
	int block = ppn / pagePerBlock;
	ASSERT(plane % planePerDie == pl);
	ASSERT(block % blockPerPlane == b);

	/* write data to the pointer */
	assert(flag[writePointer[plane]] == FREE);
	flag[writePointer[plane]] = VALID;
	validPageOfBlock[writePointer[plane] / pagePerBlock]++;

	assert(freePageOfBlock[writePointer[plane] / pagePerBlock] > 0);
	assert(freePageOfPlane[plane] > 0);
	freePageOfBlock[writePointer[plane] / pagePerBlock]--;
	freePageOfPlane[plane]--;

	/* invalidate old data */
	assert(flag[ppn] == VALID);
	flag[ppn] = INVALID;
	invalidPageOfBlock[ppn / pagePerBlock]++;
	assert(invalidPageOfBlock[ppn / pagePerBlock] < pagePerBlock);
	assert(validPageOfBlock[ppn / pagePerBlock] > 0);
	validPageOfBlock[ppn / pagePerBlock]--;

	/* update the mapping table */
	lpnToPpnTable[lpn] = writePointer[plane];
	ppnToLpnTable[writePointer[plane]] = lpn;
	ppnToLpnTable[ppn] = -1;

	/* fill physical address information */
	curr->ppn = writePointer[plane];

	/* insert this request to waiting pool */
	numHostWrite++;
	host_write_num++; // wendy
	ssd_fsm_on_event_created(curr);
	ssd_waiting_pool_push_back(curr);

	/* update the write pointer */
	writePointer[plane]++;

	/* check if we need to write a summary page */
	if ((writePointer[plane] % pagePerBlock) == (pagePerBlock - 1)) { 
		flag[writePointer[plane]] = META;

		ppnToLpnTable[writePointer[plane]] = -1;

		assert(freePageOfBlock[writePointer[plane] / pagePerBlock] > 0);
		assert(freePageOfPlane[plane] > 0);
		freePageOfBlock[writePointer[plane]/pagePerBlock]--;
		freePageOfPlane[plane]--;

		assert(freePageOfBlock[writePointer[plane] / pagePerBlock] == 0);

		/* =================================== */
		/* Generate summary page write request */
		/* =================================== */

		ioreq_event *tmp = (ioreq_event *) getfromextraq();
		tmp->devno = curr->devno;
		tmp->busno = curr->busno;
		tmp->flags = curr->flags;
		tmp->opid = curr->opid;
		tmp->bcount = currdisk->params.page_size;
		tmp->type = DEVICE_ACCESS_COMPLETE;

		/* recover request's sector address */
		tmp->blkno = ppnToLpnTable[writePointer[plane]] * currdisk->params.page_size; // wendy: -8

		/* we don't care about original ssdsim elements for eclab simulator */
		tmp->ssd_elem_num = 0;

		/* point to the parent request (used for counting down finished sectors) */
		tmp->tempptr2 = NULL;

		/* fill information used for eclab FTL */
		tmp->lpn = ppnToLpnTable[writePointer[plane]]; // wendy: -1
		tmp->parent_lpn = -1;
		tmp->ppn = writePointer[plane];
		tmp->arrival_time = simtime;
		tmp->access_type = SSD_REQ_PROGRAM;
		tmp->access_phase = SSD_REQ_CREATED;
		tmp->access_initiator = SSD_REQ_HOST;
		tmp->next_waiting = (ioreq_event *) NULL;
		tmp->prev_waiting = (ioreq_event *) NULL;

		/* insert this request to waiting pool */
		numHostWrite++;
		host_write_num++; // wendy
		ssd_fsm_on_event_created(tmp);
		ssd_waiting_pool_push_back(tmp);

		/* =================================== */

		assignWritePointer(plane);
	}

	if (freePageOfPlane[plane] < gcThreshold) {
		garbageCollection(plane, curr);
	}
}

// add by wendy
int get_die_number(int lpn)
{
	int tmp;
	int die_number;
//	int lpn = curr->lpn;
	tmp = lpnToPpnTable[lpn];
	tmp /= pagePerBlock;
	tmp /= blockPerPlane;
	tmp /= planePerDie;
	die_number = tmp % diePerChannel;

	return die_number;
}
