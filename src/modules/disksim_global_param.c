#include "disksim_global_param.h"
#include <libparam/bitvector.h>
#include "../disksim_global.h"
#include <libddbg/libddbg.h>
static int DISKSIM_GLOBAL_INIT_SEED_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_INIT_SEED_loader(int result, int i) { 
 disksim->seedval = i;
 DISKSIM_srand48(disksim->seedval);

}

static int DISKSIM_GLOBAL_INIT_SEED_WITH_TIME_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_INIT_SEED_WITH_TIME_loader(int result, int i) { 
if (! (RANGE(i,0,1))) { // foo 
 } 
 if(i) { disksim->seedval = DISKSIM_time(); }

}

static int DISKSIM_GLOBAL_REAL_SEED_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_REAL_SEED_loader(int result, int i) { 
 disksim->seedval = i;

}

static int DISKSIM_GLOBAL_REAL_SEED_WITH_TIME_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_REAL_SEED_WITH_TIME_loader(int result, int i) { 
if (! (RANGE(i,0,1))) { // foo 
 } 
 disksim->seedval = DISKSIM_time();

}

static int DISKSIM_GLOBAL_STATISTIC_WARM_UP_TIME_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_STATISTIC_WARM_UP_TIME_loader(int result, double d) { 
if (! ((d >= 0))) { // foo 
 } 
 disksim->warmup_event = (timer_event *) getfromextraq();
 disksim->warmup_event->type = TIMER_EXPIRED;
 disksim->warmup_event->time = d * (double) 1000.0;
 disksim->warmup_event->func = &disksim->timerfunc_disksim;

}

static int DISKSIM_GLOBAL_STATISTIC_WARM_UP_IOS_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_STATISTIC_WARM_UP_IOS_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->warmup_iocnt = i;

}

static int DISKSIM_GLOBAL_STAT_DEFINITION_FILE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_STAT_DEFINITION_FILE_loader(int result, char *s) { 
 char *path = lp_search_path(lp_cwd, s);
 if(!path) {
 ddbg_assert2(0, "Couldn't find statdefs file in path");
 }
 else {
 statdeffile = fopen(path, "r");
 ddbg_assert2(statdeffile != 0, "failed to open statdefs file!");
 }

}

static int DISKSIM_GLOBAL_OUTPUT_FILE_FOR_TRACE_OF_IO_REQUESTS_SIMULATED_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_OUTPUT_FILE_FOR_TRACE_OF_IO_REQUESTS_SIMULATED_loader(int result, char *s) { 
if (! ((outios = fopen(s, "w")) != NULL)) { // foo 
 } 
 strcpy(disksim->outiosfilename, s);

}

static int DISKSIM_GLOBAL_DETAILED_EXECUTION_TRACE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_DETAILED_EXECUTION_TRACE_loader(int result, char *s) { 
if (! ((disksim->exectrace = fopen(s, "w")) != NULL)) { // foo 
 } 
 disksim->exectrace_fn = strdup(s);

}

static int DISKSIM_GLOBAL_FAIRNESS_SCHEDULER_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_FAIRNESS_SCHEDULER_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->fairness_scheduler = i;

}

static int DISKSIM_GLOBAL_USE_PID_TRACE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_USE_PID_TRACE_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->use_pid_trace = i;

}

static int DISKSIM_GLOBAL_CHOSEN_PID_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_CHOSEN_PID_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->chosen_pid = i;

}

static int DISKSIM_GLOBAL_TIMESLICE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_TIMESLICE_loader(int result, double d) { 
if (! ((d >= 0.0))) { // foo 
 } 
 disksim->timeslice = d;

}

static int DISKSIM_GLOBAL_READ_TIMESLICE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_READ_TIMESLICE_loader(int result, double d) { 
if (! ((d >= 0.0))) { // foo 
 } 
 disksim->read_timeslice = d;

}

static int DISKSIM_GLOBAL_WRITE_TIMESLICE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_WRITE_TIMESLICE_loader(int result, double d) { 
if (! ((d >= 0.0))) { // foo 
 } 
 disksim->write_timeslice = d;

}

static int DISKSIM_GLOBAL_CURR_OVER_NEXT_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_CURR_OVER_NEXT_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->curr_over_next = i;

}

static int DISKSIM_GLOBAL_READ_OVER_WRITE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_READ_OVER_WRITE_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->read_over_write = i;

}

static int DISKSIM_GLOBAL_HOST_OVER_GC_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_HOST_OVER_GC_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->host_over_gc = i;

}

static int DISKSIM_GLOBAL_NCQ_COMMAND_SIZE_BOUND_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_NCQ_COMMAND_SIZE_BOUND_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->ncq_command_size_bound = i;

}

static int DISKSIM_GLOBAL_DISPATCH_THRESHOLD_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_DISPATCH_THRESHOLD_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->dispatch_threshold = i;

}

static int DISKSIM_GLOBAL_INTERNAL_SCHEDULER_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_INTERNAL_SCHEDULER_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->internal_scheduler = i;

}

static int DISKSIM_GLOBAL_WRITE_DEADLINE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_WRITE_DEADLINE_loader(int result, double d) { 
if (! ((d >= 0))) { // foo 
 } 
 disksim->write_deadline = d;

}

static int DISKSIM_GLOBAL_IO_WRITE_DEADLINE_SCHEDULER_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_IO_WRITE_DEADLINE_SCHEDULER_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->io_write_deadline_scheduler = i;

}

static int DISKSIM_GLOBAL_INTERNAL_WRITE_DEADLINE_SCHEDULER_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_INTERNAL_WRITE_DEADLINE_SCHEDULER_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->internal_write_deadline_scheduler = i;

}

static int DISKSIM_GLOBAL_WRITE_STARVE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_WRITE_STARVE_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->write_starve = i;

}

static int DISKSIM_GLOBAL_IO_WRITE_STARVE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_IO_WRITE_STARVE_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->io_write_starve = i;

}

static int DISKSIM_GLOBAL_INTERNAL_WRITE_STARVE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_INTERNAL_WRITE_STARVE_loader(int result, int i) { 
if (! ((i >= 0))) { // foo 
 } 
 disksim->internal_write_starve = i;

}

static int DISKSIM_GLOBAL_CHOSEN_TYPE_depend(char *bv) {
return -1;
}

static void DISKSIM_GLOBAL_CHOSEN_TYPE_loader(int result, int i) { 
 disksim->chosen_type = i;

}

void * DISKSIM_GLOBAL_loaders[] = {
(void *)DISKSIM_GLOBAL_INIT_SEED_loader,
(void *)DISKSIM_GLOBAL_INIT_SEED_WITH_TIME_loader,
(void *)DISKSIM_GLOBAL_REAL_SEED_loader,
(void *)DISKSIM_GLOBAL_REAL_SEED_WITH_TIME_loader,
(void *)DISKSIM_GLOBAL_STATISTIC_WARM_UP_TIME_loader,
(void *)DISKSIM_GLOBAL_STATISTIC_WARM_UP_IOS_loader,
(void *)DISKSIM_GLOBAL_STAT_DEFINITION_FILE_loader,
(void *)DISKSIM_GLOBAL_OUTPUT_FILE_FOR_TRACE_OF_IO_REQUESTS_SIMULATED_loader,
(void *)DISKSIM_GLOBAL_DETAILED_EXECUTION_TRACE_loader,
(void *)DISKSIM_GLOBAL_FAIRNESS_SCHEDULER_loader,
(void *)DISKSIM_GLOBAL_USE_PID_TRACE_loader,
(void *)DISKSIM_GLOBAL_CHOSEN_PID_loader,
(void *)DISKSIM_GLOBAL_TIMESLICE_loader,
(void *)DISKSIM_GLOBAL_READ_TIMESLICE_loader,
(void *)DISKSIM_GLOBAL_WRITE_TIMESLICE_loader,
(void *)DISKSIM_GLOBAL_CURR_OVER_NEXT_loader,
(void *)DISKSIM_GLOBAL_READ_OVER_WRITE_loader,
(void *)DISKSIM_GLOBAL_HOST_OVER_GC_loader,
(void *)DISKSIM_GLOBAL_NCQ_COMMAND_SIZE_BOUND_loader,
(void *)DISKSIM_GLOBAL_DISPATCH_THRESHOLD_loader,
(void *)DISKSIM_GLOBAL_INTERNAL_SCHEDULER_loader,
(void *)DISKSIM_GLOBAL_WRITE_DEADLINE_loader,
(void *)DISKSIM_GLOBAL_IO_WRITE_DEADLINE_SCHEDULER_loader,
(void *)DISKSIM_GLOBAL_INTERNAL_WRITE_DEADLINE_SCHEDULER_loader,
(void *)DISKSIM_GLOBAL_WRITE_STARVE_loader,
(void *)DISKSIM_GLOBAL_IO_WRITE_STARVE_loader,
(void *)DISKSIM_GLOBAL_INTERNAL_WRITE_STARVE_loader,
(void *)DISKSIM_GLOBAL_CHOSEN_TYPE_loader
};

lp_paramdep_t DISKSIM_GLOBAL_deps[] = {
DISKSIM_GLOBAL_INIT_SEED_depend,
DISKSIM_GLOBAL_INIT_SEED_WITH_TIME_depend,
DISKSIM_GLOBAL_REAL_SEED_depend,
DISKSIM_GLOBAL_REAL_SEED_WITH_TIME_depend,
DISKSIM_GLOBAL_STATISTIC_WARM_UP_TIME_depend,
DISKSIM_GLOBAL_STATISTIC_WARM_UP_IOS_depend,
DISKSIM_GLOBAL_STAT_DEFINITION_FILE_depend,
DISKSIM_GLOBAL_OUTPUT_FILE_FOR_TRACE_OF_IO_REQUESTS_SIMULATED_depend,
DISKSIM_GLOBAL_DETAILED_EXECUTION_TRACE_depend,
DISKSIM_GLOBAL_FAIRNESS_SCHEDULER_depend,
DISKSIM_GLOBAL_USE_PID_TRACE_depend,
DISKSIM_GLOBAL_CHOSEN_PID_depend,
DISKSIM_GLOBAL_TIMESLICE_depend,
DISKSIM_GLOBAL_READ_TIMESLICE_depend,
DISKSIM_GLOBAL_WRITE_TIMESLICE_depend,
DISKSIM_GLOBAL_CURR_OVER_NEXT_depend,
DISKSIM_GLOBAL_READ_OVER_WRITE_depend,
DISKSIM_GLOBAL_HOST_OVER_GC_depend,
DISKSIM_GLOBAL_NCQ_COMMAND_SIZE_BOUND_depend,
DISKSIM_GLOBAL_DISPATCH_THRESHOLD_depend,
DISKSIM_GLOBAL_INTERNAL_SCHEDULER_depend,
DISKSIM_GLOBAL_WRITE_DEADLINE_depend,
DISKSIM_GLOBAL_IO_WRITE_DEADLINE_SCHEDULER_depend,
DISKSIM_GLOBAL_INTERNAL_WRITE_DEADLINE_SCHEDULER_depend,
DISKSIM_GLOBAL_WRITE_STARVE_depend,
DISKSIM_GLOBAL_IO_WRITE_STARVE_depend,
DISKSIM_GLOBAL_INTERNAL_WRITE_STARVE_depend,
DISKSIM_GLOBAL_CHOSEN_TYPE_depend
};

