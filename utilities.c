// utilities (this is just me having OCD lol)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>   // for PRIu64
#define MAX_QSIZE  60  // or whatever max you need


char *ltrim(char *s)
{
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s)
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

char *trim(char *s)
{
    return rtrim(ltrim(s)); 
}


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

// ——— public API ———

// Initialize empty queue
void initQueue(MemQueue *q) {
    q->size    = 0;
    q->nextSeq = 0;
}

// Empty?
bool isEmpty(MemQueue *q) {
    return (q->size == 0);
}

// Full?
bool isFull(MemQueue *q) {
    return (q->size == MAX_QSIZE);
}

/**
 * Enqueue a MemoryWord* with the given priority.
 * Lower priority value → higher scheduling priority.
 * Items with equal priority preserve FIFO via seqno.
 */
void enqueue(MemQueue *q, struct MemoryWord *ptr, int priority) {
    if (isFull(q)) {
        fprintf(stderr, "PriorityQueue is full\n");
        return;
    }
    PQNode *node = &q->items[q->size];
    node->ptr      = ptr;
    node->priority = priority;
    node->seqno    = q->nextSeq++;
    heapifyUp(q, q->size);
    q->size++;
}

/**
 * Peek at the highest‑priority item without removing it.
 * Returns NULL (and prints) if empty.
 */
struct MemoryWord* peek(MemQueue *q) {
    if (isEmpty(q)) {
        fprintf(stderr, "PriorityQueue is empty\n");
        return NULL;
    }
    return q->items[0].ptr;
}

/**
 * (Optional) Peek at the priority of the head.
 * Returns -1 if empty.
 */
int peekPriority(MemQueue *q) {
    if (isEmpty(q)) {
        fprintf(stderr, "PriorityQueue is empty\n");
        return -1;
    }
    return q->items[0].priority;
}

/**
 * Dequeue and return the highest‑priority MemoryWord*.
 * Returns NULL (and prints) if empty.
 */
struct MemoryWord* dequeue(MemQueue *q) {
    if (isEmpty(q)) {
        fprintf(stderr, "PriorityQueue is empty\n");
        return NULL;
    }
    struct MemoryWord *top = q->items[0].ptr;
    // move last node to root
    q->items[0] = q->items[--q->size];
    heapifyDown(q, 0);
    return top;
}

void printQueue(MemQueue *q, int qid) {
    printf("  [Q%d] size=%2d |", qid, q->size);
    for (int j = 0; j < q->size; ++j) {
        struct MemoryWord *mw = q->items[j].ptr;
        printf(" (%s,pr=%d,seq=%" PRIu64 ")",
               mw->arg1,
               q->items[j].priority,
               q->items[j].seqno);
    }
    printf("\n");
}