// utilities (this is just me having OCD lol)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
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
// Defining the Queue structure
typedef struct {
    struct MemoryWord *items[MAX_QSIZE];
    int front;   // index of last‐dequeued
    int rear;    // index of next‐enqueue
} MemQueue;

// Initialize an empty queue
void initQueue(MemQueue *q) {
    q->front = -1;
    q->rear  =  0;
}

// Is the queue empty?
bool isEmpty(MemQueue *q) {
    return (q->front == q->rear - 1);
}

// Is the queue full?
bool isFull(MemQueue *q) {
    return (q->rear == MAX_QSIZE);
}

// Enqueue a pointer to a MemoryWord
void enqueue(MemQueue *q, struct MemoryWord *ptr) {
    if (isFull(q)) {
        fprintf(stderr, "Queue is full\n");
        return;
    }
    q->items[q->rear++] = ptr;
}

// Dequeue and return a pointer (or NULL if empty)
struct MemoryWord* dequeue(MemQueue *q) {
    if (isEmpty(q)) {
        fprintf(stderr, "Queue is empty\n");
        return NULL;
    }
    return q->items[++q->front];
}

// Peek at next element without removing it
struct MemoryWord* peek(MemQueue *q) {
    if (isEmpty(q)) {
        fprintf(stderr, "Queue is empty\n");
        return NULL;
    }
    return q->items[q->front + 1];
}


typedef enum{
    NEW,
    READY, 
    RUNNING,
    WAITING,
    TERMINATED
} process_state;


typedef enum{
    FILE_ACCESS,
    USER_INPUT,
    USER_OUTPUT,
    NUM_RESOURCES
} Resources;

typedef enum{
    FCFS,
    RR,
    MLFQ
} SCHEDULING_ALGORITHM;

struct MemoryWord{
    char identifier[100];  
    char arg1[100];
    int arg2;
};

