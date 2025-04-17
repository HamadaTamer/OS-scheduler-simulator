#include "utilities.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>



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


 */

int MemorySize = 20;

static int PCBID = 0; // a global counter of Process IDs in order to be tracked globally in other words ID to be used by next process initiated   


//makes memory globally accessible:
struct MemoryWord *Memory_start_location;
struct MemoryWord *Program_start_locations[3] = {0};



// structs defining the memoryWord layout
struct program{
    char programName[50];
    int priority;
    int arrivalTime;
};


struct MemoryWord * createMemory(){


    // creating program memory
    struct MemoryWord *arr = calloc(60, sizeof *arr);
    Memory_start_location = arr;
    if (!arr){
        perror("failed to create memory ");
        exit(EXIT_FAILURE);
    }

    return arr;
}

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
    dumpMemory(memory);
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

    if (strcmp(rhs,"input") == 0){
        //search for empty variable position
        int slot = -1;
        int i;
        for (i = 0; i < 4; i++) {
            // print with newline so you see it immediately
            //printf("checking slot %d → \"%s\"\n", i+5, memory[i+5].identifier);
        
            // test the _string_ emptiness, not the pointer
            if (memory[i+5].identifier[0] == '\0') {
                slot = i + 5;
                printf("found empty slot at index %d\n", slot);
                break;
            }
        }
        if (slot < 0) {
            fprintf(stderr, "Error: no place in memory for variables\n");
            exit(EXIT_FAILURE);
        }

        // take input for the variable
        printf("Is the value of %s string or an integer:  (enter 0 for int, 1 for string)\n", lhs);

        int x;
        do{
            scanf("%d", &x);
            char * tmp[100];
            if ( x!= 0 && x!= 1 ){
                printf("0 or 1 bas yasta\n");
            }
        } while ( x != 0 && x != 1 );
        
        strcpy(memory[i+5].identifier ,lhs);
        printf("Enter the value of %s \n", lhs);

        switch (x){
        case 0:
                int tmp;
                scanf("%d", &tmp);                       // read into a real int
                sprintf(memory[slot].arg1, "%d", tmp); 
                break;
            case 1:
                scanf("%s",&memory[i+5].arg1);
        }
            
    } else if (strcmp(rhs, "readfile") == 0) {
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
        printf("Integer detected: %ld\n", int_val);
    } else {
        printf("String detected: %s\n", var);
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
    printf("%s\n",cmd);

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
        
    }else if (strcmp(cmd, "writeFile") == 0){
        char * fileName = strtok(NULL, " \n");
        char * fileContent = strtok(NULL, " \n");

        FILE *fptr;
        // Open a file in writing mode
        fptr = fopen(fileName, "w");

        // Write some text to the file
        fprintf(fptr, fileContent);
        // Close the file
        fclose(fptr); 

    }else if(strcmp(cmd, "readFile") == 0){
        // no reason to read file and do nothing with it so can only logically be called with assign
        printf("no reason to read file and do nothing with it so can only logically be called with assign unless en enta bethazar");
    }else if(strcmp(cmd, "printFromTo") == 0){
        int x = atoi(strtok(NULL, " \n"));
        int y = atoi(strtok(NULL, " \n"));  
        while(x <= y){ printf("%d ",x++);}
        fflush(stdout);
    }else{
        perror("command entered is not proper!!");
        exit(EXIT_FAILURE);
    }

    // Handle semWait, semSignal, etc...

    // Finally increment PC
    sprintf(memory[3].arg1, "%d", pc + 1);
    // return true if program finished execution
    if (strcmp(memory[pc].identifier,"EOI") == 0 ) return true; 

    return false;
}


void FCFS_algo(struct program *programList[] , int clockcycles,MemQueue readyQueue, MemQueue BlockingQueue){
    
    while(true){
        for (int i = 0; i < sizeof(programList) / sizeof(struct program); i++){

            // checking if a program should be added into memory
            if (clockcycles == programList[i]->arrivalTime){
                if (PCBID == 0)
                    Program_start_locations[PCBID] = Memory_start_location;
                else{
                    Program_start_locations[PCBID] = Program_start_locations[PCBID-1]->arg2 +1;
                }
                struct memoryWord *curr_program_memory = Program_start_locations[PCBID];

                // enquing the process into the ready queue
                enqueue(&readyQueue,curr_program_memory);

                //adding the program instructions into the memory
                parseProgram( programList[i]->programName,curr_program_memory);
                
                //creating the programs PCB in memory
                createPCB(curr_program_memory, programList[i]->priority);

            }
            //execute the process that has its turn
            if (peek(&readyQueue != NULL)){
                if (execute_an_instruction(peek(&readyQueue))){
                    dequeue(&readyQueue);
                }
            }

        }
    
    }
    
}

void scheduler(struct program programList[] ){
    // initialize ready queue, blocking queue, and running process
    int clockCycles = 0;

    struct MemoryWord *runningProcessLocation = NULL;
    
    MemQueue readyQueue;
    initQueue(&readyQueue);

    MemQueue BlockingQueue;
    initQueue(&BlockingQueue);

    //setting the scheduling algorithm
    SCHEDULING_ALGORITHM algo = FCFS; 
    switch (algo)
    {
    case FCFS:
        FCFS_algo(programList,clockCycles,readyQueue, BlockingQueue);
        break;
    case RR:

        break;
    case MLFQ:

        break;
    }
    
    

}

int main() {
    // Create and zero memory
    Memory_start_location = createMemory();


    struct program programList[3] = {
        {"Program_1.txt" , 2, 0},
        {"Program_2.txt" , 2, 1},
        {"Program_3.txt" , 2, 2}
    };  

    free(Memory_start_location);
    return 0;
}

