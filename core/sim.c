/*  core/sim.c  – single‑step scheduler engine (FCFS / RR / MLFQ)  */
#include <string.h>
#include <pthread.h>
#include "sim.h" 
#include "utilities.h"     /* queue + enums + MemoryWord */


MemQueue  BlockingQueuesNotPtrs[NUM_RESOURCES];
MemQueue *BlockingQueues[NUM_RESOURCES];

bool Resources_availability[NUM_RESOURCES] = { true, true, true };
struct MemoryWord *Program_start_locations[MAX_PROGRAMS] = { 0 };



#define MEM_POOL_WORDS 256

static struct MemoryWord _sim_memory_pool[MEM_POOL_WORDS];
struct MemoryWord *Memory_start_location = NULL;


#include "program.h"
#define MAX_PROGRAMS 100
/* ───────── GLOBAL ENGINE STATE ───────── */
static struct program *plist = NULL;
static int   plen            = 0;
static SCHEDULING_ALGORITHM g_alg = FCFS;
static int   g_quantum       = 2;

static int   clk             = 0;
static int   finished        = 0;

static pthread_mutex_t sim_mtx = PTHREAD_MUTEX_INITIALIZER;

/* queues for resources come from your old globals */
extern MemQueue BlockingQueuesNotPtrs[NUM_RESOURCES];
extern MemQueue *BlockingQueues[NUM_RESOURCES];

extern struct MemoryWord *Program_start_locations[MAX_PROGRAMS];
extern bool Resources_availability[NUM_RESOURCES];

/* helpers already written in your original code */
extern void add_program_to_memory(struct program[],int,MemQueue*);
extern bool execute_an_instruction(struct MemoryWord*);
extern bool can_execute_instruction(struct MemoryWord*);
extern MemQueue*  get_blocking_queue(struct MemoryWord*);

/* ─── internal scheduler state ─── */
static struct {
    /* common */
    MemQueue ready;

    /* RR */
    int cur_q;
    struct MemoryWord *running;

    /* MLFQ */
    MemQueue q[4];
    int      cur_level[MAX_PROGRAMS];
    int      rem_q   [MAX_PROGRAMS];
    struct MemoryWord *ml_running;
    int      ml_pid;
} S;

/* forward decls */
static void step_fcfs(void);
static void step_rr  (void);
static void step_mlfq(void);
static void fill_snapshot(SimSnapshot*);

/* ───────── PUBLIC API ───────── */
void sim_reset(void)
{
    pthread_mutex_lock(&sim_mtx);
    memset(&S,0,sizeof S);
    clk = finished = 0;
    pthread_mutex_unlock(&sim_mtx);
}

void sim_init(struct program list[], int n,
              SCHEDULING_ALGORITHM alg, int quantum)
{    
    sim_reset();


    pthread_mutex_lock(&sim_mtx);
    fprintf(stderr, "[DEBUG] sim_init: plist=%p, n=%d\n", (void*)list, n);
    for(int i=0;i<n;i++){
            fprintf(stderr, "  prog[%d]=%s @%d\n", i,
                    list[i].programName,
                    list[i].arrivalTime);
    }

    Memory_start_location = _sim_memory_pool;
    fprintf(stderr,
        "[DEBUG] sim_init: Memory_start_location=%p (pool at %p..%p)\n",
        (void*)Memory_start_location,
        (void*)_sim_memory_pool,
        (void*)(&_sim_memory_pool[MEM_POOL_WORDS-1])
    );
    
    plist     = list;
    plen      = n;
    g_alg     = alg;
    g_quantum = quantum;


    initQueue(&S.ready);
    
    extern struct program *g_plist;
    extern int            g_plen;
    g_plist = plist;
    g_plen  = plen;
    if(alg == MLFQ){
        for(int i=0;i<4;i++) initQueue(&S.q[i]);
        for(int p=0;p<MAX_PROGRAMS;p++){
            S.cur_level[p]=0;
            S.rem_q[p]   =0;
        }
    }
    /* BlockingQueuesNotPtrs already initialised in old main */
    /* initialize blocking queues */
    for(int r = 0; r < NUM_RESOURCES; r++) {
        initQueue(&BlockingQueuesNotPtrs[r]);
        BlockingQueues[r] = &BlockingQueuesNotPtrs[r];

        // <<< DROP‑IN DEBUG: verify pointer + initial size
        fprintf(stderr,
            "[DEBUG] sim_init: BlockingQueues[%d] ➞ %p (size=%d)\n",
            r,
            (void*)BlockingQueues[r],
            BlockingQueues[r]->size
        );
    }

    printf("[DEBUG] sim_init: S.ready ➞ %p  size=%d\n",(void*)&S.ready, S.ready.size);

    pthread_mutex_unlock(&sim_mtx);
}

int sim_step(SimSnapshot *out)
{
    pthread_mutex_lock(&sim_mtx);

    // Debug: Dump global pointers and counters
    fprintf(stderr,
        "[DEBUG] sim_step: plist=%p, plen=%d, clk=%d, finished=%d, out=%p\n",
        (void*)plist, plen, clk, finished, (void*)out);

    if (!plist) {
        fprintf(stderr, "[ERROR] sim_step called before sim_init—plist is NULL!\n");
        pthread_mutex_unlock(&sim_mtx);
        return 0; // Return immediately if plist is not initialized
    }

    if (finished == plen) {
        fprintf(stderr, "[DEBUG] All processes finished; calling fill_snapshot for final state\n");
        fill_snapshot(out);
        pthread_mutex_unlock(&sim_mtx);
        return 0;
    }

    // Debug: Print ready queue state
    fprintf(stderr,
        "[DEBUG] Before switch: ready.items=%p, size=%d\n",
        (void*)S.ready.items, S.ready.size);

    // Execute one step based on the scheduling algorithm
    switch (g_alg) {
        case FCFS:
            fprintf(stderr, "[DEBUG] Executing FCFS step\n");
            step_fcfs();
            break;
        case RR:
            fprintf(stderr, "[DEBUG] Executing Round-Robin step\n");
            step_rr();
            break;
        case MLFQ:
            fprintf(stderr, "[DEBUG] Executing MLFQ step\n");
            step_mlfq();
            break;
        default:
            fprintf(stderr, "[ERROR] Unknown scheduling algorithm %d\n", g_alg);
            pthread_mutex_unlock(&sim_mtx);
            return 0; // Exit if the algorithm is invalid
    }

    // Increment clock and fill snapshot
    clk++;
    fprintf(stderr, "[DEBUG] Clock incremented to %d\n", clk);

    if (out) {
        fprintf(stderr, "[DEBUG] Filling snapshot at out=%p\n", (void*)out);
        fill_snapshot(out);
    } else {
        fprintf(stderr, "[ERROR] Output snapshot pointer is NULL!\n");
    }

    // Determine if there are still processes alive
    int alive = (finished < plen);
    fprintf(stderr, "[DEBUG] sim_step returning alive=%d\n", alive);

    pthread_mutex_unlock(&sim_mtx);
    return alive;
}

/* ─────── FCFS one‑tick ─────── */
static void step_fcfs(void)
{
    for(int i=0;i<plen;i++)
        if(plist[i].arrivalTime!=-1 && clk==plist[i].arrivalTime){
            add_program_to_memory(plist,i,&S.ready);
            fprintf(stderr,"[DBG] step_fcfs: queued pid=%d, Program_start_locations[%d]=%p\n",i, i, (void*)Program_start_locations[i]);
        }   
    if(!isEmpty(&S.ready)){
        struct MemoryWord *p = peek(&S.ready);        
        fprintf(stderr,"[DBG] step_fcfs: about to exec pid=%s ",p->arg1);
        if(execute_an_instruction(p)){
            fprintf(stderr,"[DBG] step_fcfs: pid %s just terminated\n", p->arg1);
            dequeue(&S.ready);
            finished++;
        }
    }
}

/* ─────── RR one‑tick ─────── */
static void step_rr(void)
{
    for(int i=0;i<plen;i++)
        if(plist[i].arrivalTime!=-1 && clk==plist[i].arrivalTime)
            add_program_to_memory(plist,i,&S.ready);

    if(!S.running && !isEmpty(&S.ready))
        S.running = peek(&S.ready);

    if(!S.running) return;

    if(can_execute_instruction(S.running)){
        if(execute_an_instruction(S.running)){
            dequeue(&S.ready);
            finished++;
            S.running=NULL; S.cur_q=0;
            return;
        }
        S.cur_q++;
        if(S.cur_q==g_quantum){
            struct MemoryWord *tmp = dequeue(&S.ready);
            enqueue(&S.ready,tmp,atoi(tmp[2].arg1));
            S.cur_q=0; S.running=NULL;
        }
    }else{
        struct MemoryWord *tmp = dequeue(&S.ready);
        enqueue(get_blocking_queue(tmp),tmp,atoi(tmp[2].arg1));
        S.cur_q=0; S.running=NULL;
    }
}

/* ─────── MLFQ helpers ─────── */
static inline int level_quant(int lvl){ return 1<<lvl; }

/* ─────── MLFQ one‑tick ─────── */
static void step_mlfq(void)
{
    const int LVL = 4;

    for(int i=0;i<plen;i++)
        if(plist[i].arrivalTime!=-1 && clk==plist[i].arrivalTime){
            add_program_to_memory(plist,i,&S.q[0]);
            S.cur_level[i]=0;
            S.rem_q[i]=level_quant(0);
        }

    /* simple: no explicit unblocking logic here, semSignal already moves */

    if(!S.ml_running){
        for(int l=0;l<LVL;l++)
            if(!isEmpty(&S.q[l])){
                S.ml_running = dequeue(&S.q[l]);
                S.ml_pid     = atoi(S.ml_running->arg1);
                if(S.rem_q[S.ml_pid]==0)
                    S.rem_q[S.ml_pid]=level_quant(l);
                S.cur_level[S.ml_pid]=l;
                break;
            }
    }
    if(!S.ml_running) return;

    if(execute_an_instruction(S.ml_running)){
        finished++;
        S.ml_running=NULL;
        return;
    }

    S.rem_q[S.ml_pid]--;
    if(S.rem_q[S.ml_pid]==0){
        int old = S.cur_level[S.ml_pid];
        int nxt = (old<LVL-1)?old+1:old;
        S.cur_level[S.ml_pid]=nxt;
        S.rem_q[S.ml_pid]=level_quant(nxt);
        enqueue(&S.q[nxt],S.ml_running,0);
        S.ml_running=NULL;
    }
}

/* ─────── snapshot − uses queue->items array ─────── */
static void fill_snapshot(SimSnapshot *o)
{
    fprintf(stderr,
        "[DEBUG] fill_snapshot: out=%p, clk=%d, finished=%d, plen=%d\n",
        (void*)o, clk, finished, plen);

    if (!o) {
        fprintf(stderr, "[ERROR] fill_snapshot: output pointer is NULL!\n");
        return;
    }

    memset(o, 0, sizeof *o);
    o->clock      = clk;
    o->algorithm  = g_alg;
    o->procs_total= plen;

    // dump each program’s base pointer
    for (int i = 0; i < plen; i++) {
        fprintf(stderr,
            "[DEBUG] Program_start_locations[%d] = %p\n",
            i, (void*)Program_start_locations[i]);
        struct MemoryWord *mem = Program_start_locations[i];
        if (!mem) {
            fprintf(stderr,
                "[ERROR] Program %d has no memory block! skipping proc fill\n", i);
            continue;
        }

        o->proc[i].pid   = i;
        o->proc[i].state = atoi(mem[1].arg1);
        o->proc[i].pc    = atoi(mem[3].arg1);
        o->proc[i].prio  = atoi(mem[2].arg1);
        o->proc[i].mem_lo= atoi(mem[4].arg1);
        o->proc[i].mem_hi=      mem[4].arg2;
    }

    // ready queue
    fprintf(stderr,
        "[DEBUG] ready queue: items=%p, size=%d\n",
        (void*)S.ready.items, S.ready.size);
    o->ready_len = S.ready.size;
    for (int i = 0; i < S.ready.size; i++) {
        o->ready[i] = atoi(S.ready.items[i].ptr[0].arg1);
    }

    // blocked queues & resource availability
    for (int r = 0; r < NUM_RESOURCES; r++) {
        MemQueue *bq = BlockingQueues[r];
        
        // <<< DROP‑IN DEBUG: catch a NULL queue pointer
        if (!bq) {
            fprintf(stderr,
                "[ERROR] BlockingQueues[%d] is NULL! skipping blocked fill\n", r);
            o->block_len[r] = 0;
            o->res_free[r]  = false;
            continue;
        }

        fprintf(stderr,
            "[DEBUG] fill_snapshot: BlockingQueues[%d] ➞ %p (size=%d), res_free=%d\n",
            r,
            (void*)bq,
            bq->size,
            Resources_availability[r]
        );
        o->block_len[r] = bq->size;
        for (int i = 0; i < bq->size; i++) {
            o->block[r][i] =
                atoi(bq->items[i].ptr[0].arg1);
        }
        o->res_free[r] = Resources_availability[r];
    }
}
