
#ifndef _DISKSIM_GLOBAL_PARAM_H
#define _DISKSIM_GLOBAL_PARAM_H  

#include <libparam/libparam.h>
#ifdef __cplusplus
extern"C"{
#endif
struct dm_disk_if;

/* prototype for disksim_global param loader function */
int disksim_global_loadparams(struct lp_block *b);

typedef enum {
   DISKSIM_GLOBAL_INIT_SEED,
   DISKSIM_GLOBAL_INIT_SEED_WITH_TIME,
   DISKSIM_GLOBAL_REAL_SEED,
   DISKSIM_GLOBAL_REAL_SEED_WITH_TIME,
   DISKSIM_GLOBAL_STATISTIC_WARM_UP_TIME,
   DISKSIM_GLOBAL_STATISTIC_WARM_UP_IOS,
   DISKSIM_GLOBAL_STAT_DEFINITION_FILE,
   DISKSIM_GLOBAL_OUTPUT_FILE_FOR_TRACE_OF_IO_REQUESTS_SIMULATED,
   DISKSIM_GLOBAL_DETAILED_EXECUTION_TRACE,
   DISKSIM_GLOBAL_FAIRNESS_SCHEDULER,
   DISKSIM_GLOBAL_USE_PID_TRACE,
   DISKSIM_GLOBAL_CHOSEN_PID,
   DISKSIM_GLOBAL_TIMESLICE,
   DISKSIM_GLOBAL_READ_TIMESLICE,
   DISKSIM_GLOBAL_WRITE_TIMESLICE,
   DISKSIM_GLOBAL_CURR_OVER_NEXT,
   DISKSIM_GLOBAL_READ_OVER_WRITE,
   DISKSIM_GLOBAL_HOST_OVER_GC,
   DISKSIM_GLOBAL_NCQ_COMMAND_SIZE_BOUND,
   DISKSIM_GLOBAL_DISPATCH_THRESHOLD,
   DISKSIM_GLOBAL_INTERNAL_SCHEDULER,
   DISKSIM_GLOBAL_WRITE_DEADLINE,
   DISKSIM_GLOBAL_IO_WRITE_DEADLINE_SCHEDULER,
   DISKSIM_GLOBAL_INTERNAL_WRITE_DEADLINE_SCHEDULER,
   DISKSIM_GLOBAL_WRITE_STARVE,
   DISKSIM_GLOBAL_IO_WRITE_STARVE,
   DISKSIM_GLOBAL_INTERNAL_WRITE_STARVE,
   DISKSIM_GLOBAL_CHOSEN_TYPE
} disksim_global_param_t;

#define DISKSIM_GLOBAL_MAX_PARAM		DISKSIM_GLOBAL_CHOSEN_TYPE
extern void * DISKSIM_GLOBAL_loaders[];
extern lp_paramdep_t DISKSIM_GLOBAL_deps[];


static struct lp_varspec disksim_global_params [] = {
   {"Init Seed", I, 0 },
   {"Init Seed with time", I, 0 },
   {"Real Seed", I, 0 },
   {"Real Seed with time", I, 0 },
   {"Statistic warm-up time", D, 0 },
   {"Statistic warm-up IOs", I, 0 },
   {"Stat definition file", S, 1 },
   {"Output file for trace of I/O requests simulated", S, 0 },
   {"Detailed execution trace", S, 0 },
   {"Fairness scheduler", I, 1 },
   {"Use pid trace", I, 1 },
   {"Chosen pid", I, 1 },
   {"Timeslice", D, 1 },
   {"Read timeslice", D, 1 },
   {"Write timeslice", D, 1 },
   {"Curr over next", I, 1 },
   {"Read over write", I, 1 },
   {"Host over gc", I, 1 },
   {"NCQ command size bound", I, 1 },
   {"Dispatch threshold", I, 1 },
   {"Internal scheduler", I, 1 },
   {"Write deadline", D, 1 },
   {"IO write deadline scheduler", I, 1 },
   {"Internal write deadline scheduler", I, 1 },
   {"Write starve", I, 1 },
   {"IO write starve", I, 1 },
   {"Internal write starve", I, 1 },
   {"Chosen type", I, 1 },
   {0,0,0}
};
#define DISKSIM_GLOBAL_MAX 28
static struct lp_mod disksim_global_mod = { "disksim_global", disksim_global_params, DISKSIM_GLOBAL_MAX, (lp_modloader_t)disksim_global_loadparams,  0, 0, DISKSIM_GLOBAL_loaders, DISKSIM_GLOBAL_deps };


#ifdef __cplusplus
}
#endif
#endif // _DISKSIM_GLOBAL_PARAM_H
