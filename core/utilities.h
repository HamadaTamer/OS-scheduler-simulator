#ifndef UTILITIES_H
#define UTILITIES_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>   // for PRIu64
#define MAX_QSIZE  60  // or whatever max you need
#define MAX_PROGRAMS 100


char *ltrim(char *s);
char *rtrim(char *s);
char *trim (char *s);



typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} process_state;

typedef enum {
    FILE_ACCESS,
    USER_INPUT,
    USER_OUTPUT,
    NUM_RESOURCES
} Resources;

typedef enum {
    FCFS,
    RR,
    MLFQ
} SCHEDULING_ALGORITHM;

struct MemoryWord {
    char identifier[100];
    char arg1[100];
    int  arg2;    // you can still use this or ignore it
};


// ——— a node in our heap ———
typedef struct {
    struct MemoryWord *ptr;  // the user payload
    int                priority;
    uint64_t           seqno; // tie‑breaker: lower = older
} PQNode;

// ——— the priority queue itself ———
typedef struct {
    PQNode  items[MAX_QSIZE];
    int     size;
    uint64_t nextSeq;  // for assigning seqno
} MemQueue;

// ——— helper: swap two nodes ———
static void swapNode(PQNode *a, PQNode *b) {
    PQNode tmp = *a;
    *a = *b;
    *b = tmp;
}

// ——— comparator: returns true if a < b in heap order ———
static bool lessThan(const PQNode *a, const PQNode *b) {
    if (a->priority != b->priority)
        return a->priority < b->priority;
    return a->seqno < b->seqno;
}

// ——— bubble up the node at idx ———
static void heapifyUp(MemQueue *q, int idx) {
    if (idx == 0) return;
    int parent = (idx - 1) / 2;
    if (lessThan(&q->items[idx], &q->items[parent])) {
        swapNode(&q->items[idx], &q->items[parent]);
        heapifyUp(q, parent);
    }
}

// ——— push down the node at idx ———
static void heapifyDown(MemQueue *q, int idx) {
    int smallest = idx;
    int left  = 2*idx + 1;
    int right = 2*idx + 2;

    if (left < q->size && lessThan(&q->items[left], &q->items[smallest]))
        smallest = left;
    if (right < q->size && lessThan(&q->items[right], &q->items[smallest]))
        smallest = right;

    if (smallest != idx) {
        swapNode(&q->items[idx], &q->items[smallest]);
        heapifyDown(q, smallest);
    }
}


void initQueue(MemQueue *q);
bool isEmpty(MemQueue *q);
bool isFull(MemQueue *q);
void enqueue(MemQueue *q, struct MemoryWord *ptr, int priority);
struct MemoryWord* peek(MemQueue *q);
int  peekPriority(MemQueue *q);
struct MemoryWord* dequeue(MemQueue *q);
void printQueue(MemQueue *q, int qid);

// ——— public API ———

#endif /* UTILITIES_H */