#include "utilities.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>


#include <pthread.h>
#include "sim.h"

static pthread_mutex_t sim_mtx = PTHREAD_MUTEX_INITIALIZER;
static struct program *g_plist;
static int g_plen;



#define MAX_PROGRAMS  100
int curr_level[MAX_PROGRAMS];
int rem_quantum[MAX_PROGRAMS];


/*
    create memory to store within it the code of a program, variables, PCB
    the memory will be divided into words where each word can be a PCB entry, program instructions, or variables in the form of a single name and the corresponding data ex: 
        
        state: "ready"
        (variableName_dataType): (value) 
        instruction: (some instruction) (unparsed) 

    now each word should be how many bytes?? i suggest 30 bytes


    Memory layout (60 words ):
        Instructions (variable but is currently 7-9)  (i suggest we add an END instruction/ indicator after the instructions to indicate that the program is DONE)
        PCB (5 values)
        Variables ( we only need space for 3 variables )

        since we have 3 programs we have to have 20 words/program

    the memory will be in the form of allocating using calloc the required number of words each of which with the wanted size

    
    the struct will contain the following:
        1. the program instructions in an array or sth 
        2. the PCB inside the struct:  
            Process ID (The process is given an ID when is being created)
            Process State
            Current Priority
            Program Counter
            Memory Boundaries
    
    create 


    scheduler:
        global array storing starting positions of every process
        first 5 PCB, next 3 variables
        pass memory location of process to be executed

        ** will treat each process as its starting location in memory **

        in general we should have ready queue, blocking queue and current running process

        FCFS: will have to have a queue that stores the programs in their order of arrival and only deques when the first program is done execution

        round robin: have to have a queue where after every quanta we deque and enque

        multi level feedback: 
            1. will have to have 4 queues, first is of highest priority, then second and so on
            2. quanta doubles with each level of queue except last queue it is round robin

        scheduler will take the processes and their arrival times 

        problem is in re-enqueuing previously blocked processes
        what we can do then is:
            in case we use RR / FCFS we can call a function that does this else call a function that does that


 */


 
int MemorySize = 150;

static int PCBID = 0; // a global counter of Process IDs in order to be tracked globally in other words ID to be used by next process initiated   

SCHEDULING_ALGORITHM algo;

//makes memory globally accessible:
struct MemoryWord *Memory_start_location;
//  struct MemoryWord *Program_start_locations[MAX_PROGRAMS] = {0};

// initializnig global queues for simple access
MemQueue readyQueueNotPtr;
MemQueue *readyQueue  ;

// MemQueue BlockingQueuesNotPtrs[NUM_RESOURCES];
// MemQueue *BlockingQueues[NUM_RESOURCES];



// MLFQ queues need global access since we need to enqueue from blocking queues into them

MemQueue MLFQ_queues_not_ptrs[4];
MemQueue *MLFQ_queues[4];



// // initializing the array to have 3 resources, and so that with the enum i can point to the corresponding resource in a readable format 
// bool Resources_availability[NUM_RESOURCES] = {true,true,true};   

// // structs defining the MemoryWord layout
// struct program{
//     char programName[50];
//     int priority;
//     int arrivalTime;
// };



struct MemoryWord * createMemory(){


    // creating program memory
    struct MemoryWord *arr = calloc(MemorySize, sizeof *arr);
    Memory_start_location = arr;
    if (!arr){
        perror("failed to create memory ");
        exit(EXIT_FAILURE);
    }

    return arr;
}

/* core/sim.c */
static struct program *plist;   /* set once in sim_init() */
static int  plen;

/* global clock & “all done?” */
static int  clk;
static int  finished;

/* algorithm‑specific state */
static struct {
    /* FCFS & RR */    MemQueue ready;
    /* RR only  */     int current_quanta; struct MemoryWord *running;
    /* MLFQ only*/     MemQueue q[4];      int curr_level[MAX_PROGRAMS];
} s;

/* ---------------------------------------------------- */
static void step_fcfs(void);
static void step_rr(void);
static void step_mlfq(void);




void dumpMemory(struct MemoryWord *memory) {
    printf("── Memory Dump ───────────────────────────\n");
    for (int i = 0; i < MemorySize; i++) {
        if (atoi(memory[i].arg1) == 0)
        printf("[%d]: id=\"%s\", arg1=\"%s\", arg2=%d\n",
               i,
               memory[i].identifier,
               memory[i].arg1,
               memory[i].arg2);
        else
            printf("[%d]: id=\"%s\", arg1=\"%d\", arg2=%d\n",
                i,
                memory[i].identifier,
                memory[i].arg1,
                memory[i].arg2);
    }
    
    printf("───────────────────────────────────────────\n\n");
}


void parseProgram(char *filename, struct MemoryWord *memory) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    memory += 8;  // skip first 8 words (reserved for PCB)
    char line[100];
    int index = 0;

    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0) continue; // skip empty lines

        if (index >= 60 - 8) { // protect memory overflow
            fprintf(stderr, "Error: Program too large for memory\n");
            exit(EXIT_FAILURE);
        }

        strncpy(memory[index].identifier, trimmed, sizeof(memory[index].identifier) - 1);
        memory[index].identifier[sizeof(memory[index].identifier) - 1] = '\0';  // null-terminate

        index++;
    }

    // Add EOI marker
    strcpy(memory[index].identifier, "EOI");

    fclose(file);
}



// creating the PCB

void createPCB(struct MemoryWord *memory, int priority){

    /*
    PCB is made up of 5 consecutive words for the following attributes:
        1.Process ID (depends on the global counter)
        2. Process state (states are: new, ready, running, waiting (blocked), terminated )
        3. current priority (initialzed with supplied memory address)
        4. Program counter (initialized with supplied memory address )
        5. Memory boundaries // fixed at supplied memory location for start, and supplied mem location + number of instructions + 1 (EOI) + 5 (PCB) + (number of variables)
    */
    

    int i = 0;

    // finding EOI

    // Process ID
    strcpy(memory[i].identifier, "ID");
    sprintf(memory[i].arg1, "%d", PCBID++);
    
    // Process State
    i++;
    strcpy(memory[i].identifier, "State");
    sprintf(memory[i].arg1, "%d", NEW);
    
    // Priority
    i++;
    strcpy(memory[i].identifier, "Current_priority");
    sprintf(memory[i].arg1, "%d", priority);
    
    // Program Counter
    i++;
    strcpy(memory[i].identifier, "Program_counter");
    sprintf(memory[i].arg1, "%d", 0);
    
    // Memory Boundaries
    i++;
    strcpy(memory[i].identifier, "Memory_Bounds");
    sprintf(memory[i].arg1, "%d",memory - Memory_start_location );           // Lower bound 

    int j=0;
    while (strcmp(memory[j].identifier, "EOI") != 0) j++;   // get location of last instruction

    memory[i].arg2 =   memory + j - Memory_start_location;       // Upper bound
}


// this method looks in the memory for a variable and returns its offset in memory from beginning of program
int lookupValue(struct MemoryWord *memory, const char *var) {
    for (int i = 5; i < 8; i++) {
        char *entry = memory[i].identifier;
        if (strcmp( memory[i].identifier, var) == 0){
            return i ;
        }
    }            
    return -1;
}


// helper: check if a file exists on disk
bool fileExists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

// helper: read entire file into one malloc'd string
char *getFileContent(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *result = malloc(size + 1);
    fread(result, 1, size, f);
    result[size] = '\0';
    fclose(f);
    return result;
}


void assignValue(char * line, struct MemoryWord *memory){
    char *lhs = strtok(NULL, " \n");
    char *rhs = strtok(NULL, " \n");
    if (!lhs || !rhs ) {
        perror("error in assign statement syntax");
        exit(EXIT_FAILURE);
    }

    if (strcmp(rhs, "input") == 0) {
        // 1) find free slot in memory[5..7]
        int slot = -1;
        for (int j = 5; j < 8; ++j) {
            if (memory[j].identifier[0] == '\0') {
                slot = j;
                break;
            }
        }
        if (slot < 0) {
            fprintf(stderr, "Error: no place in memory for variables\n");
            exit(EXIT_FAILURE);
        }
    
        // 2) record the variable name
        strncpy(memory[slot].identifier, lhs,
                sizeof memory[slot].identifier - 1);
        memory[slot].identifier[
            sizeof memory[slot].identifier - 1] = '\0';
    
        // 3) prompt and read entire line, including spaces
        printf("Enter the value of %s:\n", lhs);
        char buf[200];
        if (!fgets(buf, sizeof buf, stdin)) {
            perror("input");
            exit(EXIT_FAILURE);
        }
        // strip trailing newline
        buf[strcspn(buf, "\n")] = '\0';
    
        // 4) store verbatim in memory slot
        strncpy(memory[slot].arg1, buf,
                sizeof memory[slot].arg1 - 1);
        memory[slot].arg1[
            sizeof memory[slot].arg1 - 1] = '\0';
    }else if (strcmp(rhs, "readFile") == 0) {
        char *fv = strtok(NULL, " \n");
        if (!fv) {
            fprintf(stderr, "Syntax error: missing filename or variable name\n");
            exit(EXIT_FAILURE);
        }
    
        char tmp[100] = {0};
        FILE *file = fopen(fv, "r");
    
        if (file) {
            // Happy case: `fv` is a direct filename
            if (fgets(tmp, sizeof tmp, file)) {
                tmp[strcspn(tmp, "\n")] = '\0';
            }
            fclose(file);
        } else {
            // Try treating `fv` as a variable name
            int floc = lookupValue(memory, fv);
            if (floc < 0) {
                fprintf(stderr, "Error: '%s' is neither a file nor a valid variable\n", fv);
                exit(EXIT_FAILURE);
            }
    
            char *fname = memory[floc].arg1;
    
    
            FILE *fi = fopen(fname, "r");
            if (!fi) {
                fprintf(stderr, "Error: cannot open file '%s'\n", fname);
                exit(EXIT_FAILURE);
            }
    
            if (fgets(tmp, sizeof tmp, fi)) {
                tmp[strcspn(tmp, "\n")] = '\0';
            }
            fclose(fi);
        }
    
        // Store the result in the next free memory slot [5..7]
        int slot = -1;
        for (int i = 5; i < 8; i++) {
            if (memory[i].identifier[0] == '\0') {
                slot = i;
                break;
            }
        }
    
        if (slot < 0) {
            fprintf(stderr, "Error: no free memory slot available\n");
            exit(EXIT_FAILURE);
        }
    
        strcpy(memory[slot].identifier, lhs);
        strcpy(memory[slot].arg1, tmp);
        memory[slot].arg2 = 0;
        return;
    }else {

        char *endptr;
        long ival = strtol(rhs, &endptr, 10);
        int is_int = (*endptr == '\0');
        size_t rlen = strlen(rhs);
        int is_str = (rlen >= 2 && rhs[0]=='\"' && rhs[rlen-1]=='\"');

        if (is_int || is_str) {
            // find next free slot in memory[5..8]
            int slot = -1;
            for (int i = 5; i < 8; i++) {
                if (memory[i].identifier[0] == '\0') {
                    slot = i;
                    break;
                }
            }
            if (slot == -1) {
                perror("no place in memory for variables");
                exit(EXIT_FAILURE);
            }

            // store the variable name
            strncpy(memory[slot].identifier,
                    lhs,
                    sizeof memory[slot].identifier - 1);
            memory[slot].identifier[sizeof memory[slot].identifier - 1] = '\0';

            if (is_int) {
                // integer literal
                sprintf(memory[slot].arg1,"%d" ,ival);
            } else {
                // string literal: strip the quotes
                rhs[rlen-1] = '\0';
                strncpy(memory[slot].arg1,
                        rhs + 1,
                        sizeof memory[slot].arg1 - 1);
                memory[slot].arg1[sizeof memory[slot].arg1 - 1] = '\0';
            }
        }
    }

}  


void print_variable(const char* var) {
    char* endptr;

    // Try to convert the string to a long
    long int_val = strtol(var, &endptr, 10);

    // Check if the entire string was consumed by strtol
    if (*endptr == '\0') {
        printf("Integer variable : %ld\n", int_val);
    } else {
        printf("String variable: %s\n", var);
    }
}


// execute one instruction from the process pointed at by given parameter, and return true if program finished execution
bool execute_an_instruction( struct MemoryWord *memory){
    int pc = atoi(memory[3].arg1);
    int base = 8;  // because memory += 8 during parsing
    char *line = memory[base + pc].identifier;

    // Copy line to avoid messing with original
    char buffer[100];
    strcpy(buffer, line);

    char *cmd = strtok(buffer, " \n");

    /* ------------- NEW: stop if we are already at EOI -------------- */
    if (strcmp(memory[base + pc].identifier, "EOI") == 0)
        return true;                             /* program finished   */
    /* ----------------------------------------------------------------*/

    if (strcmp(cmd, "assign") == 0) {
        assignValue(cmd, memory);
    }else if (strcmp(cmd, "print") == 0) {
        char *var = strtok(NULL, " \n");
        // lookup var and print
        //char * rhs = strtok(cmd," \n");
        int loc = lookupValue(memory, var);
        if (loc < 0) {
            fprintf(stderr, "No variable with the given name\n");
            exit(EXIT_FAILURE);
        }    

        print_variable(memory[loc].arg1);
        
    }else if (strcmp(cmd, "writeFile") == 0) {
        // Grab the two raw tokens
        char *fileNameTok    = strtok(NULL, " \n");
        char *fileContentTok = strtok(NULL, " \n");
    
        // Resolve filename: if it’s a variable, use its value; else use literal
        int fnLoc = lookupValue(memory, fileNameTok);
        char *fname = (fnLoc >= 0
                       ? memory[fnLoc].arg1
                       : fileNameTok);
    
        // Resolve content similarly
        int fcLoc = lookupValue(memory, fileContentTok);
        char *content = (fcLoc >= 0
                         ? memory[fcLoc].arg1
                         : fileContentTok);
    
        // Open and write
        FILE *fptr = fopen(fname, "w");
        if (!fptr) {
            perror("writeFile: cannot open file");
            exit(EXIT_FAILURE);
        }
        fprintf(fptr, "%s", content);
        fclose(fptr);
    }
    else if(strcmp(cmd, "readFile") == 0){
        // no reason to read file and do nothing with it so can only logically be called with assign
        printf("no reason to read file and do nothing with it so can only logically be called with assign unless en enta bethazar");
    }else if (strcmp(cmd, "printFromTo") == 0) {
        // Fetch the two arguments as strings
        char *arg1 = strtok(NULL, " \n");
        char *arg2 = strtok(NULL, " \n");
    
        int x, y;
    
        // If arg1 is a variable in memory, use its value; else parse as literal
        int loc1 = lookupValue(memory, arg1);
        if (loc1 >= 0) {
            x = atoi(memory[loc1].arg1);
        } else {
            x = atoi(arg1);
        }
    
        // Same for arg2
        int loc2 = lookupValue(memory, arg2);
        if (loc2 >= 0) {
            y = atoi(memory[loc2].arg1);
        } else {
            y = atoi(arg2);
        }
    
        // Now print from x to y
        while (x <= y) {
            printf("%d ", x++);
        }
        printf("\n");
    }
    else if (strcmp(cmd, "semWait") == 0){
        char * resource = strtok(NULL, " \n");
        Resources tmp; 
        if (strcmp(resource, "userInput") == 0)
            tmp = USER_INPUT;
        else if (strcmp(resource, "userOutput") == 0)
            tmp = USER_OUTPUT;
        else if (strcmp(resource, "file") == 0)
            tmp = FILE_ACCESS;
        else {
            perror("invalid resource allocation request");
            exit(EXIT_FAILURE);
        }
        Resources_availability[tmp] = false;
       
    }else if (strcmp(cmd, "semSignal") == 0){
        char * resource = strtok(NULL, " \n");
        Resources tmp; 
        if (strcmp(resource, "userInput") == 0)
            tmp = USER_INPUT;
        else if (strcmp(resource, "userOutput") == 0)
            tmp = USER_OUTPUT;
        else if (strcmp(resource, "file") == 0)
            tmp = FILE_ACCESS;
        else {
            perror("invalid resource allocation request");
            exit(EXIT_FAILURE);
        }
        Resources_availability[tmp] = true;
        if (algo != MLFQ){
            while(peek(BlockingQueues[tmp]) != NULL){
                struct MemoryWord *tmp2 =  dequeue(BlockingQueues[tmp]);
                enqueue(readyQueue, tmp2, atoi(tmp2[2].arg1));
            }
        }else {
            while( peek(BlockingQueues[tmp]) != NULL){
                struct MemoryWord *tmp2 =  dequeue(BlockingQueues[tmp]);
                enqueue(MLFQ_queues[atoi(tmp2[2].arg1)], tmp2, atoi(tmp2[2].arg1));
            }
        }
    }else{
        perror("command entered is not proper!!");
        exit(EXIT_FAILURE);
    }

    // Handle semWait, semSignal, etc...

    // Finally increment PC
    sprintf(memory[3].arg1, "%d", pc + 1);
    // return true if program finished execution
    //printf("this shit => %d\n", memory[pc+base].identifier);

    if (strcmp(memory[pc+1+base].identifier,"EOI") == 0 ) return true; 

    return false;
}

bool can_execute_instruction(struct MemoryWord* memory){
    int pc = atoi(memory[3].arg1);
    int base = 8;  // because memory += 8 during parsing
    char *line = memory[base + pc].identifier;

    // Copy line to avoid messing with original
    char buffer[100];
    strcpy(buffer, line);

    char *cmd = strtok(buffer, " \n"); 
    if (strcmp(cmd, "semWait") == 0 ){
        char * resource = strtok(NULL, " \n");
        Resources tmp; 
        if (strcmp(resource, "userInput") == 0)
            tmp = USER_INPUT;
        else if (strcmp(resource, "userOutput") == 0)
            tmp = USER_OUTPUT;
        else if (strcmp(resource, "file") == 0)
            tmp = FILE_ACCESS;
        else {
            perror("invalid resource allocation request");
            exit(EXIT_FAILURE);
        }
        return Resources_availability[tmp];            
    }
    return true;    
}

MemQueue*  get_blocking_queue(struct MemoryWord* memory){
    int pc = atoi(memory[3].arg1);
    int base = 8;  // because memory += 8 during parsing
    char *line = memory[base + pc].identifier;

    // Copy line to avoid messing with original
    char buffer[100];
    strcpy(buffer, line);

    char *cmd = strtok(buffer, " \n"); 
    if (strcmp(cmd, "semWait") == 0 ){
        char * resource = strtok(NULL, " \n");
        Resources tmp; 
        if (strcmp(resource, "userInput") == 0)
            tmp = USER_INPUT;
        else if (strcmp(resource, "userOutput") == 0)
            tmp = USER_OUTPUT;
        else if (strcmp(resource, "file") == 0)
            tmp = FILE_ACCESS;
        else {
            perror("invalid resource allocation request");
            exit(EXIT_FAILURE);
        }
        return BlockingQueues[tmp];            
    }
    return NULL;          /* <<< and add a safe default here      */
}

void add_program_to_memory(struct program programList[] , int i, MemQueue *queue_to_be_used){
    if (PCBID == 0)
        Program_start_locations[PCBID] = Memory_start_location;
    else{
        //printf("offset => %d\n ",Program_start_locations[PCBID-1][4].arg2 +1 );
    Program_start_locations[PCBID] =Memory_start_location +Program_start_locations[PCBID-1][4].arg2 +1  ;
    }
    struct MemoryWord *curr_program_memory = Program_start_locations[PCBID];

    // enquing the process into the ready queue
    enqueue(queue_to_be_used,curr_program_memory, atoi(curr_program_memory[2].arg1) );

    //adding the program instructions into the memory
    parseProgram( programList[i].programName,curr_program_memory);

    //creating the programs PCB in memory this also increments the PCBID
    createPCB(curr_program_memory, programList[i].priority);
    // printf("Clock %2d: Program %d (mem@%d) arrived and enqueued. Queue size=%d\n",clockcycles, i, curr_program_memory, readyQueue.rear - readyQueue.front - 1);
    programList[i].arrivalTime = -1;
}

void FCFS_algo(struct program programList[] , int num_of_programs){
    int clockcycles = 0;
    int completed = 0;
    while(completed < num_of_programs){
        for (int i = 0; i < num_of_programs; i++){

            // checking if a program should be added into memory
            if ( programList[i].arrivalTime != -1 && clockcycles == programList[i].arrivalTime){
               add_program_to_memory(programList, i, readyQueue);
            }

        }
        //execute the process that has its turn
        if (peek(readyQueue) != NULL){
            int pc =  atoi(peek(readyQueue)[3].arg1)+8;
            printf("Clock %2d: Running prog %d, PC=%d, instr='%s'\n",clockcycles, atoi(peek(readyQueue)[0].arg1) ,pc,peek(readyQueue)[pc].identifier  );
            if (execute_an_instruction(peek(readyQueue))){
                dequeue(readyQueue);
                completed++;
            }
        }
        clockcycles++;
    }
}


void RR_algo(struct program programList[] , int num_of_programs, int Quanta ){
    int clockcycles = 0;
    // check arrivals first then move just executed process to back of queue
    int current_quanta = 0;
    int completed = 0;
    struct MemoryWord* current_process = NULL;    

    while(completed < num_of_programs){
        for (int i = 0; i < num_of_programs; i++){
            // checking if a program should be added into memory
            if ( programList[i].arrivalTime != -1 && clockcycles == programList[i].arrivalTime){
               add_program_to_memory(programList, i, readyQueue);
            }

        }
        //execute the process that has its turn
        if (peek(readyQueue) != NULL){

            // finding the next process to execute
            if (current_quanta == 0){
                current_process = peek(readyQueue);
            }
            // continuing the execution of the current process
            // check if we can execute instruction and if so then we execute
            //dumpMemory(Memory_start_location);
            int pc =  atoi(peek(readyQueue)[3].arg1)+8;
           // printf("trying    => Clock %2d: Running prog %d, PC=%d, instr='%s'\n",clockcycles, atoi(peek(readyQueue)[0].arg1) ,pc,peek(readyQueue)[pc].identifier );
                
            if (can_execute_instruction(current_process)){
                int pc =  atoi(peek(readyQueue)[3].arg1)+8;
                printf("executing => Clock %2d: Running prog %d, PC=%d, instr='%s'\n",clockcycles, atoi(peek(readyQueue)[0].arg1) ,pc,peek(readyQueue)[pc].identifier );
                //printQueue();
                // checking if last instruction and resetting the quanta
                // executing the instruction, will ready the corresponding blocked processes in case of semSignal  
                if (execute_an_instruction(current_process)){
                    dequeue(readyQueue);
                    current_quanta = 0;
                    completed++;
                }
                else {
                    current_quanta++;
                    if (current_quanta == Quanta){
                        struct MemoryWord *tmp=  dequeue(readyQueue);
                        enqueue(readyQueue, tmp,atoi(tmp[2].arg1));
                        current_quanta = 0;
                    }
                }
                clockcycles++;
            }
            // if we cant execute an instruction it must be due to resource blocking so we must place in the appropriate blocked queue
            else{
                struct MemoryWord *tmp=  dequeue(readyQueue);
                enqueue(get_blocking_queue(current_process), tmp,atoi(tmp[2].arg1) );
                current_quanta = 0;

            }                
        }else{
            clockcycles++;
        }
    }
}



void MLFQ_algo(struct program programList[], int number_of_programs) {
    const int num_levels = 4;
    // initialize the 4 ready–queues
    for (int lvl = 0; lvl < num_levels; ++lvl) {
        initQueue(&MLFQ_queues_not_ptrs[lvl]);
        MLFQ_queues[lvl] = &MLFQ_queues_not_ptrs[lvl];
    }
    // timeslice for each level = 2^(lvl+1): Q0=2, Q1=4, Q2=8, Q3=16
    int quantum_per_level[num_levels];
    for (int lvl = 0; lvl < num_levels; ++lvl){
        quantum_per_level[lvl] = 1 << lvl;
    }
    printf("\n");
    // per‐program MLFQ state
    for (int p = 0; p < number_of_programs; ++p) {
        curr_level[p]   = -1;
        rem_quantum[p]  = 0;
    }

    int completed = 0;
    int clock = 0;
    struct MemoryWord *running = NULL;
    int run_pid = -1;

    while (completed < number_of_programs) {
        // —— first: unblock any processes just signaled ——
        for (int r = 0; r < NUM_RESOURCES; ++r) {
            while (Resources_availability[r] && !isEmpty(BlockingQueues[r])) {
                struct MemoryWord *mw = dequeue(BlockingQueues[r]);
                int pid = atoi(mw->arg1);
                int lvl = curr_level[pid];
                enqueue(MLFQ_queues[lvl], mw, 0);
                printf("[C=%3d] UNBLOCK → pid=%d back to Q%d\n", clock, pid, lvl);
            }
        }
        // —— arrivals ——
        for (int p = 0; p < number_of_programs; ++p) {
            if (programList[p].arrivalTime == clock) {
                add_program_to_memory(programList, p, MLFQ_queues[0]);
                curr_level[p]  = 0;
                rem_quantum[p] = quantum_per_level[0];
                //printf("[C=%3d] ARRIVE → pid=%d in Q0\n", clock, p);
            }
        }
        // —— preempt if a higher‐priority queue is non‐empty ——
        int highest_ready = -1;
        for (int lvl = 0; lvl < num_levels; ++lvl) {
            if (!isEmpty(MLFQ_queues[lvl])) { highest_ready = lvl; break; }
        }
        if (running && highest_ready != -1 && highest_ready < curr_level[run_pid]) {
            // preempt current
           // printf("[C=%3d] PREEMPT → pid=%d lvl=%d rem_q=%d\n",clock, run_pid, curr_level[run_pid], rem_quantum[run_pid]);
            enqueue(MLFQ_queues[curr_level[run_pid]], running, 0);
            running = NULL;
        }
        // —— dispatch if CPU is free ——
        if (!running) {
            int sel_lvl = -1;
            for (int lvl = 0; lvl < num_levels; ++lvl) {
                if (!isEmpty(MLFQ_queues[lvl])) { sel_lvl = lvl; break; }
            }
            if (sel_lvl != -1) {
                running = dequeue(MLFQ_queues[sel_lvl]);
                run_pid = atoi(running->arg1);
                // ensure rem_quantum is set (for freshly arrived or demoted)
                if (rem_quantum[run_pid] == 0)
                    rem_quantum[run_pid] = quantum_per_level[sel_lvl];
                curr_level[run_pid] = sel_lvl;
                //printf("[C=%3d] DISPATCH → pid=%d from Q%d rem_q=%d\n",clock, run_pid, sel_lvl, rem_quantum[run_pid]);
            }
        }
        //print all queues
      
        // —— execute one time unit ——
        if (running) {
            int lvl = curr_level[run_pid];
            int pc  = atoi(running[3].arg1);
            char *instr = running[8 + pc].identifier;
            //printf("[C=%3d] RUN      pid=%d lvl=%d pc=%d instr=\"%s\" rem_q=%d\n", clock, run_pid, lvl, pc, instr, rem_quantum[run_pid]);
            bool finished = execute_an_instruction(running);
            if (finished) {
                completed++;
                //printf("FINISHED → pid=%d (done=%d)\n", run_pid, ++completed);
                running = NULL;
            } else {
                rem_quantum[run_pid]--;
                if (rem_quantum[run_pid] == 0) {
                    int old_lvl = lvl;
                    if (curr_level[run_pid] < num_levels - 1)
                        curr_level[run_pid]++;
                    rem_quantum[run_pid] = quantum_per_level[curr_level[run_pid]];
                    //printf("           TIMESLICE→ pid=%d demote→Q%d rem_q=%d\n",run_pid, curr_level[run_pid], rem_quantum[run_pid]);
                    enqueue(MLFQ_queues[curr_level[run_pid]], running, 0);
                    running = NULL;
                }
            }
        }
        clock++;
        // for (int lvl = 0; lvl < num_levels; ++lvl) {
        //     printf("Q%d: ", lvl);
        //     printQueue(MLFQ_queues[lvl], lvl);
        // }
    }
    printf("[DONE] All %d progs done at clock %d\n", number_of_programs, clock);
}




void scheduler(struct program programList[] , int num_of_Programs){
    // initialize ready queue, blocking queue, and running process
    struct MemoryWord *runningProcessLocation = NULL;
    
    initQueue(&readyQueueNotPtr);
    readyQueue = &readyQueueNotPtr;

    for (size_t i = 0; i < NUM_RESOURCES; i++)
    {
        initQueue(&BlockingQueuesNotPtrs[i]);
        BlockingQueues[i] = &BlockingQueuesNotPtrs[i];
    }
    
    //setting the scheduling algorithm
    algo = MLFQ; 
    int quanta = 2; 
    switch (algo)
    {
    case FCFS:
        FCFS_algo(programList, num_of_Programs);
        break;
    case RR:
        RR_algo(programList, num_of_Programs,quanta );
        break;
    case MLFQ:
        MLFQ_algo(programList, num_of_Programs);
        break;
    }
    
}





// int main() {
//     // Create and zero memory
//     Memory_start_location = createMemory();

    
//     /*
//     struct program{
//         char programName[50];
//         int priority;
//         int arrivalTime;
//     };
//     */
//     struct program programList[3] = {
//           {"Program_1.txt" , 0, 0, },
//         {"Program_2.txt" , 0, 2, },
//         {"Program_3.txt" , 0, 4, }
//     };  

//     scheduler(programList, 3);

//     free(Memory_start_location);
//     return 0;
// }

/*
questions:
    in RR,  if a process finishes before the quanta finishes , is the quanta reset to 0 for the next process or does it continue from where the previous process left 
    

*/