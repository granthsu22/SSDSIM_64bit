/*
 * DiskSim Storage Subsystem Simulation Environment (Version 4.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001-2008.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */



/*
 * DiskSim Storage Subsystem Simulation Environment (Version 2.0)
 * Revision Authors: Greg Ganger
 * Contributors: Ross Cohen, John Griffin, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 1999.
 *
 * Permission to reproduce, use, and prepare derivative works of
 * this software for internal use is granted provided the copyright
 * and "No Warranty" statements are included with all reproductions
 * and derivative works. This software may also be redistributed
 * without charge provided that the copyright and "No Warranty"
 * statements are included in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 */


#include "disksim_global.h"

void print_wendy_message(void)
{
	int i;
	double die_utilization;

	printf("gc_num: %d\n", gc_num);
	printf("host_big_write_num: %d\n", host_big_write_num);
	printf("host_big_read_num: %d\n", host_big_read_num);
	printf("host_write_num: %d\n", host_write_num);
	printf("host_read_num: %d\n", host_read_num);
	printf("gc_read_num: %d\n", gc_read_num);
	printf("gc_write_num: %d\n", gc_write_num);
	printf("erase_num: %d\n", erase_num);
	printf("pending_request: %d\n", pending_request);
	printf("totalreqs: %d\n", disksim->totalreqs);

	printf("all_response_time: %lf\n", all_response_time/total_complete_req);
	printf("ssd_response_time: %lf\n", ssd_response_time/total_complete_req);
	printf("overall_response_time: %lf\n", overall_response_time/overall_total_complete_req);

	printf("all_worst_response_time: %lf\n", all_worst_response_time);
	printf("ssd_worst_response_time: %lf\n", ssd_worst_response_time);

	printf("find_light_times: %d\n", find_light_times);
	printf("total_complete_req: %d\n", total_complete_req);

	printf("acc_light_loading_time per req: %lf\n", acc_light_loading_time/disksim->totalreqs);
	printf("acc_light_loading_time prob: %lf\n", acc_light_loading_time/simtime);

	printf("simtime: %lf\n", simtime);

	/*printf("per die: ");
	die_utilization = 0;
	for(i = 0; i < totalDie; i++) {
		die_utilization += die_time_accumulate[i];
		printf("%lf ", die_time_accumulate[i]);
	}
	printf("\n");

	die_utilization = die_utilization / totalDie / simtime;*/

	printf("acc_read_response_time_per_process: ");
	for(i = 0; i < PROCESS_NUM; i++) {
		printf("%lf ", read_overall_response_time_per_process[i]);
	}
	printf("\n");

	printf("acc_write_response_time_per_process: ");
	for(i = 0; i < PROCESS_NUM; i++) {
		printf("%lf ", write_overall_response_time_per_process[i]);
	}
	printf("\n");

	printf("read_request_per_process: ");
	for(i = 0; i < PROCESS_NUM; i++) {
		printf("%d ", read_req_num_per_process[i]);
	}
	printf("\n");

	printf("write_request_per_process: ");
	for(i = 0; i < PROCESS_NUM; i++) {
		printf("%d ", write_req_num_per_process[i]);
	}
	printf("\n");

	printf("ave_response_time_per_process: ");
	for(i = 0; i < PROCESS_NUM; i++) {
		printf("%lf ", (read_overall_response_time_per_process[i] + write_overall_response_time_per_process[i]) / (read_req_num_per_process[i] + write_req_num_per_process[i]));
	}
	printf("\n");

	printf("acc_read_queueing_time: %lf\n", read_queueing_time);
	printf("acc_write_queueing_time: %lf\n", write_queueing_time);
	printf("ave_read_queueing_time: %lf\n", read_queueing_time / host_big_read_num);
	printf("ave_write_queueing_time: %lf\n", write_queueing_time / host_big_write_num);

	printf("acc_read_ssd_time: %lf\n", read_ssd_time);
	printf("acc_write_ssd_time: %lf\n", write_ssd_time);
	printf("ave_read_ssd_time: %lf\n", read_ssd_time / host_big_read_num);
	printf("ave_write_ssd_time: %lf\n", write_ssd_time / host_big_write_num);

	// printf("die_utilization: %lf\n", die_utilization);

	printf("getfromextraq: %d\n", getfromextraq_counter);
	printf("addtoextraq: %d\n", addtoextraq_counter);
	printf("anti: %d\n", anticipate_event);
	printf("fios_process_time_total: %lf\n", fios_process_time_total);
	
	printf("anti_counter: %d\n", anti_counter);
	printf("read_anti_counter: %d\n", read_anti_counter);
	printf("anti_good: %d\n", anti_good);
	printf("read_anti_good: %d\n", read_anti_good);

	printf("anti_good_prob: %lf\n", (double)anti_good/(double)anti_counter);
	printf("read_anti_good_prob: %lf\n", (double)read_anti_good/(double)read_anti_counter);
	
	printf("total_process_time: %lf\n", total_process_time);
	printf("total_idle_time: %lf\n", total_idle_time);
	printf("total: %lf, simtime: %lf\n", total_process_time + total_idle_time, simtime);
	printf("portion: %lf\n", acc_light_loading_time/total_process_time);


}

int main (int argc, char **argv)
{
	int len;

#ifndef _WIN32
	setlinebuf(stdout);
	setlinebuf(stderr);
#endif

#ifdef DUMP_POWER_TRACE
	if (argc >= 2) {
		power_trace_filename = (char *) malloc(256 * sizeof(char));
		strcpy(power_trace_filename, argv[2]);
		strcat(power_trace_filename, ".power_trace.csv");
	} else {
		power_trace_filename = NULL;
	}
#else
	power_trace_filename = NULL;
#endif


	if(argc == 2) {
		disksim_restore_from_checkpoint (argv[1]);
	} 
	else {
		disksim = calloc(1, sizeof(struct disksim));
		disksim_initialize_disksim_structure(disksim);
		disksim_setup_disksim (argc, argv);
	}

	disksim_run_simulation ();
	disksim_cleanup_and_printstats ();
	
	print_wendy_message();	

	exit(0);
}
