#ifndef SIM_H
#define SIM_H

#include "program.h"
#include "utilities.h"   /* for MAX_PROGRAMS, NUM_RESOURCES, enums, MemoryWord */

extern MemQueue  BlockingQueuesNotPtrs[NUM_RESOURCES];
extern MemQueue *BlockingQueues[NUM_RESOURCES];
extern bool      Resources_availability[NUM_RESOURCES];
extern struct MemoryWord *Program_start_locations[MAX_PROGRAMS];

typedef struct {
    int pid, state, pc, prio, mem_lo, mem_hi;
} SimProcInfo;

typedef struct {
    int clock;                   /* global time */
    int algorithm;               /* FCFS, RR, or MLFQ */
    int procs_total;

    SimProcInfo proc[MAX_PROGRAMS];

    int ready[MAX_PROGRAMS], ready_len;
    int block[NUM_RESOURCES][MAX_PROGRAMS], block_len[NUM_RESOURCES];

    bool res_free[NUM_RESOURCES];
} SimSnapshot;

/* Public API: initialize, run one tick, reset */
void sim_init (struct program list[], int n,
               SCHEDULING_ALGORITHM alg, int quantum);
int  sim_step (SimSnapshot *out);  /* returns 1 while processes remain */
void sim_reset(void);

#endif /* SIM_H */
