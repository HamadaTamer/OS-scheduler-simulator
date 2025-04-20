#include "utilities.h"


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
        //fprintf(stderr, "PriorityQueue is empty\n");
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
